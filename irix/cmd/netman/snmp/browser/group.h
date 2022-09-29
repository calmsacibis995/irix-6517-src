#ifndef __group_h_
#define __group_h_

/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Mib Browser Group level
 *
 *	$Revision: 1.7 $
 *	$Date: 1992/10/13 16:31:18 $
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


#include <tuResourceDB.h>

#include "bTopLevel.h"

class varBindList;
class tuGadget;
class tuLabel;
class tuMultiChoice;
class tuTextField;
class bTopLevel;

static char* howOpenTypes[] = {"leaveOpen", "closeFirst"};

typedef struct val {
    char*	    name;
    tuTextField*    field;
    int		    access; // READ_ONLY, READ_WRITE, WRITE_ONLY; 0 = table ??
    struct enumList* enumlist;
    tuBool	    oidType; // this var is of type 'oid'
    struct val*	    next;
} valStruct;

class Group : public bTopLevel {
public:
    Group(struct mibNode* n, const char* instanceName, bTopLevel* otherTopLevel);
    void    getLayoutHints(tuLayoutHints*);
    void    handleGetResponse(struct result*);
    void    handleSetResponse(struct result*);
    void    saveFile(FILE*);
    void    writeSaveFile(FILE*);

private:
    valStruct*	    vals;	// linked list of labels, fields in this window.
    tuMultiChoice*  openMulti;
    tuTextField*    nodeField;
    tuLabel	    *getTimeLbl, *setTimeLbl;

    void	    groupSelected(tuGadget*);
    
    tuTextField*    findValField(char*);
    valStruct*	    findVal(char*);
    valStruct*	    findVal(tuTextField*);
    void	    nextField(tuGadget*);
        
    void	    get(tuGadget*);
    void	    set(tuGadget*);
    
    void	    doErrors(struct result*);
    void	    doOneError(varBindList*, char*);

};


#endif /* __group_h_ */
