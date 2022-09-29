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

#ident	"$Revision: 1.2 $ $Author: sfc $"

/*
 *	Test that mapping file object beyond its size
 *	without MAP_AUTOGROW flag will fail.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <sys/mman.h>

#define NL 164
#define BPL 64

char	*pattern = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
char	buf[NL*BPL];

main(argc, argv)
char **argv;
{
	int fd;
	long i;
	FILE	*f;
	struct stat statbuf;
	char *filename;

	if (argc > 1)
		filename = argv[1];
	else
		filename = tempnam(NULL, "noag");

	stat(filename, &statbuf);
	if (errno && errno != ENOENT) {
		perror("mmap stat");
		exit(1);
	}
	errno = 0;

	printf("Creating %s\n", filename);
	f = fopen(filename, "w+");
	if (f == NULL) {
		perror("mmap test, fopen");
		exit(1);
	}

	/*
	 * Create a file of 64 bytes
	 */
	if (fwrite(pattern, 1, BPL, f) != BPL) {
		perror("mmap test, fwrite");
		unlink(filename);
		exit(1);
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

	i = (long) mmap(0, NL*BPL, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (i > 0) {
		fprintf(stderr,
			"%s: maping beyond file size (incorrectly) worked!\n",
			argv[0]);
		exit(1);
	}

	printf("%s passed!\n", argv[0]);
}
