
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#define	MIN_LOOP_COUNT	1000
#define	MAX_LOOP_COUNT	10

main(int argc, char *argv[])
{
	int		f;
	int		n;
	int		bytes;
	char		*buf;
	off_t		off;
	char		*tmpdir;
	char		filepath[L_tmpnam];
	struct dioattr	dioinfo;
	int		x;
	int		i;
	int		*ip;
	char		*filename;

	tmpdir = getenv("TMPDIR");
	if (tmpdir == NULL) {
		tmpdir = "/var/tmp";
	}

	filename = tempnam(tmpdir, "diotestXXXXXX");

	if ((f = open(filename, O_CREAT|O_TRUNC|O_RDWR|O_DIRECT, 0666)) < 0) {
		printf("ERROR: Cannot create testfile %s.  Error is %d\n",
		       filename, errno);
		exit(1);
	}

	if (fcntl(f, F_DIOINFO, &dioinfo) < 0) {
		printf("ERROR: Cannot get direct I/O info.  Error is %d\n",
		       errno);
		exit(1);
	}

	unlink(filename);

	buf = (char *)memalign(dioinfo.d_mem, dioinfo.d_maxiosz);

	for (x = 0; x < MIN_LOOP_COUNT; x++) {
		for (i = 0, ip= (int*)buf;
			     i < dioinfo.d_miniosz / 4; i++, ip++) {
			*ip = (int)(ip);
		}
		if ((bytes = write(f, buf, dioinfo.d_miniosz)) !=
		    dioinfo.d_miniosz) {
			printf("ERROR: Direct write of %d bytes returned %d.  Error is %d\n",
			       dioinfo.d_miniosz, bytes, errno);
			exit(1);
		}
	}

	off = lseek(f, 0L, SEEK_SET);
	if (off != 0) {
		printf("ERROR: Seek to 0 returned %d.  Error is %d\n",
		       off, errno);
		exit(1);
	}

	for (x = 0; x < MIN_LOOP_COUNT; x++) {
		if ((bytes = read(f, buf, dioinfo.d_miniosz)) !=
		    dioinfo.d_miniosz) {
			printf("ERROR: Direct read of %d bytes returned %d.  Error is %d\n",
			       dioinfo.d_miniosz, bytes, errno);
			exit(1);
		}
		for (i = 0, ip= (int*)buf;
		     i < dioinfo.d_miniosz / 4;
		     i++, ip++) {
			if (!(*ip == (int)ip)) {
				printf("ERROR: Direct read (miniosz) returned bad data\n");
				printf("address 0x%x expected 0x%x actual 0x%x\n",
					ip, x, *ip);
				exit(1);
			}
		}
	}

	if ((bytes = read(f, buf, dioinfo.d_miniosz)) != 0) {	
		printf("ERROR: Direct read of %d bytes at EOF returned %d.  Error is %d\n",
			dioinfo.d_miniosz, bytes, errno);
			exit(1);
	}

	off = lseek(f, 0L, SEEK_SET);
	for (x = 0; x < MAX_LOOP_COUNT; x++) {
		for (i = 0, ip= (int*)buf;
		     i < dioinfo.d_maxiosz / 4;
		     i++, ip++) {
			*ip = (int)ip;
		}
		if ((bytes = write(f, buf, dioinfo.d_maxiosz)) !=
		    dioinfo.d_maxiosz) {
			printf("ERROR: Direct write of %d bytes returned %d.  Error is %d\n",
			       dioinfo.d_maxiosz, bytes, errno);
			exit(1);
		}
	}

	off = lseek(f, 0L, SEEK_SET);
	if (off != 0) {
		printf("ERROR: Seek to 0 returned %d.  Error is %d\n",
		       off, errno);
		exit(1);
	}

	for (x = 0; x < MAX_LOOP_COUNT; x++) {
		if ((bytes = read(f, buf, dioinfo.d_maxiosz)) !=
		    dioinfo.d_maxiosz) {
			printf("ERROR: Direct read of %d bytes returned %d.  Error is %d\n",
			       dioinfo.d_maxiosz, bytes, errno);
			exit(1);
		}
		for (i = 0, ip= (int*)buf;
		     i < dioinfo.d_maxiosz / 4;
		     i++, ip++) {
			if (!(*ip == (int)ip)) {
				printf("ERROR: Direct read (maxiosz) returned bad data\n");
				exit(1);
			}
		}
	}

	if ((bytes = read(f, buf, dioinfo.d_maxiosz)) != 0) {	
		printf("ERROR: Direct read of %d bytes at EOF returned %d.  Error is %d\n",
			dioinfo.d_maxiosz, bytes, errno);
			exit(1);
	}

	exit(0);
}
