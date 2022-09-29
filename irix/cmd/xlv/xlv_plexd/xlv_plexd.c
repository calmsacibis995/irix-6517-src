/**************************************************************************
 *                                                                        *
 *              Copyright (C) 1994, Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.22 $"

/*
 * xlv_plexd - User level daemon for plex copies.
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <paths.h>
#include <assert.h>
#include <stdarg.h>
#include <syslog.h>
#include <sys/sysmacros.h>
#include <sys/uuid.h>
#include <sys/flock.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/debug.h>
#include <sys/wait.h>
#include <sys/dkio.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <xlv_utils.h>
#include <xlv_cap.h>
#include "xlv_plexd_priv.h"


extern char 	*optarg;
extern int 	optind, opterr, optopt;

#define	MY_TMPDIR	"/etc"

char 	    	*xlv_plexd_tmp_dir;
char 	    	*xlv_plexd_cmd_name;
int		xlv_plexd_mlog_level;

/* we keep the 'context' of the daemon around since we need to access it
   from the signal handler */
xlv_plexd_master_t 	*plexd_ctx = 0;

char msg_cant_bg[] = "failed to daemonize : %s\n";
char msg_cant_open_fifo[] = "Open failed on %s: %s\n";
char msg_read_fifo[] = "Read request on FIFO: %d out of %d\n";
char msg_cant_read_fifo[] = "Read error on FIFO : %s\n";
char msg_cant_dequeue[] = "dequeue failed on %s : %s\n";
char msg_cant_enqueue[] = "enqueue failed on %s : %s\n";
char msg_missing_dead_pid[] = "Dead process %ld not in pid array\n";
char msg_cant_exec[] = "Cannot exec child to serve subvol \"%s\" : %s\n";
char msg_exiting[] = "plex-daemon exiting\n";
char msg_plex_copy[] = "plex_copy: %s, start=%lld, end=%lld, block_size=%d sleep_msec=%d\n";
char msg_cant_open_dev[] = "cannot open %s (dev %d.%d): %s\n";
char msg_cant_do_plex_copy[] = "DIOCXLVPLEXCOPY on %s (dev %d.%d) failed: %s\n";
char msg_revive_done[] = "Done reviving %s (minor# %d) : blk range (%lld - %lld)\n";
char msg_start_time[] = "started at %s\n";
char msg_no_subvol_name[] = "subvolume name is not specified.\n";

 
void
usage(void)
{
	printf ("usage: xlv_plexd -m <children> -b <blocksize>\n"
		"                 -w <sleep_intvl> -v <verbosity>\n");
	/* printf ("\t[-d device] [start end]\n\n"); */
	
	printf ("-m nslaves	# children. This is the Master Process.\n");
	printf ("-b blksz	Granularity of the plex-revive.\n");
	printf ("-w wait-intvl	Pause between reviving disk-chunks "
		"(msecs).\n");
	printf ("-v verbosity	Level 0 = min 3 = max. Default 2.\n");
	
	exit(0);
}


void
background(void)
{
        if (_daemonize(_DF_NOCHDIR, -1, -1, -1) < 0) {
		mlog(MLOG_NORMAL, msg_cant_bg, strerror(errno));
                exit(1);
        }
}


/* 
 * We catch death-of-child signals here. We get the child's pid,
 * and need to adjust our books accordingly, and spawn another 
 * process, if needed.
 */
/*ARGSUSED*/
void
sighandler(int sig)
{
	long pid;
	int status;

	pid = wait(&status);

	(void) plexd__free_process(pid);

	/* spawn new process(es) to do other pending work, if any */
	(void) plexd__service_rqsts ();

	/*
	 * Now reinstall handler & cause SIGCLD to be re-raised
	 * if any more children exited while we were in here.
	 */
	sigset(SIGCLD, sighandler);
}


/* 
 * all the 'run-time' errors are logged here. 
 * This is in XLV_PLEXD_LOG.
 */
void
mlog(int level, char *fmt, ... )
{
	va_list args;
	char   	str[256];

	if (level > xlv_plexd_mlog_level)
		return;

/* XXX should be calling vsyslog(3) */
	va_start(args, fmt);
	vsprintf(str, fmt, args);
	va_end(args);

	openlog(xlv_plexd_cmd_name, LOG_PID | LOG_NOWAIT, LOG_DAEMON);
        syslog(LOG_DAEMON | LOG_INFO, str);
        closelog();
}


/*
 * This is called by the initial instance of xlv_plexd. It opens
 * the fifo, waits for a request, forks off copies of itself to
 * execute the plex copies, and then waits for the next request.
 */
int
plexd__wait_for_requests(void) 
{
	int				fd, nread;
	xlv_plexd_request_t		rqsts;

	/*CONSTCOND*/
	for (;;) {
		/* We do want to get the SIGCLD signals */
		sigrelse (SIGCLD);
		/*
		 * This call will block until there is a sender, or until
		 * we get a SIGCLD signal..
		 */
		fd = open (XLV_PLEXD_RQST_FIFO, O_RDONLY);
		sighold (SIGCLD);
		if ( fd < 0 ) {
			if ( errno != EINTR ) {
				mlog(MLOG_NORMAL, msg_cant_open_fifo,
				     XLV_PLEXD_RQST_FIFO, strerror(errno));
				continue; /* dont really want to break */
			}
			/* Death of Child - Syscall got interrupted.
			 * just continue. the signal handler took care of
			 * updating our data structures
			 */
			continue;
		}

		/*CONSTCOND*/
		for (;;) { /* process data in the request fifo */
			sigrelse (SIGCLD);
			nread = read (fd, &rqsts, sizeof(xlv_plexd_request_t));
			mlog(MLOG_DEBUG, msg_read_fifo,
			     nread, sizeof(xlv_plexd_request_t));

			sighold(SIGCLD);

			if (nread < 0) {
				if (errno == EINTR)
					continue;
				mlog(MLOG_VERBOSE, msg_cant_read_fifo, 
				     strerror(errno));
				break;	/* stop reading this fifo */	
			}

			if (nread == 0)
				/*
				 * Nothing to read so stop processing fifo.
				 *
				 * XXX If we were doing a blocking read,
				 * we can do a "continue" instead of
				 * breaking out of this loop.
				 */
				break;

			/*
			 * The data is a request iff the size is correct.
			 * Otherwise ignore the data and continue to
			 * read another data bytes.
			 */
			if (nread == (int) sizeof(xlv_plexd_request_t)) {
				plexd__process_request (&rqsts);
			}
		}

		close (fd);
	}

	/*NOTREACHED*/
	return errno;

} /* end of plexd__wait_for_requests() */


/*
 * Processes a single array of xlvd requests. 
 */
void
plexd__process_request(xlv_plexd_request_t	*xlv_plexd_request)
{
	unsigned	i;

	/* First, we queue-up all our requests according to its subvolume */
	for (i=0; i < xlv_plexd_request->num_requests; i++) {
		plexd__queue_up_rqst( &xlv_plexd_request->request[i] );
	}

	/* We have to go thru the separate queues for different subvolumes,
	   and figure out who needs to be serviced, and spawn processes to
	   do the work */
	(void)plexd__service_rqsts ();
}


/*
 * We first create an array of queues that are not being serviced by a 
 * process. Then we sort it by the number of requests each queue has
 * pending, to get some kind of fairness.
 */

dev_t
plexd__select_queue(xlv_plexd_queue_t  **q)
{
	xlv_plexd_sort_t  *qarr;
	int num;
	dev_t	key;

	*q = 0;
	
	qarr = (xlv_plexd_sort_t *) malloc(
			htab_nentries(plexd_ctx->subvol_tab)
			* sizeof(xlv_plexd_sort_t));
	num = htab_getselect_entries(plexd_ctx->subvol_tab, qarr,
				     (xlv_plexd_cb_t)plexd__is_entry_good );
	if ( !num ) {
		free (qarr);
		return NULL;
	}
		
	qsort((void*)qarr, (size_t)num, sizeof(xlv_plexd_sort_t), nentries_cmp);
	
	/* select the one with the highest nentries left */
	*q = (xlv_plexd_queue_t *) qarr[num-1].queue;
	key = (dev_t) qarr[num-1].key;
	free (qarr);	/* free the sort structures ie. pointers */

	return key;
}


int
nentries_cmp( 
	const void 	*s1, 
	const void 	*s2 )
{
        return (int) ( ((xlv_plexd_sort_t *)s1)->queue->nentries - 
                       ((xlv_plexd_sort_t *)s2)->queue->nentries );
}


/*
 * See if we can afford to spawn a slave to do the work,
 * if so, see if we have work to be done. If so, find the job
 * that is most fair on everybody, and as they say, just do it!
 */
int
plexd__service_rqsts(void)
{
	xlv_plexd_queue_t  *q;
	u_int	i, j;
	dev_t	key;
	xlv_plexd_request_entry_t *request = NULL;

	/* If all our procs are busy we just have to wait */
	if ( plexd_ctx->cur_nprocesses >= plexd_ctx->max_nprocesses ) {
		return 0;
	}
	
	for (i=plexd_ctx->cur_nprocesses; i < plexd_ctx->max_nprocesses; i++) {
		key = (dev_t)plexd__select_queue(&q);	/* nentries > 0 */
		
		/* if there isnt anything that we can service .. */
		if (!q) {
			return 0;
		}
		
		(void) q_dequeue (q, (void **)&request);
		
		if (! request) {
			mlog(MLOG_DEBUG, msg_cant_dequeue,
			     request->subvol_name, strerror(errno));
			return -1;
		}

		for (j = 0; j < plexd_ctx->max_nprocesses; j++) 
			if ( plexd_ctx->pidarr[j].pid == 0 )
				break;
		/*
		 * at this point if we dont have a free pid slot,
		 * there's something really wrong
		 */
		assert (j < plexd_ctx->max_nprocesses);
		q->nprocesses++;
		plexd_ctx->cur_nprocesses++;
		plexd_ctx->pidarr[j].queue = q;
		plexd_ctx->pidarr[j].svdevice = key;
		plexd_ctx->pidarr[j].pid = plexd__spawn_request (request);
		free (request);
	}
	return 1;
}


/* 
 * Enqueue a single revive-request in the appropriate queue, based on 
 * the corresponding subvolume.
 */
int
plexd__queue_up_rqst(xlv_plexd_request_entry_t *r)
{
	xlv_plexd_queue_t *q;
	xlv_plexd_request_entry_t *rqst;
	
	/*
	 * we are going to make a copy so that rqst-entry will persist. 
	 * we didnt alloc the entire request array on the heap
	 * because it isnt full normally.
	 */
	rqst = (xlv_plexd_request_entry_t *) 
			malloc(sizeof(xlv_plexd_request_entry_t));
	memcpy(rqst, r, sizeof(xlv_plexd_request_entry_t));

	q = htab_getentry(plexd_ctx->subvol_tab, rqst->device);

	if (! q) {
		q = q_construct(sizeof(xlv_plexd_request_entry_t));
		if (! q) {
		    return -1;
		}
		htab_putentry(plexd_ctx->subvol_tab, rqst->device, q);
	}
	
	if ( q_enqueue(q, (void *)rqst) < 0 ) {
		mlog(MLOG_DEBUG, msg_cant_enqueue,
		     rqst->subvol_name, strerror(errno));
		return -1;
	}
	return 1;
}


/*
 * This is called with the pid of a dead child.
 * We just remove it from our records, and decrease
 * the total num of active processes.
 */

int
plexd__free_process(long pid)
{
	u_int i;
	xlv_plexd_queue_t *q = NULL;

	/* the number of processes is a small number; we can afford
	   to sequentially search it for a pid */
	for (i = 0; i < plexd_ctx->max_nprocesses; i++) {
		if ( plexd_ctx->pidarr[i].pid == pid ) {
			q = plexd_ctx->pidarr[i].queue;
			break;
		}
	}
	if (q == NULL) {
		mlog(MLOG_DEBUG, msg_missing_dead_pid, pid);
		return -1;
	}
	
	/* Now we need to mark the queue as not being serviced.
	   Note that only the main thread executes this code */
	q->nprocesses--;
	plexd_ctx->cur_nprocesses--;
	plexd_ctx->pidarr[i].pid = 0;
	if (q->nprocesses == 0) {
		/* no need to lock cuz there's nobody else */
		if (q->nentries == 0) {
			htab_delentry ( plexd_ctx->subvol_tab, 
				        plexd_ctx->pidarr[i].svdevice);
			q_destruct (q);
		}
	}
	
	return 1;
}


/*
 * A good entry is a queue that needs service.
 * That is, one that has enqueued entries, but no one
 * servicing any of them.
 */
u_int
plexd__is_entry_good(void *item)
{
	xlv_plexd_queue_t *q = (xlv_plexd_queue_t *)item;
	
	assert( q );

	return ( q->nentries > 0 && q->nprocesses == 0 ) ?
		(u_int) 1 : (u_int)0;
}


/*
 * Forks and execs a single plex copy request.
 */
long
plexd__spawn_request(xlv_plexd_request_entry_t  *request)
{
	long	pid;
	
	pid = fork();

	if (pid == 0) {
		/* 
		 * In child.
		 */
		char	start_blkno_str[10], end_blkno_str[10];
		char	block_size_str[10], sleep_intvl_msec_str[10];
		char	mlog_level_str[10];
		char 	device_str[10];
		int	retval;

		sprintf (start_blkno_str, "%d", request->start_blkno);
		sprintf (end_blkno_str, "%d", request->end_blkno);
		sprintf (block_size_str, "%d", plexd_ctx->block_size);
		sprintf (sleep_intvl_msec_str, "%d", 
			 plexd_ctx->sleep_intvl_msec);
		sprintf (mlog_level_str, "%d", xlv_plexd_mlog_level);
		sprintf (device_str, "%#lx", request->device);

		retval = execl(xlv_plexd_cmd_name,
		      "xlv_plexd",
		      "-t", xlv_plexd_tmp_dir,
		      "-v", mlog_level_str,
		      "-n", request->subvol_name,
		      "-d", device_str,
		      "-b", block_size_str,
		      "-w", sleep_intvl_msec_str,
		      start_blkno_str,
		      end_blkno_str,
		      NULL);

		if (retval) {
			mlog(MLOG_NORMAL, msg_cant_exec,
			     request->subvol_name, strerror(errno));
			exit (-1);
		}
	}
	
	return pid;
}


void
plexd__start_daemon (
	u_int 	max_subprocesses, 
	u_int 	sleep_intvl_msec,
	u_int 	block_size)
{
	int status;
	
	/*
	 * Delete our request fifo in the foreground and then recreate it.
	 * We don't care if it did not already exist.
	 */
	(void) unlink (XLV_PLEXD_RQST_FIFO);

	if (mknod (XLV_PLEXD_RQST_FIFO, S_IFIFO | 0666, 0)) {
		status = errno;
		goto err;
	}

	background();
	plexd_ctx = ( xlv_plexd_master_t *) 
		malloc ( sizeof (xlv_plexd_master_t) );
	plexd_ctx->cur_nprocesses = 0;
	if (max_subprocesses < 1)
		plexd_ctx->max_nprocesses = 1;
	else
		plexd_ctx->max_nprocesses = max_subprocesses;

	plexd_ctx->block_size = block_size;
	plexd_ctx->sleep_intvl_msec = sleep_intvl_msec;
	plexd_ctx->pidarr = calloc (max_subprocesses, 
				    sizeof (xlv_plexd_pidarr_entry_t));
	plexd_ctx->subvol_tab = htab_construct();
	status = plexd__wait_for_requests ();
err:
	mlog(MLOG_NORMAL, msg_exiting);
	exit(status);           /* we should never get here */
}


/*
 * performs the syscall to do the plex-copy. This is executed 
 * only by a child process.
 */
void
plexd__serve_revive_request(
	char	       *svname,
	dev_t		dev,
	__int64_t	start_blkno, 
	__int64_t	end_blkno,
	unsigned	block_size,
	unsigned	sleep_intvl_msec)
{
	xlv_plex_copy_param_t	plex_copy;
	int			fd;

	mlog(MLOG_DEBUG, msg_plex_copy,
	     svname, start_blkno, end_blkno, block_size, sleep_intvl_msec);

	/*
	 * Bug 471909: Using /tmp can be bad if /tmp is a logical volume
	 * because a plex revive on startup can stop /tmp from mounting.
	 */
	if (xlv_plexd_tmp_dir == NULL)
		xlv_plexd_tmp_dir = MY_TMPDIR;
	fd = xlv_subvol_open_by_dev(dev, svname, xlv_plexd_tmp_dir, O_RDWR);
	if (fd == -1) {
		mlog(MLOG_NORMAL, msg_cant_open_dev,
		     svname, major(dev), minor(dev), strerror(errno));
		exit (-1);
	}

	plex_copy.version = XLV_PLEX_COPY_PARAM_VERS;
	plex_copy.start_blkno = start_blkno;
	plex_copy.end_blkno = end_blkno;
	plex_copy.chunksize = block_size;
	plex_copy.sleep_intvl_msec = sleep_intvl_msec;


        if (ioctl(fd, DIOCXLVPLEXCOPY, &plex_copy) == -1) {
		mlog(MLOG_NORMAL, msg_cant_do_plex_copy, 
		     svname,  major(dev), minor(dev), strerror(errno));
		exit (-1);
	}

	mlog(MLOG_DEBUG, msg_revive_done, 
	     svname, minor(dev), start_blkno, end_blkno);
	
	return;
}



/*
 * M A I N
 *
 * Both the master and its short-lived subprocesses go thru here.
 */
int
main(int argc, char **argv) 
{
	
	unsigned		is_master = 0;
	__int64_t		start_blkno, end_blkno;
	char			*svname = NULL;
	unsigned		block_size = 64*2;      /* 64 K blocks */
	unsigned		sleep_intvl_msec = 0;	/* no wait */
	unsigned		max_subprocesses;
	int			opt;
	dev_t 			dev;

	if (cap_envl(0, CAP_DEVICE_MGT, 0) == -1 || sscanf(*argv,"-h")) {
		fprintf(stderr,
			"%s: must be started by super-user\n", 
			argv[0]);
		exit(1);
	}
	xlv_plexd_tmp_dir = MY_TMPDIR;
	xlv_plexd_cmd_name = argv[0];
	xlv_plexd_mlog_level = MLOG_DEBUG;
	opterr = 0;
	while ((opt = getopt(argc, argv, "t:v:d:m:b:w:n:h")) != EOF)
		switch (opt) {
		case 'm':
			is_master = 1;
			sscanf (optarg, "%d", &max_subprocesses);
			sigset(SIGCLD, sighandler);
			sighold(SIGCLD);
			break;

		case 'v':
			sscanf (optarg, "%d", &xlv_plexd_mlog_level);
			break;
		case 'b':
			sscanf (optarg, "%d", &block_size);
			break;
		case 'w':
			sscanf (optarg, "%d", &sleep_intvl_msec);
			break;
		case 't':
			xlv_plexd_tmp_dir = optarg;
			break;

	        /* This option is meaningless for the Master Process */
		case 'n':
			if (is_master) {
				usage();
				break;
			}
			svname = optarg;
			break;	
			
		case 'd':
			if (is_master) {
				usage();
				break;
			}
			sscanf (optarg, "%lx", &dev);
			break;	
		default:
			usage();	/* and exit */
			break;		/* lint */
		}
	argc -= optind;
	argv += optind;

	if (argc >= 2) {
		sscanf (argv[0], "%lld", &start_blkno);
		sscanf (argv[1], "%lld", &end_blkno);
	}

	/*
	 * If this is the master xlv_plexd process, put ourselves into
	 * the background and wait for requests on the fifo.
	 */
	if (is_master) {
		time_t tm;
		const int zero = 0;

		if (svname)
			usage();
		tm = time(NULL);
		mlog(MLOG_VERBOSE, msg_start_time, ctime(&tm));

		/* won't return */
		plexd__start_daemon (max_subprocesses, sleep_intvl_msec, 
			      block_size); 
		assert(zero);
        }
	
	if (svname == NULL) {
		mlog(MLOG_NORMAL, msg_no_subvol_name);
		exit (-1);
	}
	
	/*
	 * Send the revive-request to the kernel 
	 */
	plexd__serve_revive_request(svname, dev, start_blkno, end_blkno, 
				    block_size, sleep_intvl_msec);
	return (0);

} /* end of main() */
