#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>               /* for fstat() */
#include <fcntl.h>
#include <sys/mman.h>               /* for mmap() */
#include <time.h>
#include <stdio.h>
#include <list>
#include <inttypes.h>
#include <stdexcept>
#include <string>

// for host name description
     #include <sys/socket.h>
     #include <netinet/in.h>
     #include <netdb.h>

#ifndef PTHREAD_MUTEX_RECURSIVE_NP
#define PTHREAD_MUTEX_RECURSIVE_NP 0
#endif

#include "ToolAPI.hxx"
using namespace OpenSpeedShop::Framework;

#include "support.h"
#include "Commander.hxx"
#include "Clip.hxx"
#include "Experiment.hxx"

#include "ArgClass.hxx"
extern "C" void loadTheGUI(ArgStruct *);


// Local Macros
static inline
FILE *predefined_filename (std::string filename)
{
  if (filename.length() == 0) {
    return NULL;
  } if (!strcmp( filename.c_str(), "stdout")) {
    return stdout;
  } else if (!strcmp( filename.c_str(), "stderr")) {
    return stderr;
  } else if (!strcmp( filename.c_str(), "stdin")) {
    return stdin;
  } else {
    return NULL;
  }
}

// Static Local Data

// Allow only one thread at a time through the Command processor.
// Doing this allows only one thread at a time to allocate sequence numbers.
static CMDID Command_Sequence_Number = 0;

// To the outside world, Window_IDs are just simple integers.
// Allow only one thread at a time to allocate a Window ID or to
// add, remove or search the list of defined windows.
static pthread_mutex_t Window_List_Lock = PTHREAD_MUTEX_INITIALIZER;
static std::list<CommandWindowID *> CommandWindowID_list;
static CMDWID Command_Window_ID = 0;
static CMDWID Last_ReadWindow = 0;
static bool Async_Inputs = false;
static bool Looking_for_Async_Inputs = false;
static CMDWID More_Input_Needed_From_Window = 0;
static pthread_mutex_t Async_Input_Lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  Async_Input_Available = PTHREAD_COND_INITIALIZER;


// Input_Source
#define DEFAULT_INPUT_BUFFER_SIZE 4096

static char *current_prompt = "openss";

class Input_Source
{
 protected:
  Input_Source *Next_Source;
  bool Predefined;
  std::string Name;
  FILE *Fp;
  int64_t Buffer_Size;
  int64_t Next_Line_At;
  int64_t Last_Valid_Data;
  char *Buffer;
  InputLineObject *Input_Object;
  bool Trace_To_A_Predefined_File;
  std::string Trace_Name;
  FILE *Trace_F;

 public:
  // Constructor & Destructor
  Input_Source (std::string my_name) {
    Next_Source = NULL;
    Name = my_name;
    Fp = predefined_filename (my_name);
    if (Fp == NULL) {
      Predefined = false;
      Fp = fopen (my_name.c_str(), "r");
      if (Fp == NULL) {
        fprintf(stderr,"ERROR: Unable to open alternate command file %s\n",my_name.c_str());
      }
    } else {
      Predefined = true;
    }
    Next_Line_At = 0;
    Last_Valid_Data = 0;
    Buffer_Size = DEFAULT_INPUT_BUFFER_SIZE;
    Buffer = (char *)malloc(Buffer_Size);
    Input_Object = NULL;
    Trace_To_A_Predefined_File = false;
    Trace_Name = std::string("");
    Trace_F = NULL;
  }
  Input_Source (InputLineObject *clip) {
    Next_Source = NULL;
    Name = std::string("");
    Fp = NULL;
    Predefined = false;
    Next_Line_At = 0;
    Last_Valid_Data = 0;
    Buffer_Size = 0;
    Buffer = NULL;
    Input_Object = clip;
    Trace_To_A_Predefined_File = false;
    Trace_Name = std::string("");
    Trace_F = NULL;
  }
  Input_Source (int64_t buffsize, char *buffer) {
    Next_Source = NULL;
    Name = std::string("");
    Fp = NULL;
    Predefined = false;
    Next_Line_At = 0;
    Last_Valid_Data = buffsize-1;
    Buffer_Size = buffsize;
    Buffer = buffer;
    Input_Object = NULL;
    Trace_To_A_Predefined_File = false;
    Trace_Name = std::string("");
    Trace_F = NULL;
  }
  ~Input_Source () {
   /* Assume that this routine has ownership of any buffer it was given. */
    if (Buffer) {
      free (Buffer);
    }
   /* Close input files. */
    if (Fp) {
      fclose (Fp);
    }
   /* Close trace files. */
    if (Trace_F && !Trace_To_A_Predefined_File) {
      fclose (Trace_F);
    }
  }

  void Link (Input_Source *inp) { Next_Source = inp; }
  Input_Source *Next () { return Next_Source; }
  InputLineObject *InObj () { return Input_Object; }

  char *Get_Next_Line () {
    if (Next_Line_At >= Last_Valid_Data) {
      if (Fp == NULL) {
        return NULL;
      }
      Buffer[0] = *("\0");
      Buffer[Buffer_Size-1] = (char)0;
      Next_Line_At = 0;
      Last_Valid_Data = 0;
      fgets (&Buffer[0], Buffer_Size, Fp);
      if (Buffer[Buffer_Size-1] != (char)0) {
        fprintf(stderr,"ERROR: Input line from %s is too long for buffer.\n",Name.c_str());
        return NULL;
      }
      Last_Valid_Data = strlen(&Buffer[0]);
    }
    char *next_line = &Buffer[Next_Line_At];
    int64_t line_len = strlen(next_line);
    int i;
    for (i = 1; i < line_len; i++) {  
      if (next_line[i] == *("\0")) {
        line_len = i;
        break;
      }
    }
    if (next_line[0] == *("\0")) {
     /* This indicates an EOF */
      return NULL;
    }
    Next_Line_At += line_len;
    return next_line;
  }

  void Set_Trace ( std::string tofname) {
    if (Trace_F && !Trace_To_A_Predefined_File) {
      fclose (Trace_F);
    }
    FILE *tof = predefined_filename( tofname );
    bool is_predefined = (tof != NULL);
    if (tof == NULL) tof = fopen (tofname.c_str(), "a");
    Trace_To_A_Predefined_File = is_predefined;
    Trace_Name = tofname;
    Trace_F = tof;
  }
  void Remove_Trace () {
    if (Trace_F && !Trace_To_A_Predefined_File) {
      fclose (Trace_F);
    }
    Trace_To_A_Predefined_File = false;
    Trace_Name = std::string("");
    Trace_F = NULL;
  }
  bool Trace_File_Is_Predefined () { return Trace_To_A_Predefined_File; }
  std::string Trace_File_Name () { return Trace_Name; }
  FILE *Trace_File () { return Trace_F; }

  // Debug aids
  void Dump(FILE *TFile) {
      bool nl_at_eol = false;
      fprintf(TFile,"    Read from: %s",
                    (Fp) ? Name.c_str() : 
                    (Input_Object) ? "image " : "buffer ");
      if (Input_Object != NULL) {
        Input_Object->Print (TFile);
        nl_at_eol = true;
      } else if (!Fp) {
        fprintf(TFile,"len=%d, next=%d",Buffer_Size,Next_Line_At);
        if (Buffer_Size > Next_Line_At) {
          int nline = strlen (&(Buffer[Next_Line_At]));
          nl_at_eol = (Buffer[Next_Line_At+nline] == *("\n"));
          if (nline > 2) {
            fprintf(TFile,": %.20s", &(Buffer[Next_Line_At]));
            if (nline > 20) fprintf(TFile,"...");
          }
        }
      }
      if (!nl_at_eol) {
        fprintf(TFile,"\n");
      }
      if (Trace_F) {
        fprintf(TFile,"     trace to: %s\n",Trace_Name.c_str());
      }
  }
};

// CommandWindowID
class CommandWindowID
{
 protected:
  CMDWID id;
  bool remote;
  std::string I_Call_Myself;
  std::string Host_ID;
  pid_t Process_ID;
  int64_t Panel_ID;
  int64_t *MsgWaitingFlag;
  int64_t Current_Input_Level;
  int64_t Cmd_Count_In_Trace_File;
  std::string Trace_File_Name;
  FILE *Trace_F;
  bool Input_Is_Async;
  Input_Source *Input;
  pthread_mutex_t Input_List_Lock;
  EXPID FocusedExp;

 public:
  // Constructor & Destructor
  CommandWindowID ( std::string IAM, std::string  Host, pid_t Process, int64_t Panel, bool async)
    {
      remote = false;
      I_Call_Myself = IAM;
      Host_ID = Host;
      Process_ID = Process;
      Panel_ID = Panel;
      Current_Input_Level = 0;
      Cmd_Count_In_Trace_File = 0;
      Trace_File_Name = "";
      Trace_F = NULL;
      Input = NULL;
      Input_Is_Async = async;
      if (Input_Is_Async) {
        Assert(pthread_mutex_init(&Input_List_Lock, NULL) == 0); // dynamic initialization
      }
      FocusedExp = 0;

     // Generate a unique ID and remember it

     // Get exclusive access to the lock so that only one
     // add/remove/search of the list is done at a time.
      Assert(pthread_mutex_lock(&Window_List_Lock) == 0);

      id = ++Command_Window_ID;
      CommandWindowID_list.push_front(this);

     // Release the lock.
      Assert(pthread_mutex_unlock(&Window_List_Lock) == 0);

     // Allocate a trace file for commands associated with this window
      char base[20];
      snprintf(base, 20, "sshist%lld.XXXXXX",id);
      Trace_File_Name = std::string(tempnam ("./", &base[0] ));
      //Trace_File_Name = tempnam ("./", &base[0] );
      Trace_F  = fopen (Trace_File_Name.c_str(), "w");
    }
  ~CommandWindowID()
    {
     // Clear the identification field in case someone trys to reference
     // this entry again.
      id = 0;
     // Remove the trace files
      if (Trace_F) {
        fclose (Trace_F);
        int err = remove (Trace_File_Name.c_str());
      }
     // Remove the input specifiers
      if (Input) {
        for (Input_Source *inp = Input; inp != NULL; ) {
          Input_Source *next = inp->Next();
          delete inp;
          inp = next;
        }
        Input = NULL;
      }
     // Remove the control structures associate with the lock
      if (Input_Is_Async) {
        pthread_mutex_destroy(&Input_List_Lock);
      }

     // Unlink from the chain of windows

     // Get exclusive access to the lock so that only one
     // add/remove/search of the list is done at a time.
      Assert(pthread_mutex_lock(&Window_List_Lock) == 0);

      if (*CommandWindowID_list.begin()) {
        CommandWindowID_list.remove(this);
      }

     // Release the lock.
      Assert(pthread_mutex_unlock(&Window_List_Lock) == 0);
    }

  void Append_Input_Source (Input_Source *inp) {
   // Get exclusive access to the lock so that only one
   // read, write, add or delete is done at a time.
    if (Input_Is_Async) {
      Assert(pthread_mutex_lock(&Input_List_Lock) == 0);
    }

    if (Input == NULL) {
      Input = inp;
    } else {
      Input_Source *previous_inp = Input;
      while (previous_inp->Next() != NULL) {
        previous_inp = previous_inp->Next();
      }
      previous_inp->Link(inp);
    }

   // Release the lock.
    if (Input_Is_Async) {
      Assert(pthread_mutex_unlock(&Input_List_Lock) == 0);
    }

   // After a new comand is placed in the input window,
   // wake up a sleeping input reader.
    if (Looking_for_Async_Inputs) {
      Assert(pthread_mutex_lock(&Async_Input_Lock) == 0);
     // After we get the lock, be sure that the reader
     // is still waiting for input. No need to send a
     // signal if it grabbed the last line before we
     // got ahold of the lock.
      if (Looking_for_Async_Inputs) {
        Looking_for_Async_Inputs = false;
        Assert(pthread_cond_signal(&Async_Input_Available) == 0);
      }
      Assert(pthread_mutex_unlock(&Async_Input_Lock) == 0);
    }
  }
  void Push_Input_Source (Input_Source *inp) {
   // Get exclusive access to the lock so that only one
   // read, write, add or delete is done at a time.
    if (Input_Is_Async) {
      Assert(pthread_mutex_lock(&Input_List_Lock) == 0);
    }

    Input_Source *previous_inp = Input;
    inp->Link(previous_inp);
    Input = inp;

   // Release the lock.
    if (Input_Is_Async) {
      Assert(pthread_mutex_unlock(&Input_List_Lock) == 0);
    }
  }
private:
  void Pop_Input_Source () {
   // We do not need to get exclusive access to Input_List_Lock
   // Because the only path to this routine is through Read_Command,
   // which already has exclusive use of the lock.

    Input_Source *old = Input;
    Input = Input->Next();
    delete old;
  }
public:
  InputLineObject *Read_Command () {
   // Is there a good chance of finding something to read?
   // We aren't going to go to the trouble of getting the lock
   // unless there is something waiting to be processed.
    if (Input == NULL) {
     // We don't think there is anything to read.
     //
     // Yes, there is a race condiiton here, but the worst that
     // can happen is that we miss reading a line of input as
     // soon as we could.  Since this read is in a loop, we will
     // eventually get back here to read the new line.
      return NULL;
    }

   // Get exclusive access to the lock so that only one
   // read, write, add or delete is done at a time.
    if (Input_Is_Async) {
      Assert(pthread_mutex_lock(&Input_List_Lock) == 0);
    }

    InputLineObject *clip;
    char *next_line = NULL;

    while (Input != NULL) {
      clip = Input->InObj ();
      if (clip != NULL) {
       // Return this line and remove it from the input stack
        Pop_Input_Source();
        break;
      } else {
       // Read file or buffer to get input.
        next_line = Input->Get_Next_Line ();
        if (next_line != NULL) {
          break;
        }
        Pop_Input_Source();
      }
    }

   // Release the lock.
    if (Input_Is_Async) {
      Assert(pthread_mutex_unlock(&Input_List_Lock) == 0);
    }

    if (clip == NULL) {
      if (next_line == NULL) {
        return NULL;
      }
      clip = new InputLineObject(ID(), next_line);
    }

    return clip;
  }

  bool Input_Available () {
    return  ((Input == NULL) ? Input_Is_Async : true);
  }

  // Field access
  std::string Trace_Name() { return Trace_File_Name.c_str(); }
  FILE   *Trace_File() { return Trace_F; }
  CMDWID  ID () { return id; }
  EXPID   Focus () { return FocusedExp; }
  void    Set_Focus (EXPID exp) { FocusedExp = exp; }
  int64_t Input_Level () { return Current_Input_Level; }
  void    Increment_Level () { Current_Input_Level++; }
  void    Decrement_Level () { Current_Input_Level--; }

  void   Set_Alt_Trace_File ( std::string tofname ) {
    if (Input) {
      Input->Set_Trace ( tofname );
    }
  }
  void   Remove_Alt_Trace_File () {
    if (Input) {
      Input->Remove_Trace ();
    }
  }

  // For error reporting
  void Print_Location(FILE *TFile) {
    fprintf(TFile,"%s %lld",Host_ID.c_str(),(int64_t)Process_ID);
  }
  // Debug aids
  void Print(FILE *TFile) {
    fprintf(TFile,
       "W %lld: async:%s remote:%s IAM:%s %s %lld %lld, history->%s\n",
        id,Input_Is_Async?"T":"F",remote?"T":"F",
        I_Call_Myself.c_str(),Host_ID.c_str(),(int64_t)Process_ID,Panel_ID,
        Trace_File_Name.c_str());
    if (Input) {
      fprintf(TFile,"  Active Input Source Stack:\n");
      for (Input_Source *inp = Input; inp != NULL; inp = inp->Next()) {
        inp->Dump(TFile );
      }
    }
  }
  void Trace (InputLineObject *clip) {
    if (Trace_File()) {
      FILE *TFile = Trace_File();
      clip->Print (TFile);
    }
    if (Input) {
      FILE *ct = Input->Trace_File();
      if (ct) {
        fprintf(ct,"%s\n", clip->Command().c_str());
        if (Input->Trace_File_Is_Predefined()) {
         // Trace_File is something like stdout, so push the message out to user.
          fflush (ct);
        }
      }
    }
  }
  void Trace (ResultObject *rslt) {
    if (Trace_F) {
      RStatus status = rslt->Status();
      std::string result_type = rslt->Type();
      std::string msg_string = rslt->ResultMsg();
      void *result = rslt->Result();
      fprintf(Trace_F,
       "R %s %s 0x%x %s\n", (status == SUCCESS) ? "SUCCESS"
                                : (status == FAILURE) ? "FAILURE"
                                : (status == EXIT) ? "EXIT"
                                : "PARTIAL_SUCCESS",
                          (result_type.length()) ? result_type.c_str() : "notype",
                          (result != NULL) ? result : 0,
                          msg_string.c_str());
    }
  }
  void Dump(FILE *TFile)
    {
      std::list<CommandWindowID *>::reverse_iterator cwi;
      for (cwi = CommandWindowID_list.rbegin(); cwi != CommandWindowID_list.rend(); cwi++)
      {
        (*cwi)->Print(TFile);
      }
    }
};

// Local Utilities

CommandWindowID *Find_Command_Window (CMDWID WindowID)
{
  CommandWindowID *found_window = NULL;

 // Get exclusive access to the lock so that only one
 // add/remove/search of the list is done at a time.
  Assert(pthread_mutex_lock(&Window_List_Lock) == 0);

// Search for existing entry.
  if (WindowID > 0) {
    std::list<CommandWindowID *>::iterator cwi;
    for (cwi = CommandWindowID_list.begin(); cwi != CommandWindowID_list.end(); cwi++) {
      if (WindowID == (*cwi)->ID ()) {
        found_window = *cwi;
        break;
      }
    }
  }

 // Release the lock.
  Assert(pthread_mutex_unlock(&Window_List_Lock) == 0);

  return found_window;
}

int64_t Find_Command_Level (CMDWID WindowID)
{
  CommandWindowID *cwi = Find_Command_Window (WindowID);
  return (cwi != NULL) ? cwi->Input_Level() : 0;
}

// Low Level Semantic Routines

// What is the current focus associated with a CommandWindow?
EXPID Experiment_Focus (CMDWID WindowID)
{
  if (WindowID == 0) WindowID = Last_ReadWindow;
  CommandWindowID *my_window = Find_Command_Window (WindowID);
  return (my_window && my_window->Focus()) ? my_window->Focus() : 0;
}

// Set the focus for a particular CommandWindow.
EXPID Experiment_Focus (CMDWID WindowID, EXPID ExperimentID)
{
  if (WindowID == 0) WindowID = Last_ReadWindow;
  CommandWindowID *my_window = Find_Command_Window (WindowID);
  if (my_window) {
    ExperimentObject *Experiment = (ExperimentID) ? Find_Experiment_Object (ExperimentID) : NULL;
    my_window->Set_Focus(ExperimentID);
    return ExperimentID;
  }
  return 0;
}

void List_CommandWindows ( FILE *TFile )
{
  if (*CommandWindowID_list.begin()) {
    (*CommandWindowID_list.begin())->Dump(TFile);
  } else {
    fprintf(TFile,"\n(there are no defined windows)\n");
  }
}

void Trace_File_History (enum Trace_Entry_Type trace_type, std::string fname, FILE *TFile)
{
  FILE *cmdf = fopen (fname.c_str(), "r");
  struct stat stat_buf;
  if (!cmdf) {
    fprintf(stderr,"ERROR: Unable to open trace file %s\n",fname.c_str());
    return;
  }
  if (stat (fname.c_str(), &stat_buf) != 0) {
    fprintf(stderr,"ERROR: Unable to perform fstat command on %s\n",fname.c_str());
    return;
  }
  char *s = (char *)malloc(stat_buf.st_size+1);
  for (int i=0; i < stat_buf.st_size; ) {
    fgets (s, (stat_buf.st_size), cmdf);
    int len  = strlen(s);
    if (len > 2) {
      bool dump_this_record = false;
      char *t = s;
      switch (trace_type)
      {
        case CMDW_TRACE_ALL :
         // Dump raw data - i.e. all of every record.
          dump_this_record = true;
          break;
        case CMDW_TRACE_COMMANDS:
         // Dump records with leading "C ".
          if ((*s) ==  (*"C ")) {
            dump_this_record = true;
            t+=2;
          }
          break;
        case CMDW_TRACE_ORIGINAL_COMMANDS:
         // Dump records with leading "C " but strip off time stamp
          if ((*s) ==  (*"C ")) {
            for (int j = 0; j < len; t++, j++) {
              if (!strncmp(t,")",1)) break;
            }
            if (strlen(t) > 2) {
              dump_this_record = true;
              t+=3;
            }
          }
          break;
        case CMDW_TRACE_RESULTS:
         // Dump records with leading "R ".
          if ((*s) ==  (*"R ")) {
            dump_this_record = true;
            t+=2;
          }
          break;
      }
      if (dump_this_record) {
        *(s+len-1) = *("\n");
        fprintf(TFile,"%s",t);
      }
    }
    i+=len;
  }
  free (s);
  fclose (cmdf);
}

// Read the trace files and echo selected entries to another file.
void Command_Trace (enum Trace_Entry_Type trace_type, CMDWID cmdwinid, std::string tofname)
{
  FILE *tof = predefined_filename( tofname );
  bool tof_predefined = (tof != NULL);
  if (tof == NULL) tof = fopen (tofname.c_str(), "a");

  std::list<CommandWindowID *>::reverse_iterator cwi;
  for (cwi = CommandWindowID_list.rbegin(); cwi != CommandWindowID_list.rend(); cwi++)
  {
    if ((cmdwinid == 0) ||
        (cmdwinid == (*cwi)->ID ())) {
      std::string cmwtrn = (*cwi)->Trace_Name();
      FILE *cmwtrf = (*cwi)->Trace_File();
      if (!cmwtrf) {
        fprintf(stderr,"ERROR: no trace file %s\n",cmwtrn.c_str());
        return;
      }
      if (fflush (cmwtrf)) {
        fprintf(stderr,"ERROR: can not flush trace file %s\n",cmwtrn.c_str());
        return; 
      }
      Trace_File_History (trace_type, cmwtrn, tof);
    }
  }
  fflush(tof);
  if (!tof_predefined) {
    fclose (tof);
  }
}

// Set up an alternative trace file at user request.
ResultObject Command_Trace_ON (CMDWID WindowID, std::string tofname)
{
  CommandWindowID *cmdw = Find_Command_Window (WindowID);
  cmdw->Set_Alt_Trace_File (tofname);
  return ResultObject(SUCCESS, "Command_Trace_ON", (void *)0, "");
}
ResultObject Command_Trace_OFF (CMDWID WindowID)
{
  CommandWindowID *cmdw = Find_Command_Window (WindowID);
  cmdw->Remove_Alt_Trace_File ();
  return ResultObject(SUCCESS, "Command_Trace_OFF", (void *)0, "");
}

void  SpeedShop_Trace_ON (char *tofile) {
  (void)Command_Trace_ON (Last_ReadWindow, std::string(tofile));
}

void  SpeedShop_Trace_OFF(void) {
  (void)Command_Trace_OFF (Last_ReadWindow);
}

// This is the start of the Command Line Processing Routines.
// Only one thread can be executing one of these rotuines at a time,
// so the must be protected with the use of Window_List_Lock.

CMDWID Commander_Initialization (char *my_name, char *my_host, pid_t my_pid, int64_t my_panel, bool Input_is_Async)
{
 // Create a new Window
  CommandWindowID *cwid = new CommandWindowID(std::string(my_name ? my_name : ""),
                                              std::string(my_host ? my_host : ""),
                                              my_pid, my_panel, Input_is_Async);
  Async_Inputs |= Input_is_Async;
  return cwid->ID();
}

void Commander_Termination (CMDWID im)
{
  if (im) {
    CommandWindowID *my_window = Find_Command_Window (im);
    if (my_window) delete my_window;
  }
  return;
}


bool Isa_SS_Command (CMDWID issuedbywindow, const char *b_ptr) {
  int fc;
  for (fc = 0; fc < strlen(b_ptr); fc++) {
    if (b_ptr[fc] != *(" ")) break;
  }
  if (b_ptr[fc] == *("\?")) {
    bool Fatal_Error_Encountered = false;
    fprintf(stdout,"SpeedShop Status:\n");
    CommandWindowID *cw = Find_Command_Window (issuedbywindow);
    if ((cw == NULL) || (cw->ID() == 0)) {
      fprintf(stdout,"    ERROR: the window this command came from is illegal\n");
      Fatal_Error_Encountered = true;
    }
    fprintf(stdout,"  %s Waiting for Async input\n",Looking_for_Async_Inputs?" ":"Not");
    if (Looking_for_Async_Inputs) {
      if (More_Input_Needed_From_Window) {
        fprintf(stdout,"    Processing of a complex statement requires input from W %d.\n",
                More_Input_Needed_From_Window);
        CommandWindowID *lw = Find_Command_Window (More_Input_Needed_From_Window);
        if ((lw == NULL) || (!lw->Input_Available())) {
          fprintf(stdout,"    ERROR: this window can not provide more input!!\n");
          Fatal_Error_Encountered = true;
        }
      }
    }
    List_CommandWindows(stdout);
    Assert(!Fatal_Error_Encountered);
    return false;
  } else if (b_ptr[fc] == *("!")) {
    int ret = system(&b_ptr[fc+1]);
    return false;
  }
  return true;
}

InputLineObject *Append_Input_String (CMDWID issuedbywindow, char *b_ptr) {
  InputLineObject *clip = NULL;;
  if (Isa_SS_Command(issuedbywindow,b_ptr)) {
    CommandWindowID *cw = Find_Command_Window (issuedbywindow);
    Assert (cw);
    clip = new InputLineObject(issuedbywindow, std::string(b_ptr));
    Input_Source *inp = new Input_Source (clip);
    clip->SetStatus (ILO_QUEUED_INPUT);
    cw->Append_Input_Source (inp);
  }
  return clip;
}

ResultObject Append_Input_Buffer (CMDWID issuedbywindow, int64_t b_size, char *b_ptr) {
  CommandWindowID *cw = Find_Command_Window (issuedbywindow);

// DEBUG: hacks to let the gui pass information in without initializing a window.
if (!cw) {
  issuedbywindow = Command_Window_ID;  // default to the last allocated window
  Append_Input_String (issuedbywindow, b_ptr);
} else {

  Assert (cw);
  Input_Source *inp = new Input_Source (b_size, b_ptr);
  cw->Append_Input_Source (inp);
}
  return ResultObject(SUCCESS, "Command_File_T", NULL, "Command file read and processed");
}

ResultObject Append_Input_File (CMDWID issuedbywindow, std::string fromfname) {
  CommandWindowID *cw = Find_Command_Window (issuedbywindow);
  Assert (cw);
  Input_Source *inp = new Input_Source (fromfname);
  cw->Append_Input_Source (inp);
  return ResultObject(SUCCESS, "Command_File_T", NULL, "Command file read and processed");
}

ResultObject Push_Input_Buffer (CMDWID issuedbywindow, int64_t b_size, char *b_ptr) {
  CommandWindowID *cw = Find_Command_Window (issuedbywindow);
  Assert (cw);
  Input_Source *inp = new Input_Source (b_size, b_ptr);
  cw->Push_Input_Source (inp);
  return ResultObject(SUCCESS, "Command_File_T", NULL, "Command file read and processed");
}

ResultObject Push_Input_File (CMDWID issuedbywindow, std::string fromfname) {
  CommandWindowID *cw = Find_Command_Window (issuedbywindow);
  Assert (cw);
  Input_Source *inp = new Input_Source (fromfname);
  cw->Push_Input_Source (inp);
  return ResultObject(SUCCESS, "Command_File_T", NULL, "Command file read and processed");
}

// This routine continuously reads from stdin and appends the string to an input window.
// It is intended that this routine execute in it's own thread.
void SS_Direct_stdin_Input (void * attachtowindow) {
  Assert ((CMDWID)attachtowindow != 0);
  CommandWindowID *cw = Find_Command_Window ((CMDWID)attachtowindow);
  Assert (cw);
  int Buffer_Size= DEFAULT_INPUT_BUFFER_SIZE;
  char Buffer[Buffer_Size];
  Buffer[Buffer_Size-1] = *"\0";
  FILE *ttyin = fopen ( "/dev/tty", "r" );  // Read directly from the xterm window
  FILE *ttyout = fopen ( "/dev/tty", "w" );  // Write prompt directly to the xterm window
  for(;;) {
    sleep (1); /* DEBUG - give testing code time to react before splashing screen with prompt */
    fprintf(ttyout,"%s->",current_prompt);
    fflush (ttyout);
    Buffer[0] == *("\0");
    char *read_result = fgets (&Buffer[0], Buffer_Size, ttyin);
    if (Buffer[Buffer_Size-1] != (char)0) {
      fprintf(stderr,"ERROR: Input line from stdin is too long for buffer.\n");
      exit (0); // terminate the thread
    }
    if (read_result == NULL) {
     // This indicates an EOF or error
      exit (0); // terminate the thread
    }
    if (Buffer[0] == *("\0")) {
     // This indicates an EOF
      exit (0); // terminate the thread
    }
    if (cw->ID() == 0) {
     // This indicates that someone freed the input window
      exit (0); // terminate the thread
    }
    (void) Append_Input_String ((CMDWID)attachtowindow, &Buffer[0]);
  }
}

// The algorithm is to do a round-robin search of the defined window list
// by starting with the last widow that was read from.  The usual choice
// to read from the next window in the list but, if the parser indicates
// that it is processing a complex statement (e.g. a loop) the next line
// read MUST come from the previous window.
static CMDWID select_input_window (int is_more) {
  CMDWID selectwindow = Last_ReadWindow;

 // Get exclusive access to the lock so that only one
 // add/remove/search of the list is done at a time.
  Assert(pthread_mutex_lock(&Window_List_Lock) == 0);

  if (is_more == 0) {

    std::list<CommandWindowID *>::reverse_iterator cwi;
    for (cwi = CommandWindowID_list.rbegin(); cwi != CommandWindowID_list.rend(); cwi++)
    {
      if (selectwindow == (*cwi)->ID ()) {
        std::list<CommandWindowID *>::reverse_iterator next_cwi = ++cwi;
        if (next_cwi == CommandWindowID_list.rend()) {
          next_cwi = CommandWindowID_list.rbegin();
        }
        selectwindow = (*next_cwi)->ID();
        goto window_found;
      }
    }

   // A fall through the loop indicates that we didn't find the Last_ReadWindow.
   // Do error recovery by choosing the first one on the list.
    cwi = CommandWindowID_list.rbegin();
    if ((*cwi)) selectwindow = (*cwi)->ID();
    //selectwindow = (*CommandWindowID_list.rbegin())->ID();

  }

window_found:

 // Release the lock.
  Assert(pthread_mutex_unlock(&Window_List_Lock) == 0);

  Assert (selectwindow);
  Last_ReadWindow = selectwindow;
  return selectwindow;
}

static void There_Must_Be_More_Input (CommandWindowID *cw) {
  if ((cw == NULL) || (!cw->Input_Available())) {
    fprintf(stderr,"ERROR: The input source that started a complex statement"
                   " failed to complete the expression.\n");
    Assert (cw->Input_Available());
  }
}

InputLineObject *SpeedShop_ReadLine (int is_more)
{
  char *save_prompt = current_prompt;
  InputLineObject *clip;

  CMDWID readfromwindow = select_input_window(is_more);
  CMDWID firstreadwindow = readfromwindow;
  bool I_HAVE_ASYNC_INPUT_LOCK = false;
  
  if (is_more) {
    current_prompt = "....ss";
  }

read_another_window:

  CommandWindowID *cw = Find_Command_Window (readfromwindow);
  Assert (cw);

  do {
    clip = cw->Read_Command ();
    if (clip == NULL) {
     // The read failed.  Why?  Can we find something else to read?

     // It might be possible to read from a different window.
     // Try all of them so we can pick up commands that are waiting.
      readfromwindow = select_input_window(is_more);
      if (readfromwindow != firstreadwindow) {
       // There is another window to read from.
        goto read_another_window;
      }

     // After checking all windows for waiting input,
     // we might look for input from the gui or a terminal.
      if (Async_Inputs) {
        if (I_HAVE_ASYNC_INPUT_LOCK) {
         // Force a wait until data is ready.
          More_Input_Needed_From_Window = (is_more) ? readfromwindow : 0;
          Assert(pthread_cond_wait(&Async_Input_Available,&Async_Input_Lock) == 0);
          I_HAVE_ASYNC_INPUT_LOCK = false;
          Assert(pthread_mutex_unlock(&Async_Input_Lock) == 0);
        } else {
  
          if (is_more) {
            There_Must_Be_More_Input (cw);
          }

         // Get the lock and try again to read from each of the input windows
         // because something might have arrived after our first check of the
         // window.
          Assert(pthread_mutex_lock(&Async_Input_Lock) == 0);
          Looking_for_Async_Inputs = true;
          I_HAVE_ASYNC_INPUT_LOCK = true;
        }
        goto read_another_window;
      }

     // The end of all window inputs has been reached.
      if (is_more) {
       // We MUST read from this window!
       // This is an error situation.  How can we recover?
        There_Must_Be_More_Input(NULL);
      }

     // Return an empty line to indicate an EOF.
      break;
    }
    const char *s = clip->Command().c_str();
    int len  = strlen(s);
    if (!Isa_SS_Command(readfromwindow, s)) {
      clip = NULL;
      continue;
    }
    if (len > 1) {
      if (!strcasecmp ( s, "quit\n") ||
          !strcasecmp ( s, "quit")) {
        clip = NULL;
        break;
      }
    }
    if (len > 1) {
      if (!strcasecmp ( s, "gui\n")) {
        loadTheGUI((ArgStruct *)NULL);
        clip = NULL;
      }
    }
  } while (clip == NULL);

  if (I_HAVE_ASYNC_INPUT_LOCK) {
    I_HAVE_ASYNC_INPUT_LOCK = false;
    Looking_for_Async_Inputs = false;
    Assert(pthread_mutex_unlock(&Async_Input_Lock) == 0);
  }

  current_prompt = save_prompt;
  if (clip == NULL) {
    return NULL;
  }

 // Assign a sequence number to the command.
  clip->SetSeq (++Command_Sequence_Number);
  clip->SetStatus (ILO_IN_PARSER);

 // Log the command.
  cw->Trace(clip);

  return clip;
}
