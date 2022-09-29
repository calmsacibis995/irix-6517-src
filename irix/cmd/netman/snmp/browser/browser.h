#ifndef __browser_h_
#define __browser_h_

/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Mib Browser main window
 *
 *	$Revision: 1.12 $
 *	$Date: 1992/10/22 01:56:22 $
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


extern "C" {
    char *ReadAndParse();
    struct mibNode* FindByName(struct mibNode*, char*);
    struct mibNode* FindByFullName(struct mibNode*, char*);
    struct mibNode* FindById(struct mibNode*, char*);
    struct mibNode* find_child(struct mibNode* parent, int id);
    void print_node(struct mibNode*); // for debugging only
    void print_enumlist(struct enumList*);
    char* getEnumString(struct enumList*, int);
    int getEnumValue(struct enumList*, char*);
    int FindFullNameId(struct mibNode*, char*, char*);
} // extern c

extern struct mibNode* mib;

#define MIN_PREF_HEIGHT 350
#define MIN_PREF_WIDTH  400

#define MAX_PREF_HEIGHT 500
#define MAX_PREF_WIDTH  500

#define notfoundMsg     "No such name"
#define badvalueMsg     "Bad value"
#define readonlyMsg     "Read only"
#define generrorMsg     "Generic error"
#define timeexpiredMsg  "Time expired"


class tuMenu;
class tuRowColumn;
class tuTextField;
class tuTimer;
class tuXExec;

class mibQueryHandler;
class varBindList;
class DialogBox;
class Indiv;

// Define the command line arguments that we understand.  For example,
// saying "-fn foo" is the same as putting "Browser*font: foo" in a
// .Xdefaults file. 
// xxx need better arg names for these (i.e "-foo" names); xxx
static XrmOptionDescRec args[] = {
    {"-n",	"*node",		XrmoptionSepArg, NULL},
    {"-c",	"*community",		XrmoptionSepArg, NULL},
    {"-t",	"*timeout",		XrmoptionSepArg, NULL},
    {"-r",	"*retries",		XrmoptionSepArg, NULL},
    {"-help",	"*help", 		XrmoptionNoArg, "Yes"},
};

static const int nargs = sizeof(args) / sizeof(XrmOptionDescRec);

// Define some application specific resources.  Right now, they all
// need to be placed into one structure.  Note that most of these
// names match up to the command line arguments above.
struct AppResources {
    char* node;
    char* community;
    int retries;
    int timeout;
    tuBool help;
};


// this struct is used to associate a snmp transaction ID with 
// the bTopLevel that requested it.
typedef struct ti_struct{
    int		transId;
    bTopLevel*	btl;
    ti_struct*	next;
} trInfo ;

class Browser : public bTopLevel {
public:
    Browser(const char* instanceName, tuColorMap* cmap,
	   tuResourceDB* db, AppResources* ar, 
           char* progName, char* progClass);
    void	run(void);
	
//  void	post(int msg, char *errString = NULL, int lineNumber = 0);
		
//    void	setExec(tuXExec *e);
    tuXExec*	getExec(void)	{ return exec; }
    static void	doHelp(tuGadget*, void*);
    void	doDescribe(bTopLevel*);
    
    char*	getNodeName(void);
    
    void	notifyBtlClosed(bTopLevel*);

    int		get(bTopLevel*, varBindList*);
    int		getNext(bTopLevel*, varBindList*);
    int		set(bTopLevel*, varBindList*);
    void	getResponse(int, struct result*);
    void	setResponse(int, struct result*);
    void	snmpResponse(int, struct result*, tuBool);

    DialogBox*	getDialogBox(void);
    void	openDialogBox(void);
    void	closeDialogBox(tuGadget *);
    void	closeLicDialogBox(tuGadget *);
    void	closeProgressDialogBox(tuGadget *);
    void	setDialogBoxCallBack(void);
    void	usage(void);
    
    char*	getLastFile()		{ return lastFile; }
    void	setLastFile(char* newName);

private:
    tuXExec*	    exec;
    tuTimer*	    timer;

    tuTextField*    nodeField;
    tuTextField*    communityField;
    tuTextField*    timeoutField;
    tuTextField*    retriesField;
    
    tuMenu*	    navMenu;
    tuRowColumn*    buttonsRC;
    
    Indiv*	indiv;
    DialogBox	*dialog, *licDialog, *progressDialog;
    
    trInfo*	transInfoList;
    int		highTransId;

    char*	lastFile;

    void	init(tuGadget*);
    void	nextField(tuGadget*);
    void	checkForChild(char*);
    void	quit(tuGadget*);
    void	help(tuGadget*);
    void	closeKids(tuGadget*);
    
    void	mibSelected(tuGadget*);
	
    void	readFields(char**, char**, int*, int*);
        
    void	outputResult(int);

};


#endif /* __browser_h_ */
