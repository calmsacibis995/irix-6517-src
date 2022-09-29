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

#ident	"$Revision: 1.3 $ $Author: sfc $"
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
	char *m1;
	int fd, i;
	FILE	*f;
	struct stat s1, s2, s3;
	char *filename;
	int errors = 0;
	char c;

	if (argc > 1)
		filename = argv[1];
	else
		filename = tempnam(NULL, "mstatYY");

	stat(filename, &s1);
	if (errno && errno != ENOENT) {
		perror("stat");
		exit(1);
	}
	errno = 0;

	printf("Creating %s\n", filename);
	f = fopen(filename, "w+");
	if (f == NULL) {
		perror("fopen");
		exit(1);
	}

	/*
	 * Create a file of 64 bytes
	 */
	if (fwrite(pattern, 1, BPL, f) != BPL) {
		perror("fwrite");
		unlink(filename);
		exit(1);
	}
	fclose(f);

	if (chmod(filename, 0777)) {
		perror("chmod");
		unlink(filename);
		exit(1);
	}

	fd = open(filename, O_RDWR);
	if (fd < 0) {
		perror("open");
		unlink(filename);
		exit(1);
	}

	if (unlink(filename)) {
		perror("unlink");
		exit(1);
	}

	m1 = mmap(0, BPL, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (m1 == (caddr_t) -1L) {
		perror("mmap");
		exit(1);
	} else
		printf("map returned 0x%x\n", m1);

	if (fstat(fd, &s1)) {
		perror("stat");
		exit(1);
	}

	sleep(1);

	c = *(volatile char *)m1;

	if (fstat(fd, &s2)) {
		perror("stat");
		exit(1);
	}

	if (s1.st_atime == s2.st_atime) {
		fprintf(stderr, "%s: FAILURE: inode access time not updated 0x%x\n",
			argv[0], s1.st_atime);
		errors++;
	}

	for (i = 0; i < BPL; i++)
		m1[i] = i & 0xff;

	sleep(1);

	if (fstat(fd, &s3)) {
		perror("stat");
	}

	if (s3.st_mtime == s2.st_mtime) {
		fprintf(stderr, "%s: FAILURE: inode modify time not updated\n",
			argv[0]);
		errors++;
	}

	if (s3.st_ctime == s2.st_ctime) {
		fprintf(stderr, "%s: FAILURE: inode change time not updated\n",
			argv[0]);
		errors++;
	}

	if (!errors)
		printf("%s: TEST PASSED!\n", argv[0]);
}
