#ifndef __bTopLevel_h_
#define __bTopLevel_h_

/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Mib Browser - common class for all 'tuTopLevel' windows
 *
 *	$Revision: 1.5 $
 *	$Date: 1992/10/03 00:49:02 $
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

#include <stdio.h>

#include <tuTopLevel.h>
#include <tuResourceDB.h>

#include "parser.h"

class tuCheckBox;
class tuFrame;
class tuLabel;
class tuTextField;
class tuXExec;

class varBind;


class bTopLevel : public tuTopLevel {
public:
    bTopLevel(struct mibNode* n,
	   const char* instanceName, tuColorMap* cmap, tuResourceDB* db,
           char* progName, char* progClass, Window transientFor = NULL);
    bTopLevel(struct mibNode* n,
	   const char* instanceName, bTopLevel* otherTopLevel,
           tuBool transientForOtherTopLevel = False);
    ~bTopLevel();

    void	    setExec(tuXExec *e);
    bTopLevel*	    getParentWindow()	{ return parentBTL; }
//    void	    closeChildWindows();
    int		    getHowOpen()	{ return howOpen; }
    struct mibNode* getNode()		{ return node; }
    char*	    getHelpCardName()	{ return helpCard; }
    void	    openGroup(struct mibNode*);
    virtual void    handleGetResponse(struct result*);
    virtual void    handleSetResponse(struct result*);
    void    saveRecurse(struct mibNode*, FILE*);
    virtual void    saveFile(FILE*);
    virtual void    writeSaveFile(FILE*);

protected:
    tuXExec*	    exec;
    struct mibNode* node;
    char*	    helpCard;
    tuGadget*	    stuff;
    tuCheckBox*	    closeCheckBox;
    tuTextField*    nameField;
    tuTextField*    idField;
    char	    fullName[MAXSTRLEN];
    char	    fullId[MAXIDLEN];
    char	    getTimeStr[30];
    char	    setTimeStr[30];

    bTopLevel*	    parentBTL;	// window that created us
    bTopLevel**	    childWindows;	// array of windows we created.
    int		    numChildWindows;// how many we might need (max)
    int		    howOpen; // 0 = "leaveOpen", 1 = "closeFirst"

    void	    save(tuGadget*);
    void	    saveAs(tuGadget*);
    void	    savePrompterFile(tuGadget*);

    void	    doPathLabels();
    tuBool	    buildMenu(tuMenu*, struct mibNode*, char*);
    virtual void    menuSelected(tuGadget *);
    
    void	    doDescribe(tuGadget*);
    void	    handleErrs(struct result*);
    
    static void	    closeNodesKids(struct mibNode*);
    static void	    closeKids(tuGadget*, void*);
    static void	    closeIt(tuGadget*, void*);
    static void	    openMain(tuGadget*, void*);
    static void	    openParent(tuGadget*, void*);
    
    varBind*	    makeVarBind(char*, char*);
    varBind*	    makeVarBind(char*, int);

};


#endif /* __bTopLevel_h_ */
