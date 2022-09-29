/* pcConsole.h - PC Keyboard and VGA Controller header file */

/* Copyright 1984-1993 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,24sep93,vin  created

*/

#ifndef __INCpcConsoleh
#define __INCpcConsoleh

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _ASMLANGUAGE

#include "tylib.h"

#define NPARS	16			/* number of escape parameters */

/* key board device descriptor */

typedef struct 
    {
    BOOL	curMode;		/* cursor mode TRUE / FALSE */
    int		kbdMode;		/* keyboard mode Japanese/English */
    UINT16	kbdFlags;		/* 16 bit keyboard flags */
    UINT16	kbdState;		/* unshift :shift :cntrl:numeric */
    int		currCon;		/* current console */		
    FUNCPTR 	kbdHook;		/* vga console hook */
    } KBD_CON_DEV;
   
/* vga console device descriptor */

typedef struct 
    {
    UCHAR *	memBase;		/* video memory base */
    UCHAR *	selReg;			/* select register */
    UCHAR *	valReg;			/* value register */
    int 	row, col;		/* current cursor position */
    UCHAR *	curChrPos;      	/* current character position */
    UCHAR	curAttrib;		/* current attribute  */
    UCHAR	defAttrib;      	/* current default attribute */
    int 	nrow, ncol;		/* current screen geometry */
    int 	scst, sced;		/* scroll region from to */
    BOOL	rev;			/* revarse mode char */
    BOOL	autoWrap;		/* auto Wrap mode */
    BOOL	sv_rev;			/* saved revarse mode char */
    int 	sv_row, sv_col;		/* saved cursor position */
    UCHAR	sv_curAttrib;	 	/* saved attributes */
    BOOL	scrollCheck;     	/* scroll check */
    UCHAR *	charSet;         	/* character set Text or Graphics */
    int		vgaMode;         	/* added to support graphics Mode */
    BOOL	colorMode;       	/* color mode MONO / COLOR */
    BOOL	insMode;         	/* insert mode on / off */
    char	tab_stop [80];      	/* tab stop mark */
    UINT16	escFlags;		/* 16 bit escape flags */
    int 	escParaCount;	 	/* seen escape parameters (count) */
    int 	escPara[NPARS];  	/* parameters */
    BOOL	escQuestion;     	/* ? mark in escape sequence */
    char	escResp[10];     	/* esc sequence response buffer */
    FUNCPTR     vgaHook;		/* key board hook */
    } VGA_CON_DEV;


/* pc console device descriptor */

typedef struct				/* CON_DRV_DEV */
    {
    TY_DEV	tyDev;
    BOOL	created;		/* true if this device is created */
    KBD_CON_DEV * ks;			/* pointer to keyboard descriptor */
    VGA_CON_DEV * vs;			/* pointer to vga console descriptor */
    } PC_CON_DEV;

/******************************************************************************
*
* Keyboard  definitions
*/

#define WAIT_MAX	100	/* Maximum wait time for keyboard */
#define E0_BASE		0x80	/* enhanced keyboard base */
#define EXTND_SIZE	16	/* no keys extra with extended code 0xe0 */

/* keyboard function table Index */

#define AS	0	/* normal character index */
#define SH	1	/* shift index */
#define CN	2	/* control index */
#define NM	3	/* numeric lock index */
#define CP	4	/* capslock index */
#define ST	5	/* stop output index */
#define EX	6	/* extended code index */
#define ES	7	/* escape and extended code index */

/* Keyboard special key flags */

#define NORMAL		0x0000		/* normal key */
#define STP		0x0001  	/* capslock flag */
#define NUM		0x0002  	/* numeric lock flag */
#define CAPS		0x0004  	/* scroll lock stop output flag */
#define SHIFT		0x0008  	/* shift flag */
#define CTRL		0x0010  	/* control flag */
#define EXT		0x0020  	/* extended scan code 0xe0 */
#define ESC		0x0040  	/* escape key press */
#define	EW		EXT|ESC 	/* escape and Extend */
#define E1		0x0080  	/* extended scan code with 0xE1 */
#define PRTSC		0x0100  	/* print screen flag */
#define BRK		0x0200		/* make break flag for keyboard */

/* keyboard  on off defines */

#define	K_ON  		0xff	/* key  */
#define	K_OFF  		0x00	/* key  */

/* monitor definitions */

#define	TEXT_SET		0	/* Normal text set */
#define	GRAPHICS_VT100_SET	1	/* vt100 graphics set */
#define	IBM_GRAPHICS_SET	2	/* IBM graphics character set */
#define	TEXT_MODE		0	/* monitor in text mode */
#define	GRAPHICS_MODE		1	/* monitor in graphics mode */
#define	INSERT_MODE_OFF		0	/* character insert mode off */
#define	INSERT_MODE_ON		1	/* character insert mode on */
#define	FG_ATTR_MASK		0x07	/* foreground attribute mask */
#define	BG_ATTR_MASK		0x70	/* background attribute mask */
#define	INT_BLINK_MASK		0x88	/* intensity and blinking mask */
#define	FORWARD			1	/* scroll direction forward */
#define	BACKWARD		0       /* scroll direction backward */

/* escape flags */

#define  ESC_NORMAL		0x0001	/* normal state */
#define  ESC_ESC           	0x0002	/* ESC state */
#define  ESC_BRACE         	0x0004	/* ESC [ state */
#define  ESC_GET_PARAMS    	0x0008	/* ESC [ n state */
#define  ESC_GOT_PARAMS    	0x0010  /* ESC [ n;n;n; state */
#define  ESC_FUNC_KEY      	0x0020	/* ESC [ [ state */
#define  ESC_HASH          	0x0040	/* ESC # state */
#define  ESC_SET_TEXT      	0x0080	/* ESC ( state */
#define  ESC_SET_GPRAHICS  	0x0100	/* ESC ) state */

/* ioctl and attribute definitions */

#define CONIOSETATRB		1001
#define CONIOGETATRB		1002
#define CONIOSETKBD		1003
#define CONIOSCREENREV		1004
#define CONIOBEEP		1005
#define CONIOCURSORON		1006
#define CONIOCURSOROFF		1007
#define CONIOCURSORMOVE		1008
#define CONIOCURCONSOLE		1009

#define UNDERLINE               0x01   /* only if monochrome */
#define ATRB_FG_BLACK		0x00
#define ATRB_FG_BLUE		0x01
#define ATRB_FG_GREEN		0x02
#define ATRB_FG_CYAN		0x03
#define ATRB_FG_RED		0x04
#define ATRB_FG_MAGENTA		0x05
#define ATRB_FG_BROWN		0x06
#define ATRB_FG_WHITE		0x07
#define ATRB_BRIGHT		0x08
#define ATRB_FG_GRAY		(ATRB_FG_BLACK   | ATRB_BRIGHT)
#define ATRB_FG_LIGHTBLUE	(ATRB_FG_BLUE    | ATRB_BRIGHT)
#define ATRB_FG_LIGHTGREEN	(ATRB_FG_GREEN   | ATRB_BRIGHT)
#define ATRB_FG_LIGHTCYAN	(ATRB_FG_CYAN    | ATRB_BRIGHT)
#define ATRB_FG_LIGHTRED	(ATRB_FG_RED     | ATRB_BRIGHT)
#define ATRB_FG_LIGHTMAGENTA	(ATRB_FG_MAGENTA | ATRB_BRIGHT)
#define ATRB_FG_YELLOW		(ATRB_FG_BROWN   | ATRB_BRIGHT)
#define ATRB_FG_BRIGHTWHITE	(ATRB_FG_WHITE   | ATRB_BRIGHT)
#define ATRB_BG_BLACK		0x00
#define ATRB_BG_BLUE		0x10
#define ATRB_BG_GREEN		0x20
#define ATRB_BG_CYAN		0x30
#define ATRB_BG_RED		0x40
#define ATRB_BG_MAGENTA		0x50
#define ATRB_BG_BROWN		0x60
#define ATRB_BG_WHITE		0x70
#define ATRB_BLINK		0x80
#define ATRB_CHR_REV		0x0100

/* function declarations */
#if defined(__STDC__) || defined(__cplusplus)

extern	void	kbdIntr (void);
extern  void 	kbdHrdInit (void);
extern	int	pcConDrv (void);
extern	int	pcConDevCreate (char *name, FAST int channel, int rdBufSize,
			      int wrtBufSize);
extern  void	vgaHrdInit (void);
extern	int 	vgaWriteString (FAST PC_CON_DEV * pPcCoDv);

#else
extern	void	kbdIntr ();
extern	void	kbdHrdInit ();
extern	int	pcConDrv ();
extern	int	pcConDevCreate ();
extern  void	vgaHrdInit (void);
extern  int 	vgaWriteString ();

#endif  /* __STDC__ */

#endif  /* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCpcConsoleh */




