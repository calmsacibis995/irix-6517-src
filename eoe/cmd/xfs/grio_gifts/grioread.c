/*
 * This is a sample program to illustrate how to setup a guaranteed
 * rate stream.  It can only be run as super user.
 * 
 * The source file must be on the real time parition of an 
 * XFS file system. It is assumed that the file system has a blocksize of 
 * 4K bytes. The extent size of the file, or if this does not exist
 * then the realtime extent size of the file system, is used as
 * the I/O size. A rate guarantee for the I/O size number of bytes each 
 * second is obtained.  The rate guarantee is associated with the given
 * file, and the file is read.
 *
 * The rate guarantee has the following characteristics:
 *	PER_FILE_GUAR, REALTIME_SCHED_GUAR,  NON_ROTOR_GUAR
 *
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
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
#include <sys/lock.h>
#include <sys/uuid.h>
#include <sys/grio.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <limits.h>
#include <sys/prctl.h>
#include <sys/schedctl.h>
#include <sys/syssgi.h>
#include <sys/fs/xfs_itable.h>
#include <sys/fs/xfs_fsops.h>

#define FSBLKSIZE		4*1024

/*
 * This program opens a specified file.
 * Sets itself as a realtime process on a specified cpu.
 * Gets a rate guarantee of 64k bytes per second on the file.
 *
 * Reads from the file and 
 */

/*
 * get_grio_guarantee
 *
 * Issue the library call to get the rate gurantee.
 *	The guarantee will be on the file indicated by "fd".
 *	The guaranteed rate will be 1 request of size "iosize" bytes
 *	each second. The start time is now and the duration of the
 *	guarantee is 50000 seconds.  
 *
 */
void
get_grio_guarantee(int fd, int iosize) 
{
	int 		ret;
	grio_resv_t 	grio;

	grio.gr_duration = 50000;
	grio.gr_start	 = time(0);
	grio.gr_optime	 = 1000000;	/* 1 second in milliseconds */
	grio.gr_opsize   = iosize;
	grio.gr_flags	 = REALTIME_SCHED_GUAR;

	ret = grio_reserve_file( fd, &grio);
	
	if (ret ) {
		printf("Could not get rate guarantee.\n");
		printf("Error = %d \n",grio.gr_error);
		if ( grio.gr_error == ENOSPC) {
			printf("Device [%s] is out of bandwidth.\n",
				grio.gr_errordev);
		}
		exit( -1 );
	}

	ret = grio_associate_file(fd, &grio.gr_stream_id );

	if ( ret ) {
		printf("could not associate file with stream \n");
		exit( -1 );
	}
}

/*
 * main:
 *
 *	 This program opens a specified file.
 * 	 Gets a rate guarantee on the file.
 * 	 Reads from the file and checks that each read completed within 
 *	 one second.
 *
 * The command syntax is:
 *	grioread	[ -c cpunum ] -f filename
 *
 *
 */
main( int argc, char *argv[])
{
	int		c, fd, cpunum, count, t_size, seconds, iosize;
	char		*filename = NULL, *buffer;
	struct  	tms ttms;
	clock_t		t_start, t_finish, t_end, t_begin;
	struct 	fsxattr	fsxattr;
	xfs_fsop_geom_t	geo;

	/*
	 * Lock the process text segment into memory 
	 * so that it cannot be swapped.
	 */
        if (plock( TXTLOCK )) {
                printf("Could not plock grioread process. \n");
                exit(-2);
        }

	/*
	 * Set the process to non-degrading priority in the
	 * highest priority class.
	 */
        if (schedctl(NDPRI, 0, NDPHIMAX)) {
                printf("Could not set process priority for grioread.\n");
                exit(-2);
	}

	/*
	 * get command line args
	 */
        while ((c = getopt(argc, argv, "f:c:")) != EOF) {
                switch (c) {
                        case 'c':
                                cpunum = atoi(optarg);
				/*
			 	 * Force the process to run on a single
				 * cpu. Normally, this cpu will have
				 * been marked as ISOLATED and RESTRICTED
				 * so that only select processes will	
				 * run on it.
	 			 */
                                if (sysmp(MP_MUSTRUN, cpunum) == -1) {
                                        printf("sysmp mustrun on %d failed.\n",
						cpunum);
					exit( -2 );
				}
                                break;
			case 'f':
				filename =  strdup( optarg );
				break;
                        default:
                                printf("Bad argument %c \n",c);
                                break;
                }
        }


	/*
	 * Check for filename
	 */
	if ( filename == NULL ) {
		printf("Must specify input file with -f option. \n");
		exit( -1 );
	}

	/*
	 * Open the realtime file with direct I/O flag.
	 */
	fd = open( filename, O_RDWR | O_DIRECT);
	if ( fd < 0 ) {
		printf("Open of %s failed \n",filename);
		exit( -1 );
	}

	/*
	 * Get the file's extended attributes
	 */
	if ( fcntl( fd, F_FSGETXATTR, &fsxattr ) ) {
		printf("Could not get extended attributes for %s. \n",filename);
		exit( -1 );
	}

	/*
	 * If this is not a realtime file exit.
	 */
	if ( !(fsxattr.fsx_xflags & XFS_XFLAG_REALTIME) ) {
		printf("%s is not a realtime XFS file. \n",filename);
		exit( -1 );
	}
	
	/*
	 * If the file extent size is set, use this for
 	 * the I/O size.
	 */
	if (fsxattr.fsx_extsize) {
		iosize = fsxattr.fsx_extsize;
	} else {
		/*
		 * Otherwise, use the realtime file system extent size.
	 	 */
		if ( syssgi( SGI_XFS_FSOPERATIONS, fd,
			XFS_FS_GEOMETRY,(void *)0, &geo) < 0 ) {
			printf("Could not get file system geometry.\n");
			exit( -1 );
		}
		iosize = geo.rtextsize * geo.blocksize;
	}

	printf("I/O size is %d bytes.\n",iosize);
	printf("Guarantee is for %d Kbytes/sec. \n",
		iosize/1024);
	/*
	 * Get aligned data buffer.
	 */
	if ( (buffer = (char *)memalign( FSBLKSIZE, iosize)) == NULL ) {
		printf("Could not allocate buffer. \n");
		exit( -1 );
	}

	/*
	 * Pin data buffer into memory so that it cannot be swapped.
 	 */
	if ( mpin( buffer, iosize) ) {
		printf("Could not pin buffer into memory.\n");
		exit( -1 );
	}
	
	/*
	 * Get the rate guarantee.
	 */
	get_grio_guarantee( fd, iosize);

	/*
	 * Begin reading the file.
 	 */
	printf("\nStart rate guaranteed reading.\n");
	
	t_start = times(&ttms);
	t_begin = times(&ttms);
	t_size  = 0;
	count   = 0;
	while ( read( fd, buffer, iosize ) == iosize ) {
		t_finish = times(&ttms);

		if ( (t_finish - t_start) >  HZ) {
			printf("Rate guarantee failed on read %d.\n",
				count);
			printf("Time difference was %d ticks.\n",
				t_finish - t_start);
			exit( -3 );
		}

		/*
		 * Add 1 second to start time.
		 */
		t_start += HZ;
		t_size += iosize;
		count++;
	}
	t_end = times(&ttms);

	seconds = (t_end - t_begin) / HZ;

	printf("\n");
	printf("Performed %d read operations for a total of %d bytes "
		"in %d seconds.\n", count, t_size, seconds);
	printf("Average rate of %d Kbytes/sec.\n",
		(t_size/seconds)/1024);
}

