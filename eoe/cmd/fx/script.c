/* this option implements the -s (script) option of fx, added primarily
 * for the autoinst feature, but documented and usable for other purposes.
 * At this time, it only implements partitioning.  When -s is given, all
 * other command line arguments are ignored!
*/


/* Blank lines and lines starting with # are ignored.  Continuation lines
 * are not currently supported, but could be added (ending in \).
 * script format is:
 *	device  size  type
 * with trailing junk (name field from inst mrconfig file) ignored.
 * see doline() for how fields are parsed.  sizes/starting block, etc.
 * are always in terms of 512 byte blocks.
 * Field seperators can be space or tab, or a combination.
 * No spaces are allowed.  Device names are as they show up in 
 * /dev/rdsk/ (without leading /dev/rdsk).  Each line is treated
 * independently, and if they are conflicting, the later line always
 * wins.  Partition, ID, and controller are derived by parsing the
 * device name.  Unnecessary opens (ID and controller the same as 
 * previous device) are prevented by remembering the previous device.
*/

#include "fx.h"	/* only for types in prototypes.h */
#include "prototypes.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

extern char fulldrivename[];
static char sline[256];
static int vhok;	/* did we read the vh OK? */
static int prot_ctlr, prot_id, prot_part, prot_lun, protected;
static int64_t prot_firstblk, prot_blocks; /* keep as blocks, not
	partition data, for easier detection of overlap */

/* actually do the partitioning */
dopart(int part, int fstype, int std, int64_t startblk, int64_t psize, int
	chkprot)
{

	if(!vhok && !std) {
		fprintf(stderr, "Warning: %s: no valid disk info, initializing as generic disk\n",
			fulldrivename);
		create_all_func();
		show_pts(&vh);
	}
	if(std) {
		/* zero all the other partitions also.  normal fx code does,
		 * but we don't go through quite the same path, so do it 
		 * explictly */
		int fpart;
		for(fpart=0; fpart<NPARTAB; fpart++)
			if(fpart != PTNUM_VOLUME && fpart != PTNUM_VOLHDR)
				PT(&vh)[fpart].pt_nblks = 0;
		if(std == 1)
			pt_sroot_func();
		else
			pt_optdrive_func();
	}
	else {
		if(psize == -1LL)
			psize = PT(&vh)[PTNUM_VOLUME].pt_nblks - startblk;
		if((psize+startblk) > PT(&vh)[PTNUM_VOLUME].pt_nblks) {
			fprintf(stderr, "Error: start + size too large to fit on disk\n");
			return 1;

		}

		if(chkprot) {
			struct partition_table p = PT(&vh)[prot_part];
			struct partition_table pint;
			p.pt_nblks = prot_blocks;
			p.pt_firstlbn = prot_firstblk;
			pint.pt_nblks = psize;
			pint.pt_firstlbn = startblk;
			if(intersect(&p, &pint)) {
				fprintf(stderr, "Error: Overlaps protected partition, ignored\n\t");
				return 1;
			}
		}

		PT(&vh)[part].pt_firstlbn = startblk;
		PT(&vh)[part].pt_nblks = psize;
		if(fstype != -1)
			PT(&vh)[part].pt_type = fstype;
	}

	(void)update_vh();

	return 0;
}


doline(char *spec)
{
	char *dev, *sdev, *size, *type, *tmp, *sz;
	int bad=0, dostd=0, thisprot=0, chkover=0, doallochk=0, fstyp, r;
	unsigned ctlr, id, part, lun=0;
	int64_t startblk, psize=0;

	if(!(dev=strtok(spec, " \t\n")) || !*dev)
		bad = 1;
	else if(*dev == '#')
		return 0;	/* allow whitespace # comments */
	else if(!(size=strtok(NULL, " \t\n")) || !*size)
		bad = 1;
	else if(!(type=strtok(NULL, " /\t\n")) || !*type) /* / because
		* xfs/blocksize, and we don't want the /blocksize part */
		bad = 1;
	/* don't care about trailing stuff */
	if(bad)
		return bad;

	if(!strcmp("protect", type))
		thisprot = 1;
	if(!strcmp("existing", size)) {
		if(!thisprot)
			return 0; /* fx nop, except for type == protect */
		else
			psize = -1LL;
	}
	else if(strcmp("standard", size)) {
		/* check for valid sizes here, including  "remainder".
		* If not valid size:
		* 	(start#|followspart#):("remainder"|"all"|sizeinblocks#)
		* fail.  /blocksize is ignored in fx. */
		sz = strtok(size, ":");
		if(!sz || !*sz)
			return 1;
		if(!strncmp("all", sz, 3)) {
			psize = -1LL;	/* whole disk, follows part 8 */
			startblk = -PTNUM_VOLHDR;
		}
		else if(strncmp("followspart", sz, 11)) {
			startblk = (int64_t)strtoull(sz, &tmp, 10);
			 /* 0 valid for vh, and for deleting parts! */
			if(sz == tmp)
				return 1;
		}
		else /* have to defer check, in case not already opened */
			doallochk = 1;
		if(psize != -1LL)	 { /* if not the "all" case above */
			char *psz = strtok(NULL, "");
			if(!psz || !*psz)
				return 1;
			else if(strncmp("remainder", psz, 9)) {
				psize = (int64_t)strtoull(psz, &tmp, 10);
				if(psz == tmp)
					return 1;	/* 0 valid to allow them to delete partitions,
						but must be explict, not empty */
			}
			else	/* remainder */
				psize = -1LL;
		}
	}

	if(thisprot)
		;
	else if(!(r=strcmp("root", type)) || !strcmp("option", type)) {
		if(strcmp("standard", size))
			return 1; /* root and option require standard */
		fstyp = PTYPE_XFS;
		dostd = r ? 2 : 1;
		startblk = 0; /* not used */
	}
	else if(!strcmp("xfs", type))
		fstyp = PTYPE_XFS;
	else if(!strcmp("efs", type))
		fstyp = PTYPE_EFS;
	else if(!strcmp("raw", type))
		fstyp = PTYPE_RAW;
	else if(!strcmp("swap", type))
		fstyp = PTYPE_RAW;
	else if(!strcmp("preserve", type))
		fstyp = -1;
	else
		return 1;

	if(strncmp("dks", dev, 3))
		bad = 1;
	ctlr = strtoul(&dev[3], &tmp, 10);
	if(ctlr == 0 && tmp == &dev[3])
		return 1;
	if(*tmp != 'd')
		return 1;
	sdev = dev;
	dev = tmp+1;
	id = strtoul(dev, &tmp, 10);
	if(id == 0 && tmp == dev)
		return 1;
	if(*tmp == 'l')	{ /* LUN */
		dev = tmp+1;
		lun = strtoul(dev, &tmp, 10);
		if(lun == 0 && tmp == dev)
			return 1;
	}
	else
		lun = 0;
	if(*tmp != 's')
		return 1;
	dev = tmp+1;
	part = strtoul(dev, &tmp, 10);
	if((part==0 && tmp==dev) || part==PTNUM_VOLUME)
		return 1;	/* has to be a number, and not vol, volhdr OK */

	if(ctlrno != ctlr || scsilun != lun || id != driveno) {
		ctlrno = ctlr;
		scsilun = lun;
		driveno = id;
		/* show what we wound up with */
		if(ctlrno != (uint)-1)	/* show the last disk we did */
			show_pts(&vh);
		gclose();	/* close previous */
		strncpy(fulldrivename, sdev, 80);	/* from fx.c */
		if(gopen(id, PTNUM_VOLUME) < 0) {
			ctlrno = (uint)-1;	/* for open for next line */
			return 0;	/* error message already issued */
		}
		/* read these in here, for partition overlap checking, etc. */
		if(readdvh(&vh) && readinvh(&vh)) {
			memset(&vh, 0, sizeof(vh));	/* make sure no leftovers */
			vhok = 0;	/* don't complain here, we complain in dopart(),
				if not initializing as standard */
		}
		else
			vhok = 1;
	}

	if(doallochk)  {
		unsigned lpart = strtoul(&sz[11], &tmp, 10);
		if(tmp == &sz[11] || lpart >= NPARTAB)
			return 1; 	/* not numeric, or invalid partition # */
		if(PT(&vh)[lpart].pt_nblks <= 0) {
			fprintf(stderr, "Error: can't follow partition %u, not allocated\n\t",
				lpart);
			return 1;
		}
		startblk = PT(&vh)[lpart].pt_firstlbn + PT(&vh)[lpart].pt_nblks;
	}

	if(thisprot) {	/* only after successful open */
		if(psize == 0LL) {
			/* turn off protect, if size is explictly 0 */
			protected = 0;
			prot_blocks = 0;
			return 0;
		}
		if(!vhok) {
			fprintf(stderr, "Error: can't protect, no volume header\n\t");
			return 1;
		}
		prot_part = part;
		protected = 1;
		prot_ctlr = ctlr;
		prot_id = id;
		prot_lun = lun;
		if(psize == -1LL) {
			/* "existing" or "all" */
			prot_blocks = PT(&vh)[part].pt_nblks;
			prot_firstblk = PT(&vh)[part].pt_firstlbn;
		}
		else {	/* explict */
			prot_blocks = psize;
			prot_firstblk = startblk + PT(&vh)[part].pt_firstlbn;
		}
		return 0;
		/* nothing else to do for this line */
	}

	if(protected && prot_ctlr == ctlr && prot_id == id &&
		prot_lun == lun) {
		/* need to be sure we won't trash the protected partition */
		if(prot_part == part) {
			if(startblk > prot_firstblk ||
				(startblk+psize) < (prot_firstblk+prot_blocks)) {
				fprintf(stderr,
				  "Error: would change protected partition, ignored\n\t");
				return 1;
			}
		}
		else if(dostd) {
			fprintf(stderr, "Error: can't use \"standard\"; disk has protected partition\n\t");
			return 1;
		}
		else
			chkover = 1;
	}

	return dopart(part, fstyp, dostd, startblk, psize, chkover);
}

void
fxscript(char *scriptname)
{
	char line[256];
	FILE *sc;
	int errs = 0;

	if((sc=fopen(scriptname, "r")) == NULL) {
		fprintf(stderr, "Error: couldn't open script file %s: %s\n", 
			scriptname, strerror(errno));
		exit(1);
	}

	driver = SCSI_NAME;	/* only work with dks devices */
	drivernum = SCSI_NUMBER;
	driveno = 0;	/* only have one drive open at a time */
	ctlrno = (uint)-1;	/* to force gopen() on first device */

	line[sizeof(line)-1] = '\n';
	while(fgets(line, sizeof(line), sc)) {
		if(line[sizeof(line)-1] != '\n') {
			fprintf(stderr, "Error: line too long, skipped:\n\t%s\n", line);
			while(fgetc(sc) != '\n' && !(feof(sc)||ferror(sc)))
				;
			line[sizeof(line)-1] = '\n';
			continue;
		}
		if(*line == '\n' || *line == '#') {
			continue;
		}
		strcpy(sline, line); /* for error messages, "line" is trashed */
		if(doline(line)) {
			fprintf(stderr, "Error: script line:\n\t%s\n", sline);
			errs++;
		}
	}
	if(ctlrno != (uint)-1)	/* show the last disk we did */
		show_pts(&vh);
	exit(errs);
}
