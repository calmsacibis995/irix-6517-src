/* ============================================================================
||  aiocat.c : This highly artificial example demonstrates asynchronous I/O. 
||
|| The command syntax is:
||	aiocat [ -o outfile ] [-a {0|1|2|3} ] infilename...
||
|| The output file is given by -o, with $TMPDIR/aiocat.out by default.
|| The aio method of waiting for completion is given by -a as follows:
||	-a 0 poll for completion with aio_error() (default)
||	-a 1 wait for completion with aio_suspend()
||	-a 2 wait on a semaphore posted from a signal handler
||	-a 3 wait on a semaphore posted from a callback routine
||
|| Up to MAX_INFILES input files may be specified. Each input file is
|| read in BLOCKSIZE units. The output file contains the data from
|| the input files in the order they were specified. Thus the
|| output should be the same as "cat infilename... >outfile".
||
|| When DO_SPROCS is compiled true, all I/O is done asynchronously
|| and concurrently using one sproc'd process per file.  Thus in a
|| multiprocessor concurrent input can be done.
============================================================================ */

#define _SGI_MP_SOURCE	/* see the "Caveats" section of sproc(2) */
#include <sys/time.h>	/* for clock() */
#include <errno.h>		/* for perror() */
#include <stdio.h>		/* for printf() */
#include <stdlib.h>		/* for getenv(), malloc(3c) */
#include <ulocks.h>		/* usinit() & friends */
#include <bstring.h>	/* for bzero() */
#include <sys/resource.h> /* for prctl, get/setrlimit() */
#include <sys/prctl.h>	/* for prctl() */
#include <sys/types.h>	/* required by lseek(), prctl */
#include <unistd.h>		/* ditto */
#include <sys/types.h>	/* wanted by sproc() */
#include <sys/prctl.h>	/* ditto */
#include <signal.h>		/* for signals - gets sys/signal and sys/siginfo */
#include <aio.h>		/* async I/O */

#define BLOCKSIZE 2048	/* input units -- play with this number */
#define MAX_INFILES 10	/* max sprocs: anything from 4 to 20 or so */
#define DO_SPROCS 1		/* set 0 to do all I/O in a single process */

#define QUITIFNULL(PTR,MSG) if (NULL==PTR) {perror(MSG);return(errno);}
#define QUITIFMONE(INT,MSG) if (-1==INT) {perror(MSG);return(errno);}
/*****************************************************************************
|| The following structure contains the info needed by one child proc.
|| The main program builds an array of MAX_INFILES of these.
|| The reason for storing the actual filename here (not a pointer) is
|| to force the struct to >128 bytes.  Then, when the procs run in 
|| different CPUs on a CHALLENGE, the info structs will be in different
|| cache lines, and a store by one proc will not invalidate a cache line
|| for its neighbor proc.
*/
typedef struct child
{
		/* read-only to child */
	char fname[100];		/* input filename from argv[n] */
	int			fd;			/* FD for this file */
	void*		buffer;		/* buffer for this file */
	int			procid;		/* process ID of child process */
	off_t		fsize;		/* size of this input file */
		/* read-write to child */
	usema_t*	sema;		/* semaphore used by methods 2 & 3 */
	off_t		outbase;	/* starting offset in output file */
	off_t		inbase;		/* current offset in input file */
	clock_t		etime;		/* sum of utime/stime to read file */
	aiocb_t		acb;		/* aiocb used for reading and writing */
} child_t;

/******************************************************************************
|| Globals, accessible to all processes
*/
char*		ofName = NULL;	/* output file name string */
int			outFD;			/* output file descriptor */
usptr_t*	arena;			/* arena where everything is built */
barrier_t*	convene;		/* barrier used to sync up */
int			nprocs = 1;		/* 1 + number of child procs */
child_t*	array;			/* array of child_t structs in arena */
int			errors = 0;		/* always incremented on an error */

/******************************************************************************
|| forward declaration of the child process functions
*/
void inProc0(void *arg, size_t stk);	/* polls with aio_error() */
void inProc1(void *arg, size_t stk);	/* uses aio_suspend() */
void inProc2(void *arg, size_t stk);	/* uses a signal and semaphore */
void inProc3(void *arg, size_t stk);	/* uses a callback and semaphore */

/******************************************************************************
// The main()
*/
int main(int argc, char **argv)
{
	char*		tmpdir;			/* ->name string of temp dir */
	int			nfiles;			/* how many input files on cmd line */
	int			argno;			/* loop counter */
	child_t*	pc;				/* ->child_t of current file */
	void (*method)(void *,size_t) = inProc0; /* ->chosen input method */
	char		arenaPath[128];	/* build area for arena pathname */
	char		outPath[128];	/* build area for output pathname */	
	/*
	|| Ensure the name of a temporary directory.
	*/
	tmpdir = getenv("TMPDIR");
	if (!tmpdir) tmpdir = "/var/tmp";
	/*
	|| Build a name for the arena file.
	*/
	strcpy(arenaPath,tmpdir);
	strcat(arenaPath,"/aiocat.wrk");
	/*
	|| Create the arena. First, call usconfig() to establish the
	|| minimum size (twice the buffer size per file, to allow for misc usage)
	|| and the (maximum) number of processes that may later use
	|| this arena.  For this program that is MAX_INFILES+10, allowing
	|| for our sprocs plus those done by aio_sgi_init().
	|| These values apply to any arenas made subsequently, until changed.
	*/
	{
		ptrdiff_t ret;
		ret = usconfig(CONF_INITSIZE,2*BLOCKSIZE*MAX_INFILES);
		QUITIFMONE(ret,"usconfig size")
		ret = usconfig(CONF_INITUSERS,MAX_INFILES+10);
		QUITIFMONE(ret,"usconfig users")
		arena = usinit(arenaPath);
		QUITIFNULL(arena,"usinit")
	}
	/*
	|| Allocate the barrier.
	*/
	convene = new_barrier(arena);
	QUITIFNULL(convene,"new_barrier")
	/*
	|| Allocate the array of child info structs and zero it.
	*/
	array = (child_t*)usmalloc(MAX_INFILES*sizeof(child_t),arena);
	QUITIFNULL(array,"usmalloc")
	bzero((void *)array,MAX_INFILES*sizeof(child_t));
	/*
	|| Loop over the arguments, setting up child structs and
	|| counting input files.  Quit if a file won't open or seek,
	|| or if we can't get a buffer or semaphore.
	*/
	for (nfiles=0, argno=1; argno < argc; ++argno )
	{
		if (0 == strcmp(argv[argno],"-o"))
		{ /* is the -o argument */
			++argno;
			if (argno < argc)
				ofName = argv[argno];
			else
			{
				fprintf(stderr,"-o must have a filename after\n");
				return -1;
			}
		}
		else if (0 == strcmp(argv[argno],"-a"))
		{ /* is the -a argument */
			char c = argv[++argno][0];
			switch(c)
			{
			case '0' : method = inProc0; break;
			case '1' : method = inProc1; break;
			case '2' : method = inProc2; break;
			case '3' : method = inProc3; break;
			default:
				{
					fprintf(stderr,"unknown method -a %c\n",c);
					return -1;
				}
			}
		}
		else if ('-' == argv[argno][0])
		{ /* is unknown -option */
			fprintf(stderr,"aiocat [-o outfile] [-a 0|1|2|3] infiles...\n");
			return -1;
		}
		else	
		{ /* neither -o nor -a, assume input file */
			if (nfiles < MAX_INFILES)
			{
				/*
				|| save the filename
				*/
				pc = &array[nfiles];
				strcpy(pc->fname,argv[argno]);
				/*
				|| allocate a buffer and a semaphore.  Not all
				|| child procs use the semaphore but so what?
				*/
				pc->buffer = usmalloc(BLOCKSIZE,arena);
				QUITIFNULL(pc->buffer,"usmalloc(buffer)")
				pc->sema = usnewsema(arena,0);
				QUITIFNULL(pc->sema,"usnewsema")
				/*
				|| open the file
				*/
				pc->fd = open(pc->fname,O_RDONLY);
				QUITIFMONE(pc->fd,"open")
				/*
				|| get the size of the file. This leaves the file
				|| positioned at-end, but there is no need to reposition 
				|| because all aio_read calls have an implied lseek.
				|| NOTE: there is no check for zero-length file; that
				|| is a valid (and interesting) test case.
				*/
				pc->fsize = lseek(pc->fd,0,SEEK_END);
				QUITIFMONE(pc->fsize,"lseek")
				/*
				|| set the starting base address of this input file
				|| in the output file.  The first file starts at 0.
				|| Each one after starts at prior base + prior size.
				*/
				if (nfiles) /* not first */
					pc->outbase =
						array[nfiles-1].fsize + array[nfiles-1].outbase;
				++nfiles;
			}
			else
			{
				printf("Too many files, %s ignored\n",argv[argno]);
			}
		}
	} /* end for(argc) */
	/*
	|| If there was no -o argument, construct an output file name.
	*/
	if (!ofName)
	{
		strcpy(outPath,tmpdir);
		strcat(outPath,"/aiocat.out");
		ofName = outPath;
	}
	/*
	|| Open, creating or truncating, the output file.
	|| Do not use O_APPEND, which would constrain aio to doing
	|| operations in sequence.
	*/
	outFD = open(ofName, O_WRONLY+O_CREAT+O_TRUNC,0666);
	QUITIFMONE(outFD,"open(output)")
	/*
	|| If there were no input files, just quit, leaving empty output
	*/
	if (!nfiles)
	{
		return 0;
	}
	/*
	|| Note the number of processes-to-be, for use in initializing
	|| aio and for use by each child in a barrier() call.
	*/
	nprocs = 1+nfiles;
	/*
	|| Initialize async I/O using aio_sgi_init(), in order to specify
	|| a number of locks at least equal to the number of child procs
	|| and in order to specify extra sproc users.
	*/
	{
		aioinit_t ainit = {0}; /* all fields initially zero */
		/*
		|| Go with the default 5 for the number of aio-created procs,
		|| as we have no way of knowing the number of unique devices.
		*/
#define AIO_PROCS 5
		ainit.aio_threads = AIO_PROCS;
		/*
		|| Set the number of locks aio needs to the number of procs
		|| we will start, minimum 3.
		*/
		ainit.aio_locks = (nprocs > 2)?nprocs:3;
		/*
		|| Warn aio of the number of user procs that will be
		|| using its arena.
		*/
		ainit.aio_numusers = nprocs;
		aio_sgi_init(&ainit);
	}
	/*
	|| Process each input file, either in a child process or in
	|| a subroutine call, as specified by the DO_SPROCS variable.
	*/
	for (argno = 0; argno < nfiles; ++argno)
	{
		pc = &array[argno];
#if DO_SPROCS
#define CHILD_STACK 64*1024
		/*
		|| For each input file, start a child process as an instance
		|| of the selected method (-a argument).
		|| If an error occurs, quit. That will send a SIGHUP to any
		|| already-started child, which will kill it, too.
		*/
		pc->procid = sprocsp(method		/* function to start */
							,PR_SALL	/* share all, keep FDs sync'd */
							,(void *)pc	/* argument to child func */
							,NULL		/* absolute stack seg */
							,CHILD_STACK);	/* max stack seg growth */
		QUITIFMONE(pc->procid,"sproc")
#else
		/*
		|| For each input file, call the selected (-a) method as a
		|| subroutine to copy its file.
		*/
		fprintf(stderr,"file %s...",pc->fname);
		method((void*)pc,0);
		if (errors) break;
		fprintf(stderr,"done\n");
#endif
	}
#if DO_SPROCS
	/*
	|| Wait for all the kiddies to get themselves initialized.
	|| When all have started and reached barrier(), all continue.
	|| If any errors occurred in initialization, quit.
	*/
	barrier(convene,nprocs);
	/*
	|| Child processes are executing now. Reunite the family round the
	|| old hearth one last time, when their processing is complete.
	|| Each child ensures that all its output is complete before it
	|| invokes barrier().
	*/
	barrier(convene,nprocs);
#endif
	/*
	|| Close the output file and print some statistics.
	*/
	close(outFD);
	{
		clock_t	timesum;
		long	bytesum;
		double	bperus;
		printf("    procid   time     fsize     filename\n");
		for(argno = 0, timesum = bytesum = 0 ; argno < nfiles ; ++argno)
		{
			pc = &array[argno];
			timesum += pc->etime;
			bytesum += pc->fsize;
			printf("%2d: %-8d %-8d %-8d  %s\n"
					,argno,pc->procid,pc->etime,pc->fsize,pc->fname);
		}
		bperus = ((double)bytesum)/((double)timesum);
		printf("total time %d usec, total bytes %d, %g bytes/usec\n"
		             ,timesum            , bytesum , bperus);
	}
	/*
	|| Unlink the arena file, so it won't exist when this progam runs
	|| again. If it did exist, it would be used as the initial state of
	|| the arena, which might or might not have any effect.
	*/
	unlink(arenaPath);
	return 0;
}
/******************************************************************************
|| inProc0() alternates polling with aio_error() with sginap(). Under
|| the Frame Scheduler, it would use frs_yield() instead of sginap().
|| The general pattern of this function is repeated in the other three;
|| only the wait method varies from function to function.
*/
int inWait0(child_t *pch)
{
	int ret;
	aiocb_t* pab = &pch->acb;
	while (EINPROGRESS == (ret = aio_error(pab)))
	{
		sginap(0);
	}
	return ret;
}
void inProc0(void *arg, size_t stk)
{
	child_t *pch = arg;			/* starting arg is ->child_t for my file */
	aiocb_t *pab = &pch->acb;	/* base address of the aiocb_t in child_t */
	int ret;					/* as long as this is 0, all is ok */
	int bytes;					/* #bytes read on each input */
	/*
	|| Initialize -- no signals or callbacks needed.
	*/
	pab->aio_sigevent.sigev_notify = SIGEV_NONE;
	pab->aio_buf = pch->buffer; /* always the same */
#if DO_SPROCS
	/*
	|| Wait for the starting gun...
	*/
	barrier(convene,nprocs);
#endif
	pch->etime = clock();
	do /* read and write, read and write... */
	{
		/*
		|| Set up the aiocb for a read, queue it, and wait for it.
		*/
		pab->aio_fildes = pch->fd;
		pab->aio_offset = pch->inbase;
		pab->aio_nbytes = BLOCKSIZE;
		if (ret = aio_read(pab))
			break;	/* unable to schedule a read */
		ret = inWait0(pch);
		if (ret)
			break;	/* nonzero read completion status */
		/*
		|| get the result of the read() call, the count of bytes read.
		|| Since aio_error returned 0, the count is nonnegative.
		|| It could be 0, or less than BLOCKSIZE, indicating EOF.
		*/
		bytes = aio_return(pab); /* actual read result */
		if (!bytes)
			break;	/* no need to write a last block of 0 */
		pch->inbase += bytes;	/* where to read next time */
		/*
		|| Set up the aiocb for a write, queue it, and wait for it.
		*/
		pab->aio_fildes = outFD;
		pab->aio_nbytes = bytes;
		pab->aio_offset = pch->outbase;
		if (ret = aio_write(pab))
			break;
		ret = inWait0(pch);
		if (ret)
			break;
		pch->outbase += bytes;	/* where to write next time */
	} while ((!ret) && (bytes == BLOCKSIZE));
	/*
	|| The loop is complete.  If no errors so far, use aio_fsync()
	|| to ensure that output is complete.  This requires waiting
	|| yet again.
	*/
	if (!ret)
	{
		if (!(ret = aio_fsync(O_SYNC,pab)))
		ret = inWait0(pch);
	}
	/*
	|| Flag any errors for the parent proc. If none, count elapsed time.
	*/
	if (ret) ++errors;
	else pch->etime = (clock() - pch->etime);
#if DO_SPROCS
	/*
	|| Rendezvous with the rest of the family, then quit.
	*/
	barrier(convene,nprocs);
#endif
	return;
} /* end inProc1 */
/******************************************************************************
|| inProc1 uses aio_suspend() to await the completion of each operation.
|| Otherwise it is the same as inProc0, above.
*/

int inWait1(child_t *pch)
{
	int ret;
	aiocb_t* susplist[1]; /* list of 1 aiocb for aio_suspend() */
	susplist[0] = &pch->acb;
	/*
	|| Note: aio.h declares the 1st argument of aio_suspend() as "const."
	|| The C compiler requires the actual-parameter to match in type,
	|| so the list we pass must either be declared "const aiocb_t*" or
	|| must be cast to that -- else cc gives a warning.  The cast
	|| in the following statement is only to avoid this warning.
	*/
	ret = aio_suspend( (const aiocb_t **) susplist,1,NULL);
	return ret;
}
void inProc1(void *arg, size_t stk)
{
	child_t *pch = arg;			/* starting arg is ->child_t for my file */
	aiocb_t *pab = &pch->acb;	/* base address of the aiocb_t in child_t */
	int ret;					/* as long as this is 0, all is ok */
	int bytes;					/* #bytes read on each input */
	/*
	|| Initialize -- no signals or callbacks needed.
	*/
	pab->aio_sigevent.sigev_notify = SIGEV_NONE;
	pab->aio_buf = pch->buffer; /* always the same */
#if DO_SPROCS
	/*
	|| Wait for the starting gun...
	*/
	barrier(convene,nprocs);
#endif
	pch->etime = clock();
	do /* read and write, read and write... */
	{
		/*
		|| Set up the aiocb for a read, queue it, and wait for it.
		*/
		pab->aio_fildes = pch->fd;
		pab->aio_offset = pch->inbase;
		pab->aio_nbytes = BLOCKSIZE;
		if (ret = aio_read(pab))
			break;
		ret = inWait1(pch);
		/*
		|| If the aio_suspend() return is nonzero, it means that the wait
		|| did not end for i/o completion but because of a signal. Since we
		|| expect no signals here, we take that as an error.
		*/
		if (!ret) /* op is complete */
			ret = aio_error(pab);  /* read() status, should be 0 */
		if (ret)
			break;	/* signal, or nonzero read completion */
		/*
		|| get the result of the read() call, the count of bytes read.
		|| Since aio_error returned 0, the count is nonnegative.
		|| It could be 0, or less than BLOCKSIZE, indicating EOF.
		*/
		bytes = aio_return(pab); /* actual read result */
		if (!bytes)
			break;	/* no need to write a last block of 0 */
		pch->inbase += bytes;	/* where to read next time */
		/*
		|| Set up the aiocb for a write, queue it, and wait for it.
		*/
		pab->aio_fildes = outFD;
		pab->aio_nbytes = bytes;
		pab->aio_offset = pch->outbase;
		if (ret = aio_write(pab))
			break;
		ret = inWait1(pch);
		if (!ret) /* op is complete */
			ret = aio_error(pab);  /* should be 0 */
		if (ret)
			break;
		pch->outbase += bytes;	/* where to write next time */
	} while ((!ret) && (bytes == BLOCKSIZE));
	/*
	|| The loop is complete.  If no errors so far, use aio_fsync()
	|| to ensure that output is complete.  This requires waiting
	|| yet again.
	*/
	if (!ret)
	{
		if (!(ret = aio_fsync(O_SYNC,pab)))
			ret = inWait1(pch);
	}
	/*
	|| Flag any errors for the parent proc. If none, count elapsed time.
	*/
	if (ret) ++errors;
	else pch->etime = (clock() - pch->etime);
#if DO_SPROCS
	/*
	|| Rendezvous with the rest of the family, then quit.
	*/
	barrier(convene,nprocs);
#endif
} /* end inProc0 */
/******************************************************************************
|| inProc2 requests a signal upon completion of an I/O. After starting
|| an operation, it P's a semaphore which is V'd from the signal handler.
*/
#define AIO_SIGNUM SIGRTMIN+1 /* arbitrary choice of signal number */
void sigHandler2(const int signo, const struct siginfo *sif )
{
	/*
	|| In this minimal signal handler we pick up the address of the
	|| child_t info structure -- which was put in aio_sigevent.sigev_value
	|| field during initialization -- and use it to find the semaphore.
	*/
	child_t *pch = sif->si_value.sival_ptr ;
	usvsema(pch->sema);
	return; /* stop here with dbx to print the above address */
}
int inWait2(child_t *pch)
{
	/*
	|| Wait for any signal handler to post the semaphore.  The signal
	|| handler could have been entered before this function is called,
	|| or it could be entered afterward.
	*/
	uspsema(pch->sema);
	/*
	|| Since this process executes only one aio operation at a time,
	|| we can return the status of that operation.  In a more complicated
	|| design, if a signal could arrive from more than one pending
	|| operation, this function could not return status.
	*/
	return aio_error(&pch->acb);
}
void inProc2(void *arg, size_t stk)
{
	child_t *pch = arg;			/* starting arg is ->child_t for my file */
	aiocb_t *pab = &pch->acb;	/* base address of the aiocb_t in child_t */
	int ret;					/* as long as this is 0, all is ok */
	int bytes;					/* #bytes read on each input */
	/*
	|| Initialize -- request a signal in aio_sigevent. The address of
	|| the child_t struct is passed as the siginfo value, for use
	|| in the signal handler.
	*/
	pab->aio_sigevent.sigev_notify = SIGEV_SIGNAL;
	pab->aio_sigevent.sigev_signo = AIO_SIGNUM;
	pab->aio_sigevent.sigev_value.sival_ptr = (void *)pch;
	pab->aio_buf = pch->buffer; /* always the same */
	/*
	|| Initialize -- set up a signal handler for AIO_SIGNUM.
	*/
	{
		struct sigaction sa = {SA_SIGINFO,sigHandler2};
		ret = sigaction(AIO_SIGNUM,&sa,NULL);
		if (ret) ++errors; /* parent will shut down ASAP */
	}	
#if DO_SPROCS
	/*
	|| Wait for the starting gun...
	*/
	barrier(convene,nprocs);
#else
	if (ret) return;
#endif
	pch->etime = clock();
	do /* read and write, read and write... */
	{
		/*
		|| Set up the aiocb for a read, queue it, and wait for it.
		*/
		pab->aio_fildes = pch->fd;
		pab->aio_offset = pch->inbase;
		pab->aio_nbytes = BLOCKSIZE;
		if (!(ret = aio_read(pab)))
			ret = inWait2(pch);
		if (ret)
			break;	/* could not start read, or it ended badly */
		/*
		|| get the result of the read() call, the count of bytes read.
		|| Since aio_error returned 0, the count is nonnegative.
		|| It could be 0, or less than BLOCKSIZE, indicating EOF.
		*/
		bytes = aio_return(pab); /* actual read result */
		if (!bytes)
			break;	/* no need to write a last block of 0 */
		pch->inbase += bytes;	/* where to read next time */
		/*
		|| Set up the aiocb for a write, queue it, and wait for it.
		*/
		pab->aio_fildes = outFD;
		pab->aio_nbytes = bytes;
		pab->aio_offset = pch->outbase;
		if (!(ret = aio_write(pab)))
			 ret = inWait2(pch);
		if (ret)
			break;
		pch->outbase += bytes;	/* where to write next time */
	} while ((!ret) && (bytes == BLOCKSIZE));
	/*
	|| The loop is complete.  If no errors so far, use aio_fsync()
	|| to ensure that output is complete.  This requires waiting
	|| yet again.
	*/
	if (!ret)
	{
		if (!(ret = aio_fsync(O_SYNC,pab)))
			ret = inWait2(pch);
	}
	/*
	|| Flag any errors for the parent proc. If none, count elapsed time.
	*/
	if (ret) ++errors;
	else pch->etime = (clock() - pch->etime);
#if DO_SPROCS
	/*
	|| Rendezvous with the rest of the family, then quit.
	*/
	barrier(convene,nprocs);
#endif
} /* end inProc2 */

/******************************************************************************
|| inProc3 uses a callback and a semaphore. It waits with a P operation.
|| The callback function executes a V operation.  This may come before or
|| after the P operation.
*/
void callBack3(union sigval usv)
{
	/*
	|| The callback function receives the pointer to the child_t struct,
	|| as prepared in aio_sigevent.sigev_value.sival_ptr.  Use this to 
	|| post the semaphore in the child_t struct.
	*/
	child_t *pch = usv.sival_ptr;
	usvsema(pch->sema);
	return;
}
int inWait3(child_t *pch)
{
	/*
	|| Suspend, if necessary, by polling the semaphore.  The callback
	|| function might be entered before we reach this point, or after.
	*/
	uspsema(pch->sema);
	/*
	|| Return the status of the aio operation associated with the sema.
	*/
	return aio_error(&pch->acb);	
}
void inProc3(void *arg, size_t stk)
{
	child_t *pch = arg;			/* starting arg is ->child_t for my file */
	aiocb_t *pab = &pch->acb;	/* base address of the aiocb_t in child_t */
	int ret;					/* as long as this is 0, all is ok */
	int bytes;					/* #bytes read on each input */
	/*
	|| Initialize -- request a callback in aio_sigevent. The address of
	|| the child_t struct is passed as the siginfo value to be passed
	|| into the callback. 
	*/
	pab->aio_sigevent.sigev_notify = SIGEV_CALLBACK;
	pab->aio_sigevent.sigev_func = callBack3;
	pab->aio_sigevent.sigev_value.sival_ptr = (void *)pch;
	pab->aio_buf = pch->buffer; /* always the same */
#if DO_SPROCS
	/*
	|| Wait for the starting gun...
	*/
	barrier(convene,nprocs);
#endif
	pch->etime = clock();
	do /* read and write, read and write... */
	{
		/*
		|| Set up the aiocb for a read, queue it, and wait for it.
		*/
		pab->aio_fildes = pch->fd;
		pab->aio_offset = pch->inbase;
		pab->aio_nbytes = BLOCKSIZE;
		if (!(ret = aio_read(pab)))
			ret = inWait3(pch);
		if (ret)
			break;	/* read error */
		/*
		|| get the result of the read() call, the count of bytes read.
		|| Since aio_error returned 0, the count is nonnegative.
		|| It could be 0, or less than BLOCKSIZE, indicating EOF.
		*/
		bytes = aio_return(pab); /* actual read result */
		if (!bytes)
			break;	/* no need to write a last block of 0 */
		pch->inbase += bytes;	/* where to read next time */
		/*
		|| Set up the aiocb for a write, queue it, and wait for it.
		*/
		pab->aio_fildes = outFD;
		pab->aio_nbytes = bytes;
		pab->aio_offset = pch->outbase;
		if (!(ret = aio_write(pab)))
			 ret = inWait3(pch);
		if (ret)
			break;
		pch->outbase += bytes;	/* where to write next time */
	} while ((!ret) && (bytes == BLOCKSIZE));
	/*
	|| The loop is complete.  If no errors so far, use aio_fsync()
	|| to ensure that output is complete.  This requires waiting
	|| yet again.
	*/
	if (!ret)
	{
		if (!(ret = aio_fsync(O_SYNC,pab)))
			ret = inWait3(pch);
	}
	/*
	|| Flag any errors for the parent proc. If none, count elapsed time.
	*/
	if (ret) ++errors;
	else pch->etime = (clock() - pch->etime);
#if DO_SPROCS
	/*
	|| Rendezvous with the rest of the family, then quit.
	*/
	barrier(convene,nprocs);
#endif
} /* end inProc3 */
