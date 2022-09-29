#ifndef __tuDialogBox_h_
#define __tuDialogBox_h_

// $Revision: 1.1 $
// $Date: 1996/02/26 01:28:23 $
#include "tuTopLevel.h"

class tuRowColumn;
class tuBitmapTile;

class tuDialogBox : public tuTopLevel {
  public:
    tuDialogBox(const char* instanceName, tuColorMap* cmap,
		tuResourceDB* db, char* progName, char* progClass,
		Window transientFor = NULL);
    tuDialogBox(const char* instanceName, tuTopLevel* othertoplevel,
		tuBool transientForOtherTopLevel = False);
    ~tuDialogBox(void);

    virtual void map(tuBool propagate = True);

    virtual void setText(const char* s);
    virtual void setText(const char* s[]);

    tuBitmapTile* getBitmapTile(void) { return bitmap; }
    tuRowColumn* getButtonContainer(void) { return buttonContainer; }

  protected:
    void initialize(void);

  private:
    tuRowColumn* textContainer;
    tuRowColumn* buttonContainer;
    tuBitmapTile* bitmap;
};

#endif /* __tuDialogBox_h */
