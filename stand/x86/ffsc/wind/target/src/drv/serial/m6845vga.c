/* m6845Vga.c - motorola 6845 vga console driver routines */

/* Copyright 1993-1993 Wind River System, Inc. */
#include "copyright_wrs.h"

/*
modification history
--------------------
01d,03aug95,myz  fixed the warning message
01c,14jun95,hdn  removed function declarations defined in sysLib.h.
01b,30mar94,hdn  changed CONSOLE_TTY to PC_CONSOLE.
01a,20oct93,vin  created
*/

/*
DESCRIPTION
This is the driver fo Video Contoroller Chip (6845) normally  used in the
386/486 personal computers.

USER CALLABLE ROUTINES
The routines in this driver are accessed from a generic console
driver through the hook routine, the pointer to which is initialized
in the VGA_CON_DEV structure at the time of initialization.
This makes the access to these routines more generic without declaring
these functions as external. 

The routines in this driver which are accessed from an external module
are vgaWriteString() and vgaHrdInit(). vgaWriteString() is installed
as a device transmit start-up routine in tyDevInit(). vgaHrdInit() is
called to initialize the device descriptors and the vga console.

All virtual consoles are mapped to the same screen buffer. This is a 
very basic implementation of virtual consoles. Multiple screen
buffers are not used to switch between consoles. This implementation
is left for the future. Mutual exclusion for the screen buffer is 
guaranteed within the same console but it is not implemented across multiple
virtual consoles because all virtual consoles use the same screen buffer. If
multiple screen buffers are implemented then the mutual exclusion between
virtual consoles can be implemented.

Before using the driver, it must be initialized by calling vgaHrdInit(). 
This routine should be called exactly once. Normally, it is called from 
a generic driver for eg. from tyCoDrv() which is called before tyCoDevCreate.


SEE ALSO
tyLib

NOTES
The macro N_VIRTUAL_CONSOLES should be defined in config.h file.
This refers to the number of virtual consoles which the user wishes to have.
The user should define INCLUDE_ANSI_ESC_SEQUENCE in <target>.h file if
the ansi escape sequences are required. Special processing in the vga
driver is done if an escape sequence exists.
*/

/* includes */

#include "vxworks.h"
#include "iv.h"
#include "iolib.h"
#include "ioslib.h"
#include "memlib.h"
#include "rnglib.h"
#include "tylib.h"
#include "private/funcbindp.h"
#include "intlib.h"
#include "tasklib.h"
#include "errnolib.h"
#include "stdio.h"
#include "string.h"
#include "config.h"
#include "drv/serial/pcconsole.h"


/* externals */

IMPORT PC_CON_DEV	pcConDv [N_VIRTUAL_CONSOLES] ;


/* locals */

LOCAL VGA_CON_DEV	vgaConDv [N_VIRTUAL_CONSOLES];
LOCAL UCHAR		curSt,curEd;		/* current cursor mode */
LOCAL int 		tyWrtThreshold  = 20;	/* min bytes free in output 
						 * buffer before the next 
						 * writer will be enabled */
LOCAL UCHAR * vgaCharTable [] = 
    {
    /* 8-bit Latin-1 mapped to the PC charater set: '\0' means non-printable */
    (unsigned char *)
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    " !\"#$%&'()*+,-./0123456789:;<=>?"
    "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
    "`abcdefghijklmnopqrstuvwxyz{|}~\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\040\255\233\234\376\235\174\025\376\376\246\256\252\055\376\376"
    "\370\361\375\376\376\346\024\371\376\376\247\257\254\253\376\250"
    "\376\376\376\376\216\217\222\200\376\220\376\376\376\376\376\376"
    "\376\245\376\376\376\376\231\376\235\376\376\376\232\376\376\341"
    "\205\240\203\376\204\206\221\207\212\202\210\211\215\241\214\213"
    "\376\244\225\242\223\376\224\366\233\227\243\226\201\376\376\230",
    /* vt100 graphics */
    (unsigned char *)
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    " !\"#$%&'()*+,-./0123456789:;<=>?"
    "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^ "
    "\004\261\007\007\007\007\370\361\040\007\331\277\332\300\305\007"
    "\007\304\007\007\303\264\301\302\263\007\007\007\007\007\234\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\040\255\233\234\376\235\174\025\376\376\246\256\252\055\376\376"
    "\370\361\375\376\376\346\024\371\376\376\247\257\254\253\376\250"
    "\376\376\376\376\216\217\222\200\376\220\376\376\376\376\376\376"
    "\376\245\376\376\376\376\231\376\376\376\376\376\232\376\376\341"
    "\205\240\203\376\204\206\221\207\212\202\210\211\215\241\214\213"
    "\376\244\225\242\223\376\224\366\376\227\243\226\201\376\376\230",
    /* IBM graphics: minimal translations (CR, LF, LL, SO, SI and ESC) */
    (unsigned char *)
    "\000\001\002\003\004\005\006\007\010\011\000\013\000\000\000\000"
    "\020\021\022\023\024\025\026\027\030\031\032\000\034\035\036\037"
    "\040\041\042\043\044\045\046\047\050\051\052\053\054\055\056\057"
    "\060\061\062\063\064\065\066\067\070\071\072\073\074\075\076\077"
    "\100\101\102\103\104\105\106\107\110\111\112\113\114\115\116\117"
    "\120\121\122\123\124\125\126\127\130\131\132\133\134\135\136\137"
    "\140\141\142\143\144\145\146\147\150\151\152\153\154\155\156\157"
    "\160\161\162\163\164\165\166\167\170\171\172\173\174\175\176\177"
    "\200\201\202\203\204\205\206\207\210\211\212\213\214\215\216\217"
    "\220\221\222\223\224\225\226\227\230\231\232\233\234\235\236\237"
    "\240\241\242\243\244\245\246\247\250\251\252\253\254\255\256\257"
    "\260\261\262\263\264\265\266\267\270\271\272\273\274\275\276\277"
    "\300\301\302\303\304\305\306\307\310\311\312\313\314\315\316\317"
    "\320\321\322\323\324\325\326\327\330\331\332\333\334\335\336\337"
    "\340\341\342\343\344\345\346\347\350\351\352\353\354\355\356\357"
    "\360\361\362\363\364\365\366\367\370\371\372\373\374\375\376\377"
    };


/* forward declarations */

LOCAL void	vgaStatInit (void);
LOCAL void	vgaHook (FAST VGA_CON_DEV *  pVgaConDv, int arg, int opCode);
LOCAL void	vgaClear (FAST VGA_CON_DEV *  pVgaConDv, int position, 
			  UCHAR eraseChar);
LOCAL void	vgaScroll (FAST VGA_CON_DEV *  pVgaConDv, int pos, int line, 
			   BOOL upDn, FAST UCHAR atr);
LOCAL void	vgaInsertChar (FAST VGA_CON_DEV *  pVgaConDv, 
			       FAST int nInsChar);
LOCAL void 	vgaCursorOn (void);
LOCAL void	vgaCursorOff (void);
LOCAL void	vgaCursorPos (FAST UINT16 pos);
LOCAL void	vgaScreenRev (FAST VGA_CON_DEV * pVgaConDv);
LOCAL void	vgaDelLeftChar (FAST VGA_CON_DEV *  pVgaConDv);
LOCAL void	vgaCarriageReturn (FAST VGA_CON_DEV *  pVgaConDv);
LOCAL void	vgaBackSpace (FAST VGA_CON_DEV *  pVgaConDv);
LOCAL void	vgaLineFeed (FAST VGA_CON_DEV *  pVgaConDv);
LOCAL void	vgaTab (FAST VGA_CON_DEV *  pVgaConDv);
LOCAL void	vgaConBeep (BOOL mode);

#ifdef INCLUDE_ANSI_ESC_SEQUENCE

LOCAL UCHAR vgaColorTable [] = 
    {
    /* black, red, green, brown, blue, magenta, cyan, light grey */
           0,   4,     2,     6,    1,       5,    3,          7
    };
LOCAL void	vgaEscResponse (FAST PC_CON_DEV * pPcCoDv, int  responseId );
LOCAL void	vgaPutCursor (FAST VGA_CON_DEV *  pVgaConDv);
LOCAL void	vgaSaveCurAttrib (FAST VGA_CON_DEV *  pVgaConDv);
LOCAL void	vgaRestAttrib (FAST VGA_CON_DEV *  pVgaConDv);
LOCAL void	vgaSetMode (FAST  PC_CON_DEV *  pPcCoDv, BOOL onOff);
LOCAL void	vgaClearLine (FAST VGA_CON_DEV *  pVgaConDv);
LOCAL void	vgaInsertLine (FAST VGA_CON_DEV *  pVgaConDv);
LOCAL void	vgaSetAttrib (FAST VGA_CON_DEV *  pVgaConDv);
LOCAL void	vgaDelLines (FAST VGA_CON_DEV *  pVgaConDv);
LOCAL void	vgaDelRightChars (FAST VGA_CON_DEV *  pVgaConDv, int nDelChar);

#endif /* INCLUDE_ANSI_ESC_SEQUENCE */

/*******************************************************************************
*
* vgaConBeep - sound beep function (using timer 2 for tone)
* 
* This function is responsible for producing the beep 
*
* RETURNS: N/A
*
* NOMANUAL
*/

LOCAL void vgaConBeep 
    (
    BOOL	mode	/* TRUE:long beep  FALSE:short beep */
    )
    {
    int		beepTime;
    int		beepPitch;
    FAST int 	oldlevel;

    if (mode)
        { 
        beepPitch = BEEP_PITCH_L;
        beepTime  = BEEP_TIME_L;	/* long beep */
        }
    else
        { 
        beepPitch = BEEP_PITCH_S;
        beepTime  = BEEP_TIME_S;	/* short beep */
        }

    oldlevel = intLock ();

    /* set command for counter 2, 2 byte write */

    sysOutByte (PIT_BASE_ADR + 3, 0xb6);	
    sysOutByte (PIT_BASE_ADR + 2, (beepPitch & 0xff));
    sysOutByte (PIT_BASE_ADR + 2, (beepPitch >> 8));

    /* enable counter 2 */
    sysOutByte (DIAG_CTRL, sysInByte (DIAG_CTRL) | 0x03);	

    taskDelay (beepTime);

    /* desable counter 2 */
    sysOutByte (DIAG_CTRL, sysInByte (DIAG_CTRL) & ~0x03);

    intUnlock (oldlevel);
    return;
    }

/******************************************************************************
*
* vgaHrdInit - initialize the VGA Display
*
* This function is called externally to initialize the vga display
*
* RETURNS: N/A
*
* NOMANUAL
*/

void vgaHrdInit (void)
    {

    /* get cursor shape and mode */

    sysOutByte ((int) CTRL_SEL_REG, 0x0a);
    curSt = sysInByte ((int) CTRL_VAL_REG);
    sysOutByte ((int) CTRL_SEL_REG, 0x0b);
    curEd = sysInByte ((int) CTRL_VAL_REG);

    vgaStatInit ();

    /* clear screen and position cursor at 0,0 */

    vgaClear (pcConDv [PC_CONSOLE].vs, 2,' ');
    vgaCursorOn ();
    return;
    } 

/*******************************************************************************
*
* vgaStatInit - initialize the VGA Display state
*
* RETURNS: N/A
*/

LOCAL void vgaStatInit (void)
    {
    int		ix;			/* to hold the index */
    FAST VGA_CON_DEV * pVgaConDv;	/* pointer to hold vga descriptor */

    for (ix = 0; ix < N_VIRTUAL_CONSOLES; ix++)
	{
	pcConDv [ix].vs	= pVgaConDv = &vgaConDv [ix];

	/* (VGA) Display status initialization */
	pVgaConDv->memBase	 = CTRL_MEM_BASE;
	pVgaConDv->colorMode     = COLOR_MODE;      /* color mode */   
	pVgaConDv->sv_col        = pVgaConDv->col   = 0;
	pVgaConDv->sv_row        = pVgaConDv->row   = 0;
	pVgaConDv->sv_curAttrib  = pVgaConDv->curAttrib = DEFAULT_ATR;
	pVgaConDv->defAttrib     = DEFAULT_ATR;
	pVgaConDv->curChrPos     = pVgaConDv->memBase;  /* current  position */
	pVgaConDv->sv_rev        = pVgaConDv->rev   = FALSE;
	pVgaConDv->ncol          = 80;                /* Number of columns */
	pVgaConDv->nrow          = 25;                /* Number of text rows */
	pVgaConDv->scst          = 0;                 /* scroll start */
	pVgaConDv->sced          = 24;                /* scroll end */
	pVgaConDv->autoWrap      = TRUE;              /* auto Wrap mode */
	pVgaConDv->scrollCheck   = FALSE;             /* scroll  flag off */
	pVgaConDv->charSet       = vgaCharTable [TEXT_SET]; /* character set */
	pVgaConDv->vgaMode       = TEXT_MODE;         /* video mode  */
	pVgaConDv->insMode       = INSERT_MODE_OFF;   /* insert mode */
	pVgaConDv->escFlags      = ESC_NORMAL;        /* normal character */
	pVgaConDv->escParaCount  = 0;                 /* zero parameters */
	pVgaConDv->escQuestion   = FALSE;             /* ? flag set to false */
	bzero ((char *)pVgaConDv->escPara, sizeof(pVgaConDv->escPara));
	bzero (pVgaConDv->tab_stop, sizeof(pVgaConDv->tab_stop));
	pVgaConDv->tab_stop [ 0] = 1;
	pVgaConDv->tab_stop [ 8] = 1;
	pVgaConDv->tab_stop [16] = 1;
	pVgaConDv->tab_stop [24] = 1;
	pVgaConDv->tab_stop [32] = 1;
	pVgaConDv->tab_stop [40] = 1;
	pVgaConDv->tab_stop [48] = 1;
	pVgaConDv->tab_stop [56] = 1;
	pVgaConDv->tab_stop [64] = 1;
	pVgaConDv->tab_stop [72] = 1;
	pVgaConDv->vgaHook	 = (FUNCPTR) vgaHook;	/* install the hook */
	}
    return;
    } 

/******************************************************************************
*
* vgaHook - hook called from an external module
* 
* This function performs various functions depending on the opCode.
* Typically this hook is to facilitate the user to add any additional
* routines which can be accessed externally through the vga console
* descriptor.
*
* RETURNS: N/A
*/

LOCAL void vgaHook 
    (
    FAST VGA_CON_DEV *  pVgaConDv,      /* pointer to the vga descriptor */
    int			arg,		/* argument */
    int			opCode		/* operation to perform */
    )
    {
    switch (opCode)
	{
	case 0:
	      vgaScreenRev (pVgaConDv); /* reverse screen */
	      break;
	case 1:
	      vgaConBeep (arg);		/* produce beep */
	      break;
	case 2:
	      vgaCursorOn ();		/* turn the cursor on */
	      break;
	case 3:
	      vgaCursorOff ();		/* turn the cursor Off */
	      break;
	case 4:				/* position the cursor */
	      pVgaConDv->row = (arg >> 8) & 0xff;
	      pVgaConDv->col = arg & 0xff;
	      if (pVgaConDv->row >= pVgaConDv->nrow)
	      pVgaConDv->row = pVgaConDv->nrow - 1;
	      if (pVgaConDv->col >= pVgaConDv->ncol)
	      pVgaConDv->col = pVgaConDv->ncol - 1;
	      vgaCursorPos ((UINT16) (pVgaConDv->row * pVgaConDv->ncol 
				      + pVgaConDv->col));
	      break;
	default:
	      break;
	}
    }

/******************************************************************************
*
* vgaClear - Clear Screen
*
* This routine clears the screen depending on the value of position.
* If position is 2 then it clears the whole screen and moves the cursor to 
* location 0,0. If position is 1 then the screen is erased from the cursor to
* until the end of display. If position is equal to any other value then 
* the screen is erased until the cursor.
*
* RETURNS: N/A
*/

LOCAL void vgaClear 
    (
    FAST VGA_CON_DEV *	pVgaConDv,	/* pointer to the vga descriptor */
    int			position,	/* cursor position  0 or 1 or 2 */
    UCHAR		eraseChar	/* erase character eg space is  0x20 */
    )
    {
    FAST UINT16 *	cp;	/* hold the beginning position */
    FAST UINT16 *	end;	/* hold the end position */
    FAST UINT16 	erase;	/* erase character with attribute */

    erase = (pVgaConDv->defAttrib << 8) + eraseChar;
    if ( position == 2 )
	{ 
	cp  = (UINT16 *) pVgaConDv->memBase;
	end = (UINT16 *) (pVgaConDv->memBase + 2048 * CHR);
	pVgaConDv->col = pVgaConDv->row = 0;
	pVgaConDv->curChrPos = (UCHAR *) pVgaConDv->memBase;
	}
    else if ( position == 0 )
	{ 
	cp  = (UINT16 *)pVgaConDv->curChrPos;
	end = (UINT16 *)(pVgaConDv->memBase + 2048 * CHR);
	}
    else  
	{
	cp  = (UINT16 *) pVgaConDv->memBase;
	end = (UINT16 *) (pVgaConDv->curChrPos + CHR);
	}
    for (; cp < end; cp++ )
        {
        *cp     = erase;  
        }
    }

/******************************************************************************
*
* vgaScreenRev - Reverse Screen
*
* RETURNS: N/A
*/

LOCAL void vgaScreenRev 
    (
    FAST VGA_CON_DEV * pVgaConDv	/* pointer to the vga descriptor */
    )
    {
    UCHAR *	cp;	/* to hold the current pointer */
    UCHAR	atr;	/* to hold the attribute character */

    for (cp = pVgaConDv->memBase; cp < pVgaConDv->memBase + 2000 * CHR; 
	 cp += CHR)
        {
        atr = *(cp+1);
        *(cp+1)  = atr & INT_BLINK_MASK;
        *(cp+1) |= (atr << 4) & BG_ATTR_MASK;
        *(cp+1) |= (atr >> 4) & FG_ATTR_MASK;
        }
    }

/*******************************************************************************
*
* vgaScroll - Scroll Screen
*
* This function scrolls the screen according to the scroll direction.
* scroll direction FORWARD or BACKWARD 
*
* RETURNS: N/A
*/

LOCAL void vgaScroll 
    (
    FAST VGA_CON_DEV *	pVgaConDv,	/* pointer to the vga descriptor */
    int			pos,		/* scroll start line position */
    int			lines,		/* scroll line count */
    BOOL		upDn,		/* scroll direction  */
    FAST UCHAR		atr		/* atrribute for char */
    )
    {
    FAST UCHAR *	dest;	/* to hold the destination pointer */
    FAST UCHAR *	src;	/* to hold the source pointer */

    if (pos < pVgaConDv->scst || pos > pVgaConDv->sced)
        return;

    if (upDn)
        {
	/* scroll direction is forward */

        if (pVgaConDv->scst + lines > pVgaConDv->sced + 1)
            lines = pVgaConDv->sced - pVgaConDv->scst + 1;

        for (dest = pVgaConDv->memBase + pVgaConDv->ncol * CHR * 
	     pVgaConDv->scst, src = pVgaConDv->memBase + pVgaConDv->ncol * 
	     CHR * (pVgaConDv->scst + lines); src < pVgaConDv->memBase + 
	     pVgaConDv->ncol * CHR * (pos + 1); *dest++ = *src++ );

        for (dest = pVgaConDv->memBase + pVgaConDv->ncol * CHR * 
	     (pos - (lines - 1)); dest < pVgaConDv->memBase + pVgaConDv->ncol *
	     CHR * (pos + 1); dest += CHR )
            {
            *dest     = ' ';
            *(dest+1) = atr;
            }
        }
    else
        {
	/* scroll direction is backward */

        if (pVgaConDv->scst + lines > pVgaConDv->sced + 1)
            lines = pVgaConDv->sced - pVgaConDv->scst + 1;

        for (dest = pVgaConDv->memBase + pVgaConDv->ncol * CHR * 
	     (pVgaConDv->sced + 1) - 1, src = pVgaConDv->memBase + 
	     pVgaConDv->ncol * CHR * (pVgaConDv->sced - (lines - 1)) - 1;
             src > pVgaConDv->memBase + pVgaConDv->ncol * CHR * pos - 1;
             *dest-- = *src-- );

        for (dest = pVgaConDv->memBase + pVgaConDv->ncol * CHR * 
	     (pos + lines) - 1; dest > pVgaConDv->memBase + pVgaConDv->ncol * 
	     CHR * pos - 1;
             dest -= CHR )
            {
            *dest     = atr;
            *(dest-1) = ' ';
            }
        }
    return;
    }

/*****************************************************************************
*
* vgaInsertChar - insert the character at the current cursor location
*
* RETURNS: N/A
*/

LOCAL void vgaInsertChar
    (
    FAST VGA_CON_DEV *	pVgaConDv,	/* pointer to the vga descriptor */
    FAST int		nInsChar	/* number of insert Characters */
    )
    {
    FAST int		xPos ;  /* to hold the horizontal position  */
    FAST UINT16   	erase;	/* to hold erase character with attribute */
    FAST UINT16   	swap;	/* to hold old x position */
    FAST UINT16 *	chrPos;	/* to hold actual address of character */

    if (nInsChar > pVgaConDv->ncol)
	nInsChar = pVgaConDv->ncol;

    else if (nInsChar == 0) 
	nInsChar = 1;
    
    while (nInsChar-- != 0)
	{
	xPos   = pVgaConDv->col;
	chrPos = (UINT16 *) pVgaConDv->curChrPos;
	erase  = (pVgaConDv->defAttrib << 8) + ' ' ; 

	while (xPos++ < pVgaConDv->ncol)
	    {
	    /* make the current character as next erase */

	    swap    = *chrPos;
	    *chrPos = erase ; 
	    erase   = swap;   
	    chrPos++;
	    }
	}
    }

/*****************************************************************************
*
* vgaDelLeftChar - delete the character to the left side of the cursor
*
* RETURNS: N/A
*/

LOCAL void vgaDelLeftChar 
    (
    FAST VGA_CON_DEV *	pVgaConDv	/* pointer to the vga descriptor */
    )
    {
    FAST UINT16		erase;	/* erase character with attributes */

    erase = (pVgaConDv->defAttrib << 8) + ' ' ;
    if (pVgaConDv->autoWrap || pVgaConDv->ncol > 0)
	{
	pVgaConDv->col--;
	pVgaConDv->curChrPos -= CHR;
	}
    *(UINT16 *)pVgaConDv->curChrPos = erase;

    }

/*****************************************************************************
*
* vgaCarriageReturn - do a carriage return on the monitor
*
* RETURNS: N/A
*/

LOCAL void vgaCarriageReturn 
    (
    FAST VGA_CON_DEV *	pVgaConDv	/* pointer to the vga descriptor */
    )
    {

    pVgaConDv->curChrPos -= pVgaConDv->col * CHR;
    pVgaConDv->col = 0;

    }

/*****************************************************************************
*
* vgaBackSpace - do a back space on the monitor
*
* RETURNS: N/A
*/

LOCAL void vgaBackSpace 
    (
    FAST VGA_CON_DEV *	pVgaConDv	/* pointer to the vga descriptor */
    )
    {

    if (pVgaConDv->autoWrap || pVgaConDv->ncol > 0)
	{
	pVgaConDv->col--;
	pVgaConDv->curChrPos -= CHR;
	}
    if (pVgaConDv->col < 0)
	{
	pVgaConDv->col = pVgaConDv->ncol - 1;
	pVgaConDv->row--;
	pVgaConDv->scrollCheck = TRUE;
	}
    }

/*****************************************************************************
*
* vgaTab - do a tab on the monitor
*
* RETURNS: N/A
*/

LOCAL void vgaTab 
    (
    FAST VGA_CON_DEV *	pVgaConDv	/* pointer to the vga descriptor */
    )
    {
    int			ix;

    for (ix = pVgaConDv->col + 1; ix < 80; ix++)
	{
	if (pVgaConDv->tab_stop [ix])
	    {
	    pVgaConDv->col = ix;
	    break;
	    }
	}
    if (pVgaConDv->autoWrap && ix >= 80)
	{
	pVgaConDv->col = 0;
	pVgaConDv->row++;
	pVgaConDv->scrollCheck = TRUE;
	}
    pVgaConDv->curChrPos = ( pVgaConDv->memBase + 
			    pVgaConDv->row * pVgaConDv->ncol * CHR + 
			    pVgaConDv->col * CHR) ;
    }

/******************************************************************************
*
* vgaLineFeed - do a line feed on the monitor
*
* RETURNS: N/A
*/

LOCAL void vgaLineFeed 
    (
    FAST VGA_CON_DEV *	pVgaConDv	/* pointer to the vga descriptor */
    )
    {
    pVgaConDv->curChrPos += pVgaConDv->ncol * CHR;
    pVgaConDv->row++;
    pVgaConDv->scrollCheck = TRUE;

    }

/******************************************************************************
*
* vgaCursorPos - Put the cursor at a specified location
*
* RETURNS: N/A
*/

LOCAL void vgaCursorPos 
    (
    FAST UINT16 pos			/* position of the cursor */
    )
    {

    sysOutByte ((int) CTRL_SEL_REG, 0x0e);
    sysOutByte ((int) CTRL_VAL_REG, (pos >> 8) & 0xff);
    sysOutByte ((int) CTRL_SEL_REG, 0x0f);
    sysOutByte ((int) CTRL_VAL_REG, pos & 0xff);
    return;
    }

/******************************************************************************
*
* vgaCursorOn - switch the cursor on
*
* RETURNS: N/A
*/

LOCAL void vgaCursorOn (void)
    {
    sysOutByte ((int) CTRL_SEL_REG, 0x0a);
    sysOutByte ((int) CTRL_VAL_REG, curSt & ~0x20);
    sysOutByte ((int) CTRL_SEL_REG, 0x0b);
    sysOutByte ((int) CTRL_VAL_REG, curEd);
    return;
    }

/******************************************************************************
*
* vgacursoroff - swith the cursor off
*
* RETURNS: N/A
*/

LOCAL void vgaCursorOff (void)
    {
    sysOutByte ((int) CTRL_SEL_REG, 0x0a);
    sysOutByte ((int) CTRL_VAL_REG, 0x20);
    sysOutByte ((int) CTRL_SEL_REG, 0x0b);
    sysOutByte ((int) CTRL_VAL_REG, 0x00);
    return;
    }

/*******************************************************************************
*
* vgaWriteString - Write Character string  to VGA Display
* 
* This function does the write to the vga routine. This routine is provided as
* transmitter startup routine when tyDevInit is called.
*
* RETURNS: number of bytes written to the screen
*
* NOMANUAL
*/

int vgaWriteString
    (
    FAST PC_CON_DEV *	pPcCoDv	/* pointer to the console descriptor */
    )
    {
    int			dummy;
    UCHAR		ch;
    FAST int		nBytes;
    FAST UCHAR		atr;
    FAST RING_ID        ringId = pPcCoDv->tyDev.wrtBuf;
    FAST VGA_CON_DEV *	pVgaConDv = pPcCoDv->vs;
    
    pPcCoDv->tyDev.wrtState.busy = TRUE;
    atr = pVgaConDv->curAttrib;
    nBytes = 0;
    
    /* check if we need to output XON/XOFF for the read side */

    if (pPcCoDv->tyDev.wrtState.xoff || pPcCoDv->tyDev.wrtState.flushingWrtBuf)
	{
    	pPcCoDv->tyDev.wrtState.busy = FALSE;
	return nBytes;
	}

    while (RNG_ELEM_GET (ringId,&ch,dummy) != 0)
        {
	nBytes++;		/* increment the number of bytes */

	/* If character is normal and printable */
	
	if ( (pVgaConDv->escFlags == ESC_NORMAL) && (pVgaConDv->charSet [ch]
						     != 0))
	    {
	    *(UINT16 *)pVgaConDv->curChrPos = (atr << 8) + 
		                                  pVgaConDv->charSet [ch];
	    if (pVgaConDv->col == pVgaConDv->ncol - 1)
		{ 
		if (pVgaConDv->autoWrap)
		    { 
		    vgaCarriageReturn (pVgaConDv);	/* time to wrap */
		    vgaLineFeed (pVgaConDv);
		    goto VGA_CHECK;		/* go do the wrap check */
		    }
		}
	    else
		{
		pVgaConDv->col++;
		pVgaConDv->curChrPos += CHR;
		continue;
		}
	    }

	switch (ch)
	    {
	    case 0x07:		/* BEL */
	         vgaConBeep (FALSE);
		 continue;
	    
	    case 0x08:		/* Back Space */
		 vgaBackSpace (pVgaConDv);
		 continue;

            case '\t':		/* TAB code */
		 vgaTab (pVgaConDv);
		 continue;

            case '\n':		/* LF code */
		 if ((pPcCoDv->tyDev.options & OPT_CRMOD) == OPT_CRMOD)
		    vgaCarriageReturn (pVgaConDv);
		 vgaLineFeed (pVgaConDv);
		 goto VGA_CHECK;

            case 0x0b:		/* VT code */
		 vgaLineFeed (pVgaConDv);
		 goto VGA_CHECK;

            case 0x0c:		/* Clear Screen code */
		 vgaClear (pVgaConDv, 2, ' ');
		 continue;

	    case 0x0d:		/* CR code */
		 vgaCarriageReturn (pVgaConDv);
		 continue;

#ifdef INCLUDE_ANSI_ESC_SEQUENCE

	    case 0x1b:		/* escape character */
		 pVgaConDv->escFlags = ESC_ESC;
		 continue; 

            case 0x9b:		/* escape brace */
		 pVgaConDv->escFlags = ESC_BRACE;
		 continue;
#endif /* INCLUDE_ANSI_ESC_SEQUENCE */

	    case 0x0e:		/* set the character set to VT100 graphics */
		 pVgaConDv->charSet = vgaCharTable [GRAPHICS_VT100_SET];
		 continue;

            case 0x0f:		/* set the character set to normal text set */
		 pVgaConDv->charSet = vgaCharTable [TEXT_SET];
		 continue;

	    case 0x7f:		/* special character for del */
		 vgaDelLeftChar (pVgaConDv);
		 continue;
	  }

#ifdef INCLUDE_ANSI_ESC_SEQUENCE	
	switch (pVgaConDv->escFlags)
	    {
	    int 	ix;	/* to hold temp data */

	    case ESC_ESC:
	            pVgaConDv->escFlags = ESC_NORMAL;
		    switch (ch)
			{
			case '[':	/* escape brace */
				pVgaConDv->escFlags = ESC_BRACE;
				continue;

			case 'E':	/* cr lf */
				vgaCarriageReturn (pVgaConDv);
				vgaLineFeed (pVgaConDv);
				goto VGA_CHECK;

			case 'M':	/* cursor up */
				pVgaConDv->row --;
				vgaPutCursor (pVgaConDv);
				continue;
				
			case 'D':	/* generate a linefeed */
				vgaLineFeed (pVgaConDv);
				goto VGA_CHECK;

			case 'H':	/* give tab */
				vgaTab (pVgaConDv);
				continue;
				
			case 'Z':	/* get device attribute */
				vgaEscResponse (pPcCoDv,2);
				continue;
				
			case '7':	/* save current attributes */
				vgaSaveCurAttrib (pVgaConDv);
				continue;
				
			case '8':	/* restore current attributes */
				vgaRestAttrib (pVgaConDv);
				continue;
				
			case '(':	/* set character set to text */
				pVgaConDv->escFlags = ESC_SET_TEXT;
				continue;
				
			case ')':	/* set character set to grapics set */
				pVgaConDv->escFlags = ESC_SET_GPRAHICS;
				continue;
				
			case '#':	/* goto ESC_HASH state */
				pVgaConDv->escFlags = ESC_HASH;
				continue;
				
			case 'c':	/* reset display */
				pPcCoDv->ks->kbdHook (0);
				vgaStatInit ();
				vgaClear (pVgaConDv, 2, ' ');
				vgaCursorOn ();
				continue;

			case '>':	/* set numeric mode */
				pPcCoDv->ks->kbdFlags |= NUM;
				pPcCoDv->ks->kbdHook (1);
				continue;

			case '=':	/* set non numeric mode */
				pPcCoDv->ks->kbdFlags &= ~NUM;
				pPcCoDv->ks->kbdHook (1);
				continue;
           		}
		    continue;

	    case ESC_BRACE:	/* Got ESC [ */
	            for (ix = 0; ix < NPARS; ix++)
		        pVgaConDv->escPara [ix] = 0;
		    pVgaConDv->escParaCount = 0;
	            pVgaConDv->escFlags = ESC_GET_PARAMS;
		    if ( ch == '[')
			{
			pVgaConDv->escFlags = ESC_FUNC_KEY;
			continue;
			}

	            /* if received ? in the escape sequence */

		    if ( (pVgaConDv->escQuestion = (ch == '?')) )
		       continue;

	    case ESC_GET_PARAMS:	/* get parameters */
		    if ( (ch == ';') && (pVgaConDv->escParaCount < NPARS -1))
			{
			pVgaConDv->escParaCount++;
			continue;
			}
		    else if (ch >= '0' && ch <= '9')
			{
			pVgaConDv->escPara[pVgaConDv->escParaCount] *= 10;
			pVgaConDv->escPara[pVgaConDv->escParaCount] += ch -'0';
			continue;
			}
		    else
		        pVgaConDv->escFlags = ESC_GOT_PARAMS;

	    case ESC_GOT_PARAMS:
		    pVgaConDv->escFlags = ESC_NORMAL;
		    switch (ch)
			{
			case 'h':	/* set vga modes  ESC [ n h */
			        vgaSetMode (pPcCoDv, TRUE);
				continue;
				
			case 'l':	/* reset vga mode  ESC [ n l */
				vgaSetMode (pPcCoDv, FALSE);
				continue;

			case 'n':
				if (!pVgaConDv->escQuestion)
				    {
				    if (pVgaConDv->escPara [0] == 5)
					{ /* status report */
					vgaEscResponse (pPcCoDv,1);
					}
				    else if ( pVgaConDv->escPara [0] == 6)
					{ /* cursor position report */
					vgaEscResponse (pPcCoDv,0);
					}
				    continue;
				    }
			}
		    if (pVgaConDv->escQuestion) 
			{
			pVgaConDv->escQuestion = FALSE;
			continue;
			}
		    switch (ch)
			{
			case 'G':	/* ESC [ n G :move cursor by columns */
			        if (pVgaConDv->escPara [0] > 0)
				    pVgaConDv->escPara [0]--;

				pVgaConDv->col = pVgaConDv->escPara [0];
				vgaPutCursor (pVgaConDv);
				continue;

			case 'A':	/* ESC [ n A :cursor move up */
			        if (pVgaConDv->escPara [0] == 0)
				    pVgaConDv->escPara [0]++;

				pVgaConDv->row -= pVgaConDv->escPara [0];
				vgaPutCursor (pVgaConDv);
                                continue;

			case 'B':	/* ESC [ n B :cursor move down */
			        if (pVgaConDv->escPara [0] == 0)
				    pVgaConDv->escPara [0]++;

				pVgaConDv->row += pVgaConDv->escPara [0];
				vgaPutCursor (pVgaConDv);
				continue;
				
			case 'C':	/* ESC [ n C :cursor move right */
			        if (pVgaConDv->escPara [0] == 0)
				    pVgaConDv->escPara [0]++;

				pVgaConDv->col += pVgaConDv->escPara [0];
				vgaPutCursor (pVgaConDv);
				continue;

			case 'D':	/* ESC [ n D :cursor move left */
			        if (pVgaConDv->escPara [0] == 0)
				    pVgaConDv->escPara [0]++;

				pVgaConDv->col -= pVgaConDv->escPara [0];
				vgaPutCursor (pVgaConDv);
				continue;

			case 'E':	/* ESC [ n E :cursor move by n rows */
			        if (pVgaConDv->escPara [0] == 0)
				    pVgaConDv->escPara [0]++;

				pVgaConDv->row += pVgaConDv->escPara [0];
				pVgaConDv->col = 0;
				vgaPutCursor (pVgaConDv);
				continue;

			case 'F':	/* ESC [ n F :move cursor laterally */
			        if (pVgaConDv->escPara [0] == 0)
				    pVgaConDv->escPara [0]++;

				pVgaConDv->row -= pVgaConDv->escPara [0];
				pVgaConDv->col = 0;
				vgaPutCursor (pVgaConDv);
				continue;

			case 'd':	/* ESC [ n d :move cursor vertically */
			        if (pVgaConDv->escPara [0] > 0)
				    pVgaConDv->escPara [0]--;

				pVgaConDv->row = pVgaConDv->escPara [0];
				vgaPutCursor (pVgaConDv);
				continue;
				
			case 'H':	/* ESC [ n;n H :position the cursor */
                        case 'f':	/* ESC [ n;n f :position the cursor */
			        if (pVgaConDv->escPara [0] > 0)
				    pVgaConDv->escPara [0]--;

			        if (pVgaConDv->escPara [1] > 0)
				    pVgaConDv->escPara [1]--;

				pVgaConDv->row = pVgaConDv->escPara [0];
				pVgaConDv->col = pVgaConDv->escPara [1];
				vgaPutCursor (pVgaConDv);
				continue;

			case 'J':	/* ESC [ n J :clear display */
				vgaClear (pVgaConDv, pVgaConDv->escPara [0],
					  ' ');
				continue;
				
			case 'K':	/* ESC [ n K :clear Line */
				vgaClearLine (pVgaConDv);
				continue;
			   
			case 'L':	/* ESC [ n L :insert Lines */
				vgaInsertLine (pVgaConDv);
				continue;
				
			case 'M':	/* ESC [ n M :delete lines */
				vgaDelLines (pVgaConDv);
				continue;

			case 'P':	/* ESC [ n P :delete on right side */
				vgaDelRightChars (pVgaConDv, 
						  pVgaConDv->escPara [0]);
				continue;
				
			case 'c':	/* ESC [ n c :get response from term */
				if ( pVgaConDv->escPara [0] == 0 )
				    vgaEscResponse (pPcCoDv,2);

				continue;

			case 'g':	/* ESC [ n g :give tabs */
				if ( pVgaConDv->escPara [0] == 0 )
				    {
				    vgaTab (pVgaConDv);
				    }
				else if ( pVgaConDv->escPara [0] == 3)
				    {
				    pVgaConDv->tab_stop [0] = 0; 
				    pVgaConDv->tab_stop [1] = 0;
				    pVgaConDv->tab_stop [2] = 0;
				    pVgaConDv->tab_stop [3] = 0;
				    pVgaConDv->tab_stop [4] = 0;
				    }
				continue;
				
			case 'm':	/* ESC [ m :set the attributes */
				vgaSetAttrib (pVgaConDv);
				continue;

			case 'r':	/* ESC [ n;n r : set scroll limits */
				if (pVgaConDv->escPara [0] == 0)
				    pVgaConDv->escPara [0]++;

				if (pVgaConDv->escPara [1] == 0)
				    pVgaConDv->escPara[1] = pVgaConDv->ncol-1;

				if ((pVgaConDv->escPara [0] < 
				    pVgaConDv->escPara [1]) && 
				    (pVgaConDv->escPara [1] < pVgaConDv->ncol))
				    {
				    pVgaConDv->scst = pVgaConDv->escPara [0] -
				    		      1;
				    pVgaConDv->sced = pVgaConDv->escPara [1];
				    pVgaConDv->row = 0;
				    vgaPutCursor (pVgaConDv);
				    }
				continue;
				
			case 's':	/* ESC [ s :save attributes */
				vgaSaveCurAttrib (pVgaConDv);
				continue;

			case 'u':	/* ESC [ u :restore attributes */
				vgaRestAttrib (pVgaConDv);
				continue;

			case '@':	/* ESC [ n @ :insert n characters */
				vgaInsertChar (pVgaConDv, 
					       pVgaConDv->escPara [0]);
				continue;
			}
		    
		    continue;

	    case ESC_FUNC_KEY:
		    pVgaConDv->escFlags = ESC_NORMAL;
		    continue;

	    case ESC_HASH:
		    pVgaConDv->escFlags = ESC_NORMAL;
		    if ( ch == '8' )
		       vgaClear (pVgaConDv, 2, 'E');	/* erase char to 'E' */
		    continue;
		    
	    case ESC_SET_TEXT:
	    case ESC_SET_GPRAHICS:
		    if ( ch == 'O' )
		       pVgaConDv->charSet = vgaCharTable [GRAPHICS_VT100_SET];
		    else if ( ch == 'B')
			{
			pVgaConDv->charSet = vgaCharTable [TEXT_SET];
			pVgaConDv->escFlags = ESC_NORMAL;
			}
		    else if ( ch == 'U')
			{
			pVgaConDv->charSet = vgaCharTable [IBM_GRAPHICS_SET];
			pVgaConDv->escFlags = ESC_NORMAL;
			}
		    continue;

	    default:
		    pVgaConDv->escFlags = ESC_NORMAL;
	    }
#endif /* INCLUDE_ANSI_ESC_SEQUENCE */

	VGA_CHECK:	/* label for checking at the end of the loop */
	    {
	    if (pVgaConDv->scrollCheck && pVgaConDv->curChrPos >= 
		pVgaConDv->memBase + pVgaConDv->ncol * CHR * (pVgaConDv->sced 
							      + 1))
		{ 
		/* forward scroll check */
		
		pVgaConDv->row = pVgaConDv->sced;
		vgaScroll (pVgaConDv, pVgaConDv->row, pVgaConDv->scrollCheck,
			   FORWARD, pVgaConDv->defAttrib);
		while (pVgaConDv->curChrPos >= pVgaConDv->memBase + 
		       pVgaConDv->ncol * CHR * (pVgaConDv->sced + 1))
		pVgaConDv->curChrPos -= pVgaConDv->ncol * CHR;
		}
	    else if (pVgaConDv->scrollCheck && pVgaConDv->curChrPos < 
		     pVgaConDv->memBase + pVgaConDv->col * CHR * 
		     pVgaConDv->scst)
		{
		/* rearword scroll check */

		pVgaConDv->row = pVgaConDv->scst;
		vgaScroll (pVgaConDv, pVgaConDv->row, pVgaConDv->scrollCheck,
			   BACKWARD, pVgaConDv->defAttrib);
		while (pVgaConDv->curChrPos < pVgaConDv->memBase + 
		       pVgaConDv->col * CHR * pVgaConDv->scst)
		pVgaConDv->curChrPos += pVgaConDv->ncol * CHR;
		}
	    else if (pVgaConDv->curChrPos > pVgaConDv->memBase + 
		     pVgaConDv->ncol * CHR * pVgaConDv->nrow)
		{ 
		/* out of range check (over) */

		while (pVgaConDv->curChrPos > pVgaConDv->memBase + 
		       pVgaConDv->ncol * CHR * pVgaConDv->nrow)
		    {
		    pVgaConDv->curChrPos -= pVgaConDv->ncol * CHR;
		    pVgaConDv->row--;
		    }
		}
	    else if (pVgaConDv->curChrPos < pVgaConDv->memBase)
		{ 
		/* out of range check (under) */

		while (pVgaConDv->curChrPos < pVgaConDv->memBase)
		    {
		    pVgaConDv->curChrPos += pVgaConDv->ncol * CHR;
		    pVgaConDv->row++;
		    }
		}
	    }	/* VGA_CHECK: */
	}	/* end of the main for loop */

    vgaCursorPos ((pVgaConDv->curChrPos - pVgaConDv->memBase) / CHR );
    pPcCoDv->tyDev.wrtState.busy = FALSE;

    /* when we pass the write threshhold, give write synchonization
     * and release tasks pended in select */
    
    if (rngFreeBytes (ringId) >= tyWrtThreshold)
            {
            semGive (&pPcCoDv->tyDev.wrtSyncSem);
            if (_func_selWakeupAll != NULL)
                (* _func_selWakeupAll) (&pPcCoDv->tyDev.selWakeupList, 
					SELWRITE);
	    }
    return nBytes;	/* return the number of characters processed */
    }

#ifdef INCLUDE_ANSI_ESC_SEQUENCE
    
/*****************************************************************************
*
* vgaEscResponse - This function gives back a response to an escape sequence.
*                  The valid response Ids are 
*                  0 for cursor position, 1 for terminal status, 
*                  2 for terminal device attributes.
*
* RETURNS: N/A
*/

LOCAL void vgaEscResponse
    (
    FAST PC_CON_DEV *	pPcCoDv,	/* pointer to the console descriptor */
    int			responseId	/* response Id */
    )
    {
    FAST VGA_CON_DEV *	pVgaConDv;	/* pointer to the vga descriptor */
    pVgaConDv = pPcCoDv->vs;
    tyIoctl (&pPcCoDv->tyDev, FIORFLUSH, 0);
    if ( responseId == 0)
	{
	sprintf (pVgaConDv->escResp, "\033[%d;%dR",pVgaConDv->row, 
		 pVgaConDv->col );
	}
    else if ( responseId == 1) 
	{
	sprintf (pVgaConDv->escResp, "\033[0n");
	}
    else if ( responseId == 2)
	{
	sprintf (pVgaConDv->escResp, "\033[?1;0c");
	}
    else
	{
	pVgaConDv->escResp[0] = '\0';
	}
    
    rngBufPut (pPcCoDv->tyDev.rdBuf, pVgaConDv->escResp, 
	       strlen (pVgaConDv->escResp) );
    semGive (&pPcCoDv->tyDev.rdSyncSem);
    selWakeupAll (&pPcCoDv->tyDev.selWakeupList, SELREAD);

    }

/*****************************************************************************
*
* vgaSaveCurAttrib - saves all the current attributes, cursor position
*                    screen reverse mode.
*
* RETURNS: N/A
*/

LOCAL void vgaSaveCurAttrib 
    (
    FAST VGA_CON_DEV *	pVgaConDv	/* pointer to the vga descriptor */
    )
    {
    pVgaConDv->sv_rev       = pVgaConDv->rev;
    pVgaConDv->sv_col       = pVgaConDv->col;
    pVgaConDv->sv_row       = pVgaConDv->row;
    pVgaConDv->sv_curAttrib = pVgaConDv->curAttrib;

    }
    
/*****************************************************************************
*
* vgaRestAttrib - restores all the saved attributes, cursor position
*                    screen reverse mode.
*
* RETURNS: N/A
*/

LOCAL void vgaRestAttrib 
    (
    FAST VGA_CON_DEV *	pVgaConDv	/* pointer to the vga descriptor */
    )
    {
    pVgaConDv->rev       = pVgaConDv->sv_rev;
    pVgaConDv->col       = pVgaConDv->sv_col;
    pVgaConDv->row       = pVgaConDv->sv_row;
    pVgaConDv->curAttrib = pVgaConDv->sv_curAttrib;
    
    }

/*****************************************************************************
*
* vgaSetAttrib - sets various attributes 
*
* RETURNS: N/A
*/

LOCAL void vgaSetAttrib 
    (
    FAST VGA_CON_DEV *	pVgaConDv	/* pointer to the vga descriptor */
    )  
    {
    FAST int		ix;	/* register to hold the index */
    FAST UCHAR		attr;	/* register to hold the current attribute */
    
    attr = pVgaConDv->curAttrib;
    for (ix = 0 ; ix <= pVgaConDv->escParaCount; ix++)
	{
	switch (pVgaConDv->escPara [ix])
	    {
	    case 0:
	          attr =  DEFAULT_ATR;
		  break;
		  
            case 1:
		  attr |= ATRB_BRIGHT;		/* set intensity */
		  break;
	       
	    case 4:
		  if (!pVgaConDv->colorMode)
		      {
		      attr |= UNDERLINE;
		      }
		  break;
		  
	    case 5:
		  attr |= ATRB_BLINK;		/* toggle blinking */
		  break;

	    case 7:
		  pVgaConDv->rev = TRUE; /* set reverse mode */
		  attr = (attr & 0x88) | (((attr >> 4) | ((attr << 4) & 0x7)));
		  break;
		  
	    case 21:
	    case 22:
		  attr &= ~ATRB_BRIGHT;		/* reset brightness */
		  break;
		  
	    case 24:
		  if (!pVgaConDv->colorMode)
		      {
		      attr &= ~UNDERLINE;	/* reset underline mode */
		      }
		  break;
		  
	    case 25:
		  attr &= ~ATRB_BLINK;
		  break;

	    case 27:
		  pVgaConDv->rev = FALSE; 	/* reset reverse mode */
		  break;

	    default:		
		  /* set foreground and background attribute */

		  if ((pVgaConDv->escPara [ix] >= 30) && 
		      (pVgaConDv->escPara [ix] <= 37) )
		      {
		      attr = (attr & 0xf8) | 
		             (vgaColorTable [pVgaConDv->escPara [ix] - 30]) ;
		      }
		  else if ((pVgaConDv->escPara [ix] >= 40) &&
			   (pVgaConDv->escPara [ix] <= 47))
		      {
		      attr = (attr & 0x8f) |
		             vgaColorTable [pVgaConDv->escPara [ix] - 40] << 4;
		      }
		  break;
            }
	}
    pVgaConDv->curAttrib = attr;   /* set the current attribute */
    }

/*****************************************************************************
*
* vgaSetMode - set various special modes according to the input onOff
*
* RETURNS: N/A
*/

LOCAL void vgaSetMode
    (
    FAST  PC_CON_DEV *	pPcCoDv,	/* pointer to the vga descriptor */
    BOOL         	onOff		/* on or off */
    )
    {
    FAST int		ix;
    FAST VGA_CON_DEV *	pVgaConDv;	/* pointer to the vga descriptor */
    
    pVgaConDv = pPcCoDv->vs;
    for ( ix =0; ix <= pVgaConDv->escParaCount; ix++ )
	{
	if ( pVgaConDv->escQuestion )
	    {
	    switch (pVgaConDv->escPara [ix])
		{ /* modes set reset */
		case 1:
		      pPcCoDv->ks->curMode = (onOff) ?  FALSE : TRUE;
		      break;
		      
                case 3: /* 80/132 mode switch unimplemented */
		      vgaClear (pVgaConDv, 2, ' ');
		      break;
		      
                case 5:
		      pVgaConDv->rev = (pVgaConDv->rev) ? FALSE : TRUE;
		      vgaScreenRev (pVgaConDv);
		      break;
		      
                case 7:
		      pVgaConDv->autoWrap = onOff;
		      break;
		      
                case 25:
		      (onOff) ?  vgaCursorOn() : vgaCursorOff() ;
		      break;
		}
	    }
	else if ( pVgaConDv->escPara [ix] == 4)
	    {
	    pVgaConDv->insMode = onOff;
	    }
	}
    }

/*****************************************************************************
*
* vgaPutCursor - place the cursor in the specified location
*
* RETURNS: N/A
*/

LOCAL void vgaPutCursor 
    (
    FAST VGA_CON_DEV *	pVgaConDv	/* pointer to the vga descriptor */
    )
    {
    if ( pVgaConDv->col  < 0)
	pVgaConDv->col = 0;

    else if (pVgaConDv->col >= pVgaConDv->ncol)
	pVgaConDv->col = pVgaConDv->ncol - 1;

    if ( pVgaConDv->row < pVgaConDv->scst)
	pVgaConDv->row = pVgaConDv->scst;

    else if ( pVgaConDv->row >= pVgaConDv->sced )
	pVgaConDv->row = pVgaConDv->sced;
	    
    pVgaConDv->curChrPos = (pVgaConDv->memBase + pVgaConDv->row *
			    pVgaConDv->ncol * CHR + pVgaConDv->col * CHR );
    
    }

/*****************************************************************************
*
* vgaClearLine - clear the line according to the position passed.
*
* RETURNS: N/A
*/

LOCAL void vgaClearLine 
    (
    FAST VGA_CON_DEV *	pVgaConDv	/* pointer to the vga descriptor */
    )
    {
    FAST UINT16 *	cp;	/* hold the beginning value */
    FAST UINT16 *	end;	/* hold the end value */
    FAST UINT16		erase;	/* hold the erase character */

    erase = (pVgaConDv->defAttrib << 8 ) + ' ';
    
    if (pVgaConDv->escPara [0] == 0 )
	{
	cp  = (UINT16 *) (pVgaConDv->curChrPos + CHR );
	end = (UINT16 *) (pVgaConDv->memBase + pVgaConDv->row  * 
			  pVgaConDv->ncol * CHR + (pVgaConDv->ncol - 1) * CHR);
	}
    else if (pVgaConDv->escPara [0] == 1)
	{
	cp  = (UINT16 *) (pVgaConDv->memBase + pVgaConDv->row * 
			 pVgaConDv->ncol * CHR);
	end = (UINT16 *) (pVgaConDv->curChrPos);
	}
    else 
	{
	cp  = (UINT16 *) (pVgaConDv->memBase + pVgaConDv->row * 
			  pVgaConDv->ncol * CHR);
	end = (UINT16 *) (cp + (pVgaConDv->ncol - 1) * CHR);
	pVgaConDv->curChrPos = (UCHAR *) cp ;
	pVgaConDv->col = 0;
	}
    for (; cp <= end; cp ++)
	{
	*cp = erase;
	}
    }

/******************************************************************************
*
* vgaInsertLine - inserts as many number of lines as passed in the escape
*                  parameter
*
* RETURNS: N/A
*/

LOCAL void vgaInsertLine 
    (
    FAST VGA_CON_DEV *	pVgaConDv	/* pointer to the vga descriptor */
    )
    {
    int			ix;
    int			start;
    int			end;

    start = pVgaConDv->scst;
    end   = pVgaConDv->sced;
    pVgaConDv->scst = pVgaConDv->row;
    pVgaConDv->sced = pVgaConDv->nrow - 1 ;
    ix = pVgaConDv->escPara [0];
    if (pVgaConDv->escPara [0] == 0)
	{
	ix = 1;
	}
    if (pVgaConDv->row + ix >= pVgaConDv->nrow)
	{
	ix = pVgaConDv->nrow - pVgaConDv->row - 1;
	}
    vgaScroll (pVgaConDv, pVgaConDv->row, ix, BACKWARD, pVgaConDv->defAttrib);
    pVgaConDv->curChrPos -= pVgaConDv->col * CHR;
    pVgaConDv->col = 0;
    pVgaConDv->scst = start;
    pVgaConDv->sced = end ;
    
    }
     
/******************************************************************************
*
* vgaDelLines - deletes as many as lines as passed in the escape 
*               parameter
*
* RETURNS: N/A
*/

LOCAL void vgaDelLines 
    (
    FAST VGA_CON_DEV *	pVgaConDv	/* pointer to the vga descriptor */
    )
    {
    int			ix;
    int			start;
    int			end;

    start = pVgaConDv->scst;
    end = pVgaConDv->sced;
    pVgaConDv->scst = pVgaConDv->row;
    pVgaConDv->sced = pVgaConDv->nrow - 1;
    ix = pVgaConDv->escPara [0];
    if (ix == 0)
	{
	ix = 1;
	}
    if (pVgaConDv->row + ix > pVgaConDv->nrow) 
	{
	ix = pVgaConDv->row;
	}
    vgaScroll (pVgaConDv, pVgaConDv->sced, ix, FORWARD, pVgaConDv->defAttrib);
    pVgaConDv->curChrPos -= pVgaConDv->col * CHR;
    pVgaConDv->col = 0;
    pVgaConDv->scst = start;
    pVgaConDv->sced = end;

    }

/******************************************************************************
*
* vgaDelRightChars - deletes character right of the cursor
*
* RETURNS: N/A
*/

LOCAL void vgaDelRightChars
    (
    FAST VGA_CON_DEV *	pVgaConDv,	/* pointer to the vga descriptor */
    int			nDelChar	/* number of Deletes to do */
    )
    {
    FAST int		xPos;	/* current position in columns */
    FAST UINT16 *	cp;	/* current pointer */
    FAST UINT16		erase;	/* to hold erase character with Attribute */
    
    erase = (pVgaConDv->defAttrib << 8 ) + ' ';

    if (nDelChar + pVgaConDv->col  >=  pVgaConDv->ncol )
	{
	nDelChar = pVgaConDv->ncol - pVgaConDv->col - 1;
	}
    else if (nDelChar == 0)
	{
	nDelChar = 1;
	}
    while ( nDelChar-- )
	{
	xPos = pVgaConDv->col;
	cp   = (unsigned short *) pVgaConDv->curChrPos;
	while (++xPos < pVgaConDv->ncol)
	    {
	    *cp = *(cp +1);
	    cp++;
	    }
	*cp =  erase;           /* erase the last character */
	}
    }

#endif /* INCLUDE_ANSI_ESC_SEQUENCE */
