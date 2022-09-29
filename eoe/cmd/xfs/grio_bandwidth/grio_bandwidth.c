#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/grio_bandwidth/RCS/grio_bandwidth.c,v 1.13 1998/07/20 23:31:15 kanoj Exp $"
/* 
 * grio_bandwidth 
 * 
 * 	This utility is used to determine the number of I/O operations of a 
 * 	given size that can per performed on a given disk device in one
 * 	second.
 * 
 * 	The user supplies a device, an I/O size, and a sample period.
 *	Optionally the user can tell the utility to use only read operations
 *	or write operations instead of a random combination of the two.
 * 
 *	The utility issues I/O requests of the given size to the  
 * 	given device including a "random" seek.
 *	It determines the average number of I/O operations that were
 * 	issued per second during the sample period, degrades the performance
 *	by 10% and then prints the results. It also keeps track of the  
 *	longest operation time and prints the number of such "worst case"
 *	operations that can be performed each second.
 * 
 *	iosize is specified in bytes
 *	sampletime is specified in seconds
 */
#include "grio_bandwidth.h"


/*
 * Global data
 */
int	*child_status_average = NULL;
int	shmid_average = 0;
int	*child_status_worst = NULL;
int	shmid_worst = 0;
int	*child_status_longestio = NULL;
int	shmid_longestio = 0;

int	io_tick_time[TICK_TIME_SIZE];
int     too_high;
int	io_time[MAX_IOS];
int	print_each_io = 0;


/*
 * Forward routine declarations.
 */
void delete_shm_seg( void );
int measure_disk_bw( char *, int, int, int, clock_t, int, int, FILE *);
int make_shm_seg( int, int);
int get_size_of_device( char *, int *, char *, int);
int remove_old_bw_line_from_config_file( char *, int );
int add_new_bw_line_to_config_file( char *, int, int );
void nodename_to_volname(char *, char *);

/* 
 * usage()
 *
 *	Print usage message and exit.
 *
 * RETURNS:
 *	no return
 *
 */
void
usage(void)
{
	fprintf(stderr,"grio_bandwidth [-rwuV] "
		"-d griodiskname -s iosize [-t sampletime] \n");
	fprintf(stderr,"grio_bandwidth [-rwuV] "
		"-c controllername -s iosize [-t sampletime] \n");
	fprintf(stderr,"grio_bandwidth [-frwuV] "
		"-R -s iosize [-t sampletime] \n");

	delete_shm_seg();
	exit( -1 );
}

/* 
 * remove_nodename_params_from_config_file
 *
 *	Remove any REPLACE lines from the /etc/grio_disks
 *	file that specify the given nodename.
 *
 * RETURNS:
 *	0 on sucess
 *	-1 on erro
 */
int
remove_nodename_params_from_config_file( 
char	nodename[DEV_NMLEN])
{
	int	result;
	char	cmd[LINE_LENGTH];
	char	replaceline[LINE_LENGTH];
	char	tmpfile[FILE_NAME_LEN];
	struct  stat sbuf;

	bzero( tmpfile, sizeof( tmpfile) );
	sprintf( tmpfile, "/tmp/.grio_bandwidth.%d",getpid());
	sprintf(replaceline,"REPLACE.*%s ",nodename);
	sprintf(cmd,"cat %s | grep -v '%s' > %s", GRIODISKS, replaceline, tmpfile );

	/*
	 * Issue command
 	 */
	result = system( cmd );
	if ( system_failed( result, 1 ) ) {
		unlink( tmpfile );
		fprintf(stderr,"grio_bandwidth: Command \"%s\" failed.\n", cmd);
		return( -1 );
		
	}

	if ( stat( tmpfile, &sbuf) ) {
		/*
		 * if stat fails, this probably means the file
		 * does not exist ( no output from grep ).
		 * unlink file just be make sure
		 */
		unlink( tmpfile );
	} else {
		bzero( cmd,  LINE_LENGTH );
		sprintf(cmd,"/sbin/cp %s %s", tmpfile, GRIODISKS );
		result = system( cmd );
		if ( system_failed( result, 1 ) ) {
			unlink( tmpfile );
			fprintf(stderr,"grio_bandwidth: Command \"%s\" failed.\n", cmd);
			return( -1 );
		}
		unlink( tmpfile );
	}

	return( 0 );
}

/*
 * add_nodename_params_to_config_file()
 *	
 *	Add a line of the form:	
 *		"REPLACE 	nodename iosize numiops" 
 *	
 *	to the /etc/grio_disks file.
 *
 * RETURNS:
 *	0 on success
 *	-1 on error
 */
int
add_nodename_params_to_config_file( 
char	nodename[DEV_NMLEN], 
int	numiops,
int	iosize)
{
	int	result;
	char	cmd[LINE_LENGTH];

	sprintf(cmd,"echo \"REPLACE\t%s\t%d\t%d\" >> %s",nodename, iosize, numiops, GRIODISKS);
	result = system( cmd );
	if ( system_failed( result, 1 ) ) {
		fprintf(stderr,"grio_bandwidth: Command \"%s\" failed. \n",cmd);
		return( -1 );
	}

	return( 0 );
}

/* 
 * reclaim_bw_results()
 *
 *	Print the results of the RAID bandwidth measurement
 *	and/or add the REPLACE lines to the /etc/grio_disks file.
 *
 * RETURNS:
 *	0 on sucess
 *	-1 on failure
 */
int
reclaim_bw_results(
device_name_t	nodenames[MAX_NUM_DEVS],
int		num_devices,
int		iosize,
int		rw,
int		addparams,
int		verbose,
int		streams,
int		raid)
{
	int	i, j, t1;
	int	count = 0, average, worst;
	char	idstr[DISK_ID_SIZE +1], devicename[DEV_NMLEN];


	for ( i = 0; i < num_devices; i++ ) {
		average = 0;
		worst = 0;
		/*
 		 * capture first "stream" number of results and get the average
 		 */
		for ( j = 0; j < streams; j++ ) {
			average  += child_status_average[count];
			worst    += child_status_worst[count];
			count++;
		}
		bzero( idstr, DISK_ID_SIZE + 1 );
		nodename_to_volname(nodenames[i].devicename, devicename);
		get_size_of_device(devicename, &t1, idstr, 1 );

		printf("device %s \twith ID \"%s\",\n", nodenames[i].devicename, idstr);
		if ( rw == READ_FLAG ) {
			printf("using only read ops, provided:\n");
		} else if ( rw == WRITE_FLAG ) {
			printf("using only write ops, provided:\n");
		} else {
			printf("using a mix of read and write ops, provided :\n");
		}
#ifndef DEGRADE_EACH_THREAD
		/*
		 * degrade performance by 10%
		 */
		average = average - ((average+9)/10);
#endif
		printf("on average:\t\t%d ops of size %d each second \n",
                        average, iosize);

                printf("in the worst case:\t%d ops of size %d each second \n",
                        worst, iosize);

		if ( addparams ) {
			if ( verbose ) {
				printf("Adding bandwidth information to grio config file %s.\n",
					GRIODISKS);
			}

			if ( raid ) {
				remove_nodename_params_from_config_file(nodenames[i].devicename);
				add_nodename_params_to_config_file(nodenames[i].devicename, average, iosize );
			} else {
				remove_old_bw_line_from_config_file( idstr, iosize );
				add_new_bw_line_to_config_file( idstr, average, iosize );
			}
		}
	}

	return( 0 );
}

/* 
 * wrapper_measure_disk_bw()
 *
 *	This routine (process) issues I/O to one of the
 *	devices and stores the results in child_status_average[arglist->index],
 *	and child_status_worst[arglist->index];
 *
 * RETURNS:
 *	no return - exit 0
 *
 */
void 
wrapper_measure_disk_bw( void *argptr ) 
{
	disk_bw_arg_list_t *arglist = 
		(disk_bw_arg_list_t *)argptr;

	measure_disk_bw(
		arglist->devicename,
		arglist->iosize,
		arglist->rw,
		arglist->verbose,
		arglist->sampletime,
		arglist->silent,
		arglist->index,
		stdout);
	exit( 0 );
}

/* 
 * measure_multiple_disks_io()
 *
 *	Execute bandwidth measurement on the given
 *	devices.
 *
 * RETURNS:
 *	0 on sucess
 *	-1 on error
 */
int
measure_multiple_disks_io(
int		num_devices,
device_name_t	nodenames[MAX_NUM_DEVS],
int		sampletime,
int		iosize,
int		rw,
int		addparams, 
int		verbose,
int		streams, 
int		raid,
int		reclaim_results)
{
	int	i,j;
	int	wstat, result, pidcount = 0;
	int	pids[MAX_NUM_PROCS];
	int	pid_completed[MAX_NUM_PROCS];
	disk_bw_arg_list_t *arglist;


	/*
 	 * delete any previously created shared memory segments
 	 */
	delete_shm_seg();

	bzero( pids, sizeof(pids) );
	bzero( pid_completed, sizeof(pid_completed) );

	if ( ( num_devices * streams ) > MAX_NUM_PROCS ) {
		fprintf(stderr,"grio_bandwidth: Too many simultaneous streams specified.\n");
		fprintf(stderr,"The maximum number of simultaneous streams with this "
			"configuration is %d.\n",
			MAX_NUM_PROCS/num_devices);
		return( -1 );
	}

	if ( make_shm_seg( num_devices * streams, num_devices * streams ) )  {
		return( -1 );
	}

	for ( i = 0; i < num_devices; i++) {
		for ( j = 0; j < streams; j++) {
			arglist = malloc( sizeof( disk_bw_arg_list_t ) );
			if ( arglist == NULL ) {
				perror("malloc failed");
			}
			bzero( arglist, sizeof(disk_bw_arg_list_t ) );

			strncpy( arglist->devicename, 
				nodenames[i].devicename, 
				strlen(nodenames[i].devicename));
			arglist->iosize 	= iosize;
			arglist->rw		= rw,
			arglist->verbose 	= verbose;
			arglist->sampletime 	= sampletime;
			arglist->silent		= raid;
			arglist->index		= pidcount;
			arglist->addparams	= addparams & (!raid);

			if ( verbose ) {
				printf("Starting I/O bandwidth test on device %s.\n",
					arglist->devicename);
			}

			if ( (pids[pidcount] = sproc( wrapper_measure_disk_bw, 
					PR_SALL, (void *)arglist)) == -1 ) {
				perror("sproc failed");
			} else {
				pid_completed[pidcount] = -1;
				pidcount++;
			}
		}
	}

	if ( pidcount != ( streams * num_devices) ) {
		fprintf(stderr,"grio_bandwidth: Could not launch %d concurrent "
			"I/O streams.\n", streams);
		return( -1 );
	}

	while ( ( result = wait(&wstat) ) != -1 )  {
		for ( i = 0; i < pidcount; i ++) {
			if ( result == pids[i] ) {
				pid_completed[i] = system_failed( wstat, 1);
			}
		}
	}

	if ( errno != ECHILD ) {
		fprintf(stderr,"grio_bandwidth: An unexpected error occured while "
			"running concurrent RAID device bandwidth tests.\n"
			"Error number is %d.\n",errno);
		return( -1 );
	}

	for ( i = 0; i < pidcount; i++ ) {
		if ( pid_completed[i] ) {
			if ( raid ) {
				fprintf(stderr,"grio_bandwidth: Error occured running "
					"bandwidth test on device %s \n",
					nodenames[i/streams].devicename);
				fprintf(stderr,"Run the following command to determine "
					"the error:\n");
				fprintf(stderr,"    grio_bandwidth -d %s -s %d -t %d -r\n",
					nodenames[i/streams].devicename,iosize, sampletime);
			}
			return( -1 );
		}
	}

	if ( reclaim_results ) {
		reclaim_bw_results(nodenames, 
			num_devices, iosize, rw, addparams, verbose, streams, raid );
	}

	return( 0 );
}

/*
 * measure_single_disk_io() 
 * 
 *	Invoke measure_multiple_disks_io() with a array size fo 1.
 * 
 *  RETURNS:
 *	0 on sucess 
 *	non-0 on error
 */
int
measure_single_disk_io(
char    *nodename,
int     iosize,
int     rw,
int     verbose,
clock_t sampletime,
int     addparam)
{
	device_name_t	nodenames[1];

	strcpy(nodenames[0].devicename, nodename );

	return(measure_multiple_disks_io(1, nodenames, sampletime, 
		iosize, rw, addparam, verbose, 1, 0, 1));
}

/*
 * get_size_of_device()
 *
 *
 *
 *
 *
 */
int
get_size_of_device( char *devicename, int *basesector, char *idstr, int print_err ) 
{
	int			fd, i;
	struct volume_header	vh;
	struct stat		stat;
	int			maxpartitionsize = 0;
	

	if  ( (fd = open(devicename,O_RDONLY)) == -1 ) {
		if ( print_err ) {
			fprintf(stderr,
				"grio_bandwidth: could not open device %s\n",
				devicename);
			usage();
		} else {
			delete_shm_seg();
			exit( -1 );
		}
	}

	if ( fstat(fd, &stat) == -1 ) {
		if ( print_err ) {
			fprintf(stderr,
				"grio_bandwidth: could not open device %s\n",
				devicename);
			usage();
		} else {
			delete_shm_seg();
			exit( -1 );
		}
	}

	if ( !(stat.st_mode & S_IFCHR ) ) {
		if ( print_err ) {
			fprintf(stderr,
				"grio_bandwidth: must specify a character device.\n");
			usage();
		} else {
			delete_shm_seg();
			exit( -1 );
		}
	}

	if ( ioctl(fd, DIOCGETVH, &vh ) != 0 ){
		if ( print_err ) {
			fprintf(stderr,
				"grio_bandwidth: could not get volume header for "
				"device  %s\n", devicename);
			usage();
		} else {
			delete_shm_seg();
			exit( -1 );
		}
	}

	if ( vh.vh_magic != VHMAGIC ) {
		if ( print_err ) {
			fprintf(stderr,
				"grio_bandwidth: invalid volume header for "
				"device  %s\n", devicename);
			usage();
		} else {
			delete_shm_seg();
			exit( -1 );
		}
	}


	if ( vh.vh_dp.dp_secbytes != 512 ) {
		if ( print_err ) {
			fprintf(stderr,
				"grio_bandwidth: only device with 512 byte sector "
				"sizes are supported.\n");
			fprintf(stderr,
				"grio_bandwidth: device %s has a sector size of %d.\n",
				devicename, vh.vh_dp.dp_secbytes);
			usage();
		} else {
			delete_shm_seg();
			exit( -1 );
		}
	}

	for ( i = 0 ; i < NPARTAB; i++ ) {
		if ( vh.vh_pt[i].pt_nblks > maxpartitionsize) {
			maxpartitionsize = vh.vh_pt[i].pt_nblks;
		}

		/*
	 	 * get the size of the volume header
		 */
		if ( i == 8 ) {
			*basesector = vh.vh_pt[i].pt_nblks;
		}
	}

	if ( ioctl(fd, DIOCDRIVETYPE, idstr) == -1 ) {
		fprintf(stderr,
			"grio_bandwidth: could not get device id string.\n");
		fprintf(stderr,"This device may be controlled by non-SCSI device driver.\n");
		strncpy( idstr,"no disk id",10);
	}
	close(fd);




	return( maxpartitionsize );
}


int
issue_seek(
	int	 	fd,
	int		iosize,
	long long 	devsize,
	long long	baseoffset)
{
	long long	seekloc, curloc, halfsize, diffloc;
	double		mix;

	
	curloc = lseek64( fd, 0LL, SEEK_CUR);
	mix = drand48();

	halfsize = devsize/2;
	if (curloc < halfsize ) {
		seekloc = curloc + halfsize;
		seekloc = seekloc + ((devsize - seekloc)*mix);
		diffloc = seekloc - curloc;
	} else {
		seekloc = curloc - halfsize;
		seekloc = seekloc - (seekloc*mix);
		diffloc = curloc  - seekloc;
	}

	if ( halfsize > diffloc ) {
		fprintf(stderr,"grio_bandwidth: internal error - "
			"invalid seek location \n");
		delete_shm_seg();
		exit( -1 );
	}


	if ( (seekloc + iosize) >= devsize ) {
		/*
		 * check for end of device
		 */
		seekloc = seekloc - iosize;
	} else {
		/*
		 * make sure rounding will not put
		 * offset less than baseoffset
		 */
		seekloc = seekloc + 4095LL;
	}

	seekloc = seekloc + baseoffset;

	/*
	 * mask to a 4k boundary
	 */
	seekloc = seekloc & (0xfffffffffffff000LL);

	if ( seekloc < baseoffset ) {
		fprintf(stderr,"grio_bandwidth: internal error - "
			"invalid seek location \n");
		delete_shm_seg();
		exit( -1 );
	}

	if ( lseek64(fd, seekloc, SEEK_SET) == -1 ) { 
		fprintf(stderr,"grio_bandwidth: seek to %lld failed \n",
			seekloc);
	}
	return( 0 );
}


int
issue_io(
	int	fd,
	char	*buffer,
	int	iosize,
	int	rw,
	int	index)
{
	int		ret, do_read;
	double		mix;
	clock_t		start, complete;
	struct  tms	t;

	mix   = drand48();
	start = times(&t);


	if ( rw == READ_FLAG ) {
		do_read = 1;
	} else if (rw == WRITE_FLAG) {
		do_read = 0;
	} else {
		if ( (mix*100) < 50 ) {
			do_read = 1;
		} else {
			do_read = 0;
		}
	}

	if ( do_read ) {
		if ( (ret = read(fd, buffer, iosize)) != iosize ) { 
			fprintf(stderr,"grio_bandwidth: read op error\n");
			fprintf(stderr,"iosize %d, ret %d, errno %d\n",
				iosize, ret, errno);
		}
	} else {
		if ( (ret = write(fd, buffer, iosize)) != iosize ) { 
			fprintf(stderr,"grio_bandwidth: write op error\n");
		}
	}

	complete = times(&t);

	if ( (complete - start) > child_status_longestio[index] ) {
		child_status_longestio[index] = complete - start;
	}

	return( complete - start );
}


/*
 * issue_io_ops()
 *
 *
 *
 *
 *
 */
int
issue_io_ops( 
char 	*devicename, 
char 	*buffer, 
int	devicesize, 
int	basesector,
int 	iosize, 
int	sampletime,
int	rw,
int	verbose,
int	index,
FILE	*output_fd)
{
	int		fd, count, avcount, iotime;
	long long	longdevsize, longtemp, longbase;
	time_t		starttime;

        if ( (fd = open( devicename, O_RDWR ) ) == -1) {
		fprintf(stderr,
			"grio_bandwidth: could not open device %s\n",
			devicename);
		usage();
        }

	count		= 0;
	starttime 	= time( 0 );

	longtemp	= basesector;
	longbase	= (512LL)*longtemp;

	longtemp	= devicesize;
	longdevsize	= ((512LL)*longtemp) - longbase;

	lseek( fd, 0,  SEEK_SET);

	while ( (starttime + sampletime) > time(0) ) {
		issue_seek(fd, iosize, longdevsize, longbase);
		iotime = issue_io( fd, buffer, iosize, rw, index );

		if ( verbose  && (iotime > 30 ) ) {
			fprintf(output_fd,"\tIO on disk [%s] took \n"
				"\t %d ticks to complete (time is %d)\n", 
				devicename, iotime, time(0));
		}

		if ( iotime > TICK_TIME_SIZE ) {
			too_high++;
		} else  {
			io_tick_time[iotime]++;
		}

		if ( count < MAX_IOS )  {
			io_time[ count ] = iotime;
		}

		count++;
	}
	avcount = count / sampletime;
	return( avcount );
}


int
remove_old_bw_line_from_config_file(
char	*idstr,
int	iosize)
{
	int	ksize = iosize/1024, result;
	char	cmd[LINE_LENGTH];
	char	addline[LINE_LENGTH];
	char	tmpfile[LINE_LENGTH];
	char	expanded_idstr[LINE_LENGTH];
	struct	stat sbuf;
	

	sprintf(expanded_idstr,"%s",idstr);
	sprintf( tmpfile,"/tmp/.grio_bandwidth.%d",getpid() );
	sprintf( addline,"ADD.*%s.*%dK",idstr,ksize);
	sprintf(cmd,"cat %s | grep -v '%s' > %s ", GRIODISKS, addline, tmpfile);

	result = system( cmd );
	if ( system_failed( result, 1 ) ) {
		unlink( tmpfile );
		fprintf(stdout,"grio_bandwidth: Command \"%s\" failed.\n", cmd);                
		return( -1 );
	}

	if ( stat( tmpfile, &sbuf) ) {
		/*
		 * if stat fails, this probably means the file
		 * does not exist ( no output from grep ).
		 * unlink file just be make sure
		 */
		unlink( tmpfile );
	} else {
		bzero( cmd, LINE_LENGTH );
		sprintf(cmd,"/sbin/cp %s %s", tmpfile, GRIODISKS );
		result = system( cmd );
		if ( system_failed( result, 1 ) ) {
			unlink( tmpfile );
			fprintf(stdout,"grio_bandwidth: Command \"%s\" failed.\n", cmd);
			return( -1 );
		}
		unlink( tmpfile );
	}
	return( 0 );

}

/* 
 *
 *
 */
int
add_new_bw_line_to_config_file( 
char	*idstr,
int	count,
int	iosize)
{
	int     result;
	char    cmd[LINE_LENGTH];

	sprintf(cmd,"echo \"ADD\\t\\\"%s\\\"\\t%dK\\t%d\" >> %s",idstr,iosize/1024,count,GRIODISKS);
	result = system( cmd );
	if ( system_failed( result, 1 ) ) {
		fprintf(stdout,"grio_bandwidth: Command \"%s\" failed. \n",cmd);                
		return( -1 );
	}

	return( 0 );

}


/*
 * measure_disk_bw()
 *
 *
 *
 *
 */
int
measure_disk_bw(
char	*nodename, 
int 	iosize, 
int 	rw, 
int	verbose,
clock_t	sampletime, 
int 	silent, 
int 	index,
FILE	*error_fd)
{
	int		count, basesector = 0;
	long		devicesize;
	char		*buffer, devicename[DEV_NMLEN];
	char		idstr[DISK_ID_SIZE + 1];
	FILE		*fp, *nullfp;

	/*
 	 * If the silent flag is set, send the output to /dev/null
 	 */
	if ( silent ) {
		nullfp = fopen("/dev/null","w");
		fp = nullfp;
	} else {
		nullfp = NULL;
		fp = error_fd;
	}

	/*
 	 * devicesize is specified in units of 512 byte sectors
	 */
	bzero( idstr, DISK_ID_SIZE + 1 );
	nodename_to_volname(nodename, devicename);
	devicesize = get_size_of_device(devicename,&basesector,idstr, !silent);

	buffer = memalign( 4096, iosize );

	count = issue_io_ops(devicename,
		buffer, devicesize, basesector, iosize, sampletime, 
		rw, verbose, index, fp);

	free( buffer );

	if ( child_status_longestio[index] < 1 ) {
		fprintf(fp,"grio_bandwidth: an error occured determining the "
			"worst case bandwidth perfomance of device %s.\n",
			devicename);
		if ( nullfp ) {
			fclose( nullfp );
		}
		return( -1 );
	}

	child_status_average[ index ] =  count;
	child_status_worst[ index ] =  100/child_status_longestio[index];

	if ( nullfp ) {
		fclose( fp );
	}

	return( count );
}


/*
 * make_shm_seg
 *	Create two shared memory segments each of size "numentries" integers.
 *	One is pointed at by child_status_average, the other is pointed
 *	at by child_status_worst.
 *
 */
int
make_shm_seg( int numentries, int users )
{

	shmid_average = shmget( IPC_PRIVATE, sizeof(int)*numentries, IPC_CREAT);
	if ( shmid_average == -1 ) {
		fprintf(stderr,"grio_bandwidth: could not create shared memory id. \n");
		return( -1 );
	}
	child_status_average = (int *)shmat(shmid_average, (void *)0, 0 );
	if ( child_status_average == (int *)(-1) ) {
		fprintf(stderr,"grio_bandwidth: could not attach to  shared memory segment. \n");
		return( -1 );
	}

	shmid_worst = shmget( IPC_PRIVATE, sizeof(int)*numentries, IPC_CREAT);
	if ( shmid_worst == -1 ) {
		fprintf(stderr,"grio_bandwidth: could not create shared memory id. \n");
		return( -1 );
	}
	child_status_worst = (int *)shmat(shmid_worst, (void *)0, 0 );
	if ( child_status_worst == (int *)(-1) ) {
		fprintf(stderr,"grio_bandwidth: could not attach to  shared memory segment. \n");
		return( -1 );
	}

	shmid_longestio = shmget( IPC_PRIVATE, sizeof(int)*numentries, IPC_CREAT);
	if ( shmid_longestio == -1 ) {
		fprintf(stderr,"grio_bandwidth: could not create shared memory id. \n");
		return( -1 );
	}
	child_status_longestio = (int *)shmat(shmid_longestio, (void *)0, 0 );
	if ( child_status_longestio == (int *)(-1) ) {
		fprintf(stderr,"grio_bandwidth: could not attach to  shared memory segment. \n");
		return( -1 );
	}

	bzero( child_status_average, sizeof(int)*numentries);
	bzero( child_status_worst, sizeof(int)*numentries);
	bzero( child_status_longestio, sizeof(int)*numentries);

	usconfig( CONF_INITUSERS, 3*(users + 1) );

	return( 0 );
}

/*
 * delete_shm_seg()
 *
 *	Tear down shared memory segments.
 *
 *
 */
void
delete_shm_seg(void)
{
	if ( child_status_average ) {
		if ( shmdt( child_status_average ) ) {
			fprintf(stderr,"grio_bandwidth: Error detaching shared memory segment.\n");
		}
		if ( shmctl( shmid_average, IPC_RMID) ) {
			fprintf(stderr,"grio_bandwidth: Error removing shared memory id.\n");
		}
		child_status_average = NULL;
	}

	if ( child_status_worst ) {
		if ( shmdt( child_status_worst ) ) {
			fprintf(stderr,"grio_bandwidth: Error detaching shared memory segment.\n");
		}
		if ( shmctl( shmid_worst, IPC_RMID) ) {
			fprintf(stderr,"grio_bandwidth: Error removing shared memory id.\n");
		}
		child_status_worst = NULL;
	}

	if ( child_status_longestio ) {
		if ( shmdt( child_status_longestio ) ) {
			fprintf(stderr,"grio_bandwidth: Error detaching shared memory segment.\n");
		}
		if ( shmctl( shmid_longestio, IPC_RMID) ) {
			fprintf(stderr,"grio_bandwidth: Error removing shared memory id.\n");
		}
		child_status_longestio = NULL;
	}
}

/*
 * This is copied from daemon/disk.c.
 * INPUT:
 *	 block/char device number of partition or volume header or
 *	 disk id.
 * RETURN:
 *	 0 on failure
 *	 1 on success
 */
STATIC int
get_diskname(dev_t diskpart, char *pathnm)
{
	int  buflen = DEV_NMLEN;
	const char *part_keyword = "/" EDGE_LBL_PARTITION "/";
	const char *vhdr_keyword = "/" EDGE_LBL_VOLUME_HEADER "/";
	const char *disk_keyword = "/" EDGE_LBL_DISK;
	const char *dvol_keyword = "/" EDGE_LBL_VOLUME "/";
	char *t1;
	struct stat sbuf;

	bzero(pathnm, DEV_NMLEN);
	dev_to_rawpath(diskpart, pathnm, &buflen, 0);

	/*
	 * Verify it is a disk name.
	 */
	t1 = strstr(pathnm, disk_keyword);
	if (t1 == NULL)
		return(0);

	/*
	 * Now try to slash off the partition/volume_header part.
	 */
	t1 = strstr(pathnm, part_keyword);
	if (t1 == NULL)
		t1 = strstr(pathnm, vhdr_keyword);
	if (t1 == NULL)
		t1 = strstr(pathnm, dvol_keyword);
	if (t1 != NULL)
		*t1 = 0;
	if (stat(pathnm, &sbuf) == -1)
		return(0);
	return(1);
}


/*
 * The input device # is the block device # of the disk reported
 * by xlv or the unique disk name passed from ggd shm segment.
 */
int 
convert_devnum_to_nodename(dev_t device, char *nodename)
{
	char pathbuf[DEV_NMLEN];

	bzero(pathbuf, DEV_NMLEN);
	get_diskname(device, nodename);
	return(0);
}


/*
 * The user may have passed in the hwgraph name or the alias 
 * name of the disk.
 */
int
user_diskname_to_nodename(char *user_diskname, char *nodename)
{
	struct stat statbuf;

	if (stat(user_diskname, &statbuf))
		return(-1);
	return(convert_devnum_to_nodename(statbuf.st_rdev, nodename));
}

/*
 * Change the grio nodename to the char volume name for accessing 
 * the disk.
 */

void
nodename_to_volname(char *nodename, char *volname)
{
	strncpy(volname, nodename, DEV_NMLEN);
	strcat(volname, "/" EDGE_LBL_VOLUME "/" EDGE_LBL_CHAR);
}

/*
 * main
 *
 *
 *
 */
main(int argc, char *argv[])
{

	int		c, iosize = 0, sampletime = DEFAULT_SAMPLETIME;
	int		rw = 0, verbose = 0;
	int		checkdevbw = 0, raid = 0, controller = 0;
	int		addparams = 0;
	int		fairshare = 0;
	char		*devicename=NULL, nodename[DEV_NMLEN], *contname=NULL;
	extern  	char *optarg;

	while ( (c = getopt( argc, argv, "RVfrwc:d:s:t:u" )) != EOF ) {
		switch (c) {
			case 'd':
				devicename = strdup(optarg);
				checkdevbw = 1;
				break;
			case 'f':
				fairshare = 1;
				break;
			case 's':
				iosize = atoi(optarg);
				break;
			case 't':
				sampletime = atoi(optarg);
				break;
			case 'u':
				addparams = 1;
				break;
			case 'r':
				rw |= READ_FLAG;
				break;
			case 'w':
				rw |= WRITE_FLAG;
				break;
			case 'R':
				raid = 1;
				break;
			case 'V':
				verbose = 1;
				break;
			case 'c':
				contname = strdup(optarg);
				controller = 1;
				break;
			default:
				usage();
		}
	}

	if (rw == 0) {
		rw = READ_FLAG;
	}

	if ( sampletime < 60 ) {
		fprintf(stderr,
			"grio_bandwidth: sample time must be longer "
			"than 60 seconds.\n");
		usage();
	}

	if ( iosize == 0 ) {
		fprintf(stderr,
			"grio_bandwidth: must specify an I/O size in bytes.\n");
		usage();
	}

	if ( iosize % 512 ) {
		fprintf(stderr,
			"grio_bandwidth: I/O size must be a multiple " 
			"of 512 bytes.\n");
		usage();
	}

	if ( fairshare && ( ! ( raid ) ) ) {
		fprintf(stderr, "grio_bandwidth: -f option is only valid when -R option "
			"is specified.\n");
		usage();
	}


	if ( (checkdevbw + raid + controller) > 1 ) {
		fprintf(stderr,
			"grio_bandwidth: only one of options -R, c or d may "
			"be specified\n");
		usage();
	}

	if ( (checkdevbw + raid + controller) == 0 ) {
		fprintf(stderr,
			"grio_bandwidth: one of options -R, c or d must "
			"be specified\n");
		usage();
	}

	if ( raid ) {
		measure_raid_bw( sampletime, iosize, rw, addparams, verbose, fairshare );
		return( 0 );
	} else if (controller) {
		measure_controller_bw(contname, sampletime, iosize, rw, addparams, verbose);
		return(0);
	}


	if ( devicename == NULL ) {
		fprintf(stderr,
			"grio_bandwidth: must specify "
			"a disk device to measure bandwidth.\n");
		usage();
	}


	if (user_diskname_to_nodename(devicename, nodename) == -1) {
		fprintf(stderr,
			"grio_bandwidth: must specify disk alias "
			"name or grio node name for disk.\n");
		usage();
	}

	/*
	 * Issue bandwidth measurement commands.
	 */
	measure_single_disk_io(nodename, iosize, rw, verbose, 
		sampletime, addparams);
	delete_shm_seg();
	return(0);
}
