/*
 * Standard Jukebox Interface implemented via a SCSI passthru driver.
 */
#include <sys/types.h>
#include <sys/ioctl.h>
#include <dslib.h>
#include <sys/scsi.h>
#include <sys/fcntl.h>
#include <string.h>
#include <bstring.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "control.h"

#define getsense(dsp) \
	  ((unchar *)(((struct context *) dsp->ds_private)->dsc_sense))
#define getcmd(dsp)	(((struct context *) dsp->ds_private)->dsc_cmd)
#define ADDSENSEQUAL(p) (((unchar *)(p))[13])

#define SCSI_MVMED	0xa5	/* SCSI command to move media on a jukebox */
#define SCSI_IELEM	0x07	/* SCSI command to inventory media */
#define SCSI_RELEM	0xb8	/* SCSI command to read info for a slot */
#define SCSI_DOOR	0x1e	/* SCSI command to lock jukebox door */

/*
 * Store some private information for the jukebox.
 */
struct devinfo {
	struct dsreq	*dsp;	/* handle into /dev/scsi driver */
	int		jbtype;	/* type of jukebox we are talking to */
	int		DTaddr;	/* Data Transfer address (ie: tape drive) */
	int		IEaddr;	/* Import Element address (ie: port) */
	int		STaddr;	/* Storage address (ie: slot) */
	int		MTaddr;	/* Medium Transport address (ie: robot arm) */
} devi;
#define	JBTYPE_DECDLT27	1	/* a DEC DLT2700, DLT2700xt, DLT4700, DLT7700 robot */
#define	JBTYPE_DECDLT25	2	/* a DEC DLT2500, DLT2500xt, DLT4500, DLT7500 robot */
#define	JBTYPE_EXA10I	3	/* an Exabyte 10i stacker */
#define	JBTYPE_NTP	4	/* an NTP stacker */
#define	JBTYPE_DIANA1	5	/* a Fujitsu 3480-style Diana-1 */
#define	JBTYPE_DIANA2	6	/* a Fujitsu 3480-style Diana-2 */
#define	JBTYPE_DIANA3	7	/* a Fujitsu 3480-style Diana-3 */
#define	JBTYPE_MGSTRMP	8	/* an IBM Magstar MP 3570 */
#define JBTYPE_DST410   9       /* an Ampex DST 41x robot */
#define JBTYPE_DST810   10      /* an Ampex DST 81x robot */
#define JBTYPE_RIALTO   11      /* SGI DLT-7000 stacker (OEM'd from HP) */


#define IS_DIANA1(x)	((x) == JBTYPE_DIANA1)
#define IS_DIANA2(x)	((x) == JBTYPE_DIANA2)
#define IS_DIANA3(x)	((x) == JBTYPE_DIANA3)

#define IS_FUJTAPE(x)	(IS_DIANA1(x) || IS_DIANA2(x) || IS_DIANA3(x))

/**************  START of Fujitsu Diana-1 parameters  ***********************/
#define G6_SENSCART	0xCA	/* Fujitsu Diana-1 Sense Cartridge cmd code */
#define G6_ALDRMSEL	0xCE	/* Fujitsu Diana-1 Autoloader Mode Select cmd */
#define G6_SELCART	0xCB	/* Fujitsu Diana-1 Select Cartridge cmd code */

#define FUJ_MODCD_MSK	3	/* Mode Code field mask */

#define CARTDATA_LEN	\
  (sizeof(struct scsi_sensecart_data))	/* Sense Cartridge data buffer length */

#define FUJ_AUTOMODE	1	/* Autoloader Auto Mode */
#define FUJ_SYSMODE	2	/* Autoloader System Mode */

/****************  END of Fujitsu Diana-1 parameters  ***********************/

/**************  START of Fujitsu Diana-2 and -3 parameters  ****************/
#define FUJ_SETSYS	1	/* fujd2d3mode(): set System Mode */
#define FUJ_SETORI	2	/* fujd2d3mode(): set original mode */

#define MS_HDR_LEN	4	/* Mode Page header length */

/* Medium Changer Vendor Unique Params Mode Page */
#define FUJMC_VUNIQ_PNO	0		/* V.U. Params. Mode Page */
#define FD2MC_MSP00_LEN	(MS_HDR_LEN+12)	/* V.U. Mode Page Length (Diana-2) */
#define FD3MC_MSP00_LEN	(MS_HDR_LEN+16)	/* V.U. Mode Page Length (Diana-3) */
#define FUJ_MODCD_OFF	(MS_HDR_LEN+2)	/* Mode Code byte offset */
#define FUJ_HLTLD_MSK	0x10		/* HltLd bit mask */
#define FUJ_MODCD(x)	(((x)[FUJ_MODCD_OFF]) & FUJ_MODCD_MSK)
/****************  END of Fujitsu Diana-2 and -3 parameters  ****************/

struct inqdata {
	char	vendor[9];	/* vendor ID string (w/NULL) */
	char	prodid[17];	/* product ID string (w/NULL) */
	char	revision[5];	/* revision info string (w/NULL) */
};

int debug;

int checkspecial(void);
void errstring(struct devinfo *devi);
struct inqdata *inquiry(struct devinfo *devi);
void dump_cmd(unchar *cmd, int c);
void dump_sense(unchar *sense);
void fillg5cmd(struct dsreq *dsp, unchar *cmd, unchar b0, unchar b1,
		 unchar b2, unchar b3, unchar b4, unchar b5, unchar b6,
		 unchar b7, unchar b8, unchar b9, unchar b10, unchar b11);
void fillg6cmd(struct dsreq *dsp, unchar *cmd, unchar b0, unchar b1,
		 unchar b2, unchar b3, unchar b4, unchar b5, unchar b6,
		 unchar b7, unchar b8, unchar b9);
int check_ready(struct devinfo *devi);
static int fujd1aldrchk(void);
static int fujd1mvmed(int magpos);
static int fujd2d3mode(int modereq);

#ifndef SCSI_INQUIRY_LEN
#define SCSI_INQUIRY_LEN        32      /* max length of SCSI inquiry data */
#endif

/*
 * SCSI Devce Capabilities Page
 *
 * Element types:  "DT" -> Data Transfer    "ST" -> STorage
 *		   "IE" -> Import Export    "MT" -> Medium Transport
 *
 * The StorXX fields tell whether a tape may be "stored" even temporarily
 * in the associated element type.  A value of 0 says that the indicated
 * element type will only hold the medium long enough to do an operation,
 * eg: StorMT==0 implies that the robot cannot "store" a tape.
 *
 * The XXtoYY fields tell whether the indicated transfer is supported by
 * the "move medium" command.  A 1 bit says that transfers between those
 * types is supported.
 *
 * The XXexYY fields tell whether the indicated transfer is supported by
 * the "exhange medium" command.  A 1 bit says that exhanges between those
 * types is supported.
 */
struct scsi_devcap_page {
	unchar	pad00[4];		/* mode page control header */
	unchar	PageSave:1, pad01:1, pagecode:6;
	unchar	length;
	unchar	pad10:4, StorDT:1, StorIE:1, StorST:1, StorMT:1;
	unchar	pad03;
	unchar	pad20:4, MTtoDT:1, MTtoIE:1, MTtoST:1, MTtoMT:1;
	unchar	pad21:4, STtoDT:1, STtoIE:1, STtoST:1, STtoMT:1;
	unchar	pad22:4, IEtoDT:1, IEtoIE:1, IEtoST:1, IEtoMT:1;
	unchar	pad23:4, DTtoDT:1, DTtoIE:1, DTtoST:1, DTtoMT:1;
	unchar	pad04[4];
	unchar	pad30:4, MTexDT:1, MTexIE:1, MTexST:1, MTexMT:1;
	unchar	pad31:4, STexDT:1, STexIE:1, STexST:1, STexMT:1;
	unchar	pad32:4, IEexDT:1, IEexIE:1, IEexST:1, IEexMT:1;
	unchar	pad33:4, DTexDT:1, DTexIE:1, DTexST:1, DTexMT:1;
};

/*
 * SCSI Element Address Assignment Page
 *
 * Element types:  "DT" -> Data Transfer    "ST" -> STorage
 *		   "IE" -> Import Export    "MT" -> Medium Transport
 *
 * The XXaddr fields give the starting element address for the given
 * element type.  Eg: tapes start at the element address found in DTaddr;
 * The XXcount fields give the number of element addresses for the given
 * element type.  Eg: there are DTcount tape drives;
 */
struct scsi_elemaddr_page {
	unchar	pad00[4];	/* mode page control header */
	unchar	PageSave:1, pad01:1, pagecode:6;
	unchar	length;
	unchar	MTaddr[2];	/* Medium Transports (ie: robots) */
	unchar	MTcount[2];
	unchar	STaddr[2];	/* Storage Elements (ie: slots) */
	unchar	STcount[2];
	unchar	IEaddr[2];	/* Import/Export Elements (ie: ports) */
	unchar	IEcount[2];
	unchar	DTaddr[2];	/* Data Transfer Elements (ie: drives) */
	unchar	DTcount[2];
	unchar	pad[2];
};


/*
 * SCSI Read Element Status definitions.
 *
 * Overall header for returned information
 */
struct scsi_rdelem {
	unchar	first[2];	/* first element addr reported on */
	unchar	number[2];	/* number of elements reported on */
	unchar	pad2;
	unchar	bytecount[3];	/* total bytes in the returned message */
};
/*
 * One "page" of descriptions, all of which are of the same type.
 */
struct scsi_rdelem_page {
	unchar	pad1;
	unchar	pvoltag:1, avoltag:1, pad2:6;	/* volume tag information */
	unchar	desclen[2];	/* number bytes in each description */
	unchar	pad3;
	unchar	bytecount[3];	/* number bytes in whole page */
};
/*
 * A description of a single element.
 */
struct scsi_rdelem_desc {
	unchar	addr[2];	/* address of the element being reported on */
	unchar	pad1:5, except:1, pad2:1, full:1;	/* error/full status */
	unchar	pad3[6];
	unchar	svalid:1, pad4:7;	/* source field valid or not */
	unchar	source[2];		/* where element was last */
	unchar	voltag[36];		/* volume tag info */
};

/*
 * Fujitsu Diana-1 data returned by vendor unique SENSE CARTRIDGE command
 */
struct scsi_sensecart_data {
	unchar	modecode;	/* Mode Code field; CTGLD,MGZIN,INHEJT bits */
	unchar	magpos;		/* magazine position field */
	unchar	magsize;	/* magazine size field */
	unchar	unused;		/* unused byte */
	unchar	bitmap[2];	/* 10-bit bitmap for cartridges present */
} Senscart;

/*
 * Start the translation from SCSI (and /dev/scsi) error codes to human
 * readable error messages.  If "checkcond" is set, look the next table.
 */
struct mainerrs {
	int	dsret;		/* checked against /dev/scsi ds_ret field */
	int	checkcond;	/* look this up in other table */
	char	*message;	/* resulting error string */
} mainlist[] = {
	{ DSRT_SENSE,	1, "" },
	{ DSRT_DEVSCSI,	0, "general devscsi failure" },
	{ DSRT_MULT,	0, "request rejected" },
	{ DSRT_CANCEL,	0, "lower request cancelled" },
	{ DSRT_REVCODE,	0, "unsupported rev number" },
	{ DSRT_AGAIN,	0, "try again, recoverable bus error" },
	{ DSRT_HOST,	0, "general host failure" },
	{ DSRT_NOSEL,	0, "no unit responded to select" },
	{ DSRT_SHORT,	0, "incomplete xfer (not an error)" },
	{ DSRT_TIMEOUT,	0, "request idle longer than requested" },
	{ DSRT_LONG,	0, "target overran data bounds" },
	{ DSRT_PROTO,	0, "misc. protocol failure" },
	{ DSRT_EBSY,	0, "busy dropped unexpectedly" },
	{ DSRT_REJECT,	0, "message reject" },
	{ DSRT_PARITY,	0, "parity error on SCSI bus" },
	{ DSRT_MEMORY,	0, "host memory error" },
	{ DSRT_CMDO,	0, "error during command phase" },
	{ DSRT_STAI,	0, "error during status phase" },
	{ DSRT_UNIMPL,	0, "not implemented" },
	{ DSRT_NOSENSE,	0, "cmd w/status, error getting sense" },
	{ DSRT_OK,	0, "complete xfer w/no error status" },
	{ 0, 0, "" }
};

/*
 * Finish the translation from SCSI (and /dev/scsi) error codes to human
 * readable error messages.  Only non-zero fields are meaningful
 * (ie: a zero field will match any value).  Note that as a result,
 * the order of the entries in this table is very important.
 */
struct checkerrs {
	int	dsstatus;	/* checked against /dev/scsi ds_status field */
	int	sensekey;	/* checked against sense key from SCSI packet */
	int	addsense;	/* checked against SCSI additional sense code */
	int	addqual;	/* checked against SCSI add-sense qualifier */
	int	lookagain;	/* look at this error more closely */
	char	*message;	/* resulting error string */
} checklist[] = {
    { STA_BUSY,  0x02, 0x04, 0x00, 0, "logical unit not ready, cause not reportable" },
    { STA_BUSY,  0x02, 0x04, 0x01, 0, "unit in process of becoming ready" },
    { STA_BUSY,  0x02, 0x04, 0x03, 0, "manual intervention required" },
    { STA_BUSY,  0x02, 0x04, 0x05, 0, "door close required" },
    { STA_BUSY,  0x02, 0x08, 0x00, 0, "library unit communications failure" },
    { STA_BUSY,  0x02, 0x80, 0x07, 0, "system STOP button is pushed" },
    { STA_BUSY,  0x02, 0x80, 0x09, 0, "system OFFLINE button is pushed" },
    { STA_BUSY,  0x02, 0x80, 0x00, 0, "door close required" },
    { STA_BUSY,  0x02, 0x00, 0x00, 0, "device not ready" },

    { STA_CHECK, 0x01, 0x55, 0xbe, 0, "command completed with warning" },
    { STA_CHECK, 0x01, 0x80, 0x05, 0, "bar code failure" },
    { STA_CHECK, 0x01, 0x00, 0x00, 0, "unrecognized recovered error" },

    { STA_CHECK, 0x02, 0x04, 0x01, 0, "unit in process of becoming ready" },
    { STA_CHECK, 0x02, 0x04, 0x02, 0, "unit must first initiate element status" },
    { STA_CHECK, 0x02, 0x04, 0x03, 0, "manual intervention required" },
    { STA_CHECK, 0x02, 0x04, 0x05, 0, "door close required" },
    { STA_CHECK, 0x02, 0x04, 0x83, 0, "access door open" },
    { STA_CHECK, 0x02, 0x04, 0x88, 0, "dowload needed" },
    { STA_CHECK, 0x02, 0x04, 0x89, 0, "dowload checksum error" },
    { STA_CHECK, 0x02, 0x05, 0x00, 0, "LUN did not respond to selection" },
    { STA_CHECK, 0x02, 0x08, 0x00, 0, "library unit communications failure" },
    { STA_CHECK, 0x02, 0x53, 0x02, 0, "cartridge in tape drive was front loaded" },
    { STA_CHECK, 0x02, 0x53, 0xaa, 0, "tape drive is not ready to be loaded" },
    { STA_CHECK, 0x02, 0x55, 0x00, 0, "transaction queue is full" },
    { STA_CHECK, 0x02, 0x5a, 0xc0, 0, "close the cabinet required" },
    { STA_CHECK, 0x02, 0x80, 0x07, 0, "system STOP button is pushed" },
    { STA_CHECK, 0x02, 0x80, 0x09, 0, "system OFFLINE button is pushed" },
    { STA_CHECK, 0x02, 0x80, 0x00, 0, "door close required" },
    { STA_CHECK, 0x02, 0x00, 0x00, 0, "device not ready" },

    { STA_CHECK, 0x03, 0x30, 0x00, 0, "incompatible medium installed-attempting 36-track write over 18-track data away from BOT" },
    { STA_CHECK, 0x03, 0x53, 0x00, 0, "load failure occurred and buffered write data exists" },
    { STA_CHECK, 0x03, 0x00, 0x00, 0, "unrecognized medium error" },

    { STA_CHECK, 0x04, 0x15, 0x01, 0, "move error" },
    { STA_CHECK, 0x04, 0x40, 0x80, 0, "diagnostic failure" },
    { STA_CHECK, 0x04, 0x42, 0x00, 0, "power-on self test failure" },
    { STA_CHECK, 0x04, 0x44, 0x00, 0, "hardware error" },
    { STA_CHECK, 0x04, 0x55, 0x9a, 0, "robot is not responding" },
    { STA_CHECK, 0x04, 0x55, 0x9b, 0, "command not recognized" },
    { STA_CHECK, 0x04, 0x55, 0xa2, 0, "empty on get" },
    { STA_CHECK, 0x04, 0x55, 0xa3, 0, "full on put" },
    { STA_CHECK, 0x04, 0x55, 0xbc, 0, "communication failure" },
    { STA_CHECK, 0x04, 0x55, 0xbd, 0, "unable to excecute movment commands" },
    { STA_CHECK, 0x04, 0x55, 0xbf, 0, "mechanical failure" },
    { STA_CHECK, 0x04, 0x55, 0xc1, 0, "internal software database error" },
    { STA_CHECK, 0x04, 0x00, 0x00, 0, "unrecognized hardware error" },

    { STA_CHECK, 0x05, 0x15, 0x00, 0, "mechanical positioning error" },
    { STA_CHECK, 0x05, 0x1a, 0x00, 0, "invalid parameter list length" },
    { STA_CHECK, 0x05, 0x20, 0x00, 0, "invalid command operation code" },
    { STA_CHECK, 0x05, 0x21, 0x01, 0, "invalid element address" },
    { STA_CHECK, 0x05, 0x22, 0x80, 0, "drive is not online" },
    { STA_CHECK, 0x05, 0x22, 0x81, 0, "drive to library interface error" },
    { STA_CHECK, 0x05, 0x22, 0x00, 0, "unsupported command" },
    { STA_CHECK, 0x05, 0x24, 0x00, 1, "invalid field in CDB" },
    { STA_CHECK, 0x05, 0x25, 0x00, 1, "library unit not connected" },
    { STA_CHECK, 0x05, 0x26, 0x00, 1, "invalid field in parameter list" },
    { STA_CHECK, 0x05, 0x28, 0x01, 0, "import/export element accessed" },
    { STA_CHECK, 0x05, 0x28, 0x00, 0, "state transition, medium may have changed" },
    { STA_CHECK, 0x05, 0x29, 0x00, 0, "power on or bus/device reset" },
    { STA_CHECK, 0x05, 0x3a, 0x00, 0, "medium not present" },
    { STA_CHECK, 0x05, 0x3b, 0x0d, 0, "medium destination element full" },
    { STA_CHECK, 0x05, 0x3b, 0x0e, 0, "medium source element empty" },
    { STA_CHECK, 0x05, 0x3b, 0x83, 0, "tape drive door closed" },
    { STA_CHECK, 0x05, 0x3b, 0x84, 0, "tape drive door closed" },
    { STA_CHECK, 0x05, 0x3b, 0x8f, 0, "first destination element empty" },
    { STA_CHECK, 0x05, 0x3b, 0x90, 0, "second destination element full" },
    { STA_CHECK, 0x05, 0x3b, 0x91, 0, "second destination element empty" },
    { STA_CHECK, 0x05, 0x3b, 0x92, 0, "transport element full" },
    { STA_CHECK, 0x05, 0x3b, 0x93, 0, "element is not accessible" },
    { STA_CHECK, 0x05, 0x3b, 0x94, 0, "drive is full-operation cannot be performed" },
    { STA_CHECK, 0x05, 0x3b, 0x00, 0, "medium destination element full" },
    { STA_CHECK, 0x05, 0x3d, 0x00, 0, "invalid identify message in" },
    { STA_CHECK, 0x05, 0x40, 0x00, 0, "device self diagnostic failed" },
    { STA_CHECK, 0x05, 0x44, 0x00, 0, "internal jukebox failure" },
    { STA_CHECK, 0x05, 0x4e, 0x00, 0, "overlapped commands attempted" },
    { STA_CHECK, 0x05, 0x52, 0x00, 0, "cartridge fault" },
    { STA_CHECK, 0x05, 0x53, 0x02, 0, "medium removal prevented" },
    { STA_CHECK, 0x05, 0x53, 0x00, 0, "medium load/unload/eject failed" },
    { STA_CHECK, 0x05, 0x80, 0x22, 0, "unknown if element contains cartridge" },
    { STA_CHECK, 0x05, 0x80, 0x5c, 0, "no autoloader installed" },
    { STA_CHECK, 0x05, 0x80, 0x5d, 0, "no cartridge at requested position" },
    { STA_CHECK, 0x05, 0x90, 0xa0, 0, "medium changer not in System Mode" },
    { STA_CHECK, 0x05, 0x00, 0x00, 0, "unrecognized illegal request" },

    { STA_CHECK, 0x06, 0x04, 0x85, 0, "inlet/outlet operation occurred" },
    { STA_CHECK, 0x06, 0x28, 0x00, 0, "not-ready to ready transition" },
    { STA_CHECK, 0x06, 0x29, 0x00, 0, "power on or bus/device reset" },
    { STA_CHECK, 0x06, 0x2a, 0x01, 0, "mode parameters changed" },
    { STA_CHECK, 0x06, 0x2a, 0x80, 0, "online repair parameters" },
    { STA_CHECK, 0x06, 0x33, 0x00, 0, "tape length error" },
    { STA_CHECK, 0x06, 0x3f, 0x01, 0, "microcode has been changed" },
    { STA_CHECK, 0x06, 0x3f, 0x03, 0, "inquiry data has changed" },
    { STA_CHECK, 0x06, 0x80, 0x07, 0, "system STOP button is pushed" },
    { STA_CHECK, 0x06, 0x80, 0x09, 0, "system OFFLINE button is pushed" },
    { STA_CHECK, 0x06, 0x80, 0x00, 0, "door close required" },
    { STA_CHECK, 0x06, 0x88, 0x00, 0, "temperature too high" },
    { STA_CHECK, 0x06, 0x00, 0x00, 0, "unrecognized unit attention" },

    { STA_CHECK, 0x0b, 0x04, 0x00, 0, "aborted command" },
    { STA_CHECK, 0x0b, 0x55, 0xb9, 0, "put fail and restore to previous condition" },
    { STA_CHECK, 0x0b, 0x55, 0xba, 0, "put fail and fail to restore" },
    { STA_CHECK, 0x0b, 0x55, 0xbb, 0, "receive a scsi abort" },
    { STA_CHECK, 0x0b, 0x5a, 0x00, 0, "robot offline" },
    { STA_CHECK, 0x0b, 0x5a, 0x97, 0, "door(s) open" },
    { STA_CHECK, 0x0b, 0x80, 0x0d, 0, "cartrige only partially gripped" },
    { STA_CHECK, 0x0b, 0x80, 0x10, 0, "drive load failed, check alignment" },
    { STA_CHECK, 0x0b, 0x80, 0x21, 0, "inventory operation timeout" },
    { STA_CHECK, 0x0b, 0x80, 0x23, 0, "barcode reader communications failure" },
    { STA_CHECK, 0x0b, 0x80, 0x80, 0, "vertical light curtain blocked" },
    { STA_CHECK, 0x0b, 0x80, 0x81, 0, "light curtain test failure" },
    { STA_CHECK, 0x0b, 0x81, 0x00, 0, "gripper failure" },
    { STA_CHECK, 0x0b, 0x84, 0x00, 0, "vertical movement failure" },
    { STA_CHECK, 0x0b, 0x86, 0x00, 0, "carousel movement failure" },
    { STA_CHECK, 0x0b, 0x8b, 0x00, 0, "pass-through failure" },
    { STA_CHECK, 0x0b, 0x8c, 0x00, 0, "inlet/outlet port failure" },
    { STA_CHECK, 0x0b, 0x00, 0x00, 0, "unrecognized aborted command" },

    { STA_CHECK, 0x0d, 0x00, 0x02, 0, "end-of-medium detected" },
    { 0, 0, 0, 0, 0, "" }
};

/*
 * Initialize the driver.
 */
int
setup(char *devname)
{
	if ((devi.dsp = dsopen(devname, O_RDWR)) == NULL) {
		fprintf(stderr, "unable to open scsi device %s\n", devname);
		return(1);
	}

	/* Check for ready removed - Bug 505860 */

	return(0);
}

/*
 * Clean up after ourselves.
 */
void
cleanup(void)
{
	dsclose(devi.dsp);
}

/*
 * List the devices that this driver will support
 */
struct jb_specialcase {
	char	*vendor;
	char	*product;
	int	prefix;
	int	type;
} jb_devs[] = {
	{"",		"DLT2700",	0, JBTYPE_DECDLT27}, /* DLT2700(xt) */
	{"",		"DLT2500",	0, JBTYPE_DECDLT25}, /* DLT2500(xt) */
	{"",		"DLT4700",	0, JBTYPE_DECDLT27}, /* DLT4700 */
	{"",		"DLT4500",	0, JBTYPE_DECDLT25}, /* DLT4500 */
	{"",		"DLT7700",	0, JBTYPE_DECDLT27}, /* DLT7700 */
	{"",		"DLT7500",	0, JBTYPE_DECDLT25}, /* DLT7500 */
	{"EXABYTE",	"EXB-10",	6, JBTYPE_EXA10I},   /* 10i or 10e */
	{"IBM",	        "03590",	0, JBTYPE_NTP},      /* NTP */
	{"FUJITSU",     "M101",		4, JBTYPE_DIANA1},   /* Diana-1 */
	{"FUJITSU",     "M2483",	5, JBTYPE_DIANA2},   /* Diana-2 */
	{"FUJITSU",     "M2488",	5, JBTYPE_DIANA3},   /* Diana-3 */
	{"IBM",	        "03570",	0, JBTYPE_MGSTRMP},  /* IBM 3570 */
        {"AMPEX", 	"DST410", 	0, JBTYPE_DST410},   /* DST 41x */
        {"AMPEX", 	"DST810", 	0, JBTYPE_DST810},   /* DST 81x */
        {"HP", 		"C6280-7000", 	0, JBTYPE_RIALTO},   /* HP Rialto */
	{NULL,		NULL,		0, 0}
};

/*
 * Inquire of the jukebox whether it requires special attention.
 */
int
checkspecial(void)
{
	struct jb_specialcase *special;
	struct inqdata *inqinfo;

	/*
	 * Get the device type
	 */
	inqinfo = inquiry(&devi);
	if (inqinfo == NULL) {
		errstring(&devi);
		return(-1);
	}

	/*
	 * Determine if this driver must special case this device.
	 */
	for (special = &jb_devs[0]; special->vendor; special++) { 
		if ((strlen(special->vendor) > 0) &&
		    (strncmp(inqinfo->vendor,
			     special->vendor,
			     sizeof(inqinfo->vendor)) != 0))
			continue;
		if (special->prefix) {
			if (bcmp(inqinfo->prodid,
				    special->product,
				    special->prefix) == 0) {
				break;
			}
		} else if (strncmp(inqinfo->prodid,
				   special->product,
				   sizeof(inqinfo->prodid)) == 0) {
			break;
		}
	}
	if (special->vendor)
		devi.jbtype = special->type;
	else
		devi.jbtype = 0;
	return(0);
}

/*
 * Read the jukebox configuration and squirrel it away.
 */
int
printconfig(char *devname)
{
	struct scsi_elemaddr_page elemaddr;
	unchar *sense;
	int drivecnt, portcnt, slotcnt, robotcnt;

	/*
	 * Check for special circumstances.
	 */
	if (checkspecial() < 0)
		return(-1);

	/*
	 * Special case our two problem children.
	 */
	if (devi.jbtype == JBTYPE_DECDLT27) {
		/*
		 * In 5.2 we cannot talk to LUN1, but the DLT2700 stacker
		 * will not respond to mode select commands on LUN0.
		 * We simply fake all the info.
		 */
		drivecnt = 1;
		portcnt = 0;
		slotcnt = 7;
		robotcnt = 1;
	} else if (devi.jbtype == JBTYPE_DECDLT25) {
		/*
		 * In 5.2 we cannot talk to LUN1, but the DLT2700 stacker
		 * will not respond to mode select commands on LUN0.
		 * We simply fake all the info.
		 */
		drivecnt = 1;
		portcnt = 0;
		slotcnt = 5;
		robotcnt = 1;
	} else if (devi.jbtype == JBTYPE_EXA10I) {
		/*
		 * The Exabyte EXB-10i is a pain, special case it.
		 */
		drivecnt = 1;
		portcnt = 0;
		slotcnt = 10;
		robotcnt = 1;
	} else if (IS_DIANA1(devi.jbtype)) {
		/*
		 * stuff Fujitsu Diana-1 parameters; only magazine size varies
		 */
		drivecnt = 1;		/* Data Transfer (tape drive) count */
		portcnt = 1;		/* Import Element (port) count */
		slotcnt = Senscart.magsize;	/* Storage (slot) count */
		robotcnt = 1;		/* Medium Transport (robot arm) count */
	} else {
		/*
		 * Do a MODE SENSE to get the element addresses,
		 * and the existence of import/export.
		 */
		check_ready(&devi);
		fillg0cmd(devi.dsp, (unchar *)CMDBUF(devi.dsp), G0_MSEN,
			0x08, 0x1d, 0, sizeof(elemaddr), 0);
		filldsreq(devi.dsp, (unchar *)&elemaddr, sizeof(elemaddr),
			DSRQ_READ|DSRQ_SENSE);
		if (doscsireq(getfd(devi.dsp), devi.dsp)) {
			sense = getsense(devi.dsp);
			if (debug) {
				dump_cmd((unchar *)getcmd(devi.dsp), 6);
				dump_sense(sense);
				printf("mode sense failed\n");
			}
			errstring(&devi);
			return(-1);
		}
		drivecnt = V2(elemaddr.DTcount);
		portcnt = V2(elemaddr.IEcount);
		slotcnt = V2(elemaddr.STcount);
		robotcnt = V2(elemaddr.MTcount);
	}

	/*
	 * Print the characteristics of that device.
	 */
	printf("%s: %d drive(s), %d slot(s), %d robot(s), %d port(s)\n",
		    devname, drivecnt, slotcnt, robotcnt, portcnt);

	return(0);
}

/*
 * Read device positions. Ie: find out what "addresses" to use.
 */
int
devpositions(void)
{
	struct scsi_elemaddr_page elemaddr;
	unchar *sense;

	/*
	 * Check for special circumstances.
	 */
	if (checkspecial() < 0)
		return(-1);

	/*
	 * Special case our two problem children.
	 */
	if (devi.jbtype == JBTYPE_DECDLT27 || devi.jbtype == JBTYPE_DECDLT25) {
		/*
		 * In 5.2 we cannot talk to LUN1, but the DLT2700 stacker
		 * will not respond to mode select commands on LUN0.
		 * We simply fake all the info.
		 */
		devi.DTaddr = 0x10-1;
		devi.IEaddr = 0;
		devi.STaddr = 0x100-1;
		devi.MTaddr = 0-1;
	} else if (devi.jbtype == JBTYPE_EXA10I) {
		/*
		 * The Exabyte 10-i is a pain, special case it.
		 */
		devi.DTaddr = 0-1;
		devi.IEaddr = 0;
		devi.STaddr = 1-1;
		devi.MTaddr = 11-1;
	} else if (IS_DIANA1(devi.jbtype)) {
		/*
		 * check if a Fujitsu Diana-1 has an autoloader attached
		 */
		if (fujd1aldrchk())
		    /*
		     * exit program here since this routine's return value
		     * is ignored and this drive does not have an autoloader
		     */
			exit(1);
		devi.DTaddr = 0; /* Data Transfer address (ie: tape drive) */
		devi.IEaddr = 0; /* Import Element address (ie: port) */
		devi.STaddr = 0; /* Storage address (ie: slot) */
		devi.MTaddr = 0; /* Medium Transport address (ie: robot arm) */
	} else {
		/*
		 * Do a MODE SENSE to get the element addresses,
		 * and the existence of import/export.
		 */
		check_ready(&devi);
		fillg0cmd(devi.dsp, (unchar *)CMDBUF(devi.dsp), G0_MSEN, 0x08,
			0x1d, 0, sizeof(elemaddr), 0);
		filldsreq(devi.dsp, (unchar *)&elemaddr, sizeof(elemaddr),
			DSRQ_READ|DSRQ_SENSE);
		if (doscsireq(getfd(devi.dsp), devi.dsp)) {
			sense = getsense(devi.dsp);
			if (debug) {
				dump_cmd((unchar *)getcmd(devi.dsp), 6);
				dump_sense(sense);
				printf("mode sense failed\n");
			}
			errstring(&devi);
			return(-1);
		}
		devi.DTaddr = V2(elemaddr.DTaddr)-1;
		devi.IEaddr = V2(elemaddr.IEaddr)-1;
		devi.STaddr = V2(elemaddr.STaddr)-1;
		devi.MTaddr = V2(elemaddr.MTaddr)-1;
	}
	if (debug && !IS_DIANA1(devi.jbtype)) {
		printf("drive(s) base address: 0x%04x\n", devi.DTaddr+1);
		printf("slot(s)  base address: 0x%04x\n", devi.STaddr+1);
		printf("robot(s) base address: 0x%04x\n", devi.MTaddr+1);
		printf("port(s)  base address: 0x%04x\n", devi.IEaddr+1);
	}
	return(0);
}

/*
 * Simple move of media from one place to another.
 */
int
movemedia(int src, int srctype, int dst, int dsttype)
{
	unchar *sense;
	int from, to, via, trans;

    /* disallow any Diana-1 or -2 request for slot-to-slot movement */
	if ((IS_DIANA1(devi.jbtype) || IS_DIANA2(devi.jbtype))
	 && srctype == TYPE_SLOT && dsttype == TYPE_SLOT)
	{
		printf("ERROR: This autoloader cannot perform bin-to-bin movement.\n");
		return(-1);
	}

    /* perform Fujitsu Diana-1 autoloader operation; pass the magazine bin no.
     * as the argument, i.e., the "load from" or "unload to" slot number */
	if (IS_DIANA1(devi.jbtype))
		return(fujd1mvmed(srctype == TYPE_SLOT ? src : dst));

    /* handle Fujitsu Diana-2 autoloader unload operation limitation */
	if (IS_DIANA2(devi.jbtype)
	 && srctype == TYPE_DRIVE && dsttype == TYPE_SLOT)
	{
		dst = -devi.STaddr;	/* force dst used in command to be 0 */
		printf("NOTE: Fujitsu M2483 request handled as unload to original medium slot.\n");
	}

	from = src;
	to = dst;
	via = 0;
	switch (srctype) {
	case TYPE_DRIVE:	src += devi.DTaddr;	break;
	case TYPE_SLOT:		src += devi.STaddr;	break;
	case TYPE_ROBOT:	src += devi.MTaddr;	break;
	case TYPE_PORT:		src += devi.IEaddr;	break;
	}
	switch (dsttype) {
	case TYPE_DRIVE:	dst += devi.DTaddr;	break;
	case TYPE_SLOT:		dst += devi.STaddr;	break;
	case TYPE_ROBOT:	dst += devi.MTaddr;	break;
	case TYPE_PORT:		dst += devi.IEaddr;	break;
	}
	trans = devi.MTaddr+1;

	if (debug)
		printf("src = %d (0x%02x), dst = %d (0x%02x), trans = %d (0x%02x)\n",
			    from, src, to, dst, via, trans);

	check_ready(&devi);
	(void)fujd2d3mode(FUJ_SETSYS);	/* if Fujitsu, set System Mode */
	fillg5cmd(devi.dsp, (unchar *)CMDBUF(devi.dsp), SCSI_MVMED, 0,
		B2(trans), B2(src), B2(dst), 0, 0, 0, 0);
	filldsreq(devi.dsp, 0, 0, DSRQ_READ|DSRQ_SENSE);
	TIME(devi.dsp) = 5*60*1000;	/* set 5-minute timeout */
	if (doscsireq(getfd(devi.dsp), devi.dsp)) {
		sense = getsense(devi.dsp);
		if (debug) {
			dump_cmd((unchar *)getcmd(devi.dsp), 12);
			dump_sense(sense);
			printf("tape move failed\n");
		}
		errstring(&devi);
		(void)fujd2d3mode(FUJ_SETORI); /* if Fujitsu, set orig. mode */
		return(-1);
	}
	(void)fujd2d3mode(FUJ_SETORI);	/* if Fujitsu, set original mode */
	return(0);
}

/*
 * Get Error String - return a descriptive string for the last error
 * that occurred in the jukebox.
 */
void
errstring(struct devinfo *devi)
{
	struct mainerrs *mainp;
	struct checkerrs *checkp;
	int ret, status, sense, addsense, addqual;

	for (mainp = mainlist; *mainp->message; mainp++) {
		ret = RET(devi->dsp);
		if (mainp->dsret && (mainp->dsret != ret))
			continue;
		if (mainp->checkcond)
			break;
		printf("%s\n", mainp->message);
		return;
	}
	for (checkp = checklist; *checkp->message; checkp++) {
		status = STATUS(devi->dsp);
		sense = SENSEKEY(SENSEBUF(devi->dsp));
		addsense = ADDSENSECODE(SENSEBUF(devi->dsp));
		addqual = ADDSENSEQUAL(SENSEBUF(devi->dsp));

		if (checkp->dsstatus && (checkp->dsstatus != status))
			continue;
		if (checkp->sensekey && (checkp->sensekey != sense))
			continue;
		if (checkp->addsense && (checkp->addsense != addsense))
			continue;
		if (checkp->addqual && (checkp->addqual != addqual))
			continue;
		if (checkp->lookagain)
			break;
		printf("%s (0x%02x%02x)\n", checkp->message, addsense, addqual);
		return;
	}

	/*
	 * It is assumed that only the one entry that has this field set,
	 * and that only a move-medium command will get this error.
	 */
	if (checkp->lookagain && (CMDBUF(devi->dsp)[0] != SCSI_MVMED)) {
		switch (FIELDPTR(SENSEBUF(devi->dsp))) {
		case 0x02:
			printf("invalid transport address\n");
			break;
		case 0x04:
		case 0x06:
			printf("invalid source or destination address\n");
			break;
		default:
			printf("command not supported\n");
			break;
		}
	} else {
		printf("%s (0x%02x%02x)\n", checkp->message, addsense, addqual);
	}
}

/*
 * Do a SCSI inquiry command to find out what type of device we are
 * talking to.
 */
struct inqdata *
inquiry(struct devinfo *devi)
{
	unchar inqbuf[SCSI_INQUIRY_LEN];
	static struct inqdata idata;
	char *ch;
	int i;

	/*
	 * Do the requested inquiry command.
	 */
	if (inquiry12(devi->dsp, (caddr_t)inqbuf, sizeof(inqbuf), 0) != 0) {
		fprintf(stderr, "inquiry failure\n");
		errstring(devi);
		return(NULL);
	}
	if (DATASENT(devi->dsp) < 32) {
		fprintf(stderr, "Not enough inquiry data returned\n");
		errstring(devi);
		return(NULL);
	}
	if (debug) {
		printf("Peripheral Device Type=%d  \"%-12.8s\" \"%.16s\" \"%.4s\"\n",
			     DATABUF(devi->dsp)[0] & 0x7F,
			     &DATABUF(devi->dsp)[8],
			     &DATABUF(devi->dsp)[16],
			     &DATABUF(devi->dsp)[32]);
	}

	/*
	 * Copy the requested data back to the caller.
	 */
	strncpy(idata.vendor, &DATABUF(devi->dsp)[8], 8);
	strncpy(idata.prodid, &DATABUF(devi->dsp)[16], 16);
	strncpy(idata.revision, &DATABUF(devi->dsp)[32], 4);
	for (ch = &idata.vendor[7], i = 7; (*ch == ' ') && (i >= 0);
			ch--, i--)
		*ch = 0;
	for (ch = &idata.prodid[15], i = 15; (*ch == ' ') && (i >= 0);
			ch--, i--)
		*ch = 0;
	for (ch = &idata.revision[4], i = 15; (*ch == ' ') && (i >= 0);
			ch--, i--)
		*ch = 0;

	return(&idata);
}

#ifdef NOT_YET
/*
 * Do any commands required to accept a tape through the "inlet" or
 * to eject any media through the "outlet".  This isn't supported
 * by SGI's current jukeboxes (DEC DLT or Exabyte 10I), so....
 */
int
inlet(struct devinfo *devi, struct sji_init_inlt_cmd *cmd)
{
	errstring(&devi);
	return(-1);
}

/*
 * Force the jukebox to (re)examine the tapes in the slots.
 */
int
initelems(struct devinfo *devi, struct sji_init_elem_cmd *cmd)
{
	unchar *sense;

	check_ready(&devi);
	fillg0cmd(devi->dsp, (unchar *)CMDBUF(devi->dsp), SCSI_IELEM,
		0, 0, 0, 0, 0);
	filldsreq(devi->dsp, 0, 0, DSRQ_READ|DSRQ_SENSE);
	TIME(devi->dsp) = 30*60*1000;	/* set 30-minute timeout for Odetics */
	if (doscsireq(getfd(devi->dsp), devi->dsp)) {
		sense = getsense(devi->dsp);
		if (debug) {
			dump_cmd((unchar *)getcmd(devi->dsp), 6);
			dump_sense(sense);
			printf("init status failed\n");
		}
		errstring(&devi);
		return(-1);
	}
	return(0);
}

/*
 * Return information on a set of slots (ie: is there a tape in it?).
 */
int
readelemtags(struct devinfo *devi, caddr_t cmd, int rtag)
{
	unchar *sense;
	int code, low, cnt, type, size, i, j;
	char *buffer, *databuf;
	struct scsi_rdelem *rdep;
	struct scsi_rdelem_page *page;
	struct scsi_rdelem_desc *desc;	/* generic element description */
	struct sji_rd_tag_cmd *tcmd;
	struct sji_rd_tag_data *tdata;
	struct sji_elem_tag *telem;
	struct sji_rd_elem_cmd *ecmd;
	struct sji_rd_elem_data *edata;
	struct sji_elem_data *eelem;

	if (rtag) {
		tcmd = (struct sji_rd_tag_cmd *)cmd;
		type = tcmd->elem_type;
		low = (int)tcmd->elem_low;
		cnt = (int)tcmd->num_elem;
		size = tcmd->buf_size;
	} else {
		ecmd = (struct sji_rd_elem_cmd *)cmd;
		type = ecmd->elem_type;
		low = (int)ecmd->elem_low;
		cnt = (int)ecmd->elem_hi - low + 1;
		size = ecmd->buf_size;
	}

	/*
	 * SJI definitions are backwards from SCSI-2 definitions.
	 */
	switch (type) {
	case DRIVE_TYPE:    code = 0x4; low += devi.DTaddr;	break;
	case SLOT_TYPE:     code = 0x2; low += devi.STaddr;	break;
	case INL_OUTL_TYPE: code = 0x3; low += devi.IEaddr;	break;
	case TRNSPT_TYPE:   code = 0x1; low += devi.MTaddr;	break;
	default: errstring(&devi); return(-1);
	}

	/*
	 * Execute the SCSI command to actually read the element information.
	 */
	if ((buffer = malloc(size)) == NULL) {
		perror("malloc");
		exit(1);
	}
	check_ready(&devi);
	fillg5cmd(devi->dsp, (unchar *)CMDBUF(devi->dsp), SCSI_RELEM,
		0x00|code, B2(low), B2(cnt), 0, B3(size), 0, 0);
	filldsreq(devi->dsp, (unchar *)buffer, size, DSRQ_READ|DSRQ_SENSE);
	TIME(devi->dsp) = 4*60*1000;	/* set 4-minute timeout */
	if (doscsireq(getfd(devi->dsp), devi->dsp)) {
		sense = getsense(devi->dsp);
		if (debug) {
			dump_cmd((unchar *)getcmd(devi->dsp), 12);
			dump_sense(sense);
			printf("read element status failed\n");
		}
		errstring(&devi);
		free(buffer);
		return(-1);
	}

	/*
	 * Parse through the returned information, building a structure
	 * to return to the caller as we go.
	 */
	cnt = 0;
	rdep = (struct scsi_rdelem *)buffer;
	page = (struct scsi_rdelem_page *)&buffer[ sizeof(*rdep) ];
	if (rtag) {
		tdata = (struct sji_rd_tag_data *)tcmd->buf_addr;
		telem = &tdata->elem_tag_data[0];
	} else {
		edata = (struct sji_rd_elem_data *)ecmd->buf_addr;
		eelem = &edata->elem_data[0];
	}
	for (i = 0; i < V2(rdep->number); i += j) {
		desc = (struct scsi_rdelem_desc *)((char *)page+sizeof(*page));
		for (j = 0; j < V3(page->bytecount)/V2(page->desclen); j++) {
			if (rtag) {
				tag_work(&devi, (code==0x4), telem, desc, page);
				telem++;
			} else {
				elem_work(&devi, (code==0x4), eelem, desc);
				eelem++;
			}
			desc = (struct scsi_rdelem_desc *)((char *)desc +
							   V2(page->desclen));
		}
		page = (struct scsi_rdelem_page *)((char *)page +
						   V3(page->bytecount));
	}
	free(buffer);

	return(0);
}

static void
tag_work(struct devinfo *devi,
		int tape,
		struct sji_elem_tag *elem,
		struct scsi_rdelem_desc *desc,
		struct scsi_rdelem_page *page)
{
	if (tape && desc->except)
		elem->pres_val = 0;
	else
		elem->pres_val = 1;
	elem->med_pres = desc->full;

	/*
	 * Work around EXB-10i bug after opening the door
	 */
	if (!tape && devi->jbtype == JBTYPE_EXA10I)
		elem->med_pres = TRUE;
	/* end work-around */

	elem->med_side = 0;
	elem->tag_val = page->pvoltag;
	if (page->pvoltag)
		bcopy(desc->voltag, elem->elem_tag, sizeof(elem->elem_tag));
}

static void
elem_work(struct devinfo *devi,
		 int tape,
		 struct sji_elem_data *elem,
		 struct scsi_rdelem_desc *desc)
{
	if (tape && desc->except)
		elem->pres_val = 0;
	else
		elem->pres_val = 1;
	elem->med_pres = desc->full;
	elem->orig_val = desc->svalid;
	elem->med_side = 0;
	if (devi.jbtype == JBTYPE_DECDLT27 || devi.jbtype == JBTYPE_DECDLT25) {
		elem->med_origin = V2(desc->source);
		if ((elem->med_origin == devi->DTaddr-1) ||
		    (elem->med_origin == devi->DTaddr)) {
			elem->med_origin = 0;
			elem->med_origin_type = DRIVE_TYPE;
		} else {
			elem->med_origin -= devi->DTaddr-1;
			elem->med_origin_type = SLOT_TYPE;
		}
	} else {
		elem->med_origin = V2(desc->source);
		if (elem->med_origin == 0)
			elem->med_origin_type = DRIVE_TYPE;
		else
			elem->med_origin_type = SLOT_TYPE;
	}
}

/*
 * Allow media removal (unlock the front door)
 */
int
door(struct devinfo *devi, struct sji_door_open_cmd *cmd)
{
	unchar *sense;
	int i;

	check_ready(&devi);
	for (i = 0; i < 10; i++) {
		if (debug)
			printf("allowing media removal\n");
		fillg0cmd(devi->dsp, (unchar *)CMDBUF(devi->dsp), SCSI_DOOR,
			0, 0, 0, cmd->open ? 0x1 : 0x0, 0);
		filldsreq(devi->dsp, 0, 0, DSRQ_READ|DSRQ_SENSE);
		TIME(devi->dsp) = 2*60*1000;	/* set 2-minute timeout */
		if (doscsireq(getfd(devi->dsp), devi->dsp)) {
			sense = getsense(devi->dsp);
			if (debug) {
				dump_cmd((unchar *)getcmd(devi->dsp), 6);
				dump_sense(sense);
				printf("media removal allow failed\n");
			}
			sleep(10);
		} else {
			break;
		}
	}
	if (i >= 10) {
		errstring(&devi);
		return(-1);
	}
	return(0);
}
#endif /* NOT_YET */

/*
 * Dump out the SCSI command sent to the jukebox.
 */
void
dump_cmd(unchar *cmd, int c)
{
	int i;

	printf("cmd data:\n");
	for( i = 0 ; i < c ; i++ )
		printf("0x%02x ",*cmd++);
	printf("\n");
}

/*
 * Dump out the SCSI sense codes returned by the jukebox.
 */
void
dump_sense(unchar *sense)
{
	int i;

	printf("sense data:\n");
	for( i = 0 ; i < 19 ; i++ )
		printf("0x%02x ", *sense++);
	printf("\n");
}

/*
 * fillg5cmd - Fill a Group 5 (actually, any 12 byte) command buffer
 */
void
fillg5cmd(struct dsreq *dsp, unchar *cmd, unchar b0, unchar b1,
		 unchar b2, unchar b3, unchar b4, unchar b5, unchar b6,
		 unchar b7, unchar b8, unchar b9, unchar b10, unchar b11)
{
	unchar *c;

	c = cmd;
	*c++ = b0; *c++ = b1; *c++ = b2; *c++ = b3; *c++ = b4; *c++ = b5;
	*c++ = b6; *c++ = b7; *c++ = b8; *c++ = b9; *c++ = b10; *c++ = b11;
	CMDBUF(dsp) = (caddr_t)cmd;
	CMDLEN(dsp) = 12;
}

/*
 * fillg6cmd - Fill a Group 6 (actually, any 10 byte) command buffer
 */
void
fillg6cmd(struct dsreq *dsp, unchar *cmd, unchar b0, unchar b1,
		 unchar b2, unchar b3, unchar b4, unchar b5, unchar b6,
		 unchar b7, unchar b8, unchar b9)
{
	unchar *c;

	c = cmd;
	*c++ = b0; *c++ = b1; *c++ = b2; *c++ = b3; *c++ = b4; *c++ = b5;
	*c++ = b6; *c++ = b7; *c++ = b8; *c++ = b9;
	CMDBUF(dsp) = (caddr_t)cmd;
	CMDLEN(dsp) = 10;
}

/*
 * Delay as long as the device is reporting a "unit attention".
 */
int
check_ready(struct devinfo *devi)
{
	unchar *sense;
	int    rc;

	while ((rc = testunitready00(devi->dsp)) != 0) {
		sense = getsense(devi->dsp);
		if ((sense[2] & 0xf) != 6)
			break;
		if (debug)
			printf("Test ready fail, %s\n",
				     (RET(devi->dsp) == DSRT_NOSEL)
				     ? "Unable to select"
				     : "not ready");
	}
	return(rc);
}


/*
 * Fujitsu Diana-1 routine to check on the existence of an autoloader on the
 * selected drive.  This must be done by issuing a Sense Cartridge command
 * followed by an Autoloader Mode Select command.  The latter actually
 * determines whether there is an autoloader or not, but the former is
 * required so that the Mode Code setting is not altered by the latter.
 * In addition, the former (the Sense Cartridge command) returns the number
 * of bins in any magazine that may be present in the autoloader, for
 * display using the "-c" program option.
 */
static int
fujd1aldrchk(void)
{
	unchar *sense;

    /* clear any Unit Attention */
	(void)testunitready00(devi.dsp);

    /* issue vendor unique Sense Cartridge command (supported
       whether or not the drive is equipped with an autoloader) */
	fillg6cmd(devi.dsp, (unchar *)CMDBUF(devi.dsp), G6_SENSCART,
		0, 0, 0, 0, 0, 0, 0, CARTDATA_LEN, 0);
	filldsreq(devi.dsp, (unchar *)&Senscart, CARTDATA_LEN,
		DSRQ_READ|DSRQ_SENSE);
	if (doscsireq(getfd(devi.dsp), devi.dsp))
	{
		printf("Bad Fujitsu M1016/M1017 Sense Cartridge command\n");
		sense = getsense(devi.dsp);
		if (debug)
		{
			dump_cmd((unchar *)getcmd(devi.dsp), 10);
			dump_sense(sense);
			printf("Fujitsu M1016/M1017 Sense Cartridge failed\n");
		}
		errstring(&devi);
		return(-1);
	}

	if (debug)
		printf("sense cartridge data: 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X\n",
		  Senscart.modecode, Senscart.magpos, Senscart.magsize,
		  Senscart.unused, Senscart.bitmap[0], Senscart.bitmap[1]);

    /* issue vendor unique Autoloader Mode Select command (as this command only
       completes successfully if the drive is equipped with an autoloader, this
       command can determine whether one exists on the current drive) */
	fillg6cmd(devi.dsp, (unchar *)CMDBUF(devi.dsp), G6_ALDRMSEL,
		0, 0, 0, 0, 0, 0, 0, Senscart.modecode & FUJ_MODCD_MSK, 0);
	filldsreq(devi.dsp, 0, 0, DSRQ_READ|DSRQ_SENSE);
	if (doscsireq(getfd(devi.dsp), devi.dsp))
	{
		sense = getsense(devi.dsp);

	    /* determine if failure due to no autoloader */
		if (SENSEKEY(sense) == SC_ILLEGALREQ
		 && ADDSENSECODE(sense) == 0x80 && ADDSENSEQUAL(sense) == 0x5C)
		{
			printf("ERROR: Fujitsu M1016/M1017 drive lacks autoloader.\n");
			return(-1);
		}
		printf("Bad Fujitsu M1016/M1017 Autoloader Mode Select command\n");
		if (debug)
		{
			dump_cmd((unchar *)getcmd(devi.dsp), 10);
			dump_sense(sense);
			printf("Fujitsu M1016/M1017 Autoloader Mode Select failed\n");
		}
		errstring(&devi);
		return(-1);
	}

    /* clear any Unit Attention */
	(void)testunitready00(devi.dsp);

	return(0);
}

/*
 * Fujitsu Diana-1 routine to move media using the vendor unique Select
 * Cartridge command.  Note that this command only uses one argument, which
 * is the "magazine position".  As this is the bin number containing a tape
 * to be loaded into the drive, the tape unload operation is implicit.  This
 * also means that a load or unload will be done, as appropriate for the
 * current state of the bin referenced.  Hence, the same operation for a bin
 * twice in a row could result in a load and then an unload, or vice versa!
 */
static int
fujd1mvmed(int magpos)
{
	unchar *sense;

	printf("NOTE: Fujitsu M1016/M1017 load/unload handled identically - see man page.\n");

    /* issue vendor unique Select Cartridge command (supported
       whether or not the drive is equipped with an autoloader) */
	fillg6cmd(devi.dsp, (unchar *)CMDBUF(devi.dsp), G6_SELCART,
		0, 0, 0, 0, 0, 0, 0, (unchar)magpos, 0);
	filldsreq(devi.dsp, 0, 0, DSRQ_READ|DSRQ_SENSE);
	TIME(devi.dsp) = 5*60*1000;	/* set 3-minute timeout */
	if (doscsireq(getfd(devi.dsp), devi.dsp))
	{
		printf("Bad Fujitsu M1016/M1017 Select Cartridge command\n");
		sense = getsense(devi.dsp);
		if (debug)
		{
			dump_cmd((unchar *)getcmd(devi.dsp), 10);
			dump_sense(sense);
			printf("Fujitsu M1016/M1017 Select Cartridge failed\n");
		}
		errstring(&devi);
		return(-1);
	}

	return(0);
}

/*
 * Fujitsu Diana-2 and -3 routine to turn System Mode on, and to return the
 * drive to its original mode.  System Mode is the required mode for the
 * SCSI MOVE MEDIUM command to be able to work.  Auto Mode is the default
 * mode upon power-on.  In Auto mode, the unloading of a cartridge will
 * cause the automatic loading of the next cartridge in the magazine.  
 *
 * This routine is called both before and after a MOVE MEDIUM command is
 * executed to turn System Mode on, and to return the drive to its original
 * mode, respectively.  Returning to the original mode, if it is Auto Mode,
 * allows the autochanger to progress through a magazine's tapes if the user
 * unloads a tape using 'mt offline/unload' or ioctl(MTOFFL/MTUNLOAD).
 */
static int
fujd2d3mode(int modereq)
{
	static char orimode;		/* original autoloader mode storage */
	unchar *sense;
	char mdata[FD3MC_MSP00_LEN];	/* mode data buffer */
	int msp00_len =
	    IS_DIANA2(devi.jbtype) ? FD2MC_MSP00_LEN : FD3MC_MSP00_LEN;

    /* this code only applicable to Fujitsu Diana-2 and -3 */
	if (!IS_DIANA2(devi.jbtype) && !IS_DIANA3(devi.jbtype))
		return(0);

    /* get Medium Changer Vendor Unique Parameters Mode Page with Mode Sense */
	fillg0cmd(devi.dsp, (unchar *)CMDBUF(devi.dsp), G0_MSEN,
		0, FUJMC_VUNIQ_PNO, 0, msp00_len, 0);
	filldsreq(devi.dsp, (unchar *)mdata, msp00_len, DSRQ_READ|DSRQ_SENSE);
	if (doscsireq(getfd(devi.dsp), devi.dsp))
	{
		printf("Bad Mode Code Mode Sense\n");
		sense = getsense(devi.dsp);
		if (debug)
		{
			dump_cmd((unchar *)getcmd(devi.dsp), 6);
			dump_sense(sense);
			printf("mode sense failed\n");
		}
		errstring(&devi);
		return(-1);
	}

    /* save current mode if setting System Mode */
	if (modereq == FUJ_SETSYS)
		orimode = FUJ_MODCD(mdata);

    /* stuff new Mode Code into Mode Page */
	mdata[0] = 0;				/* clear data length */
	mdata[MS_HDR_LEN+0] &= 0x7F;		/* clear PS bit */
	mdata[FUJ_MODCD_OFF] &= 0xFC;		/* clear Mode Code field */
	mdata[FUJ_MODCD_OFF] |= (modereq == FUJ_SETSYS) ? FUJ_SYSMODE : orimode;

    /* prevent Diana-2 autoloader from loading next tape in case doing unload */
	if (IS_DIANA2(devi.jbtype))
		mdata[FUJ_MODCD_OFF] |= FUJ_HLTLD_MSK;	/* set HltLd bit */
	if (debug)
  	   printf("HltLd bit=%s\n",
                  (mdata[FUJ_MODCD_OFF] & 0x10) ? "ON" : "OFF");

    /* Set new Mode Code with Mode Select */
	fillg0cmd(devi.dsp, (unchar *)CMDBUF(devi.dsp), G0_MSEL,
		0, 0, 0, msp00_len, 0);
	filldsreq(devi.dsp, (unchar *)mdata, msp00_len, DSRQ_WRITE|DSRQ_SENSE);
	if (doscsireq(getfd(devi.dsp), devi.dsp))
	{
		printf("Bad Mode Code Mode Select\n");
		sense = getsense(devi.dsp);
		if (debug)
		{
			dump_cmd((unchar *)getcmd(devi.dsp), 6);
			dump_sense(sense);
			printf("mode select failed\n");
		}
		errstring(&devi);
		return(-1);
	}

    /* clear Unit Attention caused by the Mode Select */
	(void)testunitready00(devi.dsp);

	return(0);
}
