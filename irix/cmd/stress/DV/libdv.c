/*
 * Copyright 1990-1998 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */
#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

char buf[BUFSIZ*8];
int buflen = sizeof(buf);

int
dodv(int s1, int s2)
{
	struct stat sb;
	char file[MAXPATHLEN];
	int fd;
	FILE *fp, *fp2;
	int r;
	int remsize;
	void *p;
	int written = 0;
	int readcnt = 0;
	char *p2;
	int i;
	int nbytes;

	fp2 = fdopen(s1, "r");
	if (fp2 == 0) {
		perror("fdopen");
		return -1;
	}
	p2 = fgets(file, sizeof(file), fp2);
	if (p2 == 0) {
		perror("fgets");
		return -1;
	}

	if (p2 = strchr(p2, '\n')) {
		*p2 = '\0';
	}

	if (rand() & 1) {
		fp = fopen(file, "r");
		if (fp == 0) {
			perror(file);
			return -1;
		}
		r = fstat(fileno(fp), &sb);
		if (r < 0) {
			perror("fstat 1");
			return -1;
		}
		while ((r = fread(buf, 1, sizeof(buf), fp)) > 0) {
			readcnt += r;
			r = write(s2, buf, r);
			if (r < 0) {
				perror("write");
				return -1;
			}
			written += r;
		}
		assert(readcnt == sb.st_size);
		assert(written == sb.st_size);
		(void)fclose(fp);
	} else {
		int niov;
		struct iovec iov[5];

		niov = rand() % 5;
		if (niov == 0)
			niov = 1;
		assert(niov < 5);
		fd = open(file, O_RDONLY, 0);
		if (fd < 0) {
			perror(file);
			return -1;
		}
		r = fstat(fd, &sb);
		if (r < 0) {
			perror("fstat");
			return -1;
		}
		nbytes = sb.st_size / niov;
		if (nbytes == 0)
			nbytes = sb.st_size;
		remsize = sb.st_size;
		p = mmap((void *)0, sb.st_size, PROT_READ|PROT_WRITE,
			MAP_PRIVATE, fd, 0);
		if (p == MAP_FAILED) {
			perror("mmap");
			return -1;
		}

		p2 = p;
		for (i = 0; i <= niov; i++) {
			int cur = nbytes < remsize ? nbytes : remsize;
			iov[i].iov_base = p2;
			p2 += cur;
			iov[i].iov_len = cur;
			remsize -= cur;
			if (remsize == 0)
				break;
		}
		r = writev(s2, iov, i + 1);
		if (r < 0) {
			perror("write");
			return -1;
		}
		assert(r == sb.st_size);
		munmap(p, sb.st_size);
		(void) close(fd);
	}
	return 0;
}
