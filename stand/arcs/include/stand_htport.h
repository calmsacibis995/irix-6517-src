#ifndef __SYS_STAND_HTPORT_H__
#define __SYS_STAND_HTPORT_H__

/*
 * stand_htport.h
 *
 * Header file for standalone host textport.
 */

#ident "$Revision: 1.34 $"

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

/* textport color map
 * 0-7		black -> white		(defined in <style.h>)
 * 8		screen
 * 9		textport		(defined in <style.h>)
 * 10		panel
 * 14		inr border
 * 20-32	sgiish colors for dialog(defined in <style.h>)
 * 35-39	grays			(defined in <style.h>)
 * 40-47	high intensity black -> white
 * 48-50	logo colors for gfxgui
 * 51-58	b->w dither (GL 8-15)	(defined in <style.h>)
 * 59-63	machdep icon colors	(defined in <style.h>)
 * 96-255	background shading ramp
 */
#define SCR_IDX		8
#define PNL_IDX		10

#define HIGHLBASE	40

#define TOPCOLOR_R 0xb0
#define TOPCOLOR_G 0xd2
#define TOPCOLOR_B 0xff

#define BOTTOMCOLOR_R 0x5f
#define BOTTOMCOLOR_G 0x5f
#define BOTTOMCOLOR_B 0x8f

#define RAMPSIZE 160
#define RAMPSTART (256-RAMPSIZE)

#define	TP_MAXROWS	40
#define	TP_MAXCOLS	80	

#define MAXAPARAMS 4

#define SCR_DF_R		127
#define SCR_DF_G		158
#define SCR_DF_B		191

#define TP_DF_R			96
#define TP_DF_G			96
#define TP_DF_B			112

#define	XBORDER	2
#define	YBORDER	2
#define BOX_HT		20
#define	BOX_DC		6
#define	PNL_MAXROWS	20
#define	PNL_MAXCOLS	80

#define chartoindex(c)		((c) - (((c)&0x80) ? 2*' '+2 : ' '))
struct fontinfo {			/* should match GL Fontchar */
	short offset;
	char xsize;
	char ysize;
	signed char xorig;
	signed char yorig;
	short xmove;
};

struct htp_font {
	unsigned short *data;
	struct fontinfo *info;
	short height;			/* maximum total height */
	short descender;		/* maximum descender */
};

/*
 * textport modes
 */
#define HCTPCODE(c)		('G'<<8|(c))
#define TP_MODE			HCTPCODE(2) /* basic textport "TP" */
#define PNL_MODE		HCTPCODE(3) /* boot panel "PNL" */
#define AUTO_INIT		HCTPCODE(4)
#define GUI_MODE		HCTPCODE(5) /* gfx gui mode */

/* flags */
#define CURSOFF		0x01
#define TP_NOBG		0x02
#define TP_AUTOWRAP	0x04
#define TP_WANTDONEB	0x08
#define TP_DONEB	0x10

#define SCRB18_HT	17
#define SCRB18_DC	4

#define	FONTWIDTH	9
#define	XSIZE(cols)	((cols) * FONTWIDTH)

/*
 * graphical characters recognized in the BOOT PANEL mode
 */
#define		L_BRAK		((char) 1)
#define		U_BAR		((char) 2)
#define		O_BAR		((char) 5)
#define		R_BRAK		((char) 6)

/* astates */
#define NORM		0
#define ESC		1
#define CSI		2
#define CSICONT		3

#define	CURSOR_COLOR		GREEN
#define	BORDER_COLOR		WHITE
#define INR_BORDER		14

/* states */
#define	CLEAN		0
#define	DIRTY		1
#define VOFF		2
#define GUI		3

#define	     DIRTYLINE(tx, r) (tx)->row[r]->dirty = 1
#define	     DIRTYCURSOR(tx, r) (tx)->row[r]->dirty = 2

/*
 * special graphics attributes and characters
 */
#define		AT_UL		(1<<8)
#define		AT_RV		(2<<8)
#define		AT_BX		(4<<8)
#define		AT_HL		(8<<8)
#define		IS_UL(c)	((c) & AT_UL)
#define		IS_RV(c)	((c) & AT_RV)
#define		IS_BX(c)	((c) & AT_BX)
#define		IS_HL(c)	((c) & AT_HL)

struct htp_bitmap {
	unsigned short *buf;
	short xsize, ysize;
	short xorig, yorig;
	short xmove, ymove;
	short sper;
};

struct htp_fncs {
        void    (*blankscreen)(void*, int);
        void    (*color)(void*, int);
        void    (*mapcolor)(void*, int, int, int, int);
        void    (*sboxfi)(void*, int, int, int, int);
        void    (*pnt2i)(void*, int, int);
        void    (*cmov2i)(void*, int, int);
        void    (*drawbitmap)(void*, struct htp_bitmap*);
	/* block move -- leave as 0 if gfx does not support this */
	void	(*blockm)(void*, int, int, int, int, int, int);
	void	(*init)(void*, int*, int*);
	void	(*movec)(void*, int, int);		/* move cursor */
};

/*
 *  htport data structures
 */
struct htp_row {
	char	maxcol;			/* last used column */
	char	dirty;			/* 1 if row needs painting,
					 * 2 if cursor needs to be erased
					 */
	unsigned short data[TP_MAXCOLS];
#ifdef _TP_COLOR
	unsigned char color[TP_MAXCOLS];
#endif
};

struct htp_textport {
	char	state;
	char	astate;
	short	llx, lly;	/* coordinates of lower window corner */
	short	numrows;	/* number of rows displayed */
	short	numcols;	/* number of columns displayed */
	short	crow, ccol;	/* cursor row & column */
	short	aparams[MAXAPARAMS];
	short	aparamc;
	short	aflags;
	unsigned short	attribs;
	struct htp_row *row[TP_MAXROWS];
	struct htp_row rowd[TP_MAXROWS]; /* textport presumed bigger than panel */
};

struct htp_state {
	/* public (used by gui) */
	struct htp_fncs *fncs;
	void *hw;
	struct htp_font *fonts;		/* pointer to font data */
	int x,y,buttons;
	void (*mshandler)(int, int, int);
	int clogow, clogoh;		/* product logo w/h */
	unsigned char *clogodata;	/* product logo data */

	/* private to textport */
	int xscreensize, yscreensize;
	int fontheight, fontdescender;
	int boot_panel_mode;
	int bgnd;
	int fgnd;
	int cfgnd;
	int cbgnd;
	int page_r, page_g, page_b;
	struct htp_textport textport;	/* must be last for _TP_COLOR */
};

#endif /* C || C++ */

#endif /* !__SYS_STAND_HTPORT_H__ */
