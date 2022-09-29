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
#ident "$Revision: 1.7 $"

/*
 * Test driver for xlv_plexd.
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/debug.h>
#include <sys/wait.h>
#include <sys/dkio.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <xlv_utils.h>
#include <xlv_plexd.h>

int
main (int argc, char **argv)
{
	int			fd, nwrite;
	u_int                   i, j;
	xlv_plexd_request_t	xlv_plexd_request;
	u_int 			num, st;
	uuid_t			subvol;
	char 			*str;

	if (argc > 1)
		sscanf (argv[1], "%d", &xlv_plexd_request.num_requests);
	else
		xlv_plexd_request.num_requests = 15;
	
	uuid_create(&subvol, &st);
	uuid_to_string(&subvol, &str, &st);
	for (i=0; i < xlv_plexd_request.num_requests / 3; i++) {
		for (j=0; j<3; j++) {
			sprintf (xlv_plexd_request.request[i*3+j].subvol_name,
				 "%s_%d", str, i);
			num = i * 3 * 100 + j * 100;
			xlv_plexd_request.request[i*3+j].start_blkno = (long)num;
			xlv_plexd_request.request[i*3+j].end_blkno = (long)(num + 99);
			xlv_plexd_request.request[i*3+j].device = 
				(dev_t)(i*3+j);
			printf( "%s\n", 
			       xlv_plexd_request.request[i*3+j].subvol_name );
			num++;
		}
	}
	
	if ((fd = open (XLV_PLEXD_RQST_FIFO, O_WRONLY)) == -1) {
		perror ("xlv_plexd_test, open failed");
		exit (-1);
	}

	nwrite = write (fd, &xlv_plexd_request, sizeof(xlv_plexd_request));
	if (nwrite != (int)sizeof(xlv_plexd_request)) {
		close (fd);
		perror ("xlv_plexd_test, write failed");
		exit (-1);
	}

	close (fd);
	return 1;
}



