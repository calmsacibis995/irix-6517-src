#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/test/ggd_tst/RCS/ggd_tst.c,v 1.22 1997/03/24 18:38:29 kanoj Exp $"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <grio.h>
#include <sys/time.h>
#include <sys/ktime.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <bstring.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/bsd_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_itable.h>
#include <sys/lock.h>
#include <sys/uuid.h>
#include <sys/times.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>

#include <limits.h>
#include <sys/prctl.h>
#include <sys/schedctl.h>
#include <sys/syssgi.h>

extern int errno;
int 	linenum = 0;
int	timeeachrequest = 0;
int	printid = 0;
int	verbose = 0;
int	usestreamid = 0;
stream_id_t	stream_id;

char *emsg[50]=
	{"NULLERROR",
	 "EPERM",
	 "ENOENT",
	 "ESRCH",
	 "EINTR",
	 "EIO",
	 "ENXIO",
	 "E2BIG",
	 "ENOEXEC",
	 "EBADF",
	 "ECHILD",
	 "EAGAIN",
	 "ENOMEM",
	 "EACCES",
	 "EFAULT",
	 "ENOTBLK",
	 "EBUSY",
	 "EEXIST",
	 "EXDEV",
	 "ENODEV",
	 "ENOTDIR"};

#define MAX_TICK_COUNT		10001

	

/*
 *	Input File Format:
 *
 * 	O "filename" starttime duration optime opsize DI/BUF ID#
 *	   This opens file "filename".
 *	   The reservation will start at time now + starttime and
 *	   last for duration. (startime and duration are in seconds)
 *	   The reservation will be for opsize bytes in optime seconds.
 *	   DI means that direct I/O will be performed, FS means file system
 *	   I/O will be performed. 
 *	   ID# is an integer which will be used in subsequent T, U, and C 
 *	   commands to identify this file. 
 *
 *	F "filename" DI/BUF ID#
 *	   This opens file "filename" without asking for rate guarantee.
 *
 *	C  ID#
 *	   This close the file previously opened and identified with ID#.
 *
 *	B  ID#
 *	   This causes the process to begin a timed interval for file ID#.
 *	   The interval will continue until until an "E" command is 
 *	   encountered. This is used to test it rate guarantee can be
 *	   met using multiple I/O request.
 *
 *	E  ID#
 *	   This causes the process to end a timed interval for file ID#.
 *	   If the rate guarantee is not met, a message will be printed.
 *
 *`	W  ID# untiltime
 *	   This causes the process to wait until untiltime seconds have
 *	   passed since the last I/O op was performed on file with ID#.
 *
 *
 * 	P "filename" extentsize totalsize DI/BU RT/NRT ID#
 *	   This command will preallocate a file with the given extent
 *	   size and total size.
 *
 *
 *	X 
 *	  This command will fork a child process.
 *
 *	#  comment line
 *	
 *	L  ID#  R size 1
 * 		loop reading file in increments of size until eof
 *
 */


int convert_uuid_to_string( uuid_t *uuid, char **newstring);
int start_new_file(void);
int do_exec_op(void);
int compare_buffer_pattern(char *, int , int );
int close_this_file(void);
int begin_time_op(void);
int end_time_op(void);
int prealloc_this_file(void);
int is_direct_io(char *);
int wait_for_guarantee(char *);
int is_realtime_io(char *);
void usage(void);
int do_delay(void);
int do_read_loop(void);
int is_vod_str(char *);
int is_noguar_str(char *);
int do_untime_ioop(void);
int convert_string_to_uuid( char *, uuid_t *);

int parse_open_line(char*,int*,char**,int*,int*,int*,int*,int*);
int pass_guarantee_to_new_file(void);

typedef struct file_info {
	char	name[36];
	int 	optsize;
	int	optime;
	int	fd;
	clock_t lastreq;
	clock_t begintime;
	long long numbytes;
	int	waitforguarantee;
	int	gotguarantee;
	int 	vod;
	uuid_t	uuid;
} file_info_t;

#define FILE_FD( id ) 		file_info[id].fd
#define FILE_IOTIME( id ) 	file_info[id].optime
#define FILE_IOSIZE( id ) 	file_info[id].optsize
#define FILE_STREAM_ID( id ) 	file_info[id].uuid

#define	TIME			timeeachrequest

/*
 * global data
 */
file_info_t file_info[256];
time_t now;
char line[160];



#define NUM_OPT_IOS( wholesize , optiosize )                 \
         ((wholesize / optiosize) +    \
                        (wholesize % optiosize ? 1 : 0 ))


/*
 * grio_remove_request()
 *      The call removes a file rate guarantee. The griorp parameter is
 *  not needed it is only for backward compatibility.
 *
 * RETURNS:
 *      0 on success
 *      -1 on error
 */
int
grio_remove_request( int fd, grio_resv_t *griorp)
{
        int             ret = 0;
        uint_t          status;
        stream_id_t     stream_id;


        /*
         * get the stream id associated with this process/file pair
         */
        uuid_create_nil( &stream_id, &status );
        if (    syssgi( SGI_GRIO, GRIO_GET_STREAM_ID,
                        getpid(), fd, &stream_id) ) {
                griorp->gr_error = EIO;
                return( -1 );

        }

        /*
         * remove the stream associated with the given stream id
         */
        ret = grio_unreserve_bw( &stream_id );

        if ( ret ) {
                griorp->gr_error = EIO;
        }

        return( ret );
}

/*
 * grio_pass_bandwidth()
 *
 *
 * RETURNS:
 *      -1 on failure
 *      open file descriptor to "newfile" on success
 */
int
grio_pass_bandwidth( int oldfd, char *newfile, int flags)
{
        int             newfd;
        uint_t          status;
        stream_id_t     stream_id;


        /*
         * check if this pid/file pair really has a guarantee
         */
        uuid_create_nil( &stream_id, &status);
        if (    syssgi( SGI_GRIO, GRIO_GET_STREAM_ID,
                        getpid(), oldfd, &stream_id ) ) {
                return( -1 );
        }

        /*
         * open the new file first
         */
        if ( (newfd = open( newfile, flags))  == -1 ) {
                return( newfd );
        }

        if ( grio_associate_file( newfd, &stream_id) ) {
                close( newfd );
                return( -1 );
        }

        close( oldfd );

        return( newfd );
}



int
check_fs_limits(
	dev_t	fs_dev)
{
	int	bytes = 0, seconds = 0, ret;

	ret = grio_query_fs( fs_dev, 0, &bytes, &seconds);

	if (ret ) {
		printf("grio_query_file_system failed \n");
	} else {
		printf("remaining bandwidth on fs 0x%x : %d/%d \n",
			fs_dev, bytes, seconds);
	}
	return( 0 );
}



int
send_grio_request(
	int	fd, 	
	time_t	start, 
	int	duration, 
	int 	optime, 
	int	opsize, 
	int	flags, 
	int	*error, 
	int	id)
{
	int 		ret;
	char 		*str, *uuidstr;
	struct	stat	statbuf;
	grio_resv_t 	grio;

	bzero( &grio, sizeof(grio_resv_t) );

	grio.gr_start  		= start;
	grio.gr_duration	= duration;
	grio.gr_optime 		= optime; 
	grio.gr_opsize 		= opsize;

	grio.gr_flags  		=  flags; 

	if ( fstat( fd, &statbuf) ) {
		printf("fstat in send_grio_request() failed \n");
		exit( -1 );
	}
	
	if ( usestreamid ) {
			convert_uuid_to_string( &stream_id, &str);
			ret = grio_associate_file(fd, &stream_id);
			if (ret == 0 ) {
printf("stream id [%s] was passed to new file \n",str);
				bcopy( &stream_id, &FILE_STREAM_ID(id), 16 );
			} else {
printf("stream id [%x] was NOT passed to new file, ret = %d errno = %d \n",str,ret,errno);
			}
	} else {
		if (flags & PER_FILE_GUAR) {
printf("getting per guar \n");
			ret = grio_reserve_file( fd, &grio);
		} else if (flags & PER_FILE_SYS_GUAR) {
printf("getting per fs guar \n");
			ret = grio_reserve_fs( statbuf.st_dev, &grio);
		} else {
			printf("UNKNOWN TYPE OF RESERVATION \n");
			exit( -1 );
		}
		*error = ret;

		if (ret) {
			printf("grio reserve request FAILED: %s (%d):\n",
					emsg[grio.gr_error], grio.gr_error);
			if (grio.gr_error == ENOSPC) {
				printf("Out of bandwidth on device [%s].\n",
					grio.gr_errordev);
			printf("Bandwidth avail is %d bytes per %d usecs\n",
					grio.gr_opsize, grio.gr_opsize);
			} else if (grio.gr_error == EINVAL) {
				printf("Could not communicate to daemon.\n");
			} else if (grio.gr_error == ESRCH) {
				printf("Invalid process id.\n");
			} else if (grio.gr_error == EBADF) {
				printf("Could not stat file or file already \n");
			} else if (grio.gr_error == EIO) {
				printf("Error in grio_resv_t structure \n");
			} else if (grio.gr_error == EACCES) {
				printf("Could not get (flags) 0x%x guarantee.\n",
					flags);
			} else if (grio.gr_error == ENOENT) {
				printf("No file extents \n");
			} else {
				printf("UNKNOW ERROR CONDITION \n");
				exit( -1 );
			}
			return(-1);
		} else {
			bcopy( &grio.gr_stream_id, &FILE_STREAM_ID(id), 16 );


			check_fs_limits( statbuf.st_dev );

			if (printid ) {
				convert_uuid_to_string( &FILE_STREAM_ID(id), &uuidstr);
				fprintf(stderr,"STREAMID IS [%s] \n",uuidstr);
			}

			ret = grio_associate_file(fd, &FILE_STREAM_ID(id));
			if (ret) {
				printf("associate file with stream FAILED \n");
			}

			ret = 1;
		} 

	}
	return( ret );
}


void
send_grio_unreserve_request(
	int 	id)
{
	int		ret;
	grio_resv_t	grior;


	ret = grio_remove_request( FILE_FD(id), &grior );

	if (ret) {
		printf("grio_remove_request FAILED %d, %d\n",
			ret, errno);
	} 
}


void
send_grio_unreserve_request_old(int id )
{
	int ret;

	ret = grio_unreserve_bw( &FILE_STREAM_ID( id ) );

	if (ret) {
		printf("grio_unreserve_bandwidth FAILED %d, %d \n",
			ret, errno);
	} 
}



struct timeval firstcmdtime;
int no_read_check = 0;

main( int argc, char *argv[])
{
	char 	cmd, *str;
	int 	c, cpunum; 
	int 	ret;
	extern char *optarg;

	if (plock(PROCLOCK)) {
		printf("Could not lock ggd_tst process. \n");
		exit(-2);
	}
	schedctl(NDPRI, 0, NDPHIMAX);

	while ((c = getopt(argc, argv, "ivtd:c:s:")) != EOF) {
		switch (c) {
			case 'd':
				switch ( *optarg ) {
					case 'n':
						no_read_check = 1;
						break;
					default:
						printf("Bad option %c \n",
								*optarg);
						break;
				}
				break;
			case 'c':
				cpunum = atoi(optarg);
				if (sysmp(MP_MUSTRUN, cpunum) == -1) {
					printf("Mustrun on %d failed\n",cpunum);
				}
				break;
			case 't':
				timeeachrequest = 1;
				break;
			case 'i':
				printid = 1;
				break;
			case 'v':
				verbose = 1;
				break;
			case  's':
				usestreamid = 1;
				str = strdup(optarg);
				break;
			default:
				printf("Bad argument %c \n",c);
				break;
		}
	}

	
	if ( usestreamid ) {
		convert_string_to_uuid( str, &stream_id );
		free( str );
	}

	now = time(0);

	ret = 0;
	while (fgets( line, sizeof(line), stdin)) {
		linenum++;
		sscanf(line,"%s",&cmd);
		if (cmd == '#') {
			/*
			 * Commnet line.
			 */
			;
		} else if (cmd == 'C') {
			ret = close_this_file();
		} else if (cmd == 'O') {
			ret = start_new_file();
		} else if (cmd == 'B') {
			ret = begin_time_op();
		} else if (cmd == 'E') {
			ret = end_time_op();
		} else if (cmd == 'P') {
			ret = prealloc_this_file();
		} else if (cmd == 'X') {
			ret = do_exec_op();
		} else if (cmd == 'L') {
			ret = do_read_loop();
		} else if (cmd == 'D') {
			ret = do_delay();
		} else if (cmd == 'U') {
			ret = do_untime_ioop();
		} else if (cmd == 'G') {
			ret = pass_guarantee_to_new_file();
		} else {
			printf("UNKNOWN CMD [%s] \n",&cmd);
			ret = -1;
		}
		if (ret) {
			usage();
			exit(-1);
		}
	}
	return( 0 );
}

int
begin_time_op( void )
{
	char cmd[5];
	int id;
	struct tms ttms;

	sscanf(line,"%s %d", &cmd, &id);

	if (!FILE_FD(id)) {
		printf("invalid id %d to begin time on \n",id);
		return(-1);
	}
	file_info[id].begintime = times(&ttms);
	file_info[id].numbytes  = 0;
	file_info[id].lastreq  = 0;
	return(0);
}

int
end_time_op( void )
{
	char 		cmd[5];
	int 		id, ratetime, bps;
	long long 	numbytes;
	int 		seconds;
	int 		grated = 0, numops;
	clock_t		t_now;
	struct 	tms 	ttms;

	sscanf(line,"%s %d", cmd, &id);

	if (!FILE_FD(id)) {
		printf("invalid id %d to end time on \n",id);
		return(-1);
	}

	t_now = times(&ttms);

	numbytes	= file_info[id].numbytes;
	numops		= NUM_OPT_IOS(numbytes, (FILE_IOSIZE(id)));
	ratetime	= FILE_IOTIME(id) * numops;
	seconds		= (t_now - file_info[id].begintime) / HZ;
		

	if (FILE_IOSIZE(id) != 1) {
		grated = 1;
	}

	if (grated) {
		if ( seconds > ratetime) {
			printf("Guarantee FAILED on ID %d. ",id);
			bps = FILE_IOSIZE(id) / FILE_IOTIME(id);
			printf("   Guaranteed rate: %d bps.\n",bps);
			bps = numbytes/seconds;
			printf("   Actual rate: %d bps.\n",bps);
			printf("   Time difference is %d seconds.\n", 
				seconds - ratetime);
		} else {
			bps = numbytes/seconds;
			printf("Guarantee Met %d bps on ID %d. ", bps,id);
		}
		printf("start (%d), end (%d), ",
			file_info[id].begintime, t_now);
		printf("rate = %d \n\n",ratetime);
	} else {
		bps = numbytes/seconds;
		fprintf(stderr," end non guaranteed stream \n");
		fprintf(stderr,"   I/O rate: %d bps.\n",bps);
	}
	return(0);
}

int
wait_op( void )
{
	char cmd[5];
	int id, untiltime;

printf("WAIT OP FAILED \n");
exit(-1);
	sscanf(line,"%s %d %d", &cmd, &id, &untiltime);
	
	if (!FILE_FD(id)) {
		printf("unknown file id to wait on %d \n",id);
		return( -1 );
	}
	
	return( 0 );
}

int
close_this_file( void )
{
	char 	cmd[5];
	int 	ret, id;

	/*
 	 * Remove guarantee l
	 */
	sscanf(line,"%s %d", &cmd,&id);

	if (!FILE_FD(id)) {
		ret = -1;
		printf("can't close unopened file id %d \n",id);
	} else {
		ret = 0;
		if (file_info[id].gotguarantee == 1 ) {
			send_grio_unreserve_request( id );
		} else if (file_info[id].gotguarantee == -1) {
printf("there is no guarantee on file id %d \n", id);
			;
		} else {
printf("close_this_file error %d \n",file_info[id].gotguarantee);
		}

		close(FILE_FD(id));
		file_info[id].fd = 0;
		file_info[id].optime = 0;
		file_info[id].optsize = 0;
	}
	return( ret );
}

int
start_new_file( void )
{
	char 		*filename;
	int  		start, duration, optime, opsize, id;
	int  		flags, fd, error;
	int		grioflags;
	struct stat	statbuf;


	flags = O_SYNC | O_DIRECT | O_RDWR;

	parse_open_line( line, &id, &filename, &start, 
			&duration, &optime, &opsize, &grioflags);

	if (stat( filename, &statbuf)) {
		flags |=  O_RDWR | O_CREAT;
	}

	if (FILE_FD(id)) {
		printf("id %d already used \n",id);
		return(-1);
	}
	if ((fd = open( filename, flags)) == -1)  {
		printf(" open of %s failed \n",filename);
	} else {
		file_info[id].gotguarantee = 
				send_grio_request( fd, 
					now + start,
					duration, 
					optime, 
					opsize, 
					grioflags,  
					&error, 
					id);
		if (file_info[id].gotguarantee == -1) {
			printf("FAILED: could not get guarantee.\n");
			exit( -1 );
		} else {
			;
			/*
			 * printf("Got rate guarantee \n");
			 */
		}
	}

	file_info[id].fd = fd;
	strcpy(file_info[id].name, filename );
	file_info[id].optime = optime;

	file_info[id].optsize = opsize;

	if ((grioflags & FIXED_ROTOR_GUAR) ||  
	    (grioflags & SLIP_ROTOR_GUAR) )
		file_info[id].vod = 1;
	else
		file_info[id].vod = 0;

	return(0);
}

int
pass_guarantee_to_new_file( void )
{
	char 		filename[55], cmd[5];
	int		oldid, newid, flags;


	flags = O_SYNC | O_DIRECT | O_RDWR;

	/*
 	 * Start new guarantee line.
	 */
	sscanf(line,"%s %d %d %s",
	   cmd, &oldid, &newid, filename);

printf("oldid = %d, newid = %d \n",oldid,newid);
printf("cmd [%s], filename[%s] \n",cmd,filename);

	if ( FILE_FD(newid) ) {
		printf("id %d already used \n",newid);
		return(-1);
	}


	FILE_FD(newid) = grio_pass_bandwidth( FILE_FD(oldid), filename, flags );

	if (FILE_FD(newid) == -1 ) {
		printf("grio_pass_bandwidth FAILED \n");
		return( -1 );
	}

	/*
	 * setup new id
	 */
	file_info[newid].gotguarantee =  1;
	strcpy(file_info[newid].name, filename );
	file_info[newid].optime = file_info[oldid].optime;
	file_info[newid].optsize = file_info[oldid].optsize;
	file_info[newid].vod = file_info[oldid].vod;

	return(0);
}

int
prealloc_this_file( void )
{
	char 		dirtstr[5], realtimestr[5];
	char 		cmd[5], filename[55];
	int  		extentsize, totalsize, id;
	int  		flags, fd, ret;
	struct stat	statbuf;
	struct flock	flock;
	struct fsxattr	fsxattr;

	/*
 	 * Start new guarantee line.
	 */
	sscanf(line,"%s %d %s %d %d %s %s",
	   cmd,&id, filename,&extentsize,&totalsize,dirtstr,realtimestr);

	flags = O_SYNC;

	if (is_direct_io( dirtstr )) {
		flags |=  O_DIRECT;
	}

	flags |=  O_RDWR;
	if (stat( filename, &statbuf)) {
		flags |=  O_RDWR | O_CREAT;
	}

	if (FILE_FD(id)) {
		printf("id %d already used \n",id);
		return(-1);
	}
	if ((fd = open( filename, flags)) == -1)  {
		printf(" open of %s failed \n",filename);
	} 

	ret = fcntl(fd, F_FSGETXATTR, &fsxattr);
	if (ret) {
		printf("F_FSGETXATTR FAILED %d,%d \n",
			ret,errno);
		exit( -1 );
	}

	if ( ! (fsxattr.fsx_xflags & XFS_XFLAG_REALTIME) ) {
		if (is_realtime_io(realtimestr)) {
			fsxattr.fsx_xflags |= XFS_XFLAG_REALTIME;
		} 

		ret = fcntl(fd, F_FSSETXATTR, &fsxattr);

		if (ret) {
			printf("F_FSSETXATTR (set rt bit) FAILED %d,%d \n",
				ret, errno);
			exit( -1 );
		}
	} else {
		fsxattr.fsx_xflags |= XFS_XFLAG_REALTIME;
	}


	fsxattr.fsx_extsize  = extentsize;
	ret = fcntl(fd, F_FSSETXATTR, &fsxattr);
	if (ret) {
		printf("F_FSSETXATTR (set extent size) FAILED %d,%d \n",
			ret,errno);
		exit( -1 );
	}



	/*
	 * preallocate file space
	 */
	if (totalsize != 0) {
		flock.l_whence = 0;
		flock.l_len = totalsize;
		flock.l_start = (off_t)0;
		ret = fcntl(fd, F_RESVSP, &flock);
		if ( ret ) {
			printf("F_RESVP FAILED %d,%d \n",
				ret, errno);
			exit( -1 );
		}
	}



	file_info[id].fd = 0;
	file_info[id].optime = 0;
	file_info[id].optsize = 0;
	close(fd);

	return(0);
}

int
display_elapsed_times( 
int	elapsed_times[], 
int	totalcmds, 
int	totalio, 
int	totaltime,
int	optiosize,
int	optiotime)
{
	int	i;
	int	expectedio, optioticks;

	
	printf("Total time for %d bytes of I/O was %d ticks, cmd %d \n",
		totalio, totaltime, totalcmds);

	if ( totaltime == 0 )  {
		return( 0 );
	}

	optioticks = optiotime / 10000;

	printf("Reservation was for %d bytes of I/O every %d ticks \n",
		optiosize, optioticks);

	expectedio = (totaltime/optioticks)*optiosize;

	if (expectedio <= totalio) {
		printf("\tGuaranteed SUCCEEDED. \n");
	} else {
		printf("\tGuaranteed FAILED. \n");
	}

	printf("Expected %d bytes of I/O in %d ticks \n",
		expectedio, totaltime);
	printf("Achieved %d bytes of I/O in %d ticks \n",
		totalio, totaltime);
	printf("Average rate of %d bytes of I/O in %d ticks \n",
		(totalio/totaltime)*optioticks,optioticks);


	/*
	 * convert ticks to milliseconds by multiplying by 10
	 *
 	 */
	for (i = 0; i < 10000; i++) {
		if (elapsed_times[i]) {
			printf("# %d ms %7d ios",
				(i+1)*10,elapsed_times[i]);
			printf("\n");
		}
	} 
	printf(">100 sec %7d ios\n",elapsed_times[10000]);

	return( 0 );
}

int
do_read_loop( void )
{
	char 	rw, cmd[5], *buffer;
	int  	id, thissize, ret, pattern;
	int	readeof = 0, totalcmd = 0;
	int	totalsize = 0;
	int	optioticks, nextcycle;
	struct	tms ttms;
	clock_t	start_time, end_time, start, end, timediff;
	int	elapsed_times[MAX_TICK_COUNT];

	bzero(elapsed_times, sizeof(int) * MAX_TICK_COUNT);

	/*
 	 * Perform untimed read on open file.
	 */
	sscanf(line,"%s %d %s %d %d %d",
			&cmd,
			&id,
			&rw,
			&thissize, 
			&pattern);

	if (FILE_FD(id) == 0) {
		printf("invalid file id %d \n",id);
		return( -1);
	}

	optioticks =  FILE_IOTIME(id) / 10000;

	if (rw != 'S') {
		buffer = memalign( 4096, thissize);
		if ( mpin( buffer, thissize) != 0 ) {
			printf("mpin failed. errno = %d \n",errno);
		}
		bzero(buffer, thissize);
	}

	ret = 1;
	start_time = 0;
	while ( !readeof ) {

		start = times(&ttms);
		if ( (ret = (int)read(FILE_FD(id),buffer,thissize)) != (thissize)) {
			if (ret == -1)  {
				printf("READ RETURNED -1 ERROR %d \n",errno);
			}

			if (ret != 0) {
				printf("EOF RETURN %d SHORT READ \n",ret);
			}

			readeof = 1;
		}

		file_info[id].numbytes  += ret;

		/*
		 * start time at most recent boundary		
		 * NOTE:
		 * 	this may not always be correct !
		 * 	how do i get real start/slot time
		 */
		if ( start_time == 0 ) {
			if ( file_info[id].vod ) {
				start_time = (times(&ttms) / optioticks ) * 
						optioticks;
			} else {
				start_time = start;
			}

		} 

		totalsize += ret;
		totalcmd++;

		end = times(&ttms);

		timediff = (end - start);
		if ( timediff > 10000 )  {
			(elapsed_times[10000])++;
		} else {
			(elapsed_times[timediff])++;
		}

		if (timediff > 200 ) {
			printf("LONG timediff io num %d \n",totalcmd);
		}


		if (file_info[id].gotguarantee == 1) {

			if ( ((end - start_time) / optioticks) > totalcmd ) {
				printf("time = %d, totalcmd = %d, optioticks = %d \n",
					end - start_time, totalcmd, optioticks);
					printf("RATE FAILED - REQ TOO LONG \n");
			}
			if ( verbose ) {
				printf("end 0x%x start 0x%x diff = %d, optio %d, totalcmd %d \n",
					end, start_time,  
					end - start_time, 
					optioticks, totalcmd);
			}
			

			nextcycle = end % optioticks;
			if (nextcycle)
				nextcycle = optioticks - nextcycle;

#ifdef NOTNOW
			if ( nextcycle > 6 ) {
				if ( (nextcycle - 5 ) > 100 ) {
					printf("napping for %d \n",
						nextcycle - 5 );
				}
				sginap( nextcycle -5 );
			} 
#endif
		}
	}
	end_time = times(&ttms);

	display_elapsed_times( elapsed_times, 
			totalcmd, totalsize, 
			end_time - start_time,
			FILE_IOSIZE(id), FILE_IOTIME(id) );

	return(0);
}

int
fill_buffer_pattern(char *buf, int pattern, int bufsize)
{
	char c;
	int i;

	if (pattern == 0) {
		c = 0;
	} else if (pattern == 1) {
		c = 'a';
	} else if (pattern == 2) {
		c = 'b';
	} else if (pattern == 3) {
		c = 'c';
	} else if (pattern == 4) {
		c = 'd';
	} else {
		printf("unknown pattern %d \n",pattern);
		return(-1);
	}

	for ( i = 0; i < bufsize; i++) 
		buf[i] = c;

	return(0);
}

int
compare_buffer_pattern(char *buf, int pattern, int bufsize)
{
	char c;
	int i, failcount;

	if (pattern == 0) {
		return (0);
	} else if (pattern == 1) {
		c = 'a';
	} else if (pattern == 2) {
		c = 'b';
	} else if (pattern == 3) {
		c = 'c';
	} else if (pattern == 4) {
		c = 'd';
	} else {
		printf("unknown pattern %d \n",pattern);
		return(-1);
	}

	for ( i = 0; i < bufsize; i++)  {
		if (buf[i] != c) {
			printf("linenum %d \n",linenum);
			printf("FAILED: miscompare at loc %d, pid = %d \n",i,getpid());
			printf("Got [%d] expected [%d] \n",buf[i],c);
			failcount = 0;
			while ( (buf[i] != c) && (i < bufsize) ) {
				i++;
				failcount++;
			}
			printf("%d bytes miscompared \n",failcount);
			return( 1 );
		}
	}
	return(0);
}

void
usage(void)
{
	printf("usage message \n");
}

int
is_direct_io(char *str)
{
	int	ret;

	if (strcmp(str,"DI") == 0) {
		ret = 1;
	} else if (strcmp(str, "BUF") == 0) {
		ret = 0;
	} else {
		ret =  0;
	}
	return( ret );
}

int
is_realtime_io(char *str)
{
	int	ret;

	if (strcmp(str,"RT") == 0) {
		ret = 1;
	} else if (strcmp(str, "NRT") == 0) {
		ret = 0;
	} else {
		printf("Invalid RT I/O type [%s].\n",str);
		exit(-1);
	}
	return( ret );
}

int
is_vod_str(char *str)
{
	int 	ret;

	if (strncmp(str,"VOD",3) == 0) {
		ret = 1;
	} else if (strncmp(str, "NOVOD", 5) == 0) {
		ret = 0;
	} else {
		ret = 0;
	}
	return( ret );
}

int
is_noguar_str(char *str)
{
	int	ret;

	if (strncmp(str,"NOGUAR",6) == 0) {
		ret = 1;
	} else {
		ret = 0;
	}
	return( ret );
}

int
wait_for_guarantee(char *str )
{
	int	ret;

	if (strcmp(str, "WAIT") == 0) {
		ret = 1;
	} else if  (strcmp(str, "NO") == 0) {
		ret = 0;
	} else {
		printf("Invalid WAIT/NO type [%s].\n",str);
		exit(-1);
	}
	return( ret );
}

int
do_exec_op(void)
{
	char cmd[5];
	pid_t	pid;
	int	stat,id, fd;

	/*
 	 * Perform timed read/write on open file.
	 */
	sscanf(line,"%s %d", &cmd, &id);

	if (pid = fork()) {
/* 		wait(&stat); */
		waitpid(pid, &stat, 0);
		printf("child completed stat = %x\n",stat);
		exit(0);
	} else {
		close( FILE_FD(id));
		fd = open(file_info[id].name, O_RDWR | O_DIRECT);
		FILE_FD(id) = fd;
	}

	return( 0 );

}

int
do_delay( void )
{
	sleep( 2 );
	return( 0 );
}

int
do_untime_ioop( void )
{
	char 		rw;
	char 		cmd[5];
	char 		*buffer;
	int  		id, thissize, ret, pattern, cmpsize;
	int  		readcmd = 0, writecmd = 0;
	long 		long       longseek, longsize;

	/*
	 * Perform timed read/write on open file.
	 */
	sscanf(line,"%s %d %s %d %d %d",
		&cmd,&id,&rw,&thissize, &pattern);

	if (FILE_FD(id) == 0) {
		printf("invalid file id %d \n",id);
		return( -1);
	}


	if ( (rw != 'S') && (rw != 'L') ) {
		buffer = memalign( 4096, thissize);

		if ( mpin( buffer, thissize) != 0 ) {
			printf("mpin failed. errno = %d \n",errno);
		}
		bzero(buffer, thissize);
	}

	if (rw == 'R')  {
		if ( (ret = (int)read(FILE_FD(id),buffer,thissize)) != (thissize)) {
			printf("read failed ret = %d \n", ret);
		}
		readcmd = 1;
	} else if (rw == 'W') {
		if ((ret = fill_buffer_pattern(buffer, pattern, thissize)))
			return(ret);

		if ((ret = (int)write(FILE_FD(id), buffer,thissize))!=(thissize)){
			printf("write failed = %d, errno = %d \n",ret, errno);
		}
		writecmd = 1;
	} else if (rw == 'S') {
		lseek(FILE_FD(id), thissize, 0);
	} else if (rw == 'L') {
		longsize = (long long)thissize;
		longseek = longsize*1024*1024;
		lseek(FILE_FD(id), longseek, 0);
	} else {
		printf("bad T cmd %s \n",rw);
		return( -1);
	}

	if (readcmd || writecmd) {
		if (readcmd) {
			if (thissize > 10000)
				cmpsize = 10000;
			else
				cmpsize = thissize;
			ret = compare_buffer_pattern(buffer, pattern, cmpsize);

			if (ret < 0 ) {
				printf("unknown pattern %d \n",pattern);
			}

			if (ret == 1) {
				printf("file offset = %lx \n",file_info[id].numbytes);
				printf("filename = [%s] \n",file_info[id].name);                        
			}
		}

		if ( munpin(buffer,  thissize) != 0 ) {
			printf("munpin failed, errno = %d \n",errno);
		}

		free(buffer);
		file_info[id].numbytes += thissize;
	}

	return(0);

}


/*
 *  Total size must be 128 bits.  N.B. definition of uuid_t in types.h!
 */
#define NODE_SIZE       6

typedef struct {
        u_int32_t       uu_timelow;     /* time "low" */
        u_int16_t       uu_timemid;     /* time "mid" */
        u_int16_t       uu_timehi;      /* time "hi" and version */
        u_int16_t       uu_clockseq;    /* "reserved" and clock sequence */
        u_int16_t       uu_node [NODE_SIZE / 2];/* ethernet hardware address*/
} uu_t;

#define UUID_STR_LEN    36      /* does not include trailing \0 */
#define UUID_FORMAT_STR "%08x-%04hx-%04hx-%04hx-%04x%04x%04x"

static char *uuid_format_str  = { UUID_FORMAT_STR };

int
convert_uuid_to_string( uuid_t *uuid, char **newstring)
{
        uu_t    *uu = (uu_t *) uuid;

        if (newstring == NULL)
                return( -1 );

        *newstring = (char *)malloc(UUID_STR_LEN+1);
        if (uuid == NULL) {
                sprintf (*newstring, uuid_format_str,
                        0, 0, 0, 0, 0, 0, 0);
                return( -1 );
        }

        sprintf (*newstring, uuid_format_str,
                uu->uu_timelow,
                uu->uu_timemid,
                uu->uu_timehi,
                uu->uu_clockseq,
                uu->uu_node [0],
                uu->uu_node [1],
                uu->uu_node [2]);
        return( 0 );
}

int
convert_string_to_uuid( char *str, uuid_t *uuid )
{
	uu_t	*uu = (uu_t *) uuid;
	char	*tmpstrp, *lasttok;
	char	numstr[5];

	tmpstrp = strdup( str );

	uu->uu_timelow  = (u_int32_t)strtoul(strtok(tmpstrp,"-"),0,16);
	uu->uu_timemid  = (u_int16_t)strtol( strtok(0, "-"), 0, 16 );
	uu->uu_timehi   = (u_int16_t)strtol( strtok(0, "-"), 0, 16 );
	uu->uu_clockseq = (u_int16_t)strtol( strtok(0, "-"), 0, 16 );
	lasttok         = strtok( 0, "-");

	numstr[0] = *lasttok++;
	numstr[1] = *lasttok++;
	numstr[2] = *lasttok++;
	numstr[3] = *lasttok++;
	numstr[4] = 0;

	uu->uu_node[0] = (u_int16_t)strtol( numstr, 0, 16);

	numstr[0] = *lasttok++;
	numstr[1] = *lasttok++;
	numstr[2] = *lasttok++;
	numstr[3] = *lasttok++;
	numstr[4] = 0;

	uu->uu_node[1] = (u_int16_t)strtol( numstr, 0, 16);

	numstr[0] = *lasttok++;
	numstr[1] = *lasttok++;
	numstr[2] = *lasttok++;
	numstr[3] = *lasttok++;
	numstr[4] = 0;

	uu->uu_node[2] = (u_int16_t)strtol( numstr, 0, 16);

	return( 0 );
}

int
get_flag_bits( char *flagname )
{
	if (strncmp(flagname, "PER_FILE_GUAR",13) == 0 ) {
printf("got flag PER_FILE_GUAR \n");
		return( PER_FILE_GUAR );
	} else if (strncmp(flagname, "PER_FILE_SYS_GUAR",17) == 0 ) {
printf("got flag PER_FILE_SYS_GUAR \n");
		return( PER_FILE_SYS_GUAR );
	} else if (strncmp(flagname, "PROC_PRIVATE_GUAR",17) == 0 ) {
printf("got flag PROC_PRIVATE_GUAR \n");
		return( PROC_PRIVATE_GUAR );
	} else if (strncmp(flagname, "PROC_SHARE_GUAR",15) == 0 ) {
printf("got flag PROC_SHARE_GUAR \n");
		return( PROC_SHARE_GUAR );
	} else if (strncmp(flagname, "FIXED_ROTOR_GUAR",16) == 0 ) {
printf("got flag FIXED_ROTOR_GUAR \n");
		return( FIXED_ROTOR_GUAR );
	} else if (strncmp(flagname, "SLIP_ROTOR_GUAR",15) == 0 ) {
printf("got flag SLIP_ROTOR_GUAR \n");
		return( SLIP_ROTOR_GUAR );
	} else if (strncmp(flagname, "NON_ROTOR_GUAR",14) == 0 ) {
printf("got flag NON_ROTOR_GUAR \n");
		return( NON_ROTOR_GUAR );
	} else if (strncmp(flagname, "REALTIME_SCHED_GUAR",19) == 0 ) {
printf("got flag REALTIME_SCHED_GUAR \n");
		return( REALTIME_SCHED_GUAR );
	} else if (strncmp(flagname, "NON_SCHED_GUAR",14) == 0 ) {
printf("got flag NON_SCHED_GUAR \n");
		return( NON_SCHED_GUAR );
	} else  {
		printf("UNKNOWN FLAG [%s] \n",flagname);
		return( -1 );
	}
}

int
translate_flags( char *flagsstr) 
{
	int	flags = 0;
	char	*tmpstr, *ftokp;


	tmpstr = strdup( flagsstr );

	ftokp = strtok( flagsstr, "|" );

	while ( ftokp ) {
		flags |= get_flag_bits( ftokp );
		ftokp = strtok(NULL,"|");
	}
	

	free(tmpstr);
	return( flags );
}

/*
 * parese_open_line() 
 *	Line is of the form:
 *
 *
 *	"CMD ID FILENAME STARTTIME DURATIONTIME IOTIME IOSIZE FLAGS"
 */
int
parse_open_line( 
	char	*line, 
	int	*id,
	char 	**filename, 
	int	*starttime,
	int	*durationtime,
	int	*iotime,
	int	*iosize,
	int	*flags)
{

	char	*tmpline, *tokp, *flagsstr;

	tmpline = strdup(line);

	/*
	 * get command token
	 */
	tokp = strtok( tmpline, " ");

	/*
  	 * get id token
	 */
	tokp = strtok( NULL, " ");
	*id = atoi( tokp );

	/*
	 * get filename token
 	 */
	tokp = strtok( NULL, " " );
	*filename = strdup(tokp);

	/*
	 * get starttime token
 	 */
	tokp = strtok( NULL, " " );
	*starttime = atoi( tokp );

	/*
	 * get duration token
 	 */
	tokp = strtok( NULL, " " );
	*durationtime = atoi( tokp );

	/*
	 * get iotime token
 	 */
	tokp = strtok( NULL, " " );
	*iotime = atoi( tokp );

	/*
	 * get iosize token
 	 */
	tokp = strtok( NULL, " " );
	*iosize = atoi( tokp );
	
	flagsstr = strtok( NULL, " " );
	*flags = translate_flags( flagsstr );

	free( tmpline );

	printf(" Open file %s \n",*filename);
	printf(" Start %d, Duration %d, Iotime %d, Iosize %d, Flags 0x%x \n",
		*starttime, *durationtime, *iotime, *iosize, *flags);
	
	return( 0 );
}





