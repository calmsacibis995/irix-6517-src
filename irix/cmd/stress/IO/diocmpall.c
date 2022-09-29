
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <sys/schedctl.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <ulocks.h>

#include <unistd.h>
#include <stdlib.h>

/*
** this program is to see how bad the failures due to the r10000
** speculative read issue are relative to direct i/o
**
** we use two files becuase dio is kept in sync with normal buffer
** cache reads & writes, so we don't want any interference there.
*/

int
usage_bye(char * prog)
{
	fprintf(stderr,"usage: %s [-h] [-p|-m] [-s nthreads] [-n times]" 
				"[-t method] file1 file2\n",prog);
	fprintf(stderr,"\t-s threads\tsproc threads hurt\n");
	fprintf(stderr,"\t-n count\tloop through file count iterations\n");
	fprintf(stderr,"\t-p\t\tpage align, page size prebuffer\n");
	fprintf(stderr,"\t-m\t\tmiddle of page, diobuff page aligned\n");
	fprintf(stderr,"\t-t method\t[ss|bzero] specstore.s or bzero\n");
	fprintf(stderr,"\t-f\t\trun at fixed (non-degrading) priorities\n");
	fprintf(stderr,"\t-d\t\tprint debugging information (add more "
			"-d options for more info\n");
	fprintf(stderr,"\t-X\t\tdon't request O_DIRECT on 2nd fd\n");
	fprintf(stderr,"\t-D\t\tdon't exit on failure, just count 'em\n");
	fprintf(stderr,"\t-V\t\ttoss in some random VM sproc/forks\n");
	fprintf(stderr,"\t-v\t\tverbose, show file diffs\n");
	exit(0);
	/*NOTREACHED*/
}

volatile char * nbuffer; 	/* normal i/o buffer */
volatile char * pre_dio; 	/* page/area just before diobuffer */
int prebuf_size;
volatile char * diobuffer;	/* direct i/o buffer */
int chunksize = 0;		/* chunksize of data to compare */
void sproc_bzero(void * arg);
#ifdef NOT
void sproc_spec(void * arg);
#endif	/* NOT */
void (* method) (void *) = sproc_bzero;

int do_vm_ops = 0;
int do_vm_cnt = 0;

/*
** in an attempt to push out more cache lines, create a couple more
** places to bzero() also (push more addresses through the cache)
*/
volatile char * a1[16];
volatile char * a2;
volatile char * a3;
volatile char * a4;
volatile char * a5;
volatile char * a6;
volatile char * a7;
volatile char * a8;
volatile char * a9;
volatile char * a10;

int
main(int argc, char * argv[])
{
	char * srcfile_n = NULL;
	char * srcfile_dio = NULL;
	int ntimes = 100;

	int nfd;			/* normal file descriptor */
	int diofd;			/* direct i/o file descriptor */
	__int32_t * ncmp;		/* normal compare ptr */
	__int32_t * diocmp;		/* direct compare ptr */
	long filesize;			/* size of the files */
	__int32_t prev_first = 0xdeadbeef;	/* previous first chunk */

	struct stat statbuf;		/* stat info */
	struct dioattr dioinfo;		/* direct i/o info */

	int ndone;			/* times through loop */
	int i;				/* counter */
	long rc;			/* return code */
	int filepos;			/* position in file */
	int do_sproc = 1;		/* start speculative hurt thread */
	int child = -1;			/* child process number (sproc) */
	int debug = 0;			/* debug printf messages */
	int dio_option = O_DIRECT;	/* default to with dio */

	long pagesize;			/* size of a page in this system */
	int page_align = 1;		/* start,align on page boundary */
	int non_degrade = 0;		/* run at non-degrading priority */

	int noexit = 0;			/* exit on error */
	int verbose = 0;		/* show differences */
	int nfailures = 0;
	int sprocs = 0;

	int c;
	extern char * optarg;
	extern int optind;

	a2 = (volatile char *) malloc( 16 );

	/*
	** parse command line arguments
	*/
	while ((c = getopt(argc, argv, "dhn:s:mpft:XDVv")) != EOF) {
		switch (c) {
			case 'X':
				dio_option = 0;
				break;
			case 'd':
				debug++;
				break;
			case 'n':
				ntimes = atoi( optarg );
				if (ntimes < 0) {
					usage_bye(argv[0]);
					/* NOTREACHED */
				}
				break;
			case 's':
				do_sproc = atoi( optarg );
				if (do_sproc < 0) {
					usage_bye(argv[0]);
				}
				if (do_sproc > 7) {
					usconfig(CONF_INITUSERS, do_sproc+2);
				}
				break;
			case 'p':
				page_align = 1;
				break;
			case 'm':
				page_align = 0;
				break;
			case 'f':
				non_degrade = 1;
				break;
			case 't':
				if (!strncmp(optarg, "ss", 2)) {
#ifdef	NOT
					method = sproc_spec;
#else
					fprintf(stderr,"method ss isn't "
						"compiled in\n");
					usage_bye(argv[0]);
#endif	/* NOT */
				}
				else if (!strncmp(optarg, "bz", 2)) {
					method = sproc_bzero;
				}
				else {
					fprintf(stderr,"unrecognized type "
						" of test %s\n",optarg );
					fprintf(stderr,"usr 'ss' or 'bzero'\n");
					usage_bye(argv[0]);
				}
				break;
			case 'D':
				noexit = 1;
				break;
			case 'V':
				do_vm_ops = 1;
				break;
			case 'v':
				verbose = 1;
				break;

			case 'h':
			default:
				usage_bye(argv[0]);
				/* NOTREACHED */
				break;
		}
	}

	if ((optind+1) < argc) {
		srcfile_n = argv[optind];
		srcfile_dio = argv[optind+1];
	} else {
		usage_bye(argv[0]);
		/* NOTREACHED */
	}

	pagesize = sysconf( _SC_PAGESIZE );

	a3 = (volatile char *) memalign( pagesize, 16 );

	/*
	** open the files and make sure they're at least the same size
	** if they're not, well, they're not gonna compare the same are
	** they?
	*/
	nfd = open( srcfile_n, O_RDONLY );
	if (debug) {
		fprintf(stderr,"opening file %s with%s O_DIRECT\n",
				srcfile_dio,((dio_option == 0)?"out":"") );
	}
	diofd = open( srcfile_dio, O_RDONLY | dio_option );

	if ((nfd < 0) || (diofd < 0)) {
		fprintf(stderr,"failed to open one of %s or %s\n",
			srcfile_n, srcfile_dio);
		exit(1);
	}

	if (fstat( nfd, &statbuf ) < 0) {
		fprintf(stderr,"failed to stat %s\n",srcfile_n);
		exit(1);
	}
	filesize = statbuf.st_size;

	a4 = (volatile char *) memalign( pagesize, 16 );

	if (fstat( diofd, &statbuf ) < 0) {
		fprintf(stderr,"failed to stat %s\n",srcfile_dio);
		exit(1);
	}

	if (filesize != statbuf.st_size) {
		fprintf(stderr,"%s and %s are not the same size.  they"
			" certainly won't compare the same.\n",srcfile_n,
			srcfile_dio);
		exit(1);
	}

	/*
	** now get the direct i/o information, and adjust the size of
	** the files we're going to check relative to the io size
	*/
	if (dio_option) {
		if (fcntl(diofd, F_DIOINFO, &dioinfo) < 0) {
			fprintf(stderr,"fcntl F_DIOINFO failed\n");
			exit(1);
		}
	} else {
		/* xfs values */
		dioinfo.d_miniosz = 4096;
		dioinfo.d_mem = 512;
		dioinfo.d_maxiosz = 40960;
	}

	chunksize = dioinfo.d_miniosz * 10;	/* ~40k? */

	filesize = (filesize / chunksize) * chunksize;

	a5 = (volatile char *) memalign( pagesize, 16 );

	if (filesize <= 0) {
		fprintf(stderr,"the files aren't big enough.  use large "
			"files for this test\n");
		exit(1);
	}

	/*
	** allocate the data buffers
	*/
	nbuffer = (char *)memalign( pagesize, chunksize );
	if (nbuffer == NULL) {
		fprintf(stderr,"nbuffer: failed to allocate %d bytes\n");
		exit(1);
	}
	if (page_align) {
		pre_dio = (char *)memalign( pagesize, chunksize + pagesize );
		diobuffer = pre_dio + pagesize;
		prebuf_size = pagesize;

		if (debug) {
			fprintf(stderr,"page aligned, pre_dio 0x%x, "
				"diobuffer 0x%x, psize %d\n",
				pre_dio, diobuffer, prebuf_size);
		}
	}
	else {
		pre_dio = (char *)memalign(pagesize, chunksize + pagesize );
		diobuffer = pre_dio + (pagesize / 2);
		prebuf_size = pagesize/2;

		if (debug) {
			fprintf(stderr,"non-aligned, pre_dio 0x%x, "
				"diobuffer 0x%x, psize %d\n",
				pre_dio, diobuffer, prebuf_size);
		}
	}
	if (diobuffer == NULL) {
		fprintf(stderr,"diobuffer: failed to allocate %d bytes\n",
			chunksize+pagesize);
		exit(1);
	}

	a6 = (volatile char *) memalign( pagesize, 16 );
	a7 = (volatile char *) memalign( pagesize, 16 );
	a8 = (volatile char *) memalign( pagesize, 16 );
	a9 = (volatile char *) memalign( pagesize, 16 );
	a10 = (volatile char *) memalign( pagesize, 16 );

	if (non_degrade) {
		if (schedctl(NDPRI, getpid(), NDPHIMIN) == -1) {
			fprintf(stderr,"schedctl on parent %d "
				" failed, errno %d\n",getpid(),errno);
		}
	}

	/*
	** this would be the place to start the sproc
	*/
	for (i=0; i<do_sproc; i++) {
		child = sproc( method, PR_SADDR, 1 );
		if (debug > 1) fprintf(stderr,"sproc'd child %d\n",child);
		if (child < 0) {
			fprintf(stderr,"sproc %d failed, rc %d, errno %d\n",
							i,child,errno);
			exit(1);
		}
		else if (non_degrade) {
			if (schedctl(NDPRI, child, NDPHIMIN-2) == -1) {
				fprintf(stderr,"schedctl on child %d "
					" failed, errno %d\n",child,errno);
			}
		}
	}

	/*
	** and loop through the data, checking it
	*/
	for (ndone = 0; (ntimes < 1) || (ndone < ntimes); ndone++) {
		lseek(nfd, SEEK_SET, 0);
		lseek(diofd, SEEK_SET, 0);

		for (filepos = 0; filepos < filesize; filepos += chunksize) {
			if (debug > 2) {
				fprintf(stderr,"prior to normal read\n");
			}
			rc = read(nfd, (void*)nbuffer, chunksize);
			if (debug > 2) {
				fprintf(stderr,"post normal read, rc %d\n",rc);
			}
			if (rc != chunksize) {
				fprintf(stderr,"normal read failed, rc %d "
					"errno %d, loop %d, offset %d\n",
					rc,errno,ndone+1,filepos);
				exit(1);
			}
			if (debug > 2) {
				fprintf(stderr,"prior to direct i/o read\n");
			}
			rc = read(diofd, (void*)diobuffer, chunksize);
			if (debug > 2) {
				fprintf(stderr,"post dio read, rc %d\n",rc);
			}
			if (rc != chunksize) {
				fprintf(stderr,"dio read failed, rc %d "
					"errno %d, loop %d, offset %d\n",
					rc, errno, ndone+1,filepos );
				exit(1);
			}

			ncmp = (__int32_t *) nbuffer;
			diocmp = (__int32_t *) diobuffer;
			for (i=0; i<chunksize; i+=sizeof(__int32_t)) {
				if (*ncmp != *diocmp) {
				    nfailures++;
				    if (!noexit) {
					break;
				    }
				    if (verbose)
					fprintf(stderr,
						"\nbyte %d (word 0x%x), "
						"normal 0x%x "
						"dio 0x%x prev_chunk[0] 0x%x", 
						filepos+i,
						(filepos+i)/sizeof(__int32_t),
						*ncmp,*diocmp,
						prev_first);
				}
				else if (i == 0) {
					prev_first = *diocmp;
				}
				ncmp++; diocmp++;
			}
		}
		if (nfailures)
		    fprintf(stderr,"\nERROR: files differ, loop %d chunksize %d (0x%x)",
			ndone+1, chunksize, chunksize);
		if (ntimes > 1) {
		    if (nfailures == 0) {
			fprintf(stderr,"\rdiocmpall: loop %d of %d completed successfully",
				ndone+1,ntimes);
		    }
		    else {
			fprintf(stderr,"\ndiocmpall: loop %d of %d completed, %d "
				"mismatches\n",ndone+1,ntimes,nfailures );
			exit(1);
		    }
		}
		else {
		    if (nfailures == 0) {
			fprintf(stderr,"\rcompleted loop %d successfully",
				ndone+1);
		    }
		    else {
			fprintf(stderr,"\nloop %d completed, %d "
				"mismatches\n",ndone+1,nfailures );
			exit(1);
		    }
		}
	}

	printf("\nprogram completed successfully, no problems found\n");

	exit(0);
}


#ifdef 	NOT
void
sproc_spec(void * arg)
{
	int i;
	extern void specstore( volatile char * );

	sigset( SIGHUP, SIG_DFL );
	prctl( PR_TERMCHILD );

	while (1) {
		for (i=0; i<1000; i++) {
			specstore( diobuffer );
		}
		sginap(1);
	}
}
#endif

/*ARGSUSED*/
void
dummy_sproc(void * arg)
{
	return;
}

void
sproc_bzero(void * arg)
{
	pid_t wpid = 0;
	int status;
	int i;

	sigset( SIGHUP, SIG_DFL );
	prctl( PR_TERMCHILD );

	while (1) {
		/* Toss in some VM operations to test locking/copying of
		 * locked down areas every 100 loops.
		 */
		if (do_vm_ops) {
			if (wpid) {
				waitpid(wpid,&status,0);
				wpid = 0;
			}

			do_vm_cnt = (do_vm_cnt+1) % 100;
			switch (do_vm_cnt) {
			case 0:
				wpid = sproc(dummy_sproc,PR_SADDR,0);
				break;
			case 1:
				wpid = sproc(dummy_sproc,0,0);
				break;
			case 2:
				if ((wpid = fork()) == 0) {
					exit(0);
				}
				break;
			case 3:
				if ((wpid = fork()) == 0) {
					bzero(pre_dio, prebuf_size);
					exit(0);
				}
				break;
			}
		}
		for (i=0; i<1000; i++) {
			bzero(pre_dio, prebuf_size);
			bzero(a1, 16);
			bzero(a2, 16);
			bzero(a3, 16);
			bzero(a4, 16);
			bzero(a5, 16);
			bzero(a6, 16);
			bzero(a7, 16);
			bzero(a8, 16);
			bzero(a9, 16);
			bzero(a10, 16);
		}
		sginap(1);
	}
}
