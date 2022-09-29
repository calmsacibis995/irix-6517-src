#ifndef	__myLabel_h_
#define	__myLabel_h_

// $Revision: 1.2 $
// $Date: 1992/10/16 02:18:58 $

//  same as tuLabel, except text is a fixed-size char array,
// to avoid all the deletes & strdups

class tuGadget;
class tuLabel;

class myLabel : public tuLabel {
  public:
    myLabel(tuGadget* parent, const char* instanceName, const char* text);
    ~myLabel();
    
    static void registerForPickling();

    void setText(const char* t);

  protected:
    char    textArray[21];
    static tuResourceChain resourceChain;
};

#endif /* __myLabel_h_ */
