# include "fx.h"

#ident "$Revision: 1.62 $"

ITEM ptype_items[] =
{
	{"volhdr", PTYPE_VOLHDR},
	{"", -1}, /* was trkrepl */
	{"", -1}, /* was secrepl */
	{"raw", PTYPE_RAW},
	{"", -1}, /* was bsd */
	{"", -1}, /* was sysv */
	{"volume", PTYPE_VOLUME},
	{"efs", PTYPE_EFS},
	{"lvol", PTYPE_LVOL},
	{"rlvol", PTYPE_RLVOL},
	{"xfs", PTYPE_XFS},
	{"xfslog", PTYPE_XFSLOG},
	{"xlv", PTYPE_XLV},
	{0}
};
MENU ptype_menu = { ptype_items, "partition-types" };
MENU menu_type;
char noflopartmsg[] = "no partitions on floppy disks\n";

int majorchange = 1;
static int fstype = PTYPE_XFS;

extern int nomenus;     /* see fx.c, -c and -s options */

#define LOGSIZE 4  /* Size of log partition for xlv */


/* display the partitions from the supplied volhdr struct.  */
void
show_pts(struct volume_header *vp)
{
	register int i, first=1, j;
	char *s;
	register struct partition_table *p;

	j = 0;
	menu_type = ptype_menu;

	if( menu_type.nitems <= 0 )
		menu_digest(&menu_type);

	p = &PT(vp)[j];
	for( i = j; i < NPARTAB; i++,p++ ) {
		if(p->pt_nblks && p->pt_firstlbn >= 0) {
			if(first) {
				setoff("partitions");
				printf(
		    "part  type        blocks            "
		    "Megabytes   (base+size)\n");
				first=0;
			}
			s = (unsigned)p->pt_type >= menu_type.nitems
			    ? "unknown" : menu_type.items[p->pt_type].name;
			printf(
		" %2d: %-7.7s%8u + %-8u   %5u + %-5u\n",
			    i, s,
			    p->pt_firstlbn, p->pt_nblks, 
			    mbytes(p->pt_firstlbn), (long)mbytes(p->pt_nblks));
		}
	}
}


/*
 * print the partition table, very similar to dumpslice, but with a heading
 */
void
show_pt_func(void)
{
#ifdef SMFD_NUMBER
	if(drivernum == SMFD_NUMBER) {
		printf(noflopartmsg);
		return;
	}
#endif

	argcheck();

	show_pts((struct volume_header *)&vh);
	(void)checkparts();
}


/*
 * change the partition table.  Note that when mb is being
 * done, that we do NOT change lbn, etc if the user just pressed
 * return to keep the old value, or we could change the value
 * due to rounding, etc.  Default to mb.
 */
void
set_pt_func(void)
{
    uint slice, aslice, use;
    struct partition_table p1;
    unsigned firstone, lastblk;
    char flags[2];
    char promptmax[48];
    int blkflag=0, mbyteflag=1,j;
	int allstandard = 1; /* default to prompting for standard parts only while set*/

    if( ptype_menu.nitems <= 0 )
	    menu_digest(&ptype_menu);
    menu_type = ptype_menu;
    j = 0;


#ifdef SMFD_NUMBER
	if(drivernum == SMFD_NUMBER) {
		printf(noflopartmsg);
		return;
	}
#endif

	printf("Enter .. when done\n");
	/* check for both; if both given, blocks is used */
	checkflags("bm", flags);
	if(flags[0])
		blkflag = 1;
	else if(flags[1])
		mbyteflag = 1;

    for( slice = j; slice < NPARTAB; slice++ )
    {
	if( slice == PTNUM_VOLUME)
	    continue;	/* we don't allow users to set this */
	p1 = PT(&vh)[slice];

	/* only prompt for the partitions we normally create devices for,
	 * unless the partition appears to be valid (i.e., they changed it,
	 * or it was created by a 3.1 fx that created all the partitions).
	 * They can still specify any partition they want at the prompt. */
	if (allstandard && (p1.pt_nblks <= 0 || p1.pt_firstlbn < 0)) {
		switch(slice) {
		case PTNUM_FSROOT:
		case PTNUM_FSSWAP:
		case PTNUM_FSUSR:
		case PTNUM_FSALL:
		case PTNUM_VOLHDR:
			break;
		default:
			continue;
		}
	}

	argslice(&aslice, slice, NPARTAB, "change partition");
	if(aslice == PTNUM_VOLUME)
	{
	    printf("sorry, can't change volume partition\n");
	    slice--;
	    continue;
	}
        p1 = PT(&vh)[aslice];
	if(!p1.pt_nblks && !p1.pt_type)	/* if changing a currently empty
		* partition, show it as xfs by default, not volhdr (which is 0), as that
		* seemed to worry some people; if it's not volhdr, it was probably set
		* before and then made 0 length, so start from the older value */
		p1.pt_type = PTYPE_XFS;
        dumpslice(&p1, "before");
        argchoice((int *)&use, p1.pt_type, &menu_type, "partition type");
        p1.pt_type = use;

	lastblk = PT(&vh)[PTNUM_VOLUME].pt_nblks;
	firstone = p1.pt_firstlbn;

	if(blkflag) {
		if(firstone > lastblk)
			firstone = lastblk;
		argnum(&firstone, firstone, lastblk+1, "first block");
		p1.pt_firstlbn = firstone;
	}
	else if(mbyteflag) {
		unsigned lastmb = mbytes(lastblk);
		firstone = mbytes(firstone);
		if(firstone > lastmb)
			firstone = lastmb;
		if(argnum(&firstone, firstone, lastmb+1, "base in megabytes"))
			p1.pt_firstlbn = mbytetoblk(firstone);
	}

	lastblk -= p1.pt_firstlbn;	/* is now max # of blks in partition */
	firstone = p1.pt_nblks;

	if(blkflag) {
		sprintf(promptmax, "number of blocks (max %u)", lastblk);
		argnum(&firstone, firstone, lastblk+1, promptmax);
		p1.pt_nblks = firstone;
	}
	else if(mbyteflag) {
		unsigned nummb = mbytes(lastblk);
		firstone = mbytes(firstone);
		sprintf(promptmax, "size in megabytes (max %u)", nummb);
		if(argnum(&firstone, firstone, nummb+1, promptmax))
			p1.pt_nblks = mbytetoblk(firstone);
	}

	if(p1.pt_type != PT(&vh)[aslice].pt_type ||
	    p1.pt_firstlbn != PT(&vh)[aslice].pt_firstlbn ||
	    p1.pt_nblks != PT(&vh)[aslice].pt_nblks) {
	    /* only say it's changed, do the checks, etc. if they really changed
		something */
	    PT(&vh)[aslice] = p1;
	    changed = 1;
	    switch(aslice) {
		    case PTNUM_FSROOT:
		    case PTNUM_FSSWAP:
		    case PTNUM_FSUSR:
		    case PTNUM_FSALL:
		    case PTNUM_VOLHDR:
			    break;
		    default:
			    allstandard = 0; /* show all in numerical order now */
	    }

	    dumpslice(&p1, " after");
	    if( partcheck(aslice, &p1) < 0 )
		    aslice--;
	}

	slice = aslice;	/* do the next one after the one that they changed, not 
	    * necessarily the next in the original sequence */
    }

    (void)checkparts();
}

/*
 * print one entry in the partition table
 */
void
dumpslice(struct partition_table *p, char *prmpt)
{
    register char *s;
    size_t spaces;

    menu_type = ptype_menu;
    if( menu_type.nitems <= 0 )
	    menu_digest(&menu_type);

    s = (unsigned)p->pt_type >= menu_type.nitems
	    ? "unknown" : menu_type.items[p->pt_type].name;
	spaces = strlen(prmpt) + 18;
    printf("%s:  type %-7.7s    block %7u,      %4u MB\n"
		"%*.*s len:  %7u blks, %4u MB\n",
	    prmpt, s,
	    p->pt_firstlbn, mbytes(p->pt_firstlbn), 
		spaces, spaces, "",
		p->pt_nblks, mbytes(p->pt_nblks));
}

/*
 * initialize default partition sizes and types.  make all partitions
 * be cyl-aligned; user can change later.  calculate sizes of root,
 * and swap partitions, based on a bounded percentage of
 * the total disk size.
 *
 * assumptions:
 * that the total disk size is greater than the sum of the minima.
 */
# define PCT	100
# define MB	((long)1<<20)

/*	NOTE: in the past, the min and max were in terms of sectors;
	now they are in bytes, because the sector size isn't known
	at compile time!  swapmin should be large enough for a miniroot,
	which means 28 MB currently.  rootmin should be large enough
	for a normal root, which means at least 18MB.

	swapmax is for the default create/part, not a real max.  I've
	bumped the default swap area to 128, but dropped the percent to
	10, and set the min to 39 (39 instead of 40, so I don't
	complain about older drives where the 40MB swap rounded down to
	39), so 200-400MB drives still get 40MB.  I also made swapmin
	32, as we had miniroots of 30MB for a while, and may again.
	rootmax changes to 50MB primarily for the high end, but rootpct
	drops to 2, so we keep it smaller for the smaller disks.
	rootmin is 18, we are marginal on 6.2 32 bit systems at 18, but
	we make it.  64 bit systems are marginal even at 25.  See the runtime
	setting of rootmin to 30 below for 64 bit systems.
	rootpct changes for 6.2 so we get ~21MB on 1GB
	disks, and 50MB on disks > 2GB, on the split / and /usr disks,
	at NSD's request.  Also see PTYPE_VOLHDR_DFLTSZ in dvh.h for
	volhdr size.
*/
int rootpct = 4; daddr_t rootmin = 18, rootmax = 50;
int swappct = 10; daddr_t swapmin = 39, swapmax = 128;

char whichparts[NPARTAB];

void
create_parts(void)
{
    register struct partition_table *p, *q;
    uint nblks, l, s;
    uint logblks;

	/* 64 bit systems have rootmin of 30MB now; 18 for others; 
	 * the tuning gives a larger value for all the drives we have
	 * shipped on those systems; this is primarily for the warning
	 * check at the end (same resetting also in pt_resize_func()).
	 * Not worth checking to see if we've already done it; just do it. */
#if defined(_STANDALONE) && _MIPS_SIM == _AIB64
    rootmin = 30;
#elif !defined(_STANDALONE)
    if (sysconf(_SC_KERN_POINTERS) == 64)
		rootmin = 30;
#endif

    bzero((char *)PT(&vh), sizeof PT(&vh));

    switch (drivernum) {

	case SCSI_NUMBER:
	/* because of the fixups in dksc.c to protect users from 'bad' vh's,
	 * we need to get the real # of cyls when creating default
	 * partitions.  Since this is the only way the vol pt can
	 * be created, we don't need to worry when editting partitions */
		{
		struct device_parameters d;
		scsiset_dp(&d);
		l = 0;
		}
		break;
#ifdef SMFD_NUMBER
	case SMFD_NUMBER:
		printf(noflopartmsg);
		return;
#endif /* SMFD_NUMBER */

	default:
		printf("Don't know how to initialize partitions for driver type %u\n",
			drivernum);
		return;
    }

	if(whichparts[PTNUM_VOLUME]) {
		p = PT(&vh)+PTNUM_VOLUME;
		p->pt_type = PTYPE_VOLUME;
		p->pt_firstlbn = 0;
		nblks = p->pt_nblks = DP(&vh)->dp_drivecap;
	}
	else
		nblks = DP(&vh)->dp_drivecap;

	if(whichparts[PTNUM_VOLHDR]) {
		p = PT(&vh)+PTNUM_VOLHDR;
		p->pt_type = PTYPE_VOLHDR;
		p->pt_firstlbn = 0;
		p->pt_nblks = PTYPE_VOLHDR_DFLTSZ;
		if(p->pt_nblks > nblks)
			p->pt_nblks = nblks;
		nblks -= p->pt_nblks;
	}

	q = PT(&vh)+PTNUM_VOLHDR;

	/* Number of blocks for xlv log partition.  */
	if (whichparts[PTNUM_XFSLOG])
		logblks = mbytetoblk(LOGSIZE);

	if(whichparts[PTNUM_FSALL]) {
		/* entire disk except vh and badblock (if any) */
		p = PT(&vh)+PTNUM_FSALL;
		p->pt_type = fstype;
		p->pt_firstlbn = q->pt_firstlbn + q->pt_nblks;
		p->pt_nblks = nblks;
		if (whichparts[PTNUM_XFSLOG] && logblks < p->pt_nblks)
			p->pt_nblks -= logblks;
	}

	/* follows volhdr now, not root */
	if(whichparts[PTNUM_FSSWAP]) {
		p = PT(&vh)+VP(&vh)->vh_swappt;
		p->pt_type = PTYPE_RAW;
		p->pt_firstlbn = q->pt_firstlbn+q->pt_nblks;
		l = (DP(&vh)->dp_drivecap / PCT) * swappct;
		if( l < (s = mbytetoblk(swapmin) + 1))
			l = s;
		if( l > (s = mbytetoblk(swapmax)))
			l = s;
		if( l > nblks )
			l = nblks;
		p->pt_nblks = l;
		if(p->pt_nblks < (mbytetoblk(swapmin) + 1))
			scerrwarn("swap partition will be too small (%lld blocks)\n", l);
		nblks -= l;
	}

	if(whichparts[PTNUM_FSROOT]) {
		/* root partition */
		q = PT(&vh)+VP(&vh)->vh_swappt;
		p = PT(&vh)+VP(&vh)->vh_rootpt;
		p->pt_type = fstype;
		p->pt_firstlbn = q->pt_firstlbn + q->pt_nblks;
		if(whichparts[PTNUM_FSUSR]) {	/* old style (pt_root_func) */
			l = (DP(&vh)->dp_drivecap / PCT) * rootpct;
			if( l < (s = mbytetoblk(rootmin) + 1))
				l = s;
			if( l > (s = mbytetoblk(rootmax)))
				l = s;
			if( l > nblks)
				l = nblks;
			p->pt_nblks = l;
			nblks -= l;
		}
		else {	/* new style (pt_sroot_func) */
			p->pt_nblks = nblks;
			nblks -= p->pt_nblks;
		}
		if(p->pt_nblks < (mbytetoblk(rootmin) + 1))
			scerrwarn("swap partition will be too small (%lld blocks)\n",
				p->pt_nblks);
	}


	if(whichparts[PTNUM_FSUSR]) {
		p = PT(&vh)+PTNUM_FSUSR;
		q = PT(&vh)+VP(&vh)->vh_rootpt; /* always follows root now */
		p->pt_type = fstype;
		p->pt_firstlbn = q->pt_firstlbn+q->pt_nblks;
		p->pt_nblks = nblks;
		if (whichparts[PTNUM_XFSLOG] && p->pt_nblks > logblks)
			p->pt_nblks -= logblks;
		nblks -= p->pt_nblks;
	}

	if(whichparts[PTNUM_XFSLOG]) {
		p = PT(&vh)+PTNUM_XFSLOG;
		if (whichparts[PTNUM_FSUSR])
			q = PT(&vh)+PTNUM_FSUSR;
		else
			q = PT(&vh)+PTNUM_FSALL;
		p->pt_type = PTYPE_XFSLOG;
		p->pt_firstlbn = q->pt_firstlbn + q->pt_nblks;
		if(logblks > nblks)
			logblks = nblks;
		p->pt_nblks = logblks;
		nblks -= logblks;	/* in case we add more parts later */
		q = PT(&vh)+PTNUM_XFSLOG;
	}

	changed = 1;
}

void
create_pt_func(void)
{
	bzero(whichparts, sizeof(whichparts));
	/* don't create part 7 any more, when it's brand new or when they ask
	 * for creat/part.  Some people then make a filesystem on part 7
	 * otherwise, leaving what looks like valid swap, and if they then
	 * try to install, they can trash the filesystem.  So now force them
	 * to use repart/option or the like, if they want that (which gets
	 * rid of part 1, etc. */
	whichparts[PTNUM_VOLUME] = whichparts[PTNUM_VOLHDR] =
		whichparts[PTNUM_FSROOT] = whichparts[PTNUM_FSSWAP] = 1;

	create_parts();
	printf("...created default partitions, use /repartition menu to change\n");
}


/*
 * refresh partition table from disk
 */
void
readin_pt_func(void)
{
    CBLOCK b;
#ifdef SMFD_NUMBER
	if(drivernum == SMFD_NUMBER) {
		printf(noflopartmsg);
		return;
	}
#endif

    printf("...reading in partition table\n");
    if( readinvh(VP(&b)) < 0 )
	return;
    bcopy((char *)PT(&b), (char *)PT(&vh), sizeof PT(&vh));
}

/*
 * check each partition for legality.
 * return <0 if any partition is found illegal, 0 if all OK.
 * check all, because partcheck prints a message on problems
 */
int
checkparts(void)
{
	register int i, illegal;
	register struct partition_table *p;

	p = PT(&vh);
	for(illegal = i = 0; i < NPARTAB; i++ )
		if(partcheck(i, p++))
			illegal++;

	return illegal;
}


/*
 * return <0 if the given partition is found bad, 0 if all OK.
 * a legal partition must:
 *	be of a legal size
 *	be of a legal type
 *	if root, of a FS type
 *	if swap, of RAW type
 *	if volume, must match device size.
 *	if fs or logv, must not intersect volhdr
 */ 
int
partcheck(uint n, struct partition_table *p)
{
    STRBUF s;
    register char *f;
    int ign, isfs;

# define PARTERROR(m)	(printf(f,m) , f = ", %s")

    f = s.c;
   
    isfs = isfstype(p->pt_type);
    sprintf(f, "warning: partition %u %cs", n, '%');

    ign = p->pt_nblks == 0 || p->pt_firstlbn == -1;
    if( !ign )
    {
    	if( isfs )
    	{
		if( intersect(p, PT(&vh)+PTNUM_VOLHDR) )
	    	PARTERROR("intersects volhdr partition");
    	}


    	if(p->pt_firstlbn > DP(&vh)->dp_drivecap)
		PARTERROR("base out of range");

    	if((p->pt_firstlbn+p->pt_nblks) > DP(&vh)->dp_drivecap)
		PARTERROR("size out of range");
    }

	/* don't complain if this isn't the root drive! */
    if( VP(&vh)->vh_swappt == n  && isrootdrive) {
		if(p->pt_nblks == 0)
			PARTERROR("(swap) is zero size");
		else if( ign || (!isfs && p->pt_type != PTYPE_RAW))
			PARTERROR("(swap) type must be raw, bsd, sysv, efs, xfs, lvol, rlvol, xlv, or xfslog");
	}

	/* don't complain if this isn't the root drive! */
    if( VP(&vh)->vh_rootpt == n  && isrootdrive) {
		if(p->pt_nblks == 0)
			PARTERROR("(root) is zero size");
		else if( ign || !isfs )
			PARTERROR("(root) type must be raw, bsd, sysv, efs, xfs, lvol, rlvol, xlv, or xfslog");
	}

    if( n == PTNUM_VOLHDR )
    {
	if( ign || !(p->pt_type == PTYPE_VOLHDR) )
	    PARTERROR("not of type volhdr");
	if((p->pt_firstlbn) )
	    PARTERROR("volhdr doesn't start at 0");
	if((p->pt_nblks <= 0) )
	    PARTERROR("volhdr is too small");
    }

    if( n == PTNUM_VOLUME )
    {
	if( ign || !(p->pt_type == PTYPE_VOLUME) )
	    PARTERROR("not of type volume");
	if((p->pt_firstlbn) )
	    PARTERROR("doesn't start at 0");
	if(p->pt_nblks > DP(&vh)->dp_drivecap)
	    PARTERROR("is larger than drive capacity");
    }

    if( f != s.c )
    {
	newline();
	return -1;
    }

    return 0;
# undef PARTERROR
}

int
isfstype(int t)
{
    return t==PTYPE_EFS || t==PTYPE_XFS || t==PTYPE_XFSLOG ||
           t==PTYPE_LVOL || t==PTYPE_RLVOL || t==PTYPE_XLV;
}


/*
 * return true if 2 partitions overlap
 */
intersect(struct partition_table *p1, struct partition_table *p2)
{
    /* beginning of p1 must be after end of p2 or vice versa */
    return p1->pt_nblks > 0 && p2->pt_nblks > 0
     && !(p1->pt_firstlbn >= (p2->pt_firstlbn+p2->pt_nblks)
            || p2->pt_firstlbn >= (p1->pt_firstlbn+p1->pt_nblks));
}


/* this is called whenever the repartition menu is displayed */
void
show_pt_cap(void)
{
	printf("\n");
	show_pt_func();

	newline();
	if(drivernum == SCSI_NUMBER)
		showcapacity_func();
	else
		printf("Drive capacity is %d blocks\n",
			PT(&vh)[PTNUM_VOLUME].pt_nblks);
}

static ITEM partitions[] = {
    { "root", PTNUM_FSROOT },
    { "swap", PTNUM_FSSWAP },
    { "usr", PTNUM_FSUSR },
    { "volume", PTNUM_FSALL },
    { "xfslog", PTNUM_XFSLOG },
    {0}
};


static char *
ptname(uint pnum)
{
    ITEM *p = partitions;
    static char pname[20];	/* large enough for bad vals of pnum */
    while(p->name) {
	if(p->value == pnum)
	    return p->name;
	p++;
    }
    sprintf(pname, "part #%d", pnum);
    return pname;
}


/* check to see if a changing partition now overlaps another partition.
 * If so, fix up the one that is NOT changing.  Check to see if it goes
 * to zero size, and ask about that special case before continuing.
 * sanity checks for min part sizes are done on return, so don't bother here.
*/
checkoverlap(uint pnum, struct volume_header *tmpvh, uint chkpnum)
{
    struct partition_table *pt, *chkpt;
    char msg[80];
    int adjust;

    pt = &tmpvh->vh_pt[pnum];
    chkpt = &tmpvh->vh_pt[chkpnum];
    if(!intersect(pt, chkpt)) {
	return 0;	/* OK */
    }
    if(pt->pt_firstlbn <= chkpt->pt_firstlbn &&
	(pt->pt_firstlbn+pt->pt_nblks) >= (chkpt->pt_firstlbn+chkpt->pt_nblks)) {
	/* changing partition completely contains checked partition */
	sprintf(msg, "This would remove the %s partition, is that OK",
	    ptname(chkpnum));
	if(no(msg))
		return 1;
	chkpt->pt_nblks = 0;
    }
    else if(pt->pt_firstlbn < chkpt->pt_firstlbn) {
	/* bump start of checked partition, and decrease size */
	adjust = (pt->pt_firstlbn+pt->pt_nblks) - chkpt->pt_firstlbn;
	chkpt->pt_firstlbn =  pt->pt_firstlbn + pt->pt_nblks;
	chkpt->pt_nblks -= adjust;
    }
    else	/* just drop the block count.
	 * Note that this catches the (shouldn't happen) case of the
	 * changing partition being complete contained in the checked
	 * partition.  We could figure out which end is bigger, but the
	 * user can always fix that up later. */
	chkpt->pt_nblks = pt->pt_firstlbn - chkpt->pt_firstlbn;
    return -1;
}

/* These 4 functions are all called from the repartition menu at the
 * top level, and are primarily intended to allow SIMPLE re-partitioning
 * without requiring much expertise.  Reasonable defaults are 
 * given, and things like pt_type are set automatically.
*/

#define P_MB	0
#define P_PCT	1
#define P_BLKS	2

#define PVH PT(&vh)

void
pt_resize_func(void)
{
	int pnum, method, pchanged, fixup;
	unsigned maxblocks, maxvolblks=0, newval, oldval;
	struct partition_table pt;
	struct volume_header oldvh;
	struct volume_header tmpvh;
	char promptmax[48];
	static ITEM how[] = { 
		{ "megabytes (2^20 Bytes)", P_MB },
		{ "percentage", P_PCT },
		{ "blocks", P_BLKS },
		{0}
	};
	static MENU how_menu = { how, "partitioning method" };
	static MENU part_menu = { partitions, "partition to change" };

	/* 64 bit systems have rootmin of 30MB now; 18 for others; 
	 * the tuning gives a larger value for all the drives we have
	 * shipped on those systems; this is primarily for the warning
	 * check at the end.  same code also in create_parts().
	 * Not worth checking to see if we've already done it; just do it. */
#if defined(_STANDALONE) && _MIPS_SIM == _AIB64
    rootmin = 30;
#elif !defined(_STANDALONE)
    if (sysconf(_SC_KERN_POINTERS) == 64)
		rootmin = 30;
#endif

	if((PVH[PTNUM_FSALL].pt_nblks<=0 || PVH[PTNUM_FSALL].pt_firstlbn < 0) &&
	  (PVH[PTNUM_FSROOT].pt_nblks<=0 || PVH[PTNUM_FSROOT].pt_firstlbn < 0 ||
	  PVH[PTNUM_FSSWAP].pt_nblks<=0 || PVH[PTNUM_FSSWAP].pt_firstlbn < 0))
	    printf(
		"This does not appear to be a standard root or option drive.\n"
		"You should probably use either the \"rootdrive\" or the\n"
		"\"optiondrive\" menu choice to create a standard drive\n"
		"partition layout before using this menu\n\n");

	reinst_warning();

	printf("\nAfter changing the partition, the other partitions will\n"
	    "be adjusted around it to fit the change.  The result will be\n"
	    "displayed and you will be asked whether it is OK, before the\n"
	    "change is committed to disk.  Only the standard partitions may\n"
	    "be changed with this function.  Type ? at prompts for a list\n"
	    "of possible choices\n\n");

	/* default to swap partition, if valid, otherwise entire.  CSD folks
	 * say most common reason to partition is to make swap larger.  Next
	 * most common is when setting up a new non-SGI drive as an additional
	 * drive. argchoice takes array index, and returns array index. */
	if(PVH[PTNUM_FSSWAP].pt_nblks<=0 || PVH[PTNUM_FSSWAP].pt_firstlbn<0)
		pnum = 3;	/* index from partition menu array */
	else
		pnum = 1;
	argchoice(&pnum, pnum, &part_menu, part_menu.title);

	pnum = partitions[pnum].value;
	pt = PVH[pnum];
	/* set type correctly before dumping the slice */
	switch(pnum) {
	case PTNUM_FSROOT:
	    pt.pt_type = fstype;
	    break;
	case PTNUM_FSSWAP:
	    pt.pt_type = PTYPE_RAW;
	    break;
	case PTNUM_FSUSR:
	    pt.pt_type = fstype;
	    break;
	case PTNUM_FSALL:
	    pt.pt_type = fstype;
	    break;
	case PTNUM_XFSLOG:
	    pt.pt_type = PTYPE_XFSLOG;
	}
	dumpslice(&pt, "current");

	if(drivernum == SCSI_NUMBER &&
	    scsi_readcapacity((uint*)&oldval) != -1)
	    /* use real max for scsi, so that all blocks can be
	     * used for variable geometry drives or zone sparing */
	    maxvolblks = oldval;
	else
	    maxvolblks = PVH[PTNUM_VOLUME].pt_nblks;

	if((int)maxvolblks == -1 || !maxvolblks) {
		/* defend against errors */
		printf("volume size not valid, can not proceed\n");
		return;
	}
	
	/* don't let the partitions creep down into the volhdr */
	maxblocks = maxvolblks - (PVH[PTNUM_VOLHDR].pt_nblks -
		PVH[PTNUM_VOLHDR].pt_firstlbn);

	/* if the partition wasn't previously used (as adding
	 * a user partition on an old option drive), ensure
	 * that the base address is at least > than the volhdr. */
	if(pt.pt_firstlbn < 
		(PVH[PTNUM_VOLHDR].pt_firstlbn + PVH[PTNUM_VOLHDR].pt_nblks))
		pt.pt_firstlbn = 
			PVH[PTNUM_VOLHDR].pt_firstlbn + PVH[PTNUM_VOLHDR].pt_nblks;

	argchoice(&method, P_MB, &how_menu, how_menu.title);

	/* note that we need to check that the user actually changed the
	 * value, otherwise with the conversion, we could wind up with
	 * a different partition layout, even if they just accepted the current
	 * value */
	pchanged = 0;
	switch(method) {
	case P_MB:
	    oldval = mbytes(pt.pt_nblks);
		sprintf(promptmax, "size in megabytes (max %u)",
			mbytes(maxblocks));
	    argnum(&newval, oldval, mbytes(maxblocks)+1, promptmax);
	    if(oldval != newval) {
		    newval = mbytetoblk(newval);
		    pchanged = 1;
	    }
	    break;
	case P_PCT:
	    oldval =  (100*pt.pt_nblks)/PVH[PTNUM_VOLUME].pt_nblks;
	    argnum(&newval, oldval,
		(100*maxblocks)/PVH[PTNUM_VOLUME].pt_nblks+1,
		"percentage of disk");
	    if(oldval != newval) {
		newval = (newval * PVH[PTNUM_VOLUME].pt_nblks) / 100;
		pchanged = 1;
	    }
	    break;
	case P_BLKS:
		sprintf(promptmax, "size in blocks (max %u)", maxblocks);
	    argnum(&newval, pt.pt_nblks, maxblocks+1, "size in blocks");
	    if(newval != pt.pt_nblks)
		pchanged = 1;
	    break;
	}

	if(!pchanged) {
	    printf("\nNo change made\n");
	    return;
	}

	/* due to rounding, converted value may be > than the max */
	pt.pt_nblks = newval > maxvolblks ? maxvolblks : newval;

	if((pt.pt_firstlbn + pt.pt_nblks) > maxvolblks)  /* bump down base ? */
	    pt.pt_firstlbn = maxvolblks - pt.pt_nblks;

	/* want to make all changes to a copy, in case they decide
	 * not to use the modified version */
	bcopy(&vh, &tmpvh, sizeof(tmpvh));
	bcopy(&pt, &tmpvh.vh_pt[pnum], sizeof(pt));

	/* check for overlaps, and fix up as needed.  Check in 'correct' order,
	 * as a fixup in the first may require a fixup in the second also.
	 * No check for entire, since we assume they are using the
	 * whole disk, or are using the leftover for some special
	 * purpose, but still set type.  If any checkoverlap made changes,
	 * except the first we have to iterate through again.
	 * we needlessly check the first one a second time, if it was
	 * the one with a fixup, but that is cheap, and it's cleaner
	 * to code this way. */
	do {
	    switch(pnum) {
	    case PTNUM_FSROOT:
		if(fixup=checkoverlap(pnum, &tmpvh, PTNUM_FSSWAP) == 1)
		    break;
		if((fixup = checkoverlap(pnum, &tmpvh, PTNUM_FSUSR)))
		    break;
		fixup = checkoverlap(pnum, &tmpvh, PTNUM_XFSLOG);
		break;
	    case PTNUM_FSSWAP:
		if((fixup = checkoverlap(pnum, &tmpvh, PTNUM_FSUSR)) == 1)
		    break;
		if((fixup = checkoverlap(pnum, &tmpvh, PTNUM_XFSLOG)))
		    break;
		fixup = checkoverlap(pnum, &tmpvh, PTNUM_FSROOT);
		break;
	    case PTNUM_FSUSR:
		if(fixup=checkoverlap(pnum, &tmpvh, PTNUM_FSSWAP) == 1)
		    break;
		if((fixup = checkoverlap(pnum, &tmpvh, PTNUM_FSROOT)))
		    break;
		fixup = checkoverlap(pnum, &tmpvh, PTNUM_XFSLOG);
		break;
	    case PTNUM_XFSLOG:
		if((fixup = checkoverlap(pnum, &tmpvh, PTNUM_FSUSR)) == 1)
		    break;
		if((fixup = checkoverlap(pnum, &tmpvh, PTNUM_FSALL)))
		    break;
		if((fixup = checkoverlap(pnum, &tmpvh, PTNUM_FSSWAP)))
		    break;
		fixup = checkoverlap(pnum, &tmpvh, PTNUM_FSROOT);
		break;
	    case PTNUM_FSALL:
		fixup = checkoverlap(pnum, &tmpvh, PTNUM_XFSLOG);
		break;
	    }
	} while(fixup == -1);
	if(fixup == 1)
		return;

	/* this can happen with SCSI drives where some of the capacity
	 * was unused, or sometimes when going from vh's done by older
	 * versions of fx.  It looks weird if a partition goes past the
	 * end of the whole vol, and may upset some drivers or programs
	 * like prtvtoc, so fix it up.
	*/
	if((tmpvh.vh_pt[pnum].pt_firstlbn + tmpvh.vh_pt[pnum].pt_nblks) >
		tmpvh.vh_pt[PTNUM_VOLUME].pt_nblks)
		tmpvh.vh_pt[PTNUM_VOLUME].pt_nblks = 
			tmpvh.vh_pt[pnum].pt_firstlbn + tmpvh.vh_pt[pnum].pt_nblks;

	show_pts(&tmpvh);
	/* check validity as root drive.  Don't check if FSALL.
	 * if root is very large, don't check for usr partition, since they
	 * probably made a single large partition, rather than a seperate
	 * root and user partition.  4*rootmax is a MINIMAL realistic
	 * value for this, it will probably be much larger */
	if((pnum != PTNUM_FSALL && tmpvh.vh_pt[PTNUM_FSALL].pt_nblks == 0) &&
	    (tmpvh.vh_pt[PTNUM_FSROOT].pt_nblks < mbytetoblk(rootmin) ||
	    tmpvh.vh_pt[PTNUM_FSSWAP].pt_nblks < mbytetoblk(swapmin) ||
	    (tmpvh.vh_pt[PTNUM_FSROOT].pt_nblks < mbytetoblk(4*rootmax) &&
	    tmpvh.vh_pt[PTNUM_FSUSR].pt_nblks < mbytetoblk(4*rootmax))))
	    printf("\nNotice: if this partition layout is used, the drive\n"
		"        may only be usable as a non-root drive due to small\n"
		"        partitions\n\n");

	if(no("Use the new partition layout"))
		return;

	bcopy(&vh, &oldvh, sizeof(vh));		/* Keep one in case of error */

	bcopy(&tmpvh, &vh, sizeof(tmpvh));

	if (update() != 0) {
		scerrwarn("label write failed!\n");

		bcopy(&oldvh, &vh, sizeof(vh));		/* Restore original */
	} else {

		/* suppress further partition warnings until exit or change
		 * to a new drive */
		majorchange = 1;
	}
}


#define PSROOT 0
#define PROOT 1
#define POPT 2

#define EXTERNAL 0
#define INTERNAL 1


static ITEM fstypes[] = {
    { "xfs", PTYPE_XFS },
    { "efs", PTYPE_EFS },
    {0}
};

static MENU fstype_menu = { fstypes, "type of data partition" };


/* 
 * Ask enough questions of the user to sufficiently determine partition
 * option. xfs or efs partition type? External xfslog partition required?
 */
void
select_options(int partition_option)
{
	int onum = 0;
	argchoice(&onum, onum, &fstype_menu, fstype_menu.title);
	fstype = fstypes[onum].value;

	switch (partition_option) {
	case	PSROOT:
		   pt_sroot_func();
		   break;
	case	PROOT:
		   pt_root_func();
		   break;
	case	POPT:
		   pt_optdrive_func();
		   break;
	}
	
}

void
root_select_func(void)
{
	int partition_option = PROOT;

	select_options(partition_option);
}

void
sroot_select_func(void)
{
	int partition_option = PSROOT;

	select_options(partition_option);
}

void
option_select_func(void)
{
	int partition_option = POPT;

	select_options(partition_option);
}

void
pt_optdrive_func(void)
{
	struct volume_header oldvh;


	reinst_warning();

	bcopy(&vh, &oldvh, sizeof(vh));		/* Keep one in case of error */

	bzero(whichparts, sizeof(whichparts));
	whichparts[PTNUM_VOLUME] = whichparts[PTNUM_VOLHDR] =
		whichparts[PTNUM_FSALL] = 1;
	create_parts();
	/* suppress further partition warnings until exit or change
	 * to a new drive */
	if(changed) {
		if (update() != 0) {
			scerrwarn("label write failed!\n");

			bcopy(&oldvh, &vh, sizeof(vh));	/* Restore original */

			changed = 0;
		} else {
			majorchange = 1;
		}
	}
}


/* makes the new style system disk (vh swap root) layout
 * as of irix6.5, swap moves ahead of root, so miniroot can
 * be copied to swap, and disk repartitioned while leaving the
 * miniroot intact.  For 5.3-6.4, it was "vh root swap"
*/
void
pt_sroot_func(void)
{
	struct volume_header oldvh;


	reinst_warning();

	bcopy(&vh, &oldvh, sizeof(vh));		/* Keep one in case of error */

	bzero(whichparts, sizeof(whichparts));
	whichparts[PTNUM_VOLUME] = whichparts[PTNUM_VOLHDR] =
		whichparts[PTNUM_FSROOT] = whichparts[PTNUM_FSSWAP] = 1;
	/* when creating a 'root' drive, force root and swap to
	 * the 'normal' locations; this is done primarily in case
	 * drive was in the middle of an install, when root is
	 * set to the swap partition. */
	VP(&vh)->vh_rootpt = PTNUM_FSROOT;
	VP(&vh)->vh_swappt = PTNUM_FSSWAP;
	create_parts();
	/* suppress further partition warnings until exit or change
	 * to a new drive */
	if(changed) {
		if (update() != 0) {
			scerrwarn("label write failed!\n");

			bcopy(&oldvh, &vh, sizeof(vh));	/* Restore original */

			changed = 0;
		} else {
			majorchange = 1;
		}
	}
}



/* makes the old style system disk (vh swap root usr) layout.
 * As of irix6.5, swap moves ahead of root, so miniroot can
 * be copied to swap, and disk repartitioned while leaving the
 * miniroot intact.  For 5.3-6.4, it was "vh root swap usr"
*/
void
pt_root_func(void)
{
	struct volume_header oldvh;

	reinst_warning();

	bcopy(&vh, &oldvh, sizeof(vh));		/* Keep one in case of error */

	bzero(whichparts, sizeof(whichparts));
	whichparts[PTNUM_VOLUME] = whichparts[PTNUM_VOLHDR] =
		whichparts[PTNUM_FSROOT] = whichparts[PTNUM_FSSWAP] =
		whichparts[PTNUM_FSUSR] = 1;
	/* when creating a 'root' drive, force root and swap to
	 * the 'normal' locations; this is done primarily in case
	 * drive was in the middle of an install, when root is
	 * set to the swap partition. */
	VP(&vh)->vh_rootpt = PTNUM_FSROOT;
	VP(&vh)->vh_swappt = PTNUM_FSSWAP;
	create_parts();
	/* suppress further partition warnings until exit or change
	 * to a new drive */
	if(changed) {
		if (update() != 0) {
			scerrwarn("label write failed!\n");

			bcopy(&oldvh, &vh, sizeof(vh));	/* Restore original */

			changed = 0;
		} else {
			majorchange = 1;
		}
	}
}


void
pt_expert_func(void)
{
	struct volume_header oldvh;


	reinst_warning();

	bcopy(&vh, &oldvh, sizeof(vh));		/* Keep one in case of error */

	set_pt_func();	/* same as /label/set/partition, but warn them first */
	/* suppress further partition warnings until exit or change
	 * to a new drive */
	if(changed) {
		if (update() != 0) {
			scerrwarn("label write failed!\n");

			bcopy(&oldvh, &vh, sizeof(vh));	/* Restore original */

			changed = 0;
		} else {
			majorchange = 1;
		}
	}
}

void
reinst_warning(void)
{
	/* If they are already committed, repeated warnings are annoying */
	if(majorchange || nomenus)
		return;

	if(!yesno(-1,
	"Warning: you will need to re-install all software and restore user data\n"
	"from backups after changing the partition layout.  Changing partitions\n"
	"will cause all data on the drive to be lost.  Be sure you have the drive\n"
	"backed up if it contains any user data.  Continue" ))
		mpop();
}
