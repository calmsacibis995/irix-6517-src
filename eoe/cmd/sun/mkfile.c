#ifndef lint
static  char sccsid[] = "@(#)mkfile.c	1.1 88/06/09 4.0NFSSRC; from 1.3 88/02/08 SMI";
#endif

/*
 * Copyright (c) 1986 by Sun Microsystems, Inc.
 */

#include <sys/param.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <bstring.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#define	MAXBUFFERSIZE	(256 * 1024)

static void usage(void);

main(int argc, char **argv)
{
	int fd;
	off64_t size = 0;
	int mult = 0;
	int bytes = 0;
	off64_t wrote = 0;
	int len = 0;
	int c;
	int errflg = 0;
	int errs = 0;
	int nobytes = 0;
	int verbose = 0;
	struct dioattr da;
	void *buf = NULL;
	int buflen = 0, nbuflen;
	int bufalign = 0, nbufalign, bufmin;
	int oflags;
	flock64_t flck;

	while ((c = getopt(argc, argv, "nv")) != EOF) {
		switch(c) {
			case 'n':
				nobytes++;
				break;
			case 'v':
				verbose++;
				break;
			default:
				errflg++;
				break;
		}
	}
	if (argc < optind + 2 || errflg)
		usage();
	mult = 1;
	len = strlen(argv[optind]);
	if (isalpha(argv[optind][len-1])) {
		switch (argv[optind][len-1]) {
		case 'k':
		case 'K':
			mult = 1024;
			break;
		case 'b':
		case 'B':
			mult = 512;
			break;
		case 'm':
		case 'M':
			mult = 1024*1024;
			break;
		case 'g':
		case 'G':
			mult = 1024*1024*1024;
			break;
		default:
			fprintf(stderr, "unknown size %s\n", argv[optind]);
			usage();
		}
		argv[optind][len-1] = '\0';
	}
	size = atoll(argv[optind]) * mult;
	optind++;
	while (optind < argc) {
		if (verbose)
			fprintf(stdout, "%s %lld bytes\n", argv[optind], size);
		oflags = O_CREAT|O_TRUNC|O_WRONLY|(nobytes ? 0 : O_DIRECT);
		fd = open(argv[optind], oflags, 0600);
		if ((oflags & O_DIRECT) &&
		    ((fd < 0 && errno == EINVAL) ||
		     fcntl(fd, F_DIOINFO, &da) < 0)) {
			close(fd);
			oflags &= ~O_DIRECT;
			fd = open(argv[optind], oflags, 0600);
		}
		if (fd < 0) {
			perror(argv[optind]);
			optind++;
			errs++;
			continue;
		}
		if (size == 0) {
			close(fd);
			optind++;
			continue;
		}
		if (lseek64(fd, size - 1, SEEK_SET) < 0) {
			/*
			 * This check doesn't actually work for 6.2
			 * efs and nfs2, although it should.
			 */
			perror(argv[optind]);
			errs++;
		} else if (nobytes) {
			if (write(fd, "", 1) < 0) {
				perror(argv[optind]);
				errs++;
			}
		} else {
			flck.l_whence = SEEK_SET;
			flck.l_start = 0;
			flck.l_len = size;
			(void)fcntl(fd, F_RESVSP64, &flck);
			if (oflags & O_DIRECT) {
				nbufalign = da.d_mem;
				if (da.d_miniosz <= MAXBUFFERSIZE &&
				    MAXBUFFERSIZE <= da.d_maxiosz)
					nbuflen = MAXBUFFERSIZE;
				else if (da.d_maxiosz < MAXBUFFERSIZE)
					nbuflen = da.d_maxiosz;
				else
					nbuflen = da.d_miniosz;
				bufmin = da.d_miniosz;
			} else {
				nbuflen = MAXBUFFERSIZE;
				nbufalign = sizeof(long);
				bufmin = 0;
			}
			if (nbuflen > buflen || nbufalign > bufalign) {
				if (buf)
					free(buf);
				buf = memalign(nbufalign, nbuflen);
				buflen = nbuflen;
				bzero(buf, nbuflen);
				nbufalign = bufalign;
			}
			wrote = 0;
			lseek64(fd, 0, SEEK_SET);
			while (wrote < size) {
				if (size - wrote >= buflen)
					bytes = buflen;
				else if (bufmin)
					bytes = roundup(size - wrote, bufmin);
				else
					bytes = size - wrote;
				len = write(fd, buf, bytes);
				if (len < 0) {
					perror(argv[optind]);
					unlink(argv[optind]);
					errs++;
					break;
				}
				wrote += len;
			}
			if (wrote > size && ftruncate64(fd, size) < 0) {
				perror(argv[optind]);
				unlink(argv[optind]);
				errs++;
			}
		}
		if ( close(fd) < 0 ) {
			perror(argv[optind]);
			unlink(argv[optind]);
			errs++;
		}
		optind++;
	}
	return errs != 0;
}

static void
usage(void)
{
	fprintf(stderr, "mkfile: [-nv] <size> <name1> [<name2>] ...\n");
	exit(2);
}
