#ifndef __fancyList_h_
#define __fancyList_h_

// $Revision: 1.1 $
// $Date: 1992/07/08 01:03:19 $

#include <stdio.h>

#include <tuList.h>

class tuCallBack;
class tuEventGrabber;
class tuScrollBar;

class fancyList : public tuList {
  public:
    fancyList(tuGadget* parent, char* instanceName);
    ~fancyList();

    static void registerForPickling();

    void getLayoutHints(tuLayoutHints* hints);
    void resize(int w, int h);
    
    void map(tuBool propagate = True);
    void deselect();
    void clearAll();
    void goToEnd(); // move to bottom of list
    int writeToFile(FILE*); // write children to specified file.
    void render();
    
  protected:
    int getNumInternalChildren();
    tuGadget* getInternalChild(int w);

    void inc(tuGadget*);
    void pageInc(tuGadget*);
    void dec(tuGadget*);
    void pageDec(tuGadget*);
    void adjustKids(tuBool);
    void leftDrag(tuGadget*);
    void rightDrag(tuGadget*);
    void leftInc(tuGadget*);
    void rightInc(tuGadget*);
    void leftDec(tuGadget*);
    void rightDec(tuGadget*);
    void leftPageInc(tuGadget*);
    void rightPageInc(tuGadget*);
    void leftPageDec(tuGadget*);
    void rightPageDec(tuGadget*);
    
    void relayout(tuGadget* = NULL);

    void childevent(tuGadget*);

    tuScrollBar* leftScrollbar;
    tuScrollBar* rightScrollbar;

    static tuResourceChain resourceChain;
    static tuResourceItem resourceItems[];
};

#endif /* __fancyList_h_ */
