#include "fx.h"
#include "dklabel.h"
#include <sys/termio.h>
#include <sys/buf.h>
#include <sys/iobuf.h>
#include <sys/elog.h>
#include <sys/dksc.h>	/* scsi.h included by dklabel.h */
#include <stddef.h>	/* for offsetof() */

typedef struct mode_sense_data sndata_t;
typedef struct mode_select_data sldata_t;

#define CDC_BUF_UNITS 256	/* divisor for buffer full/empty ratios
for most newer drives */

/* if geometry or format page can't be read, use these
 * values.  I did default to 32 and 1, since I saw this most often
 * on some vendors cd drives, but since number of cyls is a 16 bit
 * quantity, those values overflowed on drives with a capacity 
 * > 1 Gbytes, keeping you from using all of the drive.  This
 * will take us up to 16 Gb for faked drives
*/
#define DEF_SECSTRK 64
#define DEF_HEADS 8

/*	these pages are getting gross enough that the next time
	around, I probably ought to just make an array of 0x40
	of them.... */
sndata_t current_err_params;
sndata_t current_acc_params;
sndata_t current_geom_params;
sndata_t current_blk_params;
sndata_t current_cach_params;
sndata_t current_cach2_params;
sndata_t current_conn_params;
struct err_recov *current_err_page;
struct dev_format *current_acc_page;
struct dev_geometry *current_geom_page;
struct block_descrip *current_blk_page;
struct cachectrl *current_cach_page;
struct connparms *current_conn_page;
struct cachescsi2 *current_cach2_page;
struct common changeable[ALL+1];	/* for masking changeable pages */
struct common currentbits[ALL+1];	/* for masking changeable pages */
u_char pglengths[ALL+1];	/* lengths of each page for mode select,
	sense.  len of 0 means page not supported */

unsigned scsi_cap;	/* need it global now for readinvh() */

static unsigned bd_blklen;
static unsigned long sel_bits;

#ifdef SMFD_NUMBER
# include <sys/smfd.h>
#endif /* SMFD_NUMBER */

struct dev_fdgeometry *current_fdgeom_page;
sndata_t current_fdgeom_params;


/* len is length of additional data plus 4 byte sense header 
		plus 8 byte block descriptor + 2 byte page header */
#define SENSE_LEN_ADD (8+4+2)

#undef DEBUGMODE

static void maskch(u_char *, u_char *, u_char *, u_char);
static void scsi_dumppage(unsigned pg, u_char *pbuf, int hasblk);
static int scsi_modesense(sndata_t *addr, u_char pgcode);
static int scsi_modeXselect(sndata_t *addr, int verbose);
static int scsi_modeselect(sldata_t *addr, int verbose);
static void defsub(int ix, struct defect_entry *bp, char *tgt);
static void print_def_list(struct defect_entry *def_start, size_t defect_cnt, char *header, int log);
void scsi_getsupported(void);

static int istoshiba156;	/* hack hack */

/* used to convert all the two byte sequences from scsi modesense, etc.
 * to shorts for printing, etc. */
static ushort
sc_bytes_sh(u_char *a)
{
	return (((ushort)a[0])<<8) + (ushort)a[1];
}

/* used to convert all the 3 byte sequences from scsi modesense, etc.
 * to uints for printing, etc. */
static uint
sc_3bytes_uint(u_char *a)
{
	return (((uint)a[0])<<16) + (((uint)a[1])<<8) +  (uint)a[2];
}

/* used to convert all the 4 byte sequences from scsi modesense, etc.
 * to uints for printing, etc. */
static uint
sc_bytes_uint(u_char *a)
{
	return (((uint)a[0])<<24) + (((uint)a[1])<<16) + (((uint)a[2])<<8)
		+ (uint)a[3];
}


/* used to convert to the two byte sequences for scsi modesel, etc. */
static void
sc_sh_bytes(int from, u_char *to)
{
	to[0] = (u_char)(from >> 8);
	to[1] = (u_char)from;
}

/* used to convert to the three byte sequences for scsi modesel, etc. */
static void
sc_uint_3bytes(int from, u_char *to)
{
	to[0] = (u_char)(from >> 16);
	to[1] = (u_char)(from >> 8);
	to[2] = (u_char)from;
}


void
scsi_dname(char *s, int len)
{
	char p[SCSI_DEVICE_NAME_SIZE];

	istoshiba156 = 0;
	if(gioctl(DIOCDRIVETYPE, p) < 0) {
		scerrwarn("problem getting drivetype");
		*s = '\0';
	}
	else {	/* copy and ensure null-termination */
		if(len > sizeof(p))
			len =  sizeof(p) + 1;
		strncpy(s, p, len);
		s[len-1] = '\0';
		/* Toshiba can't handle the PF bit on mode selects; everyone
			else can, and some drives require it.  All of them allow
			saved params so far; WORMs, etc. may not, so this may need
			to be modified later. */
		if(bcmp("TOSHIBA MK156FB", p, 15) == 0) {
			istoshiba156 = 1;
			sel_bits = 1;
		}
#ifdef SMFD_NUMBER
		else if(drivernum == SMFD_NUMBER)
			sel_bits = 0;	/* floppies support neither */
#endif
		else
			sel_bits = 0x11;
		/* EINVAL means we are really running under 3.2, so don't complain */
		if(gioctl(DIOCSELFLAGS, (void *)sel_bits) < 0 && errno != EINVAL) {
			err_fmt("Warning: unable to set modeselect flags");
			sel_bits = 0;
		}
	}
}

/*	unfortunately, the time to format isn't a linear function of
	anything in particular; this is purely empirical.  We print
	the length of time because otherwise people might
	think the drive is hung, since nothing is printed.  Then they
	reset the system, and of course the drive is hosed until they
	re-format it, usually after calling the hotline.
	Now that we support different block sizes, adjust for that also.
*/

void
do_scsiformat(int noask)
{
	unsigned blksize=0, minit;

	if(scsi_readcapacity(&blksize) == -1 || !blksize) {
		/* make a reasonable guess... Happens sometimes
		when the drive was only partially formatted before being
		reset (or an earlier format failed) for some reason.  As
		with dksc driver, assume worst case for this. */
		minit = 480;
		goto doit;
	}

	/* allow for different block sizes when computing time.
	 * we are real close to overflowing 32 bits if we
	 * aren't careful, so divide first. */
	blksize = (blksize/DEV_BSIZE) * DP(&vh)->dp_secbytes;

	if(blksize < 1000)	/* 48 tpi floppies */
		minit = 1;
	else if(blksize < 3000)	/* 96 tpi floppies and most 3.5" */
		minit = 2;
	else if(blksize < 42000) /* 3.5" floptical */
	        minit = 25;
	else if(blksize < (1024*1024))
		/* less than .5 GB, allow fixed time */
	        minit = 45;
	else {
		/* aproximate as 50 MB/minute format time, round up to 5 min;
		 * formula still pretty much works for 9 and 18 GB drives, but
		 * increased the max... */
		minit = (blksize/102400);
		minit = (minit + 4) / 5;
		minit *= 5;
		if(minit > 600) minit = 600;
	}

doit:
	printf("format will take approximately %u minute%s ...\n",
	    minit, minit>1 ? "s" : "");
	printf(
	"\nA low level format is almost never necessary, and may cause drive\n"
	"\tproblems.  We suggest that you do not do a low level format\n"
	"\tunless the disk is completely unusable, or known to be damaged\n"
	"\tin a way that a low level format might fix.\n\n");
	if (blksize >= 38 * 1000 * 1000) /* >= 19GB capacity */
	printf("\n\t!!Warning!!  If the drive takes more than 8 hours to format, then\n"
	       "\tit may not finish before the format command times out.  Certain\n"
	       "\tdrives of over 18GB capacity are projected to require 8 hours,\n"
	       "\tand thus may not successfully complete a format with 'fx'.\n\n");
	flushoutp();  /* primarily for -c, but also for output to a pipe */
	if(!noask)
		lastchance();

	if (gioctl(DIOCFORMAT, 0) < 0)
		scerrwarn("format of drive failed");
	else
		printf("format completed successfully\n");
	/*	re-read params, capacity, etc. after formatting.  Some drives
		won't report back changed capacity, etc. after a modesel, until
		the format is done.  (Do even if format fails, since format
		might be done with default params instead of current). */
	scsiset_dp(DP(&vh));
	flushoutp();  /* primarily for -c, but also for output to a pipe */
}

static
scsi_modeselect(sldata_t *addr, int verbose)
{
	struct dk_ioctl_data modeselect_data;
	u_char pgcode, opgcode;

	/*	If the same data was last used for a sense, the high
		bit on the page code will be set.  Rather than fixing
		all possible callers, just ensure that it's not set. */
	opgcode = addr->dk_pages.common.pg_code;
	addr->dk_pages.common.pg_code &= ALL;
	pgcode = addr->dk_pages.common.pg_code;
	if(pglengths[pgcode] == 0) {
#ifdef DEBUGMODE
		err_fmt("page %d not supported for MODE SELECT\n",
		    pgcode);
#endif /* DEBUGMODE */
		return -1;
	}
	/* +4 for the header block, +2 for the pgcode */
	modeselect_data.i_len = pglengths[pgcode] + 4 + 2;
	modeselect_data.i_addr = (caddr_t)addr;

	/* mask off the non-changeable bits. etc.  */
	maskch(addr->dk_pages.common.pg_maxlen, changeable[pgcode].pg_maxlen,
		currentbits[pgcode].pg_maxlen, pglengths[pgcode]);
	/* no block descr to mask */
	if(verbose) {
		printf("will set:\n");
		scsi_dumppage(opgcode, (u_char *)addr, 0);
	}
	if (gioctl(DIOCSELECT, &modeselect_data) < 0) {
		if(sel_bits & 1)  {	/* maybe doesn't support saving;
			* can't count on bit in sense header since too many
			* SCSI 1 drives out there */
			sel_bits &= ~1;
			if(gioctl(DIOCSELFLAGS, (void *)sel_bits) == 0 &&
				gioctl(DIOCSELECT, &modeselect_data) == 0)
				return 0;
		}
		err_fmt("Warning: error setting drive parameters (page %d)",
		    pgcode);
		return -1;	/* caller should do a modesense again if
			this struct will be re-used; otherwise may
					print bogus info. */
	}
	return 0;
}

/*	doing a mode select in extended form with block desc.  Usually
	done with data from a mode sense.  Note that some bits that are
	valid in the header on a sense are reserved on a select (such
	as DPOFUA), so we always clear those.  This actually shows up
	on new scsi 2 drives, such as the ELITE 2. */
static
scsi_modeXselect(sndata_t *addr, int verbose)
{
	struct dk_ioctl_data modeselect_data;
	u_char *pgcode, opgcode;

	/*	The high bit on the page code will be set from the sense;
		clear here instead of all callers.  We have to do it this way
		because bd_len may be either 0 or 8... */
	pgcode = &((u_char *)addr)[offsetof(sndata_t, dk_pages) +
		offsetof(struct common, pg_code)];
	addr->dpofua = 0;	/* reserved on select */
	if(addr->bd_len==0)
		pgcode -= sizeof(addr->block_descrip);
	else if(bd_blklen)
		sc_uint_3bytes(bd_blklen, &addr->block_descrip[5]);
	opgcode = *pgcode;
	*pgcode &= ALL;

	/* shouldn't happen, but... (6 because of the 4 byte header, and
		2 bytes for pgcode and pglen */
	if(pglengths[*pgcode] == 0 || addr->sense_len<6) {
#ifdef DEBUGMODE
		err_fmt("page %d not supported for MODE SELECT\n",
		    *pgcode);
#endif /* DEBUGMODE */
		return -1;
	}
	modeselect_data.i_len = addr->sense_len+1;
	modeselect_data.i_addr = (caddr_t)addr;

	/* these are reserved when selecting */
	addr->reserv0 = addr->reserv1 = addr->sense_len = 0;
	/* setting these to 0 preserves old semantics; some drives allow
	 * some of them to be changed. */
	addr->mediatype = addr->wprot = addr->dpofua = 0;

	/* mask off the non-changeable bits. etc.  */
	maskch(addr->dk_pages.common.pg_maxlen, changeable[*pgcode].pg_maxlen,
		currentbits[*pgcode].pg_maxlen, pglengths[*pgcode]);
	if(verbose) {
		printf("will set:\n");
		scsi_dumppage(opgcode, (u_char *)addr, 1);
	}
	if(gioctl(DIOCSELECT, &modeselect_data) < 0) {
		if(sel_bits & 1)  {	/* maybe doesn't support saving;
			* can't count on bit in sense header since too many
			* SCSI 1 drives out there */
			sel_bits &= ~1;
			if(gioctl(DIOCSELFLAGS, (void *)sel_bits) == 0 &&
				gioctl(DIOCSELECT, &modeselect_data) == 0)
				return 0;
		}
		err_fmt("Warning: error setting drive parameters (page %d)",
		    *pgcode);
		return -1;
	}
	return 0;
}



struct defect_list {
	struct defect_header defect_header;
	struct defect_entry  defect_entry[8190];
};

/*
 * Get the current defect list.
 * Type is the type of defect list that should be retrieved.
 */
static
get_scsidefects(struct defect_list *addr, uint len, u_char def_type)
{
	struct dk_ioctl_data getdefects_data;

	getdefects_data.i_addr = (caddr_t)addr;
	getdefects_data.i_len = len;
	getdefects_data.i_page = def_type;

	if(gioctl(DIOCRDEFECTS, &getdefects_data) < 0) {
		if(errno != EINVAL || len <= MAX_IOCTL_DATA)
			err_fmt("Warning: error reading drive defects");
		/* else the 3.3 fx is being used on 3.2, so don't report */
		return -1;
	}
	return 0;
}

/* if log is non-zero, defects are shown as logical, not physical.
	Unfortunately, some mfg (such as Toshiba) return the phys
	format, even if you request log (which is ok by std), BUT
	the returned fmt type is log, NOT phys (which is NOT ok by std).
	(This was for drives made in 1988!)
	We used to default to CHS, we now (1997) default to logical.
	See showbb_func() in bb.c

	To print just grown, we try that first, and if it fails, we do
	it by getting all, then mfg, the eliminate mfg from entire
	list.  This is needed because many mfg's don't return just
	grown list.  Return 0 on success, 1 if error.

	logical value really means:
		-1 == byte from index, 1 == logical, 0 == c|h|s
*/
int
print_scsidefects(int logical, int listtype)
{
	struct defect_list def_list;
	size_t defect_cnt, cntdiv;
	int rtype, type;
	char *dtype;

	/*** NOTE: logical won't work right for Toshiba, and maybe others ***/
	if(logical == 0)
		type = 5;
	else if(logical == 1)
		type = 0;
	else if(logical == -1)
		type = 4;
	else {
		errwarn("(internal) unknown badblock format requested\n");
		type = 5;	/* physical supported by the most mfg */
	}

	if(listtype == 1) {	/* just mfg */
		type |= 2<<3;
		dtype = "manufacturers";
	}
	else if(listtype == 2) {	/* just grown */
		type |= 2<<2;
		dtype = "grown";
	}
	else {	/* combined, if neither set... */
		type |= 3<<3;
		dtype = "complete";
	}
	if (get_scsidefects(&def_list, sizeof(def_list), type))
		return 1;

	/* 	Check return type because drives (at least CDC Wren,
		and HItachi 514C) return their default format with
		a recovered error if given a type they don't support. */
	rtype = def_list.defect_header.format_bits;
	if(rtype != type) {
		printf("drive doesn't support requested badblock format\n");
		if(!(rtype&0x4))
			logical = 1;	/* logical block */
		else if((rtype&7) == 5)
			logical = 0;	/* physical: cyl/head/sec */
		else if((rtype&7) == 4)
			logical = -1;	/* byte from index: cyl/head/bytes */
		else {
			printf("returned type is a 'reserved' type, can't handle it\n");
			return 1;
		}
	}
	cntdiv = logical==1 ? sizeof(daddr_t) : sizeof(struct defect_entry);
	defect_cnt = sc_bytes_sh(def_list.defect_header.defect_listlen);
	defect_cnt /= cntdiv;
	if((defect_cnt*cntdiv) > sizeof(def_list)) {
		printf("claimed size of list (%d) is greater than requested, "
		       "probably not valid\n", defect_cnt);
		defect_cnt = sizeof(def_list) / cntdiv;
	}
	if(defect_cnt == 0 && rtype != type)
		return 1;	/* something wrong, don't try to print.
			* probably drive doesn't support format requested.
			* See bug #554444, and calling code in bb.c */
	print_def_list(def_list.defect_entry, defect_cnt,
	    dtype, logical);
	return 0;
}

/* if logical, just treat as array of 4 byte longs (msb first)
 * ix not used, but colprint passes it.
*/
/*ARGSUSED*/
void
deflogical( int ix, u_char *bp, char *tgt)
{
	u_long bn = sc_bytes_uint(bp);
	sprintf(tgt, "%lu", bn);
}

/* convert 4 bytes (either bytes to index, or sector in track) as returned
 * by the read defect command into a long */
uint
defbytes_to_uint(u_char *a)
{
	return sc_bytes_uint(a);
}

static
sort_log_def(const void *a, const void *b)
{
	return (int)(defbytes_to_uint((u_char *)a) - defbytes_to_uint((u_char *)b));
}

/* note that def_sector can be either a sector #, or bytes from index; so
 * for now no validity checking is done on it.
 * ix not used, but colprint passes it.
*/
/*ARGSUSED*/
static void
defsub(int ix, struct defect_entry *bp, char *tgt)
{
	u_int defect_cyl = sc_3bytes_uint(bp->def_cyl);
	uint secnum  = defbytes_to_uint(bp->def_sector);

	if(secnum == 0xffffffff)	/* whole track */
		sprintf(tgt, "(%u/track %d)", defect_cyl, bp->def_head);
	else
		sprintf(tgt, "(%u/%d/%u)", defect_cyl, bp->def_head, secnum);
}

static
sort_phys_def(const void *av, const void *bv)
{
	u_int cyla, cylb;
	struct defect_entry *a = (struct defect_entry *)av;
	struct defect_entry *b = (struct defect_entry *)bv;

 	cyla = sc_3bytes_uint(a->def_cyl);
	cylb = sc_3bytes_uint(b->def_cyl);
	if(cyla != cylb)
		return cyla - cylb;
	if(a->def_head != b->def_head)
		return a->def_head - b->def_head;
	return (int)(defbytes_to_uint(a->def_sector) - defbytes_to_uint(b->def_sector));
}

static void
print_def_list(struct defect_entry *def_start, size_t defect_cnt,
	char *header, int log)
{
	newline();
	if(!defect_cnt) {
		printf("No defects in %s list\n", header);
		return;
	}

	printf("total %d in %s defect list\n", defect_cnt, header);
	if(log>0) {
		printf("Format == logical block\n");
		/* sort so display looks 'better'.  Not all mfg return
			a sorted list */
		fxsort((char *)def_start, defect_cnt,
		    sizeof(daddr_t), sort_log_def);
		colprint(def_start, (int)defect_cnt, sizeof (daddr_t), 8, deflogical);
	}
	else {
		if(log == 0)
			printf("Format ==  (cylinder/head/sector) (physical, not logical)\n");
		else
			printf("Format ==  (cylinder/head/bytes from index) (physical, not logical)\n");
		/* sort so display looks 'better'.  Not all mfg return
			a sorted list */
		fxsort(def_start, defect_cnt,
		    sizeof(struct defect_entry), sort_phys_def);
		colprint(def_start, (int)defect_cnt, sizeof *def_start, 11, defsub);
	}
	printf("total %d in %s defect list\n", defect_cnt, header);
}

scsi_addbb(uint bn)
{
	u_char bbuf[MAX_BSIZE];	/* gwrite will use 'correct' secsz */
	if(gioctl(DIOCADDBB, (void *)(ulong)bn))
		return -1;
	/*	on at least some mfg's controllers (i.e. Toshiba 156FB),
		the sparing doesn't take effect until the block is
		re-written, so write a block to it.  This could be enhanced
		later to try to read the block first, in case error is
		soft. */
	TestPatFill(bbuf, DP(&vh)->dp_secbytes);
	return gwrite(bn, bbuf, 1);
}

static
scsi_modesense(sndata_t *addr, u_char pgcode)
{
	struct dk_ioctl_data modesense_data;

	modesense_data.i_page = pgcode;	/* before AND ! */
	pgcode &= ALL;
	if(!pglengths[pgcode]) {
#ifdef DEBUGMODE
		err_fmt("page 0x%xd not supported for MODE SENSE\n", pgcode);
#endif /* DEBUGMODE */
		return 1;
	}
	modesense_data.i_addr = (caddr_t)addr;

	/* len is length of additional data plus 4 byte sense header 
		plus 8 byte block descriptor + 2 byte page header */
	modesense_data.i_len = pglengths[pgcode] + SENSE_LEN_ADD;
	if(modesense_data.i_len > 0xff)	/* only one byte! (don't want modulo) */
		modesense_data.i_len = 0xff;

	if (gioctl(DIOCSENSE, &modesense_data) < 0) {
		/* don't complain if ALL, caller gives slightly different msg */
		if(pgcode != ALL)
			err_fmt("Warning: problem reading drive parameters (page %d)", pgcode);
		/* else get 'better' message in scsi_getsupported() */
		return 1;
	}
	return 0;
}

scsi_readcapacity(unsigned *addr)
{
	if (gioctl(DIOCREADCAPACITY, addr) < 0) {
		*addr = 0;	/* be paranoid */
		err_fmt("Error reading drive capacity");
		return -1;
	}
	/* always set drivecap when readcapacity succeeds, since pre-kudzu
	 * systems would not have filled it in, and besides, we use it in
	 * a number of places in fx, so we want it to be accurate.  Doesn't
	 * matter too much if it's different from what was on disk, and doesn't
	 * get written out */
	DP(&vh)->dp_drivecap = scsi_cap = *addr;
	return 0;
}



/*	called early from check_dp() to be sure we have all needed
	parameters. Also called after a 'set param'. It's really misnamed,
	in that it sets (initializes) the params used by fx, but doesn't
	set them for the device.
*/
void
scsiset_dp(struct device_parameters *dp)
{
	unsigned blksize;
	int flags, qdepth;

	scsi_getsupported();	/* find supported pages and lengths */
	get_scsi_param();
	blksize = 0;

	(void)scsi_readcapacity(&blksize);
	flags = dp->dp_flags & DP_CTQ_EN;
	qdepth = dp->dp_ctq_depth;
	bzero((char *)dp, sizeof *dp);
	dp->dp_drivecap = scsi_cap;

#ifdef SMFD_NUMBER
	if(current_fdgeom_page && (drivernum == SMFD_NUMBER ||
		(pglengths[DEV_FDGEOMETRY] && !pglengths[DEV_GEOMETRY]
		&& !pglengths[DEV_FORMAT]))) {
		if(drivernum != SMFD_NUMBER)
			errwarn("device appears to be a floppy\n");
		if(bd_blklen)
			dp->dp_secbytes = bd_blklen;
		else
			dp->dp_secbytes = sc_bytes_sh(current_fdgeom_page->g_bytes_sec);
		dp->dp_flags = flags;
	}
	else
#endif /* SMFD_NUMBER */
	/* Too many places in this block to prefix each of them... */
	if(pglengths[DEV_FORMAT] && current_acc_page &&
		pglengths[DEV_GEOMETRY] && current_geom_page) {
		/* protect against drives that don't implement these, and so
		 * (incorrectly IMHO) leave it 0.  This has actually been
		 * observed on some SCSI RAM disks. */
		if(!sc_bytes_sh(current_acc_page->f_trk_zone))
			sc_sh_bytes(1, current_acc_page->f_trk_zone);
		if(!sc_bytes_sh(current_acc_page->f_sec_trac)) {
			sc_sh_bytes(DEF_SECSTRK, current_acc_page->f_sec_trac);
			/* reasonable default; avoid divide by 0 */
			errwarn("drive reports 0 sectors per track, assume %d\n",
				sc_bytes_sh(current_acc_page->f_sec_trac));
		}

		if(!current_geom_page->g_nhead) {
			current_geom_page->g_nhead = 1;
			errwarn("drive reports 0 heads, assuming %d\n",
				current_geom_page->g_nhead);
		}

		dp->dp_flags = flags;

		if(!*LP(&sg)->d_name)	/* may not have it yet */
			scsi_dname(LP(&sg)->d_name, sizeof(LP(&sg)->d_name));
		if(bd_blklen)
			dp->dp_secbytes = bd_blklen;
		else if(istoshiba156 && pglengths[BLOCK_DESCRIP] &&
			current_blk_page && current_blk_page->b_blen)
			dp->dp_secbytes =  current_blk_page->b_blen;
		else if(pglengths[DEV_FORMAT] && current_acc_page)
			dp->dp_secbytes = sc_bytes_sh(current_acc_page->f_bytes_sec);
		else if(pglengths[DEV_FDGEOMETRY])
			dp->dp_secbytes = sc_bytes_sh(current_fdgeom_page->g_bytes_sec);
		/* this horrible hack is done only so if we setup a drive with
		 * kudzu, that it won't crash a pre-kudzu prtvtoc, or cause a
		 * pre-kudzu fx to bitch. Do not use these fields elsewhere */
		dp->_dp_cylinders = sc_bytes_sh(current_acc_page->f_alttrk_vol);
		dp->_dp_heads = current_geom_page->g_nhead;
		dp->_dp_sect = sc_bytes_sh(current_acc_page->f_sec_trac);
	}
	else {
		/* This shows up on things like CD-ROM.  This is sort of gross,
		* but works and avoids alarming people (not to mention later
		* coredumps in the partition code)...  */
		if(expert)
		  printf("Unable to get device geometry\n");
	}

	/* somewhat match old hack of couldn't get geometry */
	if(!dp->_dp_heads) dp->_dp_heads = DEF_HEADS;
	if(!dp->_dp_sect) dp->_dp_sect = DEF_SECSTRK;
	if(!dp->_dp_cylinders)
		dp->_dp_cylinders = scsi_cap/(dp->_dp_heads*dp->_dp_sect);

	dp->dp_ctq_depth = qdepth;
	if(!dp->dp_secbytes)
		dp->dp_secbytes = DEV_BSIZE;	/* best guess */
}


/* find supported pages and lengths */
void
scsi_getsupported(void)
{
	sndata_t data;
	register i, maxd, pgnum;
	u_char *d = (u_char *)&data;

	bzero(&data, sizeof(data));
	bzero(pglengths, sizeof(pglengths));	/* in case not first call */
	pglengths[ALL] = sizeof(data)-(1+SENSE_LEN_ADD);
	/* some devices, like maxtor optical won't return default if no media
		in drive (or unformatted), but will return current... This is
		supposed to be fixed in new firmware soon, but it doesn't cost
		anything but a kernel error message to try both. */
	if(scsi_modesense(&data, ALL|CURRENT) &&
	    scsi_modesense(&data, ALL|DEFAULT)) {
		pglengths[ALL] = 0;
		err_fmt("Unable to get list of supported configuration pages");
		return;
	}
	/* sense_len doesn't include itself; set pglengths[ALL] for completeness */
	pglengths[ALL] = maxd = data.sense_len + 1;
	if(data.bd_len > 7) {
		/* Note that bd_blklen is the *logical* block size, and if
		 * available, is preferred over the *physical* sector size
		 * in f_bytes_sec, which may be different.  (In fact, this
		 * is the case with syquest drives, and also with the
		 * Maxtor Tahiti II.)  */
		bd_blklen = sc_3bytes_uint(&data.block_descrip[5]);
		i = 4 + data.bd_len;	/* skip header and block descr */
	}
	else {
		bd_blklen = 0;	/* just in case we changed drives */
		i = 4;	/* just the header */
	}
	while(i < maxd) {
		pgnum = d[i] & ALL;
		pglengths[pgnum] = d[i+1];
		i += pglengths[pgnum] + 2;	/* +2 for header */
	}

	/*	now get all the changeable fields in the pages so they can be
		used as masks in modeselect.  Similarly for the current bits,
		so they can be OR'ed in wherever the changeable bits are 0
		NOTE: this could be a problem, since some drives want to see
		0 for non-changeable bits, and others want to see the current
		bits.  Neither SCSI1 nor SCSI2 are really quite clear what to
		do about this....
		Also note that the current bits don't really need to be re-obtained
		after each change, since theoretically, we are only using them for
		non-changeable values.  In fact, however, some non-changeable bits
		do get set as a side effect of other changes.  Thus it is a GOOD
		thing that this routine does in fact get called after each set
		of changes to the parameters is made.
	*/
	for(i=0; i< ALL; i++) {
		if(pglengths[i]) {
			if(scsi_modesense(&data, i|CHANGEABLE)==0)
				bcopy(&data.dk_pages.common, &changeable[i], pglengths[i]+2);
			else	/* probably a transient error of some kind, since they
				just told us they supported the page.... */
				printf("can't get changeable bits for page %x\n", i);
			if(scsi_modesense(&data, i|CURRENT)==0)
				bcopy(&data.dk_pages.common, &currentbits[i], pglengths[i]+2);
			else	/* probably a transient error of some kind, since they
				just told us they supported the page.... */
				printf("can't get current values for page %x\n", i);
		}
	}
}


/* create a 'standard' sgilabel for scsi drives */
void
scsiset_label(CBLOCK *sgijunk, int showname)
{
	static struct disk_label generic_scsi_disk;
	char name[SCSI_DEVICE_NAME_SIZE+1];	/* max len of inquiry string */

	generic_scsi_disk.d_magic = D_MAGIC;
	scsi_dname(name, sizeof (name));
	if(showname)
		printf("Scsi drive type == %s\n", name);
	else
		printf("...creating default sgiinfo\n");
#ifdef SMFD_NUMBER
	if(drivernum == SMFD_NUMBER)
		return;	/* no label on floppies */
#endif /* SMFD_NUMBER */
	strncpy(generic_scsi_disk.d_name, name, sizeof(name));
	bcopy((char *)&generic_scsi_disk, sgijunk, sizeof(generic_scsi_disk));
}

/* Show the drive non-geometry parameters, either current, saved,
 * mfg default, or changeable.  We show geometry and params
 * seperately, to be consistent with the 'set' menu.
 * All the data should be zeroed if the sense might fail, OR if
 * the page isn't supported.  Otherwise bogus values can
 * get printed. 
*/
static void
showparam(u_char pgtype, char *dp_header)
{
	sndata_t err_params;
	sndata_t cach_params;
	sndata_t cach2_params;
	sndata_t conn_params;
	sndata_t fdgeom_params;
	struct err_recov *err_page;
	struct cachectrl *cach_page;
	struct cachescsi2 *cach2_page;
	struct connparms *conn_page;
	struct dev_fdgeometry *fdgeom_page;

	/*	bzero all the param blocks first, because sometimes earlier
		firmware revs didn't support all the fields (and return less
		data), and we sometimes need to check for non-zero fields.
	*/
	bzero(&err_params, sizeof(err_params));
	bzero(&conn_params, sizeof(conn_params));
	bzero(&cach2_params, sizeof(cach2_params));
	bzero(&cach_params, sizeof(cach_params));
	bzero(&fdgeom_params, sizeof(fdgeom_params));

	if(pglengths[DEV_FDGEOMETRY]) {	/* floppy */
		/* need this for params, because for floppies, some of
		 * the non-geometry info is in this page */
		scsi_modesense(&fdgeom_params, pgtype | DEV_FDGEOMETRY);
		fdgeom_page = (struct dev_fdgeometry *)(fdgeom_params.block_descrip +
			fdgeom_params.bd_len);
	}
	if(pglengths[CACHE_SCSI2]) {	/* prefer 'standard' if avail */
		scsi_modesense(&cach2_params, pgtype | CACHE_SCSI2);
		cach2_page = (struct cachescsi2 *)
			(cach2_params.block_descrip +
			cach2_params.bd_len);
	}
	else
		cach2_page = (struct cachescsi2 *)cach2_params.block_descrip;
	if(pglengths[CACHE_CONTR]) {	/* BUT, always do this
		if supported, because some CDC/Imprimis/Seagate
		drives need both bits set, and there is also c_ccen */
		scsi_modesense(&cach_params, pgtype | CACHE_CONTR);
		cach_page = (struct cachectrl *)(cach_params.block_descrip +
			cach_params.bd_len);
	}
	else
		cach_page = (struct cachectrl *)cach_params.block_descrip;
	if(pglengths[CONN_PARMS]) {
		scsi_modesense(&conn_params, pgtype | CONN_PARMS);
		conn_page = (struct connparms *)(conn_params.block_descrip +
			conn_params.bd_len);
	}
	else
		conn_page = (struct connparms *)conn_params.block_descrip;

	if(pglengths[ERR_RECOV]) {
		scsi_modesense(&err_params, pgtype | ERR_RECOV);
		err_page = (struct err_recov *)(err_params.block_descrip +
		    err_params.bd_len);
	}
	else
		err_page = (struct err_recov *)err_params.block_descrip;

	setoff("%s%s", dp_header, pgtype==CHANGEABLE?"":" parameters");
	if(pgtype == CHANGEABLE) {	/* no graceful way to get this simpler */
		printf("error correction             %x     ",
		    (err_page->e_err_bits & E_DCR));
		printf("data transfer on error              %x\n",
		    (err_page->e_err_bits & E_DTE) ? 1 : 0);
		printf("report recovered errors      %x     ",
		    (err_page->e_err_bits & E_PER) ? 1 : 0);
		printf("delay for error recovery            %x\n",
		    (err_page->e_err_bits & E_RC) ? 1 : 0);
		printf("transfer bad data blocks     %x     ",
		    (err_page->e_err_bits & E_TB) ? 1 : 0);
		printf("Error retry attempts             %4x\n",
		    err_page->e_retry_count);
		printf("auto bad block reallocation (write) %x\n",
		    (err_page->e_err_bits & E_AWRE) ? 1 : 0);
		printf("auto bad block reallocation (read)  %x\n",
		    (err_page->e_err_bits & E_ARRE) ? 1 : 0);
		if(pglengths[CACHE_SCSI2]) {	/* prefer scsi2 way */
			printf("Drive read-ahead          %4x     ",
			    cach2_page->c_rcd ? 1 : 0);
			printf("Drive buffered writes            %4x\n",
			    cach2_page->c_wce ? 1 : 0);
			printf("Drive disable prefetch    %4x     ",
			    sc_bytes_sh(cach2_page->c_predislen));
			printf("Drive minimum prefetch           %4x\n",
			    sc_bytes_sh(cach2_page->c_minpre));
			printf("Drive maximum prefetch    %4x     ",
			    sc_bytes_sh(cach2_page->c_maxpre));
			printf("Drive prefetch ceiling           %4x\n",
			    sc_bytes_sh(cach2_page->c_maxpreceil));
			printf("Number of cache segments    %2x\n",
			    cach2_page->c_numseg);
		}
		else if(pglengths[CACHE_CONTR])
			printf("Drive read-ahead             %x\n",
			       cach_page->c_ce);
		if(pglengths[CONN_PARMS]) {
			printf("Read buffer ratio         %4x     ",
			       conn_page->c_bfull);
			printf("Write buffer ratio               %4x\n",
			       conn_page->c_bempty);
		}
	}
	else {
		printf("Error correction %s         ", 
		    (err_page->e_err_bits & E_DCR) ? "disabled" : "enabled ");
		printf("%s data transfer on error\n",
		    (err_page->e_err_bits & E_DTE) ? "Disable" : "Enable");
		printf("%s report recovered errors     ",
		    (err_page->e_err_bits & E_PER) ? "Do" : "Don't");
		if(err_page->e_err_bits & E_PER) printf("   ");
		printf("%s delay for error recovery\n",
		    (err_page->e_err_bits & E_RC) ? "Don't" : "Do");
		printf("%s transfer bad blocks         ",
		    (err_page->e_err_bits & E_TB) ? "Do" : "Don't");
		if(err_page->e_err_bits & E_TB) printf("   ");
		printf("Error retry attempts          %2d\n",
		    err_page->e_retry_count);
		printf("%s auto bad block reallocation (read)\n",
		    (err_page->e_err_bits & E_ARRE) ? "Do" : "No");
		printf("%s auto bad block reallocation (write)\n",
		    (err_page->e_err_bits & E_AWRE) ? "Do" : "No");
		if(pglengths[CACHE_SCSI2]) {	/* prefer scsi2 way */
			printf("Drive readahead %sabled          ",
			    cach2_page->c_rcd ? "dis" : " en");
			printf("Drive buffered writes %sabled\n",
			    cach2_page->c_wce ? " en" : "dis");
			printf("Drive disable prefetch   %5u    ",
			    sc_bytes_sh(cach2_page->c_predislen));
			printf("Drive minimum prefetch     %5u\n",
			    sc_bytes_sh(cach2_page->c_minpre));
			printf("Drive maximum prefetch   %5u    ",
			    sc_bytes_sh(cach2_page->c_maxpre));
			printf("Drive prefetch ceiling     %5u\n",
			    sc_bytes_sh(cach2_page->c_maxpreceil));
			printf("Number of cache segments  %4u\n",
			    cach2_page->c_numseg ? cach2_page->c_numseg : 1);
		}
		else if(pglengths[CACHE_CONTR])
			printf("Drive read-ahead %sabled\n",
			    cach_page->c_ce ? " en" : "dis");
		if(pglengths[CONN_PARMS]) {
			printf("Read buffer ratio      %3d/%d",
			    conn_page->c_bfull, CDC_BUF_UNITS);
			printf("    Write buffer ratio       %3d/%d\n",
			    conn_page->c_bempty, CDC_BUF_UNITS);
		}
		printf("Command Tag Queueing ");
		if (DP(&vh)->dp_flags & DP_CTQ_EN)
			printf("enabled, maximum depth %3u\n",
			       DP(&vh)->dp_ctq_depth > 0 ? DP(&vh)->dp_ctq_depth : 255);
		else
			printf("disabled\n");
	}
	newline();

	if(pglengths[DEV_FDGEOMETRY]) {	/* floppy */
		char *f;
		int n;
		if(pgtype == CHANGEABLE)
			f = "%17s = %4x";
		else
			f = "%17s = %4d";
		printf(f, "Spinup (.1 sec)",
			fdgeom_page->g_moton);
		printf(f, "Spindown on idle",
			fdgeom_page->g_motoff);
		n = sc_bytes_sh(fdgeom_page->g_headset);
		/* assume 500Kbits/sec transfer rate */
		if(pgtype != CHANGEABLE)
			n /= 10;	/* else show all changeable bits */
		printf(f, " Head settle (ms)", n);
		newline();
	}
}

/* Show the drive geometry parameters, either current, saved,
 * mfg default, or changeable.  We show geometry and params
 * seperately, to be consistent with the 'set' menu.
 * All the data should be zeroed if the sense might fail, OR if
 * the page isn't supported.  Otherwise bogus values can
 * get printed. 
*/
static void
showgeom(u_char pgtype, char *dp_header)
{
	char *f;
	int n;
	sndata_t acc_params;
	sndata_t geom_params;
	sndata_t fdgeom_params;
	struct dev_format *acc_page;
	struct dev_geometry *geom_page;
	struct dev_fdgeometry *fdgeom_page;

	/*	bzero all the param blocks first, because sometimes earlier
		firmware revs didn't support all the fields (and return less
		data), and we sometimes need to check for non-zero fields.
	*/
	bzero(&geom_params, sizeof(geom_params));
	bzero(&acc_params, sizeof(acc_params));
	bzero(&fdgeom_params, sizeof(fdgeom_params));

	if(pglengths[DEV_FDGEOMETRY]) {	/* floppy */
		scsi_modesense(&fdgeom_params, pgtype | DEV_FDGEOMETRY);
		fdgeom_page = (struct dev_fdgeometry *)(fdgeom_params.block_descrip +
			fdgeom_params.bd_len);
	}

	if(pglengths[DEV_FORMAT]) {
		scsi_modesense(&acc_params, pgtype | DEV_FORMAT);
		acc_page = (struct dev_format *)(acc_params.block_descrip +
			acc_params.bd_len);
	}
	else if(!pglengths[DEV_FDGEOMETRY]) {	/* only for non-floppy */
		acc_page = (struct dev_format *)acc_params.block_descrip;
		/* set these to avoid divide by 0 errors later; some drives,
		such as Sony SMO-C501 optical don't support this page... */
		sc_sh_bytes(1, acc_page->f_trk_zone);
		sc_sh_bytes(1, acc_page->f_interleave);
		sc_sh_bytes(DEF_SECSTRK, acc_page->f_sec_trac);
		if(expert)
		  printf("Can't get drive geometry, assuming %d sectors/track\n",
			sc_bytes_sh(acc_page->f_sec_trac));
	}
	if(pglengths[DEV_GEOMETRY]) {
		scsi_modesense(&geom_params, pgtype | DEV_GEOMETRY);
		geom_page = (struct dev_geometry *)(geom_params.block_descrip +
			geom_params.bd_len);
	}
	else if(!pglengths[DEV_FDGEOMETRY]) {	/* only for non-floppy */
		unsigned blksize;
		geom_page = (struct dev_geometry *)geom_params.block_descrip;
		/*	set these to avoid divide by 0 errors later; some drives,
		such as Sony SMO-C501 optical don't support his page...
		SCSI-2 says we should be able to get some of this info from
		the density code in the param descr block, but the Sony
		doesn't give us a descr block... */
		geom_page->g_nhead = DEF_HEADS;
		if(expert)
		   printf("Can't get drive geometry, assuming %d heads\n", DEF_HEADS);
		/* do a readcapacity and then fake sec_trk and cyls */
		if(scsi_readcapacity(&blksize) == 0) {
			blksize /= sc_bytes_sh(acc_page->f_sec_trac) * DEF_HEADS;
			sc_uint_3bytes(blksize, geom_page->g_ncyl);
		}
		else	/* set to single cyl to avoid divide by zero problems later */
			sc_uint_3bytes(1, geom_page->g_ncyl);
	}

	setoff("%s%s", dp_header, pgtype==CHANGEABLE?"":" geometry");

	if(pgtype == CHANGEABLE)
		f = "%17s = %4x";
	else
		f = "%17s = %4d";

	/*	set up so that 'related' items follow each other in
		column's, not rows */
	if(!pglengths[DEV_FDGEOMETRY]) {	/* non-floppy */
		printf(f, "Tracks/zone", sc_bytes_sh(acc_page->f_trk_zone));
		printf(f, "Sect/track", sc_bytes_sh(acc_page->f_sec_trac));
		newline();
		printf(f, "Alt sect/zone", sc_bytes_sh(acc_page->f_altsec));
		printf(f, "Interleave", sc_bytes_sh(acc_page->f_interleave));
		printf(f, "Cylinders", sc_3bytes_uint(geom_page->g_ncyl));
		newline();
		printf(f, "Alt track/volume", sc_bytes_sh(acc_page->f_alttrk_vol));
		printf(f, "Cylinder skew", sc_bytes_sh(acc_page->f_cylskew));
		printf(f, "Heads", geom_page->g_nhead);
		newline();
		printf(f, "Alt track/zone", sc_bytes_sh(acc_page->f_alttrk_zone));
		printf(f, "Track skew", sc_bytes_sh(acc_page->f_trkskew));
		n = 0;	/* for check below */
		if(bd_blklen)
			n = bd_blklen;	/* use value from block descr */
		else if(istoshiba156 && pglengths[BLOCK_DESCRIP] &&
			current_blk_page && current_blk_page->b_blen) {
			n = current_blk_page->b_blen;
		}
		else if(pglengths[DEV_FORMAT] && sc_bytes_sh(acc_page->f_bytes_sec))
			n = sc_bytes_sh(acc_page->f_bytes_sec);
		if(!n) { /* make it obvious if we don't know */
			if(DP(&vh)->dp_secbytes) {
				n = DP(&vh)->dp_secbytes;
			}
			else	/* duplicate code in scsiset_dp */
				n = DP(&vh)->dp_secbytes = DEV_BSIZE;	/* best guess */
			if(expert)
				printf("  bytes/sec UNKNOWN, using %d", n);
			else
				printf(f, "Data bytes/sec", n);
		}
		else
			printf(f, "Data bytes/sec", n);

		if(geom_page->g_rotatrate[0] || geom_page->g_rotatrate[1]) {
			newline();
			printf(f, "Rotational rate", sc_bytes_sh(geom_page->g_rotatrate));
		}
	}
	else { 	/* floppy */
		printf(f, "Sectors/Track", fdgeom_page->g_spt);
		printf(f, "Steps/Cylinder", fdgeom_page->g_stpcyl);
		newline();

		printf(f, "Cylinders", sc_bytes_sh(fdgeom_page->g_ncyl));
		printf(f, "Heads", fdgeom_page->g_nhead);
		newline();
		printf(f, "Block len", sc_bytes_sh(fdgeom_page->g_bytes_sec));
	}
	newline();
}

static void
scsi_showdata(void (*showfunc)(u_char, char *))
{
	char flags[4];

	checkflags("cmds", flags);

	if(flags[1])
	    (*showfunc)(CHANGEABLE, "modifiable fields: fields that are 0 aren't changeable");
	else if(flags[2])
		(*showfunc)(DEFAULT, "drive manufacturer's default");
	else if(flags[3]) {
#ifdef SMFD_NUMBER
		if(drivernum == SMFD_NUMBER) {
			printf("no saved parameters for floppy disks\n");
			return;
		}
#endif
		(*showfunc)(SAVED, "saved drive");
	}
	else /* 'c' or none */
		(*showfunc)(CURRENT, "current drive");
}


/* show non-geometry parameters, defaulting to current
* values, but optionally allowing the others */
void
scsi_showparam_func(void)
{
	scsi_showdata(showparam);

}

/* show geometry parameters, defaulting to current
* values, but optionally allowing the others */
void
scsi_showgeom_func(void)
{
	scsi_showdata(showgeom);
}


ITEM	set_ecc_items[] = {
	{"enabled", 0},
	{"disabled", 1},
	{0},
};

ITEM	set_ecc_items_bar[] = {
	{"disabled", 0},
	{"enabled", 1},
	{0},
};

MENU	set_ecc_menu = {
	set_ecc_items, "ecc parameters"};
MENU	set_ecc_menu_bar = {
	set_ecc_items_bar, "ecc parameters"};

/* set geometry drive parameters.  called via scsidata.M */
void
scsi_setgeom_func(void)
{
	int current_val;
	uint n;
	STRBUF s;
	sldata_t geom_params;
	sndata_t acc_params;
	int geomchange = 0;

	sprintf(s.c,"NOTE: you will need to reformat the disk after changing\n"
	"the drive geometry.  This will cause all data on the drive\n"
	"to be lost.  Continue");

	/* warn them they will have to reformat, unless they are already
	 * committed to it.  If they are already committed, repeated
	 * warnings are annoying */
	if(!formatrequired && !yesno(-1, s.c) )
		return;

	if(!current_geom_page)	/* haven't read current yet! */
		readin_dp();

	bzero(&geom_params, sizeof(geom_params));

	/* bcopy from current to get the block descr filled in */
	if(current_acc_params.bd_len)
		bcopy(&current_acc_params, &acc_params,
		    pglengths[DEV_FORMAT] + 4 + 2);
	else
		bzero(&acc_params, sizeof(acc_params));

	if(pglengths[DEV_GEOMETRY] && pglengths[DEV_FORMAT]) {
		/* really need both, or have huge number of checks
		 * in this block.  check in case fd opened as dksc */
	/* max # of tracks / zone is number of tracks on drive */
	n = sc_3bytes_uint(current_geom_page->g_ncyl);
	if(!current_geom_page->g_nhead) {
		current_geom_page->g_nhead = 1;
		errwarn("drive reports 0 heads, assuming %d\n",
			current_geom_page->g_nhead);
	}
	n *= current_geom_page->g_nhead;
	argnum(&n, sc_bytes_sh(current_acc_page->f_trk_zone), n+1, "Tracks/zone");
	if(sc_bytes_sh(current_acc_page->f_trk_zone) != n) {
		geomchange = 1;
		sc_sh_bytes(n, current_acc_page->f_trk_zone);
	}

	/* if they one more than a tracks worth, they should use trk_zone */
	argnum(&n, sc_bytes_sh(current_acc_page->f_altsec),
		sc_bytes_sh(current_acc_page->f_sec_trac), "Alt sect/zone");
	if(sc_bytes_sh(current_acc_page->f_altsec) != n) {
		geomchange = 1;
		sc_sh_bytes(n, current_acc_page->f_altsec);
	}

	argnum(&n, sc_bytes_sh(current_acc_page->f_alttrk_zone),
		sc_bytes_sh(current_acc_page->f_trk_zone)-1, "Alt track/zone");
	if(sc_bytes_sh(current_acc_page->f_alttrk_zone) != n) {
		geomchange = 1;
		sc_sh_bytes(n, current_acc_page->f_alttrk_zone);
	}

	argnum(&n, sc_bytes_sh(current_acc_page->f_alttrk_vol),
		current_geom_page->g_nhead*100, "Alt track/volume");
	if(sc_bytes_sh(current_acc_page->f_alttrk_vol) != n) {
		geomchange = 1;
		sc_sh_bytes(n, current_acc_page->f_alttrk_vol);
	}

	argnum(&n, sc_bytes_sh(current_acc_page->f_trkskew),
		sc_bytes_sh(current_acc_page->f_sec_trac), "Track Skew");
	if(sc_bytes_sh(current_acc_page->f_trkskew) != n) {
		geomchange = 1;
		sc_sh_bytes(n, current_acc_page->f_trkskew);
	}

	argnum(&n, sc_bytes_sh(current_acc_page->f_cylskew),
		sc_bytes_sh(current_acc_page->f_sec_trac), "Cylinder Skew");
	if(sc_bytes_sh(current_acc_page->f_cylskew) != n) {
		geomchange = 1;
		sc_sh_bytes(n, current_acc_page->f_cylskew);
	}

	if(bd_blklen)
		current_val = bd_blklen;	/* use value from block descr */
	else if(istoshiba156 && pglengths[BLOCK_DESCRIP] &&
		current_blk_page && current_blk_page->b_blen)
		current_val = current_blk_page->b_blen;
	else if(pglengths[DEV_FORMAT] && sc_bytes_sh(current_acc_page->f_bytes_sec))
		current_val = sc_bytes_sh(current_acc_page->f_bytes_sec);
	else	/* best guess... */
		current_val = DEV_BSIZE;

	argnum(&n, current_val, MAX_BSIZE+1, "Data bytes/sec");
	/*	some drives, such as Imprimis/Seagate only allow you to change
		the block size by changing the block descr.  Thus we do
		a modeXselect for the acc page, and set it in the block
		descr, as well as putting it in f_bytes_sec, which may
		not be entirely correct (see bd_blklen disccusion in 
		scsi_getsupported()) */
	if(current_val != n) {
		geomchange = 1;
		sc_sh_bytes(n, current_acc_page->f_bytes_sec);
		bd_blklen = n;
	}

	}
#ifdef SMFD_NUMBER
	else if(pglengths[DEV_FDGEOMETRY] && current_fdgeom_page) {
		long on;
		on = n = sc_bytes_sh(current_fdgeom_page->g_bytes_sec);
		argnum(&n, n, 0x10000, "bytes/sect");
		if(on != n) {
			geomchange = 1;
			sc_sh_bytes(n, current_fdgeom_page->g_bytes_sec);
		}

		on = n = sc_bytes_sh(current_fdgeom_page->g_ncyl);
		argnum(&n, n, 0x10000, "Cylinders");
		if(on != n) {
			geomchange = 1;
			sc_sh_bytes(n, current_fdgeom_page->g_ncyl);
		}

		argnum(&n, current_fdgeom_page->g_nhead, 0xff, "Heads");
		if(current_fdgeom_page->g_nhead != n) {
			geomchange = 1;
			current_fdgeom_page->g_nhead = n;
		}

		argnum(&n, current_fdgeom_page->g_spt, 0xff, "Sectors/Track");
		if(current_fdgeom_page->g_spt != n) {
			geomchange = 1;
			current_fdgeom_page->g_spt = n;
		}
	}
#endif /* SMFD_NUMBER */
	else {
		printf("Can't get any geometry info, sorry\n");
		return;
	}

	lastchance_dp();
	if(geomchange)
	    formatrequired = 1;

	if(pglengths[DEV_FORMAT]) {
		bcopy(current_acc_page, &acc_params.dk_pages.dev_format, 
		    pglengths[DEV_FORMAT] + 4 + 2);
		acc_params.bd_len = 8;
		acc_params.sense_len = 13 + pglengths[DEV_FORMAT];	/* 8 + 6 - 1 */
		/*	Xselect because we want to set the block len in the
			block descr.  Force use of descr even if sense
			didn't return it.  */
		scsi_modeXselect(&acc_params, 0);
	}
	if(pglengths[DEV_GEOMETRY]) {
		bcopy(current_geom_page, &geom_params.dk_pages.dev_geometry, 
		    pglengths[DEV_GEOMETRY] + 4 + 2);
		scsi_modeselect(&geom_params, 0);
	}
#ifdef SMFD_NUMBER
	if(drivernum == SMFD_NUMBER && pglengths[DEV_FDGEOMETRY]) {
		sldata_t fdgeom_params;
		bzero(&fdgeom_params, sizeof(fdgeom_params));

		/* NCR won't accept media type of 0 as being current... */
		fdgeom_params.mediatype = current_fdgeom_params.mediatype;

		bcopy(current_fdgeom_page,
		    &fdgeom_params.dk_pages.dev_fdgeometry, 
		    pglengths[DEV_FDGEOMETRY] + 4 + 2);
		scsi_modeselect(&fdgeom_params, 0);
		scsi_modesense(&current_fdgeom_params, DEV_FDGEOMETRY|CURRENT);
	}
#endif /* SMFD_NUMBER */

	if(pglengths[DEV_GEOMETRY])
		scsi_modesense(&current_geom_params, DEV_GEOMETRY|CURRENT);
	if(pglengths[DEV_FORMAT])
		scsi_modesense(&current_acc_params, DEV_FORMAT|CURRENT);

	/* and now set the values in the vh */
	scsiset_dp(DP(&vh));
}

/* set non-geometry drive parameters.  called via scsidata.M */
void
scsi_setparam_func(void)
{
	int ecc_bit;
	int current_val;
	uint n;
	sndata_t err_params;
	sndata_t acc_params;
	sldata_t cach_params;
	sldata_t cach2_params;
	sldata_t conn_params;
	sldata_t fdgeom_params;

	if(!current_err_page)	/* haven't read current yet! */
		readin_dp();

	bzero(&err_params, sizeof(err_params));
	bzero(&cach2_params, sizeof(cach2_params));
	bzero(&cach_params, sizeof(cach_params));
	bzero(&conn_params, sizeof(conn_params));
	bzero(&fdgeom_params, sizeof(fdgeom_params));

	/* bcopy from current to get the block descr filled in */
	if(current_acc_params.bd_len)
		bcopy(&current_acc_params, &acc_params,
		    pglengths[DEV_FORMAT] + 4 + 2);
	else
		bzero(&acc_params, sizeof(acc_params));

#ifdef SMFD_NUMBER
	if(drivernum == SMFD_NUMBER && pglengths[DEV_FDGEOMETRY]) {
		argnum(&n, current_fdgeom_page->g_stpcyl, 0xff, "Step Pulses/Cylinder");
		current_fdgeom_page->g_stpcyl = n;

		argnum(&n, current_fdgeom_page->g_moton, 0xff,
		    "Time to spinup (.1 sec)");
		current_fdgeom_page->g_moton = n;

		argnum(&n, current_fdgeom_page->g_motoff, 0xff,
		    "Spin down after idle (.1 sec)");
		current_fdgeom_page->g_motoff = n;

		n  = sc_bytes_sh(current_fdgeom_page->g_headset);
		n /= 10;	/* assume 500Kbits/sec transfer rate */
		argnum(&n, n, 0x10000, "Head settle (ms)");
		n *= 10;
		sc_sh_bytes(n, current_fdgeom_page->g_headset);

		bzero(&fdgeom_params, sizeof(fdgeom_params));

		/* NCR won't accept media type of 0 as being current... */
		fdgeom_params.mediatype = current_fdgeom_params.mediatype;

		bcopy(current_fdgeom_page,
		    &fdgeom_params.dk_pages.dev_fdgeometry, 
		    pglengths[DEV_FDGEOMETRY] + 4 + 2);
		scsi_modeselect(&fdgeom_params, 0);
		scsi_modesense(&current_fdgeom_params, DEV_FDGEOMETRY|CURRENT);

		return;	/* none of these go in the vh struct */
	}
#endif /* SMFD_NUMBER */

	newline();
	current_val = (current_err_page->e_err_bits & E_DCR) ? 1: 0;
	argchoice(&ecc_bit, current_val, &set_ecc_menu, "Error correction");
	if (ecc_bit)
		current_err_page->e_err_bits |= E_DCR;
	else
		current_err_page->e_err_bits &= ~E_DCR;

	current_val = (current_err_page->e_err_bits & E_DTE) ? 1: 0;
	argchoice(&ecc_bit, current_val, 
	    &set_ecc_menu, "Data transfer on error");
	if (ecc_bit)
		current_err_page->e_err_bits |= E_DTE;
	else
		current_err_page->e_err_bits &= ~E_DTE;

	current_val = (current_err_page->e_err_bits & E_PER) ? 1: 0;
	argchoice(&ecc_bit, current_val, 
	    &set_ecc_menu_bar, "Report recovered errors");
	if (ecc_bit)
		current_err_page->e_err_bits |= E_PER;
	else
		current_err_page->e_err_bits &= ~E_PER;

	current_val = (current_err_page->e_err_bits & E_RC) ? 1: 0;
	argchoice(&ecc_bit, current_val, 
	    &set_ecc_menu, "Delay for error recovery");
	if (ecc_bit)
		current_err_page->e_err_bits |= E_RC;
	else
		current_err_page->e_err_bits &= ~E_RC;

	argnum(&n, current_err_page->e_retry_count, 100, "Err retry count");
	current_err_page->e_retry_count = n;
	current_val = (current_err_page->e_err_bits & E_TB) ? 1: 0;
	argchoice(&ecc_bit, current_val, 
	    &set_ecc_menu_bar, "Transfer of bad data blocks");
	if (ecc_bit)
		current_err_page->e_err_bits |= E_TB;
	else
		current_err_page->e_err_bits &= ~E_TB;

	current_val = (current_err_page->e_err_bits & E_AWRE) ? 1: 0;
	argchoice(&ecc_bit, current_val, 
	    &set_ecc_menu_bar, "Auto bad block reallocation (write)");
	if (ecc_bit)
		current_err_page->e_err_bits |= E_AWRE;
	else
		current_err_page->e_err_bits &= ~E_AWRE;
	current_val = (current_err_page->e_err_bits & E_ARRE) ? 1: 0;
	argchoice(&ecc_bit, current_val, 
	    &set_ecc_menu_bar, "Auto bad block reallocation (read)");
	if (ecc_bit)
		current_err_page->e_err_bits |= E_ARRE;
	else
		current_err_page->e_err_bits &= ~E_ARRE;

	if(pglengths[CACHE_SCSI2]) {	/* prefer 'standard' if avail */
		current_val = current_cach2_page->c_rcd ? 0 : 1;
		argchoice(&ecc_bit, current_val, 
		    &set_ecc_menu_bar, "Read ahead caching");
		current_cach2_page->c_rcd = ecc_bit ? 0 : 1;

		current_val = current_cach2_page->c_wce;
		argchoice(&ecc_bit, current_val, 
		    &set_ecc_menu_bar, "Write buffering");
		current_cach2_page->c_wce = ecc_bit ? 1 : 0;

		n = sc_bytes_sh(current_cach2_page->c_predislen);
		argnum(&n, n, 0x10000, "Drive disable prefetch");
		sc_sh_bytes(n, current_cach2_page->c_predislen);

		n = sc_bytes_sh(current_cach2_page->c_minpre);
		argnum(&n, n, 0x10000, "Drive minimum prefetch");
		sc_sh_bytes(n, current_cach2_page->c_minpre);

		n = sc_bytes_sh(current_cach2_page->c_maxpre);
		argnum(&n, n, 0x10000, "Drive maximum prefetch");
		sc_sh_bytes(n, current_cach2_page->c_maxpre);

		n = sc_bytes_sh(current_cach2_page->c_maxpreceil);
		argnum(&n, n, 0x10000, "Drive prefetch ceiling");
		sc_sh_bytes(n, current_cach2_page->c_maxpreceil);
		if(pglengths[CACHE_CONTR]) {/*	some of the CDC drives need it set */
			/* in both pages, and the ccen is only in CACHE_CONTR */
			current_cach_page->c_ccen = current_cach_page->c_ce =
				~current_cach2_page->c_rcd;
		}

		if (((struct cachescsi2 *) &changeable[CACHE_SCSI2])->c_numseg)
		{
			n = (uint) current_cach2_page->c_numseg;
			argnum(&n, n, 256, "Number of cache segments");
			current_cach2_page->c_numseg = (u_char) n;
		}
	}
	else {
		current_val = current_cach_page->c_ce;
		argchoice(&ecc_bit, current_val, 
		    &set_ecc_menu_bar, "Read ahead caching");
		if (ecc_bit)
			current_cach_page->c_ce = 1;
		else
			current_cach_page->c_ce = 0;
		/* set cyl cross enable to same as the cache enable */
		current_cach_page->c_ccen = current_cach_page->c_ce;
	}

	current_val = (DP(&vh)->dp_flags & DP_CTQ_EN) ? 1: 0;
	argchoice(&ecc_bit, current_val, 
	    &set_ecc_menu_bar, "Enable CTQ");
	if (ecc_bit) {
		DP(&vh)->dp_flags |= DP_CTQ_EN;
		n = (uint)(DP(&vh)->dp_ctq_depth);
		if (n == 0)
			n = 2;
		argnum(&n, n, 255, "CTQ depth");
		DP(&vh)->dp_ctq_depth = (u_char) n;
	} else {
		DP(&vh)->dp_flags &= ~DP_CTQ_EN;
		DP(&vh)->dp_ctq_depth = 0;
	}

	/* connection params */
	current_conn_page->c_bfull = getfrac(current_conn_page->c_bfull,
	    CDC_BUF_UNITS, "Read buffer ratio");
	current_conn_page->c_bempty = getfrac(current_conn_page->c_bempty,
	    CDC_BUF_UNITS, "Write buffer ratio");

	sc_uint_3bytes(n, &acc_params.block_descrip[5]);
	sc_uint_3bytes(n, &err_params.block_descrip[5]);

	lastchance_dp();

	if(pglengths[ERR_RECOV]) {
		/*	this is a hack for Toshiba CDROM; I really should just
			the bullet and convert all of these to use Xselect; Toshiba
			doesn't support the format page, but you can change the
			blocklen in the block descr, and it does support err page */
		bcopy(current_err_page, &err_params.dk_pages.err_recov, 
		    pglengths[ERR_RECOV] + 4 + 2);
		err_params.bd_len = 8;
		err_params.sense_len = 13 + pglengths[ERR_RECOV];	/* 8 + 6 - 1 */
		/*	Xselect because we want to set the block len in the
			block descr.  Force use of descr even if sense
			didn't return it.  */
		scsi_modeXselect(&err_params, 0);
	}
	if(pglengths[CACHE_SCSI2]) {	/* prefer 'standard' if avail */
		bcopy(current_cach2_page, &cach2_params.dk_pages.cachescsi2,
		    pglengths[CACHE_SCSI2] + 4 + 2);
		scsi_modeselect(&cach2_params, 0);
	}
	if(pglengths[CACHE_CONTR]) {	/* BUT, always do this
		if supported, because some CDC/Imprimis/Seagate
		drives need both bits set, and there is also c_ccen */
		bcopy(current_cach_page, &cach_params.dk_pages.cachctrl,
		    pglengths[CACHE_CONTR] + 4 + 2);
		scsi_modeselect(&cach_params, 0);
	}
	if(pglengths[CONN_PARMS]) {
		bcopy(current_conn_page, &conn_params.dk_pages.cparms,
		    pglengths[CONN_PARMS] + 4 + 2);
		scsi_modeselect(&conn_params, 0);
	}

	/* NOTE: a sense is always done following the selects, since
	 * some drives are setting values in other pages as a side
	 * effect...  Also, if the sector length was changed, then
	 * we want the global pages to get their blk descr's set;
	 * otherwise we might inadvertently set it back on a later
	 * Xselect. */
	if(pglengths[CONN_PARMS])
		scsi_modesense(&current_conn_params, CONN_PARMS|CURRENT);
	if(pglengths[CACHE_CONTR])
		scsi_modesense(&current_cach_params, CACHE_CONTR|CURRENT);
	if(pglengths[CACHE_SCSI2])
		scsi_modesense(&current_cach2_params, CACHE_SCSI2|CURRENT);
	if(pglengths[ERR_RECOV])
		scsi_modesense(&current_err_params, ERR_RECOV|CURRENT);

	/* and now set the values in the vh; 2nd arg is 0 so
	 * we don't complain about geometry differences, since this
	 * routine doesn't change geometry. */
	scsiset_dp(DP(&vh));
}


/* set all parameters to drive manufacturer's defaults, which might
 * be different than the way SGI ships the drives.  If the geometry
 * would be different, warn them before doing anything.
*/
void
scsi_defparm_func(void)
{
	sndata_t params;
	int geomchanges = 0;

	if(!current_geom_page)	/* haven't read current yet! */
		readin_dp();

#ifdef SMFD_NUMBER
	if(drivernum == SMFD_NUMBER && pglengths[DEV_FDGEOMETRY]) {
		struct dev_fdgeometry *fdg;
		scsi_modesense(&params, DEV_FDGEOMETRY|DEFAULT);
		fdg = (struct dev_fdgeometry *)(params.block_descrip + params.bd_len);
		if(fdg->g_bytes_sec[1] != current_fdgeom_page->g_bytes_sec[1] ||
			fdg->g_bytes_sec[0] != current_fdgeom_page->g_bytes_sec[0] ||
			fdg->g_ncyl[1] != current_fdgeom_page->g_ncyl[1] ||
			fdg->g_ncyl[0] != current_fdgeom_page->g_ncyl[0] ||
			fdg->g_nhead != current_fdgeom_page->g_nhead ||
			fdg->g_spt != current_fdgeom_page->g_spt)
			geomchanges = 1;
		bd_blklen = sc_bytes_sh(fdg->g_bytes_sec);
	}
#endif /* SMFD_NUMBER */

	if(pglengths[DEV_GEOMETRY] && !geomchanges) {
		struct dev_geometry *geom;
		scsi_modesense(&params, DEV_GEOMETRY|DEFAULT);
		geom = (struct dev_geometry *)(params.block_descrip + params.bd_len);
		if(!current_geom_page->g_nhead) {
			/* no printf here; should have been reported before */
			current_geom_page->g_nhead = 1;
		}
		if(geom->g_ncyl[2] != current_geom_page->g_ncyl[2] ||
			geom->g_ncyl[1] != current_geom_page->g_ncyl[1] ||
			geom->g_ncyl[0] != current_geom_page->g_ncyl[0] ||
			geom->g_spindlesync != current_geom_page->g_spindlesync ||
			geom->g_nhead != current_geom_page->g_nhead)
			geomchanges = 1;
	}
	if(pglengths[DEV_FORMAT] && !geomchanges) {
		struct dev_format *fmt;
		scsi_modesense(&params, DEV_FORMAT|DEFAULT);
		fmt = (struct dev_format *)(params.block_descrip + params.bd_len);
		if(params.bd_len && bd_blklen != sc_3bytes_uint(&params.block_descrip[5]) ||
			fmt->f_trk_zone[0] != current_acc_page->f_trk_zone[0] ||
			fmt->f_trk_zone[1] != current_acc_page->f_trk_zone[1] ||
			fmt->f_altsec[0] != current_acc_page->f_altsec[0] ||
			fmt->f_altsec[1] != current_acc_page->f_altsec[1] ||
			fmt->f_alttrk_zone[0] != current_acc_page->f_alttrk_zone[0] ||
			fmt->f_alttrk_zone[1] != current_acc_page->f_alttrk_zone[1] ||
			fmt->f_alttrk_vol[0] != current_acc_page->f_alttrk_vol[0] ||
			fmt->f_alttrk_vol[1] != current_acc_page->f_alttrk_vol[1] ||
			fmt->f_trkskew[0] != current_acc_page->f_trkskew[0] ||
			fmt->f_trkskew[1] != current_acc_page->f_trkskew[1] ||
			fmt->f_cylskew[0] != current_acc_page->f_cylskew[0] ||
			fmt->f_cylskew[1] != current_acc_page->f_cylskew[1] ||
			fmt->f_bytes_sec[0] != current_acc_page->f_bytes_sec[0] ||
			fmt->f_bytes_sec[1] != current_acc_page->f_bytes_sec[1])
			geomchanges = 1;
		sc_uint_3bytes(bd_blklen, &params.block_descrip[5]);
	}

	if(geomchanges) {
		if(!yesno(-1, "NOTE: restoring drive manufacture default parameters will\n"
		"change the drive geometry, which requires that the drive be\n"
		"re-formatted.  This will cause all data on the drive to be lost.\n"
		"Continue"))
			return;
	}

	do_scsidefault();

	DP(&vh)->dp_flags = 0;

	/* set the values in the vh struct from the current params */
	scsiset_dp(DP(&vh));
	return;
}


/* Set all parameters on all supported pages to drive manufacturer's defaults.
 * Called from fx.c, smfd,c and scsi_defparm_func() above
*/
void
do_scsidefault(void)
{
	sndata_t params;
	register pg;

	for(pg=0; pg<ALL; pg++)
		if(pglengths[pg]) {
			scsi_modesense(&params, DEFAULT | pg);
			scsi_modeXselect(&params, 0);
		}
}


/* get the current (not default) parameters for the scsi device */
void
get_scsi_param(void)
{
	/*	readin all the supported pages that fx uses.
		data for unsupported pages should be zero'ed in
		case user switches to a different drive.  The
		page ptrs should always be set to simplify the
		code in other parts of fx. */
	if(pglengths[DEV_FORMAT]) {
		scsi_modesense(&current_acc_params, DEV_FORMAT);
		current_acc_page = (struct dev_format *)
		    (current_acc_params.block_descrip
		    + current_acc_params.bd_len);
	}
	else if(!pglengths[DEV_FDGEOMETRY]) {	/* only for non-floppy */
		current_acc_page = (struct dev_format *)
		    (current_acc_params.block_descrip);
		bzero(current_acc_page, sizeof(*current_acc_page));
		/* set these to avoid divide by 0 errors later; some drives,
			such as Sony SMO-C501 optical don't support this page... */
		sc_sh_bytes(1, current_acc_page->f_trk_zone);
		sc_sh_bytes(1, current_acc_page->f_interleave);
		sc_sh_bytes(DEF_SECSTRK, current_acc_page->f_sec_trac);
		if(expert)
		  printf("Can't get drive geometry, assuming %d sectors/track\n",
			DEF_SECSTRK);
	}
	if(pglengths[DEV_GEOMETRY]) {
		scsi_modesense(&current_geom_params, DEV_GEOMETRY);
		current_geom_page = (struct dev_geometry *)
		    (current_geom_params.block_descrip
		    + current_geom_params.bd_len);
		if(!current_geom_page->g_nhead) {
			current_geom_page->g_nhead = 1;
			errwarn("drive reports 0 heads, assuming %d\n",
				current_geom_page->g_nhead);
		}
	}
	else if(!pglengths[DEV_FDGEOMETRY]) {	/* only for non-floppy */
		unsigned blksize;
		current_geom_page = (struct dev_geometry *)
		    (current_geom_params.block_descrip);
		bzero(current_geom_page, sizeof(*current_geom_page));
		/*	set these to avoid divide by 0 errors later; some drives,
			such as Sony SMO-C501 optical don't support his page...
			SCSI-2 says we should be able to get some of this info from
			the density code in the param descr block, but the Sony
			doesn't give us a descr block... */
		current_geom_page->g_nhead = DEF_HEADS;
		if(expert)
		  printf("Can't get drive geometry, assuming %d heads\n", DEF_HEADS);
		/* do a readcapacity and then fake sec_trk and cyls */
		if(scsi_readcapacity(&blksize) == 0) {
			blksize /= sc_bytes_sh(current_acc_page->f_sec_trac) * DEF_HEADS;
			sc_uint_3bytes(blksize, current_geom_page->g_ncyl);
		}
		else	/* set to single cyl to avoid divide by zero problems later */
			sc_uint_3bytes(1, current_geom_page->g_ncyl);
	}
	if(pglengths[BLOCK_DESCRIP]) {	/* damn toshiba */
		scsi_modesense(&current_blk_params, BLOCK_DESCRIP);
		current_blk_page = (struct block_descrip *)
		    (current_blk_params.block_descrip +
		    current_blk_params.bd_len);
	}
	else {
		current_blk_page = (struct block_descrip *)
		    (current_blk_params.block_descrip);
		bzero(current_blk_page, sizeof(*current_blk_page));
	}

	if(pglengths[CACHE_SCSI2]) {	/* prefer 'standard' if avail */
		scsi_modesense(&current_cach2_params, CACHE_SCSI2);
		current_cach2_page = (struct cachescsi2 *)
		    (current_cach2_params.block_descrip +
		    current_cach2_params.bd_len);
	}
	else {
		current_cach2_page = (struct cachescsi2 *)
		    (current_cach2_params.block_descrip);
		bzero(current_cach2_page, sizeof(*current_cach2_page));
	}
	if(pglengths[CACHE_CONTR]) {	/* BUT, always do this
		if supported, because some CDC/Imprimis/Seagate
		drives need both bits set, and there is also c_ccen */
		scsi_modesense(&current_cach_params, CACHE_CONTR);
		current_cach_page = (struct cachectrl *)
		    (current_cach_params.block_descrip + 
		    current_cach_params.bd_len);
	}
	else {
		current_cach_page = (struct cachectrl *)
		    (current_cach_params.block_descrip);
		bzero(current_cach_page, sizeof(*current_cach_page));
	}

	if(pglengths[CONN_PARMS]) {
		scsi_modesense(&current_conn_params, CONN_PARMS);
		current_conn_page = (struct connparms *)
		    (current_conn_params.block_descrip + 
		    current_conn_params.bd_len);
	}
	else {
		current_conn_page = (struct connparms *)
		    (current_conn_params.block_descrip);
		bzero(current_conn_page, sizeof(*current_conn_page));
	}
	/* fall through */
	if(pglengths[DEV_FDGEOMETRY]) {
		scsi_modesense(&current_fdgeom_params, DEV_FDGEOMETRY);
		current_fdgeom_page = (struct dev_fdgeometry *)
		    (current_fdgeom_params.block_descrip
		    + current_fdgeom_params.bd_len);
	}
	if(pglengths[ERR_RECOV]) {
		scsi_modesense(&current_err_params, ERR_RECOV);
		current_err_page = (struct err_recov *)
		    (current_err_params.block_descrip
		    + current_err_params.bd_len);
	}
	else {
		current_err_page = (struct err_recov *)
		    (current_err_params.block_descrip);
		bzero(current_err_page, sizeof(*current_err_page));
	}
}


void
lastchance_dp(void)
{
	STRBUF s;
	sprintf(s.c, "about to modify drive parameters on disk %s! ok", dksub());
	banner("WARNING");
	if( !yesno(-1, s.c) ) {
		/* if not changing, re-read current values! */
		get_scsi_param();
		mpop();
	}
	changed = 1;
}


/*	dump out a scsi mode-select page, which may or may not contain
 *	a block descriptor
*/
static void
scsi_dumppage(unsigned pg, u_char *pbuf, int hasblk)
{
	u_char *a;
	char *l;
	int i;
#define PGTYPE(pg) ((pg)&(SAVED|CURRENT|DEFAULT|CHANGEABLE))

	if(PGTYPE(pg)==SAVED)
		l = "(saved";
	else if(PGTYPE(pg)==DEFAULT)
		l = "(default";
	else if(PGTYPE(pg)==CHANGEABLE)
		l = "(modifable";
	else if(PGTYPE(pg)==CURRENT)
		l = "(current";
	printf("0x%02x %12s): 0x%02x", pg&ALL, l, pglengths[pg&ALL]);
	printf("  hdr: %3x%3x%3x%3x", pbuf[0], pbuf[1],
		pbuf[2], pbuf[3]);
	if(hasblk && pbuf[3]) {
		printf("  blk desc: %3x%3x%3x%3x%3x%3x%3x%3x",
			pbuf[4], pbuf[5], pbuf[6], pbuf[7],
			pbuf[8], pbuf[9], pbuf[10], pbuf[11]);
		a = &pbuf[12];
	}
	else
		a = &pbuf[4];
	printf("\n     pgdata: ");
	for(i=0; i<(pglengths[pg&ALL]+2); i++) {
		if((i%18) == 17)
			printf("\n%13s", " ");
		printf("%3x", *a++);
	}
	printf("\n");
}

/*	called from the top level debug menu if expert mode.  Show the
	supported pages and their lengths.
*/
void
showpages_func(void)
{
	register u_char pg;
	int which = 0, anyflags = -1;
	char flags[4];

	checkflags("cmds", flags);

	for(pg=0; pg < ALL; pg++) {
		if(pglengths[pg]) {
			register i;
			if(which)
				printf("   ");
			if(anyflags) for(i=0; i<sizeof(flags); i++) {
				if(flags[i]) {
					sndata_t addr;
					u_char type;
					anyflags = 1;
					if(i==0) type = CURRENT;
					else if(i==1) type = CHANGEABLE;
					else if(i==2) type = DEFAULT;
					else if(i==3) type = SAVED;
					if(scsi_modesense(&addr, pg|type) == 0)
						scsi_dumppage(pg|type, (u_char *)&addr, 1);
				}
			}
			if(anyflags <= 0) {
				anyflags = 0;	/* so for loop only done first page */
				printf("0x%02x: 0x%02x", pg, pglengths[pg]);
				if(++which == 6) {
					which = 0;
					printf("\n");
				}
			}
		}
	}
}


/*	do a mode select on an arbitrary page with arbitrary data. data not
	entered at the end of the requested data is interpreted to be 0
*/
void
setpage_func(void)
{
	uint i, pglen;
	void *addr;
	u_char *d, pg;
	uint64_t *pgdata;
	sndata_t sense;
	/* large enough for largest possible returned data */
	uint64_t pdata[256/sizeof(uint64_t)];	

	argnum(&i, 0, ALL, "page #");
	pg = (u_char)i;
	if(!pglengths[pg]) {
		printf("device says page 0x%x isn't supported\n", pg);
		return;
	}
	pglen = pglengths[pg];
	pgdata = pdata;
	addr = (void *)&sense;

	/* in addition to being informative, this sets the header 'correctly' */
	if(scsi_modesense((sndata_t *)addr, CURRENT | pg) == 0) {
		if(((sndata_t *)addr)->bd_len == 0)
			printf("block descriptors don't appear to be supported\n");
		scsi_dumppage(pg|CURRENT, (u_char *)addr, 1);
	}

	if(!no("use block descriptor")) {
		sense.bd_len = 8;
		sense.sense_len = 13 + pglengths[pg];	/* 8 + 6 - 1 */
		d = (u_char *)&sense.dk_pages;
		printf("enter up to 8 bytes: ");
		bzero(pgdata, 8*sizeof(*pgdata));
		i = get_nums(0x100LL, pgdata, 8);
		while(i-- > 0)
			sense.block_descrip[i] = (u_char)pgdata[i];
	}
	else {
		sense.sense_len = 0;
		((sldata_t *)addr)->blk_len = 0;
		d = (u_char *)&((sldata_t *)addr)->dk_pages;
	}
	printf("enter up to 0x%x bytes (skipping pgnum and len): ", pglen);
	i = get_nums(0x100LL, pgdata, pglen);
	if(i > 0) {
		int zero = pglen - i;
		*d++ = pg;
		*d++ = pglen;
		while(i-- > 0)
			*d++ = (u_char)*pgdata++;
		bzero(d, zero);	/* zero rest */
		if(sense.bd_len == 8)
			i = scsi_modeXselect((sndata_t *)addr, 1);
		else
			i = scsi_modeselect((sldata_t *)addr, 1);
		if(!i) {
			printf("current parameters are:\n");
			if(scsi_modesense(&sense, CURRENT | pg))
				/* add a bit more to the message already printed */
				printf("Strange: sense after select failed\n");
			else
				scsi_dumppage(pg|CURRENT, (u_char *)&sense, 1);
		}
		/* else error already reported */
	}
	/* else out of range or invalid #; already reported */
	scsiset_dp(DP(&vh));
}


/*	called from the top level debug menu if expert mode.  Show the
	capacity as returned by the readcapacity command.
*/
void
showcapacity_func(void)
{
	unsigned blksize;

	if(scsi_readcapacity(&blksize) == 0)
		printf("capacity is %u blocks\n", blksize);
	/* else error message already printed */
}

/*	used to mask off bits that aren't changeable.  Some drives are
	not tolerant of attempts to change them, even if they are the
	same as the current/default value.

	NOTE: this could be a problem, since some drives want to see
	0 for non-changeable bits, and others want to see the current
	bits.  Neither SCSI1 nor SCSI2 are really quite clear what to
	do about this....   We have prevailed on most of our suppliers
	to always accept the current values, even if the bits are NOT
	changeable.
*/
static void
maskch(register u_char *set, register u_char *change, register u_char *cur,
u_char len)
{
	register u_char i;

	for(i=0; i < len; i++) {
		*set &= *change;
		*set |= ~(*change) & *cur;
		set++, change++, cur++;
	}
}

