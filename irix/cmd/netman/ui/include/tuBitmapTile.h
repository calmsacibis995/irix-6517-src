#ifndef	__tuBitmapTile_h_
#define	__tuBitmapTile_h_

// $Revision: 1.1 $
// $Date: 1996/02/26 01:28:23 $
#include "tuGadget.h"

class tuGC;

class tuBitmapTile : public tuGadget {
  public:
    tuBitmapTile(tuGadget* parent, const char* instanceName);
    ~tuBitmapTile();

    static void registerForPickling();

    void setBitmap(const char* data, unsigned int width, unsigned int height);

    virtual void render();
    virtual void bindResources(tuResourceDB*db=NULL,tuResourcePath*path=NULL);
    virtual void getLayoutHints(tuLayoutHints* hints);

  protected:

    static tuResourceChain resourceChain;
    static tuResourceItem resourceItems[];

    char *fileName;
    Pixmap stencil;
    short pix_width, pix_height;
    tuGC* fgc;
    GC stencilgc;
};

#endif /* __tuBitmapTile_h_ */
