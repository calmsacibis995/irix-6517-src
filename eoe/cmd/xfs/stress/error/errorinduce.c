#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syssgi.h>
#include <sys/stat.h>
#include <sys/errno.h>
/*
 * Simple program to induce errors at a specified frequency.
 * This uses the syssgi calls SGI_{DKSC|XLV}_{UN}INDUCE_IO_ERROR
 * which are only built in with -DDEBUG or -DINDUCE_IO_ERROR flags.
 *
 * This meant to be used for XFS error handling testing.
 * errorinduce -sv /dev/rxlv/test = INDUCE at XLV level
 * errorinduce -uv /dev/rxlv/test = stop induction of errors at XLV level
 */
char *progname;
int verbose = 0;
int dflag = 0, xflag = 0, kflag = 0, sflag = 0, uflag = 0, nflag = 0;
int freq = 2;
dev_t errdev = 0;

static int	doerror(void);

static void
usage(void)
{
	fprintf(stderr,
		"usage: %s "
		"\t[-v] [-x{xlv}|k{dksc}] \n"
		"\t[-s{set}|u{unset}] [-f freq] [-d dev_t] [rawdev] \n",
		progname);
	exit(1);
}

static dev_t
rawpathtodevt(char *rpath)
{
	struct stat st;

	if (stat(rpath, &st) < 0)
		return (dev_t) 0;
	return (st.st_rdev);
	       
}

static int
doerror(void)
{
	int cmd, error;

	if (nflag)
		return 0;

	if (sflag) {
		cmd = (kflag) ?
			SGI_DKSC_INDUCE_IO_ERROR : SGI_XLV_INDUCE_IO_ERROR;
	} else {
		cmd = (kflag) ?
			SGI_DKSC_UNINDUCE_IO_ERROR : SGI_XLV_UNINDUCE_IO_ERROR;
	}

	error = syssgi(cmd, errdev, freq);
	if (verbose)
		if (error)
			perror("SGI_{DKSC|XLV}_{UN}INDUCE_IO_ERROR");
	return (error);
}

int
main(int argc, char **argv)
{
	int ch, error;

	progname = argv[0];

	while ((ch = getopt(argc, argv, "d:vxnksuf:")) != EOF) {
		switch(ch) {
		      case 'v':
			verbose++;
			break;
			
		      case 'd':
			dflag++;
			errdev = atoi(optarg);
			if (errdev <= 0)
				usage();
			break;

		      case 'x':
			xflag++;
			break;
			
		      case 'k':
			kflag++;
			break;

		      case 's':
			sflag++;
			break;

		      case 'u':
			uflag++;
			break;
		      
		      case 'f':
			freq = atoi(optarg);
			break;
			
		      case 'n':
			nflag++;
			break;

		      default:
			usage();
		}
	}
	if (!errdev && !dflag &&
	    optind == argc-1) {
		errdev = rawpathtodevt(argv[optind]);
	}
	if (errdev <= 0)
		usage();

	if ((xflag && kflag) ||
	    (sflag && uflag))
		usage();

	if (!sflag && !uflag)
		usage();

	if (verbose) {
		fprintf(stderr,
			"device = 0x%x\n",
			errdev);
	}
	error = doerror();
	if (verbose) {
		if (sflag) {
			fprintf(stderr, 
				"I/O error induced\n");
		} else {
			fprintf(stderr, 
				"I/O error UN induced\n");
		}
	}
		       
	return (error);
}
