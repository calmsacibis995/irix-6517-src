#ident  "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/ggd/daemon/RCS/ggd.c,v 1.92 1997/09/30 18:50:40 kanoj Exp $"
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/bsd_types.h>
#include <sys/dir.h>
#include <errno.h>
#include <sys/stat.h>
#include <bstring.h>
#include <fcntl.h>
#include <string.h>
#include <getopt.h>
#include <assert.h>
#include <signal.h>
#include <time.h>
#include <grio.h>
#include <sys/mkdev.h>
#include <sys/fs/xfs_inum.h>
#include <sys/syssgi.h>
#include <sys/uuid.h>
#include <sys/major.h>
#include <sys/lock.h>
#include <sys/param.h>
#include <sys/ktime.h>
#include <sys/sysmp.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/scsi.h>
#include <sys/capability.h>
#include <invent.h>
#include "ggd.h"
#include "griotab.h"
#include "externs.h"

/*
 * Guaranteed Granting Daemon (ggd)
 *
 *	This is a user level process started at system boot time. It is
 *	responsible for intercepting guaranteed rate I/O requests from
 *	user processes and determining if the system can satisfy such a
 *	request.
 *
 *	This process maintains a data base of the various types of
 *	SCSI disks and buses and their respective bandwidths.
 *
 */

/* 
 * declare external fucntion prototypes
 */
extern void exit( int );
extern void allocate_stream(void );
extern void deallocate_stream(void );
extern int init_stream_count(void);
extern int can_allocate_stream(void);
extern int vdisk_purge_cache_entry( dev_t );
extern int update_file_reservations( time_t );
extern int expire_reservation(pid_t,  stream_id_t *);
extern int unreserve_disk_bandwidth(dev_t, grio_cmd_t *,int *,int *);
extern int unreserve_vdisk_bandwidth(dev_t, grio_cmd_t *,int *,int *);
extern int unreserve_device_bandwidth(dev_t, grio_cmd_t *,int *,int *);
extern int get_time_until_next_file_operation( grio_cmd_t *, time_t);
extern int reserve_disk_bandwidth(grio_cmd_t*, stream_id_t *,
		int *,time_t *,int *,int *);
extern int reserve_vdisk_bandwidth(grio_cmd_t*, stream_id_t *,
		int *,time_t *,int *,int *);
extern int reserve_device_bandwidth(grio_cmd_t*, stream_id_t *,
		int *,time_t *,int *,int *);
extern int add_new_reservation(grio_cmd_t *,dev_t, int, int);
extern reservations_t *get_resv_from_id(reservations_t *,dev_t, stream_id_t *);
extern int grio_info_get_dev_resvs( char *, grio_stats_t *);
extern int grio_info_get_file_resvs( dev_t, gr_ino_t, grio_stats_t *);
extern int grio_info_get_proc_resvs( pid_t, grio_stats_t *);
extern int create_ggd_info_segment( int );
extern int vdisk_init(void);
extern int process_griotab(char *);

STATIC void command_loop( void);
STATIC void update_reservation_lists( void );
STATIC int already_running( void );
int check_xlv_device( dev_t fs_dev);
STATIC int process_arguments( int, char *[]);
STATIC int get_grio_num_sub_actions(int *, grio_cmd_t *);
STATIC int get_grio_command( grio_cmd_t *);
STATIC int process_grio_command(int, grio_cmd_t *);
STATIC int process_grio_file_resv_command( grio_cmd_t *);
STATIC int process_grio_unresv_file_command( grio_cmd_t *);
STATIC int process_vdisk_purge_command( grio_cmd_t *);
STATIC int reply_to_grio_command( grio_cmd_t *);
STATIC int get_new_grio_stream_id( stream_id_t *);
STATIC int reserve_grio_bandwidth(grio_cmd_t*, stream_id_t*, int *,int *);
STATIC int unreserve_grio_bandwidth(char, grio_cmd_t*, int *, int *);
STATIC int reserve_filesystem_bandwidth(grio_cmd_t*, stream_id_t*, int *,int *);
STATIC int init(void);
STATIC int process_grio_get_stats_command( grio_cmd_t *);
STATIC int process_grio_get_file_resvd_bw_command( grio_cmd_t *);

void ggd_fatal_error( char *errstr );

/*
 * Global debug variables.
 */
int dbglevel  = 0;

extern int defaultoptiosize;

extern int			ggd_node_count;
extern ggd_info_summary_t	*ggd_info;

/*
 * main()
 * 
 *	The routine initializes the global data structures
 *	and starts the command processing loop.
 *
 */
main( int argc, char *argv[] )
{

	if (plock(PROCLOCK)) {
		printf("Could not lock ggd process in memory.\n");
		exit(-1);
	}

	if (already_running()) 
		exit( -1 );

	if ( init_stream_count() )
		exit( -1 );

	if (process_arguments( argc, argv ))
		exit( -1 );

	ggd_node_count = 2000;

	if (create_ggd_info_segment(ggd_node_count) ) {
		ggd_fatal_error("Could not create GRIO shared memory segment");
		exit( -1 );
	}

	if (init())
		exit(-1);

	command_loop();
	return( 0 );
}


/*
 * command_loop()
 *
 *	This routine is the central processing loop. It reads the grio 
 *	command, executes whatever has to be done and if necessary writes 
 *	a return message to the requestor.
 *
 *
 * RETURN:
 *	never returns
 */
STATIC void
command_loop(void)
{
	grio_cmd_t	griocmd_time;
	grio_cmd_t	*griocmd = NULL;
	int		num_sub_acts;


	for (;;) {
		get_time_until_next_file_operation(&griocmd_time, 
							time(0));
		if (get_grio_num_sub_actions(&num_sub_acts, 
					&griocmd_time) == 0) {
			griocmd = (grio_cmd_t *) 
				malloc(num_sub_acts * sizeof(grio_cmd_t));
			assert(griocmd != NULL);
			if (get_grio_command(griocmd) == 0) {
				process_grio_command(num_sub_acts, griocmd);
			} 
			if(griocmd) {
				free(griocmd);
				griocmd = NULL;
			}
		}
		update_reservation_lists();
	}
}


/*
 * set_realtime_cpu
 *	Issue the sytem calls to make the ggd a realtime process.
 *	ISOLATE the given cpu and mark it as NONPREEMPTIVE.
 *	Force ggd to run only on this cpu.
 *
 * RETURNS:
 * 	0 on success
 *	-1 on failure
 */
STATIC int
set_realtime_cpu(int cpunum)
{
	int maxcpus;

	if ((maxcpus = sysmp(MP_NPROCS)) == -1) {
		printf("Cannot get number of processors available.\n");
		return( -1 );
	}


	if ((cpunum < 0) || (cpunum > (maxcpus -1))) {
		printf("Invalid processor number %d \n",cpunum);
		return( -1 );
	}

	if (sysmp(MP_ISOLATE, cpunum) == -1) {
		printf("Could not isolate processor %d.\n", cpunum);
		return( -1 );
	}

#ifndef DEBUG
	if (sysmp(MP_NONPREEMPTIVE, cpunum) == -1) {
		printf("Nonpreempt processor %d failed.\n", cpunum);
		return( -1 );
	}
#endif

	if (sysmp(MP_MUSTRUN, cpunum) == -1) {
		printf("Could not run ggd on processor %d.\n", cpunum);
		return( -1 );
	}

	return(0);
}

/* 
 * process_arguments()
 * 
 *	This routine processes the command line arguments
 * 
 * RETURNS: 
 * 	always returns 0
 */
STATIC int
process_arguments( int argc, char *argv[])
{
	int 		c, cpunum, newlevel;

	/* 
	 * parse command line arguements.
	 */
	while ((c = getopt(argc, argv, "v:c:o")) != EOF) {
		switch(c) {
			case 'v':
				newlevel = atoi(optarg);
				if ( newlevel > dbglevel ) {
					dbglevel = newlevel;
				}

				printf("dbglevel set to %d \n", dbglevel);
				break;
			case 'c':
				cpunum = atoi(optarg);
				set_realtime_cpu(cpunum);
				break;
			case 'o':
				defaultoptiosize = atoi(optarg);
				break;
			default:
				break;
		}
	}
	return( 0 );
}

/*
 * process_grio_command()
 *
 *	This routine parses the user supplied grio_cmd_t-s and calls the
 *	appropriate routines.
 *
 * ASSUMPTIONS:
 *	It is assumed that all return fields in the grio_resv_t structure
 *	will be filled in by the called routines.
 *
 * RETURN:
 *	0 on success
 * 	non-zero on error
 */
STATIC int
process_grio_command(int num_sub_cmds, grio_cmd_t *grio_sub_cmds)
{
	int 		status = 0, errors = 0, i;
	int		asynch_count = 0;
	grio_cmd_t	*griocp;

	dbgprintf( 6, ("process cmd: %d \n",griocp->gr_cmd ) );

	for(i = 0; i < num_sub_cmds; i++)  {
		griocp = &(grio_sub_cmds[i]);
		switch (griocp->gr_cmd) {
			case GRIO_UNRESV:
			case GRIO_UNRESV_ASYNC:
				status = process_grio_unresv_file_command(griocp);
				break;
			case GRIO_RESV:
				status = process_grio_file_resv_command(griocp);
				break;
			case GRIO_PURGE_VDEV_ASYNC:
				status = process_vdisk_purge_command(griocp);
				break;
			case GRIO_GET_BW:
				status = process_grio_file_resv_command(griocp);
				if (griocp->cmd_info.gr_resv.gr_error == 0 ) {
					process_grio_unresv_file_command(griocp);
				}
				break;
			case GRIO_GET_STATS:
				status = process_grio_get_stats_command(griocp);
				break;
			case GRIO_GET_FILE_RESVD_BW:
				status = process_grio_get_file_resvd_bw_command(griocp);
				break;
			default:
				status = EINVAL;
				errors++;
				break;
		}

		if(GRIO_ASYNC_CMD(griocp))  {
			asynch_count++;
		}
		assert( griocp->cmd_info.gr_resv.gr_error == status );

		if ((griocp->gr_cmd < GRIO_MAX_COMMANDS) &&
	     					(griocp->gr_cmd > 0 )) {
			ggd_info->per_cmd_count[ griocp->gr_cmd ] += 1;
		}

		/* reservations need to be updated after the execution of
		 * each sub-cmd.
		 */
		update_reservation_lists();
	}

	/*
	 * send reply to caller
	 */
	if (asynch_count == 0) {   /* all sub-cmds were synchronous */
		reply_to_grio_command(grio_sub_cmds);
	}

	if(errors)  {
		status = EINVAL;
	}

	return status;
}


/*
 * process_grio_file_resv_command()
 *
 *	This routine determines on which virtual disk the given file is
 *	located. It then checks to see if the requested bandwidth is
 *	available and if so, the bandwidth is reserved.
 *	Once the device bandwidth has been reserved, a reservation is
 *	added to the file reservation list.
 *
 *
 * RETURN:
 *	0 on success
 *	non-zero on error
 */
STATIC int
process_grio_file_resv_command( grio_cmd_t *griocp )
{
	int			status;
	int			total_slots = -1, max_count_per_slot = -1;
	grio_resv_t		*griorp;
	stream_id_t		gr_stream_id;


	griorp		= GRIO_GET_RESV_DATA(griocp);

	if ( can_allocate_stream() ) {
		allocate_stream();

		get_new_grio_stream_id( &gr_stream_id );

		status = reserve_grio_bandwidth( griocp,
			&gr_stream_id, &total_slots, &max_count_per_slot);

		if ( status )
			deallocate_stream();
	} else {
		strcpy(griorp->gr_errordev, "license");
		griorp->gr_error = status = ENOSPC;
	}

	/*
	 * Return error.
	 */
	if ( status ) {
		dbgprintf( 1, ("reservation denied with %d \n",status) );
		if ( status == ENOSPC ) {
			dbgprintf(1,("errodev = %s \n",griorp->gr_errordev) );
			dbgprintf(1,("gr_optime = %d \n",griorp->gr_optime) );
			dbgprintf(1,("gr_opsize = %d \n",griorp->gr_opsize) );
		}
		return( status );
	}

	/*
	 * copy id into grio_cmd_t
	 */
	COPY_STREAM_ID( gr_stream_id, griorp->gr_stream_id );

	/*
	 * add the file reservation to the list.
	 */
	add_new_reservation( griocp, griocp->one_end.gr_dev, 
			total_slots, max_count_per_slot);

	return( 0 );
}

STATIC int
process_grio_get_file_resvd_bw_command( grio_cmd_t *griocp )
{
	extern void	get_file_resvd_bw_info_from_cache(grio_cmd_t *);

	/* We walk the cache and for each entry there, we see if
	 * there exists a reservation entry with this stream_id.
	 * We sum up the allocated bandwidth for all such matches
	 * to get the total bandwidth.
	 * This is inefficient, but avoids a lot of code
	 * rearrangement. Besides, this code path is used only
	 * by prioGetBandwidth() which is used rather infrequently
	 * and is provided only for backwards compatibility for the
	 * PRIO APIs.
	 */
	get_file_resvd_bw_info_from_cache(griocp);

	return 0;
}

STATIC int
process_grio_get_stats_command( grio_cmd_t *griocp )
{
	int 		status = EINVAL;
	char		devname[DEV_NMLEN];
	pid_t		procid;
	dev_t		fsdev;
	gr_ino_t	ino;
	grio_stats_t	*griosp;

	griosp = GRIO_GET_STATS_DATA( griocp );

	switch ( griocp->gr_subcmd ) {
		case GRIO_DEV_RESVS:
			strcpy( devname, griosp->devname);
			status = grio_info_get_dev_resvs( devname, griosp );
			break;
		case GRIO_FILE_RESVS:
			fsdev = griocp->one_end.gr_dev;
			ino    = griocp->one_end.gr_ino;
			status = grio_info_get_file_resvs( fsdev, ino, griosp );
			break;
		case GRIO_PROC_RESVS:
			procid = griocp->gr_procid;
			status = grio_info_get_proc_resvs( procid, griosp );
			break;
		default:
			/*
		 	 * unknown subcommand
			 */
			break;
	}

	return( status );
}


/*
 * unreserve_grio_bandwidth()
 *
 *	This routine is a switch which calls the appropriate function
 *	depending on the device type.
 *
 * RETURN:
 *	0 on success
 *	non-zero on error
 */
int
unreserve_grio_bandwidth(
	char		type,
	grio_cmd_t	*griocp,
	int		*maxreqsize,
	int		*maxreqtime)
{
	int status = 0;

	switch (type) {
		case END_TYPE_REG:
			if ( check_xlv_device( griocp->one_end.gr_dev) ) {
				if ( status = unreserve_vdisk_bandwidth( 
						griocp->one_end.gr_dev, griocp,
						maxreqsize, maxreqtime ) ) {
					dbgprintf( 1, ("unreserve vdisk bandwidth failed.\n"));
				}
			} else {
				if ( status = unreserve_disk_bandwidth(
						griocp->one_end.gr_dev, griocp,
						maxreqsize, maxreqtime ) ) {
					dbgprintf( 1, ("unreserve disk bandwidth failed.\n"));
				}
			}
			break;

		case END_TYPE_SPECIAL:
#if LATER
			if ( status = unreserve_device_bandwidth(
						griocp->one_end.gr_dev, griocp,
						maxreqsize, maxreqtime ) ) {
				dbgprintf( 1, ("unreserve device bandwidth failed.\n"));
			}
#else
			status = EBADF;
#endif /* LATER */
			break;

		default:
			status = EIO;
			break;
	}

	return (status);
}


/*
 * process_grio_unresv_file_command()
 *
 *	This routine removes a grio rate guarantee. It tranverses the 
 * 	device tree to remove the reservations associated with the 
 * 	given guarantee	and then removes the file reservation.
 *
 * RETURN:
 *	0 on success
 *	non-zero on error
 */
STATIC int
process_grio_unresv_file_command( grio_cmd_t *griocp )
{
	int 		status = EINVAL;
	int		maxreqtime, maxreqsize;
	reservations_t	*resvp;

	resvp = get_resv_from_id( FILE_RSV_LIST(), 0, 
			&(griocp->cmd_info.gr_resv.gr_stream_id));

	if ( resvp == NULL ) {
		griocp->cmd_info.gr_resv.gr_error = status;
		return( status );
	}

	griocp->one_end.gr_end_type = resvp->resv_info.glob_resv_info.file_type;
	griocp->one_end.gr_dev = resvp->resv_info.glob_resv_info.fs_dev;
	griocp->one_end.gr_ino = resvp->resv_info.glob_resv_info.inum;

	status = unreserve_grio_bandwidth(griocp->one_end.gr_end_type, griocp,
			&maxreqsize, &maxreqtime);

	/*
	 * return bandwidth to grio library
	 * so it can save the info in the library dbase.
	 */
	griocp->gr_bytes_bw =  maxreqsize;
	griocp->gr_usecs_bw =  maxreqtime;

	dbgprintf( 6, ("ggd: unreserve returning max = %d, maxtime = %d \n",
		maxreqsize, maxreqtime));


	/*
	 * If the bandwidth reservation has been removed,
	 * mark the file reservation as expired.
	 */
	if  (status == 0) {
		if ( status = expire_reservation( 
				griocp->gr_procid,
				&(griocp->cmd_info.gr_resv.gr_stream_id)) ) {
			dbgprintf( 1, ("could not expire file reservation \n"));
		}
	} 
	return( status );
}

/*
 * process_vdisk_purge_request()
 *	This routine is called when the XLV volume information has been
 *	updated and the ggd needs to read new copies from the kernel.
 *
 *
 * RETURNS
 *	0 on success
 *
 */
STATIC int
process_vdisk_purge_command( grio_cmd_t *griocp )
{
	return(vdisk_purge_cache_entry(griocp->one_end.gr_dev));
}

/*
 * update_reservation_lists() 
 * 	This routine removes expired reservations. It is called 
 *	frequently to keep the reservation lists short ( hopefully ).
 * 
 * RETURNS: 
 *	none 	
 */
STATIC void
update_reservation_lists(void)
{
	time_t now = time(0);

	/*
  	 * If there were no file reservation changes, then there can't
	 * be any controller/disk changes.
	 */
	if (update_file_reservations(now)) {
		update_device_reservations(now);
	}
}

/* get_grio_num_sub_actions()
**	This routine returns the number of sub-actions in the 
**	first element of the grio-queue that was inserted by
**	libgrio; if there are no elements yet (the queue-head is NULL
**	it returns 0
*/
STATIC	int
get_grio_num_sub_actions(int *num_cmds, grio_cmd_t *griotime)
{
	int	status;
	cap_t	ocap;
	cap_value_t cap_device_mgt = CAP_DEVICE_MGT;
	
	ocap = cap_acquire(1, &cap_device_mgt);
	status = syssgi(SGI_GRIO, GRIO_READ_NUM_CMDS, num_cmds, griotime);
	cap_surrender(ocap);
	dbgprintf(10,("sgi_grio: GRIO_READ_NUM_CMDS returned \n"));
	return	status;
}


/*
 * get_grio_command()
 *
 *	This routine reads the next grio_cmd_t structure from the
 *	pipe. It performs some verification of the validity of the 
 *	contents and then returns the structure to the caller.
 *
 *  NOTE:
 *	This routine will block until a request has been written to the
 * 	pipe or until the command time has expired.
 *
 * RETURNS:
 *	1 if a command was read successfully
 *	0 otherwise
 */
STATIC int
get_grio_command( grio_cmd_t *griocp) 
{
	int 	status;
	cap_t	ocap;
	cap_value_t cap_device_mgt = CAP_DEVICE_MGT;

	ocap = cap_acquire(1, &cap_device_mgt);
	status = syssgi(SGI_GRIO, GRIO_READ_GRIO_REQ, griocp);
	cap_surrender(ocap);

	dbgprintf(10,("sgi_grio: READ_GRIO_REQ returned \n"));

	return( status );
}

/*
 * get_new_grio_stream_id
 *
 * RETURNS:
 *	a new uuid
 */
STATIC int
get_new_grio_stream_id( stream_id_t *gr_stream_id)
{
	uint_t	status;

	uuid_create( gr_stream_id, &status);

	return( status );
}


/*
 *  already_running()
 *	This routine determines if a copy of the ggd daemon is 
 *	already active.
 *
 * RETURNS:
 *	1 if a copy of the daemon is already running
 *	0 if not
 */
STATIC int
already_running( void )
{
	int 		ret = 1, result;
	char		line[100];
	int 		fresh = 0;
	pid_t		pid;
	cap_t		ocap;
	cap_value_t	cap_device_mgt = CAP_DEVICE_MGT;

	/*
	 * This tries to do atomic updating of the lock file, to prevent 
	 * multiple ggds running at once
	 */
	int fd = open(GRIO_LOCK_FILE, O_RDWR);
	if (fd == -1) {
		if ( errno == ENOENT ) {
			if ((fd = creat( GRIO_LOCK_FILE, 0644)) == -1) {
				sprintf(line, 
					"Could not create ggd lock file %s", 
					GRIO_LOCK_FILE);
				ggd_fatal_error(line);
				return 0;
			}
			fresh = 1;
		} else {
			sprintf(line, 
				"Could not create ggd lock file %s", 
				GRIO_LOCK_FILE);
			ggd_fatal_error(line);
			return ( ret );
		}
	}

	if (lockf(fd, F_TLOCK, 0) == -1) {
		sprintf(line, 
			"Could not access ggd lock file %s", 
			GRIO_LOCK_FILE);
		ggd_fatal_error(line);
		close(fd);
		return ( ret );
	}

	if (!fresh) {
		if ((result = read(fd, &pid, sizeof pid)) == -1) {
			sprintf(line, 
				"Could not read ggd lock file %s", 
				GRIO_LOCK_FILE);
			ggd_fatal_error(line);
			close(fd);
			return ( ret );
		} else if (result == sizeof pid) {
			if (kill(pid, 0) != -1) {
				ggd_fatal_error("The ggd daemon is "
					"already running");
				close(fd);
				return ( ret );
			} else if (oserror() != ESRCH) {
				ggd_fatal_error("Could not determine "
					"if ggd daemon is already running");
				close(fd);
				return ( ret );
			}
		}
		lseek(fd, 0, SEEK_SET);
	}

	pid = getpid();
	if ((result = write(fd, &pid, sizeof pid)) != sizeof pid) {
		sprintf(line, 
			"Could not update ggd lock file %s", 
			GRIO_LOCK_FILE);
		ggd_fatal_error(line);
		unlink( GRIO_LOCK_FILE);
		close(fd);
		return ( ret );
	}

	close(fd);
	ocap = cap_acquire(1, &cap_device_mgt);
	errno = ret = 0;
	syssgi(SGI_GRIO, GRIO_REMOVE_ALL_STREAMS_INFO, 0, 0);
	if (errno == ENOSYS) ret = 1;
	cap_surrender(ocap);
	if (ret) {
		printf("The UNIX kernel is configured without grio\n");
		exit(1);
	}
	return(0);
}


/* 
 * reply_to_grio_command()
 *
 *	This routine uses the sysgi system call GRIO_WRITE_GRIO_RESP
 *	to send a response to the process that issued a bunch of 
 *	requests to the ggd.
 *
 * RETURN:
 *	0 on success
 *	exits on failure
 */
STATIC int
reply_to_grio_command(grio_cmd_t *grio_sub_cmds)
{
	cap_t		ocap;
	cap_value_t	cap_device_mgt = CAP_DEVICE_MGT;
	int		ret = 0, tmperr;
loop:
	ocap = cap_acquire(1, &cap_device_mgt);
	errno = 0;
	ret = syssgi(SGI_GRIO, GRIO_WRITE_GRIO_RESP, grio_sub_cmds);
	tmperr = errno;
	cap_surrender(ocap);

	if (ret == 0)
		return(0);

	/*
	 * if the requesting process was killed and took its request
	 * off the queue, just resume.
	 */
	else if (tmperr == ENOENT)
		return(0);

	/*
	 * retry if memory is scarce.
	 */
	else if (tmperr == ENOMEM)
		goto loop;
	else
		ggd_fatal_error("could not reply to request");
	return(0);
}

/*
 * check_xlv_device
 *	Return true if fs_dev is an XLV device.
 *
 * RETURNS:
 *	1 if the device if good
 *	0 if it is not
 */
int
check_xlv_device( dev_t fs_dev)
{
	major_t	maj;

	maj = major( fs_dev );
	return( maj == XLV_MAJOR );
}

/*
 * reserve_grio_bandwidth
 *	Reserve the bandwidth for the given reservation.
 *	This function is basically a switch which calls the
 *	appropriate function based on the device type.
 *
 * RETURNS:
 *	0 on success
 *	non-zero on error
 */
STATIC int
reserve_grio_bandwidth(
	grio_cmd_t		*griocp,
	stream_id_t		*gr_stream_id,
	int			*total_slots,
	int			*max_count_per_slot)
{
	int	error=0;
#if LATER
	int 		max = 0;
	time_t		maxtime;
#endif /* LATER */

	switch (griocp->one_end.gr_end_type) {
		case END_TYPE_REG:
			error = reserve_filesystem_bandwidth(griocp,
					gr_stream_id, total_slots,
					max_count_per_slot);
			break;

		case END_TYPE_SPECIAL:
#if LATER
			error = reserve_device_bandwidth(griocp,
					gr_stream_id, &max, &maxtime,
					total_slots,
					max_count_per_slot);
#else
			error = EBADF;
#endif /* LATER */
			break;

		default:
			error = EBADF;
			break;
	}

	return (error);
}

/*
 * reserve_filesystem_bandwidth
 *	Reserve the bandwidth for the given reservation.
 *	Fill the gr_bytes_bw and gr_usec_bw fields of the return
 *	command structure. If the reservation is denied, fill in
 *	gr_opsize and gr_optime fields to contain the remaining device
 *	bandwidth.
 *
 * RETURNS:
 *	0 on sucess
 *	non-zero on error
 */
STATIC int
reserve_filesystem_bandwidth(
	grio_cmd_t		*griocp,
	stream_id_t		*gr_stream_id, 
	int			*total_slots,
	int			*max_count_per_slot)

{
	int 		max = 0;
	time_t		maxtime;
	grio_resv_t	*griorp = GRIO_GET_RESV_DATA(griocp);

	dbgprintf(3, ("resv fs band on %x \n",griocp->one_end.gr_dev) );
	error_device_node = NULL;

	if ( check_xlv_device(griocp->one_end.gr_dev) ) {
		reserve_vdisk_bandwidth( griocp, gr_stream_id, 
			&max, &maxtime,total_slots, max_count_per_slot);
	} else {
		reserve_disk_bandwidth( griocp, gr_stream_id,
			&max, &maxtime,total_slots, max_count_per_slot);
	}

	/*
	 * if griorp->gr_error is set to ENOSPC, then find
	 * the maximum bandwidth available on the file system
 	 */
	if ( griorp->gr_error == ENOSPC ) {
		__int64_t reqsz, reqtm;

		/*
		 * We return ENOSPC for more than 1 condition.
		 * Check that we are really out of bw.
		 */
		if (error_device_node) {
			determine_remaining_bandwidth(error_device_node,
				griorp->gr_start, &reqsz, &reqtm);
			max = (int)reqsz;
			maxtime = (time_t)reqtm;
		} else {
			max = 0;
			maxtime = 0;
		}
		griorp->gr_opsize = max;
		griorp->gr_optime = maxtime;
	}

	/*
	 * return bandwidth to grio library
	 * so it can save the info in the library dbase.
	 */
	griocp->gr_bytes_bw =  max;
	griocp->gr_usecs_bw =  maxtime;

	error_device_node = NULL;
	return( griorp->gr_error );
}

/*
 * ggd_fatal_error
 *	Print the given error message and exit.
 *
 * RETURNS:
 *	does not return
 */
void
ggd_fatal_error( char *errstr )
{
	printf("GGD FATAL ERROR: %s \n",errstr);
	printf("Exiting.\n");
	exit( -1 );
}


/*
 * This routine calls any init routines for subsystems supported
 * by ggd.
 */

STATIC int
init(void)
{
	init_cache();

	/*
	 * Find out the SGI recommended/administrator specified
	 * parameters.
	 */
	if (process_griotab(GRIODISKS)) 
		return(1);
	if (vdisk_init()) 
		return(1);
	update_device_reservations(time(0));
	return(0);
}

