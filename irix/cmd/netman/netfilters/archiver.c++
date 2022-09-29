// $Revision: 1.13 $
// $Date: 1996/02/26 01:27:45 $

#include "archiver.h"
#include "archiverLayout.h"
#include "blankFiller.h"
#include "dblLabel.h"
#include "dialog.h"
#include "fancyList.h"

#include "helpWin.h"
#include "help.h"

#include <stdio.h>
#include <strings.h>
#include <bstring.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include <tuScreen.h>
#include <tuTopLevel.h>
#include <tuWindow.h>
#include <tuVisual.h>
#include <tuXExec.h>

#include <tuArrowButton.h>
#include <tuCallBack.h>
#include <tuFilePrompter.h>
#include <tuGadget.h>
#include <tuGap.h>
#include <tuLabel.h>
#include <tuLabelButton.h>
#include <tuLabelMenuItem.h>
#include <tuList.h>
#include <tuMenu.h>
#include <tuMenuBar.h>
#include <tuRowColumn.h>
#include <tuScrollBar.h>
#include <tuSeparator.h>
#include <tuTextField.h>
#include <tuTopLevel.h>
#include <tuUnPickle.h>

#define MIN_PREF_HEIGHT 250
#define MIN_PREF_WIDTH  400
#define MAX_PREF_HEIGHT 600
#define MAX_PREF_WIDTH  800

// xxx what are real names and iconnames ??
#define MAIN_TITLE      "NetFilters"
#define MAIN_ICONNAME   "NetFilters"
#define FILLER_TITLE    "Filter Variables"
#define FILLER_ICONNAME "Variables"

void archHelpFcn(tuGadget *g, void* card);

HelpWin* helpTop;

DialogBox* Archiver::saveDialog = NULL;
DialogBox* Archiver::dialogBox = NULL;

tuDeclareCallBackClass(ArchiverCallBack, Archiver);
// we'll implement it ourselves, since 'doit' is different than the standard one.
ArchiverCallBack::ArchiverCallBack(Archiver* o, void (Archiver::*m)(tuGadget*)) {
    obj = o;
    method = m;
}

ArchiverCallBack::~ArchiverCallBack() {}

// only "doit" if this was called by certain gadgets
void ArchiverCallBack::doit(tuGadget* g) {
    if (g == NULL || g->getTopLevel() == (tuTopLevel*)obj ||
	  g == obj->getSaveDialog() ||
	  g == obj->getDialogBox() ||
	  g == obj->getFiller() ||
	  g == obj->getFilePrompterGadget(NULL)) {
	(obj->*method)(g);
    }
}

class ArchSaveCallBack : public ArchiverCallBack {
  public:
    ArchSaveCallBack(Archiver* obj,
		    void (Archiver::*method)(tuGadget*),
		    const char* desc);
    virtual void doit(tuGadget*);
  protected:
    const char* desc;
    DialogBox* dialog;
};


ArchSaveCallBack::ArchSaveCallBack(Archiver* obj,
				 void (Archiver::*method)(tuGadget*),
				 const char* d)
: ArchiverCallBack(obj, method) {
    desc = d;
    dialog = NULL;
}


void ArchSaveCallBack::doit(tuGadget* g) {
    // printf("ArchSaveCallBack::doit. name = %s\n", getName());
    if (dialog != NULL && g == dialog) {
	enum tuDialogHitCode hitCode = dialog->getHitCode();
	if (hitCode == tuYes) {
	    obj->setSaveDoneCallBack(this);
	    obj->save();
	} else if (hitCode == tuNo) {
	    ArchiverCallBack::doit(NULL);
	}
	dialog = NULL;
    } else {
	if (g != NULL && g->getTopLevel() != obj) return;
	if (!obj->isChanged()) {
	    ArchiverCallBack::doit(g);
	} else {
	    obj->getSaveDialog()->unmap(); // was 'cancel() - essentially unmaps it.
	    dialog = obj->getSaveDialog(); // Do it after the cancel,
					       // because the cancel
					       // may have cleared our
					       // value for dialog.
	    char buf[500];
	    sprintf(buf, "Save changes before %s?", desc);
	    dialog->question(buf);
	    dialog->setCallBack(this);
	    dialog->resizeToFit();
	    dialog->map();
	    if (getName() == NULL) {
		registerName(desc); // XXX Horrible hack. We make sure that
				    // this callback is registered, so
				    // that it won't be deleted, so
				    // that we can set multiple things
				    // to have it as its callback.
				    // Gross, gross, gross.
	    }
	}
    }
}



#define defaultlayout "BBoard()"
#define HELP_CARD	    "/usr/lib/HelpCards/NetFilters.main.help"
#define HELP_FILE	    "/usr/lib/HelpCards/NetFilters.main.helpfile"


// Display main window help
void
archMainHelpFcn(tuGadget *, void* helpCardName) {

    if (helpTop == NULL)
	archHelpFcn (0, helpCardName);
    else
	archHelpFcn (0, HELP_FILE);
}


// Display other windows help
void
archHelpFcn(tuGadget *, void* helpCardName) {

    if (helpTop == NULL)
	HELPdisplay((char*) helpCardName);
    else {
	int err = helpTop->setContent((const char*)helpCardName);
	if (err == 0)
	    helpTop->map();
    }
}


tuResourceItem Archiver::resourceItems[] = {
 //   { "layout", 0, tuResString, defaultlayout, offsetof(Archiver,layoutstr),},
    0,
};

tuResourceChain Archiver::resourceChain = {
    "Archiver",
    Archiver::resourceItems,
    &tuTopLevel::resourceChain
};



Archiver::Archiver(const char* instanceName, tuColorMap* cmap,
		     tuResourceDB* db, char* progName, char* progClass, 
		     char* fileName, tuBool out) 
: tuTopLevel(instanceName, cmap, db, progName, progClass)
{
    resources = &resourceChain;

    changed = False;
    
    if (!strcmp(fileName, INITIAL_FILE)) {
	struct stat statbuf;
	char buf[256];

	char* home = getenv("HOME");
	if (home != 0) {
	    sprintf(buf, "%s/%s", home, DEFAULT_FILE);
	    if (stat(buf, &statbuf) == 0) {
		strcpy(fileName, buf);
	    } else
		strcpy(fileName, DEFAULT_PATH);
	} else
	    strcpy(fileName, DEFAULT_PATH);
    }

    char* newName = expandTilde(fileName);
    if (newName == 0) {
	lastFile = strdup(fileName);
    } else {
	lastFile = newName;
    }

    stdOut = out;
    // make sure output is line buffered, for when we write to stdout
    // setlinebuf(stdout); 
    
    (new ArchSaveCallBack(this, Archiver::newList, "opening new file"))->registerName("__Archiver_new");
    (new ArchSaveCallBack(this, Archiver::load, "opening"))->registerName("__Archiver_read");

    (new ArchiverCallBack(this, Archiver::save))->registerName("__Archiver_save");
    (new ArchiverCallBack(this, Archiver::saveAs))->registerName("__Archiver_saveAs");

    (new ArchiverCallBack(this, Archiver::add))->registerName("__Archiver_add");
    (new ArchiverCallBack(this, Archiver::modify))->registerName("__Archiver_modify");
    (new ArchiverCallBack(this, Archiver::remove))->registerName("__Archiver_remove");
    (new ArchiverCallBack(this, Archiver::specify))->registerName("__Archiver_specify");
    (new ArchiverCallBack(this, Archiver::acceptSpecify))->registerName("__Archiver_acceptSpecify");
        
    (new ArchiverCallBack(this, Archiver::listFcn))->registerName("__Archiver_listFcn");
    (new ArchiverCallBack(this, Archiver::enableAdd))->registerName("__Archiver_enableAdd");
    (new ArchiverCallBack(this, Archiver::testFcn))->registerName("__Archiver_testFcn");
    
    (new tuFunctionCallBack(archMainHelpFcn, HELP_CARD))->registerName("__Archiver_help");


    quitCB = new ArchSaveCallBack(this, Archiver::close, "quitting");
    quitCB->registerName("__Archiver_close");
    catchQuitApp(quitCB);
    catchDeleteWindow(quitCB);

    setIconicCallBack(new ArchiverCallBack(this, Archiver::iconicCB));


    TUREGISTERGADGETS;
    dblLabel::registerForPickling();
    fancyList::registerForPickling();
    tuGadget* stuff = tuUnPickle::UnPickle(this, layoutstr);
  
    filterField = (tuTextField*) stuff->findGadget("filterField");
    commentField = (tuTextField*) stuff->findGadget("commentField");

    ourList = (fancyList*) stuff->findGadget("bigList");
    ourList->setCallBack(new ArchiverCallBack(this, Archiver::listFcn));
    ourList->setOtherCallBack(new ArchiverCallBack(this, Archiver::listDblFcn));
    
    addBtn     = stuff->findGadget("addBtn");
    removeBtn  = stuff->findGadget("removeBtn");
    specifyBtn = stuff->findGadget("specifyBtn");
    modifyBtn  = stuff->findGadget("modifyBtn");
    
    addBtn->setEnabled(True);       // always enable this one
    removeBtn->setEnabled(False);   // only enable it if something selected
    specifyBtn->setEnabled(False);  // only enable it if text has $ in it
    modifyBtn->setEnabled(False);   // only enable it if something selected

    tuLayoutHints hints;
    Display* disp = getDpy();
    if (strcmp(ServerVendor(disp), "Silicon Graphics") != 0)
    {
        // setup no showcase help window
	helpTop = new HelpWin("filtersHelp", (tuTopLevel*)this);
	helpTop->setName("NetFilters Help");
	helpTop->setIconName("filtersHelp");
	helpTop->bind();
	helpTop->getLayoutHints(&hints);
	helpTop->resize(hints.prefWidth, hints.prefHeight);
    }
    else
        helpTop = NULL;

    filler = new blankFiller("filler", (tuTopLevel*)this); // transient = true?
    filler->bind();
    filler->setCallBack(new ArchiverCallBack(this, Archiver::acceptSpecify));
    filler->setName(FILLER_TITLE);
    filler->setIconName(FILLER_ICONNAME);

    fillerX = 100;
    fillerY = 400;
    saveDialog = new DialogBox(progClass, (tuTopLevel*)this); // transient = true?
    dialogBox = new DialogBox(progClass, (tuTopLevel*)this, False);
    
    filler->resizeToFit();
    
    // read file (either the default, or what user specified on cmd line)
    
    if (readNamedFile(lastFile)) {
	char tmpStr[200];
	strcpy(tmpStr, MAIN_TITLE);
	strcat(tmpStr, " - ");
	strcat(tmpStr, lastFile);
	setName(tmpStr);
    } else {
	dialogBox->warning(errno, "Could not load file %s", lastFile);
	openDialogBox();
	delete [] lastFile;
	lastFile = NULL;
	setName(MAIN_TITLE);
    }
    
    setIconName(MAIN_ICONNAME);

} // Archiver()


Archiver::~Archiver() {
    
} // ~Archiver()

char *
Archiver::expandTilde(const char *c)
{
    if (c[0] != '~')
        return 0;
    char *p = strchr(c, '/');
    if (p == c + 1 || c[1] == '\0') {
        char *home = getenv("HOME");
	unsigned int len = strlen(home);
	if (len == 1)
	    len = 0;
	char *nc = new char[strlen(c) + len];
	strncpy(nc, home, len);
	if (p != 0)
	    strcpy(nc + len, p);
	return nc;
    }
    if (p != 0)
        *p = '\0';
    struct passwd *pw = getpwnam(c + 1);
    if (pw == 0)
        return 0;
    unsigned int len = strlen(pw->pw_dir);
    char *nc = new char[strlen(c) + len];
    strncpy(nc, pw->pw_dir, len + 1);
    if (p != 0) {
        *(nc + len) = '/';
        strcpy(nc + len + 1, p + 1);
        *p = '/';
    }
    return nc;
} // expandTilde

void
Archiver::getLayoutHints(tuLayoutHints* hints) {
    tuLayoutHints listHints;
    ourList->getLayoutHints(&listHints);

    int width  = TU_MIN(listHints.prefWidth + 20,  MAX_PREF_WIDTH);
    int height = TU_MIN(listHints.prefHeight + 100, MAX_PREF_HEIGHT);
    width  = TU_MAX(width,  MIN_PREF_WIDTH);
    height = TU_MAX(height, MIN_PREF_HEIGHT);
    hints->prefWidth = width;
    hints->prefHeight = height;
    hints->flags = 0;
}


void
Archiver::add(tuGadget*) {
    new dblLabel(ourList, "noName", filterField->getText(),
	commentField->getText());

    filterField->setText("", 0);
    commentField->setText("", 0);
    
    changed = True;
    
    removeBtn->setEnabled(False);
    specifyBtn->setEnabled(False);
    modifyBtn->setEnabled(False);

    ourList->updateLayout();
    ourList->deselect();
    ourList->goToEnd();
} // add


void
Archiver::modify(tuGadget*) {
    dblLabel* current = (dblLabel*) ((fancyList*)ourList)->getCurrentSelected();

    if (current && !strcmp(current->getClassName(), "DblLabel")) {
    	current->setLeftText((char*)filterField->getText());
    	current->setRightText((char*)commentField->getText());
        filterField->setText("", 0);
        commentField->setText("", 0);

	changed = True;

	removeBtn->setEnabled(False);
	specifyBtn->setEnabled(False);
	modifyBtn->setEnabled(False);
    }
    ourList->updateLayout();
    ourList->deselect();
} // modify


void
Archiver::specify(tuGadget*) {
    assert(filler != NULL);
    fillerX = filler->getXOrigin();
    fillerY = filler->getYOrigin();
    filler->unmap();
    
    filler->setFilter((char*)filterField->getText());
    filler->setComment((char*)commentField->getText());
    filler->resizeToFit();
    
    filler->setIconic(False);
    filler->setInitialOrigin(fillerX, fillerY);
    filler->map();

} // specify


void
Archiver::acceptSpecify(tuGadget*) {
    // printf("acceptSpecify\n");
    assert(filler != NULL);
    char* newFilter = filler->getNewFilter();
    char* newComment = filler->getNewComment();
    filler->unmap();
    
    filterField->setText(newFilter, strlen(newFilter));
    filterField->movePointToHome(False);
    commentField->setText(newComment, strlen(newComment));
    commentField->movePointToHome(False);
    
} // acceptSpecify

// this gets called AFTER we actually change state between iconic & not
void
Archiver::iconicCB(tuGadget *) {
    if (getIconic()) {
	if (filler->isMapped()) {
	    fillerWasMapped = True;
	    fillerX = filler->getXOrigin();
	    fillerY = filler->getYOrigin();
	    filler->unmap();
	} else
	    fillerWasMapped = False;
	    

    } else {
	if (fillerWasMapped) {
	    filler->setInitialOrigin(fillerX, fillerY);
	    filler->map();
	}
    }
    
} // iconicCB

void
Archiver::remove(tuGadget*) {
    dblLabel* current = (dblLabel*) ((fancyList*)ourList)->getCurrentSelected();
    if (current) current->markDelete();

    filterField->setText("", 0);
    commentField->setText("", 0);
    
    changed = True;
    
    removeBtn->setEnabled(False);
    specifyBtn->setEnabled(False);
    modifyBtn->setEnabled(False);

    ourList->updateLayout();
} // remove


void
Archiver::newList(tuGadget*) {
    changed = False;
    delete [] lastFile;
    lastFile = NULL;

    filterField->setText("", 0);
    commentField->setText("", 0);
    ourList->clearAll();
    ourList->updateLayout();  
    
    setName(MAIN_TITLE);  
}


tuBool
Archiver::readNamedFile(char* fileName) {
    assert(ourList != NULL);

    
    // read each line, get "leftText", "rightText"
    FILE *fp = fopen(fileName, "r");

    if (fp == NULL)
	return False;
    
    // remove anything that's in list already
    filterField->setText("", 0);
    commentField->setText("", 0);
    ourList->clearAll();

    char filter[256], comment[256];
    filter[0] = '#';
    comment[0] = '#';
    int length;
    
    do {
    	// allow for comment lines, starting with # ("invisible" comments)
    	while (filter[0] == '#' &&
    	    fp != NULL &&
    	    fgets(filter, sizeof filter, fp) != NULL) {
    	}
	length = strlen(filter) -1;
    	if (filter[length] == '\n')
	    filter[length] = NULL;
	    
    	while (comment[0] == '#' &&
    	    fp != NULL &&
    	    fgets(comment, sizeof comment, fp) != NULL) {
    	}
	length = strlen(comment) -1;
    	if (comment[length] == '\n')
	    comment[length] = NULL;

	// if first char is '#', we ran out of lines, so no comment
	if (comment[0] == '#')
	    comment[0] = NULL;

    	// create a new item, put it in the list
    	if (filter[0] != '#' && comment[0] != '#')
    	    new dblLabel(ourList, "noName", filter, comment);
		
    	filter[0] = '#';
    	comment[0] = '#';

    } while (fp != NULL && fgets(filter, sizeof filter, fp) != NULL);

    
    fclose(fp);

    delete [] lastFile;
    lastFile = strdup(fileName);

    // after all, clean up the list
    ourList->updateLayout();
    changed = False;
    
    char tmpStr[200];
    strcpy(tmpStr, MAIN_TITLE);
    strcat(tmpStr, " - ");
    strcat(tmpStr, lastFile);
    setName(tmpStr);
    
    return True;
    
} // readNamedFile



void
Archiver::close(tuGadget*) {

    HELPclose();

    exit(0);
} // close


void
Archiver::testFcn(tuGadget*) {
    printf(" Archiver::testFcn()\n");
} // testFcn


void
Archiver::enableAdd(tuGadget*) {
    addBtn->setEnabled(True);
} // enableAdd


void
Archiver::listFcn(tuGadget* theList) {
    dblLabel* current = (dblLabel*) ((fancyList*)theList)->getCurrentSelected();
    if (!current) {
        filterField->setText(" ", 1);
        commentField->setText(" ", 1);
        filterField->setText("", 0);
        commentField->setText("", 0);
	
	removeBtn->setEnabled(False);
	specifyBtn->setEnabled(False);
	modifyBtn->setEnabled(False);

    } else if (!strcmp(current->getClassName(), "DblLabel")) {
        filterField->setText(" ", 1);
        commentField->setText(" ", 1);

    	char* leftText = current->getLeftText();
    	char* rightText = current->getRightText();

	filterField->setText(leftText, strlen(leftText));
	filterField->movePointToHome(False);
	commentField->setText(rightText, strlen(rightText));
	commentField->movePointToHome(False);

	removeBtn->setEnabled(True);
	modifyBtn->setEnabled(True);

	if (strchr(leftText, '$'))
    	    specifyBtn->setEnabled(True);
	else
    	    specifyBtn->setEnabled(False);
    }
} // listFcn

// if user double clicks on an item in the list, select that one in the list
// but also make select the whole filter in the filterField, just like
// the user had gone there and triple-clicked.
void
Archiver::listDblFcn(tuGadget*) {
    filterField->selectAll();
    if (stdOut) {
	fprintf(stdout, "%s\n", filterField->getText());
	fflush(stdout);
    }
}

tuBool
Archiver::isChanged() {
    return changed;
}

blankFiller*
Archiver::getFiller() {
    return filler;
}

DialogBox*
Archiver::getSaveDialog() {
    return Archiver::saveDialog;
}

DialogBox*
Archiver::getDialogBox() {
    return Archiver::dialogBox;
}

tuFilePrompter*
Archiver::getFilePrompterGadget(const char* title) {
    // printf("getFilePrompterGadget(%s)\n", title);
    if (prompter == NULL) {
	prompter = new tuFilePrompter("prompter", this, True);
	prompter->setCallBack
	    (new ArchiverCallBack(this, Archiver::fileCallBack));
    }
    if (title) {
	prompter->unmap();
	prompter->setName(title);
    }
    return prompter;
}


void Archiver::setSaveDoneCallBack(tuCallBack* c) {
    // printf("setSaveDoneCallBack(%s)\n", c->getName());
    if (saveDoneCallBack) saveDoneCallBack->markDelete();
    saveDoneCallBack = c;
}


tuBool
Archiver::dosave() {
    // printf("dosave -- will save lastFile (%s)\n", lastFile);
    assert (lastFile != NULL);
    
    FILE *fp = fopen(lastFile, "w");
    if (fp == NULL)
	return False;
	
    fprintf(fp, "# NetFilters Filters\n");
    
    if (ourList->writeToFile(fp) != 0) {
	fclose(fp);
	return False;
    }

    fclose(fp);
        
    changed = False;

    return True;
} // dosave


void
Archiver::load(tuGadget*) {
    // printf("Archiver::load\n");
    tuFilePrompter* gadget = getFilePrompterGadget("Open file");
    gadget->growToFit();
    gadget->mapWithCancelUnderMouse();
    saving = False;
}


void
Archiver::save(tuGadget*) {
    // printf("save\n");
    if (lastFile == NULL) {
	// printf("  lastFile is null,  so calling saveAs\n");
	// xxx or just give message that says THEY have to do saveAs?
	// saveAs(g);
	dialogBox->warning("No current file name. Use the \"Save As\" command.");
	openDialogBox();
    } else {
	// printf("  calling dosave()\n");
	if (dosave()) {
	    if (saveDoneCallBack) {
		saveDoneCallBack->doit(NULL);
		saveDoneCallBack->markDelete();
		saveDoneCallBack = NULL;
	    }
	} else {
	    dialogBox->warning(errno, "Could not save file %s", lastFile);
	    openDialogBox();
	}
    }
} // save

void Archiver::saveAs(tuGadget*) {
    // printf("saveAs\n");
    tuFilePrompter* gadget = getFilePrompterGadget("Save As");

    gadget->growToFit();
    gadget->mapWithCancelUnderMouse();
    saving = True;
} // saveAs

void Archiver::fileCallBack(tuGadget* g) {
    // if (saving) printf("fileCallBack - saving is true\n");
    // else        printf("fileCallBack - saving is false\n");

    tuFilePrompter* gadget = (tuFilePrompter*) g;
    char* path = gadget->getSelectedPath();
    if (!path) {
	gadget->unmap();
	if (saveDoneCallBack) {
	    saveDoneCallBack->markDelete();
	    saveDoneCallBack = NULL;
	}
	return;
    }
    if (!saving) {
	// printf("   not saving,  so load file(%s)\n", path);
	if (readNamedFile(path)) {
	    gadget->unmap();
	} else {
	    dialogBox->warning(errno, "Could not load file %s", path);
	    openDialogBox();
	}
    } else {
	struct stat statbuf;
	if (stat(path, &statbuf) == 0) {
	    DialogBox* d = getSaveDialog();
	    d->unmap();
	    d->setCallBack(new ArchiverCallBack(this,
		&Archiver::fileCallBack2));
	    d->question("File %s already exists; overwrite?", path);
	    openSaveDialog();
	} else {
	    fileCallBack2(NULL);
	}
    }
    delete [] path;
    
} // fileCallBack
			   


void Archiver::fileCallBack2(tuGadget* gg) {
    // printf("fileCallBack2\n");
    DialogBox* g = (DialogBox*) gg;
    tuFilePrompter* gadget = getFilePrompterGadget(NULL);
    if (g && g->getHitCode() != tuYes) {
	if (g->getHitCode() == tuCancel)
	    gadget->unmap(); // Cancel pressed; so we'll cancel all the dialog boxes.
    } else {
	char* path = gadget->getSelectedPath();
	if (path == NULL) {
	    gadget->unmap();
	    if (saveDoneCallBack) {
		saveDoneCallBack->markDelete();
		saveDoneCallBack = NULL;
	    }
	    return;
	}
	delete [] lastFile;
	lastFile = strdup(path);
	if (dosave()) {
	    char tmpStr[200];
	    strcpy(tmpStr, MAIN_TITLE);
	    strcat(tmpStr, " - ");
	    strcat(tmpStr, lastFile);
	    setName(tmpStr);
 	    gadget->unmap();
	    if (saveDoneCallBack) {
		saveDoneCallBack->doit(NULL);
		saveDoneCallBack->markDelete();
		saveDoneCallBack = NULL;
	    }
	} else {
	    dialogBox->warning(errno, "Could not save file %s", lastFile);
	    openDialogBox();
	    delete [] lastFile;
	    lastFile = NULL;
	    setName(MAIN_TITLE);
	}
	delete [] path;
    }
    if (g) {
	g->setCallBack(NULL);
	g->unmap(); // was cancel() - I think this just unmaps it, since null callback first
    }
    
} // fileCallBack2


void
Archiver::openSaveDialog(void) {

    if (saveDialog->isMapped())
	saveDialog->unmap();

    saveDialog->map();
    XFlush(getDpy());

}

void
Archiver::openDialogBox(void) {
    dialogBox->setCallBack(new ArchiverCallBack(this, Archiver::closeDialogBox));

    if (dialogBox->isMapped())
	dialogBox->unmap();

    dialogBox->map();
    XFlush(getDpy());

}

void
Archiver::closeDialogBox(tuGadget *) {
    dialogBox->unmap();
    XFlush(getDpy());
}



void
Archiver::warning(const char* str) {  
    DialogBox* dialog = getSaveDialog();
    if (dialog->isMapped())
	dialog->unmap();
    dialog->warning(str);
    dialog->setCallBack(NULL);
    dialog->resizeToFit();
    dialog->map();

}

void
Archiver::usage() {
    DialogBox* dialog = getSaveDialog();
    dialog->information("usage: netfilters [-f filter_file]");
    dialog->setCallBack(new ArchiverCallBack(this, Archiver::close));
    dialog->resizeToFit();
    dialog->map();

}
