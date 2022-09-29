/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

main()
{
	int fd = open("/dev/frsdrv/1", 2);

	if (fd < 0) {
		perror("open");
		exit(-1);
	}

	if (ioctl(fd, 0, 0) < 0) {
		perror("ioctl");
		exit(-1);
	}

	if (close(fd) < 0) {
		perror("close");
		exit(-1);
	}
}
