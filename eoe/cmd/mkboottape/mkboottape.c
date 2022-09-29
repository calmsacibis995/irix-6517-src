/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
#ident "mkboottape/mkboottape.c: $Revision: 1.5 $"

/*
 * mkboottape [-f FILE_OR_DEVICE] file_list
 * exit(99) means major error
 * exit(<99) means error on individual file
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "tpd.h"

/* NOTE: at SGI, the QIC-* boot tapes are supposed to be byte-swapped.
 * This is stupid, but it is too late to change... */
char *tapedev = "/dev/tape";

char *progname;
int errflg;
union tp_header th;

#define	MAXPATH		256

struct filelist {
	char name[TP_NAMESIZE];
	char fullname[MAXPATH];
	int len;
} filelist[TP_NENTRIES];

union endian {
	char c[4];
	int i;
} endian;
int leflag;	/* non-zero if little-endian */
#define	TO_BE(x)	(leflag ? nuxi(x) : x)

void extract(int cnt, char **args);
int opentape(int mode);
unsigned nuxi(unsigned x);
int cksum(register int *ip, unsigned bcnt);

main(argc, argv)
int argc;
register char **argv;
{
	struct stat st;
	register struct filelist *fp, *fpt;
	register struct tp_entry *te;
	char namebuf[TP_NAMESIZE];
	char buf[TP_BLKSIZE];
	char *cp;
	int fd;
	int tfd;
	int type;
	int curblock;
	int bytes;
	int lflag = 0, xflag = 0;

	init_endian();
#ifdef DEBUG
	fprintf(stderr, "this is a %s-endian machine\n",
	    leflag ? "little" : "big");
#endif
	fp = filelist;
	progname = *(argv++);
	argc--;

	/*
	 * process flags
	 */
	for (; argc > 0 && **argv == '-'; argc--, argv++) {

		switch ((*argv)[1]) {
		case 'x':	/* extract */
			xflag++;
			break;
		case 'l':	/* list */
			lflag++;
			break;
		case 'f':
			if (--argc <= 0) {
				goto usage;
			}
			tapedev = *++argv;
			break;
		
		default:
usage:
			fprintf(stderr, "Usage: %s [ -f tapedev ] [ -l | -x [ file ... ] | file ...]\n",
			    progname);
			exit(99);
		}
	}

	if ( lflag )
		list();

	if( xflag )
		extract(argc, argv);

	for (; argc > 0; argc--, argv++) {
		/*
		 * open file, stat it, add vitals to filelist
		 */
		fd = open(*argv, O_RDONLY);
		if (fd < 0) {
			fprintf(stderr, "%s: Can't open %s\n",
			    progname, *argv);
			errflg++;
			continue;
		}
		if (fstat(fd, &st) < 0) {
			fprintf(stderr, "%s: Can't stat %s\n",
			    progname, *argv);
			goto err;
		}
		type = st.st_mode & S_IFMT;
		if (type != S_IFREG) {
			fprintf(stderr, "%s: Not a regular file: %s\n",
			    progname, *argv);
			goto err;
		}
		cp = strrchr(*argv, '/');
		if (cp == NULL)
			cp = *argv;
		else
			cp++;		/* skip slash */
		if (strlen(*argv) >= MAXPATH-1) {
			fprintf(stderr, "%s: Pathname too long: %s\n",
			    progname, *argv);
			goto err;
		}
		if (strlen(cp) >= TP_NAMESIZE) {
			strncpy(namebuf, *argv, TP_NAMESIZE-1);
			namebuf[TP_NAMESIZE-1] = 0;
			cp = namebuf;
			fprintf(stderr,
			   "%s: WARNING: filename %s truncated to %s\n",
			   progname, *argv, cp);
			goto err;
		}
		for (fpt = filelist; fpt < fp; fpt++)
			if (strcmp(cp, fpt->name) == 0) {
				fprintf(stderr,
				    "%s: Duplicate file name: %s\n",
				    progname, cp);
				goto err;
			}
		if (fp >= &filelist[TP_NENTRIES]) {
			fprintf(stderr,
			    "%s: Directory overflow: %s not on tape\n",
			    progname, *argv);
err:
			errflg++;
			close(fd);
			continue;
		}
		strcpy(fp->fullname, *argv);
		strcpy(fp->name, cp);
		fp->len = st.st_size;
		fp++;
		close(fd);
	}
	if (fp == filelist) {
		/*
		 * we assume this wasn't really intended
		 */
		fprintf(stderr, "%s: no files specified\n", progname);
		exit(99);
	}
	/*
	 * build tape directory
	 */
	bzero(&th, sizeof(th));
	th.th_td.td_magic = TO_BE(TP_MAGIC);
	te = th.th_td.td_entry;
	curblock = 1;	/* start data blocks after directory */
	for (fpt = filelist; fpt < fp; fpt++, te++) {
		strcpy(te->te_name, fpt->name);
		te->te_nbytes = TO_BE(fpt->len);
		te->te_lbn = TO_BE(curblock);
		curblock += (fpt->len + TP_BLKSIZE - 1) / TP_BLKSIZE;
	}
	th.th_td.td_cksum = (unsigned)cksum((int *)&th, sizeof(th));
	th.th_td.td_cksum = TO_BE(th.th_td.td_cksum);
	/*
	 * open tape and write directory
	 */
	tfd = opentape(O_RDWR|O_CREAT);

	if (write(tfd, &th, sizeof(th)) != sizeof(th)) {
		fprintf(stderr, "%s: Can't write to %s\n", progname, tapedev);
		exit(99);
	}
	/*
	 * scan filelist and write files
	 */
	for (fpt = filelist; fpt < fp; fpt++) {
#ifdef DEBUG
		printf ("Processing %s\n", fpt->fullname);
#endif /* DEBUG */
		if ((fd = open(fpt->fullname, O_RDONLY)) < 0) {
			fprintf(stderr, "%s: Couldn't reopen %s\n",
			    progname, fpt->fullname);
			exit(99);
		}
		while (fpt->len) {
			bytes = fpt->len > TP_BLKSIZE ? TP_BLKSIZE : fpt->len;
			if (bytes != TP_BLKSIZE)
				bzero(buf, TP_BLKSIZE);
			if (read(fd, buf, bytes) != bytes) {
				fprintf(stderr, "%s: Can't read %s\n",
				    progname, fpt->fullname);
				exit(99);
			}
			if (write(tfd, buf, TP_BLKSIZE) != TP_BLKSIZE) {
				fprintf(stderr, "%s: Write to %s failed\n",
				    progname, tapedev);
				exit(99);
			}
			fpt->len -= bytes;
		}
		close(fd);
	}
	close(tfd);
	exit(errflg);
}

init_endian()
{
	endian.i = 0x01020304;
	leflag = (endian.c[0] == 4);
}

cksum(register int *ip, unsigned bcnt)
{
	register sum;
	register int *ep;

	if (bcnt & 0x3) {
		fprintf(stderr, "%s: Internal error: bad tpd size\n",
		    progname);
		exit(99);
	}
	ep = ip + (bcnt >> 2);
	sum = 0;
	while (ip < ep) {
		sum += TO_BE(*ip);
		ip++;
	}
	return(-sum);
}

unsigned
nuxi(unsigned x)
{
	return(
	    ((x&0xff)<<24)
	   |((x&0xff00)<<8)
	   |((x&0xff0000)>>8)
	   |((x&0xff000000)>>24)
	);
}

list()
{
	int i;

	(void)opentape(O_RDONLY);
	
	printf("Filename       Size\n");
	for ( i = 0; th.th_td.td_entry[i].te_name[0] && i < TP_NENTRIES; i++ )
		printf("%-14s %u\n",
			th.th_td.td_entry[i].te_name,
			TO_BE(th.th_td.td_entry[i].te_nbytes));
	
	exit(0);
}

/* extract named files from tape directory; if argc is 0, extract
 * all of the files.  Note that, unlike dvhtool, the extracted
 * files are not null padded up to the tape block size.
*/
void
extract(int cnt, char **args)
{
	int tfd, fd, i, f, x;
	unsigned curblkno;
	register struct tp_entry *te;
#define TPB_SIZ 128	/* tape blocks in buf */
	char tpbuf[TP_BLKSIZE*TPB_SIZ];
	
	tfd = opentape(O_RDONLY);
	curblkno = 1;	/* we are positioned after the dir now */

	if(cnt) {	/* mark files that are NOT to be extracted; done
		* this way because we want to extract in directory order,
		* in order to avoid tape repositioning. */
		int found =0;
		for(te=th.th_td.td_entry,i=0; te->te_name[0] && i<TP_NENTRIES;
			i++, te++) {
			for(f=0; f<cnt; f++)
				if(strcmp(args[f], te->te_name) == 0) {
					*args[f] = '\0';
					found++;
					break;
				}
			if(f == cnt)
				te->te_lbn = 0;	/* skip it */
		}
		for(f=0; f<cnt; f++)
			if(*args[f])
				fprintf(stderr, "%s not on tape, ignored\n", args[f]);
		if(!found) {
			fprintf(stderr, "nothing to extract\n");
			exit(99);	/* oops, none found! */
		}
	}

	/* now extract the requested files */
	for(te=th.th_td.td_entry,i=0; te->te_name[0] && i<TP_NENTRIES;
		i++, te++) {
		unsigned nblks;
		if(!te->te_lbn)
			continue;
		nblks = (te->te_nbytes+TP_BLKSIZE-1)/TP_BLKSIZE;
		if((fd=creat(te->te_name, 0666)) == -1) {
			fprintf(stderr, "Couldn't create %s, skipped: %s\n",
				te->te_name, sys_errlist[errno]);
			continue;
		}
		te->te_lbn = TO_BE(te->te_lbn);
		te->te_nbytes = TO_BE(te->te_nbytes);
		if(curblkno != te->te_lbn) {	/* skip to correct block */
			unsigned nskip;
			nskip = te->te_lbn - curblkno;
			for( ; nskip >= TPB_SIZ; nskip-=TPB_SIZ) {
				if((cnt=read(tfd, tpbuf, sizeof(tpbuf))) != sizeof(tpbuf)) {
skiperr:				 if(cnt == -1)
						perror("Error while skipping blocks");
					else
						fprintf(stderr, "Short read while skipping blocks\n");
					exit(99);
				}
			}
			if((cnt=read(tfd, tpbuf, nskip*TP_BLKSIZE)) != nskip*TP_BLKSIZE)
				goto skiperr;
		}

		/* position if file completely extracted */
		curblkno = te->te_lbn + nblks;

		while(te->te_nbytes) {
			if(te->te_nbytes >= sizeof(tpbuf))
				x = f = sizeof(tpbuf);
			else {
				x = te->te_nbytes;
				f = ((x+TP_BLKSIZE-1)/TP_BLKSIZE) * TP_BLKSIZE;
			}
			if((cnt=read(tfd, tpbuf, f)) != f) {
				if(cnt == -1)
					fprintf(stderr, "Tape error while extracting %s: %s\n",
						te->te_name,sys_errlist[errno]);
				else
					fprintf(stderr, "Short read while extracting %s\n",
						te->te_name);
				exit(99);
			}
			if((cnt=write(fd, tpbuf, x)) != x) {
				if(cnt == -1)
					fprintf(stderr, "Write error while extracting %s: %s\n",
						te->te_name,sys_errlist[errno]);
				else
					fprintf(stderr, "Short write while extracting %s\n",
						te->te_name);
				exit(99);
			}
			te->te_nbytes -= x;
		}
		close(fd);
	}
	exit(0);
}

int
opentape(int mode)
{
	int fd;

	/* third open arg used only when creating... */
	if ((fd = open(tapedev, mode, 0666)) < 0) {
		fprintf(stderr, "%s: Can't open %s - %s\n",
				progname, tapedev, sys_errlist[errno]);
		exit(99);
	}

	if(mode == O_RDONLY) {
		if (read(fd, &th, sizeof(th)) != sizeof(th)) {
			fprintf(stderr, "%s: Can't read from %s - %s\n",
					progname, tapedev, sys_errlist[errno]);
			exit(99);
		}

		if ( th.th_td.td_magic != TO_BE(TP_MAGIC) ) {
			fprintf(stderr,"%s: bad magic number for tape directory\n",
					progname);
			exit(99);
		}
	}
	return fd;
}
