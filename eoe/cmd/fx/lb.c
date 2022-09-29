#include <sys/types.h>
#include "dklabel.h"	/* needs to be before fx.h for prototypes */
#include "fx.h"


char *bootfile = "/unix";
static struct device_parameters otherparams;

extern MENU *cmenu;
extern int verbose_sgiinfo;
extern MENU drive_menu;
extern int nomenus;	/* see fx.c, -c option */


void
sync_func(void)
{
#ifdef SMFD_NUMBER
	if(drivernum == SMFD_NUMBER) {
		printf("no labels on floppy disks\n");
		return;
	}
#endif /* SMFD_NUMBER */

	printf("writing label info to %s\n", dksub());

	if (update() != 0)
		scerrwarn("label write failed!\n");
}

void
show_all_func(void)
{
	if(drivernum == SCSI_NUMBER) {
		scsi_showparam_func();
		scsi_showgeom_func();
	}
#ifdef SMFD_NUMBER
	else if(drivernum == SMFD_NUMBER) {
		/* check so people won't see error messages when they choose
		 * 'all' and they are dealing with the floppy */
		scsi_showparam_func();
		scsi_showgeom_func();
		return;
	}
#endif /* SMFD_NUMBER */
	show_pt_func();
	show_bi_func();
	show_dt_func();
	show_sgiinfo_func();
}

void
create_all_func(void)
{
#ifdef SMFD_NUMBER
	if(drivernum == SMFD_NUMBER)
		/* nothing to do at all, so just return */
		return;
#endif

	/*	do bootinfo before pt_func, because if bootinfo sets the swap
		and root partition #'s, and if we are creating everything, we
		want to use the sgi defaults, not something left over */
	create_bi_func();
	create_pt_func();
	create_sgiinfo_func();
	create_dt_func();
}


void
show_bi_func(void)
{
	char *f;
#ifdef SMFD_NUMBER
	if(drivernum == SMFD_NUMBER) {
		printf("no bootinfo on floppy disks\n");
		return;
	}
#endif /* SMFD_NUMBER */

	f = "%15s = %d    ";
	setoff("bootinfo");
	printf(f, "root partition", VP(&vh)->vh_rootpt);
	printf(f, "swap partition", VP(&vh)->vh_swappt);
	printf("%s = %.16s", "bootfile", VP(&vh)->vh_bootfile);
	newline();
}

void
set_bi_func(void)
{
	uint rootpt, swappt;
	STRBUF s;
#ifdef SMFD_NUMBER
	if(drivernum == SMFD_NUMBER) {
		printf("no bootinfo on floppy disks\n");
		return;
	}
#endif /* SMFD_NUMBER */

	argslice(&rootpt, PTNUM_FSROOT, PTNUM_VOLHDR, "root partition");
	argslice(&swappt, PTNUM_FSSWAP, PTNUM_VOLHDR, "swap partition");
	strncpy(s.c, VP(&vh)->vh_bootfile, BFNAMESIZE);
	s.c[BFNAMESIZE] = '\0';
	argstring(s.c, s.c, "bootfile");
	argcheck();
	VP(&vh)->vh_rootpt = rootpt;
	VP(&vh)->vh_swappt = swappt;
	strncpy(VP(&vh)->vh_bootfile, s.c, BFNAMESIZE);
	(void)checkparts();
	changed = 1;
	show_bi_func();
}

void
create_bi_func(void)
{
#ifdef SMFD_NUMBER
	if(drivernum == SMFD_NUMBER) {
		printf("no bootinfo on floppy disks\n");
		return;
	}
#endif /* SMFD_NUMBER */

	printf("...creating default bootinfo\n");

	VP(&vh)->vh_rootpt = PTNUM_FSROOT;
	VP(&vh)->vh_swappt = PTNUM_FSSWAP;
	strncpy(VP(&vh)->vh_bootfile, bootfile, BFNAMESIZE);
	changed = 1;
}

void
readin_bi_func(void)
{
	CBLOCK b;

	printf("...reading in bootinfo\n");
	if( readdvh(VP(&b)) < 0 )
		return;
	bcopy((char *)VP(&b)->vh_bootfile, (char *)VP(&vh)->vh_bootfile,
	    sizeof VP(&b)->vh_bootfile);
	VP(&vh)->vh_rootpt = VP(&b)->vh_rootpt;
	VP(&vh)->vh_swappt = VP(&b)->vh_swappt;
}


void
create_dt_func(void)
{
#ifdef SMFD_NUMBER
	if(drivernum == SMFD_NUMBER) {
		printf("no volume header on floppy disks\n");
		return;
	}
#endif /* SMFD_NUMBER */
	bzero((char *)DT(&vh), sizeof DT(&vh));
	printf("...creating default volume directory\n");
	changed = 1;
}


void
readin_dt_func(void)
{
	CBLOCK b;

	printf("...reading in volume directory\n");
	if( readdvh(VP(&b)) < 0 )
		return;
	bcopy((char *)DT(&b), (char *)DT(&vh), sizeof DT(&vh));
}


/* ix not used, but colprint passes it */
/*ARGSUSED*/
static void
desub(int ix, int *t, char *tgt)
{
	register struct volume_directory *dirp;
	dirp = DT(&vh)+*t;
	sprintf(tgt, "%2d: %-10.8s block %4d size %7d",
	    *t, dirp->vd_name, dirp->vd_lbn, dirp->vd_nbytes);
}


void
show_dt_func(void)
{
	int dirnums[NVDIR];
	register uint i, j;
#ifdef SMFD_NUMBER
	if(drivernum == SMFD_NUMBER) {
		printf("no volume header on floppy disks\n");
		return;
	}
#endif /* SMFD_NUMBER */

	j = 0;
	for( i = 0; i < NVDIR; i++ )
		if( *DT(&vh)[i].vd_name != '\0' )
			dirnums[j++] = i;
	setoff("directory entries");
	colprint(dirnums, j, sizeof *dirnums, 38, desub);
}

/*
 * update the named directory entry with the given data.
 */

void
update_dt(char *name, void *data, int len)
{
	register struct volume_directory *vd;
	char ls_map[4096];	/* this is pretty ugly; the size
		    sets how much of the vh is even looked at... */
	int ls_max = sizeof(ls_map);
	int slen;	/* len in sectors */
	int lbn;

#ifdef SMFD_NUMBER
	if(drivernum == SMFD_NUMBER) {
		printf("no volume header on floppy disks\n");
		return;
	}
#endif /* SMFD_NUMBER */

	if (!len)
	{
		printf("Request for 0 len directory entry!?\n"); /* DHXX*/
		return;
	}


	if( (vd = findent(name, 1)) == 0 )
	{
		errwarn("can't alloc entry for %s", name);
		return;
	}

	ls_init(ls_map, &ls_max);

	slen = btos(len);
	len = stob(slen); /* round it up to a sector size */
	if(vd->vd_lbn)
		map_unbusy(ls_map, ls_max, vd->vd_lbn, slen);
	if((lbn = map_alloc(ls_map, ls_max, slen)) < 0 )
	{
		map_busy(ls_map, ls_max, lbn, slen);
		errwarn("can't alloc label space for %s", name);
		return;
	}

	if( gwrite((daddr_t)lbn, data, slen) < 0 )
	{
		/* print this even if EROFS */
		errwarn("can't write %s", name);
		return;
	}

	vd->vd_lbn = lbn;	/* Success */
	vd->vd_nbytes = len;
}

void
ls_init(char *lmap, int *lmax)
{
	register int i;

	if(*lmax > PT(&vh)[PTNUM_VOLHDR].pt_nblks)
		*lmax = PT(&vh)[PTNUM_VOLHDR].pt_nblks;
	map_unbusy(lmap, *lmax, (daddr_t)0, *lmax);

	/*  Busy the first 2 blocks.  2 because we had limited 
	* FAT support that we ended up never using.  May find a
	* use for it in the future...  This number must agree with
	* the code in dvhtool that creates files in the volhdr.
	*
	* NOTE: originally the volume_header struct was supposed to be
	* written in sector 0 of each track in cyl 0 for redundancy.
	* Due to a bug in dvhtool and fx, this wasn't enforced until
	* 3.2.  The redundant copies have never been used by anything,
	* and few people knew about them, and NOW (2/92) we find that
	* we need to put much larger files in the volhdr.  That meant
	* that as of 3.2, we simply busied out the entire first
	* cylinder (because even though we only wrote 2 single sector
	* files, if we didn't do this, the copies of the volhdr could
	* have gotten written out on top of files we put in volhdr
	* after we wrote the files, thus trashing them.  We can't go
	* out and retroactively increase the size of the volhdr
	* partition on thousands of machines, so now we are
	* deliberately going back to NOT making the redundant copies,
	* and allowing use of the whole volhdr partition (except sector
	* 0, of course) for files.  This big comment is so that no
	* one will come back in the future and 'fix' this again...
	* Dave Olson, 2/92 */

	map_busy(lmap, *lmax, 0, 2);

	/* now mark all the blocks used by existing files as in use */
	for( i = 0; i < NVDIR; i++ )
		map_busy(lmap, *lmax, DT(&vh)[i].vd_lbn,
		    btos(DT(&vh)[i].vd_nbytes));
}

/*
 * find the named entry.  optionally create it if nonexistent.
 */
struct volume_directory *
findent(char *name, int flag)
{
	register struct volume_directory *vd, *slot;
	register int i;

	slot = 0;
	for( vd = DT(&vh) , i = NVDIR; --i >= 0; vd++ )
	{
		if( strncmp(name, vd->vd_name, VDNAMESIZE) == 0 )
			return vd;
		if( slot == 0 )
			if( *vd->vd_name == '\0' && vd->vd_nbytes == 0 )
				slot = vd;
	}
	if(flag && slot) {
		strncpy(slot->vd_name, name, VDNAMESIZE);
		slot->vd_lbn = 0;
		changed = 1;
		return slot;
	}
	return 0;
}

/* 
 * Clear the named entry, if it exists, from the volume directory.
 * Needed when zapping the badblock list.
 */
void
clearent(char *name)
{
	register struct volume_directory *vd;
	register int i;
	int zapping = 0;

	for( vd = DT(&vh) , i = NVDIR; --i >= 0; vd++ )
	{
		if (!zapping && strncmp(name, vd->vd_name, VDNAMESIZE) == 0 )
			zapping = 1;
		if (zapping)
		{
			if (i > 1) bcopy((caddr_t)(vd + 1), (caddr_t)vd, 
			    sizeof (struct volume_directory));
			else bzero((caddr_t)vd, sizeof (struct volume_directory));
		}
	}
}

/*
 * read in the label information.
 * first we must get the volume header.  if that is invalid,
 * get the drive type and set up defaults.
 */
void
init_label(char *dname)
{
	int mustrewrite = 0;
	int partsok;

#ifdef SMFD_NUMBER
	if(drivernum == SMFD_NUMBER) {
		check_dp(dname, 0);
		return;
	}
#endif /* SMFD_NUMBER */

	if( readdvh(&vh) < 0 )	{ /* no volhdr of either type */
		mustrewrite = 1;
		check_dp(dname, 0);
		create_label();  
	}
	else {
	    check_dp(dname, 1);
	    if(expert && (partsok=checkparts())) {
		    printf("NOTE: %s existing partitions are"
		      " inconsistent with drive geometry\n", dname);
		    if(partsok < 0) {	 
		    /* some were out of range default to no, since 
		     * this will make it difficult if not impossible
		     * to get their old partitions (and thus their data) 
		     * back.  */
		    if(!no("create default partitions (answering yes may"
			    "make it difficult\n\tto retrieve existing"
			    "data from the drive)")) {
			    create_pt_func();
			    mustrewrite = 1;
		    }
		    /* default to no here also */
		    if(!no("edit partitions") ){
			    callfunc(set_pt_func, "partitions");
			    mustrewrite = 1;
		    }
		    }
	    }
	}

	if(mustrewrite)
		set_vh(VP(&vh));

}

/*
 * initialize drive parameters; if vhvalid, use the volume header.
 * in the end, we should have 3 consistent pieces of info:
 *	the disk name
 *	the sgi disk type
 *	drive parameters
 *
 * the argument is the name of a disk drive.  it may be
 *	''		null; ask for drive type
 *	'other'		the literal other; prompt for all info
 *	xxx		the full name of a disk drive; get info from tables
 *
 * Returns: 0 if existing volume header is OK, 
 * 	    1 if the volume header must be rewritten to the driver.
 */
void
check_dp(char *dname, int vhvalid)
{
	CBLOCK sgijunk;
	int goodsgilab = 0;
	int cantread = 0;

	switch (drivernum) {
#ifdef SMFD_NUMBER
	case SMFD_NUMBER:	/* done just to get name printed */
#endif /* SMFD_NUMBER */
	case SCSI_NUMBER:
		otherparams.dp_flags = DP(&vh)->dp_flags;
		otherparams.dp_ctq_depth = DP(&vh)->dp_ctq_depth;
		otherparams.dp_drivecap = DP(&vh)->dp_drivecap;
		scsiset_dp(&otherparams);
		scsiset_label(&sgijunk, 1);
		break;

	default:
		printf("Don't know how to set parameters for driver type %d\n",
		    drivernum);
		return;
	}

	if(expert)
	    changed = !vhvalid;

	if(vhvalid && expert && dpcmp(DP(&vh), &otherparams)) {
		    printf("\
	    warning: %s disagrees with existing volume header parameters\n",
			dname);
		    if( yes("show differences") )
			    showdiff_dp();
		    /* default to using existing vh; otherwise user can wind
			    up with partitition layout that dosn't match their
			    filesystems. */
		    if(!yes("use existing volume header") )
			    changed = 1;
	    }

	/* always do this; was if mustrewrite || expert; but vh still needs
		to be valid for exercising, even if not expert.  Turns out
		the equivalent of this must be buried down in the code
		for hard disks, because it worked for them, but wouldn't
		work for floppies.  Can't see what this would ever hurt,
		and hopefully we can remove the redundant code in other
		portions of fx sometime...  Olson, 4/83 */
	bcopy((char *)&otherparams, (char *)DP(&vh), sizeof otherparams);

	if (vhvalid && expert)
	{
		bzero(&sg, sizeof (struct disk_label));
		cantread = readsgilabel (&sg, !expert);

		/* hardly worth it for scsi, but... */
		if(cantread==0 && ((struct disk_label *)&sg)->d_magic == D_MAGIC)
			goodsgilab = 1;
	}

	if (!goodsgilab
#ifdef SMFD_NUMBER
	    && drivernum != SMFD_NUMBER
#endif /* SMFD_NUMBER */
	    )
		/* don't complain, and don't mark anything as changed.  If they
		 * do happen to write the volhdr for some other reason, then
		 * we'll create the sgilabel;  see comments at readsgilabel()
		 * for why we don't complain, 4/97. */
		bcopy((char *)&sgijunk, (char *)&sg, sizeof(struct disk_label));
}

int
dpcmp(struct device_parameters *v, struct device_parameters *a)
{
	if(v->dp_secbytes == a->dp_secbytes &&
	    (!a->dp_drivecap || v->dp_drivecap == a->dp_drivecap))
		return 0;
	return 1;
}
void
showdiff_dp(void)
{
	register struct device_parameters *v, *a;
	register char *f;

	v = DP(&vh); 
	a = &otherparams;
# define DD(s, e) if(v->e!=a->e)printf(f,s,v->e,a->e)
	f = "%15s = volume header %-5d; should be %-5d to match drive\n";

	DD("bytes/sec", dp_secbytes);
	if(v->dp_drivecap) DD("drive capacity", dp_drivecap);
# undef DD
}


/*
 * make a default volume header based on the current drive parameters.
 */
void
create_label(void)
{
	DP(&vh)->dp_flags = 0;		/* make ctq default disabled */
	create_bi_func();
	create_pt_func();
	create_dt_func();
}

/* Update mbr from vh */
int
update_vh(void)
{
	int	error;
	char b[MAX_BSIZE];	/* gwrite will use the 'actual' secsize */

	/* DHXX: must set volume header in driver as well as on disk!!
       set_vh now has the side-effect of setting the magic # and
       checksum in vh for us. */

	VP(&vh)->vh_magic = 0;

	error = set_vh(VP(&vh)); 

	if ( ! error) {
		bzero(b, sizeof b);
		bcopy((char *)VP(&vh), b, sizeof *VP(&vh));

		if (gwrite(0, b, 1) < 0 )
			errwarn("can't write volume header on disk");
	}
	
	return error;
}

int
checkvh(struct volume_header *vhp)
{
	if ((vhp->vh_magic == VHMAGIC
	    && vhchksum((int *)vhp, sizeof *vhp) == 0 ))
		return 0;

	bzero(vhp, sizeof *vhp);
	return -1;
}

int
readdvh(struct volume_header *vhp)
{
	get_vh(vhp);
	if (checkvh(vhp) < 0 )  
		return errwarn("invalid label from disk driver, ignored");
	return 0;
}

int
readinvh(struct volume_header *vhp)
{

	if(gread(0, vhp, 1) == 0 && checkvh(vhp) == 0 ) {
		extern unsigned scsi_cap;
		if(!DP(vhp)->dp_drivecap) {
			if(!scsi_cap) scsi_readcapacity(&scsi_cap); /* shouldn't happen */
			DP(vhp)->dp_drivecap = scsi_cap;
		}
		return 0;
	}

	return -1;
}

void
lastchance(void)
{
	STRBUF s;
	sprintf(s.c, "about to destroy data on disk %s! ok", dksub());

	if(nomenus)
		printf("%s\n", s.c);
	else {
	    banner("WARNING");
	    if( !yesno(-1, s.c) )
		    mpop();
	}
	changed = 1;
}

void
optupdate(void)
{
	STRBUF s;
	CBLOCK b;

#ifdef SMFD_NUMBER
	if(drivernum == SMFD_NUMBER)	/* not on floppies */
		return;
#endif /* SMFD_NUMBER */

	/* fetch real label from disk, make sure it's valid */
	if( readinvh(VP(&b)) < 0 )
		changed = 1;

	if( changed )
	{
		sprintf(s.c, "\
label info has changed for disk %s.  write out changes", dksub());

		if( yes(s.c) ) {

			if (update() != 0)
				scerrwarn("label write failed!\n");
		}
	}
}

int
update(void)
{
	int	error;


	switch (drivernum) {

	case SCSI_NUMBER:
		break;	/* nothing to do for badblocks on scsi */

	default:
		printf("Don't know how to update for driver type %d\n",
		    drivernum);
		return EINVAL;
	}

	error = update_vh();

	if ( ! error ) {
		/* Don't allow messing with 
		 * sgilabel if nonexpert 
		 */
		if (expert)
			update_sgi();   

		changed = 0;
	}

	return error;
}

void
getattribs(int *_n, ITEM *items)
{
	register ITEM *t;
	register int n, b;
	STRBUF s;

	n = *_n;
	for( t = items; t->name != 0; t++ )
	{
		b = n & t->value;
		n &= ~t->value;
		sprintf(s.c, "enable %s", t->name);
		if( yesno(b, s.c) )
			n |= t->value;
	}
	*_n = n;
}

/* bytes to sectors */
uint
btos(uint n)
{
	return (n + DP(&vh)->dp_secbytes - 1) / DP(&vh)->dp_secbytes;
}

void
readin_dp(void)
{
	CBLOCK b;

	if( readdvh(VP(&b)) < 0 )
		return;
	bcopy((char *)DP(&b), (char *)DP(&vh), sizeof *DP(&vh));
}

char *
dksub(void)
{
	static STRBUF s;
	if (device_name_specified)
		sprintf(s.c, "%s", device_name);
	else
	{
		sprintf(s.c, "%s(%d,%d,%d)", driver, ctlrno, driveno, scsilun);
#ifdef SMFD_NUMBER
		if(drivernum == SMFD_NUMBER) {
			char *type;
			if(type=(char *)smfd_partname(partnum))
				strcat(s.c, type);
		}
#endif /* SMFD_NUMBER */
	}

	return s.c;
}

/* ----- map routines ----- */
void
map_busy(char *map, uint len, uint a, int n)
{
	while(n) {
		if(a < len )
			map[a++] = 1;
		n--;
	}
}

void
map_unbusy(char *map, uint len, uint a, int n)
{
	while(n) {
		if(a < len )
			map[a++] = 0;
		n--;
	}
}

map_alloc(char *map, uint len, uint n)
{
	register uint r, f;

	f = 0;
	for( r = 0; r < len; r++ )
		if( map[r] )
		{
			f = 0;
		}
		else
		{
			if( ++f >= n )
			{
				r++;
				while(n) {
					map[--r] = 1;
					n--;
				}
				return (int)r;
			}
		}
	return -1;
}

/* ----- geometry routines ----- */
uint
stob(uint n)
{
	return n * DP(&vh)->dp_secbytes;
}


/* round # of blocks to to number of megabytes; since we are getting
 * close to 4 Gb disks, do in such a way that we don't overflow an
 * unsigned 32bit value.
*/
uint
mbytes(uint blocks)
{
	blocks += ((1<<19)-1) / DP(&vh)->dp_secbytes;
	return blocks / ((1<<20) /  DP(&vh)->dp_secbytes);
}

/* return block number corresponding to value in megabytes */
uint
mbytetoblk(uint mbyte)
{
	return mbyte * ((1<<20) /  DP(&vh)->dp_secbytes);
}
