/*
 *  prio_cp
 *  Copies one file to a second file.
 *
 *  usage: prio_cp bytes_per_second buffer_size f1 f2
 *
 *  This is a completely contrived example, but it does show usage of all
 *  of the Priority I/O Bandwidth Allocation API.
 *
 *  Compile with:
 *	cc -O -o prio_cp prio_cp.c
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <task.h>
#include <errno.h>
#include <sys/prio.h>


main(int argc, char **argv)
{
    int ifd, ofd;
    struct stat stat_buf;
    pid_t lock_holder;
    int bytes_copied;
    int size, error;
    int buf_size;
    char *buf;
    bandwidth_t bytes_per_second;
    bandwidth_t read_bps, write_bps;
    int arg;
    struct dioattr dioinfo;

    if (argc != 5)
    {
	fprintf(stderr, "Usage: prio_cp bytes_per_second buffer_size f1 f2\n");
	exit(1);
    }

    /*
     *  Open the files.
     */
    if ((ifd = open(argv[3], O_RDONLY)) == -1)
    {
	fprintf(stderr, "prio_cp: unable to open \"%s\" for reading.\n",
		argv[3]);
	exit(1);
    }

    if (fcntl(ifd, F_GETFL, &arg) < 0) {
	fprintf(stderr, "prio_cp: unable to do F_GETFL fcntl on \"%s\" \n",
		argv[3]);
	exit(1);
    }
    arg |= FDIRECT;
    if (fcntl(ifd, F_SETFL, arg) < 0) {
	fprintf(stderr, "prio_cp: unable to do F_SETFL fcntl on \"%s\" \n",
		argv[3]);
	exit(1);
    }
    fstat(ifd, &stat_buf);

    if (fcntl(ifd, F_DIOINFO, &dioinfo) < 0) {
	fprintf(stderr, "prio_cp: unable to do F_DIOINFO fcntl on \"%s\" \n",
		argv[3]);
	exit(1);
    }


    if ((ofd = open(argv[4], O_WRONLY | O_CREAT | O_TRUNC , 0644)) == -1)
    {
	fprintf(stderr, "prio_cp: unable to open \"%s\" for writing.\n",
		argv[4]);
	exit(1);
    }

    if (fcntl(ofd, F_GETFL, &arg) < 0) {
	fprintf(stderr, "prio_cp: unable to do F_GETFL fcntl on \"%s\" \n",
		argv[4]);
	exit(1);
    }
    arg |= FDIRECT;
    if (fcntl(ofd, F_SETFL, arg) < 0) {
	fprintf(stderr, "prio_cp: unable to do F_SETFL fcntl on \"%s\" \n",
		argv[4]);
	exit(1);
    }

    if (fcntl(ofd, F_DIOINFO, &dioinfo) < 0) {
	fprintf(stderr, "prio_cp: unable to do F_DIOINFO fcntl on \"%s\" \n",
		argv[4]);
	exit(1);
    }

    bytes_per_second = atoi(argv[1]);
    buf_size = atoi(argv[2]);
    if ((buf_size % dioinfo.d_miniosz) != 0) {
	fprintf(stderr, "prio_cp: buffer not block size aligned\n");
	exit(1);
    }

    if ((buf = valloc(buf_size)) == NULL)
    {
	fprintf(stderr, "prio_cp: unable malloc a buffer of %d bytes.\n",
		buf_size);
	exit(1);
    }

    /*
     *  Allocate bandwidth for read operations on the input file
     *  and report any problems with allocation.
     */
    if (prioSetBandwidth(ifd, PRIO_READ_ALLOCATE,
			   bytes_per_second, &lock_holder))
    {
	fprintf(stderr, "Bandwidth Allocation failed beacause ");
	switch (errno)
	{
	    case ENOPKG:
		fprintf(stderr, "it is not supported on this sytem.\n");
		break;
	    case E2BIG:
		fprintf(stderr, "there is not enough available bandwidth.\n");
		break;
	    case EBADFD:
		fprintf(stderr, "an invalid file descriptor was supplied.\n");
		break;
	    case EINVAL:
		fprintf(stderr, "one or more of the parameters are invalid.\n");
		break;
	    case ENOLCK:
		fprintf(stderr, "process %d is currently holding on lock.\n",
			lock_holder);
		break;
	}
    }

    prioGetBandwidth(ifd, &read_bps, &write_bps);

    bytes_copied = 0;
    while (1)
    {
	size = read(ifd, buf, buf_size);
	if (size < 0) {
		fprintf(stderr, "read returns %d\n", size);
		if (errno == EINVAL)
			fprintf(stderr, "Maybe the DIRECT I/O requirements are not met. Please see fcntl(2)--F_DIOINFO\n");
	}

	prioLock(&lock_holder);
	prioSetBandwidth(ifd, PRIO_READ_ALLOCATE, 0, &lock_holder);
	prioSetBandwidth(ofd, PRIO_WRITE_ALLOCATE, bytes_per_second,
			   &lock_holder);
	prioUnlock();

	bytes_copied += size;
	error = write(ofd, buf, size);
	if (error < 0) {
		fprintf(stderr, "write returns %d\n", error);
		if (errno == EINVAL)
			fprintf(stderr, "Maybe the DIRECT I/O requirements are not met. Please see fcntl(2)--F_DIOINFO\n");
	}

	if (bytes_copied >= stat_buf.st_size)
	{
	    close(ifd);
	    fchmod(ofd, stat_buf.st_mode);
	    close(ofd);
	    exit(0);
	}

	prioLock(&lock_holder);
	prioSetBandwidth(ofd, PRIO_WRITE_ALLOCATE, 0, &lock_holder);
	prioSetBandwidth(ifd, PRIO_READ_ALLOCATE, bytes_per_second,
			   &lock_holder);
	prioUnlock();
    }
}
