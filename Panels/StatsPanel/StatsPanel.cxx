////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005 Silicon Graphics, Inc. All Rights Reserved.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License as published by the Free
// Software Foundation; either version 2.1 of the License, or (at your option)
// any later version.
//
// This library is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
// details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this library; if not, write to the Free Software Foundation, Inc.,
// 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
////////////////////////////////////////////////////////////////////////////////


#include "StatsPanel.hxx"   // Change this to your new class header file name
#include "PanelContainer.hxx"   // Do not remove
#include "plugin_entry_point.hxx"   // Do not remove


#include "SPListView.hxx"   // Change this to your new class header file name
#include "SPListViewItem.hxx"   // Change this to your new class header file name
#include "UpdateObject.hxx"
#include "SourceObject.hxx"
#include "PreferencesChangedObject.hxx"

#include "preference_plugin_info.hxx" // Do not remove

#include "MetricInfo.hxx" // dummy data only...
// This is only hear for the debugging tables....
static char *color_name_table[10] =
  { "red", "orange", "yellow", "skyblue", "green" };



StatsPanel::StatsPanel(PanelContainer *pc, const char *n, void *argument) : Panel(pc, n)
{
  setCaption("StatsPanel");

  groupID = (int)argument;

  metricHeaderTypeArray = NULL;

  bool ok;
  numberItemsToRead = getPreferenceTopNLineEdit().toInt(&ok);
  if( !ok )
  {
printf("Invalid \"number of items\" in preferences.   Resetting to default.\n");
    numberItemsToRead = 5;
  }

  frameLayout = new QVBoxLayout( getBaseWidgetFrame(), 1, 2, getName() );

  lv = NULL;
  
  getBaseWidgetFrame()->setCaption("StatsPanelBaseWidget");

  char name_buffer[100];
  sprintf(name_buffer, "%s [%d]", getName(), groupID);
  setName(name_buffer);
}


/*! The only thing that needs to be cleaned is anything allocated in this
    class.  By default that is nothing.
 */
StatsPanel::~StatsPanel()
{
  // Delete anything you new'd from the constructor.
}

void
StatsPanel::itemSelected(QListViewItem *item)
{
  dprintf("StatsPanel::clicked() entered\n");

  if( item )
  {
    dprintf("  item->depth()=%d\n", item->depth() );
  
    SPListViewItem *nitem = (SPListViewItem *)item;
    while( nitem->parent() )
    {
      dprintf("looking for 0x%x\n", nitem->parent() );
      nitem = (SPListViewItem *)nitem->parent();
    }
  

    if( nitem )
    {
      dprintf("here's the parent! 0x%x\n", nitem);
      dprintf("  here's the rank of that parent: rank = %s\n",
        nitem->text(1).ascii() );
      matchSelectedItem( atoi( nitem->text(1).ascii() ) );
    }
  }
}


void
StatsPanel::matchSelectedItem(int element)
{
  dprintf ("StatsPanel::matchSelectedItem() = %d\n", element );

  int i = 0;
  HighlightList *highlightList = new HighlightList();
  highlightList->clear();
  HighlightObject *hlo = NULL;

  MetricInfo *fi = NULL;
  MetricInfoList::Iterator it = NULL;

  i = 0;
  for( it = collectorData->metricInfoList.begin();
       it != collectorData->metricInfoList.end();
       it++ )
  {
    fi = (MetricInfo *)*it;
    for( int line=fi->start; line <= fi->end; line++)
    {
      if( i >= 5 )
      {
        hlo = new HighlightObject(fi->fileName, line, color_name_table[4], "exclusive time");
      } else
      {
        hlo = new HighlightObject(fi->fileName, line, color_name_table[i], "exclusive time");
      }
// fprintf(stderr, "  pushback hlo: line=%d in color (%s)\n", line, hlo->color);
      highlightList->push_back(hlo);
    }
    i++;
  }


  i = 0;
  for( it = collectorData->metricInfoList.begin();
       it != collectorData->metricInfoList.end();
       it++ )
  {
     fi = (MetricInfo *)*it;
     if( i == element )
     {
       break;
     }
     i++;
  }

  dprintf ("%d (%s) (%s) (%d)\n", element, fi->functionName, fi->fileName, fi->function_line_number );
  
  char msg[1024];
  sprintf(msg, "%d (%s) (%s) (%d)\n", element, fi->functionName, fi->fileName, fi->function_line_number );
  

  SourceObject *spo = new SourceObject(fi->functionName, fi->fileName, fi->function_line_number, TRUE, highlightList);



  if( broadcast((char *)spo, NEAREST_T) == 0 )
  { // No source view up...
    char *panel_type = "Source Panel";
//Find the nearest toplevel and start placement from there...
    Panel *p = getPanelContainer()->dl_create_and_add_panel(panel_type, NULL, (void *)groupID);
    if( p != NULL ) 
    {
      p->listener((void *)spo);
    }
  }
}


void
StatsPanel::updateStatsPanelData(int expID, QString experiment_name)
{
   // Read the new data, destroy the old data, and update the StatsPanel with
   // the new data.


  dprintf("updateStatsPanelData() enterd.\n");
printf("updateStatsPanelData(%d, %s) enterd.\n", expID, experiment_name.ascii() );

  if( lv != NULL )
  {
    delete lv;
    lv = NULL;
  }

  if( lv == NULL )
  {
    lv = new SPListView( this, getBaseWidgetFrame(), getName(), 0 );
 
    connect( lv, SIGNAL(clicked(QListViewItem *)), this, SLOT( itemSelected( QListViewItem* )) );

    lv->setAllColumnsShowFocus(TRUE);

    // If there are subitems, then indicate with root decorations.
    lv->setRootIsDecorated(TRUE);

    // If there should be sort indicators in the header, show them here.
    lv->setShowSortIndicator(TRUE);

  }

  // Sort in decending order
  bool ok;
  int columnToSort = getPreferenceColumnToSortLineEdit().toInt(&ok);
  if( !ok )
  {
    columnToSort = 0;
  }
  lv->setSorting ( columnToSort, FALSE );

  // Figure out which way to sort
  bool sortOrder = getPreferenceSortDecending();
  if( sortOrder == TRUE )
  {
    lv->setSortOrder ( Qt::Descending );
  } else
  {
    lv->setSortOrder ( Qt::Ascending );
  }


  lv->clear();

  getUpdatedData();

  SPListViewItem *lvi;

// First delete the old column list.  (also used for dynamic menus)
  columnList.clear();
  if( metricHeaderTypeArray != NULL )
  {
    delete []metricHeaderTypeArray;
  }
  int header_count = collectorData->metricHeaderInfoList.count();
  metricHeaderTypeArray = new int[header_count];
  int i=0;
  for( MetricHeaderInfoList::Iterator pit = collectorData->metricHeaderInfoList.begin(); pit != collectorData->metricHeaderInfoList.end(); ++pit )
  { 
    MetricHeaderInfo *mhi = (MetricHeaderInfo *)*pit;
    QString s = mhi->label;
    lv->addColumn( s );
  metricHeaderTypeArray[i] = mhi->type;
  
    columnList.push_back( s );
    i++;
  }

  MetricInfo *fi;
  char buffer[1024];
  char rankstr[10];
  char filestr[21];
  char funcstr[21];

  i = 0;
  for( MetricInfoList::Iterator it = collectorData->metricInfoList.begin();
       it != collectorData->metricInfoList.end();
       it++ )
  {
    if( i >= numberItemsToRead )
    {
      break;
    }
    fi = (MetricInfo *)*it;

    dprintf("fi->functionName=(%s)\n", fi->functionName );
    char *ptr = NULL;
    ptr = truncateCharString(fi->functionName, 17);
    strcpy(funcstr, ptr);
    free(ptr);
    ptr = truncateCharString(fi->fileName, 17);
    strcpy(filestr, ptr);
    free(ptr);
    sprintf(rankstr, "%d", fi->index);
//     sprintf(buffer, "%-9s %-15s  %3.3f   %-20s  %d\n", rankstr, funcstr, fi->percent, filestr, fi->function_line_number);

    char percentstr[10];
    char exclusivestr[20];
    char startlinenostr[20];
    char endlinenostr[20];
    sprintf(percentstr, "%f", fi->percent);
    sprintf(exclusivestr, "%f", fi->exclusive_seconds);
    sprintf(startlinenostr, "%d", fi->start);
    sprintf(endlinenostr, "%d", fi->end);
    lvi=  new SPListViewItem( this, lv, percentstr, rankstr, exclusivestr, funcstr, filestr, startlinenostr, endlinenostr );
      lvi = new SPListViewItem( this, lvi, "SubText", QString("Additional Text for Rank ")+QString(rankstr) );
        (void)new SPListViewItem( this, lvi, "SubSubText", QString("Additional Text for Rank ")+QString(rankstr) );
    i++;
  }

  frameLayout->addWidget(lv);

  lv->show();
}
void
StatsPanel::languageChange()
{
  // Set language specific information here.
}

/*! This calls the user 'menu()' function
    if the user provides one.   The user can attach any specific panel
    menus to the passed argument and they will be displayed on a right
    mouse down in the panel.
    /param  contextMenu is the QPopupMenu * that use menus can be attached.
 */
bool
StatsPanel::menu(QPopupMenu* contextMenu)
{
  dprintf("StatsPanel::menu() requested.\n");
  contextMenu->insertSeparator();

//  contextMenu->insertItem("Number visible entries...", this, SLOT(setNumberVisibleEntries()), CTRL+Key_1, 0, -1);
  contextMenu->insertItem("Number visible entries...", this, SLOT(setNumberVisibleEntries()));

  contextMenu->insertSeparator();

//  contextMenu->insertItem("Compare...", this, SLOT(compareSelected()), CTRL+Key_1, 0, -1);
  contextMenu->insertItem("Compare...", this, SLOT(compareSelected()) );

  contextMenu->insertSeparator();

  int id = 0;
  QPopupMenu *columnsMenu = new QPopupMenu(this);
  columnsMenu->setCaption("Columns Menu");
  contextMenu->insertItem("&Columns Menu", columnsMenu, CTRL+Key_C);

  for( ColumnList::Iterator pit = columnList.begin();
           pit != columnList.end();
           ++pit )
  { 
    QString s = (QString)*pit;
    columnsMenu->insertItem(s, this, SLOT(doOption(int)), CTRL+Key_1, id, -1);
    if( lv->columnWidth(id) )
    {
      columnsMenu->setItemChecked(id, TRUE);
    } else
    {
      columnsMenu->setItemChecked(id, FALSE);
    }
    id++;
  }

//  contextMenu->insertItem("Export Report Data...", this, NULL, NULL);
  contextMenu->insertItem("Export Report Data...");

  return( TRUE );
}

/*! If the user panel save functionality, their function
     should provide the saving.
 */
void 
StatsPanel::save()
{
  dprintf("StatsPanel::save() requested.\n");
}

/*! If the user panel provides save to functionality, their function
     should provide the saving.  This callback will invoke a popup prompting
     for a file name.
 */
void 
StatsPanel::saveAs()
{
  dprintf("StatsPanel::saveAs() requested.\n");
}

void
StatsPanel::preferencesChanged()
{ 
//  printf("StatsPanel::preferencesChanged\n");

  bool thereWasAChangeICareAbout = FALSE;

  SortOrder old_sortOrder = lv->sortOrder();
  bool new_sortOrder = getPreferenceSortDecending();

  if( old_sortOrder != new_sortOrder )
  {
    if( new_sortOrder == TRUE )
    {
      lv->setSortOrder ( Qt::Descending );
    } else
    {
      lv->setSortOrder ( Qt::Ascending );
    }
    thereWasAChangeICareAbout = TRUE;
  }


  bool ok;
  int new_numberItemsToRead = getPreferenceTopNLineEdit().toInt(&ok);
  if( ok )
  {
    if( new_numberItemsToRead != numberItemsToRead )
    {
      numberItemsToRead = new_numberItemsToRead;
      thereWasAChangeICareAbout = TRUE;
    }
  }

  int new_columnToSort = getPreferenceColumnToSortLineEdit().toInt(&ok);
  if( ok )
  {
    int old_columnToSort = lv->sortColumn();
    if( old_columnToSort != new_columnToSort )
    {
      thereWasAChangeICareAbout = TRUE;
    }
  }

  if( thereWasAChangeICareAbout )
  {
// printf("  thereWasAChangeICareAbout!!!!\n");
    updateStatsPanelData();
  }
} 


/*! When a message has been sent (from anyone) and the message broker is
    notifying panels that they may want to know about the message, this is the
    function the broker notifies.   The listener then needs to determine
    if it wants to handle the message.
    \param msg is the incoming message.
    \return 0 means you didn't do anything with the message.
    \return 1 means you handled the message.
 */
int 
StatsPanel::listener(void *msg)
{
  dprintf("StatsPanel::listener() requested.\n");
  PreferencesChangedObject *pco = NULL;

// BUG - BIG TIME KLUDGE.   This should have a message type.
  MessageObject *msgObject = (MessageObject *)msg;
  if(  msgObject->msgType  == "UpdateExperimentDataObject" )
  {
    UpdateObject *msg = (UpdateObject *)msgObject;
msg->print();
    updateStatsPanelData(msg->expID, msg->experiment_name);
if( msg->raiseFLAG )
{
  getPanelContainer()->raisePanel((Panel *)this);
}
  } else if( msgObject->msgType == "PreferencesChangedObject" )
  {
//    printf("StatsPanel:  The preferences changed.\n");
    pco = (PreferencesChangedObject *)msgObject;
    preferencesChanged();
  }

  return 0;  // 0 means, did not want this message and did not act on anything.
}

/*! Create the context senstive menu for the report. */
bool
StatsPanel::createPopupMenu( QPopupMenu* contextMenu, const QPoint &pos )
{
  dprintf ("StatsPanel: Popup the context sensitive menu here.... can you augment it with the default popupmenu?\n");
  
  dprintf("selected item = %d\n", lv->selectedItem() );

  QPopupMenu *panelMenu = new QPopupMenu(this);
  panelMenu->setCaption("Panel Menu");
  contextMenu->insertItem("&Panel Menu", panelMenu, CTRL+Key_C);
  panelMenu->insertSeparator();
  menu(panelMenu);

  if( lv->selectedItem() )
  {
  //  contextMenu->insertItem("Tell Me MORE about %d!!!", this, SLOT(details()), CTRL+Key_1 );
    contextMenu->insertItem("Go to source location...", this, SLOT(gotoSource()), CTRL+Key_1 );
    return( TRUE );
  }
  

  return( FALSE );
}


void
StatsPanel::gotoSource()
{
  dprintf("gotoSource() menu selected.\n");
}

void
StatsPanel::compareSelected()
{
  dprintf("compareSelected()\n");
}

void
StatsPanel::setNumberVisibleEntries()
{
  dprintf("setNumberVisibleEntries()\n");
{
  bool ok;
  QString s = QString("%1").arg(numberItemsToRead);
  QString text = QInputDialog::getText(
          "Visible Lines", "Enter number visible lines:", QLineEdit::Normal,
          s, &ok, this );
  if( ok && !text.isEmpty() )
  {
    // user entered something and pressed OK
    numberItemsToRead = atoi(text.ascii());
    dprintf ("numberItemsToRead=%d\n", numberItemsToRead);
    updateStatsPanelData();
  } else
  {
    // user entered nothing or pressed Cancel
  }
}
}

static int cwidth = 0;  // This isn't what I want to do long term.. 
void
StatsPanel::doOption(int id)
{
  dprintf("doOption() id=%d\n", id);

  if( lv->columnWidth(id) )
  {
    cwidth = lv->columnWidth(id);
    lv->hideColumn(id);
  } else
  {
    lv->setColumnWidth(id, cwidth);
  }
}


/*! This is just a utility routine to truncate_name long names. */
char *
StatsPanel::truncateCharString(char *str, int length)
{
  char *newstr = NULL;
//  newstr = new char( length );
  newstr = (char *)calloc( length, sizeof(char)+3+1 );

  if( length > strlen(str) )
  {
    strcpy(newstr, str);
  } else
  {
    strcpy(newstr, "...");
    int extra = strlen(str)-length;
    strcat(newstr, str+extra);
    strcat(newstr, "");
  }

  return newstr;
}

// This routine needs to be rewritten when we really get the framework 
// round trip written.
void
StatsPanel::getUpdatedData()
{
  // Get the information about the collector.  
  collectorData = new CollectorInfo();
}
