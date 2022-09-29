#ident "$Revision: 1.21 $"

/*	This bit of grossness exists so that sash can cause a Toshiba
	CDROM drive to revert back to being a CDROM in it's inquiry
	string.  With the custom SGI firmware, the drive defaults to
	a 512 byte blocksize, and inquiry info to indicate a
	non-removable hard disk.  This is needed so all the old
	PROM's in the field will work with it.  However, once
	up in the kernel, we want the CDROM to show up as a CDROM,
	and as removable.  The blocksize doesn't change.

	We don't want to send the vendor specific commands to drives
	that don't look like the cdrom, since we don't know what they
	will do (at least some seagate drives respond to it, and try
	to disconnect, who knows what they think is going on; the cmd
	could 'hang' the system.  Since the Toshiba drive gives a
	chkcondition on the c9 command if a unitatn is pending, so we
	do a reqsense first.

	This command needs to be executed prior to the sash auto-install
	stuff, which treats CDROM specially (mrboot).

	I chose to put it in the io directory, because it has many
	of the elements of a driver, although it does NOT show up
	in conf.c.
*/

#include <sys/param.h>
#include <sys/fstyp.h>
#include <sys/buf.h>
#include <sys/scsi.h>
#include <sys/invent.h>
#include <sys/types.h>
#include <arcs/types.h>
#include <arcs/hinv.h>
#include <libsc.h>
#include <libsk.h>

static int revertit(ULONG ad, ULONG un, ULONG lu);
void revert_walk(COMPONENT *);
static void docdmodeselect(ULONG, ULONG, ULONG);

void
revertcdrom(void)
{
	revert_walk(GetChild(NULL));
}

void
revert_walk(COMPONENT *c)
{
	COMPONENT *ctrl,*adap;
	u_char *inv;

	if (!c) return;

	ctrl = GetParent(c);
	adap = (ctrl) ? GetParent(ctrl) : 0;

	if(ctrl && adap && c->Type == DiskPeripheral) {
	    int resettype = 0;
	    if(ctrl->Type == DiskController && adap->Type == SCSIAdapter) {
#ifdef IP22_WD95
		if (adap->Key == 0) /* must be wd93 */
			inv = scsi_inq(adap->Key,ctrl->Key, c->Key);
		else
			inv = wd95_scsi_inq(adap->Key,ctrl->Key, c->Key);
#else
			inv = scsi_inq(adap->Key,ctrl->Key, c->Key);
#endif

		if (inv == NULL) return;

		/* two different Toshiba CD model #'s, and one of the Sony
		 * (so far) have custom firmware that comes up as a hard
		 * disk, then reverts to a CDROM on receipt of a C9 cmd.
		 * The LMS product will be used in Europe. */
		inv += 8;
		if(!strncmp((char*)inv, "TOSHIBA CD", 10) ||
		   !strncmp((char*)inv, "SONY    CD", 10) ||
		   !strncmp((char*)inv, "LMS     CM ", 11)) {
			resettype = 1;
			if(revertit(adap->Key, ctrl->Key, c->Key))
				return;
#ifdef IP22_WD95
			if (adap->Key == 0) /* must be wd93 */
				inv = scsi_inq(adap->Key, ctrl->Key, c->Key);
			else
				inv = wd95_scsi_inq(adap->Key, ctrl->Key, c->Key);
#else
				inv = scsi_inq(adap->Key, ctrl->Key, c->Key);
#endif
			if(inv == NULL)	/* oops, this shouldn't happen! */
				return;
			if((inv[0]&0x1f) == INV_CDROM) {
				/* fix up the inventory struct. */
				ctrl->Type = CDROMController;
				c->Key = ((c->Key>>8)&0xff) |
					(inv[1] & 0x80) | (inv[2] & 7);
			}
		}
	    }
	    else
		resettype = (ctrl->Type == CDROMController) ? 1 : 0;

	    if (resettype)
		docdmodeselect(adap->Key, ctrl->Key, c->Key);
	}

	revert_walk(GetChild(c));
	revert_walk(GetPeer(c));
}

static int
revertit(ULONG ad, ULONG un, ULONG lu)
{
	scsisubchan_t *sp;
	static u_char revert[10];
	static u_char *rdata;
	buf_t *bp;
	int ret;

	if ((rdata == NULL) && ((rdata = dmabuf_malloc(4)) == NULL))
		return 1;

#ifdef IP22_WD95
	if (ad == 0) /* Must be wd93 */
		if((sp=allocsubchannel(ad, un, lu)) == NULL)
			return 1;
	else /* Must be wd95 */
		if((sp=wd95_allocsubchannel(ad, un, lu)) == NULL)
			return 1;
#else
		if((sp=allocsubchannel(ad, un, lu)) == NULL)
			return 1;
#endif

	if((bp = (buf_t*)malloc(sizeof(*bp))) == NULL) {
#ifdef IP22_WD95
		if (ad == 0) /* Must be wd93 */
			freesubchannel(sp);
		else
			wd95_freesubchannel(sp);
#else
			freesubchannel(sp);
#endif
		return 1;
	}

	sp->s_buf = bp;
	sp->s_cmd = revert;
	sp->s_timeoutval = 2*HZ;
	bp->b_dmaaddr = (caddr_t)rdata;
	bp->b_flags = B_READ|B_BUSY;

	/* do a reqsense, in case a unitatn pending */
	*revert = 3;	/* reqsense */
	revert[4] = bp->b_bcount = 4;
	sp->s_len = 6;
	doscsi(sp);
	/* ignore any errors from reqsense */

	*revert = 0xc9;	/* the revert cmd */
	revert[4] = 0;
	sp->s_len = sizeof(revert);
	bp->b_bcount = 0;
	bp->b_flags = B_READ|B_BUSY;
	doscsi(sp);
	if(sp->s_error == SC_GOOD && sp->s_status == ST_GOOD)
		ret = 0;
	else {
		if(sp->s_status == ST_CHECK) {	/* clear to avoid confusion later */
			bp->b_flags = B_READ|B_BUSY;
			revert[4] = bp->b_bcount = 4;
			*revert = 3;	/* reqsense */
			sp->s_len = 6;
			doscsi(sp);
		}
		ret = 1;
	}

#ifdef IP22_WD95
	if (ad == 0) /* Must be wd93 */
		freesubchannel(sp);
	else
		wd95_freesubchannel(sp);
#else
		freesubchannel(sp);
#endif
	free(bp);
	return ret;
}

static void
docdmodeselect(ULONG ad, ULONG un, ULONG lu)
{
        scsisubchan_t *sp;
        u_char scsi_cdcmd[SC_CLASS0_SZ];
        static u_char *rdata;
        buf_t *bp;
	int tries = 3;


        if ((rdata == NULL) && ((rdata = dmabuf_malloc(0x16)) == NULL))
                return;

#ifdef IP22_WD95
	if (ad == 0) /* Must be wd93 */
		if((sp=allocsubchannel(ad, un, lu)) == NULL)
			return;
	else /* Must be wd95 */
		if((sp=wd95_allocsubchannel(ad, un, lu)) == NULL)
			return;
#else
		if((sp=allocsubchannel(ad, un, lu)) == NULL)
			return;
#endif

        if((bp = (buf_t*)malloc(sizeof(*bp))) == NULL) {
#ifdef IP22_WD95
		if (ad == 0) /* Must be wd93 */
                	freesubchannel(sp);
		else
                	wd95_freesubchannel(sp);
#else
                	freesubchannel(sp);
#endif
                return;
        }

        sp->s_buf = bp;
        sp->s_cmd = scsi_cdcmd;
        sp->s_timeoutval = 2*HZ;
        bp->b_dmaaddr = (caddr_t)rdata;

retry:
	bp->b_flags = B_WRITE|B_BUSY;
	bzero(scsi_cdcmd, SC_CLASS0_SZ);
	bzero(rdata, 0x16);

        *scsi_cdcmd = 0x15;    /* mode select */
        scsi_cdcmd[1] = 0x10;    /* PF; conforms to scsi2 */
	bp->b_bcount = scsi_cdcmd[4] = 12;  /* data phase len */
	rdata[3] = 8;	/* descr block len */
	rdata[10] = 2;	/* block size = 512 */
	sp->s_notify = NULL;
        sp->s_len = SC_CLASS0_SZ;

        doscsi(sp);

        if(sp->s_error == SC_GOOD && sp->s_status == ST_CHECK) {
                        scsi_cdcmd[3] = 0;
        		bp->b_flags = B_READ|B_BUSY;

                        bzero(scsi_cdcmd, SC_CLASS0_SZ);
                        *scsi_cdcmd = 3;    /* reqsense */
                        scsi_cdcmd[4] = bp->b_bcount = 0x16;
                        sp->s_len = SC_CLASS0_SZ;

                        doscsi(sp);

			if(--tries > 0) goto retry;
        }

#ifdef IP22_WD95
	if (ad == 0) /* Must be wd93 */
        	freesubchannel(sp);
	else
        	wd95_freesubchannel(sp);
#else
        	freesubchannel(sp);
#endif
        free(bp);
        return;
}
