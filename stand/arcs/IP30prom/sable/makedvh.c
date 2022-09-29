/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */


/**************************************************************************
 *									  *
 * 		 Copyright (C) 1988, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/* derrived from: */
#ident	"dvhtool/dvhtool.c: $Revision: 1.2 $"

/* makedvh.c -- tool for making a volume header in a file for use
 * with simulation (in this case IP30 sable).
 *
 * usage: makedvh file size [part=n,size]* [srcfile dstfile]*
 *	file size == part=8,size
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/dvh.h>
#include <sys/dkio.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <bstring.h>
#include <ctype.h>
#include <fcntl.h>

char *tmpdir = "/usr/tmp";

#define	FNLEN		128		/* size of filename buffer */

#define dev_bsize 512
#define firstfileblk 2

#define	streq(x,y)	(strcmp(x,y)==0)
#define	strneq(x,y,c)	(strncmp(x,y,c)==0)

static int vh_checksum(struct volume_header *vhp);
static struct partition_table *getvhpart(void);
static void vd_creat(char *unix_file, char *dvh_file, int vfd);
static void copy_tovh(int vfd, char *fn, daddr_t lbn, int flen);
static void sortdir(struct volume_directory *v);
static int roundblk(int cnt);
void putdvh(int fd);
void makedvh(int fd);
void usage(void);
int roundblk(int cnt);
void vd_list(void);

int partsizes[NPARTAB];
char *srcfiles[NVDIR];
char *dstfiles[NVDIR];
int nfiles = 0;

struct volume_header vh;

extern int errno;

int
main(int argc, char **argv)
{
	char *tmp, *dvhname;
	int fd;
	int i;

	tmp = (char *)getenv("TMPDIR");
	if(tmp && *tmp)
		tmpdir = tmp;

	bzero(partsizes, sizeof(int) * NPARTAB);

	if (argc < 2) {
		usage();
		exit(1);
		/*NOTREACHED*/
	}

	dvhname = argv[1];

	printf("makedvh:\n\tfilename=%s\n",dvhname);

	/* rough parse -- the code is not too careful of syntax */
	for (i=2; i < argc; i++) {
		if (!strncmp(argv[i],"part=",5)) {
			int part;
			char *p;

			part = strtol(argv[i]+5,&p,0);
			partsizes[part] = strtol(p+1,0,0);

			printf("\tpartiton %d is 0x%x blocks\n",part,partsizes[part]);
		}
		else {
			if (nfiles > NVDIR) {
				usage();
				exit(1);
			}

			srcfiles[nfiles] = argv[i++];
			dstfiles[nfiles] = argv[i];

			printf("\tfile %s %s\n",dstfiles[nfiles],srcfiles[nfiles]);
			nfiles++;
		}
	}
	
	if ((fd = open(dvhname,O_RDWR|O_CREAT|O_TRUNC,0755)) < 0) {
		perror(dvhname);
		exit(1);
	}

	makedvh(fd);

	close(fd);

	return 0;
}

void
makedvh(int fd)
{
	int t, i;

	if (!partsizes[8]) {
		fprintf(stderr,"no partition 8 for volume header!\n");
		exit(1);
	}

	bzero(&vh,sizeof(vh));				/* initial header */

	/* create core file */
	for (i = 0 ; i < partsizes[8]; i++) {
		if (write(fd,&vh,sizeof(vh)) != sizeof(vh))
			perror("vh pad write");
	}

	vh.vh_magic = VHMAGIC;
	vh.vh_rootpt = (partsizes[0] == 0);		/* 0 or swapdev */
	vh.vh_swappt = 1;
	strcpy(vh.vh_bootfile,"/unix");

	/* vh.vh_dp is left @ 0 */

	for (i=0; i < NVDIR; i++) {			/* no files */
		vh.vh_vd[i].vd_lbn = -1;
	}

	vh.vh_pt[8].pt_nblks = t = partsizes[8];
	vh.vh_pt[8].pt_firstlbn = 0;
	vh.vh_pt[8].pt_type = PTYPE_VOLHDR;

	for (i=0; i < NPARTAB; i++) {
		if ( (i != 8) && partsizes[i]) {
			vh.vh_pt[i].pt_nblks = partsizes[i];
			vh.vh_pt[i].pt_firstlbn = t;
			vh.vh_pt[i].pt_type = PTYPE_RAW;
			t += partsizes[i];
		}
	}

	for(i = 0 ; i < nfiles; i++) {			/* write files */
		vd_creat(srcfiles[i],dstfiles[i],fd);
	}

	vh.vh_csum = vh_checksum(&vh);

	putdvh(fd);
	vd_list();
}

/*
 * create an entry in the volume directory
 */
static void
vd_creat(char *unix_file, char *dvh_file, int vfd)
{
	register int i;
	register int empty;
	register int bytes;
	struct stat sbuf;
	daddr_t lbn;

	if(strlen(dvh_file) > VDNAMESIZE) {
		fprintf(stderr,"volume header filenames must be no longer than %d characters\n",
			VDNAMESIZE);
		return;
	}
	empty = -1;
	for (i = 0; i < NVDIR; i++) {
		if (empty < 0 && vh.vh_vd[i].vd_nbytes == 0)
			empty = i;
		if (vh.vh_vd[i].vd_nbytes
		    && strneq(vh.vh_vd[i].vd_name, dvh_file, VDNAMESIZE )) {
			fprintf(stderr,"There is already a file %s in volume header\n", dvh_file);
			return;
		}
	}
	if (empty == -1) {
		fprintf(stderr,"volume header directory full\n");
		return;
	}
	if(stat(unix_file, &sbuf) == -1) {
		fprintf(stderr,"can't open %s\n", unix_file);
		return;
	}

	bytes = roundblk(sbuf.st_size);	/* round up to blks */

	if(vh_availspace() < bytes) {
		fprintf(stderr,"no room in volume header for %s\n", dvh_file);
		return;
	}

	lbn = vh_contig(bytes);

	copy_tovh(vfd, unix_file, lbn, bytes);

	/* we've been resorted in vh_contig (and possibly elsewhere).
		look for a free entry again */
	for (i = 0; i < NVDIR; i++) {
		if (vh.vh_vd[i].vd_nbytes == 0)
			break;
	}
	strncpy(vh.vh_vd[i].vd_name, dvh_file, VDNAMESIZE);
	vh.vh_vd[i].vd_nbytes = bytes;
	vh.vh_vd[i].vd_lbn = lbn;
	sortdir(vh.vh_vd); /* keep in sorted order just for appearances,
		and in case any older programs counted on it. */
}

void
putdvh(int vfd)
{
	register struct volume_directory *vd;

	/*
	 * validate consistency of volume header
	 *	partition table should have entry for volume header
	 */
	if (getvhpart() == NULL)
		return;

	/*
	 * calculate vh_csum
	 */
	vh.vh_magic = VHMAGIC;
	vh.vh_csum = 0;
	vh.vh_csum = vh_checksum(&vh);

	if (lseek(vfd, 0, 0) < 0) {
		perror("seek to sector 0 failed");
		return;	/* in case interactive */
	}
	if (write(vfd, &vh, dev_bsize) != dev_bsize) {
		perror("write of volume header to sector 0 failed");
		return;	/* in case interactive */
	}
}

/*	return space avail in volume header; for testing on creats and adds
	to be sure enough room.  Caller should probably make sure that
	any deletes have been done before the creats/adds for this to
	work best.
*/
vh_availspace()
{
	register struct partition_table *vhpt;
	register struct volume_directory *vd;
	int avail;

	vhpt = getvhpart();
	if (!vhpt)
		return - 1;
	avail = vhpt->pt_nblks - firstfileblk;
	avail *= dev_bsize;
	for(vd = vh.vh_vd; vd < &vh.vh_vd[NVDIR]; vd++)
		avail -= roundblk(vd->vd_nbytes);
	return avail;
}

/* copy a file from the unix file system to the volume header */
static void
copy_tovh(int vfd, char *fn, daddr_t lbn, int flen)
/* lbn 	block # in vh of start of file */
/* flen expected length of file (amount allocated in header) */
{
	int tfd, bytes, i, res;
	char buf[8192];

	if (lseek(vfd, lbn * dev_bsize, 0) < 0) {
		fprintf(stderr,"seek to block %d failed\n", lbn);
		return;
	}
	if ((tfd = open(fn, O_RDONLY)) < 0) {
		fprintf(stderr,"couldn't open %s\n", fn);
		return;
	}
	bytes = 0;
	while(bytes < flen && (i = read(tfd, buf, sizeof(buf))) > 0) {
		i = roundblk(i);
		res = write(vfd,  buf, i);
		if(res != i) {
			if(res == -1)
				i = errno;	/* in case error in sprintf */
			sprintf(buf, "write error on volume header with %d bytes left",
				flen - bytes);
			if(res == -1) {
				errno = i;
				perror(buf);
				exit(1);
			}
			else
				fprintf(stderr,"%s\n", buf);
			goto done;
		}
		else
			bytes += i;
	}
	if(i == -1)
		fprintf(stderr,"read error on %s\n", fn);
	else if (roundblk(bytes) != roundblk(flen))
		fprintf(stderr,"%s changed length\n", fn);
done:
	close(tfd);
}


/*	figure out which partition is the volume header */

static struct partition_table *
getvhpart(void)
{
	struct partition_table *vhpt = NULL, *pt;
	register int i;

	for (i = 0; i < NPARTAB; i++) {
		pt = &vh.vh_pt[i];
		if(pt->pt_nblks && pt->pt_type == PTYPE_VOLHDR) {
			vhpt = pt;
			break;
		}
	}

	if(!vhpt)
		printf("partition table: no entry for volume header\n");
	return vhpt;
}


/*	used to sort the volume directory on lbn.  Sort so that zero and
	negative #'s sort to highest position.  These are invalid lbn values,
	and indicate an unused slot or a partially completed entry.
*/
static int
lbncmp(v0, v1)
register struct volume_directory *v0, *v1;
{
	if(v0->vd_lbn <= 0)
		return 1;
	else if(v1->vd_lbn <= 0)
		return -1;
	return v0->vd_lbn - v1->vd_lbn;
}

static void
sortdir(struct volume_directory *v)
{
	qsort((char *)v, NVDIR, sizeof(v[0]), lbncmp);
}


/*	Determine if there is a contiguous chunk big enough in the volume header.
	return it's lbn if there is, otherwise return -1.  Used to avoid copying
	vh contents to tmpdir if possible.  Must sort the dir on lbn first,
	since no requirement that lbn's be in ascending order.
	bytes must be a multiple of dev_bsize if algorithm to work correctly.
*/
vh_contig(bytes)
{
	register struct volume_directory *v, *vd0, *vd1;
	int nblks, startblk;

	bytes /= dev_bsize;

	v = vh.vh_vd;
	sortdir(v);
	if(v->vd_lbn <= 0)
		return firstfileblk;
	for(vd0=v, vd1=vd0+1; vd1->vd_lbn > 0 && vd1 < &v[NVDIR]; vd1++) {
		nblks = roundblk(vd0->vd_nbytes) / dev_bsize;
		startblk = vd0->vd_lbn + nblks;
		if(vd0->vd_lbn < firstfileblk && startblk < firstfileblk)
			 startblk = firstfileblk;
		if((startblk + bytes) <= vd1->vd_lbn)
			return startblk;
		vd0 = vd1;
	}
	if(vd1 < &v[NVDIR]) {
		struct partition_table *vhpt;
		/* check for enough between space between last entry and end of
			partition */
		vhpt = getvhpart();
		if (!vhpt)
			return - 1;
		nblks = roundblk(vd0->vd_nbytes) / dev_bsize;
		startblk = vd0->vd_lbn + nblks;
		if(vd0->vd_lbn < firstfileblk && startblk < firstfileblk)
			startblk = firstfileblk;
		if(vhpt->pt_nblks >= (startblk + bytes))
			return startblk;
	}
	return -1;
}


/* round up to dev_bsize multiple */
static int
roundblk(int cnt)
{
	cnt = (cnt + dev_bsize -1) / dev_bsize;
	cnt *= dev_bsize;
	return cnt;
}

int
vh_checksum(struct volume_header *vhp)
{
	register int csum;
	register int *ip;

	csum = 0;
	for (ip = (int *)vhp; ip < (int *)(vhp+1); ip++)
		csum += *ip;
	return(-csum);
}

/*
 * list volume directory contents
 */
void
vd_list(void)
{
	register int i;
	int namelen = VDNAMESIZE + 3;

	printf("\nCurrent contents:\n");
	printf("\t%-*s  %10s  %10s\n", 
		namelen, "File name", "Length", "Block #");
	for (i = 0; i < NVDIR; i++) {
		if (vh.vh_vd[i].vd_nbytes) {
			printf("\t%-*.*s  %10d  %10d\n", 
			    namelen, VDNAMESIZE, vh.vh_vd[i].vd_name,
			    vh.vh_vd[i].vd_nbytes, vh.vh_vd[i].vd_lbn);
		}
	}
	printf("\n");
}

void
usage(void)
{
	fprintf(stderr,"usage: makevh name [part=n,size]* [srcfile dstfile]*\n");
}
