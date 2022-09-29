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

#ident	"dvhtool/dvhtool.c: $Revision: 1.45 $"
/*
 * dvhtool.c -- tool for moving files into and out of the volhdr.
 * All other functions removed for ficus (6.3) and beyond.  Use fx or
 * prtvtoc for other functions it used to have.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/dvh.h>
#include <sys/dkio.h>
#include <sys/stat.h>
#include <sys/iograph.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <invent.h>
#include <limits.h>
#include <diskinfo.h>

char *tmpdir = "/tmp";

#ifndef DEV_BSIZE
#define DEV_BSIZE 512	/* default size to use if not set */
#endif

#define	streq(x,y)	(strcmp(x,y)==0)
#define	strneq(x,y,c)	(strncmp(x,y,c)==0)


/*
 * argument structure for volume directory manipulation routine
 */
struct vd_arg {
	int opt;
	char *unix_file;
	char *dvh_file;
};

#define	MAXVD_ARG	(2*NVDIR)	/* large enough to delete all, then add all */

/*
 * options for volume directory manipulation
 */
#define VD_NOP		-1
#define VD_ASK		0
#define VD_ADD		1
#define VD_DELETE	2
#define VD_LIST		3
#define VD_CREAT	4
#define VD_GET		5


int vfd = -1;	/* fd for volume header; set in getdvh() */
int readonly = 1;	/* if non-zero, open vh readonly */
int needcopy = 0;	/* do we really need to copy everything to /tmp first? */
int mypid;
int dev_bsize; /* size of one sector */
int vhdirty = 0;			/* volume header is dirty */
int firstfileblk = 2;	/* first blk in volhdr for files; should match fx
	* code in ls_init() in lb.c.
	* Note: originally the volume_header struct was supposed to be written
	* in sector 0 of each track in cyl 0 for redundancy.  Due to a bug in
	* dvhtool and fx, this wasn't enforced until 3.2.  The redundant copies
	* have never been used by anything, and few people knew about them, and
	* NOW (2/92) we find that we need to put much larger files in the volhdr.
	* We can't go out and increase the size of the existing volhdr partition
	* on thousands of machines, so now we are deliberately going back to
	* NOT making the redundant copies, and allowing use of the whole volhdr
	* partition (except sector 0, of course) for files.  As I expect us to
	* also have to support FATs on at least some systems in the future, I'm
	* making this 2 instead of 1.  This big comment is so that no one will
	* come back in the future and 'fix' this again...  Dave Olson
	*/

struct volume_header vh;	/* current header being edited */
char save_volhdr[PATH_MAX] = "/dev/rvh";	/* default vhdr device */
char *bootname = (char *)0;	/* new bootfile name */
int rootpart = -1, swappart = -1; /* so user can set these also */
int showboot = 0;

struct vd_arg vd_arg[MAXVD_ARG];	/* args for vol. dir. stuff */
int numvd_arg = 0;	/* number of vol. dir. args */
char *file_list[NVDIR];	/* list of files to cleanup on exit or after
	copied back to vh */
int interactive;	/* if zero, exit on 'fatal' errors */

void usage(void);
char *cmdline(int, char **);
void do_interactive(char *);
int getdvh(char *, int);
void copy_file(struct volume_directory *, char *, int);
void new_tmpfile(char *);
void rmtmpfiles(void);
void cleanup(int);
void edit_vd(void);
void vd_ask(void);
void vd_list(void);
void vd_delete(char *, int);
void vd_creat(char *, char *, int);
void vd_get(char *, char *);
int new_copy(char *, char *);
void putdvh(void);
int vh_checksum(struct volume_header *);
int min(int, int);
char *atob(char *, int *);
unsigned digit(char);
void show_bootinfo(void);
void change_bootinfo(char *);
int vh_availspace(void);
int openvh(char *volhdr);
void copytotmp(void);
void copy_tovh(char *, daddr_t, int);
struct partition_table *getvhpart(void);
int lbncmp(const void*, const void*);
void sortdir(struct volume_directory *);
int vh_contig(int);
int roundblk(int);
void eprintf(char *, ...);
void fail(void);

main(int argc, char *argv[])
{
	char *volhdr, *tmp;

	mypid = getpid();
	signal(SIGINT, cleanup);
	signal(SIGHUP, cleanup);
	signal(SIGTERM, cleanup);

	tmp = getenv("TMPDIR");
	if(tmp && *tmp)
		tmpdir = tmp;

	argc--;
	argv++;

	if(argc == 1 && **argv != '-') {
		readonly = 0;	/* never know in interactive mode */
		volhdr = *argv;
		argc--;
		argv++;
	}
	else
		volhdr = NULL;

	if (argc == 0) {
		do_interactive(volhdr);
		/* NOTREACHED */
	}

	volhdr = cmdline(argc, argv);

	if(getdvh(volhdr, 0) != -1) {
		if (bootname)
			change_bootinfo(bootname);
		if(showboot)
			show_bootinfo();
		if (numvd_arg)
			edit_vd();
		if(vhdirty)
			putdvh();
	}
	cleanup(0);
	/*NOTREACHED*/
}

void
usage(void)
{
	printf("usage: dvhtool [-v [add unix_file dvh_file]\n");
	printf("                   [create unix_file dvh_file]\n");
	printf("                   [delete dvh_file]\n");
	printf("                   [get dvh_file unix_file]\n");
	printf("                   [list] ] [vh]\n");
	printf("   (only first letter of suboptions need to be given)\n");
}


/*	process command line arguments (non-interactive), returns ptr to
	volume header name to use.  Doesn't perform any of the actions,
	just stuffs them away in vd_arg and other globals for processing
	in edit_vd, etc. */
char *
cmdline(int argc, char **argv)
{
	int c, ind = 0, newpt;
	char *optarg;

	for(ind=0; ind<argc && argv[ind][0] == '-' && (c=argv[ind][1]); ind++) {

		if(!(optarg = argv[++ind]))
			goto usage_err;

		switch(c) {
		case 'b':	/* this is deliberately not documented.  It's
			* still here, for the miniroot scripts that revert 
			* the root partition back to the "real" value */
			if(strncmp(optarg, "list", strlen(optarg)) == 0) {
				/* note that only the ending config is shown */
				showboot = 1;
				break;
			}
			bootname = optarg;
			/* if more args, and they are digits, they are root and swap parts */
			if(ind<(argc-1) && !*atob(argv[ind+1], &newpt)) {
				ind++;
				if(newpt >= NPARTAB)
					printf("partition # %d not valid, must be less than %d\n", 
						newpt, NPARTAB);
				else
					rootpart = newpt;
			}
			if(ind<(argc-1) && !*atob(argv[ind+1], &newpt)) {
				ind++;
				if(newpt >= NPARTAB)
					printf("partition # %d not valid, must be less than %d\n", 
						newpt, NPARTAB);
				else
					swappart = newpt;
			}
			readonly = 0;
			break;

		case 'v': /* manipulate volume directory */
			if (numvd_arg >= MAXVD_ARG) {
			    printf("dvhtool: too many -v commands\n");
			    exit(1);
			}	
			c = *optarg;
			switch(c) {
			case 'a':
			case 'c': /* create/add an entry in the volume directory */
				if((argc-ind) < 3)
					goto usage_err;
				vd_arg[numvd_arg].opt = (c == 'c') ? VD_CREAT : VD_ADD;
				vd_arg[numvd_arg].unix_file = argv[++ind];
				vd_arg[numvd_arg++].dvh_file  = argv[++ind];
				readonly = 0;
				break;
			case 'g':	/* get a file from the volume header */
				if((argc-ind) < 3)
					goto usage_err;
				vd_arg[numvd_arg].opt = VD_GET;
				vd_arg[numvd_arg].dvh_file  = argv[++ind];
				vd_arg[numvd_arg++].unix_file = argv[++ind];
				break;
			case 'd': /* delete an entry from volume directory */
				if((argc-ind) < 1)
					goto usage_err;
				vd_arg[numvd_arg].opt = VD_DELETE;
				vd_arg[numvd_arg++].dvh_file  = argv[++ind];
				readonly = 0;
				break;
			case 'l': /* list volume directory entries */
				vd_arg[numvd_arg++].opt = VD_LIST;
				break;
			default:
				goto usage_err;
			}
			break;

		default:
			goto usage_err;
		}
	}

	return ind==argc || argv[ind][0] == '-' ? save_volhdr : argv[ind];

usage_err:
	usage();
	exit(1);
	/*NOTREACHED*/
}


/* run in interactive mode (never returns) */
void
do_interactive(char *volhdr)
{
	char buf[PATH_MAX];

	interactive = 1;	/* no exit on errors */

	readonly = 0;	/* never know in interactive mode */
	(void)getdvh(volhdr, 1);	/* need vh before anything else */

	for (;;) {
		printf("\nCommand? (read, vd, write, or quit): ");
		if(gets(buf) == NULL)
			continue;	/* read error, etc. */
		switch (*buf) {
			case 'r':
				(void)getdvh((char *)0, 1);
				break;

			case 'v':
				vd_arg[numvd_arg].opt = VD_ASK;
				numvd_arg = 1;
				edit_vd ();
				break;

			case 'w':
				putdvh();
				break;

			case 'q':
				if(vhdirty) {
					printf("header needs to be written, do you want to? (n if not) ");
					if(gets(buf) && *buf != 'n' && *buf != 'N')
						putdvh();
				}
				cleanup(0);
				break;

			case '\0':
				break;	/* just a return, ignore it */

			default:
				printf("Unknown keyword %s\n", buf);
				break;
		}
	}
	/* NOTREACHED */
}

/* Daveh note 12/12/89: substantial error checking added to getdvh. Previously
 * it would take any old name & open it! AND write to it!! AAARRGGHHH!!!!
 */

int
getdvh(char *volhdr, int force)
{
	char buf[PATH_MAX];
	int i;
	char *name;
	char *rawname;
	struct stat sb;

	if(volhdr)
		name = volhdr;
	else
	{
forced:
		if(name) {	/* got here via goto; make cleanup consistent */
			memset(&vh, '\0', sizeof(vh));
			if (!force)
				return (-1);
			volhdr = NULL;
		}

		printf("Volume? (%s) ", save_volhdr);
		name = (gets(buf) && *buf) ? buf : save_volhdr;
	}

	if ((*name == 'q') || (*name == 'Q')) /* allow graceful quit */
		exit(0);
	if (stat(name, &sb) < 0)
	{
		eprintf("Can't access %s\n", name);
		if (!force)
			return (-1);
		else
			goto forced;
	}
	switch (sb.st_mode & S_IFMT) 
	{
	case S_IFCHR :	rawname = name;
			break;
	case S_IFBLK :
			rawname = findrawpath(name);
			if (rawname)
			{
				break;
			}
			else if (!force)
				return (-1);
			else
			{
				fprintf(stderr,"dvhtool needs to work on char device but %s is block\n",name);
				eprintf("Can't find equivalent char device \n");
				goto forced;
			}
	default:
			eprintf("%s is not a device.\n", name);
			if (!force)
				return (-1);
			else
				goto forced;
	}

	i = openvh(rawname);
	if(i == -1) {
		if(force) {
			if(volhdr)
				strncpy(save_volhdr, volhdr, sizeof(save_volhdr)-1);
			goto forced;
		}
		return -1;
	}

	/*
	 * Daveh note 12/12/89: we now always get the vh from the driver,
	 * since some disks turned up where the on-disk copy was slightly
	 * wrong: the driver sanity-checks it for us. The stat check 
	 * above ensures that vfd refers to a char device.
	 */

	if(ioctl(vfd, DIOCGETVH, &vh) == -1) 
	{
		eprintf("dvhtool: Can't get volume header - %s\n", volhdr);
		goto forced;
	}

	if(valid_vh(&vh)) {
		eprintf("dvhtool: invalid volume header returned by driver - %s\n", volhdr);
		goto forced;
	}

	/* make sure that the vh read directly is also valid; used to figure
	 * out partition number from rdev and look at type.  With hwgraph that
	 * won't work right.  also use fdes_to_devname, so that we won't
	 * accept the volume device or other devices that start at block 0,
	 * just the volhdr device.  This means we had better not go changing
	 * the canonical names of devices in the kernel...
	 * Notice that since dvhtool no longer does any initialization of the
	 * volhdr we are now requiring that the ondisk volhdr struct be valid,
	 * as well as the in memory copy.
	*/
	{	struct volume_header vh2;
		char canonical[PATH_MAX];
		int len = sizeof(canonical);
		if(getheaderfromdisk(i, &vh2)) {
			eprintf("%s does not appear to have a valid volume header\n", name);
			goto forced;
		}
		if(!fdes_to_devname(i, canonical, &len) || !strstr(canonical, 
			 "/" EDGE_LBL_DISK "/" EDGE_LBL_VOLUME_HEADER "/")) {
			eprintf("%s does not appear to be a volume header device\n", name);
			goto forced;
		}
	}

	strcpy(save_volhdr, rawname);  /* save checked name as new default. */

	dev_bsize = vh.vh_dp.dp_secbytes;

	if(dev_bsize == 0)	/* a lot of older disks had their
		secbytes set to 0, and it wasn't noticed because nothing used it! */
		dev_bsize = DEV_BSIZE;

	return 0;
}

void
copy_file(struct volume_directory *vdp, char *name, int istmp)
{
	register int bytes, res;
	register int cc;
	register int tfd;
	char buf[8192];

	if ((tfd = open(name, O_RDWR|O_CREAT|O_TRUNC, 0664)) < 0) {
		fprintf(stderr,"can't open temp file ");
		perror(name);
		goto bad;
	}

	if(istmp)
		new_tmpfile(name);

	if (lseek(vfd, vdp->vd_lbn * dev_bsize, 0) < 0) {
		fprintf(stderr,"seek to block %d", vdp->vd_lbn);
		perror("failed");
		goto bad;
	}
	for (bytes = vdp->vd_nbytes; bytes > 0;) {
		/* roundblk because 'old' fx used to write the actual size of
			the sgilabel, instead of rounding up to blksize. Other 
			programs might do this also, so be paranoid. */
		cc = roundblk(min(sizeof(buf), bytes));
		res = read(vfd, buf, cc);
		if(res != cc) {
			fprintf(stderr,"read error on volume header: file %.*s\n",
				VDNAMESIZE, vdp->vd_name);
			goto bad;
		}
		bytes -= cc;
		if(cc < sizeof(buf))	/* round up on last read */
			cc = roundblk(cc);
		if (write(tfd, buf, cc) != cc) {
			fprintf(stderr,"write error on temp file %s\n", name);
			goto bad;
		}
	}
	close(tfd);
	return;

bad:
	perror("dvhtool: fatal I/O error, cannot proceed ");
	cleanup(1);
}

void
new_tmpfile(char *fn)
{
	register int i;
	register char *cp;

	for (i = 0; file_list[i] && i < NVDIR; i++)
		continue;
	if (i >= NVDIR) /* shouldn't ever happen... */
		eprintf("too many temp files\n");
	else {
		cp = malloc(strlen(fn)+1);
		strcpy(cp, fn);
		file_list[i] = cp;
	}
}

/*	unlink tmpfiles and free storage; called after written to vh
	and we no longer need them.  Also on exit.
*/
void
rmtmpfiles(void)
{
	register int i;

	for(i = 0;  i < NVDIR; i++)
		if(file_list[i]) {
			(void)unlink(file_list[i]);
			free(file_list[i]);
			file_list[i] = NULL;
		}
}


void
cleanup(int val) /* val is explicit value, or signal number */
{
	rmtmpfiles();
	exit(val);
}



/*
 * manipulate volume directory.  All deletes are done before creats and
 * adds.  This allows creats and adds to reliably tell how much space
 * is available; which means we often won't have to copy all the files
 * to the tmpdir first.  All others including gets and lists are done in
 * the specified order.
 */
void
edit_vd(void)
{
	register int i;
	int pass = 0, adds = 0;
	register struct vd_arg *vd_argp;

again:
	for (vd_argp=vd_arg, i=0; i < numvd_arg; i++, vd_argp++) {
		switch (vd_argp->opt) {
		case VD_ASK:	/* interactive */
			vd_ask();
			break;

		case VD_CREAT:
		case VD_ADD:
			if(pass == 0)
				adds++;
			else
				vd_creat(vd_argp->unix_file, vd_argp->dvh_file,
					vd_argp->opt==VD_CREAT);
			break;

		case VD_DELETE:
			vd_delete(vd_argp->dvh_file, 1);
			vd_argp->opt = VD_NOP;
			break;

		case VD_LIST:	/* a bit tricky since have to do this in
			the correct order; may be needed on both first and
			second pass, (and at multiple points in first) */
			if(pass || !adds) {
				vd_list();
				vd_argp->opt = VD_NOP;
			}
			break;

		case VD_GET:
			vd_get(vd_argp->dvh_file, vd_argp->unix_file);
			vd_argp->opt = VD_NOP;
			break;
		}
	}
	if(pass++ == 0 && adds)
		goto again;
}

/* 'v' done interactively, no from command line */
void
vd_ask(void)
{
	char *unix_file;
	char *dvh_file;
	register char *cp;
	char buf[PATH_MAX];

	for(;;) {
		printf("(d FILE, a UNIX_FILE FILE, c UNIX_FILE FILE, g FILE UNIX_FILE or l)?\n\t");
		if(gets(buf) == NULL)
			return;
		switch (buf[0]) {
		case 'l':
			vd_list();
			break;

		case 'd':
			for (cp = &buf[1]; isspace(*cp); cp++)
				continue;
			if(!*cp) 
				printf("must give filename\n");
			else
				vd_delete(cp, 1);
			break;

		case 'a':
		case 'c':
		case 'g':
			for (cp = &buf[1]; isspace(*cp); cp++)
				continue;
			unix_file = cp;
			while (*cp && !isspace(*cp))
				cp++;
			if(*cp)
				*cp++ = 0;
			while (isspace(*cp))
				cp++;
			dvh_file = cp;
			if(!*unix_file || !*dvh_file) {
				printf("must give both filenames\n");
				break;
			}
			if (buf[0] == 'c')
				vd_creat(unix_file, dvh_file, 1);
			else if (buf[0] == 'a')
				vd_creat(unix_file, dvh_file, 0);
			else
				vd_get(unix_file, dvh_file);	/* actual dvh, unix... */
			break;

		case '\0':
		case 'q':
			return;	/* return to top level on empty line or 'q' */

		default:
			printf("illegal command %s\n", buf);
			break;
		}
	}
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

/*
 * delete a volume directory entry
 */
void
vd_delete(char *dvh_file, int warn)
{
	register int i;

	for (i = 0; i < NVDIR; i++) {
		if (vh.vh_vd[i].vd_nbytes
		    && strneq(vh.vh_vd[i].vd_name, dvh_file, VDNAMESIZE )) {
			/* different programs look at different things... */
			vh.vh_vd[i].vd_nbytes = 0;
			vh.vh_vd[i].vd_lbn = -1;
			*vh.vh_vd[i].vd_name = '\0';
			vhdirty = 1;
			sortdir(vh.vh_vd); /* keep in sorted order just for appearances,
				and in case any older programs counted on it. */
			break;
		}
	}
	if(warn && i == NVDIR)
		printf("%s not found in volume header\n", dvh_file);
}

/*
 * create an entry in the volume directory
 */
void
vd_creat(char *unix_file, char *dvh_file, int creat)
{
	register int i;
	register int empty;
	register int bytes;
	struct stat sbuf;
	daddr_t lbn;

	if(strlen(dvh_file) > VDNAMESIZE) {
		eprintf("volume header filenames must be no longer than %d characters\n",
			VDNAMESIZE);
		return;
	}
	if(creat)	/* delete first in case replacing; otherwise might
		needlessly have to copy things to tmpdir */
		(void)vd_delete(dvh_file, 0);
	empty = -1;
	for (i = 0; i < NVDIR; i++) {
		if (empty < 0 && vh.vh_vd[i].vd_nbytes == 0)
			empty = i;
		if (vh.vh_vd[i].vd_nbytes
		    && strneq(vh.vh_vd[i].vd_name, dvh_file, VDNAMESIZE )) {
			if (creat) {
				eprintf("internal error: file %s found after being deleted\n");
				return;
			} else {
				eprintf("There is already a file %s in volume header\n", dvh_file);
				return;
			}
		}
	}
	if (empty == -1) {
		eprintf("volume header directory full\n");
		return;
	}
	if(stat(unix_file, &sbuf) == -1) {
		eprintf("can't open %s\n", unix_file);
		return;
	}

	if(!sbuf.st_size)
		sbuf.st_size = 1;	/* special case, because a length of 0 is
			a marker for an empty slot, and we can mess up the
			volhdr */
	bytes = roundblk(sbuf.st_size);	/* round up to blks */

	if(vh_availspace() < bytes) {
		eprintf("no room in volume header for %s\n", dvh_file);
		return;
	}

	/* if there is enough room avail, find out if there is a contiguous
		chunk big enough; if not, we will have to copy all files there
		to tmpdir, then copy them all back. */
	if((lbn = vh_contig(bytes)) == -1) {
		if(new_copy(unix_file, dvh_file) <= 0)
			return;
		needcopy = 1;
	}
	else
		copy_tovh(unix_file, lbn, bytes);

	/* we've been resorted in vh_contig (and possibly elsewhere).
		look for a free entry again */
	for (i = 0; i < NVDIR; i++) {
		if (vh.vh_vd[i].vd_nbytes == 0)
			break;
	}
	strncpy(vh.vh_vd[i].vd_name, dvh_file, VDNAMESIZE);
	vh.vh_vd[i].vd_nbytes = bytes;
	vh.vh_vd[i].vd_lbn = lbn;
	vhdirty = 1;
	sortdir(vh.vh_vd); /* keep in sorted order just for appearances,
		and in case any older programs counted on it. */
}

/*
 * copy a file from volume header to unix file
 */
void
vd_get(char *dvhfile, char *unixfile)
{
	register int i;
	for(i = 0; i < NVDIR; i++) {
		if (vh.vh_vd[i].vd_nbytes
		    && strneq(vh.vh_vd[i].vd_name, dvhfile, VDNAMESIZE)) {
			copy_file(&vh.vh_vd[i], unixfile, 0);
			return;
		}
	}
	printf("%s not found in volume header\n", dvhfile);
}


/* make a 'copy' of the unix file in /tmp; used to actually make the
	copy, but now makes a symlink for faster copies.
*/
new_copy(char *unix_file, char *dvh_file)
{
	register int ufd = -1;
	char fn[PATH_MAX];
	struct stat sbuf;
	static char curdir[MAXPATHLEN+1];
	char symname[MAXPATHLEN+100];

	if(!*curdir) {
		if(getwd(curdir) == NULL) {
			eprintf("Can't get current directory: %s\n", curdir);
			return -1;
		}
	}
	if ((ufd = open(unix_file, O_RDONLY)) < 0) {
		eprintf("can't open %s\n", unix_file);
		return -1;
	}
	sprintf(fn, "%s/%s.%d", tmpdir, dvh_file, mypid);
	(void)unlink(fn);
	if(*unix_file != '/') {
		sprintf(symname, "%s/%s", curdir, unix_file);
		unix_file = symname;
	}
	if(symlink(unix_file, fn)) {
		fprintf(stderr, "Can't make symbolic link to %s: %s\n", fn,
			strerror(errno));
		goto cfail;
	}
	new_tmpfile(fn);
	if(fstat(ufd, &sbuf) == -1) {
		fprintf(stderr, "Can't get size of file %s: %s\n", fn,
			strerror(errno));
cfail:		close(ufd);
		fail();
		return -1;
	}
	close(ufd);
	if(!sbuf.st_size)
		sbuf.st_size = 1;	/* special case, because a length of 0 is
			a marker for an empty slot, and we can mess up the
			volhdr */
	return roundblk(sbuf.st_size);
}


void
putdvh(void)
{
		 int	error;
		 char	buf[PATH_MAX];
	register struct volume_directory *vd;


	/*
	 * validate consistency of volume header
	 *	partition table should have entry for volume header
	 */
	if(getvhpart() == NULL)
		return;
	
	/* if needcopy is set, then all the files get copied to the
		tmpdir, and we copy them all back into the vh; this happens
		when the 'free' space in the vh got fragmented. */
	if(needcopy) {
		int lbn = firstfileblk;
		char fn[PATH_MAX];

		copytotmp();

		for(vd = vh.vh_vd; vd < &vh.vh_vd[NVDIR]; vd++) {
			if(!vd->vd_nbytes)
				continue;
			sprintf(fn, "%s/%.*s.%d", tmpdir, VDNAMESIZE, vd->vd_name, mypid);
			vd->vd_lbn = lbn;
			copy_tovh(fn, lbn, vd->vd_nbytes);
			lbn += roundblk(vd->vd_nbytes)/dev_bsize;
		}
		needcopy = 0;
	}


	/*
	 * calculate vh_csum
	 */
	vh.vh_magic = VHMAGIC;
	vh.vh_csum = 0;
	vh.vh_csum = vh_checksum(&vh);

	/*
	 * Try to update the drivers first.
	 */
	if (ioctl(vfd, DIOCSETVH, &vh) < 0) {

		error = oserror();		/* Keep a copy for later */

		if (error == EBUSY) {
			/*
			 * An EBUSY can only happen on non-miniroot kernels.
			 * We're either interactive, command, or script driven.
			 * If we're not miniroot, and are command or script
			 * driven, we should terminate.
			 */

			perror("Driver DIOCSETVH update ioctl failed!");

			printf(
		"\n\tBy re-partitioning you are attempting to delete a\n"
		"\tpartition in use by a mounted file system!\n\n");

			printf(
	"Continue only if you are very certain about this action.\n"
	"A kernel panic could occur if you proceed, and then attempt\n"
	"to unmount the affected file system.\n"
	"Continue by forcing the driver update?" );

			if (gets(buf) == NULL) {

				setoserror(error);	/* Put it back	*/

				fail();
				return;		/* In case interactive	*/
			}

			if (buf[0] == 'y') {

				(void)ioctl(vfd, DIOCSETVH | DIOCFORCE, &vh);

			} else {

				setoserror(error);	/* Put it back	*/

				fail();
				return;
			}

		} else {

			perror("Driver update failed!\n");

			fail();
			return;		/* In case interactive	*/
		}
	}

	if (lseek(vfd, 0, 0) < 0) {
		perror("seek to sector 0 failed");
		fail();
		return;	/* in case interactive */
	}

	if (write(vfd, &vh, dev_bsize) != dev_bsize) {
		perror("write of volume header to sector 0 failed");
		fail();
		return;	/* in case interactive */
	}

	rmtmpfiles();	/* done with this batch (if any) */
	vhdirty = 0;
}


vh_checksum(struct volume_header *vhp)
{
	register int csum;
	register int *ip;

	csum = 0;
	for (ip = (int *)vhp; ip < (int *)(vhp+1); ip++)
		csum += *ip;
	return(-csum);
}



min(int a, int b)
{
	return(a<b?a:b);
}

/*
 * atob -- convert ascii to binary.  Accepts all C numeric formats.
 */
char *
atob(char *cp, int *iptr)
{
	register unsigned d;
	register int value = 0;
	register unsigned base = 10;
	register int minus = 0;

	*iptr = 0;
	if (!cp)
		return(0);

	while (isspace(*cp))
		cp++;

	while (*cp == '-') {
		cp++;
		minus = !minus;
	}

	/*
	 * Determine base by looking at first 2 characters
	 */
	if (*cp == '0') {
		switch (*++cp) {
		case 'X':
		case 'x':
			base = 16;
			cp++;
			break;

		case 'B':	/* a frill: allow binary base */
		case 'b':
			base = 2;
			cp++;
			break;
		
		default:
			base = 8;
			break;
		}
	}

	while ((d = digit(*cp)) < base) {
		value *= base;
		value += d;
		cp++;
	}

	if (minus)
		value = -value;

	*iptr = value;
	return(cp);
}

/*
 * digit -- convert the ascii representation of a digit to its
 * binary representation
 */
unsigned
digit(char c)
{
	register unsigned d;

	if (isdigit(c))
		d = c - '0';
	else if (isalpha(c)) {
		if (isupper(c))
			c = tolower(c);
		d = c - 'a' + 10;
	} else
		d = 999999; /* larger than any base to break callers loop */

	return(d);
}


void
show_bootinfo(void)
{
	printf ("Boot file: %s  root partition: %d  swap partition: %d\n", 
		vh.vh_bootfile[0] == 0 ? "NULL" : vh.vh_bootfile, vh.vh_rootpt,
			vh.vh_swappt);
}


void
change_bootinfo (char *p)
{
	if (p == 0) {
		int newpt;
		char stk_buf[80];

		printf ("Current Boot file name is %s, root partition %d, swap partition %d\n", 
			vh.vh_bootfile[0] == 0 ? "NULL" : vh.vh_bootfile,
			rootpart== -1?vh.vh_rootpt:rootpart,
			swappart== -1?vh.vh_swappt:swappart);
		printf ("Enter name up to %d characters: ", BFNAMESIZE);
		if(gets (stk_buf) && *stk_buf != 0)
			strncpy (vh.vh_bootfile, stk_buf, BFNAMESIZE);
		printf("Enter new root partition: ");
		if(gets (stk_buf) && *stk_buf != 0) {
			if(*atob(stk_buf, &newpt))
				printf("%s isn't a number\n", stk_buf);
			else if(newpt >= NPARTAB)
				printf("partition # %d not valid, must be less than %d\n", 
					newpt, NPARTAB);
			else
				rootpart = newpt;
		}
		printf("Enter new swap partition: ");
		if(gets (stk_buf) && *stk_buf != 0) {
			if(*atob(stk_buf, &newpt))
				printf("%s isn't a number\n", stk_buf);
			else if(newpt >= NPARTAB)
				printf("partition # %d not valid, must be less than %d\n", 
					newpt, NPARTAB);
			else
				swappart = newpt;
		}
	}
	else
		strncpy (vh.vh_bootfile, p, BFNAMESIZE);

	if(rootpart != -1)
		vh.vh_rootpt = rootpart;
	if(swappart != -1)
		vh.vh_swappt = swappart;
	vhdirty = 1;
}


/*	return space avail in volume header; for testing on creats and adds
	to be sure enough room.  Caller should probably make sure that
	any deletes have been done before the creats/adds for this to
	work best.
*/
int
vh_availspace(void)
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


/* open the volume header if different name, or not yet open */
openvh(char *volhdr)
{

	if(strcmp(save_volhdr, volhdr) == 0 && vfd != -1)
		return vfd;	/* same, and open */

	if(vfd != -1)
		close(vfd);	/* different volume */

	if((vfd = open(volhdr, readonly ? O_RDONLY : O_RDWR)) < 0) {
		eprintf("dvhtool: can't open volume header %s\n", volhdr);
		return -1;
	}
	return vfd;
}

/*	copy files from volume header to tmp directory; was always done in
	getdvh(), but often that isn't needed.  Olson, 4/89
	vh must already be validated.
*/
void
copytotmp(void)
{
	register struct volume_directory *vd;
	char fn[PATH_MAX];

	for (vd = vh.vh_vd; vd < &vh.vh_vd[NVDIR]; vd++) {
		if(vd->vd_lbn != -1 && vd->vd_nbytes) {
			sprintf(fn, "%s/%.*s.%d", tmpdir, VDNAMESIZE, vd->vd_name, mypid);
			copy_file(vd, fn, 1);
		}
	}
}


/* copy a file from the unix file system to the volume header */
void
copy_tovh(char *fn, daddr_t lbn, int flen)
	/* lbn: block # in vh of start of file */
	/* flen: expected length of file (amount allocated in header) */
{
	int tfd, bytes, i, res;
	char buf[8192];

	if (lseek(vfd, lbn * dev_bsize, 0) < 0) {
		eprintf("seek to block %d failed\n", lbn);
		return;
	}
	if ((tfd = open(fn, O_RDONLY)) < 0) {
		eprintf("couldn't open %s\n", fn);
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
				fail();
			}
			else
				eprintf("%s\n", buf);
			goto done;
		}
		else
			bytes += i;
	}
	if(i == -1)
		eprintf("read error on %s\n", fn);
	/* bytes == 0 && flen <= 512 is OK, it's the hack for 0 len files */
	else if (roundblk(bytes) != roundblk(flen) && (bytes || roundblk(flen)>512))
		eprintf("%s changed length\n", fn);
done:
	close(tfd);
}


/*	figure out which partition is the volume header */

struct partition_table *
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
lbncmp(const void *cv0, const void *cv1)
{
	struct volume_directory *v0, *v1;
	v0 = (struct volume_directory *)cv0;
	v1 = (struct volume_directory *)cv1;
	if(v0->vd_lbn <= 0)
		return 1;
	else if(v1->vd_lbn <= 0)
		return -1;
	return v0->vd_lbn - v1->vd_lbn;
}

void
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
vh_contig(int bytes)
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
roundblk(int cnt)
{
	cnt = (cnt + dev_bsize -1) / dev_bsize;
	cnt *= dev_bsize;
	return cnt;
}

/* print message, then if ! interactive, then call cleanup,
	otherwise just return */
void
eprintf(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fail();
}

void
fail(void)
{
	if(!interactive)
		cleanup(1);
}
