/*
||	Program to test shm_open(3).
||		shm_open	[-p <perms>] [-s <bytes>] [-c] [-x] [-r] [-t] [-w] <path>
||		-p <perms>	access mode to use when creating, default 0600
||		-s <bytes>	size of segment to map, default 64K
||		-c			use O_CREAT
||		-x			use O_EXCL
||		-r			use O_RDONLY, default is O_RDWR
||		-t			use O_TRUNC
||		-w			wait for keyboard input before exiting
||		<path>		the pathname of the queue, required
*/
#include <sys/mman.h>	/* shared memory and mmap() */
#include <unistd.h>		/* for getopt() */
#include <errno.h>		/* errno and perror */
#include <fcntl.h>		/* O_flags */
#include <stdio.h>
int main(int argc, char **argv)
{
	int perms = 0600;			/* permissions */
	size_t size = 65536;		/* segment size */
	int oflags = 0;				/* open flags receives -c, -x, -t */
	int ropt = 0;				/* -r option seen */
	int wopt = 0;				/* -w option seen */
	int shm_fd;					/* file descriptor */
	int mprot = PROT_READ;		/* protection flags to mmap */
	int mflags = MAP_SHARED; 	/* mmap flags */
	void *attach;				/* assigned memory adddress */
	char *path;					/* ->first non-option argument */
	int c;
	while ( -1 != (c = getopt(argc,argv,"p:s:cxrtw")) )
	{
		switch (c)
		{
		case 'p': /* permissions */
			perms = (int) strtoul(optarg, NULL, 0);
			break;
		case 's': /* segment size */
			size = (size_t) strtoul(optarg, NULL, 0);
			break;
		case 'c': /* use O_CREAT */
			oflags |= O_CREAT;
			break;
		case 'x': /* use O_EXCL */
			oflags |= O_EXCL;
			break;
		case 't': /* use O_TRUNC */
			oflags |= O_TRUNC;
			break;
		case 'r': /* use O_RDONLY */
			ropt = 1;
			break;
		case 'w': /* wait after attaching */
			wopt = 1;
			break;
		default: /* unknown or missing argument */
			return -1;      
		} /* switch */
	} /* while */
	if (optind < argc)
		path = argv[optind]; /* first non-option argument */
	else
		{ printf("Segment pathname required\n"); return -1; }
	if (0==ropt)
	{ /* read-write access, reflect in mprot and mflags */
		oflags |= O_RDWR;
		mprot |= PROT_WRITE;
		mflags |= MAP_AUTOGROW + MAP_AUTORESRV;		
	}
	else
	{ /* read-only access, mprot and mflags defaults ok */
		oflags |= O_RDONLY;
	}
	shm_fd = shm_open(path,oflags,perms);
	if (-1 != shm_fd)
	{
		attach = mmap(NULL,size,mprot,mflags,shm_fd,(off_t)0);
		if (attach != MAP_FAILED) /* mmap worked */
		{
			printf("Attached at 0x%lx, first word = 0x%lx\n",
								attach,		 *((pid_t*)attach));
			if (mprot & PROT_WRITE)
			{
				*((pid_t *)attach) = getpid();
				printf("Set first word to 0x%lx\n",*((pid_t*)attach));
			}
			if (wopt) /* wait a while, report possibly-different value */
			{
				char inp[80];
				printf("Waiting for return key before unmapping...");
				gets(inp);
				printf("First word is now 0x%lx\n",*((pid_t*)attach));
			}
			if (munmap(attach,size))
				perror("munmap()");
		}
		else
			perror("mmap()");
	}
	else
		perror("shm_open()");
	return errno;
}
