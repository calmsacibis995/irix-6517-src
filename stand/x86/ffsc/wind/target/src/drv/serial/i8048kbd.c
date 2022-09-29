/* i8048Kbd.c - intel 8048 key board driver routines */

/* Copyright 1993-1993 Wind River System, Inc. */
#include "copyright_wrs.h"

/*
modification history
--------------------
01b,14jun95,hdn  removed function declarations defined in sysLib.h.
01a,20oct93,vin  created
*/

/*
DESCRIPTION
This is the driver for the Intel 8048 Keyboard Controller Chip used on a 
personal computer 386 / 486. This driver handles the 83 key  keyboard for
PC, PC XT, and PC PORTABLE. This driver does not change the defaults set by 
the BIOS. The BIOS initializes the scan code set to 1.

USER CALLABLE ROUTINES
The routines in this driver are accessed from external modules
through the hook routine, the pointer to which is initialized
in the KBD_CON_DEV structure at the time of initialization.
This makes the access to these routines more generic without declaring
these functions as external. 

The routines in this driver which are accessed from an external module
are kbdIntr() and kbdHrdInit(). kbdIntr() is installed as an interrupt
service routine through the intConnect call in sysHwInit2(). 
kbdHrdInit() is called to initialize the device descriptors and the 
key board. 

The kbdIntr() is the interrupt handler which handles the key board interrupt
and is responsible for the handing the character received to whichever
console the kbdDv.currCon is initialized to. By default this is initialized
to PC_CONSOLE. If the user has to change the current console he will have
to make an ioctl call with the option CONIOCURCONSOLE and the argument 
as the console number which he wants to change to. To return to the console
owned by the shell the user has to do an ioctl call back to the console number
owned the shell from his application. 
 
Before using the driver, it must be initialized by calling kbdHrdInit(). 
This routine should be called exactly once. Normally, it is called from 
a generic driver for eg. from tyCoDrv() which is called before tyCoDevCreate.

SEE ALSO
conLib

NOTES
The following macros should be defined in <target>.h file
COMMAND_8048, DATA_8048,STATUS_8048. These refer to the io base addresses
of the various key board controller registers. The macros N_VIRTUAL_CONSOLES
and PC_CONSOLE should be defined in config.h file.
*/

/* includes */

#include "vxworks.h"
#include "iv.h"
#include "iolib.h"
#include "ioslib.h"
#include "memlib.h"
#include "tylib.h"
#include "errnolib.h"
#include "config.h"
#include "drv/serial/pcconsole.h"


/* externals */
IMPORT PC_CON_DEV	pcConDv [N_VIRTUAL_CONSOLES] ;


/* locals */

LOCAL KBD_CON_DEV	kbdConDv;	/* key board device descriptor */
LOCAL unsigned	char enhancedKeys[] =  /* 16 extended keys */
    {
    0x1c,   /* keypad enter */
    0x1d,   /* right control */
    0x35,   /* keypad slash */
    0x37,   /* print screen */
    0x38,   /* right alt */
    0x46,   /* break (control-pause) */
    0x47,   /* editpad home */
    0x48,   /* editpad up */
    0x49,   /* editpad pgup */
    0x4b,   /* editpad left */
    0x4d,   /* editpad right */
    0x4f,   /* editpad end */
    0x50,   /* editpad dn */
    0x51,   /* editpad pgdn */
    0x52,   /* editpad ins */
    0x53    /* editpad del */
    };


/* action table*/

LOCAL UCHAR action [144] =
    { /* Action table for scanCode */
    0,   AS,   AS,   AS,   AS,   AS,   AS,   AS, /* scan  0- 7 */
   AS,   AS,   AS,   AS,   AS,   AS,   AS,   AS, /* scan  8- F */
   AS,   AS,   AS,   AS,   AS,   AS,   AS,   AS, /* scan 10-17 */
   AS,   AS,   AS,   AS,   AS,   CN,   AS,   AS, /* scan 18-1F */
   AS,   AS,   AS,   AS,   AS,   AS,   AS,   AS, /* scan 20-27 */
   AS,   AS,   SH,   AS,   AS,   AS,   AS,   AS, /* scan 28-2F */
   AS,   AS,   AS,   AS,   AS,   AS,   SH,   AS, /* scan 30-37 */
   AS,   AS,   CP,   0,    0,    0,    0,     0, /* scan 38-3F */
    0,   0,    0,    0,    0,    NM,   ST,   ES, /* scan 40-47 */
   ES,   ES,   ES,   ES,   ES,   ES,   ES,   ES, /* scan 48-4F */
   ES,   ES,   ES,   ES,   0,    0,    0,     0, /* scan 50-57 */
    0,   0,    0,    0,    0,    0,    0,     0, /* scan 58-5F */
    0,   0,    0,    0,    0,    0,    0,     0, /* scan 60-67 */
    0,   0,    0,    0,    0,    0,    0,     0, /* scan 68-6F */
   AS,   0,    0,    AS,   0,    0,    AS,    0, /* scan 70-77 */
    0,   AS,   0,    0,    0,    AS,   0,     0, /* scan 78-7F */
   AS,   CN,   AS,   AS,   AS,   ST,   EX,   EX, /* enhanced   */
   AS,   EX,   EX,   AS,   EX,   AS,   EX,   EX  /* enhanced   */
    };

/*  key board Maps Japanese and english */
LOCAL UCHAR keyMap [2][4][144] =
    { 
    { /* Japanese Enhanced keyboard */
    { /* unshift code */
    0,  0x1b,   '1',   '2',   '3',   '4',   '5',   '6',	/* scan  0- 7 */
  '7',   '8',   '9',   '0',   '-',   '^',  0x08,  '\t',	/* scan  8- F */
  'q',   'w',   'e',   'r',   't',   'y',   'u',   'i',	/* scan 10-17 */
  'o',   'p',   '@',   '[',  '\r',   CN,    'a',   's',	/* scan 18-1F */
  'd',   'f',   'g',   'h',   'j',   'k',   'l',   ';',	/* scan 20-27 */
  ':',     0,   SH,    ']',   'z',   'x',   'c',   'v',	/* scan 28-2F */
  'b',   'n',   'm',   ',',   '.',   '/',   SH,    '*',	/* scan 30-37 */
  ' ',   ' ',   CP,     0,     0,     0,     0,     0,	/* scan 38-3F */
    0,     0,     0,     0,     0,   NM,    ST,    '7',	/* scan 40-47 */
  '8',   '9',   '-',   '4',   '5',   '6',   '+',   '1',	/* scan 48-4F */
  '2',   '3',   '0',   '.',     0,     0,     0,     0,	/* scan 50-57 */
    0,     0,     0,     0,     0,     0,     0,     0,	/* scan 58-5F */
    0,     0,     0,     0,     0,     0,     0,     0,	/* scan 60-67 */
    0,     0,     0,     0,     0,     0,     0,     0,	/* scan 68-6F */
  ' ',     0,     0,  '\\',     0,     0,   ' ',     0,	/* scan 70-77 */
    0,   ' ',     0,     0,     0,  '\\',     0,     0,	/* scan 78-7F */
  '\r',   CN,   '/',   '*',   ' ',    ST,    'F',   'A', /* extended */
    0,   'D',   'C',     0,   'B',     0,    '@',   'P'  /* extended */
    },
    { /* shift code */
    0,  0x1b,   '!',   '"',   '#',   '$',   '%',   '&',	/* scan  0- 7 */
 '\'',   '(',   ')',     0,   '=',   '~',  0x08,  '\t',	/* scan  8- F */
  'Q',   'W',   'E',   'R',   'T',   'Y',   'U',   'I',	/* scan 10-17 */
  'O',   'P',   '`',   '{',  '\r',   CN,    'A',   'S',	/* scan 18-1F */
  'D',   'F',   'G',   'H',   'J',   'K',   'L',   '+',	/* scan 20-27 */
  '*',     0,   SH,    '}',   'Z',   'X',   'C',   'V',	/* scan 28-2F */
  'B',   'N',   'M',   '<',   '>',   '?',   SH,    '*',	/* scan 30-37 */
  ' ',   ' ',   CP,      0,     0,     0,     0,     0,	/* scan 38-3F */
    0,     0,     0,     0,     0,   NM,    ST,    '7',	/* scan 40-47 */
  '8',   '9',   '-',   '4',   '5',   '6',   '+',   '1',	/* scan 48-4F */
  '2',   '3',   '0',   '.',     0,     0,     0,     0,	/* scan 50-57 */
    0,     0,     0,     0,     0,     0,     0,     0,	/* scan 58-5F */
    0,     0,     0,     0,     0,     0,     0,     0,	/* scan 60-67 */
    0,     0,     0,     0,     0,     0,     0,     0,	/* scan 68-6F */
  ' ',     0,     0,   '_',     0,     0,   ' ',     0,	/* scan 70-77 */
    0,   ' ',     0,     0,     0,   '|',     0,     0,	/* scan 78-7F */
  '\r',   CN,   '/',   '*',   ' ',    ST,    'F',   'A', /* extended */
    0,   'D',   'C',     0,   'B',     0,    '@',   'P'  /* extended */
    },
    { /* Control code */
 0xff,  0x1b,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,	/* scan  0- 7 */
 0xff,  0xff,  0xff,  0xff,  0xff,  0x1e,  0xff,  '\t',	/* scan  8- F */
 0x11,  0x17,  0x05,  0x12,  0x14,  0x19,  0x15,  0x09,	/* scan 10-17 */
 0x0f,  0x10,  0x00,  0x1b,  '\r',   CN,  0x01,   0x13,	/* scan 18-1F */
 0x04,  0x06,  0x07,  0x08,  0x0a,  0x0b,  0x0c,  0xff,	/* scan 20-27 */
 0xff,  0xff,   SH,   0x1d,  0x1a,  0x18,  0x03,  0x16,	/* scan 28-2F */
 0x02,  0x0e,  0x0d,  0xff,  0xff,  0xff,  SH,    0xff,	/* scan 30-37 */
 0xff,  0xff,   CP,   0xff,  0xff,  0xff,  0xff,  0xff,	/* scan 38-3F */
 0xff,  0xff,  0xff,  0xff,  0xff,   NM,   ST,    0xff,	/* scan 40-47 */
 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,	/* scan 48-4F */
 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,	/* scan 50-57 */
 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,	/* scan 58-5F */
 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,	/* scan 60-67 */
 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,	/* scan 68-6F */
 0xff,  0xff,  0xff,  0x1f,  0xff,  0xff,  0xff,  0xff,	/* scan 70-77 */
 0xff,  0xff,  0xff,  0xff,  0xff,  0x1c,  0xff,  0xff,	/* scan 78-7F */
  '\r',   CN,   '/',   '*',   ' ',    ST,  0x0c,  0xff, /* extended */
 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff  /* extended */
    },
    { /* non numeric code */
    0,  0x1b,   '1',   '2',   '3',   '4',   '5',   '6',	/* scan  0- 7 */
  '7',   '8',   '9',   '0',   '-',   '^',  0x08,  '\t',	/* scan  8- F */
  'q',   'w',   'e',   'r',   't',   'y',   'u',   'i',	/* scan 10-17 */
  'o',   'p',   '@',   '[',  '\r',   CN,    'a',   's',	/* scan 18-1F */
  'd',   'f',   'g',   'h',   'j',   'k',   'l',   ';',	/* scan 20-27 */
  ':',     0,   SH,    ']',   'z',   'x',   'c',   'v',	/* scan 28-2F */
  'b',   'n',   'm',   ',',   '.',   '/',   SH,    '*',	/* scan 30-37 */
  ' ',   ' ',   CP,     0,     0,     0,     0,     0,	/* scan 38-3F */
    0,     0,     0,     0,     0,   NM,    ST,    'w',	/* scan 40-47 */
  'x',   'y',   'l',   't',   'u',   'v',   'm',   'q',	/* scan 48-4F */
  'r',   's',   'p',   'n',     0,     0,     0,     0,	/* scan 50-57 */
    0,     0,     0,     0,     0,     0,     0,     0,	/* scan 58-5F */
    0,     0,     0,     0,     0,     0,     0,     0,	/* scan 60-67 */
    0,     0,     0,     0,     0,     0,     0,     0,	/* scan 68-6F */
  ' ',     0,     0,  '\\',     0,     0,   ' ',     0,	/* scan 70-77 */
    0,   ' ',     0,     0,     0,  '\\',     0,     0,	/* scan 78-7F */
  '\r',   CN,   '/',   '*',   ' ',    ST,    'F',   'A', /* extended */
    0,   'D',   'C',     0,   'B',     0,    '@',   'P'  /* extended */
    }
    },
    { /* English Enhanced keyboard */
    { /* unshift code */
    0,  0x1b,   '1',   '2',   '3',   '4',   '5',   '6',	/* scan  0- 7 */
  '7',   '8',   '9',   '0',   '-',   '=',  0x08,  '\t',	/* scan  8- F */
  'q',   'w',   'e',   'r',   't',   'y',   'u',   'i',	/* scan 10-17 */
  'o',   'p',   '[',   ']',  '\r',   CN,    'a',   's',	/* scan 18-1F */
  'd',   'f',   'g',   'h',   'j',   'k',   'l',   ';',	/* scan 20-27 */
 '\'',   '`',   SH,   '\\',   'z',   'x',   'c',   'v',	/* scan 28-2F */
  'b',   'n',   'm',   ',',   '.',   '/',   SH,    '*',	/* scan 30-37 */
  ' ',   ' ',   CP,      0,     0,     0,     0,     0,	/* scan 38-3F */
    0,     0,     0,     0,     0,   NM,    ST,    '7',	/* scan 40-47 */
  '8',   '9',   '-',   '4',   '5',   '6',   '+',   '1',	/* scan 48-4F */
  '2',   '3',   '0',   '.',     0,     0,     0,     0,	/* scan 50-57 */
    0,     0,     0,     0,     0,     0,     0,     0,	/* scan 58-5F */
    0,     0,     0,     0,     0,     0,     0,     0,	/* scan 60-67 */
    0,     0,     0,     0,     0,     0,     0,     0,	/* scan 68-6F */
    0,     0,     0,     0,     0,     0,     0,     0,	/* scan 70-77 */
    0,     0,     0,     0,     0,     0,     0,     0,	/* scan 78-7F */
  '\r',   CN,   '/',   '*',   ' ',    ST,   'F',   'A', /* extended */
    0,   'D',   'C',     0,   'B',     0,    '@',  'P'  /* extended */
    },
    { /* shift code */
    0,  0x1b,   '!',   '@',   '#',   '$',   '%',   '^',	/* scan  0- 7 */
  '&',   '*',   '(',   ')',   '_',   '+',  0x08,  '\t',	/* scan  8- F */
  'Q',   'W',   'E',   'R',   'T',   'Y',   'U',   'I',	/* scan 10-17 */
  'O',   'P',   '{',   '}',  '\r',   CN,    'A',   'S',	/* scan 18-1F */
  'D',   'F',   'G',   'H',   'J',   'K',   'L',   ':',	/* scan 20-27 */
  '"',   '~',   SH,    '|',   'Z',   'X',   'C',   'V',	/* scan 28-2F */
  'B',   'N',   'M',   '<',   '>',   '?',   SH,    '*',	/* scan 30-37 */
  ' ',   ' ',   CP,      0,     0,     0,     0,     0,	/* scan 38-3F */
    0,     0,     0,     0,     0,   NM,    ST,    '7',	/* scan 40-47 */
  '8',   '9',   '-',   '4',   '5',   '6',   '+',   '1',	/* scan 48-4F */
  '2',   '3',   '0',   '.',     0,     0,     0,     0,	/* scan 50-57 */
    0,     0,     0,     0,     0,     0,     0,     0,	/* scan 58-5F */
    0,     0,     0,     0,     0,     0,     0,     0,	/* scan 60-67 */
    0,     0,     0,     0,     0,     0,     0,     0,	/* scan 68-6F */
    0,     0,     0,     0,     0,     0,     0,     0,	/* scan 70-77 */
    0,     0,     0,     0,     0,     0,     0,     0,	/* scan 78-7F */
  '\r',   CN,   '/',   '*',   ' ',    ST,   'F',   'A', /* extended */
    0,   'D',   'C',     0,   'B',     0,   '@',   'P'  /* extended */
    },
    { /* Control code */
 0xff,  0x1b,  0xff,  0x00,  0xff,  0xff,  0xff,  0xff,	/* scan  0- 7 */
 0x1e,  0xff,  0xff,  0xff,  0x1f,  0xff,  0xff,  '\t',	/* scan  8- F */
 0x11,  0x17,  0x05,  0x12,  0x14,  0x19,  0x15,  0x09,	/* scan 10-17 */
 0x0f,  0x10,  0x1b,  0x1d,  '\r',   CN,   0x01,  0x13,	/* scan 18-1F */
 0x04,  0x06,  0x07,  0x08,  0x0a,  0x0b,  0x0c,  0xff,	/* scan 20-27 */
 0xff,  0x1c,   SH,   0xff,  0x1a,  0x18,  0x03,  0x16,	/* scan 28-2F */
 0x02,  0x0e,  0x0d,  0xff,  0xff,  0xff,   SH,   0xff,	/* scan 30-37 */
 0xff,  0xff,   CP,   0xff,  0xff,  0xff,  0xff,  0xff,	/* scan 38-3F */
 0xff,  0xff,  0xff,  0xff,  0xff,   NM,    ST,   0xff,	/* scan 40-47 */
 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,	/* scan 48-4F */
 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,	/* scan 50-57 */
 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,	/* scan 58-5F */
 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,	/* scan 60-67 */
 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,	/* scan 68-6F */
 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,	/* scan 70-77 */
 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,	/* scan 78-7F */
  '\r',   CN,   '/',   '*',   ' ',    ST,  0xff,  0xff, /* extended */
 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff  /* extended */
    },
    { /* non numeric code */
    0,  0x1b,   '1',   '2',   '3',   '4',   '5',   '6',	/* scan  0- 7 */
  '7',   '8',   '9',   '0',   '-',   '=',  0x08,  '\t',	/* scan  8- F */
  'q',   'w',   'e',   'r',   't',   'y',   'u',   'i',	/* scan 10-17 */
  'o',   'p',   '[',   ']',  '\r',   CN,    'a',   's',	/* scan 18-1F */
  'd',   'f',   'g',   'h',   'j',   'k',   'l',   ';',	/* scan 20-27 */
 '\'',   '`',   SH,   '\\',   'z',   'x',   'c',   'v',	/* scan 28-2F */
  'b',   'n',   'm',   ',',   '.',   '/',   SH,    '*',	/* scan 30-37 */
  ' ',   ' ',   CP,      0,     0,     0,     0,     0,	/* scan 38-3F */
    0,     0,     0,     0,     0,   NM,    ST,    'w',	/* scan 40-47 */
  'x',   'y',   'l',   't',   'u',   'v',   'm',   'q',	/* scan 48-4F */
  'r',   's',   'p',   'n',     0,     0,     0,     0,	/* scan 50-57 */
    0,     0,     0,     0,     0,     0,     0,     0,	/* scan 58-5F */
    0,     0,     0,     0,     0,     0,     0,     0,	/* scan 60-67 */
    0,     0,     0,     0,     0,     0,     0,     0,	/* scan 68-6F */
    0,     0,     0,     0,     0,     0,     0,     0,	/* scan 70-77 */
    0,     0,     0,     0,     0,     0,     0,     0,	/* scan 78-7F */
  '\r',   CN,   '/',   '*',   ' ',    ST,   'F',   'A', /* extended */
    0,   'D',   'C',     0,   'B',     0,    '@',  'P'  /* extended */
    }
    }
    };

/* forward declarations */

LOCAL void	kbdStatInit (void);
LOCAL void	kbdConvChar(unsigned char scanCode);
LOCAL void	kbdNormal (unsigned char scanCode);
LOCAL void	kbdShift (unsigned char scanCode);
LOCAL void	kbdCtrl (unsigned char scanCode);
LOCAL void	kbdNum (unsigned char scanCode);
LOCAL void	kbdCaps (unsigned char scanCode);
LOCAL void	kbdStp (unsigned char scanCode);
LOCAL void	kbdExt (unsigned char scanCode);
LOCAL void	kbdEs (unsigned char scanCode);
LOCAL void 	(*keyHandler[]) (unsigned char scanCode)  =
    {
    kbdNormal,   kbdShift,   kbdCtrl,   kbdNum,   kbdCaps,
    kbdStp,      kbdExt,     kbdEs
    };
LOCAL void	kbdHook (int opCode);

/*******************************************************************************
*
* kbdHrdInit - initialize the Keyboard
* 
* This routine is called to do the key board initialization from an external
* routine
*
* RETURNS: N/A
*
* NOMANUAL
*/

void kbdHrdInit (void)
    {
    UCHAR 	cond;

    cond = sysInByte (STATUS_8048);		/* get the status */
    
    sysOutByte ( COMMAND_8048, (cond | 0x40));	/* enable the keyboard */

    kbdStatInit ();

    } 

/*******************************************************************************
*
* kbdStatInit - initialize the Keyboard state
*
* This routine initializes the keyboard descriptor in the virtual consoles.
* The same keybaord descriptor is used for all the virtual consoles.
*
*/

LOCAL void kbdStatInit (void)
    {
    int		ix;		/* to hold temp variable */

    /* initialize all the key board descriptors in the virtual consoles */

    for ( ix = 0; ix < N_VIRTUAL_CONSOLES; ix++)
	{
	pcConDv [ix].ks = &kbdConDv;		/* kbd device descriptor */
	}

    kbdConDv.curMode = TRUE;	/* default mode is normal */
    
    /* default keyboard is English Enhanced key */
    
    kbdConDv.kbdMode  = ENGLISH_KBD; 
    kbdConDv.kbdFlags = NORMAL|NUM;		/* Numeric mode on */
    kbdConDv.kbdState = 0; 			/* unshift state */
    kbdConDv.kbdHook  = (FUNCPTR) kbdHook;	/* hook routine */
    kbdConDv.currCon  = PC_CONSOLE;     	/* console tty */
    
    }

/*******************************************************************************
*
* kbdIntr - interrupt level processing
*
* This routine handles the keyboard interrupts
*
* RETURNS: N/A
*
* NOMANUAL
*/

void kbdIntr (void)
    {
    FAST UCHAR	scanCode;	/* to hold the scanCode */
    FAST UCHAR	status;		/* to hold the status */

    scanCode = sysInByte (DATA_8048);		/* get the scan code */
    status = sysInByte (STATUS_8048);		/* get the status    */
    sysOutByte (COMMAND_8048, (status | 0x80));	/* send acknowledgment    */
    sysOutByte (STATUS_8048, status);		/* restore previous status */

    if (scanCode == 0xfa)
	{
	return;
	}
    kbdConvChar (scanCode);	/* convert scancode to ASCII character and */
				/* announce it to tyIRd    		   */
    }

/*******************************************************************************
*
* kbdConvChar - Convert scan code to character
*
* This routine convert scanCode to ASCII character
*/

LOCAL void kbdConvChar 
    (
    UCHAR	scanCode	/* scan Code from the keyboard */
    )
    {
    if (scanCode == 0xe0)
	{
	kbdConDv.kbdFlags |= EXT;
	return;
	}

    /* if high bit of scanCode,set break flag */

    if (((scanCode & 0x80) << 2) == BRK)  
       kbdConDv.kbdFlags |=  BRK;
    else
       kbdConDv.kbdFlags &= ~BRK;

    if ((scanCode == 0xe1) || (kbdConDv.kbdFlags & E1))
	{
	if (scanCode == 0xe1)
	    {
	    kbdConDv.kbdFlags ^= BRK; 	/* reset the break flag */
	    kbdConDv.kbdFlags ^= E1;  	/* bitwise EXOR with E1 flag */
	    }
	return;
	}
    scanCode &= 0x7f;
    if ((kbdConDv.kbdFlags & EXT) == EXT)
	{
	int 	ix;		

	for (ix = 0; ix < EXTND_SIZE; ix++)
	    {
	    if (scanCode == enhancedKeys [ix])
		{
		scanCode = E0_BASE + ix;
		ix = -1;
		break;
		}
	    }
	kbdConDv.kbdFlags ^= EXT; 	/* reset the extended flag */
	if (ix != -1)
	    {
	    return ; 			/* unknown scancode */
	    }
	}

    /* invoke the respective handler */

    (*keyHandler [action [scanCode]]) (scanCode);
    }

/******************************************************************************
*
*
* kbdNormal - Normal key
*
* This routine does the normal key processing
*/

LOCAL void kbdNormal
    (
    UCHAR	scanCode	/* scan code recieved */
    )
    {
    FAST UCHAR	chr;
    FAST int	ix = kbdConDv.currCon;	/* to hold the console no. */

    if ((kbdConDv.kbdFlags & BRK) == NORMAL) 
	{
	chr = keyMap [kbdConDv.kbdMode][kbdConDv.kbdState][scanCode];
	if ((chr == 0xff) || (chr == 0x00))
	    {
	    return;
	    }

	/* if caps lock convert upper to lower */

	if (((kbdConDv.kbdFlags & CAPS) == CAPS) && 
	    (chr >= 'a' && chr <= 'z'))
	    { 
	    chr -= 'a' - 'A';
	    } 
	tyIRd (&(pcConDv [ix].tyDev), chr);	/* give input to buffer */
	}
    }

/******************************************************************************
*
*
* kbdShift - Shift key operation
*
* This routine sets the shift state of the key board
*/

LOCAL void kbdShift
    (
    UCHAR	scanCode	/* scan code recieved */
    )
    {
    if ((kbdConDv.kbdFlags & BRK) == BRK) 
	{
	kbdConDv.kbdState = AS;
	kbdConDv.kbdFlags &= (~SHIFT);
	}
    else
	{
	kbdConDv.kbdState = SH;
	kbdConDv.kbdFlags |= SHIFT;
	}
    }

/******************************************************************************
*
*
* kbdCtrl - Control key operation
*
* This routine sets the shift state of key board to control state
*/

LOCAL void kbdCtrl
    (
    UCHAR	scanCode	/* scan code recieved */
    )
    {
    if ((kbdConDv.kbdFlags & BRK) == BRK) 
	{
	kbdConDv.kbdState = AS;
	kbdConDv.kbdFlags &= (~CTRL);
	}
    else
	{
	kbdConDv.kbdState = CN;
	kbdConDv.kbdFlags |= CTRL;
	}
    }

/******************************************************************************
*
*
* kbdCaps - Capslock key operation
*
* This routine sets the capslock state and the key board flags. 
*/

LOCAL void kbdCaps
    (
    UCHAR	scanCode	/* scan code recieved */
    )
    {
    if ((kbdConDv.kbdFlags & BRK) == NORMAL) 
	{
	kbdConDv.kbdFlags ^= CAPS;
	}
    }

/******************************************************************************
*
*
* kbdNum - Numerlock set key operation
*
* This routine sets the numeric lock state of the key board.
* The keyboard state is numeric by default. If the numeric lock is off
* then this routine initializes the keyboard state to non numeric state.
*/

LOCAL void kbdNum
    (
    UCHAR	scanCode	/* scan code recieved */
    )
    {
    if ((kbdConDv.kbdFlags & BRK) == NORMAL) 
	{
	kbdConDv.kbdFlags ^= NUM;
	kbdConDv.kbdState = (kbdConDv.kbdFlags & NUM) ? AS : NM;
	}
    }

/******************************************************************************
*
*
* kbdStp - Scroll lock set key operation
* 
* This routine sets the scroll lock state of the key board. 
* This routine outputs 0x13 if ^S is pressed or 0x11 if ^Q is pressed.
*/

LOCAL void kbdStp
    (
    UCHAR	scanCode	/* scan code recieved */
    )
    {
    FAST int	ix = kbdConDv.currCon;	/* to hold the console no. */

    if ((kbdConDv.kbdFlags & BRK) == NORMAL) 
	{
	kbdConDv.kbdFlags ^= STP;
	(kbdConDv.kbdFlags & STP) ? tyIRd (&(pcConDv [ix].tyDev),0x13) : 
	                              tyIRd (&(pcConDv [ix].tyDev),0x11);
	}
    }

/******************************************************************************
*
*
* kbdExt - Extended key board operation
*
* This routine processes the extended scan code operations
* and ouputs an escape sequence. ESC [ <chr> . The character is for example
* one of the following. (A, B, C, D, F, L, P)
*/

LOCAL void kbdExt
    (
    UCHAR 	scanCode	/* scan code recieved */
    )
    {
    FAST UCHAR	chr;
    FAST int	ix = kbdConDv.currCon;	/* to hold the console no. */

    if ((kbdConDv.kbdFlags & BRK) == NORMAL) 
	{
	chr = keyMap [kbdConDv.kbdMode][kbdConDv.kbdState][scanCode];
	tyIRd (&pcConDv [ix].tyDev, 0x1b); 	/* escape character */
	kbdConDv.curMode ? tyIRd (&pcConDv [ix].tyDev, '[') : 
	                     tyIRd (&pcConDv [ix].tyDev, 'O');
	tyIRd (&pcConDv [ix].tyDev, chr);	
	}
    }

/******************************************************************************
*
*
* kbdEs - Non Numeric key board operation
*
* This routine processes the non numeric scan code operations
*/

LOCAL void kbdEs
    (
    UCHAR	scanCode	/* scan code recieved */
    )
    {
    FAST UCHAR	chr;
    FAST int	ix = kbdConDv.currCon;	/* to hold the console no. */

    if ((kbdConDv.kbdFlags & BRK) == NORMAL) 
	{
	chr = keyMap [kbdConDv.kbdMode][kbdConDv.kbdState][scanCode];
	if ((kbdConDv.kbdFlags & NUM) == NORMAL)
	    {
	    tyIRd (&pcConDv [ix].tyDev, 0x1b);  /* escape character */
	    tyIRd (&pcConDv [ix].tyDev, 'O');
	    }
	tyIRd (&pcConDv [ix].tyDev, chr);
	}
    }

/******************************************************************************
*
* kbdHook - key board function called from an external routine
*
* This function does the respective key board operation according to the
* opCode. The user can extend this function to perform any key board 
* operation called as a hook from any external function for eg
* from the vga driver. 
*
*/

LOCAL void kbdHook
    (
    int opCode		/* operation to perform */
    )
    {
    switch (opCode)
	{
	case 0:		/* reinitialize the keyboard state */
	      kbdStatInit ();
	      break;
	case 1:
	      break;
	default:
	      break;
	}
    }
