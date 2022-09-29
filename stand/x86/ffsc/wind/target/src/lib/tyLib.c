
/* tyLib.c - tty driver support library */

/* Copyright 1984-1994 Wind River Systems, Inc. */
#include "copyright_wrs.h"

/*
modification history
--------------------
03g,20jun96,jmb  harmless errno from tyFlushRd getting back to users (SPR #6698)
03f,09dec94,rhp  fixed doc for FIOCANCEL (SPR #3828)
                 fixed library descriptions of tyIRd() and tyITx() (SPR #2484)
03e,19jul94,dvs  fixed doc for FIORBUFSET and FIOWBUFSET (SPR #2444).
03d,21jan93,jdi  documentation cleanup for 5.1.
03c,24oct92,jcf  added peculiar 3xx hack to make serial driver work.
03b,23aug92,jcf  removed select stuff so tyLib will standalone.
		 made excJobAdd call indirect.
03a,18jul92,smb  Changed errno.h to errnoLib.h.
03z,04jul92,jcf  scalable/ANSI/cleanup effort.
03y,26may92,rrr  the tree shuffle
03x,04oct91,rrr  passed through the ansification filter
                  -changed functions to ansi style
		  -changed VOID to void
		  -changed copyright notice
03w,01aug91,yao  fixed to pass 6 args to excJobAdd() call.
03v,03apr91,jaa  documentation cleanup; doc review by dnw.
03u,10aug90,kdl  added forward declarations for functions returning void.
03t,15jul90,dnw  lint: removed unused variable in tyIoctl
03s,05jul90,dnw  fixed potential races in read and write.
		 isolated all interrupt lockouts in 3 new routines:
			tyRdXoff/tyWrtXOff/tyTxStartup
		 optimized code.
		 rewrote library documentation.
03r,26jun90,dab  removed tyDevDelete().
03q,26jun90,jcf  changed binary semaphore interface.
03p,02apr90,rdc  changed to use binary semaphores.
	    hjb  beefed up ioctl documentation.
03o,29mar90,rdc  reworked to lower interrupt latency.
		 took out "semNeeded" mechanism, created seperate mutual
		 exclusion and synchronization semaphores.  Interrupt level
		 now checks to make sure task level isn't flushing or
		 replacing ring buffers.
03n,19mar90,dab  added tyDevDelete().
03m,16mar90,rdc  added select support.
03l,27jul89,hjb  added FIOPROTOHOOK, FIOPROTOARG, FIOWFLUSH, FIORFLUSH,
		 FIORBUFSET, FIOWBUFSET support for SLIP.
03k,14oct88,gae  fixed bug in FIOCANCEL; couldn't cancel twice in a row.
03j,07sep88,gae  documentation.
03i,01sep88,gae  documentation; changed RNG_MACRO parameters to int's.
03h,30may88,dnw  changed to v4 names.
03g,28may88,dnw  removed NOT_GENERIC stuff.
03f,04may88,jcf  changed semInits to semClear.
03e,29mar88,gae  added FIOISATTY to tyIoctl repertoire.
03d,26feb88,jcf  changed reboot to excToDoAdd (reboot ..) so reboot routines
		  can print stuff out.
03c,22feb88,dnw  fix coding conventions to keep f2cgen happy.
03b,23nov87,ecs  added VARARGS2 to tyIoctl to keep lint happy.
03a,23oct87,ecs  fixed mangen problems in tlRdRaw/tyRead & tyIRdRaw/tyIRd.
		 documentation.
	    jcf  changed call sysToMonitor into reboot
02z,03oct87,gae  added FIO[GS}ETOPTIONS.
02y,24jul87,gae  made ty{Backspace,DeleteLine,Eof}Char's global for shellLib.c.
02x,25jun87,ecs  changed tyIRdRaw to return ERROR on full ring buffer.
02w,05jun87,ecs  added FIOCANCEL to tyIoctl.
02v,13may87,dnw  fixed checksum bug caused by new "correct" compiler.
	   +gae
02u,23mar87,jlf  documentation.
02t,23feb87,jlf  improved documentation of tyAbortFunc.
...deleted pre 87 history - see RCS
*/

/*
This library provides routines used to implement drivers for serial
devices.  It provides all the necessary device-independent functions of a
normal serial channel, including:

.PD .1
.iP "" 4
ring buffering of input and output
.iP
raw mode
.iP
optional line mode with backspace and line-delete functions
.iP
optional processing of X-on/X-off
.iP
optional RETURN/LINEFEED conversion
.iP
optional echoing of input characters
.iP
optional stripping of the parity bit from 8-bit input
.iP
optional special characters for shell abort and system restart
.PD
.LP

Most of the routines in this library are called only by device drivers.
Functions that normally might be called by an application or
interactive user are the routines to set special characters, ty...Set().

USE IN SERIAL DEVICE DRIVERS
Each device that uses tyLib is described by a data structure of type TY_DEV.
This structure begins with an I/O system device header so that it can be added
directly to the I/O system's device list.  A driver calls tyDevInit() to
initialize a TY_DEV structure for a specific device and then calls iosDevAdd()
to add the device to the I/O system.

The call to tyDevInit() takes three parameters: the pointer to the TY_DEV
structure to initialize, the desired size of the read and write ring buffers,
and the address of a transmitter start-up routine.  This routine will be
called when characters are added for output and the transmitter is idle.  
Thereafter, the driver can call the following routines to perform the
usual device functions:
.iP "tyRead()" 12
user read request to get characters that have been input
.iP "tyWrite()"
user write request to put characters to be output
.iP "tyIoctl()"
user I/O control request
.iP "tyIRd()"
interrupt-level routine to get an input character
.iP "tyITx()"
interrupt-level routine to deliver the next output character
.LP

Thus, tyRead(), tyWrite(), and tyIoctl() are called from the driver's read,
write, and I/O control functions.  The routines tyIRd() and tyITx() are called
from the driver's interrupt handler in response to receive and transmit
interrupts, respectively.

Examples of using tyLib in a driver can be found in the source file(s) 
included by tyCoDrv.  Source files are located in src/drv/serial.

TTY OPTIONS
A full range of options affects the behavior of tty devices.  These
options are selected by setting bits in the device option word using the
FIOSETOPTIONS function in the ioctl() routine (see "I/O Control Functions"
below for more information).  The following is a list of available
options.  The options are defined in the header file ioLib.h.

.iP "OPT_LINE" 20 3
Selects line mode.  A tty device operates in one of two modes:  raw mode
(unbuffered) or line mode.  Raw mode is the default.  In raw mode, each byte
of input from the device is immediately available to readers, and the input is
not modified except as directed by other options below.  In line mode, input
from the device is not available to readers until a NEWLINE character is
received, and the input may be modified by backspace, line-delete, and
end-of-file special characters.
.iP "OPT_ECHO"
Causes all input characters to be echoed to the output of the same channel.
This is done simply by putting incoming characters in the output ring as well
as the input ring.  If the output ring is full, the echoing is lost without
affecting the input.
.iP "OPT_CRMOD"
C language conventions use the NEWLINE character as the line terminator
on both input and output.  Most terminals, however, supply a RETURN character
when the return key is hit, and require both a RETURN and a LINEFEED character
to advance the output line.  This option enables the appropriate translation:
NEWLINEs are substituted for input RETURN characters, and NEWLINEs in the
output file are automatically turned into a RETURN-LINEFEED sequence.
.iP "OPT_TANDEM"
Causes the driver to generate and respond to the special flow control
characters ^Q and ^S in what is commonly known as X-on/X-off protocol.  Receipt
of a ^S input character will suspend output to that channel.  Subsequent receipt
of a ^Q will resume the output.  Also, when the VxWorks input buffer is almost
full, a ^S will be output to signal the other side to suspend transmission.
When the input buffer is almost empty, a ^Q will be output to signal the other
side to resume transmission.
.iP "OPT_7_BIT"
Strips the most significant bit from all bytes input from the device.
.iP "OPT_MON_TRAP"
Enables the special monitor trap character, by default ^X.  When this character
is received and this option is enabled, VxWorks will trap to the ROM resident
monitor program.  Note that this is quite drastic.  All normal VxWorks
functioning is suspended, and the computer system is entirely controlled by the
monitor.  Depending on the particular monitor, it may or may not be possible to
restart VxWorks from the point of interruption.  The default monitor trap
character can be changed by calling tyMonitorTrapSet().
.iP "OPT_ABORT"
Enables the special shell abort character, by default ^C.  When this character
is received and this option is enabled, the VxWorks shell is restarted.  This is
useful for freeing a shell stuck in an unfriendly routine, such as one caught in
an infinite loop or one that has taken an unavailable semaphore.  For more
information, see the
.I "VxWorks Programmer's Guide: Shell."
.iP "OPT_TERMINAL"
This is not a separate option bit.  It is the value of the option word
with all the above bits set.
.iP "OPT_RAW"
This is not a separate option bit.  It is the value of the option word with
none of the above bits set.

I/O CONTROL FUNCTIONS
The tty devices respond to the following ioctl() functions.  The functions
are defined in the header ioLib.h.

.iP "FIOGETNAME" 20 3
Gets the file name of the file descriptor and copies it to the buffer 
referenced to by <nameBuf>:
.CS
    status = ioctl (fd, FIOGETNAME, &nameBuf);
.CE
This function is common to all file descriptors for all devices.
.LP

.iP "FIOSETOPTIONS, FIOOPTIONS" 20
Sets the device option word to the specified argument.  For example, the call:
.CS
    status = ioctl (fd, FIOOPTIONS, OPT_TERMINAL);
    status = ioctl (fd, FIOSETOPTIONS, OPT_TERMINAL);
.CE
enables all the tty options described above, putting the device in a "normal"
terminal mode.  If the line protocol  (OPT_LINE) is changed, the input
buffer is flushed.  The various options are described in ioLib.h.
.iP "FIOGETOPTIONS"
Returns the current device option word:
.CS
    options = ioctl (fd, FIOGETOPTIONS, 0);
.CE
.iP "FIONREAD"
Copies to <nBytesUnread> the number of bytes available to be read in the
device's input buffer:
.CS
    status = ioctl (fd, FIONREAD, &nBytesUnread);
.CE
In line mode (OPT_LINE set), the FIONREAD function actually returns the number
of characters available plus the number of lines in the buffer.  Thus, if five
lines of just NEWLINEs were in the input buffer, it would return the value 10
(5 characters + 5 lines).
.iP "FIONWRITE"
Copies to <nBytes> the number of bytes queued to be output in the device's
output buffer:
.CS
    status = ioctl (fd, FIONWRITE, &nBytes);
.CE
.iP "FIOFLUSH"
Discards all the bytes currently in both the input and the output buffers:
.CS
    status = ioctl (fd, FIOFLUSH, 0);
.CE
.iP "FIOWFLUSH"
Discards all the bytes currently in the output buffer:
.CS
    status = ioctl (fd, FIOWFLUSH, 0);
.CE
.iP "FIORFLUSH"
Discards all the bytes currently in the input buffers:
.CS
    status = ioctl (fd, FIORFLUSH, 0);
.CE
.iP "FIOCANCEL"
Cancels a read or write.  A task blocked on a read or write may be
released by a second task using this ioctl() call.  For example, a
task doing a read can set a watchdog timer before attempting the read;
the auxiliary task would wait on a semaphore.  The watchdog routine
can give the semaphore to the auxiliary task, which would then use the
following call on the appropriate file descriptor:
.CS
    status = ioctl (fd, FIOCANCEL, 0);
.CE
.iP "FIOBAUDRATE"
Sets the baud rate of the device to the specified argument.  For example, the
call:
.CS
    status = ioctl (fd, FIOBAUDRATE, 9600);
.CE
Sets the device to operate at 9600 baud.  This request has no meaning on a
pseudo terminal.
.iP "FIOISATTY"
Returns TRUE for a tty device:
.CS
    status = ioctl (fd, FIOISATTY, 0);
.CE
.iP "FIOPROTOHOOK"
Adds a protocol hook function to be called for each input character.
<pfunction> is a pointer to the protocol hook routine which takes two
arguments of type <int> and returns values of type STATUS (TRUE or
FALSE).  The first argument passed is set by the user via the FIOPROTOARG
function.  The second argument is the input character.  If no further
processing of the character is required by the calling routine (the input
routine of the driver), the protocol hook routine <pFunction> should
return TRUE.  Otherwise, it should return FALSE:
.CS
    status = ioctl (fd, FIOPROTOHOOK, pFunction);
.CE
.iP "FIOPROTOARG"
Sets the first argument to be passed to the protocol hook routine set by
FIOPROTOHOOK function:
.CS
    status = ioctl (fd, FIOPROTOARG, arg);
.CE
.iP "FIORBUFSET"
Changes the size of the receive-side buffer to <size>:
.CS
    status = ioctl (fd, FIORBUFSET, size);
.CE
.iP "FIOWBUFSET"
Changes the size of the send-side buffer to <size>:
.CS
    status = ioctl (fd, FIOWBUFSET, size);
.CE
.LP
Any other ioctl() request will return an error and set the status to
S_ioLib_UNKNOWN REQUEST.

INCLUDE FILES: tyLib.h, ioLib.h

SEE ALSO: ioLib, iosLib, tyCoDrv,
.pG "I/O System"
*/

/* LINTLIBRARY */

#include "vxWorks.h"
#include "ioLib.h"
#include "memLib.h"
#include "rngLib.h"
#include "wdLib.h"
#include "sysLib.h"
#include "excLib.h"
#include "intLib.h"
#include "string.h"
#include "errnoLib.h"
#include "rebootLib.h"
#include "selectLib.h"
#include "tyLib.h"
#include "private/funcBindP.h"

char	rts_state[6];
int	min_rng_chars[6] = {9999, 9999, 9999, 9999, 9999, 9999};


/* special characters */

#define XON		0x11	/* ctrl-Q XON handshake */
#define XOFF		0x13	/* ctrl-S XOFF handshake */

/* global variables */

char tyBackspaceChar	  = 0x08;	/* default is control-H */
char tyDeleteLineChar	  = 0x15;	/* default is control-U */
char tyEofChar		  = 0x04;	/* default is control-D */

int mutexOptionsTyLib = SEM_Q_FIFO | SEM_DELETE_SAFE;

/* local variables */

LOCAL char tyAbortChar	  = 0x03;	/* default is control-C */
LOCAL char tyMonTrapChar  = 0x18;	/* default is control-X */

LOCAL int tyXoffThreshold = 80;		/* max bytes free in input buffer
					 * before XOFF will be sent in
					 * OPT_TANDEM mode */
LOCAL int tyXonThreshold  = 800;	
#if 0
LOCAL int tyXonThreshold  = 100;	/* min bytes free in input buffer
					 * before XON will be sent in
					 * OPT_TANDEM mode */
#endif
LOCAL int tyWrtThreshold  = 20;		/* min bytes free in output buffer
					 * before the next writer will
					 * be enabled */
LOCAL FUNCPTR tyAbortFunc = NULL;	/* function to call when abort char
					 * received */

/* we keep track of the maximum number of chars received after we sent
 * out xoff.  This was used to debug a lazy unix system that
 * sent as many as 60 characters after we had sent xoff.
 * we'll leave the code in just for grins.
 */
LOCAL int tyXoffChars = 0;	/** TEMP **/
LOCAL int tyXoffMax   = 0;	/** TEMP **/

/* forward static functions */

static void tyFlush (TY_DEV_ID pTyDev);
static void tyFlushRd (TY_DEV_ID pTyDev);
static void tyFlushWrt (TY_DEV_ID pTyDev);
static void tyRdRTS (TY_DEV_ID pTyDev, BOOL state);
static void tyRdXoff (TY_DEV_ID pTyDev, BOOL xoff);
static void tyWrtXoff (TY_DEV_ID pTyDev, BOOL xoff);
static void tyTxStartup (TY_DEV_ID pTyDev);
static STATUS rtsChkState(FAST TY_DEV_ID pTyDev);
static void rtsSetState (FAST TY_DEV_ID pTyDev, BOOL state);						  


/*******************************************************************************
*
* tyDevInit - initialize the tty device descriptor
*
* This routine initializes a tty device descriptor according to the
* specified parameters.  The initialization includes allocating read and
* write buffers of the specified sizes from the memory pool,
* and initializing their respective buffer descriptors.
* The semaphores are initialized and the write semaphore is given
* to enable writers.  Also, the transmitter start-up routine pointer is set
* to the specified routine.  All other fields in the descriptor are zeroed.
*
* This routine should be called only by serial drivers.
*
* RETURNS
* OK, or
* ERROR if there is not enough memory to allocate data structures.
*/

STATUS tyDevInit
    (
    FAST TY_DEV_ID pTyDev, /* ptr to tty dev descriptor to init */
    int rdBufSize,         /* size of read buffer in bytes     */
    int wrtBufSize,        /* size of write buffer in bytes    */
    FUNCPTR txStartup      /* device transmit start-up routine */
    )
    {
    /* clear out device */

    bzero ((char *) pTyDev, sizeof (TY_DEV));

	 rtsSetState(pTyDev, FALSE);

    /* allocate read and write ring buffers */

    if ((pTyDev->wrtBuf = rngCreate (wrtBufSize)) == NULL)
	return (ERROR);

    if ((pTyDev->rdBuf = rngCreate (rdBufSize)) == NULL)
	return (ERROR);


    /* initialize rest of device descriptor */

    pTyDev->txStartup = txStartup;

    /* initialize semaphores */

    semBInit (&pTyDev->rdSyncSem, SEM_Q_PRIORITY, SEM_EMPTY);
    semBInit (&pTyDev->wrtSyncSem, SEM_Q_PRIORITY, SEM_EMPTY);
    semMInit (&pTyDev->mutexSem, mutexOptionsTyLib);

    /* initialize the select stuff */

    if (_func_selWakeupListInit != NULL)
	(* _func_selWakeupListInit) (&pTyDev->selWakeupList);

    tyFlush (pTyDev);

    return (OK);
    }
/*******************************************************************************
*
* tyFlush - clear out a tty device descriptor's buffers
*
* This routine resets a tty device's buffers to be empty and in the
* "initial" state.  Thus any characters input but not read by a task,
* and those written by a task but not yet output, are lost.
*
* RETURNS: N/A
*/

LOCAL void tyFlush
    (
    FAST TY_DEV_ID pTyDev       /* ptr to tty dev descriptor to clear */
    )
    {
    tyFlushRd (pTyDev);
    tyFlushWrt (pTyDev);
    }
/*******************************************************************************
*
* tyFlushRd - clear out a tty device descriptor's read buffer
*/

LOCAL void tyFlushRd
    (
    FAST TY_DEV_ID pTyDev       /* ptr to tty dev descriptor to clear */
    )
    {

    int prevErrno;

    /* get exclusive access to the device */

    semTake (&pTyDev->mutexSem, WAIT_FOREVER);

    /* mark as flushing so read interrupts won't attempt to manipulate ring */

    pTyDev->rdState.flushingRdBuf = TRUE;

    rngFlush (pTyDev->rdBuf);

    prevErrno = errno;
    semTake (&pTyDev->rdSyncSem, NO_WAIT);
    if (errno == S_objLib_OBJ_UNAVAILABLE)  /* OK if semaphore not avail. */
	errno = prevErrno;

    pTyDev->lnNBytes	= 0;
    pTyDev->lnBytesLeft	= 0;

    tyRdXoff (pTyDev, FALSE);	/* output an XON if necessary */

	 /* Set RTS */
	 if (pTyDev->options & OPT_RTS ) 
	{
		tyRdRTS(pTyDev, TRUE);
#if 0
	logMsg("Resume: (tyFlushRd) pTyDev = %x\n", pTyDev);
#endif
	}

    pTyDev->rdState.flushingRdBuf = FALSE;
    semGive (&pTyDev->mutexSem);
    }
/*******************************************************************************
*
* tyFlushWrt - clear out a tty device descriptor's write buffer
*/

LOCAL void tyFlushWrt
    (
    FAST TY_DEV_ID pTyDev       /* ptr to tty dev descriptor to clear */
    )
    {
    /* get exclusive access to the device */

    semTake (&pTyDev->mutexSem, WAIT_FOREVER);

    /* mark as flushing so tx interrupts won't attempt to manipulate ring */

    pTyDev->wrtState.flushingWrtBuf = TRUE;

    rngFlush (pTyDev->wrtBuf);
    semGive (&pTyDev->wrtSyncSem);

    pTyDev->wrtState.flushingWrtBuf = FALSE;
    semGive (&pTyDev->mutexSem);

    if (_func_selWakeupAll != NULL)	/* wake up any writers in select */
	(* _func_selWakeupAll) (&pTyDev->selWakeupList, SELWRITE);
    }
/*******************************************************************************
*
* tyAbortFuncSet - set the abort function
*
* This routine sets the function that will be called when the abort
* character is received on a tty.  There is only one global abort function,
* used for any tty on which OPT_ABORT is enabled.  When the abort character is
* received from a tty with OPT_ABORT set, the function specified in <func> will
* be called, with no parameters, from interrupt level.
*
* Setting an abort function of NULL will disable the abort function.
*
* RETURNS: N/A
*
* SEE ALSO: tyAbortSet()
*/

void tyAbortFuncSet
    (
    FUNCPTR func        /* routine to call when abort char received */
    )
    {
    tyAbortFunc = func;
    }
/*******************************************************************************
*
* tyAbortSet - change the abort character
*
* This routine sets the abort character to <ch>.
* The default abort character is ^C.
*
* Typing the abort character to any device whose OPT_ABORT option is set
* will cause the shell task to be killed and restarted.
* Note that the character set by this routine applies to all devices
* whose handlers use the standard tty package tyLib.
*
* RETURNS: N/A
*
* SEE ALSO: tyAbortFuncSet()
*/

void tyAbortSet
    (
    char ch             /* char to be abort */
    )
    {
    tyAbortChar = ch;
    }
/*******************************************************************************
*
* tyBackspaceSet - change the backspace character
*
* This routine sets the backspace character to <ch>.
* The default backspace character is ^H.
*
* Typing the backspace character to any device operating in line protocol
* mode (OPT_LINE set) will cause the previous character typed to be
* deleted, up to the beginning of the current line.
* Note that the character set by this routine applies to all devices
* whose handlers use the standard tty package tyLib.
*
*
* RETURNS: N/A
*/

void tyBackspaceSet
    (
    char ch             /* char to be backspace */
    )
    {
    tyBackspaceChar = ch;
    }
/*******************************************************************************
*
* tyDeleteLineSet - change the line-delete character
*
* This routine sets the line-delete character to <ch>.
* The default line-delete character is ^U.
*
* Typing the delete character to any device operating in line protocol
* mode (OPT_LINE set) will cause all characters in the current
* line to be deleted.
* Note that the character set by this routine applies to all devices
* whose handlers use the standard tty package tyLib.
*
*
* RETURNS: N/A
*/

void tyDeleteLineSet
    (
    char ch             /* char to be line-delete */
    )
    {
    tyDeleteLineChar = ch;
    }
/*******************************************************************************
*
* tyEOFSet - change the end-of-file character
*
* This routine sets the EOF character to <ch>.
* The default EOF character is ^D.
*
* Typing the EOF character to any device operating in line protocol mode
* (OPT_LINE set) will cause no character to be entered in the current
* line, but will cause the current line to be terminated (thus without a newline
* character).  The line is made available to reading tasks.  Thus, if the EOF
* character is the first character input on a line, a line length of zero
* characters is returned to the reader.   This is the standard end-of-file
* indication on a read call.  Note that the EOF character set by this routine
* will apply to all devices whose handlers use the standard tty package tyLib.
*
*
* RETURNS: N/A
*/

void tyEOFSet
    (
    char ch             /* char to be EOF */
    )
    {
    tyEofChar = ch;
    }
/*******************************************************************************
*
* tyMonitorTrapSet - change the trap-to-monitor character
*
* This routine sets the trap-to-monitor character to <ch>.
* The default trap-to-monitor character is ^X.
*
* Typing the trap-to-monitor character to any device whose OPT_MON_TRAP option
* is set will cause the resident ROM monitor to be entered, if one is present.
* Once the ROM monitor is entered, the normal multitasking system is halted.
*
* Note that the trap-to-monitor character set by this routine will apply to all
* devices whose handlers use the standard tty package tyLib.  Also note that
* not all systems have a monitor trap available.
*
* RETURNS: N/A
*/

void tyMonitorTrapSet
    (
    char ch             /* char to be monitor trap */
    )
    {
    tyMonTrapChar = ch;
    }
/*******************************************************************************
*
* tyIoctl - handle device control requests
*
* This routine handles ioctl() requests for tty devices.  The I/O control
* functions for tty devices are described in the manual entry for tyLib.
*
* BUGS:
* In line protocol mode (OPT_LINE option set), the FIONREAD function
* actually returns the number of characters available plus the number of
* lines in the buffer.  Thus, if five lines consisting of just NEWLINEs were
* in the input buffer, the FIONREAD function would return the value ten
* (five characters + five lines).
*
* RETURNS: OK or ERROR.
*
* VARARGS2 - not all requests include an arg.
*/

STATUS tyIoctl
    (
    FAST TY_DEV_ID pTyDev,      /* ptr to device to control */
    int request,                /* request code             */
    int arg                     /* some argument            */
    )
    {
    FAST int status = OK;
    int oldOptions;

    switch (request)
	{
	case FIONREAD:
	    *((int *) arg) = rngNBytes (pTyDev->rdBuf);
	    break;

	case FIONWRITE:
	    *((int *) arg) = rngNBytes (pTyDev->wrtBuf);
	    break;

	case FIOFLUSH:
	    tyFlush (pTyDev);
	    break;

	case FIOWFLUSH:
	    tyFlushWrt (pTyDev);
	    break;

	case FIORFLUSH:
	    tyFlushRd (pTyDev);
	    break;

	case FIOGETOPTIONS:
	    return (pTyDev->options);

	case FIOSETOPTIONS:		 	/* now same as FIOOPTIONS */
	    oldOptions = pTyDev->options;
	    pTyDev->options = arg;

	    if ((oldOptions & OPT_LINE) != (pTyDev->options & OPT_LINE))
		tyFlushRd (pTyDev);

	    if ((oldOptions & OPT_TANDEM) && !(pTyDev->options & OPT_TANDEM))
		{
		/* TANDEM option turned off: XON receiver and transmitter */

		tyRdXoff (pTyDev, FALSE);	/* output XON if necessary */
		tyWrtXoff (pTyDev, FALSE);	/* restart xmitter if nec. */
		}

      if (!(oldOptions & OPT_RTS) && (pTyDev->options & OPT_RTS)) {
			rtsSetState(pTyDev, TRUE);
			tyRdRTS(pTyDev, TRUE);
#if 0
			logMsg("Resume (FIOSETOPTIONS 1): pTyDev = %x\n", pTyDev);
#endif
		}

		/* even if the hardware flow control is reset, enable RTS */
      if ((oldOptions & OPT_RTS) && !(pTyDev->options & OPT_RTS)) {
			rtsSetState(pTyDev, TRUE);
			tyRdRTS(pTyDev, TRUE);
#if 0
			logMsg("Resume (FIOSETOPTIONS 2): pTyDev = %x\n", pTyDev);
#endif
		}

	    break;

	case FIOCANCEL:
	    semTake (&pTyDev->mutexSem, WAIT_FOREVER);

	    pTyDev->rdState.canceled = TRUE;
	    semGive (&pTyDev->rdSyncSem);

	    pTyDev->wrtState.canceled = TRUE;
	    semGive (&pTyDev->wrtSyncSem);

	    semGive (&pTyDev->mutexSem);
	    break;

	case FIOISATTY:
	    status = TRUE;
	    break;

	case FIOPROTOHOOK:
	    pTyDev->protoHook = (FUNCPTR) arg;
	    break;

	case FIOPROTOARG:
	    pTyDev->protoArg = arg;
	    break;

	case FIORBUFSET:
	    semTake (&pTyDev->mutexSem, WAIT_FOREVER);
	    pTyDev->rdState.flushingRdBuf = TRUE;

	    if (pTyDev->rdBuf)
		rngDelete (pTyDev->rdBuf);

	    if ((pTyDev->rdBuf = rngCreate (arg)) == NULL)
		status = ERROR;

	    pTyDev->rdState.flushingRdBuf = FALSE;
	    semGive (&pTyDev->mutexSem);

	    break;

	case FIOWBUFSET:
	    semTake (&pTyDev->mutexSem, WAIT_FOREVER);
	    pTyDev->wrtState.flushingWrtBuf = TRUE;

	    if (pTyDev->wrtBuf)
		rngDelete (pTyDev->wrtBuf);

	    if ((pTyDev->wrtBuf = rngCreate (arg)) == NULL)
		status = ERROR;

	    pTyDev->wrtState.flushingWrtBuf = FALSE;
	    semGive (&pTyDev->mutexSem);

	    break;

	case FIOSELECT:
	    if (_func_selTyAdd != NULL)
	        (* _func_selTyAdd) (pTyDev, arg);
	    break;

	case FIOUNSELECT:
	    if (_func_selTyDelete != NULL)
	        (* _func_selTyDelete) (pTyDev, arg);
	    break;

	default:
	    errnoSet (S_ioLib_UNKNOWN_REQUEST);
	    status = ERROR;
	}

    return (status);
    }

/* raw mode I/O routines */

/*******************************************************************************
*
* tyWrite - do a task-level write for a tty device
*
* This routine handles the task-level portion of the tty handler's
* write function.
*
* INTERNAL
* This routine is not the only place characters are put into
* the buffer: the read echo in tyIRd may put chars in output
* ring as well.  tyIRd is of course not blocked by the mutexSem,
* and we don't want to lock out interrupts for the entire time
* we are copying data to ring.  So we we set wrtBufBusy while we
* are putting into the ring buffer, and tyIRd drops echo chars
* while this flag is set.  The problem with this is that some
* input chars may not get echoed.  This shouldn't be a problem
* for interactive stuff, since it only occurs when there is
* output going on.  But for a serial protocol that relied on
* echo, like echoplex, it could be a problem.
*
* RETURNS: The number of bytes actually written to the device.
*/

int tyWrite
    (
    FAST TY_DEV_ID pTyDev,      /* ptr to device structure */
    char *buffer,               /* buffer of data to write  */
    FAST int nbytes             /* number of bytes in buffer */
    )
    {
    FAST int	bytesput;
    int		nbStart = nbytes;

    pTyDev->wrtState.canceled = FALSE;

    while (nbytes > 0)
        {
	/* XXX
	 * mutexSem is "safe" from task deletion, but sync semaphore can't be;
	 * if task is deleted after getting syncSem but before getting mutexSem,
	 * tty channel will be hung.
	 */

        semTake (&pTyDev->wrtSyncSem, WAIT_FOREVER);

	semTake (&pTyDev->mutexSem, WAIT_FOREVER);

        if (pTyDev->wrtState.canceled)
            {
	    semGive (&pTyDev->mutexSem);
            errnoSet (S_ioLib_CANCELLED);
            return (nbStart - nbytes);
            }

	/* set wrtBufBusy while are putting data into the ring buffer to
	 * inhibit tyIRd echoing chars - see description of problem above
	 */

	pTyDev->wrtState.wrtBufBusy = TRUE;
        bytesput = rngBufPut (pTyDev->wrtBuf, buffer, nbytes);
	pTyDev->wrtState.wrtBufBusy = FALSE;

	tyTxStartup (pTyDev);		/* if xmitter not busy, start it */

        nbytes -= bytesput;
        buffer += bytesput;

        /* If more room in ringId, enable next writer. */

	if (rngFreeBytes (pTyDev->wrtBuf) > 0)
	    semGive (&pTyDev->wrtSyncSem);

	semGive (&pTyDev->mutexSem);
	}

    return (nbStart);
    }
/*******************************************************************************
*
* tyRead - do a task-level read for a tty device
*
* This routine handles the task-level portion of the tty handler's read
* function.  It reads into the buffer up to <maxbytes> available bytes.
*
* This routine should only be called from serial device drivers.
*
* RETURNS: The number of bytes actually read into the buffer.
*/

int tyRead
    (
    FAST TY_DEV_ID pTyDev,      /* device to read         */
    char *buffer,               /* buffer to read into    */
    int maxbytes                /* maximum length of read */
    )
    {
    FAST int	nbytes;
    FAST RING_ID ringId;
    FAST int	nn;
    int		freeBytes = 0;

char			* p_str;
int			ttyNum;

    pTyDev->rdState.canceled = FALSE;

    FOREVER
	{
	/* XXX
	 * mutexSem is "safe" from task deletion, but sync semaphore can't be;
	 * if task is deleted after getting syncSem but before getting mutexSem,
	 * tty channel will be hung.
	 */

	semTake (&pTyDev->rdSyncSem, WAIT_FOREVER);

	semTake (&pTyDev->mutexSem, WAIT_FOREVER);

	if (pTyDev->rdState.canceled)
	    {
	    semGive (&pTyDev->mutexSem);
	    errnoSet (S_ioLib_CANCELLED);
	    return (0);
	    }

	ringId = pTyDev->rdBuf;

	if (!rngIsEmpty (ringId))
	    break;

	semGive (&pTyDev->mutexSem);
	}

    /* we exit the above loop with the mutex semaphore taken and certain
     * that the ring buffer is not empty */

    /* get characters from ring buffer */

    if (pTyDev->options & OPT_LINE)
	{
	if (pTyDev->lnBytesLeft == 0)
	    RNG_ELEM_GET (ringId, &pTyDev->lnBytesLeft, nn);

	nbytes = min ((int)pTyDev->lnBytesLeft, maxbytes);
	rngBufGet (ringId, buffer, nbytes);
	pTyDev->lnBytesLeft -= nbytes;
	}
    else
	nbytes = rngBufGet (ringId, buffer, maxbytes);


    /* check if XON needs to be output */

    if ((pTyDev->options & OPT_TANDEM) && pTyDev->rdState.xoff)
	{
	freeBytes = rngFreeBytes (ringId);
	if (pTyDev->options & OPT_LINE)
	    freeBytes -= pTyDev->lnNBytes + 1;

	if (freeBytes > tyXonThreshold)
	    tyRdXoff (pTyDev, FALSE);
	}

	if (pTyDev->options & OPT_RTS && (rtsChkState(pTyDev) == FALSE)) {

      freeBytes = rngFreeBytes (ringId);
      if (pTyDev->options & OPT_LINE)
         freeBytes -= pTyDev->lnNBytes + 1;

		if (freeBytes > tyXonThreshold)
		{
#if 0
			logMsg("Resume (tyRead): pTyDev = %x, freeBytes = %d, tyXonThreshold = %d\n", 
				pTyDev, freeBytes, tyXonThreshold);
#endif
         tyRdRTS(pTyDev, TRUE);
		}

	}


    /* If more characters in ring, enable next reader.
     * This is necessary in case several tasks blocked when ring was empty -
     * each must be awoken now
     */

    if (!rngIsEmpty (ringId))
	semGive (&pTyDev->rdSyncSem);

    semGive (&pTyDev->mutexSem);

p_str = strrchr(pTyDev->devHdr.name, '/');
p_str++;

ttyNum = *p_str - 0x30;

if (freeBytes < min_rng_chars[ttyNum])
min_rng_chars[ttyNum] = freeBytes;
    return (nbytes);
    }
/*******************************************************************************
*
* tyITx - interrupt-level output
*
* This routine gets a single character to be output to a device.  It looks at
* the ring buffer for <pTyDev> and gives the caller the next available
* character, if there is one.  The character to be output is copied to <pChar>.
*
* RETURNS:
* OK if there are more characters to send, or
* ERROR if there are no more characters.
*/


STATUS tyITx
    (
    FAST TY_DEV_ID pTyDev,   /* pointer to tty device descriptor */
    char *pChar              /* where to put character to be output */
    )
    {
    FAST RING_ID	ringId = pTyDev->wrtBuf;
    FAST int		nn;

    /* check if we need to output XON/XOFF for the read side */

    if (pTyDev->rdState.pending)
	{
	pTyDev->rdState.pending = FALSE;
	*pChar = pTyDev->rdState.xoff ? XOFF : XON;

	/* keep track of the max chars received after we sent xoff - see note
	 * at declaration of tyXoffChars
	 */
	if (pTyDev->rdState.xoff)		/** TEMP **/
	    {					/** TEMP **/
	    if (tyXoffChars > tyXoffMax)	/** TEMP **/
		tyXoffMax = tyXoffChars;	/** TEMP **/
	    tyXoffChars = 0;			/** TEMP **/
	    }					/** TEMP **/
	}

    /* if output is in XOFF, or task level is flushing output buffers,
     * then just turn off xmitter and return */

    else if (pTyDev->wrtState.xoff || pTyDev->wrtState.flushingWrtBuf)
	pTyDev->wrtState.busy = FALSE;

    /* check for carriage return needed after linefeed */

    else if (pTyDev->wrtState.cr)
	{
	*pChar = '\r';
	pTyDev->wrtState.cr   = FALSE;
	}

    /* check for more characters to output */

    else if (RNG_ELEM_GET (ringId, pChar, nn) == 0)	/* no more chars */
	pTyDev->wrtState.busy = FALSE;

    else
	{
	/* got a character to be output */

	pTyDev->wrtState.busy = TRUE;

	/* check for carriage return needs to be added after linefeed */

	if ((pTyDev->options & OPT_CRMOD) && (*pChar == '\n'))
	    pTyDev->wrtState.cr = TRUE;

	/* when we pass the write threshhold, give write synchonization
	 * and release tasks pended in select */

	if (rngFreeBytes (ringId) == tyWrtThreshold)
	    {
	    semGive (&pTyDev->wrtSyncSem);
	    if (_func_selWakeupAll != NULL)
		(* _func_selWakeupAll) (&pTyDev->selWakeupList, SELWRITE);
	    }
	}

    return (pTyDev->wrtState.busy ? OK : ERROR);
    }
/*******************************************************************************
*
* tyIRd - interrupt-level input
*
* This routine handles interrupt-level character input for tty devices.  A
* device driver calls this routine when it has received a character.  This
* routine adds the character to the ring buffer for the specified device, and
* gives a semaphore if a task is waiting for it.
*
* This routine also handles all the special characters, as specified in
* the option word for the device, such as X-on, X-off, NEWLINE, or backspace.
*
* RETURNS: OK, or ERROR if the ring buffer is full.
*/

STATUS tyIRd
    (
    FAST TY_DEV_ID pTyDev,      /* ptr to tty device descriptor */
    FAST char inchar            /* character read */
    )
    {
    FAST RING_ID	ringId;
    FAST int		nn;
    int		freeBytes = 0;
    BOOL		releaseTaskLevel;

    FAST int		options		= pTyDev->options;
    BOOL		charEchoed	= FALSE;
    STATUS		status		= OK;
char			* p_str;
int		ttyNum;


    /* if task level is flushing input buffers, then just return */

    if (pTyDev->rdState.flushingRdBuf)
	return (ERROR);

    /* If there is an input hook routine to call, call it now.
     * The hook routine will return TRUE if it doesn't want us
     * to process the character further.
     */
    if (pTyDev->protoHook &&
	(*pTyDev->protoHook)(pTyDev->protoArg, inchar) == TRUE)
	{
	return (status);
	}

    /* strip off parity bit if '7 bit' option set */

    if (options & OPT_7_BIT)
	inchar &= 0x7f;

    /* check for abort */

    if ((inchar == tyAbortChar) && (options & OPT_ABORT) && tyAbortFunc != NULL)
	(*tyAbortFunc) ();

    /* check for trap to rom monitor */

    else if ((inchar == tyMonTrapChar) && (options & OPT_MON_TRAP))
	{
	/* attempt to do the reboot from task level, go down no matter what */

	if (_func_excJobAdd != NULL)
	    (* _func_excJobAdd) (reboot, BOOT_WARM_AUTOBOOT);
	else
	    reboot (BOOT_WARM_AUTOBOOT);
	}

    /* check for XON/XOFF received */

    else if (((inchar == XOFF) || (inchar == XON)) && (options & OPT_TANDEM))
	tyWrtXoff (pTyDev, (inchar == XOFF));

    else
	{
	/* count number of chars received while in xoff - see note at
	 * declaration of tyXoffChars
	 */
	if (pTyDev->rdState.xoff)	/** TEMP **/
	    tyXoffChars++;		/** TEMP **/

	/* check for carriage return needs to be turned into linefeed */

	if ((options & OPT_CRMOD) && (inchar == '\r')) 
	    inchar = '\n';

	/* check for output echo required
	 * only echo if the write buffer isn't being deleted or
	 * written to at task level - note that this implies that characters
	 * aren't guaranteed to be echoed.
	 */

	if ((options & OPT_ECHO) &&
	    (!pTyDev->wrtState.wrtBufBusy && !pTyDev->wrtState.flushingWrtBuf))
	    {
	    ringId = pTyDev->wrtBuf;

	    /* echo the char.  some special chars are echoed differently */

	    if (options & OPT_LINE)
		{
		if (inchar == tyDeleteLineChar)
		    {
		    /* echo a newline */
		    RNG_ELEM_PUT (ringId, '\n', nn);
		    charEchoed = TRUE;
		    }

		else if (inchar == tyBackspaceChar)
		    {
		    if (pTyDev->lnNBytes != 0)
			{
			/* echo BS-space-BS */
			rngBufPut (ringId, " ", 3);
			charEchoed = TRUE;
			}
		    }

		else if ((inchar < 0x20) && (inchar != '\n'))
		    {
		    /* echo ^-char */
		    RNG_ELEM_PUT (ringId, '^', nn);
		    RNG_ELEM_PUT (ringId, inchar + '@', nn);
		    charEchoed = TRUE;
		    }

		else
		    {
		    RNG_ELEM_PUT (ringId, inchar, nn);
		    charEchoed = TRUE;
		    }
		}
	    else
		{
		/* just echo the char */
		RNG_ELEM_PUT (ringId, inchar, nn);
		charEchoed = TRUE;
		}

	    if (charEchoed)
		tyTxStartup (pTyDev);
	    }


	/* put the char in the read buffer. */

	ringId = pTyDev->rdBuf;
	releaseTaskLevel = FALSE;

	if (!(options & OPT_LINE))
	    {
	    /* not line-mode;
	     * just enter character and make immediately available to tasks */

	    if (RNG_ELEM_PUT (ringId, inchar, nn) == 0)
		status = ERROR;			/* buffer full, not entered */

	    /* only give the sync sem on first char */

	    if (rngNBytes (ringId) == 1)
		releaseTaskLevel = TRUE;
	    }
	else
	    {
	    /* line-mode;
	     * process backspace, line-delete, EOF, and newline chars special */

	    freeBytes = rngFreeBytes (ringId);

	    if (inchar == tyBackspaceChar)
		{
		if (pTyDev->lnNBytes != 0)
		    pTyDev->lnNBytes--;
		}
	    else if (inchar == tyDeleteLineChar)
		pTyDev->lnNBytes = 0;
	    else if (inchar == tyEofChar)
		{
		/* EOF - check for at least one free byte so there is room
		 * to put the line byte count below */

		if (freeBytes > 0)
		    releaseTaskLevel = TRUE;
		}
	    else
		{
		if (freeBytes >= 2)
		    {
		    if (freeBytes >= (pTyDev->lnNBytes + 2))
			pTyDev->lnNBytes++;
		    else
			status = ERROR;	/* no room, overwriting last char */

		    rngPutAhead (ringId, inchar, (int)pTyDev->lnNBytes);

		    if ((inchar == '\n') || (pTyDev->lnNBytes == 255))
			releaseTaskLevel = TRUE;
		    }
		else
		    status = ERROR;	/* no room, not even for overwriting */
		}

	    /* if line termination indicated, put line byte count
	     * in 0th character of line and advance ring buffer pointers */

	    if (releaseTaskLevel)
		{
		rngPutAhead (ringId, (char) pTyDev->lnNBytes, 0);
		rngMoveAhead (ringId, (int) pTyDev->lnNBytes + 1);
		pTyDev->lnNBytes = 0;
		}
	    }


	/* check if XON/XOFF needs to be output */

	if (options & OPT_TANDEM)
	    {
	    freeBytes = rngFreeBytes (ringId);
	    if (pTyDev->options & OPT_LINE)
		freeBytes -= pTyDev->lnNBytes + 1;

	    if (!pTyDev->rdState.xoff)
		{
		/* if input buffer is close to full, send XOFF */

		if (freeBytes < tyXoffThreshold)
		    tyRdXoff (pTyDev, TRUE);
		}
	    else
		{
		/* if input buffer has enough room now, send XON */

		if (freeBytes > tyXonThreshold)
		    tyRdXoff (pTyDev, FALSE);
		}
	    }

   if (options & OPT_RTS) {
		freeBytes = rngFreeBytes (ringId);
		if (pTyDev->options & OPT_LINE)
			freeBytes -= pTyDev->lnNBytes + 1;


		/* check for the case where the input needs to be suspended */
		if ((freeBytes < tyXoffThreshold) && (rtsChkState(pTyDev) == TRUE))
		{
#if 0
			logMsg("Suspend (tyIRd 1): pTyDev = %x, freeBytes = %d tyXoffThreshold = %d\n", 
				pTyDev, freeBytes, tyXoffThreshold);
#endif
			tyRdRTS(pTyDev, FALSE);
		}


		/* check for the case where the output can be resumed */
		else if (freeBytes > tyXonThreshold && (rtsChkState(pTyDev) == FALSE)) 
		{
#if 0
			logMsg("Resume (tyIRd 2): pTyDev = %x, freeBytes = %x tyXonThreshold = %d\n", 
				pTyDev, freeBytes, tyXonThreshold);
#endif
			tyRdRTS(pTyDev, TRUE);
}

	}


	/* if task level has new input then give read semaphore and
	 * release tasks pended in select
	 */

	if (releaseTaskLevel)
	    {
	    semGive (&pTyDev->rdSyncSem);
	    if (_func_selWakeupAll != NULL)
		(* _func_selWakeupAll) (&pTyDev->selWakeupList, SELREAD);
	    }
	}


p_str = strrchr(pTyDev->devHdr.name, '/');
p_str++;

ttyNum = *p_str - 0x30;

if (freeBytes < min_rng_chars[ttyNum])
min_rng_chars[ttyNum] = freeBytes;

    return (status);
    }


/******************************************************************************
*
* tyRdRTS - set read side of RTS Signal
*
* This routine sets the read side RTS to the specified state.
* a call is made to an Ioctl command that set RTS to the desired state
*
*/

LOCAL void tyRdRTS
    (
    FAST TY_DEV_ID pTyDev,      /* pointer to device structure */
    FAST BOOL state
	 )
{


	extern int	TTYfds[];
	char			* p_str;
	int			ttyfd;

	/* Get the tty number to use for the file discriptor */
	p_str = strrchr(pTyDev->devHdr.name, '/');
	p_str++;

	/* convert from ascii and add the FD offset for the MMSC assignments */
	ttyfd = TTYfds[*p_str - 0x30];

	if (state == FALSE) {
		if (ioctl(ttyfd, SIO_RTS_SUSPEND, 0) == OK)
			 rtsSetState(pTyDev, FALSE);
	}
	else {
		if (ioctl(ttyfd, SIO_RTS_RESUME, 0) == OK)
			rtsSetState(pTyDev, TRUE);
	}
}



/******************************************************************************
*
* tyRdXoff - set read side xon/xoff
*
* This routine sets the read side xon/xoff to the specified state.
* A flag will be set indicating that the next character sent by the
* transmit side should be the xon or xoff character.  If the transmitter
* is idle, it is started.
*/

LOCAL void tyRdXoff
    (
    FAST TY_DEV_ID pTyDev,      /* pointer to device structure */
    FAST BOOL xoff
    )
    {
    FAST int oldlevel;

    oldlevel = intLock ();				/* LOCK INTERRUPTS */

    if (pTyDev->rdState.xoff != xoff)
	{
	pTyDev->rdState.xoff	= xoff;
	pTyDev->rdState.pending = TRUE;

	if (!pTyDev->wrtState.busy)
	    {
	    pTyDev->wrtState.busy = TRUE;

	    intUnlock (oldlevel);			/* UNLOCK INTERRUPTS */

	    (*pTyDev->txStartup) (pTyDev);
	    return;
	    }
	}

    intUnlock (oldlevel);                   		/* UNLOCK INTERRUPTS */
    }
/******************************************************************************
*
* tyWrtXoff - set write side xon/xoff
*
* This routine sets the write side xon/xoff to the specified state.
* If the new state is xon and the transmitter is idle, it is started.
*
*/

LOCAL void tyWrtXoff
    (
    FAST TY_DEV_ID pTyDev,      /* pointer to device structure */
    BOOL xoff
    )
    {
    FAST int oldlevel;

    /* restart an XOFF'd transmitter */

    oldlevel = intLock ();				/* LOCK INTERRUPTS */

    if (pTyDev->wrtState.xoff != xoff)
	{
	pTyDev->wrtState.xoff = xoff;

	if (!xoff)
	    {
	    if (!pTyDev->wrtState.busy)
		{
		pTyDev->wrtState.busy = TRUE;

		intUnlock (oldlevel);                  	/* UNLOCK INTERRUPTS */

		(*pTyDev->txStartup) (pTyDev);
		return;
		}
	    }
	}

    intUnlock (oldlevel);	                   	/* UNLOCK INTERRUPTS */
    }
/******************************************************************************
*
* tyTxStartup - startup transmitter if necessary
*
* This routine starts the transmitter if it is not already busy.
* A flag is maintained so that starting the transmitter can be properly
* interlocked with interrupt level, but the startup routine itself is
* called at the interrupt level of the caller.
*/

LOCAL void tyTxStartup
    (
    FAST TY_DEV_ID pTyDev       /* pointer to device structure */
    )
    {
    FAST int oldlevel;

    /* if xmitter not busy, start it */

    if (!pTyDev->wrtState.busy)
	{
	oldlevel = intLock ();				/* LOCK INTERRUPTS */

	/* check xmitter busy again, now that we're locked out */

	if (!pTyDev->wrtState.busy)
	    {
	    pTyDev->wrtState.busy = TRUE;

	    intUnlock (oldlevel);                   	/* UNLOCK INTERRUPTS */

	    (*pTyDev->txStartup) (pTyDev);
	    return;
	    }

	intUnlock (oldlevel);                   	/* UNLOCK INTERRUPTS */
	}
    }

/******************************************************************************
*
* rtsChkState - Return the RTS flag status for the given channel. True 
*					 indicates a "RESUME" condition.
*					
*				 This routine is necessary because we do not have the visability
*				 to the low level driver information.
*
*/

LOCAL STATUS rtsChkState
    (
    FAST TY_DEV_ID pTyDev      /* pointer to device structure */
	 )
{

	char			* p_str;
	int		ttyNum;

	/* Get the tty number to use for the file discriptor */
	p_str = strrchr(pTyDev->devHdr.name, '/');
	p_str++;

	ttyNum = *p_str - 0x30;

	return(rts_state[ttyNum]);
}

/****************************************************************************
*
* rtsSetState - Set the state of the rts_state varible for the given channel
*					
*				 This routine is necessary because we do not have the visability
*				 to the low level driver information.
*
*/

LOCAL void rtsSetState
    (
    FAST TY_DEV_ID pTyDev,      /* pointer to device structure */
    BOOL state						  
	 )
{

	char			* p_str;
	int			ttyNum;
	

	/* Get the tty number to use for the file discriptor */
	p_str = strrchr(pTyDev->devHdr.name, '/');
	p_str++;

	ttyNum = *p_str - 0x30;

	if (state)
		rts_state[ttyNum] = TRUE;
	else
		rts_state[ttyNum] = FALSE;
}
