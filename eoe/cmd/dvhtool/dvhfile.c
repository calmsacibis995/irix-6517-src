#ident	"dvhtool/dvhfile.c: $Revision: 1.5 $"

#include <sys/param.h>
#include <sys/dvh.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <diskinfo.h>
#include <string.h>
#include <unistd.h>
#include "../fx/dklabel.h"

char *progname;
char **filelist;

struct volume_header vh;

int listit(char *fname);
void usage(void);
int writeit(char *fname, int heads, int sectors);
int copyfile(char *srcfile, int bytes, int fd);
void printvh(struct volume_header *vhp);
char *sginame = "CDROM installation disk";

#define VHDR_PART 8	/* volume header partition */
#define VOL_PART 10	/* entire volume */

main(int cnt, char **args)
{
	char *fname = NULL;
	uint heads = 0, sec_trk = 0, listflg = 0;
	uint pnum, psize, poff;
	char *pinfo;
	int ret = 0;
	int c;
	extern int optind, opterr;
	extern char *optarg;

	progname = args[0];

	opterr = 0;	/* handle errs ourselves */
	while((c=getopt(cnt, args, "n:f:h:s:p:l")) != -1) {
		switch(c) {
		case 'n':	/* specify name for sgi label */
			sginame = optarg;
			break;
		case 'f':
			fname = optarg;
			break;
		case 'h':
			heads = atoi(optarg);
			break;
		case 's':
			sec_trk = atoi(optarg);
			break;
		case 'p':
			pinfo = optarg;
			pnum = atoi(pinfo);
			while(*pinfo && *pinfo != ',')
				pinfo++;
			if(!*pinfo || !pinfo[1])
				usage();
			poff = atoi(++pinfo);
			while(*pinfo && *pinfo != ',')
				pinfo++;
			if(!*pinfo || !pinfo[1] || !(psize=atoi(++pinfo)))
				usage();
			while(*pinfo && *pinfo != ',')
				pinfo++;

			/* set default type, in case not given */
			vh.vh_pt[pnum].pt_type = pnum == VHDR_PART ? PTYPE_VOLUME : PTYPE_RAW;
			if(*pinfo && pinfo[1]) {
				pinfo++;
				for(c=0; c<NPTYPES; c++) {
					if(!(optarg = pttype_to_name(c)))
						continue;	/* some types aren't defined yet */
					if(strcmp(pinfo, pttype_to_name(c)) == 0) {
						vh.vh_pt[pnum].pt_type = c;
						break;
					}
				}
				if(c == NPTYPES)
					printf("unrecognized partition type %s, ignored\n", pinfo);
			}
			vh.vh_pt[pnum].pt_nblks = psize;
			vh.vh_pt[pnum].pt_firstlbn = poff;
			break;
		case 'l':
			listflg = 1;
			break;
		default:
			usage();
		}
	}

	if(!fname)
		usage();


	/* if none are set, and we aren't listing, or if listing, and any
	 * are set, but not all, fail.  This is so a list can be done after
	 * creating, with the same command invocation. */
	if((!listflg && (!heads || !sec_trk || !vh.vh_pt[VHDR_PART].pt_nblks)) ||
		((heads|sec_trk|vh.vh_pt[VHDR_PART].pt_nblks)
		&& (!heads || !sec_trk || !vh.vh_pt[VHDR_PART].pt_nblks)))
		usage();

	if(heads) {
		if((cnt-optind) > NVDIR) {
			fprintf(stderr, "Too many files given, max is %d\n", NVDIR);
			exit(2);
		}
		filelist = &args[optind];
		ret = writeit(fname, heads, sec_trk);
	}

	if(!ret && listflg)
		return listit(fname);
	return ret;
}

void
usage(void)
{
	fprintf(stderr, "Usage: %s -f file [-n labelname] [-l] -h heads -s sec_trk -p %d,off,blks,type [-p #,off,blks,type ...] [file ...]\n",
		progname, VHDR_PART);
	exit(1);
}


listit(char *fname)
{
	int fd = open(fname, O_RDONLY);

	if(fd == -1) {
		fprintf(stderr, "Can't open ");
		perror(fname);
		return 3;
	}

	if(getheaderfromdisk(fd, &vh)) {
		fprintf(stderr, "Volume header on %s isn't valid\n", fname);
		return 4;
	}

	printvh(&vh);

	return 0;
}

/* create and fill a volume header.  The last block is written to cause
 * all  unused blocks to be zero (either as a hole in the file, or with
 * current EFS, by having the kernel zero fill it.
 * We do NOT try to write the last block in the last partition, since this
 * program was written mainly to allow creation of CDROM's, and the EFS
 * partition will be dd'ed from some hard disk drive onto the end of the
 * vhdr partition for normal use.
*/
writeit(char *fname, int heads, int sectors)
{
	struct partition_table *pt;
	struct volume_directory *vdir;
	struct stat st;
	unsigned lastblk = 0;
	int cnt, fd;
	unsigned vsize, voff;
	char buf[BBSIZE], *relname;
	unsigned cyls;
	static struct disk_label sgilabel;

	sgilabel.d_magic = D_MAGIC;
	strncpy(sgilabel.d_name, sginame, sizeof(sgilabel.d_name)-1);

	fd = open(fname, O_WRONLY|O_CREAT|O_TRUNC, 0666);
	if(fd == -1) {
		fprintf(stderr, "Unable to create ");
		perror(fname);
		return(20);
	}

	vh.vh_dp.dp_secbytes = BBSIZE;	/* only one for older proms */

	if(vh.vh_pt[VOL_PART].pt_nblks<=0 || vh.vh_pt[VOL_PART].pt_firstlbn<0) {
		/* setup vol */
		for(cnt=0,pt= vh.vh_pt; cnt < NPARTAB; pt++, cnt++)
			if(pt->pt_nblks > 0 && (pt->pt_nblks + pt->pt_firstlbn) > lastblk)
				lastblk = pt->pt_nblks + pt->pt_firstlbn;
		cyls = howmany(lastblk, sectors * heads);
		vh.vh_pt[VOL_PART].pt_nblks = cyls * heads * sectors;
		vh.vh_pt[VOL_PART].pt_firstlbn = 0;
		vh.vh_pt[VOL_PART].pt_type = PTYPE_VOLUME;
	}
	vh.vh_dp.dp_drivecap = lastblk;

	/* these are necessary here, as in fx, so that CD's can be read on
	 * pre-kudzu systems if made with this dvhfile */
	vh.vh_dp._dp_cylinders = cyls;
	vh.vh_dp._dp_heads = heads;
	vh.vh_dp._dp_sect = sectors;


	vsize = roundup(vh.vh_pt[VHDR_PART].pt_nblks, sectors*heads);
	voff = heads*sectors;	/* first block in second cyl */

	if(lseek(fd, (vsize-1)*BBSIZE, SEEK_SET) == -1) {
		fprintf(stderr, "lseek to end of vhdr partition (blk %u) ", vsize-1);
		perror("fails");
		return(21);
	}
	if((cnt=write(fd, buf, BBSIZE)) != BBSIZE) {
		if(cnt == -1)
			perror("write at end of vhdr partition fails");
		else
			fprintf(stderr, "Oops, could only write %d bytes at end of vh\n ", cnt);
		return(22);
	}
	if(lseek(fd, 0, SEEK_SET) == -1) {
		perror("lseek to start of vhdr partition fails");
		return(23);
	}

	/* put the sgilabel file in */
	vdir = vh.vh_vd;
	relname = "sgilabel";
	strcpy(vdir->vd_name, relname);
	vdir->vd_nbytes = BBSIZE;
	vdir->vd_lbn = voff;
	if(lseek(fd, voff*BBSIZE, SEEK_SET) == -1) {
		fprintf(stderr, "lseek to start of vhdr in %s ", relname);
		perror("fails");
		return(24);
	}
	if((cnt=write(fd, &sgilabel, sizeof(sgilabel))) != sizeof(sgilabel)) {
		if(cnt == -1) {
			fprintf(stderr, "Write error on vh for ");
			perror(relname);
		}	
		else
			fprintf(stderr, "Short write on vh for %s\n", relname);
		return(25);
	}
	voff += howmany(BBSIZE, BBSIZE);

	for(vdir++ ; *filelist; filelist++) {
		if(stat(*filelist, &st) == -1) {
			fprintf(stderr, "%s skipped, couldn't ", *filelist);
			perror("stat");
			continue;
		}
		relname = strrchr(*filelist, '/');
		if(!relname)
			relname = *filelist;
		else
			relname++;
		if(strlen(relname) >= BFNAMESIZE) {
			printf("filename %s is too long, skipped\n", relname);
			continue;
		}
		if((voff + howmany(st.st_size, BBSIZE)) > vsize) {
			fprintf(stderr, "%s skipped, no room in vh\n", *filelist);
			continue;
		}
		strcpy(vdir->vd_name, relname);
		vdir->vd_nbytes = st.st_size;
		vdir->vd_lbn = voff;
		vdir++;
		if(lseek(fd, voff*BBSIZE, SEEK_SET) == -1) {
			fprintf(stderr, "lseek to start of vhdr in %s ", relname);
			perror("fails");
			return(24);
		}
		if(copyfile(*filelist, st.st_size, fd))
			return(25);
		voff += howmany(st.st_size, BBSIZE);
	}

	if(putdiskheader(fd, &vh, 1)) {
		fprintf(stderr, "Unable to write volume header structure\n");
		return(26);
	}
	close(fd);
	return 0;
}


/* copy from the given name to the given fd */
copyfile(char *srcfile, int bytes, int fd)
{
	char buf[BBSIZE*128];
	int cnt, amt;
	int sfd = open(srcfile, O_RDONLY);

	if(sfd == -1) {
		fprintf(stderr, "Unable to open ");
		perror(srcfile);
		return -1;
	}

	for(; bytes>0; bytes-=cnt) {
		if(bytes > sizeof(buf))
			cnt = sizeof(buf);
		else
			cnt = bytes;
		if((amt=read(sfd, buf, cnt)) != cnt) {
			if(amt == -1) {
				fprintf(stderr, "Read error on ");
				perror(srcfile);
			}	
			else
				fprintf(stderr, "Short read on %s\n", srcfile);
			return -1;
		}
		if((amt=write(fd, buf, cnt)) != cnt) {
			if(amt == -1) {
				fprintf(stderr, "Write error on vh for file ");
				perror(srcfile);
			}	
			else
				fprintf(stderr, "Short write on vh for file %s\n", srcfile);
			return -1;
		}
	}
	return 0;
}

void
printvh(struct volume_header *vhp)
{
	struct partition_table *pt;
	struct volume_directory *vdir;
	int p;
	char *ptype;
	unsigned cyls;

	for(p=0,pt= vhp->vh_pt; p < NPARTAB; pt++, p++)
		if(pt->pt_nblks > 0) {
			ptype = pttype_to_name(pt->pt_type);
			printf("Partition %2d: %7u blocks at %7u, type %s\n",
				p, pt->pt_nblks, pt->pt_firstlbn, ptype ? ptype : "unknown");
		}

	for(p=0,vdir= vhp->vh_vd; p < NVDIR; vdir++, p++)
		if(*vdir->vd_name)
			printf("File %*s: %8u bytes at block %5u\n", BFNAMESIZE,
				vdir->vd_name, vdir->vd_nbytes, vdir->vd_lbn);
}
