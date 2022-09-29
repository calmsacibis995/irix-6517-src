/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.2 $ $Author: jwag $"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>

char *filename;
char		*pattern = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
#define NLINES 64

main(argc, argv)
char **argv;
{
	char *m1, *m2;
	int fd, i;
	FILE	*f;
	struct stat statbuf;

	if (argc > 1)
		filename = argv[1];
	else
		filename = tempnam(NULL, "tfile");
	printf("Implicit i/o test m1, using file %s\n", filename);

	stat(filename, &statbuf);
	if (errno && errno != ENOENT) {
		perror("mmap stat");
		exit(1);
	}

	printf("Creating %s\n", filename);
	f = fopen(filename, "w+");
	if (f == NULL) {
		perror("mmap test, fopen");
		unlink(filename);
		exit(1);
	}

	/*
	 * Create a file of 4096 bytes
	 */
	for (i = 0 ; i < NLINES ; i++) {
		if (fwrite(pattern, 1, 64, f) != 64) {
			perror("mmap test, fwrite");
			unlink(filename);
			exit(1);
		}
	}
	fclose(f);

	errno = 0;
	chmod(filename, 0777);
	if (errno) {
		perror("mmap test, chmod");
		unlink(filename);
		exit(1);
	}

	fd = open(filename, O_RDWR);
	if (errno) {
		perror("mmap test, open");
		unlink(filename);
		exit(1);
	}
	unlink(filename);
	if (errno) {
		perror("mmap test, unlink");
		exit(1);
	}

	m1 = mmap(0, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (errno) {
		perror("mmap test, map");
		exit(1);
	} else
		printf("map returned 0x%x\n", m1);

	m2 = mmap(m1, 4096, PROT_READ|PROT_WRITE, MAP_FIXED|MAP_SHARED, fd, 0);
	if (errno) {
		perror("mmap test, map fixed (second mapping)");
		exit(1);
	}
	*m2 = 'y';
	close(fd);
	sleep(1);
	*m2 = 'z';
	printf("test passed! -- remapped at 0x%x\n", m2);
}
