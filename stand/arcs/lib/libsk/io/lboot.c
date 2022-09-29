/* this file contains variables which are in the master.d files
	for drivers used by both the kernel and standalone */

#ident	"saio/io/lboot: $Revision $"

#include <sys/param.h>
#include <sys/scsi.h>
#include <sys/scsidev.h>
#include <sys/buf.h>
#include <sys/invent.h>
#include <sys/mtio.h>
#include <sys/tpsc.h>

int showconfig = 0;	/* should initialize from Debug someday */

scsidev_t *scsidev;
int scsimajor[SC_MAXADAP] = {0,1,2,3,4,5,6,7}; /* MUST match scsi_ctlr macro,
	also the fakeout macros that turn the io struct values into
	dev_t's in dksc.c and tpsc.c */

/*  Number of scsi channels present.  Must be initialized early.  All arcs
 * prom machines can have multiple scsi controllers, although the GIO scsi
 * card may never ship.
 */
int scsicnt;
int scsiburstdma;	/* set at startup if system supports burst mode dma */

/*	bitmap of target ID's for which synchronous SCSI mode may be
	negotiated.  If unit doesn't support this mode, and the device
	obeys the protocols, then it is OK to enable it here;  If
	device doesn't follow protocols, then do not set the bit
	(devices that don't follow the protocols typically results in
	SCSI bus timeouts and resets.  At this time, only disks are
	considered candidates.  Set the 0x80 bit for ID 7, 0x40 for ID
	6 ... 0x2 for ID 1.   May be zeroed for CPUs/IO boards that
	don't support sync. SCSI, (such as 4D80 and 4D70) this variable 
	will be zeroed at boot time. 
*/
/* scsimajor was char, is int for biendian */
uint scsi_syncenable[SC_MAXADAP];
/* For IP20 and higher initialize as
 * bss section in hpc_scsi.c
 */
/* 	this is the period to use for sync negotiations, one per adapter.
	The value is in 4 nanosecond increments, so 25 is 100 ns or
	10 Mbytes/sec (fast SCSI).  This is only what we offer to the
	target, the value used will match what the target negotiates.
	For drives not supporting fast SCSI, this will be at least 50
	(200 ns, or 5 MB/sec max).  It doesn't affect throughput much
	for any (currently available) single drive, but becomes important
	when multiple targets are on the SCSI bus (particularly for
	logical volumes).  You may wish to increase this value if you
	have marginal cabling, so that  you can still use sync scsi,
	but not at the maximum rate, since cabling problems are usually
	much more obvious at higher data rates.  Only the 93B SCSI chip
	currently supports fast SCSI, so if the value is less than 50
	and the chip is a 93 or 93A, a value of 50 is used.
	For the 93B, legal fast values are 25 and 38 (other values are
	possible, but are 5 Mb/sec or less, but use the fast scsi timings).
	For SCSI 1 (not fast SCSI), legal values are 50, 75, 100, 125, 150,
	and 175.  Anything longer than 175 uses 175.  These values may be
	used on any of the 3 chips (93, 93A, and 93B).  
*/
u_char scsi_syncperiod[SC_MAXADAP] = {25, 25, 25, 25};

/*	If this is 0, the driver will not enable disconnects on the
	host adapter.  This is allows use of devices that either don't
	support disconnect, or that don't work correctly with
	disconnect enabled.  When 0, overall performance usually 
	suffers when more than one scsi device (particularly disks)
	is in use, and SCSI system disks may wait a LONG time while
	SCSI tapes are rewinding, etc.
*/
int scsi_enable_disconnect[SC_MAXADAP] = {0,0,0,0};

/* variables for tpsc driver */

/* Used to set tape type from the inquiry cmd.
 * This allows SGI and users to add new tape drives of known types without
 * re-compiling the driver.  The assumption is that the SCSI commands,
 * capabilities, and request sense info are constant within
 * a type.  This is rarely completely true, but is often close enough
 * to allow a drive to work.  Note that when no additional info (id_ailen
 * field) is returned on the inquiry, the driver sets the vendor id (id_vid)
 * to "CIPHER" and id_pid to "540S" before checking this table.
 *
 * See also sys/mtio.h for definitions of the MTCAN_* bits.
 *
 * Note that this does not quite allow arbitrary drives to be
 * connected, since interpretation of the request sense results,
 * and the modeselect that is done is based on the tp_type field.
 * That field must be set to match one of the known types, or a
 * drive must not appear in this table at all, in which case it is
 * treated as an unknown type, and the tpsc_generic structure (below)
 * is used.  However, if a drive is of an unknown type, but the inquiry
 * command indicates SCSI 2 compliance, the error codes and additional
 * sense info are decoded according to SCSI 2.
 *
 * Also note that a similar list is built into the PROMs and standalone
 * drivers, so changes to this are only effective for use under unix.
 *
 * This structure is defined in tpsc.h - read the comments there for
 * the meanings of fields, and their units, where applicable.
*/

struct tpsc_types tpsc_types[] = {
	{ CIPHER540, TPQIC24, 6, 4, "CIPHER", "540S",  1, (u_char *)"", {0, 0, 0, 0},
	  MTCAN_PREV|MTCAN_SPEOD, 20, 60, 18*60, 5*60, 512, 400*512 },

	/* the array works around a compiler warning about not enough
	 * room for the null.  This is really just "TANDBERG" */
	{ TANDBERG3660, TPQIC150, 8, 8, {'T','A','N','D','B','E','R','G'},
	  " TDC 3660",
	  /* set read/write thresholds and buffer size for better performance */
	  12,(u_char *) "\0\20\20\200\0\0\10\10\0\4", {0, 0, 0, 0},
	  MTCAN_BSF|MTCAN_BSR|MTCAN_PREV|MTCAN_SPEOD|MTCAN_CHKRDY|MTCAN_SEEK,
	  20, 60, 18*60, 5*60, 512, 400*512 },

	{ VIPER150, TPQIC150, 7, 9, "ARCHIVE", "VIPER 150", 0,(u_char *) 0, {0, 0, 0, 0},
	  MTCAN_BSF|MTCAN_BSR|MTCAN_PREV|MTCAN_SPEOD|MTCAN_CHKRDY|MTCAN_SEEK,
	  40, 60, 27*60, 5*60, 512, 400*512},

	{ VIPER60, TPQIC24, 7, 8, "ARCHIVE", "VIPER 60", 0,(u_char *) 0, {0, 0, 0, 0},
	  MTCAN_BSF|MTCAN_BSR|MTCAN_PREV|MTCAN_SPEOD|MTCAN_CHKRDY|MTCAN_SEEK,
	  40, 60, 27*60, 5*60, 512, 400*512},

	{ DATTAPE, TPDAT, 7, 6, "ARCHIVE", "Python", 0,(u_char *) 0, {0, 0, 0, 0},
	  MTCAN_BSF|MTCAN_BSR|MTCAN_APPEND|MTCAN_SETMK|MTCAN_PART|MTCAN_PREV|
	    MTCAN_SYNC|MTCAN_SPEOD|MTCAN_CHKRDY|MTCAN_VAR|MTCAN_SETSZ|
	    MTCAN_SILI|MTCAN_AUDIO|MTCAN_SEEK|MTCAN_CHTYPEANY,
	  /* minimum delay on i/o is 4 minutes, because when a retry is
	   * performed, the drive retries a number of times, and then
	   * rewinds to BOT, repositions, and tries again.  */
	  40, 4*60, 4*60, 5*60, 512, 512*512 },

	/* don't look for the "8200", so that when the Exabyte 8500 becomes
	 * available, it will work. 016 is even byte disconnect and parity
	 * check enabled.  Even though erases can take up to 2 hours, the
	 * rewtimeo is still fairly low.  The driver recognizes this special
	 * case and uses 2 hours plus rewtimeo.  Minimum delay is set somewhat
	 * high because of the long startup time after idle, and because
	 * FM's take a long time to write. */

	 /* NOTE: default blocksize of 512 for standalone,
	  * since that is all we support in standalone, and we want to
	  * allow people to make tapes even on 8mm that are bootable.
	  * as long as they make the tape with a blocksize of 512,
	  * it will work. */
        { EXABYTE8200, TP8MM_8200, 7, 8, "EXABYTE", "EXB-8200", 1,
		(u_char *)"\16", {0, 0, 0, 0},
	  MTCAN_BSF|MTCAN_BSR|MTCAN_PREV|MTCAN_CHKRDY|MTCAN_VAR|MTCAN_SETSZ|
	    MTCAN_SILI|MTCAN_CHTYPEANY,
	  80, 4*60, 25*60, 5*60, 512, 128*1024},

        /* There are only 2 densities, 8500 mode and 8200.  If someone make
         * bogus dev numbers using all 4 density bits, it will default back
         * to 8500 mode.  The inventory string sets the drive up exactly like
         * the 8200. */
        { EXABYTE8500, TP8MM_8500, 7, 8, "EXABYTE", "EXB-8500", 6, 
		(u_char *)"\40\4\16\0\200\7",
                {0x0, 0x14, 0x0, 0x0},
          MTCAN_BSF|MTCAN_BSR|MTCAN_PREV|MTCAN_CHKRDY|MTCAN_VAR|MTCAN_SETSZ|
            MTCAN_SILI|MTCAN_CHTYPEANY|MTCAN_SETDEN|MTCAN_SPEOD|MTCAN_SYNC|
                MTCAN_SEEK,
          80, 4*60, 25*60, 5*60, 1024, 128*1024},


	/* NOTE: xfrtimeo is for the 800 bpi (slowest transfer rate) */
	{ KENNEDY96X2, TP9TRACK, 7, 9, "KENNEDY", "96X2 TAPE", 0,(u_char *) 0,
	   {3, 6, 2, 1}, MTCAN_BSF|MTCAN_BSR|MTCAN_APPEND|MTCAN_LEOD |
	     MTCAN_CHKRDY|MTCAN_VAR| MTCAN_SETSZ|MTCAN_SETSP|MTCAN_SETDEN,
	  20, 60, 20*60, 5*60, 512, 60*512 }
};

/* numtypes doesn't include the generic entry */
int tpsc_numtypes = sizeof(tpsc_types)/sizeof(tpsc_types[0]);

/* this entry is used for all tapes not matching any of the entries
 * in tpsc_types.  It basicly assumes a limited QIC set of capablities. */
struct tpsc_types tpsc_generic = {
	TPUNKNOWN,TPUNKNOWN, 0, 0, "", "", 0, 0, {0, 0, 0, 0},
	MTCAN_SPEOD, 20, 60, 18*60, 5*60, 512, 400*512 
};


/* variables from kernel file */
int maxcpus = 1;	/* kernel drivers only use 1 cpu for standalone */
/* long lbolt = 0;		*//* for disk statistics */
