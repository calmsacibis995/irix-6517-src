#ifndef __SYS_SGIGSC_H__
#define __SYS_SGIGSC_H__
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  sgigsc.h
 *	- Constants and structure definitions for the Silicon Graphics graphics
 *	  system call.
 *
 *  $Revision: 3.90 $
 */

/*
 * Machine independent operations
 */
#define	SGWM_LOCK	1		/* lock graphics hardware */
#define	SGWM_UNLOCK	2		/* unlock graphics hardware */
#define	SGWM_SIO	3		/* steal sio channel */
#define	SGWM_UNSIO	4		/* unsteal sio channel */
#define	SGWM_WMRUNNING	5		/* query window manager existence */
#define	SGWM_MEWM	6		/* declare process as window manager */
#define	SGWM_LOSTGR	9		/* test for graphics capability */
#define	SGWM_TIEMOUSE	10		/* tie mouse to cursor */
#define	SGWM_UNTIEMOUSE	11		/* untie mouse from cursor */
#define	SGWM_CONSOLE	12		/* switch unix console to stream dev */
#define	SGWM_QMEM	13		/* map shared q */
#define	SGWM_UNQMEM	14		/* unmap shared q */
#define	SGWM_WMMODELOCK	15		/* WM lock/unlock mode change lock */
#define	SGWM_INIT	16		/* mark process as graphics process */

/*
 * Machine dependent operations
 */

/* GT/GTX */
#define	SG4D80_CMD_BASE		100
#define	SG4D80_TAKECX		100	/* take a context away */
#define	SG4D80_OLDGIVECX	101	/* give a context to somebody */
#define	SG4D80_GIVECX		102	/* give a context to somebody */
#define	SG4D80_RESET		104	/* reset the GM */
#define	SG4D80_MAPALL		105	/* map all GM memory and FIFO */
#define	SG4D80_GM_CMD		106	/* pass a command to the GM */
#define	SG4D80_INIT		107	/* mark process as a graphics process */
#define	SG4D80_WINADDR		108	/* get user addresses for a context */
#define	SG4D80_GET_RVTYPE	110	/* get rv board type from gm/host shm */
#define	SG4D80_ACT_CX		111	/* activate a context */
#define	SG4D80_DEACT_CX		112	/* deactivate a context */
#define	SG4D80_STORE_CX		113	/* unload a context */
#define	SG4D80_FEEDBACK		114	/* start a feedback operation */
#define	SG4D80_PICK		115	/* start pick/select mode */
#define	SG4D80_ENDPICK		116	/* stop pick/select mode */
#define	SG4D80_PROBE		117	/* probe for presence of hardware */
#define	SG4D80_RELEASE_CX	118	/* give up current context */
#define	SG4D100_PIXEL_DMA	119	/* do pixel dma */
#define	SG4D100_MAPALL		120	/* map gm for diagnostics */
#define	SG4D100_WINADDR		121	/* get user addresses for a context */
#define	SG4D100_INIT	SG4D100_WINADDR
#define	SG4D100_RECTWRITE	123	/* write a screen rectangle */
#define	SG4D80_SWAPBUFFERS	124	/* swap display buffers (w/modelock) */
#define	SG4D80_CHANGE_MODE	125	/* change display mode (w/modelock) */
#define	SG4D80_DBL_BUFFER	126	/* set dbl buffer mode (w/modelock) */
#define	SG4D80_MAP_MODE		127	/* set color map mode (w/modelock) */
#define	SG4D80_SETMAP		128	/* set multimap map nbr (w/modelock) */
#define	SG4D80_WIDCHECK		129	/* set imp wid chk mode (w/modelock) */
#define SG4D100_RECTREAD	130	/* read a screen rectangle */

#define	SG4DSP_SWAPBUFFERS	131	/* swap display buffers (w/modelock) */
#define	SG4DSP_CHANGE_MODE	132	/* change display mode (w/modelock) */
#define	SG4DSP_INIT_CX		133	/* initialize a new context */
#define	SG4DSP_COPY		134	/* copy in/out to memory */
#define	SG4DSP_TEXBIND		135	/* load/select a texture map */
#define	SG4DSP_TEXSTATE		136	/* load/store texture state */
#define	SG4DSP_ACCUMBUFFER	137	/* declare use of accumulation buffer */
#define	SG4DSP_MAPMEM		138	/* lock and map user memory */

#define	SG4D80_CMD_LIMIT	199

/* GR1 (ECLIPSE) */
#define SGGR1_CMD_BASE		200
#define SGGR1_PROBE		236
#define SGGR1_DOWNLD		233
#define SGGR1_CHECKRE		244
#define SGGR1_PROMIO		234
#define SGGR1_MAPALL		200
#define SGGR1_GINIT		201
#define SGGR1_RELEASE_CX	235
#define SGGR1_XMAPMODELO	202
#define SGGR1_XMAPMODEHI	203
#define SGGR1_MAPCOLOR		204
#define SGGR1_GETMCOLOR		223
#define SGGR1_MAPAUXCOLOR	205
#define SGGR1_SWAPBUFFERS	206
#define SGGR1_SWAPINTERVAL	207
#define SGGR1_BLINK		208
#define SGGR1_CYCLEMAP		209
#define SGGR1_GAMMARAMP		210
#define SGGR1_SETCURSOR		211
#define SGGR1_MAPCURSCOLOR	213
#define SGGR1_CURSDISPLAY	212
#define SGGR1_DISPLAYREG	214
#define SGGR1_BLANKSCREEN	215
#define SGGR1_SETMONITOR	216
#define SGGR1_GETMONITOR	222
#define SGGR1_DMA		225
#define SGGR1_GETHWCONFIG	226
#define SGGR1_PICK		219
#define SGGR1_ENDPICK		220
#define SGGR1_GSYNC		221
#define SGGR1_CREATECX		232
#define SGGR1_INITCX		243
#define SGGR1_DESTROYCX		242
#define SGGR1_GIVECX		227
#define SGGR1_TAKECX		228
#define SGGR1_ACT_CX		229
#define SGGR1_DEACT_CX		230
#define SGGR1_STORECX		237
#define SGGR1_LOADCX		238
#define SGGR1_LOADMODE		245
#define SGGR1_WRITECX		239
#define SGGR1_GETCURSOFF        289
#define	SGGR1_CMD_LIMIT		299
/*XXX Highest cmd so far = 245 */

/*XXX TEMPORARY: ECLIPSE TESTING */
#define SGGR1_CURSOFFHACK	291
#define SGGR1_POSCURSOR		296
#define SGGR1_MODERD		297
#define SGGR1_PROTECT_HQMMSB	298

/* Operations for CG2 board, valid on 4D/60, 4D/70 and 4D/70GT */
#define SG4DCG2_CMD_BASE	400
#define	SG4DCG2_READ_REG	400	/* read any CG2 register */
#define	SG4DCG2_WRITE_REG	401	/* write any CG2 register */
#define SG4DCG2_CMD_LIMIT	419

/* Operations for VMUX board, valid on 4D/20 */
#define SG4DVMUX_CMD_BASE	420
#define	SG4DVMUX_READ_REG	420	/* read any VMUX register */
#define	SG4DVMUX_WRITE_REG	421	/* write any VMUX register */
#define SG4DVMUX_CMD_LIMIT	439

/* 
 * Operations for GR1_AUXSCRN design -- be extra careful and don't reuse
 * until 1991, just in case.
 */
#define SGAUX_CMD_BASE		500
#define SGAUX_ALLOCSCRN		500	/* allocate a screen */
#define SGAUX_DEALLOCSCRN	501	/* deallocate a screen */
#define SGAUX_SETOUTSCRN	502	/* set graphics output screen */
/* XXX the following 2 are only needed until the window manager is changed to
 * keep track of the currrent input focus screen */
#define SGAUX_SETINSCRN		503	/* set graphics input (mouse tracking)
					 * screen */
#define SGAUX_GETINSCRN		505	/* return the graphics input (mouse 
					 * tracking) screen */
#define	SGAUX_GETNSCRNS		504	/* return number of graphics screens
					 * associated with the current
					 * display */
#define SGAUX_CURSDISPLAY	SGGR1_CURSDISPLAY
#define SGAUX_CMD_LIMIT		509


/*
 * Data structures for SGWM_QMEM operation.
 * All the data structures described below are assumed to be aligned on
 * a longword boundary.
 */

/*
 * This structure defines the argument for the SGWM_QMEM system call.
 */
struct	sgigsc_qmem {
	char	*base;			/* base address of q memory */
	long	entries;		/* number of q entries */
};

/*
 * This structure defines an entry in the q.  Each entry is defined to
 * be 8 longwords in length, aligned on a longword boundary.
 */
struct	sgigsc_qentry {
	struct {
		long	seconds;
		long	microseconds;
	} timeStamp;			/* time of event; relative to boot */

	long	event;			/* unique event identifier */

	union {
		long	value[5];		/* maximum data area */
		unsigned char ascii;		/* ascii code */
		long	pid;			/* process id */

		struct {
			long	x;		/* mouse x coordinate */
			long	y;		/* mouse y coordinate */
			long	buttons;	/* mouse button data */
		} mouse;

		struct {
			dev_t	dev;		/* unix device */
			unsigned char c;	/* character from sio device */
		} sio;
	} ev;				/* event value */
};

/* If timeStamp.seconds is SGT_TIME_RELATIVE
** then timeStamp.microseconds lbolt.
*/
#define SGT_TIME_RELATIVE 0xFFFFFFFF

/* definitions for bits in of ev.mouse.buttons */
#define	SGE_MOUSE_RIGHTMOUSE	0x01
#define	SGE_MOUSE_MIDDLEMOUSE	0x02
#define	SGE_MOUSE_LEFTMOUSE	0x04
#define	SGE_LPEN_BUTTON		0x01

/*
 * This structure defines an actual q.  The first two fields are the index's
 * for the q entry and remove points.  The final field is a variable length
 * array containing the actual q entries.  The number of entries is defined
 * when the SGWM_QMEM call is given, via the sgigsc_mem.entries field.
 */
struct	sgigsc_q {
	long	qin;			/* index for next q enter */
	long	qout;			/* index for next q remove */
	struct sgigsc_qentry q[1];	/* actual q */
};

#define SGWM_MINQSIZE	32		/* minimum queue size allowed */

/*
 * Event identifiers (sgigsc_qentry.event).
 */
#define	SGE_PDEATH	1		/* graphics process death */
#define	SGE_PBLOCK	2		/* graphics process blocked */
#define	SGE_KB		3		/* keyboard data */
#define	SGE_MOUSE	4		/* mouse data */
#define	SGE_SIO		5		/* sio data */
#define	SGE_LPEN	6		/* light pen data */

/*
 * Definitions for structures passed as arguments to sgigsc.
 */
typedef struct sgigsc_4D80_gm_cmd {
	int cmd;	/* command to be passed to the GM */
	char *sendp;	/* pointer to data to send to GM */
	int slen;	/* length in bytes of send data */
	char *recvp;	/* pointer to buffer for receive response */
	int rlen;	/* length in bytes of receive buffer */
} sg_4D80_gm_cmd_t;

typedef struct sgigsc_4D80_mapall {
	char	*dramp;		/* pointer to the DRAM */
	char	*pipep;		/* pointer to the FIFO */
} sg_4D80_mapall_t;

typedef struct sgigsc_4D80_winaddr {
	char	*pipep;		/* pointer to the FIFO */
	char	*feedp;		/* pointer to the feedback area */
} sg_4D80_winaddr_t;

typedef struct sgigsc_4D100_mapall {
	char	*trigp;		/* pointer to the graphics trigger */
	char	*pipep;		/* pointer to the FIFO */
	char	*tagp;		/* pointer to the tag SRAM */
	char	*dramp;		/* pointer to the shared memory area */
} sg_4D100_mapall_t;

typedef struct sgigsc_4D100_winaddr {
	char	*pipep;		/* pointer to the FIFO */
	char	*feedp;		/* pointer to the feedback area */
	char	*trigp;		/* pointer to the graphics trigger */
} sg_4D100_winaddr_t;

typedef struct sgigsc_4D80_givecx {	/* for SG4D80_GIVECX */
	int	pid;			/* process to receive the context */
	int	cx;			/* context number */
	int	screen;			/* screen number */
} sg_4D80_givecx_t;

typedef struct sgigsc_4D80_swapbuffers {	/* for SG4D80_SWAPBUFFERS */
	int	bankselect;
	int	displaybank;
	unsigned long	*modeaddr;
	unsigned long	modemask;
} sg_4D80_swapbuffers_t;

typedef struct sgigsc_4D80_change_mode {	/* for SG4D80_CHANGE_MODE */
	int	newmode;
	int	epmode;
	int	spmode;
	unsigned long	*modeaddr;
	unsigned long	modemask;
} sg_4D80_change_mode_t;

typedef struct sgigsc_4D80_map_mode {		/* for SG4D80_MAP_MODE */
	int	newmode;
	unsigned long	*modeaddr;
	unsigned long	modemask;
} sg_4D80_map_mode_t;

typedef struct sgigsc_4D80_setmap {		/* for SG4D80_SETMAP */
	int	mapnum;
	unsigned long	*modeaddr;
	unsigned long	modemask;
	unsigned long	modeval;
} sg_4D80_setmap_t;

typedef struct sgigsc_4D80_widcheck {		/* for SG4D80_WIDCHECK */
	int	enable;
	unsigned long	*widaddr;
	short	*wshm_widaddr;
} sg_4D80_widcheck_t;

typedef struct sgigsc_4D80_store_cx {	/* for SG4D80_STORE_CX */
	int	cx;		/* context to be unloaded */
	caddr_t	buffer;		/* user virtual address of unload buffer */
	int	buflen;		/* length of unload buffer in bytes */
} sg_4D80_store_cx_t;

typedef struct sgigsc_4D80_feedback {	/* for SG4D80_FEEDBACK */
	int	pipeaddr;	/* pipe address to write */
	int	pipedata;	/* data to be written */
	caddr_t	buffer;		/* user virtual address of feedback buffer */
	int	buflen;		/* length of feedback buffer in bytes */
	int	overflow;	/* buffer overflow flag (set by sgigsc) */
} sg_4D80_feedback_t;

typedef struct sgigsc_4D80_pick {	/* for SG4D80_PICK */
	caddr_t	buffer;		/* user virtual address of feedback buffer */
	int	buflen;		/* length of feedback buffer in bytes */
} sg_4D80_pick_t;

typedef struct sgigsc_4D80_endpick {	/* for SG4D80_ENDPICK */
	int	pipeaddr;	/* pipe address to write */
	int	pipedata;	/* data to be written */
	int	overflow;	/* buffer overflow flag (set by sgigsc) */
} sg_4D80_endpick_t;

typedef struct sgigsc_4D100_pixel_dma {	/* for SG4D100_PIXEL_DMA */
	int	optype;		/* 0 -> read, 1 -> write */
	caddr_t	buffer;		/* user virtual address of buffer */
	int	buflen;		/* length to transfer in bytes */
	int	offset;		/* offset into pixel buffer in bytes */
} sg_4D100_pixel_dma_t;

typedef struct sgigsc_4D100_rectwrite {	/* for SG4D100_RECTWRITE */
	caddr_t	buffer;		/* user virtual address of buffer */
	int	pixsize;	/* size in bytes of each pixel */
	int	x;		/* x coord of lower left corner */
	int	y;		/* y coord of lower left corner */
	int	width;		/* width in pixels of rect before zoom */
	int	height;		/* height in pixels of rect before zoom */
	int	xzoom;		/* x zoom factor */
	int	yzoom;		/* y zoom factor */
} sg_4D100_rectwrite_t;

typedef struct sgigsc_4DSP_change_mode {	/* for SG4DSP_SWAPBUFFERS */
	unsigned long	xmapmode;		/* and SG4DSP_CHANGE_MODE */
	unsigned long	*modeaddr;
	unsigned long	modeval;
} sg_4DSP_change_mode_t;

typedef struct sgigsc_4DSP_init_cx {		/* for SG4DSP_INIT_CX */
	int	cx;			/* context number */
	int	screen;			/* screen number */
} sg_4DSP_init_cx_t;

typedef struct sgigsc_4DSP_copy {	/* for SG4DSP_COPY */
	caddr_t buffer;		/* user virtual address of buffer */
	int	buflen;		/* length to transfer in bytes */
	int	flag;		/* 0 = load, 1 = store to main memory */
	int	cpcmd;		/* CP cmd */
	int	cpdata[15];	/* actual CP data values */
	int 	cpdatalen;	/* number of CP data items */
} sg_4DSP_copy_t;

typedef struct sgigsc_4DSP_texbind {	/* for SG4DSP_TEXBIND */
	long	npixels;	/* number of pixels in texture map */
	caddr_t vaddr;		/* user virtual address of texture map */
	long	compress;	/* 1 if the map is in compress format  */
} sg_4DSP_texbind_t;

typedef struct sgigsc_4DSP_texstate {	/* for SG4DSP_TEXSTATE */
	int	cx;		/* context number */
	int	buflen;		/* maximum buffer length */
	caddr_t buffer;		/* user virtual address of buffer */
	int	flag;		/* 0 = load, 1 = store to main memory */
} sg_4DSP_texstate_t;

typedef struct sgigsc_4DSP_mapmem {	/* for SG4DSP_MAPMEM */
	caddr_t buffer;		/* user virtual address of buffer */
	int	buflen;		/* length to transfer in bytes */
	int	flag;		/* 0 = unmap, 1 = map user memory */
} sg_4DSP_mapmem_t;

/******************************************************************************
 * GR1 (Eclipse) sgigsc() system call argument structures.
 *****************************************************************************/

/*XXX TEMPORARY FOR ECLIPSE TESTING */
typedef struct sggr1_moderd {
	long xmap;
	long *modes;
} sggr1_moderd_t;

typedef struct sggr1_poscursor {
	long x;
	long y;
} sggr1_poscursor_t;

typedef struct sggr1_cursoffhack {
	long xoff;
	long yoff;
} sggr1_cursoffhack_t;


/*
 * XMAP2 operation argument structures
 */

typedef struct sggr1_xmapmodelo {
	unsigned char colmode;
	unsigned char bufsel;
	unsigned char overlay;
	unsigned long *modeaddr;
	unsigned long modemask;
} sggr1_xmapmodelo_t;

typedef struct sggr1_xmapmodehi {
	unsigned char underlay;
	unsigned char multimap;
	unsigned char mapsel;
	unsigned long *modeaddr;
	unsigned long modemask;
} sggr1_xmapmodehi_t;

typedef struct sggr1_mapcolor {
	short index;
/*XXX shorts as in GL doc or chars? */
	unsigned char red;
	unsigned char green;
	unsigned char blue;
} sggr1_mapcolor_t;

typedef struct sggr1_getmcolor {
	short index;
/*XXX shorts as in GL doc or chars? */
	unsigned char red;
	unsigned char green;
	unsigned char blue;
} sggr1_getmcolor_t;

typedef struct sggr1_mapauxcolor {
	short index;
/*XXX shorts or chars? */
	unsigned char red;
	unsigned char green;
	unsigned char blue;
} sggr1_mapauxcolor_t;

typedef struct sggr1_swapinterval {
	short frames;
} sggr1_swapinterval_t;

typedef struct sggr1_blink {
	short rate;
	unsigned short index;
	unsigned char red, green, blue;
} sggr1_blink_t;

typedef struct sggr1_cyclemap {
	short map;
	short nextmap;
	short duration;
} sggr1_cyclemap_t;


/*
 * DAC operation argument structures
 */

typedef struct sggr1_gammaramp {
/*XXX shorts or chars? */
	unsigned short *red;
	unsigned short *green;
	unsigned short *blue;
} sggr1_gammaramp_t;


/*
 * Cursor operation argument structures
 */

typedef struct sggr1_setcursor {
	char type;
	char xorigin;
	char yorigin;
	unsigned char *glyph;
} sggr1_setcursor_t;

typedef struct sggr1_mapcurscolor {

	char index;		/* Eclipse has only 1 color entry but we may
				   have more later */
/*XXX shorts or chars? */
	unsigned char red;
	unsigned char green;
	unsigned char blue;
} sggr1_mapcurscolor_t;

typedef struct sggr1_cursdisplay {
	char display;			/* boolean: on or off */
} sggr1_cursdisplay_t;


/*
 * Display Register operation argument structures
 */

typedef struct sggr1_displayreg {
	char regnum;
	unsigned char val;
} sggr1_displayreg_t;

typedef struct sggr1_blankscreen {
	char blankscreen;
} sggr1_blankscreen_t;

typedef struct sggr1_setmonitor {
	unsigned char monitortype;
	unsigned char flag;
	long xmaxscreen;
	long ymaxscreen;
	long xcursmagic;
	long ycursmagic;
	long topscan;
} sggr1_setmonitor_t;

typedef struct sggr1_getmonitor {
	unsigned char monitortype;
	unsigned char flag;
} sggr1_getmonitor_t;

/* Display control flags: */
#define SGGR1_SYNCGRN	0x01		/* Composite sync on green */
#define SGGR1_BLANK	0x02		/* Screen is currently off */
#define SGGR1_STEREO	0x04		/* Stereo optic display enabled */
#define SGGR1_EXTCLK	0x08		/* Timing locked to external clock
					   (ie. in genlock mode) */

#define SGGR1_OVERRIDE_TOPSCAN 0x10	/* Use the topscan value supplied with
					   struct sggr1_setmonitor instead of
					   determining it from monitortype;
					   never set by sggr1_getmonitor */

/*
 * Graphics DMA argument structures
 */

typedef struct sggr1_dma {
	long token;			/* Token to send down pipe */
	long x;				/* Rectangular coordinate in pixels */
	long xlen;
	long y;
	long ylen;
	char *buf;			/* Data buffer */
	unsigned short flags;		/* Bit flags defined below */
	char pixsize;			/* Pixel size (1, 2, 4 bytes) */
	char pmoffset;			/* Number of bits of the first CPU
					   word of each scanline that are
					   to be ignored */
	long pmstride;			/* Number of 32-bit CPU words per
					   scanline */
} sggr1_dma_t;

/* Flags for graphics DMA */
#define GR1_READ	0x01		/* DMA direction: gfx to host */
#define GR1_WRITE	0x02		/* DMA direction: host to gfx */
#define GR1_LNBYLN	0x04		/* Line by line DMA: 1 line at a time */
#define GR1_SINGLE	0x08		/* Single line DMA (special case) */
#ifdef LATER	/* XXX */
#define GR1_INTR	0x10		/* Ucode should interrupt when done */
#endif 
#define GR1_STRIDE	0x20		/* Stride DMA */


/*
 * Misc. operation argument structures
 */

typedef struct sggr1_gethwconfig {
	char bitplanes;
	char zbuf;
	char gsnr;		/* in value: screen that inquiry is for */
	char _filler;
	long invstate;		/* added because shared GL can't import hinv 
				 * routines from libc */
} sggr1_gethwconfig_t;

typedef struct sggr1_pick {	/* for SGGR1_PICK */
	char *buffer;		/* user virtual address of feedback buffer */
	long buflen;		/* length of feedback buffer in 32 bit words */
} sggr1_pick_t;

typedef struct sggr1_endpick {	/* for SGGR1_ENDPICK */
	long token;		/* token kernel sends to GE for GL */
	long overflow;		/* buffer overflow flag (set by sgigsc) */
} sggr1_endpick_t;


/*
 * Window Manager operation argument structures
 */

typedef struct sggr1_initcx {	/* for SGGR1_INITCX */
	long cxid;		/* New context to be initialized */
	long wid;		/* Window ID of new context */
} sggr1_initcx_t;

typedef struct sggr1_givecx {
	long pid;		/* Process to receive the context */
	long cx;		/* Context number */
	long wid;		/* Window ID that context should use*/
} sggr1_givecx_t;

typedef struct sggr1_storecx {	/* for SGGR1_STORECX */
	long	cxid;		/* context to be unloaded */
	char	*buf;		/* user virtual address of unload buffer */
	long	buflen;		/* length of unload buffer in bytes */
} sggr1_storecx_t;

typedef struct sggr1_loadcx {	/* for SGGR1_LOADCX */
	long	cxid;		/* context to be loaded */
	long	wid;		/* Window ID of context */
	char	*buf;		/* user virtual address of load buffer */
	long	buflen;		/* length of buffer to load in bytes */
} sggr1_loadcx_t;

typedef struct sggr1_loadmode {	/* for SGGR1_LOADMODE */
	long	cxid;		/* context who's mode is being loaded */
	long	wid;		/* Window ID that context should use*/
} sggr1_loadmode_t;

typedef struct sggr1_writecx {	/* for SGGR1_WRITECX */
	long cxid;		/* Context writing into */
	long addr;		/* Data RAM addr in words */
	long count;		/* Number of words */
	unsigned long *buf;	/* Buffer to write into context */
} sggr1_writecx_t;

/* argument structure for SG4DCG2_READ_REG and SG4DCG2_WRITE_REG */
typedef struct sgigsc_4DCG2_cgreg {
    unsigned int reg;		/* register number */
    unsigned char value;	/* value */
} sg_4DCG2_cgreg_t;

/* Defines for CG2 register numbers */
#define SG_CG_MODE_REG		0
#define SG_CG_CONTROL_REG	1
#define SG_CG_HORPHASE_REG	2
#define SG_CG_SUBPHASE_REG	3
#define SG_CG_LASTREG		SG_CG_SUBPHASE_REG

typedef sg_4DCG2_cgreg_t sg_4DVMUX_reg_t;

/* Defines for VMUX register numbers */
#define SG_VMUX_CH1_DELAY_REG	0
#define SG_VMUX_CH1_PHASE_REG	1
#define SG_VMUX_CH2_DELAY_REG	2
#define SG_VMUX_CH2_PHASE_REG	3
#define SG_VMUX_CONTROL_REG	4
#define SG_VMUX_LASTREG		SG_VMUX_CONTROL_REG



/* Arguments to SGWM_WMMODELOCK and SGWM_GLMODELOCK */
#define SG_MODELOCK	0
#define SG_MODEUNLOCK	1

/*
 * Error codes returned by sgigsc()
 */
/*
 * GT specific errors
 */
#define	E_GSC_4D80_BASE		160	/* XXX */
#define E_GSC_4D80_TOOSHORT	(E_GSC_4D80_BASE+0)
#define E_GSC_4D80_TOOLONG	(E_GSC_4D80_BASE+1)
#define E_GSC_4D80_WAITING	(E_GSC_4D80_BASE+2)
#define E_GSC_4D80_GMHUNG	(E_GSC_4D80_BASE+3)
#define E_GSC_4D80_GMNOTREADY	(E_GSC_4D80_BASE+4)
#define E_GSC_4D80_PICKING	(E_GSC_4D80_BASE+5)
#define E_GSC_4D100_DMA_ERR	(E_GSC_4D80_BASE+6)

/*
 * Kernel data structures for sgigsc()
 */
#ifdef _KERNEL
#include "sys/sema.h"

/*
 * sgigsc state structure
 */
typedef struct sgstate_s {
	short	flags;		/* random flags */
	struct	sgigsc_q *q;	/* kernel virtual addr of q */
	caddr_t	uvaddr;		/* users virtual addr of q */
	int	qlen;		/* length of total q memory, in bytes */
	int	pages;		/* # of pages used to map q */
	int	entries;	/* max # of entries in the q */
	struct	proc *wman;	/* proc pointer of window manager process */
#ifdef	SVR0
	struct	pte *pte;	/* pte pointer to users base addr */
#endif
#ifdef	SVR3
	sema_t	waitsema;	/* to queue clients when window mgr is slow */
	int	waittmo;	/* timeout id that will wakeup waitsema wtrs */
	sema_t	wmmodeq;	/* to queue the WM for the mode change lock */
	sema_t	glmodeq;	/* to queue clients for the mode change lock */
	int	glmodelock;	/* count of clients who have the mode lock */
	lock_t	sglock;		/* spinlock to protect sgstate structure */
	lock_t	qlock;		/* spinlock to protect event queue */
	int	lockcount;	/* nesting count for graphics lock calls */
	sema_t	gfxsema;	/* graphics semaphore - need this to image */
	struct	graph_s	*owner;	/* graphics info struct of semaphore owner */
#endif
} sgstate_t;

extern sgstate_t sgstate;

/* bits in sgstate.flags */
#define	OPEN		0x01	/* somebody has us open */
#define	WMRUNNING	0x02	/* window manager is running */
#define	QMEM		0x04	/* q memory is shared */
#define	WANTMODELK	0x08	/* the window manager is waiting for mode lk */

/* Flag for gfx_psema */

#define NOSLEEP		0
#define SLEEP		1

/*
 * Definition of generic functions externalized by a graphics
 * output driver for use by the rest of the kernel.
 */
typedef struct gr_out_s {
	void    (*gr_poscursor)();	/* position the cursor */
	void    (*gr_blank)();		/* blank and unblank the screen */
	void    (*gr_stop)();		/* stop graphics output */
	void    (*gr_ismex)();		/* set state var 'mex is running' */
	struct pregion *(*gr_init)();	/* mark process as graphics process */
	void    (*gr_exit)();		/* called on process exit */
	int     (*gr_disp)();		/* check if process can be dispatched */
	void    (*gr_glock)();		/* lock graphics */
	void    (*gr_gresume)();		/* finish context switch */
	int     (*gr_remap)();		/* remap graphics resources */
	void    (*gr_gswtchout)();	/* context switch out of a process */
	void    (*gr_killall)();	/* kill all graphics processes */
	void	(*gr_setcursorpos)();	/* set saved cursor position */
	void	(*gr_getcursorpos)();	/* get saved cursor position */
} gr_out_t;

extern gr_out_t *gr_out_p;

/*
 * Exported sgigsc functions
 */
extern struct proc	*gsc_window_manager();
extern int		gsc_gfx_locked();
extern int		gsc_glmodelock(int);
extern int		gfx_psema(struct graph_s *, int);
extern void		gfx_vsema(struct graph_s *);
extern int		gfx_valusema();
extern struct graph_s	*gfx_owner();
extern void		gfx_lock();
extern void		gfx_unlock();

/*
 * System parameters for GT graphics
 */
#define SG_4D80_FIFO_VADDR	0x1000	/* FIFO address in user space */
#define SG_4D80_MAXCONTEXT	16	/* number of graphics contexts in HW */
#define SG_4D80_MAXFEEDBACK	0x10000	/* max feedback buffer in bytes */

/*
 * Miscellaneous argument defines
 */
#define NOFORCE		0	/* Do not force an action */
#define FORCE		1	/* Force an action */

#else	/* !_KERNEL */

extern int sgigsc(int, ...);

#endif	/* !_KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* !__SYS_SGIGSC_H__ */
