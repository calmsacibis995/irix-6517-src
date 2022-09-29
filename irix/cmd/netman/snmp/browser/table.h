#ifndef __table_h_
#define __table_h_

/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Mib Browser Table
 *
 *	$Revision: 1.8 $
 *	$Date: 1992/10/13 19:23:40 $
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

class tuGadget;
class tuRowColumn;
class tuTextField;
class tuFrame;
class tuXExec;
class varBindList;

// how many rows & columns a table can have xxx
#define MAX_ROWS 50
#define MAX_COLS 50

class Table : public bTopLevel {
public:
    Table(struct mibNode* n, const char* instanceName, bTopLevel* otherTopLevel);
    ~Table();
    void    getLayoutHints(tuLayoutHints*);
    void    saveFile(FILE*);
    void    writeSaveFile(FILE*);
    
    void    handleGetResponse(struct result*);
    void    handleSetResponse(struct result*);

private:
    tuRowColumn*    centralRC;
    varBindList     lastVBL;	// the last goodResult vbl we got from getNext    
    int		    colAccess[MAX_COLS];
    int		    colIndex[MAX_COLS];	    // which index this col is (0 if not index)
    struct enumList* colEnumList[MAX_COLS]; // enum type list
    tuBool	    colOidType[MAX_COLS];	    // this col is of type 'oid'
    char*	    indexString[MAX_ROWS];  // string of indices for each row 
    tuBool	    edited[MAX_ROWS][MAX_COLS]; // user edited this item

    int		    highRow;	// highest row number so far
    int		    highCol;	// highest col number in this table
    int		    maxIndexLength; // length of ongest indexString
    tuLabel*	    colLabels[MAX_COLS]; // label over each column
    tuFrame*	    frames[MAX_ROWS][MAX_COLS];
    tuTextField*    fields[MAX_ROWS][MAX_COLS]; // what fields are in the table
    tuLabel*	    endLabel;
    tuLabel	    *getTimeLbl, *setTimeLbl;
    tuTextField*    nodeField;
    
    void    nextField(tuGadget*);
    void    clearTable(tuGadget*);
    void    getFirst(tuGadget*);
    void    getNext(tuGadget*);
    void    set(tuGadget*);
    
};



#endif /* __table_h_ */
