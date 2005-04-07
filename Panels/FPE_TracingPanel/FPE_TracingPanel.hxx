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


#ifndef FPE_TracingPanel_H_H
#define FPE_TracingPanel_H_H
#include "Panel.hxx"           // Do not remove

#include "ProcessControlObject.hxx"
#include "ControlObject.hxx"

#include "AnimatedQPushButton.hxx"
#include <qpushbutton.h>

#include <qlayout.h>
#include <qhbox.h>
#include <qtimer.h>
#include "GenericProgressDialog.hxx"
#include "ManageCollectorsDialog.hxx"
#include "ManageProcessesDialog.hxx"

#include "SS_Input_Manager.hxx"

class PanelContainer;   // Do not remove
class QLabel;
class QLineEdit;
class QButtonGroup;
class QHBoxLayout;

class OpenSpeedshop;

#undef PANEL_CLASS_NAME
#define PANEL_CLASS_NAME FPE_TracingPanel   // Change the value of the define
                                         // to the name of your new class.

#include "Experiment.hxx"
//! Creates the FPE_TracingPanel that controls the pcSampling experiment.
class FPE_TracingPanel  : public Panel
{
  //! Q_OBJECT is needed as there are slots defined for the class
  Q_OBJECT
public:
  //! FPE_TracingPanel(PanelContainer *pc, const char *name)
    /*! This constructor is the work constructor.   It is called to
        create the new Panel and attach it to a PanelContainer.
        \param pc is a pointer to PanelContainer
               the Panel will be initially attached.
        \param name is the name give to the Panel.

        This is where the user would create the panel specific Qt code
        to do whatever functionality the user wanted the panel to perform.
     */
  FPE_TracingPanel(PanelContainer *pc, const char *n, void *argument);

  //! ~FPE_TracingPanel() - The default destructor.
  ~FPE_TracingPanel();  // Active destructor

  //! Adds use panel menus (if any).
  /*! This calls the user 'menu()' function
      if the user provides one.   The user can attach any specific panel
      menus to the passed argument and they will be displayed on a right
      mouse down in the panel.
      /param  contextMenu is the QPopupMenu * that use menus can be attached.
  */
  bool menu(QPopupMenu* contextMenu);

  //! Calls the user panel function save() request.
  void save();

  //! Calls the user panel function saveas() request.
  void saveAs();

  //! Calls the user panel function listener() request.
  /*! When a message
      has been sent (from anyone) and the message broker is notifying
      panels that they may want to know about the message, this is the
      function the broker notifies.   The listener then needs to determine
      if it wants to handle the message.
      \param msg is the incoming message.
      \return 0 means you didn't do anything with the message.
      \return 1 means you handled the message.
   */
  int listener(void *msg);

  QButtonGroup *buttonGroup;
  AnimatedQPushButton *attachCollectorButton;
  AnimatedQPushButton *detachCollectorButton;
  AnimatedQPushButton *attachProcessButton;
  AnimatedQPushButton *detachProcessButton;
  AnimatedQPushButton *runButton;
  AnimatedQPushButton *pauseButton;
#ifdef CONTINUE_BUTTON
  AnimatedQPushButton *continueButton;
#endif // CONTINUE_BUTTON
  AnimatedQPushButton *updateButton;
  AnimatedQPushButton *interruptButton;
  AnimatedQPushButton *terminateButton;

  QHBoxLayout *statusLayout;
  QLabel *statusLabel; 
  QLineEdit *statusLabelText;

  QVBoxLayout * frameLayout;
  ProcessControlObject *pco;

  bool runnableFLAG;

  QString executableNameStr;
  QString pidStr;
  QTimer *timer;
  QTimer *loadTimer;
  ManageProcessesDialog *manageProcessesDialog;
  ManageCollectorsDialog *manageCollectorsDialog;

OpenSpeedShop::Framework::Experiment *fw_experiment() { return experiment; }

#ifdef OLDWAY // move to public slots if needed again.
  void loadNewProgramSelected();
  bool detachFromProgramSelected();
  void attachToExecutableSelected();
#endif // OLDWAY
public slots:
  void saveAsSelected();
  void manageCollectorsSelected();
  void manageProcessesSelected();
  void manageDataSetsSelected();
  void loadSourcePanel();
  void loadStatsPanel();
  void progressUpdate();

private slots:
  void statusUpdateTimerSlot();

protected slots:
  virtual void languageChange();

protected:

private:
  OpenSpeedshop *mw;
  void wakeUpAndCheckExperimentStatus();

 
  int expID;  // Experiment ID of the expCreate, returned from the cli
  OpenSpeedShop::Framework::Experiment *experiment;

  void updateInitialStatus();

  QTimer *statusTimer;
  void updateStatus();

  void loadMain();
  GenericProgressDialog *pd;
  int steps;
};
#endif // FPE_TracingPanel_H_H
