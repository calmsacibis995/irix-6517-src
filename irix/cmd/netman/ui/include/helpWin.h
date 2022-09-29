#ifndef __helpWin_h_
#define __helpWin_h_

/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *      helpWin
 *
 *      $Revision: 1.1 $
 *      $Date: 1992/09/25 00:17:48 $
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */
#include <tuTopLevel.h>

class DialogBox;

class tuCallBack;
class tuRowColumn;
class tuList;
class tuLabel;
class tuLabelButton;

class HelpWin : public tuTopLevel {

  public:
    HelpWin(const char* instanceName, tuTopLevel* othertoplevel,
		tuBool transientForOtherTopLevel = False);

    void getLayoutHints(tuLayoutHints*);

    int setContent(const char* helpName);

  protected:

    void doPageUp(tuGadget*);
    void doPageDown(tuGadget*);
    void doQuit(tuGadget*);

    tuCallBack* callback;
    tuList* helpField;
    DialogBox* dialog;
};

#endif /* __helpWin_h_ */
