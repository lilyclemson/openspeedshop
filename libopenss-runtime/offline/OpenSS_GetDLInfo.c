/*******************************************************************************
 * ** Copyright (c) 2006-2012 The Krell Institue. All Rights Reserved.
 * ** Copyright (c) 2012 Matt LeGendre. All Rights Reserved.
 * **
 * ** This library is free software; you can redistribute it and/or modify it under
 * ** the terms of the GNU Lesser General Public License as published by the Free
 * ** Software Foundation; either version 2.1 of the License, or (at your option)
 * ** any later version.
 * **
 * ** This library is distributed in the hope that it will be useful, but WITHOUT
 * ** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * ** FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * ** details.
 * **
 * ** You should have received a copy of the GNU Lesser General Public License
 * ** along with this library; if not, write to the Free Software Foundation, Inc.,
 * ** 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * *******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#include "monlibs.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <dlfcn.h>
#include <pthread.h>
#include <link.h>

#define OPENSS_USE_DL_ITERATE 1

extern void offline_record_dso(const char* dsoname, uint64_t begin, uint64_t end, uint8_t is_dlopen);
extern const char* OpenSS_GetExecutablePath();

#if defined(TARGET_OS_BGP)
#include <link.h>
extern etext;
extern edata;
#elif defined(TARGET_OS_BGQ)
#include <link.h>
extern etext;
extern edata;
#endif

const char *monlibs_vdso_name = "[vdso]";
const char *monlibs_exe_name = "[exe]";
static struct dl_phdr_info *cur_list = NULL;
static unsigned int cur_list_size = 0;
static struct dl_phdr_info *new_list = NULL;
static unsigned int new_list_size = 0;
#define LIST_INITIAL_SIZE 32
static unsigned int lists_max_size = 0;

static int dl_cb(struct dl_phdr_info *info, size_t size, void *data)
{
   if (new_list_size >= lists_max_size) {
      if (lists_max_size)
         lists_max_size *= 2;
      else
         lists_max_size = LIST_INITIAL_SIZE;
      cur_list = (struct dl_phdr_info *) realloc(cur_list, sizeof(struct dl_phdr_info) * lists_max_size);
      new_list = (struct dl_phdr_info *) realloc(new_list, sizeof(struct dl_phdr_info) * lists_max_size);
   }

   memcpy(new_list + new_list_size, info, sizeof(struct dl_phdr_info));
   new_list_size++;
   return 0;
}

static int dl_phdr_cmp(const void *a, const void *b)
{
   struct dl_phdr_info *first = (struct dl_phdr_info *) a;
   struct dl_phdr_info *second = (struct dl_phdr_info *) b;
   return first->dlpi_phdr > second->dlpi_phdr;
}

static void report(struct dl_phdr_info *pinfo, int is_load, library_cb cb)
{
   mem_region mr[MAX_LOAD_SEGMENTS];
   unsigned int i, j = 0;
   ElfW(Addr) base_addr = pinfo->dlpi_addr;
   const char *name = NULL;

   if (is_load)
   {
      for (i = 0; i < pinfo->dlpi_phnum; i++) {

         const ElfW(Phdr) *phdr = pinfo->dlpi_phdr + i;
         if (phdr->p_type != PT_LOAD)
            continue;

         mr[j].mem_addr = base_addr + phdr->p_vaddr;
         mr[j].mem_size = phdr->p_memsz;
         mr[j].file_offset = phdr->p_offset;
         mr[j].file_size = phdr->p_filesz;
         mr[j].premissions = (phdr->p_flags & 7);
         j++;
      }

      if (pinfo->dlpi_name[0] == '\0') {
         /* Both the executable and vdso names are given as empty strings.  Fill in with constant strings */
         if (j == 1 && mr[0].mem_size <= 0x1000) {
            /* One segment less than a single page in size implies VDSO on linux */
            name = monlibs_vdso_name;
         }
         else {
            name = OpenSS_GetExecutablePath();
         }
      }
      else {
         name = strdup(pinfo->dlpi_name);
      }

      pinfo->dlpi_name = name;
   }
   else {
      name = pinfo->dlpi_name;
   }

   cb(base_addr, name, mr, j, is_load);

   if (!is_load) {
      if (name != monlibs_vdso_name && name != monlibs_exe_name) {
         free((void *) name);
      }
   }
}

void monlibs_getLibraries(library_cb cb)
{
   unsigned c = 0, n = 0;
   struct dl_phdr_info *temp;

   /* Fill in the new list with the current libraries. */
   new_list_size = 0;
   dl_iterate_phdr(dl_cb, NULL);

   /* Sort the new list for fast comparison */
   qsort(new_list, new_list_size, sizeof(struct dl_phdr_info), dl_phdr_cmp);

   /* Look for differences between orig and new list */
   for (;;) {
      if (c == cur_list_size && n == new_list_size) {
         /* End of both lists */
         break;
      }
      else if (c == cur_list_size) {
         /* End of cur list, but still have new list.   New object. */
         report(new_list + n++, 1, cb);
      }
      else if (n == new_list_size) {
         /* End of new list, but still have cur list.  Deleted object */
         report(cur_list + c++, 0, cb);
      }
      else if (cur_list[c].dlpi_phdr == new_list[n].dlpi_phdr) {
         /* Same element in old and new list.  No change here. */
         new_list[n++].dlpi_name = cur_list[c++].dlpi_name;
      }
      else if (cur_list[c].dlpi_phdr < new_list[n].dlpi_phdr) {
         /* There's an element in the cur list that we didn't see in the new list.  Deleted object */
         report(cur_list + c++, 0, cb);
      }
      else if (cur_list[c].dlpi_phdr > new_list[n].dlpi_phdr) {
         /* There's an element in the new list that we didn't see in the cur list.  New object */
         report(new_list + n++, 1, cb);
      }
   }

   /* Swap the new list with the cur list.  Empty (but leave allocated) the new list */
   temp = cur_list;
   cur_list = new_list;
   new_list = temp;
   cur_list_size = new_list_size;
   new_list_size = 0;
}

static void lc(ElfW(Addr) base_address, const char *name, mem_region *regions, unsigned int num_regions, int is_load)

{
#ifndef NDEBUG
    if ( (getenv("OPENSS_DEBUG_COLLECTOR") != NULL)) {
	fprintf(stderr, "OPENSS_GetDLInfo: %s %s at [%#lx, %#lx]\n",
		is_load ? "Loaded" : "Unloaded", name,
		regions[0].mem_addr,
		regions[0].mem_addr + regions[0].mem_size);
   }
#endif
   
    offline_record_dso(name, regions[0].mem_addr, regions[0].mem_addr + regions[0].mem_size, is_load);
}

int OpenSS_GetDLInfo(pid_t pid, char *path)
{
#if defined(TARGET_OS_BGP)
    offline_record_dso(OpenSS_GetExecutablePath(),(uint64_t)0x01000000,(uint64_t)&etext,0);
#elif defined(TARGET_OS_BGQ)
    offline_record_dso(OpenSS_GetExecutablePath(),(uint64_t)0x01000000,(uint64_t)&etext,0);
#else

#if defined(OPENSS_USE_DL_ITERATE)
    monlibs_getLibraries(lc);
#else
    char mapfile_name[PATH_MAX];
    FILE *mapfile;

    sprintf(mapfile_name, "/proc/%ld/maps", (long)pid);
    mapfile = fopen(mapfile_name, "r");

    if(!mapfile) {
	fprintf(stderr,"Error opening%s: %s\n", mapfile_name, strerror(errno));
	return(1);
    }

#if 0
#ifndef NDEBUG
    if ( (getenv("OPENSS_DEBUG_COLLECTOR") != NULL)) {

	pthread_t (*f_pthread_self)();
        f_pthread_self = (pthread_t (*)())dlsym(RTLD_DEFAULT, "pthread_self");
	fprintf(stderr,"OpenSS_GetDLInfo called for %s in %d,%lu\n",
		path ? path : "EMPTY PATH", getpid(),
		(f_pthread_self != NULL) ? (*f_pthread_self)() : 0);
    }
#endif
#endif

    while(!feof(mapfile)) {
	char buf[PATH_MAX+100], perm[5], dev[6], mappedpath[PATH_MAX];
	unsigned long begin, end, inode, offset;
	int n;

	/* read in one line from the /proc maps file for this pid. */
	if(fgets(buf, sizeof(buf), mapfile) == 0) {
	    break;
	}

	char *permstring = strchr(buf, (int) ' ');
	if (!(*(permstring+3) == 'x' && strchr(buf, (int) '/'))) {
	    continue;
	}

	mappedpath[0] = '\0';

	/* Read in the /proc/<pid>/maps file as it is formatted. */
	/* All fields are strings. The fields are as follows. */
	/* address  perms offset  dev  inode  pathname */
	/* The address field is begin-end in hex. */
	/* We record these as uint64_t. */
	/* perms are at least one of rwxp - we want the begin and end */
	/* address of the text section marked as "x". */
	/* We record the mappedpath as is and ignore the rest of the fields. */
        sscanf(buf, "%lx-%lx %s %lx %s %ld %s", &begin, &end, perm,
                &offset, dev, &inode, mappedpath);

	/* If a dso is passed in the path argument we only want to record */
	/* this particular dso into the openss-raw file. This happens when */
	/* the victim application has performed a dlopen. */
	if (path != NULL &&
	    mappedpath != NULL &&
	    (strncmp(path, mappedpath, strlen(path)) == 0) ) {
#ifndef NDEBUG
	    if ( (getenv("OPENSS_DEBUG_COLLECTOR") != NULL)) {
		fprintf(stderr,"OpenSS_GetDLInfo DLOPEN RECORD: %s [%08lx, %08lx]\n",
		    mappedpath, begin, end);
	    }
#endif
	    offline_record_dso(mappedpath, begin, end, 1);
	    break;
	}

	// DPM: added test for path 4-15-08
	else if (perm[2] == 'x' && path == NULL) {
#ifndef NDEBUG
	    if ( (getenv("OPENSS_DEBUG_COLLECTOR") != NULL)) {
		fprintf(stderr,"OpenSS_GetDLInfo LD RECORD %s [%08lx, %08lx]\n", mappedpath, begin, end);
	        offline_record_dso(mappedpath, begin, end, 0);
	    }
#endif
	}
    }
    fclose(mapfile);
#endif
#endif
    return(0);
}
