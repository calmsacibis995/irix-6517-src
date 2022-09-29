#include <sys/param.h>
#include <sys/dkio.h>
#include <sys/dvh.h>
#include <sys/types.h>
#include <ctype.h>
#include <saio.h>
#include <saioctl.h>
#include <libsc.h>
#include <libsk.h>
#include <uif.h>

#include <arcs/io.h>
#include <arcs/hinv.h>
#include "scsi.h"

/*
 * scsi exerciser and support routines.
 * these implement software controlled retries,
 * and device addressing by driveno instead of fd.
 * THIS ASSUMES THAT we DEALS WITH AT MOST ONE CTLR AT A TIME!
 */

struct volume_header vh;
int 					ctlrno = 0;
int 					ctlrtype = 0;
int 					driveno;
char scsi_name[] = "dksc";
char 					*driver = scsi_name;



int ioretries = 3; 	/* error retries */

int diskfd = -1; 	/* disk file descr */

/* controller & drive info: set by fx_find_ctlr() which looks at */
/* system hardware. */


/*
 * open the device pointed to by the given component.
 */

static int
gopen(COMPONENT *c, int d)
{
	extern char *getpath(COMPONENT *);
	char s[128];
	char *p;
	ULONG fd;

	p = getpath(c);

	sprintf(s, "%spart(%d)", p,d);
	msg_printf(VRB, "...opening %s\n", s);

	if (Open ((CHAR *)s, OpenReadWrite, &fd))
		fd = -1;
	diskfd = (int)fd;

	return (int)fd;
}

/*
 * read from disk at block bn, with retries

 * For SCSI, don't retry on multiple block reads or report errors
 * or add them to the error log if we are exercising the
 * disk AND this is a multiple block read.  The exercisor
 * will retry each block separately when it gets errors
 * on multiple block reads.  Otherwise we confuse the 
 * user by reporting errors as the first block of the
 * read.
 * For ESDI & SMD, though, we do NOT go through the rigmarole of trying
 * to find the exact sector since we will be mapping the whole track.
 * So in this case, while we still don't retry during exercise, we
 * SHOULD log the error.
 */
int
gread( daddr_t bn, char *buf, int count)
{
	register int i, error = 0;
	ULONG rcnt;
	LARGE offset;
	char *errstr;

	count *= BBSIZE;

	for( i = 0; i <= ioretries; i++ )
	{
		offset.hi = 0;
		offset.lo = (ULONG)bn*BBSIZE;
		if ((!(error=Seek(diskfd, &offset,SeekAbsolute)))
		    && !(error=Read(diskfd, buf, count, &rcnt))
		    && count == rcnt)
			return 0;

		if(count != BBSIZE)
			break;
		else {
			adderr(0, bn, "sr");
			if( i < ioretries )
				msg_printf(DBG, "retry #%d\n", i+1);
		}
	}

	adderr(0, bn, "hr");
	errstr = arcs_strerror(error);
	msg_printf(ERR, "Read error at block %d: %s\n", bn,
		errstr ? errstr : "unknown error");
	return -1;
}

/*
 * ioctl on the disk, with error reporting
 */
int
gioctl(int cmd, char *ptr)
{
	return ioctl(diskfd, cmd, (LONG)ptr);
}

/*
 * close if we had it open
 */
static
void
gclose(void)
{
	if(diskfd != -1) {
		Close(diskfd);
		diskfd = -1;
	}
}



/* ----- geometry routines ----- */


#define PT_VOLUME 10	/* part 10 is the whole drive */

/*
 * find the end of the area which may be exercised, given a starting
 * block number.  if starting outside of the replacement partition, stay
 * outside of it.  if inside, stay away from the next in-use replacement
 * track And the next savedefect track.
 */
daddr_t
exarea(daddr_t startbn)
{
	register int strack, i;
	int cap = 0;	/* zero in case of errors */

	/*	use lesser of capacity and end of drive; for drives
		with zone-bit-recording, or drives that have
		spares allocated on a per zone basis where a zone is
		more than one track. */
	scsi_readcapacity(&cap);
	if(cap > vh.vh_pt[PT_VOLUME].pt_nblks)
		cap = vh.vh_pt[PT_VOLUME].pt_nblks;
	return cap - startbn;
}

static void execscsi_walk(COMPONENT *, int *);

/*
   invoke routines to exercise all SCSI disk drives non-destructively
   Returns mask of failed drives.  ctrlr 0 in low 8 bits, ctrlr 1 in
   bits 8-15.
*/
int
exercise_scsi ()
{
	int error_mask = 0;

	gclose ();
	execscsi_walk(GetChild(NULL), &error_mask);

	/* ensure led off when done; calling scripts will turn it on if needed
	 */
	busy(0);

	return (error_mask);
}

static void
execscsi_walk(COMPONENT *c, int *error_mask)
{
	COMPONENT *ctrl, *adap;

	if (!c) return;

	if ((c->Type == DiskPeripheral) &&
	    ((ctrl = GetParent(c))->Type == DiskController) &&
	    ((adap = GetParent(ctrl))->Type == SCSIAdapter)) {

		driveno = ctrl->Key;
		ctlrno = adap->Key;

		if (gopen (c, 10) < 0 ) {	
			*error_mask |= (ctlrno?1:0x100) << driveno;
			msg_printf (ERR,
				    "Bad status when trying to open SCSI disk drive %d\n", driveno);
		}
		else {
			msg_printf (VRB, "Exercising SCSI disk drive %d\n", driveno);
			init_ex ();
			if ((gioctl (DIOCGETVH, (char *)&vh) < 0) || !is_vh (&vh)) {

				*error_mask |= (ctlrno?1:0x100) << driveno;
				msg_printf (ERR, "Invalid volume header on drive %d\n", driveno);
				return;
			}
			if (sequential_func ())
				*error_mask |= (ctlrno?1:0x100) << driveno;

			/* have to close here, since gclose uses a global; otherwise
			 * we leave everything but the last device open, which can hose
			 * up the prom, due to leaving devices in sync mode */
			gclose();
		}
	}

	execscsi_walk(GetChild(c),error_mask);
	execscsi_walk(GetPeer(c),error_mask);
	return;
}
