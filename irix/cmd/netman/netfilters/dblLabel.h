#ifndef	__dblLabel_h_
#define	__dblLabel_h_

// $Revision: 1.2 $
// $Date: 1992/08/24 20:42:05 $

#include <stdio.h>
#include <string.h>

#include "tuGadget.h"
#include <tuLabel.h>

class tuFont;
class tuGC;
class tuColor;

class dblLabel : public tuGadget {
  public:
    dblLabel(tuGadget* parent, const char* instanceName,
    	const char* leftText,  const char* rightText);
    ~dblLabel();

    static void registerForPickling();

    void bindResources(tuResourceDB* db = NULL, tuResourcePath* path = NULL);
    u_long getEventMask();
    void getLayoutHints(tuLayoutHints* hints);
    void render();
    void setSelected(tuBool);
    int  getNaturalShade();
    
    void resize(int w,  int h);

    void setLeftText(char*);
    void setRightText(char*);
    char* getLeftText()	    { return leftText; }
    char* getRightText()    { return rightText; }
    
    int getLeftTextWidth()  { return leftTextWidth; }	// space text needs
    int getRightTextWidth() { return rightTextWidth; }

    int getLeftWidth()	    { return leftWidth; }       // space text gets
    int getRightWidth()	    { return rightWidth; }
    
    void setLeftPosition(int p)	    { leftPosition = p; }
    void setRightPosition(int p)    { rightPosition = p; }

    int writeToFile(FILE*); // write to specified file

    void setFont(tuFont* font);
  protected:

    static tuResourceChain resourceChain;
    static tuResourceItem resourceItems[];

    void ourPaintText(int x, int y, tuFont* font, tuGC* gc,
		      const char* text, int length, 
		      int startOffset, int maxWidth);

    char* leftText;
    char* rightText;
    int leftTextLen;	// num chars
    int rightTextLen;
    int leftTextWidth;	// required width, in pixels, for whole string
    int rightTextWidth;
    int leftPosition;	// where in string to start painting (in pixels)
    int rightPosition;
    int leftWidth;  	// available width, in pixels, to display string
    int rightWidth;
    int leftX;	    	// x coord of start of left label
    int rightX;
    tuGC* gc;
    tuGC* disabledgc;
    tuGC* selectedgc;
    tuFont* font;
};

#endif /* __dblLabel_h_ */
