/* Standalone gui
 */
#ifndef _GFXGUI_H
#define _GFXGUI_H

#ident "$Revision: 1.32 $"

#include <sys/types.h>

/*  eventHandler() event types.  _WANT named events can be set in type when
 * defining an object to enable their delivery.
 */
#define _REDRAW		0x0001
#define _FREE		0x0002
#define _BUTTON_UP	0x0004
#define _MOTION_RD	0x0008

/* flags */
#define _REDRAW_UNDER	0x0001
#define _INVALID	0x0002
#define _WANT_BUTTON_UP	0x0004
#define _WANT_MOTION_RD	0x0008
#define _GRAB_FOCUS	0x0010
#define _DEFAULT	0x0020
#define _FORCE_PRESSED	0x0040

/* Header of every gui object structure */
struct gui_obj {
	struct gui_obj *next;
	struct gui_obj *prev;
	struct gui_obj *parent;
	short type;
#define GBUTTON		0
#define GTEXT		1
#define GDIALOG		2
#define TFIELD		3
#define CANVAS		4
#define RADIOLIST	5
#define PROGRESSBOX	6
	short flags;
	int x1,y1;
	int x2,y2;
	int state;
#define ST_HIGHL	(1<<0)
#define ST_PRESSED	(1<<1)
#define ST_TEXT		(1<<2)
#define ST_BITMAP	(1<<3)
	void (*eventHandler)(struct gui_obj *, int);
};

struct Text {
	struct gui_obj gui;

	short fg;
	short font;
	char *text;
};

struct Button {
	struct gui_obj gui;

	void (*cb)(struct Button *, __scunsigned_t);
	__scunsigned_t value;
	struct pcbm *pcbm;		/* prom color bitmap */
	const char *text;
	int fg;				/* color of text */
};

struct Dialog {
	struct gui_obj gui;

	struct TextField *textf;
#define DIALOGMAXB	3
	struct Button *button[DIALOGMAXB];
	int bmsg[DIALOGMAXB];
	char *msg;
	int type;
	int flags;
#define DIALOGCLICKED		0x10000000
#define DIALOGBUTTONS		0x20000000
	int which;
	int textw;
	int texth;
};

struct TextField {
	struct gui_obj gui;

	char *text;
	int len;
	int flags;
	int cx;		/* cursor pixel position */
	int cpos;	/* cursor index into text */
	int begpos;	/* index of first visible char */
};

struct ProgressBox {
	struct gui_obj gui;

	int percent;
	int tenth;
};

struct Canvas {
	struct gui_obj gui;
};

struct radioButton {
	struct gui_obj gui;

	struct radioList *parent;
};
struct _radioButton {
	struct radioButton button;
	char *text;
	void *userdata;
};
struct radioList {
	struct gui_obj gui;

	struct _radioButton *selected;
	struct _radioButton *list;
	int maxitems;
	int curitems;
};

/* conversion wrapper from various objects to generic core object */
#define guiobj(X)	(&(X)->gui)

int gfxWidth(void);
int gfxHeight(void);

/* gui init/config routines */
void setGuiMode(int on, int flags);
/* external flags == 1st 16 bits, internal flags == 2nd 16 bits */
#define GUI_NOLOGO	0x0001		/* current screen has no logo */
#define GUI_RESET	0x0002
#define GUI_NEVERLOGO	0x00010000	/* never draw logo (OEM mode) */
#define GUI_LOGOOFF	0x00020000	/* set by logoOff() for some gfx */
int isGuiMode(void);
int doEarlyGui(void);
int doGui(void);
int noLogo(void);
void logoOff(void);

/* generic gui object routines */
void drawObject(struct gui_obj *o);
void deleteObject(struct gui_obj *o);
void moveObject(struct gui_obj *o, int newx1, int newy1);
void centerObject(struct gui_obj *o, struct gui_obj *in);
void setRedrawUnder(struct gui_obj *o);

/* button routines */
struct Button *createButton(int x, int y, int w, int h);
void invalidateButton(struct Button *b, int on);
void stickButton(struct Button *b, int on);
void changeButtonFg(struct Button *b, int color);
void addButtonText(struct Button *b, const char *str);
void addButtonImage(struct Button *b, struct pcbm *map);
void setDefaultButton(struct Button *b, int keepright);
void addButtonCallBack(struct Button *,
			void (*)(struct Button *, __scunsigned_t),
			__scunsigned_t);

/* text and font routines */
struct Text *createText(char *str, int font);
void moveTextPoint(int x, int y);
int textWidth(const char *str, int font);
int fontHeight(int font);

/* textport button convience routine */
void setTpButtonAction(void (*func)(void), int msgnum, int wantbutton);
#define TPBDONE		0
#define TPBCONTINUE	1
#define TPBRETURN	2
#define WBONCHANGE	0
#define WBNOW		1

/* pop-up dialog */
void resizeDialog(struct Dialog *d, int dx, int dy);
struct Button *addDialogButton(struct Dialog *d, int bmsg);
int popupDialog(char *msg, int *bmsg, int type, int flags);
int popupAlignDialog(char *msg, struct gui_obj *align, int *bmsg, int type,
		     int flags);
int popupGets(char *msg, struct gui_obj *align, char * deflt, char *buf,
	      int len);
struct Dialog *_popupDialog(char *msg, int *bmsg, int type, int flags);
int _popupPoll(struct Dialog *d);
struct Dialog *createDialog(char *msg, int type, int flags);
#define DIALOGCONTINUE	0		/* button messages */
#define DIALOGRESTART	1
#define DIALOGSTOP	2
#define DIALOGCANCEL	3
#define DIALOGACCEPT	4
#define DIALOGPREVDEF	1000		/* previous button is default button */
#define DIALOGPREVESC	1001		/* previous button is <esc> button */
#define DIALOGINFO	0		/* dialog types */
#define DIALOGPROGRESS	1
#define DIALOGQUESTION	2	
#define DIALOGWARNING	3
#define DIALOGACTION	4
#define DIALOGCENTER	0x0001		/* flags */
#define DIALOGBIGFONT	0x0002
#define DIALOGFORCEGUI	0x0004

/* text field */
struct TextField *addDialogTextField(struct Dialog *d, int width, int flags);
struct TextField *createTextField(int x, int y, int w, int flags);
void setTextFieldBuffer(struct TextField *t, char *buf, int length);
void setTextFieldString(struct TextField *t, char *str);
int putcTextField(struct TextField *d, int c);
void moveTextFieldCursor(struct TextField *t, int dx);
#define TFIELDBIGFONT	0x0001		/* flags */
#define TFIELDDOTCURSOR	0x0002
#define TFIELDNOUSERIN	0x0004

/* canvas routines */
struct Canvas *createCanvas(int w, int h);
void drawCanvas(struct gui_obj *o);
void clearCanvasArea(struct gui_obj *over);

/* radioList routines */
struct radioList *createRadioList(int x, int y, int w, int h);
struct _radioButton *appendRadioList(struct radioList *l, char *item, int selected);
void resetRadioList(struct radioList *l);
struct _radioButton *setRadioListSelection(struct radioList *l, int sel);
struct _radioButton *getRadioListSelection(struct radioList *l);

/* progress box */
extern struct ProgressBox * createProgressBox(int x, int y, int w);
extern void changeProgressBox(struct ProgressBox *p, int per, int tenth);

/* pon support */
void pongui_setup(char *ponmsg, int checkgui);
void pongui_cleanup(void);

/* misc internal gui routines */
struct htp_state;
void addObject(struct gui_obj *obj);
void cleanGfxGui(void);
void drawBackground(void);
void drawShadedBackground(struct htp_state *htp, int x1,int y1, int x2,int y2);
void _gfxsetms(int x, int y);
void guiCheckMouse(int dx, int dy, int buttons);
void *guimalloc(size_t size);
void guiRefresh(void);
void initGfxGui(struct htp_state *htp);
void putctext(const char *str, int x, int y, int width, int font);
void puttext(const char *str, int font);
void putcltext(const char *, int, int);
void textSize(const char *str, int *w, int *h, int font);
void drawBitmap(unsigned short *, int, int);
int  txOutcharmap(struct htp_state *htp, unsigned char c, int font);
void drawPcbm(struct pcbm *, int, int);
#endif /* _GFXGUI_H */
