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

#ident	"$Revision: 1.4 $ $Author: sfc $"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <sys/mman.h>

#define NL 164
#define BPL 64

char	*filename;
char	*pattern = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
char	buf[NL*BPL];

main(argc, argv)
char **argv;
{
	char *m1;
	int fd, i;
	FILE	*f;
	struct stat statbuf;

	if (argc > 1)
		filename = argv[1];
	else
		filename = tempnam(NULL, "agfile");

	if (stat(filename, &statbuf) && errno != ENOENT) {
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
	if (chmod(filename, 0777)) {
		perror("mmap test, chmod");
		unlink(filename);
		exit(1);
	}

	if (stat(filename, &statbuf)) {
		perror("stat");
		unlink(filename);
		exit(1);
	}
	if (statbuf.st_size != BPL) {
		printf("%s stat size %d, should be %d\n",
			filename, statbuf.st_size, BPL);
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

	m1 = mmap(0, NL*BPL, PROT_READ|PROT_WRITE,
		MAP_PRIVATE|MAP_AUTOGROW, fd, 0);
	if (m1 == (caddr_t) -1L) {
		perror("autogrow test, map");
		exit(1);
	} else
		printf("map returned 0x%x\n", m1);

	for (i = 0; i < BPL; i++) {
		m1[i] = i & 0xff;
	}
	for (i = BPL*2; i < NL*BPL; i++) {
		m1[i] = i & 0xff;
	}
	if (msync(m1, NL*BPL, 0)) {
		perror("mmap test, msync");
		exit(1);
	}
	/*
	 * The mmap(PRIVATE|AUTOGROW) should neither be able
	 * to grow the file nor to change it's backing store,
	 * msync notwithstanding.
	 */
	if (lseek(fd, 0, 0) < 0) {
		perror("lseek");
		exit(1);
	}
	i = read(fd, buf, NL*BPL);
	if (i != BPL) {
		printf("read returned %d chars, not %d\n", i, BPL);
		perror("read");
		exit(1);
	}
	for (i = 0; i < BPL; i++) {
		if (buf[i] != 'X') {
			printf("autogrow: overwrite valid data (read)!\n");
			exit(1);
		}
		if (m1[i] != (i & 0xff)) {
			printf("autogrow: overwrite valid data (mapped)!\n");
			exit(1);
		}
	}
	for ( ; i < BPL*2; i++) {
		if (m1[i] != '\0') {
			printf("autogrow: gap (%d) was %d, not 0 (mapped)!\n",
				i, buf[i]);
			exit(1);
		}
	}

	printf("test passed! -- remapped at 0x%x\n", m1);
}
