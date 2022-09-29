 #ident "$Header: /proj/irix6.5.7m/isms/irix/lib/libgrio/src/RCS/grio_lib.c,v 1.47 1997/04/30 22:27:49 singal Exp $"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <grio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/statvfs.h>	/* needed for prioGetBandwidth */
#include <sys/stat.h>
#include <sys/errno.h>
#include <errno.h>
#include <sys/bsd_types.h>
#include <string.h>
#include <bstring.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/fs/xfs_inum.h>
#include <sys/syssgi.h>
#include <errno.h>
#include <sys/capability.h>

int grio_check_flags(grio_resv_t *);
int grio_set_defaults_and_check_flags( grio_resv_t *);
int grio_get_results(grio_resv_t *,grio_cmd_t *,int);
void grio_create_command_struct(grio_resv_t *,grio_cmd_t *,int,int,struct end_info,struct end_info,__uint64_t);
int grio_send_commands_to_daemon(int, grio_cmd_t *);
int grio_reserve_file_system(dev_t, grio_resv_t *);

/*
 * global defines
 */
#define	USEC_PER_SEC	1000000

/*
 * static library data
 */
static const cap_value_t cap_device_mgt = CAP_DEVICE_MGT;

/*
 * grio_query_fs_cmd
 */
int
grio_query_fs_cmd(dev_t fs_dev, grio_cmd_t *griocp)
{
	grio_resv_t	*griorp;
	struct end_info end1, end2;

	griorp = &(griocp->cmd_info.gr_resv);

	griorp->gr_flags |= PER_FILE_SYS_GUAR;

	if (grio_set_defaults_and_check_flags(griorp)) {
		griorp->gr_error = EIO;
		return ( -1 );
	}

	end1.gr_end_type = END_TYPE_REG;
	end1.gr_dev = fs_dev;
	end1.gr_ino = (ino_t)0;

	end2.gr_end_type = END_TYPE_NONE;
	end2.gr_dev = (dev_t)0;
	end2.gr_ino = (ino_t)0;

	/*
	 * create ggd daemon command
	 */
	grio_create_command_struct(griorp, griocp, GRIO_GET_BW, 0, 
			end1, end2, NULL);
	/*
	 * send command to ggd daemon
	 */
	if (grio_send_commands_to_daemon(1, griocp)) {
		griorp->gr_error = EIO;
		return( -1 );
	}

	grio_get_results( griorp, griocp, 1 );

	return(griorp->gr_error);
}

/*
 * grio_query_fs
 *	Return the bandwidth data for the given file system.
 *
 * RETURNS:
 *	0 if bandwidth info is returned. The output parameters bytes and
 *		seconds contain the bandwidth data
 *	non zero on error
 */
int
grio_query_fs( dev_t fs_dev, int *bytes, int *seconds )
{
	int		ret;
	grio_cmd_t	griocmd;

	bzero( &griocmd, sizeof(grio_cmd_t) ) ;
	
	/*
	 * ask for an impossible amount of bandwidth,
	 * return value will contain available bandwidth.
	 */
	griocmd.cmd_info.gr_resv.gr_start	= time(0);
	griocmd.cmd_info.gr_resv.gr_duration	= 100;
	griocmd.cmd_info.gr_resv.gr_optime	= USEC_PER_SEC;	
	griocmd.cmd_info.gr_resv.gr_opsize	= 64*1024;

	ret = grio_query_fs_cmd( fs_dev, &griocmd);

	if ( ( ret ) || (griocmd.cmd_info.gr_resv.gr_error == ENOSPC)) {
		*bytes		=  griocmd.cmd_info.gr_resv.gr_opsize;
		*seconds	=  griocmd.cmd_info.gr_resv.gr_optime/
					USEC_PER_SEC;
	} else {
		*bytes		=  griocmd.gr_bytes_bw;
		*seconds	=  griocmd.gr_usecs_bw/USEC_PER_SEC;
	}

	return( 0 );
}

/*
 * grio_check_flags
 *	This routine error checks the grio_resv_t gr_flags field 
 *	set by the user.
 * 	One flag bit, and only one flag bit must be set from each group 
 * 	of flags. This is not true for READ_GUAR & WRITE_GUAR. We do not
 *	check for these here.
 *
 *
 * RETURNS:
 *	0 on success
 *	EIO on failure
 */
int
grio_check_flags( grio_resv_t *griorp )
{
	int	flags 	= griorp->gr_flags;
	int 	count;

	count = 0;
	if (flags & PER_FILE_GUAR)      count++;
	if (flags & PER_FILE_SYS_GUAR)  count++;

	if ( count != 1 ) {
		return( EIO );
	}

	count = 0;
	if (flags & PROC_PRIVATE_GUAR)  count++;
	if (flags & PROC_SHARE_GUAR)    count++;

	if ( count != 1 ) {
		return( EIO );
	}

	count = 0;
	if (flags & SLIP_ROTOR_GUAR)  count++;
	if (flags & FIXED_ROTOR_GUAR) count++; 
	if (flags & NON_ROTOR_GUAR)   count++;

	if ( count != 1 ) {
		return( EIO );
	}

	/*
	 * check for unknown flags.
	 */
	if ( (flags & (~GUARANTEE_MASK) ) ) {
		return( EIO );
	}

	return( 0 );
}


/*
 * grio_set_defaults_and_check_flags()
 *	Set the default flag bits for those aspects of the 
 *	rate guarantee not specificed by the user
 *
 * 	The defaults are :
 *		PROC_SHARE_GUAR
 *		NON_ROTOR_GUAR
 *		(READ_GUAR | WRITE_GUAR)
 *
 *	Check that the set of flags is consistent and valid
 *
 * RETURNS:
 *	0 on success
 *	EIO on error
 */
int
grio_set_defaults_and_check_flags( grio_resv_t *griorp)
{
	int 	ret;

	if (!(griorp->gr_flags & PROC_PRIVATE_GUAR) ) {
		griorp->gr_flags |= PROC_SHARE_GUAR;
	}

	if (!(griorp->gr_flags & (FIXED_ROTOR_GUAR|SLIP_ROTOR_GUAR)) ) {
		griorp->gr_flags |= NON_ROTOR_GUAR;
	}

	/* If read/write guarantee type is not specified, grant read & write
	 * guarantee by default.
	 */
	if (!(griorp->gr_flags & (READ_GUAR|WRITE_GUAR))) {
		griorp->gr_flags |= (READ_GUAR|WRITE_GUAR);
	}

	/*
	 * check that all the flags are consistent
	 */
	ret = grio_check_flags( griorp );

	return ( ret );
}

/*
 * grio_create_command_struct
 *	This routine fills in the grio_cmd_t structure with the
 *	given arguments.
 *
 * RETURNS:
 *	always returns 0
 */
void
grio_create_command_struct( 
	grio_resv_t	*griorp, 
	grio_cmd_t	*griocp,
	int		cmd,
	int		subcmd,
	struct end_info	end1,
	struct end_info end2,
	__uint64_t	fp)
{

	/*
	 * Fill in structure with user supplied arguments.
 	 */
	griocp->gr_cmd				= cmd;
	griocp->gr_subcmd			= subcmd;

	griocp->gr_procid			= getpid();
	griocp->gr_fp				= fp;

	/* structure copies */
	griocp->one_end				= end1;
	griocp->other_end			= end2;

	griocp->cmd_info.gr_resv.gr_start	= griorp->gr_start;
	griocp->cmd_info.gr_resv.gr_duration	= griorp->gr_duration;
	griocp->cmd_info.gr_resv.gr_optime	= griorp->gr_optime;
	griocp->cmd_info.gr_resv.gr_opsize	= griorp->gr_opsize;

	COPY_STREAM_ID( griorp->gr_stream_id, 
				griocp->cmd_info.gr_resv.gr_stream_id);

	griocp->cmd_info.gr_resv.gr_flags	= griorp->gr_flags;
	griocp->cmd_info.gr_resv.gr_error	= 0;
}

/*
 * grio_get_results()
 *	This routine copies the results of the command from the griocp to 
 *	the user`s griorp structure.
 *
 *
 * RETURNS:
 *	always returns 0
 */
int
grio_get_results( 
	grio_resv_t	*griorp, 
	grio_cmd_t	*griocp,
	int		numlist)
{
	int i;

	for (i=0; i<numlist; i++) {

		griorp[i].gr_start = griocp[i].cmd_info.gr_resv.gr_start;
		griorp[i].gr_duration = griocp[i].cmd_info.gr_resv.gr_duration;

		/*
	 	* only fill in optime/opsize on reservation failure
	 	*/
		if ( griocp[i].cmd_info.gr_resv.gr_error == ENOSPC ) {
			griorp[i].gr_optime = griocp[i].cmd_info.gr_resv.gr_optime;
			griorp[i].gr_opsize = griocp[i].cmd_info.gr_resv.gr_opsize;
		}

		COPY_STREAM_ID( griocp[i].cmd_info.gr_resv.gr_stream_id, 
			griorp[i].gr_stream_id);

		griorp[i].gr_flags = griocp[i].cmd_info.gr_resv.gr_flags;
		griorp[i].gr_error = griocp[i].cmd_info.gr_resv.gr_error;

		bcopy( griocp[i].cmd_info.gr_resv.gr_errordev, 
			griorp[i].gr_errordev, DEV_NMLEN);

	}

	return( 0 );
}

/*
 * grio_associate_file
 *	Issue the system call which associates the given file indentified by
 *	the given file descriptor with the given stream id.
 *
 * RETURNS:
 *	0 on sucess
 *	-1 on error
 */
int
grio_associate_file( int fd, stream_id_t *stream_id)
{
	cap_t ocap;

	/*
	 * issue system call
	 */
	ocap = cap_acquire(1, &cap_device_mgt);
	if ( syssgi(	SGI_GRIO, 
			GRIO_ASSOCIATE_FILE_WITH_STREAM, 
			getpid(), 
			fd, 
			stream_id)) {
		cap_surrender(ocap);
		return( -1 );

	}
	cap_surrender(ocap);
	return( 0 );
}

/*
 * grio_dissociate_stream()
 *	Dissociate any file information associated with the stream id.
 *
 * RETURNS:
 *	0 on success
 *	-1 on error
 */
int
grio_dissociate_stream(stream_id_t *stream_id)
{
	cap_t ocap;

	/*
	 * issue system call
	 */
	ocap = cap_acquire(1, &cap_device_mgt);
	if ( syssgi(	SGI_GRIO,
			GRIO_DISSOCIATE_STREAM,
			getpid(),
			stream_id)) {
		cap_surrender(ocap);
		return(-1);
	}
	cap_surrender(ocap);
	return (0);
}

/*
 * grio_send_commands_to_daemon
 *	Issues the command list to the ggd daemon and waits for a response.
 *
 * RETURNS:
 *	0 on success
 *	non-zero on failure
 */
int
grio_send_commands_to_daemon(int num_cmds, grio_cmd_t *griocp)
{
	int ret;
	cap_t ocap;

	/*
	 * Send requests to ggd daemon.
	 */
	ocap = cap_acquire(1, &cap_device_mgt);
	if (ret = (int)syssgi(SGI_GRIO, GRIO_WRITE_GRIO_REQ, num_cmds, griocp)) {
		cap_surrender(ocap);
		return (EIO);
	}
	cap_surrender(ocap);
	return (ret);
}

/*
 * This routine preprocesses the reservation command list before sending
 * it down to the daemon.
 */
int
grio_preprocess_action_list(int numlist, grio_resv_t *griorlist,
	grio_cmd_t *grioclist, int prio)
{
	int		error = 0;
	int		listindex;
	grio_resv_t	*griorp;
	grio_cmd_t	*griocp;
	__uint64_t	fp;
	cap_t		ocap;
	struct end_info	end1, end2;
	uint_t		status;
	struct stat	statbuf;
	stream_id_t	stream_id;

	/* 
	 * Walk thru' list and validate the params for the list 
	 * of requests 
	 */
	for (listindex = 0; listindex < numlist; listindex ++) {

		griorp = &griorlist[listindex];

		/* prepare command struct for this reservation */
		griocp = &grioclist[listindex];
		bzero( griocp, sizeof(grio_cmd_t) );

		if (griorp->gr_action == GRIO_RESV_ACTION) {

			if (grio_set_defaults_and_check_flags(griorp)) {
				error = griorp->gr_error = EIO;
				continue;
			}

			if (griorp->gr_flags & PER_FILE_GUAR) {

				/* get dev_t (and inode number for XFS file)
				 * from user file descriptor.
				 */
				if (fstat( griorp->gr_fid, &statbuf)) {
					error = griorp->gr_error = EBADF;
					continue;
				}

				if ( S_ISREG(statbuf.st_mode) ) {

					end1.gr_end_type = END_TYPE_REG;
					end1.gr_dev = statbuf.st_dev;
					end1.gr_ino = statbuf.st_ino;

				} else if ( S_ISCHR(statbuf.st_mode) || S_ISBLK(statbuf.st_mode) ) {
					/* char or block special file */

					end1.gr_end_type = END_TYPE_SPECIAL;
					end1.gr_dev = statbuf.st_rdev;
					end1.gr_ino = (ino_t)0;

				} else {
					/* Neither a regular nor a device-special file */
					error = griorp->gr_error = EBADF;
					continue;
				}

				/* check if this pid/file pair already has a guarantee */
				uuid_create_nil(&stream_id, &status);
				ocap = cap_acquire(1, &cap_device_mgt);
				/* foll will set errno */
				if ( syssgi( SGI_GRIO,
					GRIO_GET_STREAM_ID,
					getpid(), griorp->gr_fid, &stream_id)
								 == 0) {
					cap_surrender(ocap);
					if (errno == EPERM)
						error = griorp->gr_error = EPERM;
					else {
						/* Special-case code for prio */
						if (!prio)
							error = griorp->gr_error = EBADF;
					}
				}
				/* Get the file pointer */
				if (syssgi( SGI_GRIO, GRIO_GET_FP, griorp->gr_fid,
						&fp) < 0) {
					cap_surrender(ocap);
					error = griorp->gr_error = EBADF;
					continue;
				}
				cap_surrender(ocap);


			} else if (griorp->gr_flags & PER_FILE_SYS_GUAR) {

				end1.gr_end_type = END_TYPE_REG;
				end1.gr_dev = griorp->gr_fsid;
				end1.gr_ino = (ino_t)0;

				/* file pointer is NULL for fs reservations */
				fp = 0;

			}

			/* The other end is always NULL for now */
			end2.gr_end_type = END_TYPE_NONE;
			end2.gr_dev = (dev_t)0;
			end2.gr_ino = (ino_t)0;

			grio_create_command_struct(griorp, griocp, 
					GRIO_RESV, 0, end1, end2, fp);
		} else {
			if (griorp->gr_action == GRIO_UNRESV_ACTION) {

				/* initialize the grio_cmd structure */
				griocp->gr_cmd = GRIO_UNRESV;
				griocp->gr_procid = getpid();
				COPY_STREAM_ID( (griorp->gr_stream_id),
					griocp->cmd_info.gr_resv.gr_stream_id);
			} else {
				/* invalid command. Set the error field to EINVAL */
				error = griorp->gr_error = EINVAL;
				continue;
			}
		}

	} /* of for loop */

	return (error);
}

/*
 * This is the main workhorse for grio_action_list.
 * To support special requirements of the prioSetBandwidth
 * command, an extra parameter "prio" is used.
 * prioSetBandwidth calls grio_action_list_sub directly with prio == 1.
 * By default, prio == 0 when called from grio_action_list.
 */
int
grio_action_list_sub(int numlist, grio_resv_t *griorlist, int prio)
{
	int		error = 0;
	grio_cmd_t	*grioclist=NULL;
	int		i;
	grio_cmd_t	grio1_cmd;

	/* Validate numlist field */
	if ( (numlist <= 0) || (griorlist == NULL)) {
		errno = EINVAL;
		return -1;
	}

	/* Allocate memory for the command structs */
	if (numlist == 1)
		grioclist = &grio1_cmd;
	else {
		grioclist = (grio_cmd_t *) malloc(sizeof(grio_cmd_t) 
							* numlist);
		if ( grioclist == NULL) {
			for (i = 0; i < numlist; i++)
				griorlist[i].gr_error = ENOMEM;
			errno = 0;
			return -1;
		}
	}

	error = grio_preprocess_action_list(numlist,
			griorlist, grioclist, prio);

	/* if any request is invalid, return */
	if (error) {
		if (numlist > 1) free (grioclist);
		/*
		 * Clear errno since we are only interested in EINVAL
		 * which has already been set for the validity check
		 * on numlist and griorlist, and other calls may set 
		 * errno.
	 	 */
		errno = 0;	
		return -1;
	}

	/* 
	 * Send list of requests to ggd and sleep on completion of
	 * the last request.
	 */
	if (grio_send_commands_to_daemon(numlist, grioclist)) {
		if (numlist > 1) free (grioclist);
		for (i = 0; i < numlist; i++)
			griorlist[i].gr_error = EIO;
		errno = 0;
		return -1;
	}

	/* 
	 * copy results of command execution to the user's griorp 
	 * structures 
	 */
	grio_get_results(griorlist, grioclist, numlist);

	/* Release memory for grioclist here */
	if (numlist > 1) free (grioclist);
	errno = 0;
	for (i = 0; i < numlist; i++)
		if (griorlist[i].gr_error != 0)
			return(-1);
	return 0;
}

/*
 * grio_action_list
 *	Try to obtain rate guarantees on the given action list. Since the
 *	requests are forwarded as a bunch to ggd, and ggd executes requests
 *	serially - these actions can be thought to be atomic in the sense
 *	that no request from another group can interfere with the requests
 *	in this group.
 *
 *	Algo.: Validate the parameters in the reservation list. Only if
 *	the parameters for all the reservation requests are valid, are the
 *	requests forwarded to the ggd daemon.
 *
 *	The user process sleeps on the completion of the last request in
 *	the list.
 *
 *	Note: Failure of one request in the list does not prevent 
 *	execution of subsequent ones once the commands have been sent to ggd.
 *
 * RETURNS:
 *	-1 on error:
 *		errno == EINVAL indicates invalid griorlist or numlist.
 *		gr_error could be one of the foll.
 *		a) ENOMEM - Memory allocation failure (for multiple subcmds)
 *		b) EIO 	  - kernel could not send the subcmds to ggd.
 *		c) EPERM  - user does not have CAP_DEVICE_MGMT perms.
 *		d) EBADF  - file unaccessible or has guarantee from process.
 *		e) other ggd returned errors.
 *	0 on success, which indicates all subcmds completed without errors.
 */
int grio_action_list(int numlist, grio_resv_t *griorlist)
{
	return (grio_action_list_sub(numlist, griorlist, 0));
}

/*
 * grio_reserve_file
 *      Try to obtain a rate guarantee on the given file.
 *
 * RETURNS:
 *      0 on sucess, the grio_resv_t structure contains the grio_stream_id
 *      -1 on failure
 */
int
grio_reserve_file(int fd, grio_resv_t *griorp)
{
	int ret = 0;

	if ((fd < 0) || (griorp == NULL)) {
		errno = EINVAL;
		return(-1);
	}
	griorp->gr_action = GRIO_RESV_ACTION;
	griorp->gr_flags |= PER_FILE_GUAR;
	griorp->gr_fid = fd;
	errno = griorp->gr_error = 0;

	ret = grio_action_list(1, griorp);
	/* if the command failed, return -1 */
	if (griorp->gr_error) {
		errno = griorp->gr_error;
		ret = -1;
	} else
		errno = 0;

	return (ret);
}

/*
 * grio_reserve_fs
 *
 *      Issue request to the ggd daemon to obtain a rate
 *      guarantee on the given file system.
 *
 *
 * RETURNS:
 *      0 on success, the grio_resv_t structure contains the grio_stream_id
 *      -1 on error
 */
int
grio_reserve_fs(dev_t fs_dev, grio_resv_t *griorp)
{
	int ret;

	if ((griorp == NULL)) {
		errno = EINVAL;
		return(-1);
	}
	griorp->gr_action = GRIO_RESV_ACTION;
	griorp->gr_flags |= PER_FILE_SYS_GUAR;
	griorp->gr_fsid = fs_dev;
	errno = griorp->gr_error = 0;

	ret = grio_action_list(1, griorp);

	/* if the command failed, return -1 */
	if (griorp->gr_error) {
		errno = griorp->gr_error;
		ret = -1;
	} else
		errno = 0;

	return (ret);
}

/*
 *  grio_unreserve_bw
 *      Remove the grio stream associated with the given grio_stream_id.
 *
 * RETURNS:
 *      0 on sucess
 *      -1 on failure
 */
int
grio_unreserve_bw( stream_id_t *grio_stream_id )
{
	int ret;
	grio_resv_t grior;

	if (grio_stream_id == NULL) {
		errno = EINVAL;
		return(-1);
	}
	grior.gr_action = GRIO_UNRESV_ACTION;
	COPY_STREAM_ID((*grio_stream_id),
				grior.gr_stream_id);
	errno = 0;
	ret = grio_action_list(1, &grior);

	/* if the command failed, return -1 */
	if (grior.gr_error) {
		errno = grior.gr_error;
		ret = -1;
	} else
		errno = 0;

	return (ret);
}

/* PRIO stubs for backward compatibility */
#include <sys/prio.h>

/*ARGSUSED*/
int
prioSetBandwidth(int fd, int mode, bandwidth_t bytesec,
			pid_t *lock_holder)
{
	struct stat	buf;
	stream_id_t	stream_id;
	uint_t		status;
	grio_resv_t	grior[2];
	ptrdiff_t	r;
	cap_t		ocap;
	int		error = 0;
	int		num_cmds;

	/* Validate the parameters */
	if ((mode != PRIO_READ_ALLOCATE) && (mode != PRIO_WRITE_ALLOCATE)
			&& (mode != PRIO_READWRITE_ALLOCATE)) {
		errno = EINVAL;
		return -1;
	}

	if (bytesec < 0) {
		errno = EINVAL;
		return -1;
	}

	if (fstat(fd, &buf) < 0) {
		errno = EINVAL;
		return -1;
	}

	/* Check if this pid/file pair already has a guarantee */
	uuid_create_nil(&stream_id, &status);
	ocap = cap_acquire(1, &cap_device_mgt);
	r = syssgi( SGI_GRIO, GRIO_GET_STREAM_ID, getpid(), fd, &stream_id);
	cap_surrender(ocap);

	num_cmds = 0;

	if (r == 0) {
		/* Reservation already exists. We need to change
		 * it to the currently requested value. We do this
		 * by enqueuing 2 grio commands: one to unreserve
		 * the existing bandwidth and the second to set it
		 * to the currently requested value.
		 * Since these 2 commands are executed atomically,
		 * this should give us the effect we desire.
		 */

		/* First prepare the grio_unreserve command */
		grior[0].gr_action	= GRIO_UNRESV_ACTION;
		COPY_STREAM_ID(stream_id, grior[0].gr_stream_id);

		num_cmds ++;

		if (bytesec) {
			/* Now, prepare the reserve command */
			grior[1].gr_action		= GRIO_RESV_ACTION;
			grior[1].gr_start		= time(0);
			grior[1].gr_duration		= GRIO_RESV_DURATION_INFINITE;
			grior[1].gr_optime		= USEC_PER_SEC;	/* prio requests are bytes/sec */
			grior[1].gr_opsize		= bytesec;
			grior[1].gr_flags		|= PER_FILE_GUAR;
			grior[1].gr_object_u.gr_fd	= fd;
			grior[1].gr_error		= 0;

			num_cmds ++;
		}

		/* initialize errno */
		errno = grior[0].gr_error = grior[1].gr_error = 0;
	
		error = grio_action_list_sub(num_cmds, grior, 1);

	} else {
		/* New reservation. Fill up the gr_resv structure
		 * and send it to grio_reserve_file.
		 */
		grior[0].gr_action 	= GRIO_RESV_ACTION;
		grior[0].gr_start	= time(0);
		grior[0].gr_duration	= GRIO_RESV_DURATION_INFINITE;
		grior[0].gr_optime	= USEC_PER_SEC;	/* prio requests are bytes/sec */
		grior[0].gr_opsize	= bytesec;
		grior[0].gr_flags	|= PER_FILE_GUAR;
		grior[0].gr_object_u.gr_fd	= fd;

		num_cmds ++;

		/* initialize errno */
		errno = grior[0].gr_error = 0;
	
		error = grio_action_list(num_cmds, grior);
	}


	if (!error) {
		errno = grior[num_cmds - 1].gr_error;
		if (!grior[num_cmds - 1].gr_error) {
			if (bytesec) {
				if (grio_associate_file(fd,
					&grior[num_cmds - 1].gr_stream_id)) {
					errno = EINVAL;
					return -1;
				}
			}
		} else {
			return -1;
		}
	} else {
		if (!errno)
			errno = grior[num_cmds - 1].gr_error;
		return -1;
	}
	return 0;
}

/*ARGSUSED*/
int
prioGetBandwidth(int fd, bandwidth_t *read_bytesec,
			bandwidth_t *write_bytesec)
{
	struct stat	buf;
	struct statvfs	statbuf;
	stream_id_t	stream_id;
	ptrdiff_t	r;
	cap_t		ocap;
	grio_cmd_t	griocmd;
	uint_t		status;

	if ((read_bytesec == NULL) && (write_bytesec == NULL)) {
		errno = EINVAL;
		return -1;
	}

	if (fstat(fd, &buf) < 0) {
		errno = EBADF;
		return -1;
	}

	/* only xfs file system supports prio */
	if (S_ISREG(buf.st_mode)) {
		fstatvfs(fd, &statbuf);
		if (strcasecmp(statbuf.f_basetype, "xfs")) {
			errno = EOPNOTSUPP;
			return -1;
		}
	}

	/* Check if this pid/file pair already has a guarantee */
	uuid_create_nil(&stream_id, &status);
	ocap = cap_acquire(1, &cap_device_mgt);
	r = syssgi( SGI_GRIO, GRIO_GET_STREAM_ID, getpid(), fd, &stream_id);
	cap_surrender(ocap);

	if (r == 0) {
		/* Reservation already exists, get the bandwidth
		 * allocation info. 
		 */

		/* Create the grio command struct */
		griocmd.gr_cmd = GRIO_GET_FILE_RESVD_BW;
		griocmd.gr_procid = getpid();
		COPY_STREAM_ID( stream_id,
				griocmd.cmd_info.gr_resv.gr_stream_id);

		/*
		 * send command to ggd daemon
		 */
		if (grio_send_commands_to_daemon(1, &griocmd)) {
			errno = EIO;
			return -1;
		}

		/* The griocmd structure contains the info. we seek */
		if (read_bytesec) {
			*read_bytesec = griocmd.gr_bytes_bw;
		}

		if (write_bytesec) {
			*write_bytesec = griocmd.gr_bytes_bw;
		}

		return 0;

	} else {
		*read_bytesec = 0;
		*write_bytesec = 0;

		return 0;
	}
}

/*
 * prioLock() and prioUnlock() are provided only for backwards
 * compatibility for prio users. No actual locking is performed.
 * Users are expected to use the grio_action_list API for atomic
 * reservations.
 */
/*ARGSUSED*/
int
prioLock(pid_t *lock_holder)
{
	return 0;
}

int
prioUnlock()
{
	return 0;
}
