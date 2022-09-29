#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/grio/RCS/main.c,v 1.13 1997/03/13 06:02:47 kanoj Exp $"

/*
 * main.c
 *	grio device reservation status utility
 *
 *	This utility displays the current reservation information on a 
 *	given device.
 *
 *
 */
#include "griostat.h"

ggd_info_summary_t	*ggd_info;

/*
 * usage()
 * 
 * RETURNS: 
 * 	doesn't
 */
void
usage( void )
{
	printf("grio: [-BCDFIPRSV] [-a] [-d devname] [-v voldev] [-f filename] [-s stream id].\n");

 	printf("-B	show device bandwidth reservations         \n");
	printf("-C	convert devices to path and show bandwidth \n");
	printf("-D	delete stream				   \n");
	printf("-F	show full bandwidth for virtual disk       \n");
	printf("-I	show global grio statistics		   \n");
	printf("-P	show device I/O path                       \n");
	printf("-R	show reservations on filename by pid       \n");
	printf("-S	show stream information			   \n");
	printf("-V 	show VOD rotation slots                    \n");
	printf("\n");
	printf("-a	specify all disk devices                   \n");
	printf("-d	specify device path                   	   \n");
	printf("-f 	specify file name                          \n");
	printf("-s	specify stream id			   \n");
	printf("-v	specify XLV volume device path             \n");

	exit(-1);
}

/*
 * grioview:
 *
 * Commands:
 *	-b		show device bandwidth reservations
 *	-c		convert devices to path and show bandwidth.
 *	-F		show full bandwidth for virtual disk
 *	-p		show device I/O path
 *	-r 		show VOD rotation slots
 *	-R		show reservations on filename by pid
 *	-i		show global grio statistics 
 *
 *	-a		all disks
 *	-d		specify device path 
 *	-v		specify XLV volume device path
 *	-f 		specify file name
 *
 */
main( int argc, char *argv[] ) 
{
	int		c, devcount = 0, volcount = 0;
	int		bandwidth = 0, rotation = 0;
	int		devpath = 0, convert = 0, full = 0;
	int		cmdcount = 0, filecount= 0, fileresv = 0;
	int		alldisks = 0, statcmd = 0, delete = 0;
	int		streaminfo = 0, strcount = 0;
	int		shownodes = 0;
	device_list_t	devlist[MAX_NUM_DEVS];
	device_list_t	vollist[MAX_NUM_DEVS];
	device_list_t	filelist[MAX_NUM_DEVS];
	stream_id_t	strlist[10];
	extern char	*optarg;

	/*
	 * get command line arguments.
	 */

	while ((c = getopt(argc, argv,"NDSIBCFRVPad:v:n:f:s:")) != EOF ) {
		switch (c) {
			case 'a':
				alldisks = 1;
				break;
			case 'B':
				bandwidth = 1;
				cmdcount++;
				break;
			case 'C':
				convert = 1;
				cmdcount++;
				break;
			case 'd':
				if (add_to_list(optarg, devlist, &devcount)) {
					usage();
				}
				break;
			case 'f':
				if (add_to_list(optarg, filelist, &filecount)) {
					usage();
				}
				break;
			case 'F':
				full = 1;
				cmdcount++;
				break;
			case 'R':
				fileresv = 1;
				cmdcount++;
				break;
			case 'S':
				streaminfo = 1;
				cmdcount++;
				break;
			case 'D':
				if (getuid() != 0 ) {
					printf("Only superuser can delete stream ids.\n");
					exit( -1 );
				}
				delete = 1;
				cmdcount++;
				break;
			case 'I':
				statcmd = 1;
				cmdcount++;
				break;
			case 'N':
				shownodes = 1;
				cmdcount++;
				break;
			case 's':
				if (add_id_to_list(optarg,strlist,&strcount)) {
					usage();
				}
				break;
			case 'P':
				devpath = 1;
				cmdcount++;
				break;
			case 'V':
				rotation = 1;
				cmdcount++;
				break;
			case 'v':
				if (add_to_list(optarg, vollist, &volcount)) {
					usage();
				}
				break;
			default:
				usage();
				break;
		}
	}

	if ( get_ggd_info( ) ) {
		exit( -1 );
	}

	/*
	 * Assume stat command, if no other is given.
	 *
	 * If command is specified, check that one and only one 
	 * command is given.
 	 */
	if ( cmdcount == 0 ) {
		statcmd = 1;
		cmdcount = 1;
	} else if  ( cmdcount != 1 ) {
		usage();
	}


	/*
	 * Display ggd_info nodes.
 	 */
	if ( shownodes ) {
		show_ggd_info_nodes();
	}

	/*
	 * Display global grio status information
	 */
	if ( statcmd ) {
		show_command_stats();
	}

	/*
	 * Display global grio streams information	
	 */
	if ( streaminfo ) {
		get_all_streams();
	}

	/*
 	 * Display the bandwidth information the given devices.
 	 */
	if (bandwidth) {
		if ( alldisks || (!devcount) ) {
			show_all_bandwidth( );
		} else {
			if (devcount) {
				if (show_bandwidth_for_devs(
						devlist, devcount) ) {
					exit( -1 );
				}
			}
		}
	}

	/*
	 * Display the bandwdidth information for the given XLV volume.
 	 */
	if (rotation) {
		if (!volcount) {
			printf("No volumes specified. \n");
			printf("Use -v option to specify logical volumes.\n");
			usage();
		}
		if (show_rotation(vollist, volcount)) {
			exit( -1 );
		}
	}

	/*
	 * Display the full bandwdidth information for the given XLV volume.
 	 */
	if (full) {
		if (!volcount) {
			printf("No volumes specified. \n");
			printf("Use -v option to specify logical volumes.\n");
			usage();
		}
		if (show_full(vollist, volcount)) {
			exit( -1 );
		}
	}

	/*
	 * Delete the given streams
	 */
	if ( delete ) {
		if (!strcount) {
			printf("No stream identifiers specified.\n");
			printf("Use -s option to specify stream id.\n");
			usage();
		}
		if ( delete_streams( strlist, strcount ) ) {
			exit( -1 );
		}
	}

	/*
 	 * Display the I/O path for the given disk devices.
 	 */
	if (devpath) {
		if (!devcount) {
			printf("No devices specified. \n");
			printf("Use -d option to specify disk devices or \n");
			printf("-n option to specify guaranteed rate "
				"devices.\n");
			usage();
		} else {
			if (show_path_for_devs(devlist, devcount)) {
				exit( -1 );
			}
		}
	}

	/*
	 * Convert the given disk devices to their constituent I/O
	 * paths and display the device bandwidths.
	 */
	if (convert) {
		if (!devcount) {
			printf("No disks specified.\n");
			printf("Use -d option to specify disk devices.\n");
			usage();
		}
		if (convert_and_show_bandwidth( devlist, devcount)) {
			exit( -1 );
		}
	}

	/*
	 * Display the pid numbers of the processes that have
	 * reservations on these files.
 	 */
	if (fileresv) {
		if (!filecount) {
			printf("No files specified.\n");
			printf("Use -f option to specify files. \n");
			usage();
		} 
		if (show_file_reservations( filelist, filecount)) {
			exit( -1 );
		}
	}

	return( 0 );
}

