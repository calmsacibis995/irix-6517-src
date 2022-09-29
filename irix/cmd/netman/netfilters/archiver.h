#ifndef __archiver_h_
#define __archiver_h_

// $Revision: 1.6 $
// $Date: 1992/10/24 20:08:51 $

#include <tuTopLevel.h>

class tuCallBack;
class tuFilePrompter;
class tuGadget;
class tuResourceDB;
class tuTextField;

class DialogBox;

class blankFiller;
class fancyList;

#define INITIAL_FILE	"__DEFAULT__"
#define DEFAULT_FILE	"Filters"
#define DEFAULT_PATH	"/usr/lib/netvis/Filters"

#define FILENAMESIZE    256     /* Size of file names */

 
class Archiver : public tuTopLevel {
  public:
    Archiver(const char* instanceName, tuColorMap* cmap,
	      tuResourceDB* db, char* progName, char* progClass,
	      char* fileName, tuBool stdOut);
    ~Archiver();

    void	usage();
    void	getLayoutHints(tuLayoutHints*);
    void	openGadget(tuGadget*);
    void	save(tuGadget* = NULL);
    tuBool	readNamedFile(char*);
    tuBool	dosave();
    tuBool	isChanged();
    static DialogBox*	getSaveDialog();
    static DialogBox*	getDialogBox();
    static void warning(const char*);
    blankFiller*      getFiller();
    tuFilePrompter*   getFilePrompterGadget(const char* title);
    void	setSaveDoneCallBack(tuCallBack*);

  protected:
    void bindCallBack(char* name, void (Archiver::*method)());

    void openSaveDialog(void);
    void openDialogBox(void);
    void closeDialogBox(tuGadget*);

    void add(tuGadget*);
    void modify(tuGadget*);
    void remove(tuGadget*);
    void specify(tuGadget*);
    void acceptSpecify(tuGadget*);
    
    void newList(tuGadget* = NULL);
    void load(tuGadget*);
    void saveAs(tuGadget*);
    void close(tuGadget*);
    void iconicCB(tuGadget*);
    void fileCallBack(tuGadget*);
    void fileCallBack2(tuGadget*);
    char * expandTilde(const char *c);

    void enableAdd(tuGadget*);
    void listFcn(tuGadget*);
    void listDblFcn(tuGadget*);

    void testFcn(tuGadget*);

    tuTextField* filterField;
    tuTextField* commentField;
    fancyList*	ourList;
    tuGadget*	addBtn;
    tuGadget*	removeBtn;
    tuGadget*	specifyBtn;
    tuGadget*	modifyBtn;
    static	tuResourceChain resourceChain;
    static	tuResourceItem resourceItems[];
    tuCallBack*	saveDoneCallBack;

    tuFilePrompter* prompter;
    blankFiller*    filler;
    static DialogBox*	    saveDialog;
    static DialogBox*	    dialogBox;
    
    tuBool  changed;	// did list change since last open or save?
    tuBool  saving;
    tuBool  stdOut;	// write double-clicked filter to stdout
    char*   lastFile;	// file we opened or saved last
    tuCallBack* quitCB;
    
    tuBool  fillerWasMapped;
    int	   fillerX,fillerY;

};

#endif /* __archiver_h_ */
