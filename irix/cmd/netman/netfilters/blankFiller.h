#ifndef __blankFiller_h_
#define __blankFiller_h_

// $Revision: 1.1 $
// $Date: 1992/07/08 01:02:52 $
#include "tuTopLevel.h"

class tuCallBack;
class tuFrame;
class tuList;
class tuRowColumn;
class tuTextField;
class tuLabel;

const int MAX_VARS = 30; // xxx what should this really be?
const int MAX_LINES = 10; // how many lines filter & comment can be broken into

class blankFiller : public tuTopLevel {
  public:
    blankFiller(const char* instanceName, tuTopLevel* othertoplevel,
	       tuBool transientForOtherTopLevel = False);
    ~blankFiller();
    
    void getLayoutHints(tuLayoutHints*);
    void map(tuBool propagate = True);
    void setCallBack(tuCallBack* c);

    void setFilter(char* varFilter);
    void setComment(char* varComment);
    char* getNewFilter()    { return filter; }
    char* getNewComment()  { return comment; }


  protected:
    virtual void    processEvent(XEvent*);
    void	    initialize();
    char**	    breakStringToFit(char*, tuFont*, int, int*);
    char*	    replaceVars(char* oldStringPtr);
    void	    accept(tuGadget *);
    void	    cancel(tuGadget *);

    tuCallBack*	    callback;
    int		    numVars;
    tuRowColumn     *varsRC, *filterRC, *commentRC, *buttonsRC;
    tuLabel*	    filterLbls[MAX_LINES];
    tuLabel*	    commentLbls[MAX_LINES];
    tuLabel*	    varLbls[MAX_VARS];
    tuFrame*	    varFrames[MAX_VARS];
    tuTextField*    varFields[MAX_VARS];
    char*	    varNames[MAX_VARS];
    tuLabel*	    filterLbl;
    tuLabel*	    commentLbl;
    char*	    filter;
    char*	    comment;
    tuBool	    canceled;
};

#endif /* __blankFiller_h_ */
