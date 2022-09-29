/*
 *	simple diskio test
 *
 *	-c - create file, truncating if preexists.
 *	-d - "duplicate": copy file1 to file2.
 *	-D - use Direct I/O
 *	-g - catch signal SIGUSR1 and terminate
 *	-i - interactive (asks before open or r/w)
 *	-m - just copy memory to memory
 *	-M - pin down the memory
 *	-r - random (instead of sequential)
 *	-S - use syssgi calls instead of read/write/lseek
 *	-t - prints summary on termination.
 *	-v - verbose: prints blocknumber on each I/O
 *      -V - verbose: prints parameters of test
 *	-w - write instead of read
 *	-z - read file backwards
 *
 *	-a - align the transfers on a given boundary (default 0x4000)
 *	-A - use async I/O
 *	-b - blocksize
 *	-e - misalign by this amount (add to align argument (-a))
 *	-f - filename (backward compatibility)
 *	-n - repeats
 *	-o - starting offset in 512-byte blocks.
 *	-p - name of synchronizing pipe from master.
 *	-P - name of synchronizing pipe to master.
 *	-s - file size in 512-byte blocks
 *      -N - use following node numbers for roundrobin data placement
 *      -I - sequential forward seek in 512-byte blocks
 *      -R - this a realtime file.
 */

#include 	<stdio.h>
#include	<varargs.h>
#include	<fcntl.h>
#include	<sys/types.h>
#include	<sys/time.h>
#include	<sys/times.h>
#include	<sys/param.h>
#include	<sys/stat.h>
#include	<sys/signal.h>
#include	<aio.h>
#include	<sys/syssgi.h>
#include        <sys/pmo.h>
#include	<sys/uuid.h>
#include        <sys/fs/xfs_fsops.h>
#include        <sys/fs/xfs_itable.h>

#define MAXASYNC	100

#define TIMEMAX  0
#define TIMEBINS 100

void
usage()
{
fprintf(stderr,
"usage: sio [-cdDgimMrStvVwzR] [-a align] [-A aiocnt] [-b blocksize in blocks]\n"
"           [-e misalign] [-n repeats] [-o start offset in blocks]\n"
"           [-I sequential forward seek in blocks]\n"
"           [-P sio_mgr_pipe -p mgr_sio_pipe] [-s total number of blocks]\n"
"           [-N node0[:node1[:node2 [... :noden]]]\n"
"           filename [copyfilename]\n");
exit(1);
/*NOTREACHED*/
}

extern int errno;
extern void *memalign();
extern void do_async(int, int, unsigned, int, int, int);
int parse_nodenums(char *arg, int *nodenums, char ***nodes);
void numa_placement(int numnodes, char *node_list[]);

double et;

char abuf[100];
unsigned where;
int bsize = 8;
int reqsize = 0;
struct stat sb;
unsigned long total = 0;

int		pinning = 0;

struct tms	timebuf;
int		time0, time1;
int 		timeit = 0;

void
printwhere()
{
	fprintf(stderr, "block %d\n", where);
}

void
scat()
{
	fprintf(stderr,"SIGPIPE!\n");
}

void *
getmemory(int align, int amount)
{
	void	*ret;
	int	 i;

	ret = memalign(align, amount);
	if (ret != NULL)
	{
		for (i = 0; i < amount; i += 4096)
			((char *) ret)[i] = 0;
	}
	return ret;
}


int
siorand()
{
	return (random());
}


int sigstop = 0;
void
sigcatch(int sig, int code, void *sc)
{
	sigstop = 1;
}


aiocb64_t aios[MAXASYNC];
aiocb64_t *aiop[MAXASYNC];

static int
sio_aio_init(char *fname, int flags, int align, int misalign, int async, int realtime)
{
	int		fd;
	char		*buf;
	int		i;
	aioinit_t	aioinit;
	struct fsxattr  fsxattr;

	for (i = 0 ; i < async ; i++) {
		if ((fd = open(fname,flags)) < 0) {
			perror("async open");
			return 1;
		}
		if (realtime)
		{ 
			bzero(&fsxattr, sizeof(fsxattr));
			fsxattr.fsx_xflags = XFS_XFLAG_REALTIME; 
			if (fcntl(fd, F_FSSETXATTR, &fsxattr) < 0) 
			{ 
				perror("fcntl(F_FSSETXATTR) failed");
				return(1);
			}
		}
		aios[i].aio_fildes = fd;
		aios[i].aio_nbytes = bsize * 512;
		aios[i].aio_reqprio = 0;
		aios[i].aio_sigevent.sigev_notify = SIGEV_NONE;
		buf = getmemory(align, bsize * 512 + misalign);
		if (buf == NULL) {
			perror("getmemory failed on async buf\n");
			return 1;
		}
		if (misalign) {
			buf = &buf[misalign];
			printf("misaligned thread %d buf at 0x%lx\n", i, buf);
		}
		aios[i].aio_buf = buf;
		if (pinning) {
			if (mpin(aios[i].aio_buf, bsize * 512 + misalign)) {
				fprintf(stderr,"mpin failed!?\n");
				perror("");
				exit(1);
			}
		}
		aiop[i] = &aios[i];
	}
	bzero((char *)&aioinit, sizeof(aioinit));
	aioinit.aio_threads = async;
	aioinit.aio_locks = 3;
	aioinit.aio_num = 1000;
	aioinit.aio_numusers = async+1;
	aio_sgi_init(&aioinit);

	return 0;
}

void
do_async(int nblox, int backwards, unsigned fsize, int async, int offset, int oflags)
{
	int err;
	off_t pos;
	int received, sent;
	int step;

	received = sent = 0;

	if (backwards) {
		pos = fsize - bsize * 512;
		step = -bsize * 512;
	}
	else {
		step = bsize * 512;
		pos = offset * 512;
	}

	/* send off the initial ios */
	while ((sent < nblox) && (sent < async)) {
		aios[sent].aio_offset = pos;
		pos += step;
		if (oflags == O_RDONLY)
		{
			if (aio_read64(aiop[sent])) {
				perror("aio_read");
				exit(1);
			}
		}
		else
		{
			if (aio_write64(aiop[sent])) {
				perror("aio_write");
				exit(1);
			}
		}
		sent++;
	}

	/* now pop and send */
	while (received < sent) {

		/* wait for one to complete */
		if (aio_suspend((const aiocb_t **)&aiop[received % async], 1, NULL) < 0)
			perror("aio_suspend");
		if (err = aio_error64(aiop[received % async])) {
			fprintf(stderr, "aio failure, errno %d\n", err);
			exit(1);
		}
		if (sent < nblox) {
			aios[received % async].aio_offset = pos;
			pos += step;
			if (oflags == O_RDONLY)
			{
				if (aio_read64(aiop[received % async])) {
					perror("aio_read");
					exit(1);
				}
			}
			else
			{
				if (aio_write64(aiop[received % async])) {
					perror("aio_write");
					exit(1);
				}
			}
			sent++;
		}
		total += bsize;
		received++;
	}
}

void
dirty_buffer(register char *buf, register int count)
{
	count -= 128;

	while (count > 0)
	{
		*((int *) &buf[count]) = count;
		count -= 128;
	}
}


void
main(argc, argv)
int		argc;
char		**argv;
{
	int		c;
	int		fd, fd2, ipfd, opfd;
	extern int	optind;
	extern char	*optarg;
	char		*buf, *obuf;
	char		*fname = NULL;
	char		*fname2 = NULL;
	unsigned long	fsize = 0;
	int 		repeats = 1;
	int		oflags;
	int		oflags2;
	int		random = 0;
	int		interact = 0;
	int		verbose = 0;
	int		Verbose = 0;
	int 		offset = 0;
	int             seq_forw_seek = 0;
	int 		backwards = 0;
	int		writing = 0;
	int		create = 0;
	int		direct = 0;
	int		async = 0;
	int		memory = 0;
	int		wilborn_wynn = 0;
	int		copy = 0;
	int		pipesync = 0;
	int		sigsio = 0;
	unsigned int	nblox;
	unsigned int	align = 0x4000;	/* default to one page */
	unsigned int	misalign = 0;
	char 		*inpipe = NULL;
	char 		*outpipe = NULL;
	unsigned 	before, after;
	int 		i;
	int 		ret;
	int		usesyssgi = 0;
	int             numnodes = 0;
	int             realtime = 0;
	char            **nodes;

#if TIMEMAX
	struct timeval timeb_s, timeb_e;
	unsigned long long time_s, time_e, time_diff;
	int timebins[TIMEBINS] = { 0 };
#endif

	/* XXX*/
	signal(SIGPIPE, scat);

	/*
	 * Parse the arguments.
	 */
	srandom(time(0) + getpid());
	while ((c = getopt(argc, argv, "cdDgimMrStvVwWzRA:a:e:b:f:n:o:p:P:s:N:I:")) != EOF)
	{
		switch (c) {
		case 'c':
			create = 1;
			break;
		case 'D':
			direct = 1;
			break;
		case 'd':
			copy = 1;
			break;
		case 'e':
			misalign = (unsigned)strtol(optarg, (char **) 0, 0);
			break;
		case 'g':
			signal(SIGUSR1, sigcatch);
			sigsio = 1;
			break;
		case 'i':
			interact = 1;
			break;
		case 'm':
			memory = 1;
			break;
		case 'r':
			random = 1;
			break;
		case 'S':
			usesyssgi = 1;
			break;
		case 't':
			timeit = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'V':
			Verbose = 1;
			break;
		case 'w':
			writing = 1;
			break;
		case 'W':
			wilborn_wynn = 1;
			break;
		case 'z':
			backwards = 1;
			break;
		case 'a':
			align = (unsigned)strtol(optarg, (char **) 0, 0);
			break;
		case 'A':
			async = strtol(optarg,(char **)0,0);
			break;
		case 'b':
			if ((bsize = strtol(optarg, (char **) 0, 0)) <= 0)
				usage();
			break;
		case 'f':
			fname = optarg;
			break;
		case 'n':
			if ((repeats = strtol(optarg, (char **) 0, 0)) <= 0)
				usage();
			break;
		case 'o':
			if ((offset = strtol(optarg, (char **) 0, 0)) < 0)
				usage();
			break;
		case 'p':
			inpipe = optarg;
			break;
		case 'P':
			outpipe = optarg;
			break;
		case 's':
			if ((fsize = strtol(optarg, (char **) 0, 0)) <= 0)
				usage();
			break;
		case 'M':
			pinning = 1;
			break;
		case 'N':
			parse_nodenums(optarg, &numnodes, &nodes);
			numa_placement(numnodes, nodes);
			break;
		case 'I':
			if ((seq_forw_seek = strtol(optarg, (char **) 0, 0)) < 0)
				usage();
			break;
		case 'R':
			realtime = 1;
		}
	}
	if (misalign && (misalign >= align))
	{
		fprintf(stderr,
			"misalign (0x%x) must be less than align (0x%x)\n",
		        misalign, align);
		usage();
	}

	if (Verbose)
	{
		if (random)
			printf("random");
		else
			printf("sequential");
		if (create)
			printf(" create,");
		else if (writing)
			printf(" write,");
		else
			printf(" read,");
		printf(" blocksize %d blocks,", bsize);
		if (offset)
			printf(" offset %d blocks,", offset);
		if (random)
			printf(" %d reps from %ld blocks\n", repeats, fsize);
		else
			printf(" total %ld blocks\n", fsize);
	}

	if (fname)
	{
		if (copy)
		{
			fprintf(stderr,"old-style -f arg incompatible with copy\n");
			exit(1);
		}
		if (inpipe || outpipe)
		{
			fprintf(stderr, "old-style -f arg incompatible with pipesync\n");
			exit(1);
		}
	}
	else if (!memory)
	{
		argc -= optind;
		if (argc <= 0) usage();
		fname = argv[optind];
		if (copy)
		{
			if (argc < 2)
			{
				fprintf(stderr,"Must specify destination file for copy.\n");
				exit(1);
			}
			fname2 = argv[optind + 1];
		}
	}

	if (async) {
		if (random) {
			(void)fprintf(stderr,"async only supports sequential acces\n");
			exit(1);
		}
		if (copy) {
			(void)fprintf(stderr,"async does not support copies\n");
			exit(1);
		}
		if (async > MAXASYNC) {
			(void)fprintf(stderr,"async io maxes out at %d\n",
			    MAXASYNC);
			exit(1);
		}
	}

	if (inpipe || outpipe)
	{
		if (!inpipe || !outpipe)
		{
			fprintf(stderr,"Both sync pipes must be specified\n");
			exit(1);
		}
		if ((ipfd = open(inpipe, O_RDONLY)) < 0)
		{
			fprintf(stderr,"Can't open %s\n",inpipe);
			exit(1);
		}
		if ((opfd = open(outpipe, O_WRONLY)) < 0)
		{
			fprintf(stderr,"Can't open %s\n",outpipe);
			exit(1);
		}
		pipesync = 1;
		if (interact)
		{
			fprintf(stderr,"Interactive mode not useful with pipesync.\n");
			exit(1);
		}
		if (verbose)
		{
			fprintf(stderr,"Verbose mode not useful with pipesync.\n");
			exit(1);
		}
	}

	if (offset && (create || copy || random))
	{
		fprintf(stderr,"Offset allowed only on sequential read\n");
		exit(1);
	}

	if (memory)
		fsize = bsize;

	if (!fsize)
	{
		if (create && !copy)
		{
			fprintf(stderr,"Must specify filesize to create.\n");
			exit(1);
		}
		timeit = 1;		/* reasonable default */
	}

	if (fsize < bsize)
	{
		if (offset)
			fprintf(stderr,"effective ");
		fprintf(stderr,"filesize %ld less than block size %d\n",
		    fsize, bsize);
		exit(1);
	}

	nblox = fsize / bsize;
	if (fsize % bsize)
	{
		fsize = nblox * bsize;
		fprintf(stderr, "Warning: filesize adjusted to %ld blocks\n",
			fsize);
	}

	if (!memory && !(create && !copy))
	{
		if (stat(fname, &sb) < 0)
		{
			fprintf(stderr,"Can't stat %s\n", fname);
			exit(1);
		}
		if ((sb.st_mode & S_IFMT) != S_IFREG)
		{
			if (!fsize)
			{
				fprintf(stderr, "must specify size if not"
					" regular file.\n");
				exit(1);
			}
		}
		else /* regular file */
		{
			if (fsize == 0)
			{
				fsize = sb.st_blocks;
				timeit = 1;
			}
			else if (fsize > sb.st_blocks && !sigsio)
			{
				fprintf(stderr, "specified size %ld is greater"
					" than actual file size %ld!\n",
					fsize, sb.st_blocks);
				exit(1);
			}
		}
	}

	if (random && (create || copy))
	{
		fprintf(stderr,"Random allowed only on read or write\n");
		exit(1);
	}

	if (copy && writing)
	{
		fprintf(stderr,"Incompatible options: copy & write!\n");
		exit(1);
	}

	if (backwards && (random || offset || writing || create || copy))
	{
		fprintf(stderr,"Backward seek allowed only on sequential read\n");
		exit(1);
	}

	if ((repeats > 1) && create)
	{
		fprintf(stderr, "Repeats not allowed with -c\n");
		exit(1);
	}

	if (copy)
	{
		oflags = O_RDONLY;
		oflags2 = O_WRONLY;
		if (create)
			oflags2 |= (O_CREAT | O_TRUNC);
	}
	else
	{
		if (create)
			oflags = O_WRONLY | O_CREAT | O_TRUNC;

		else if (writing)
			oflags = O_WRONLY;
		else
			oflags = O_RDONLY;
	}

	if  (direct) {
		struct dioattr da;
		if ((fd = open(fname, O_RDONLY | O_DIRECT, 0644)) < 0) {
			perror("dio open failed");
			exit(1);
		}
		if (fcntl(fd,F_DIOINFO,&da) < 0) {
			perror("dio info failed");
			exit(1);
		}
		if (((bsize * 512) % da.d_miniosz) || ((bsize * 512) > da.d_maxiosz)) {
			(void)fprintf(stderr,"invalid bsize for dio: "
				      "bsize %d, d_miniosz %d, d_maxiosz %d\n", 
				      bsize, da.d_miniosz, da.d_maxiosz);
			exit(1);
		}
		if (((align != da.d_mem) && (align % da.d_mem)) || misalign) {
			fprintf(stderr,
				"resetting align to 0x%x and misalign to 0\n",
				da.d_mem);
			align = da.d_mem;
			misalign = 0;
		}
		(void)close(fd);
	}

	if (!async)
	{
		buf = getmemory(align, bsize*512 + misalign);
		if (buf == NULL)
		{
			fprintf(stderr,"getmemory failed!?\n");
			perror("");
			exit(1);
		}

		if (pinning)
		{
			if (mpin(buf, bsize*512 + misalign))
			{
				fprintf(stderr,"mpin failed!?\n");
				perror("");
				exit(1);
			}
		}

		if (memory)
		{
			obuf = getmemory(align, bsize*512 + misalign);
			if (obuf == NULL)
			{
				fprintf(stderr, "getmemory failed!?\n");
				perror("");
				exit(1);
			}
			memcpy(obuf, buf, bsize*512);
			goto mcopy;
		}

		if (misalign)
		{
			buf = &buf[misalign];
			printf("buffer pointer == 0x%lx\n", buf);
		}
	}

	if (interact)
	{
		printf("fname %s repeats %d fsize %ld bsize %d nblox %d oflag %d buf %lx\n",
		    fname, repeats, fsize, bsize, nblox, oflags, (unsigned long)buf);
		if (random) printf("random\n");
		else printf("sequential\n");

		printf("Open file %s for ", fname);
		if ((oflags == O_RDWR) || (oflags & O_CREAT))
			printf("writing? ");
		else printf("reading? ");
		fflush(stdout);
		gets(abuf);
		if (*abuf != 'y') exit(0);
		if (copy)
		{
			if (create)
				printf("Create destination file? ");
			else
				printf("Open destination file? ");

			fflush(stdout);
			gets(abuf);
			if (*abuf != 'y') exit(0);
		}
	}

	if (async)
	{
		if (sio_aio_init(fname, oflags | ((direct) ? O_DIRECT : 0),
		                 align, misalign, async, (realtime && create)))
		{
			fprintf(stderr,"async init failed\n");
			exit(1);
		}
	}
	else 
	{ 
		struct fsxattr  fsxattr;
	
		if ((fd = open(fname, oflags | ((direct)?O_DIRECT:0), 0644)) < 0)
		{
			fprintf(stderr,"Can't open %s\n",fname);
			perror("");
			exit(1);
		}

		if (realtime && create)
		{ 
			bzero(&fsxattr, sizeof(fsxattr));
			fsxattr.fsx_xflags = XFS_XFLAG_REALTIME;
			if (fcntl(fd, F_FSSETXATTR, &fsxattr) < 0) 
			{ 
				fprintf(stderr,"fcntl(F_FSSETXATTR) failed on %s\n", fname);
				perror("");
				exit(1);
			}
		}
	}

	if (copy && 
	    ((fd2 = open(fname2, oflags2 | ((direct)?O_DIRECT:0), 0644)) < 0))
	{
		fprintf(stderr,"Can't open %s\n",fname2);
		perror("");
		exit(1);
	}

	if (interact)
	{
		printf("Begin ");
		if ((oflags == O_RDWR) || (oflags & O_CREAT)) printf("writing ");
		else printf("reading ");
		printf("file %s? ",fname);
		fflush(stdout);
		gets(abuf);
		if (*abuf != 'y') exit(0);
	}

	if (pipesync)
	{
		if (write(opfd, abuf, 1) != 1)
		{
			perror("sio: sync pipe write error - exiting\n");
			exit(1);
		}
		if (read(ipfd, abuf, 1) != 1)
		{
			perror("sio: sync pipe read error - exiting\n");
			exit(1);
		}
		/*
		 * When synchronizing lots of processes, and there is
		 * a possibility of process starvation, it helps to wait
		 * for a while before beginning I/O, to allow other sio
		 * processes to complete their pipe reads.  Once I/O is
		 * started by enough processes to saturate the CPU of
		 * a machine, some processes may not complete the read
		 * above until after receiving the signal to stop, in
		 * which case the read will be aborted with error, with
		 * the sync pipes in an undefined state.
		 */
		sleep(4);
	}

mcopy:
	if (timeit)
		time0 = times(&timebuf);

	if (memory)
	{
		while(repeats-- && !sigstop)
		{
			memcpy(obuf, buf, bsize*512);
			total += bsize;
		}
	}
	else if (async)
	{
		while (repeats-- && !sigstop)
			do_async(nblox, backwards, fsize * 512, async, offset,
				 oflags);
	}
	else if (random)
	{
		while (repeats-- && !sigstop)
		{
			where = offset + (siorand() % nblox) * bsize;
			if (verbose) printwhere();

			if (oflags == O_RDONLY)
			{
				if (usesyssgi)
					ret = syssgi(SGI_READB, fd, buf, where, bsize);
				else
				{
					lseek(fd, where * 512, 0);
					ret = read(fd, buf, bsize*512) / 512;
				}
				if (ret != bsize)
				{
					fprintf(stderr,"read failure on %s at ", fname);
					printwhere();
					fprintf(stderr,"Request 0x%x return 0x%x\n",
						bsize, ret);
					perror("");
					exit(1);
				}
			}
			else
			{
				if (usesyssgi)
					ret = syssgi(SGI_WRITEB, fd, buf, where, bsize);
				else
				{
					lseek(fd, where * 512, 0);
					ret = write(fd, buf, bsize*512) / 512;
				}
				if (ret != bsize)
				{
					fprintf(stderr,"write failure on %s at ", fname);
					printwhere();
					fprintf(stderr,"Request 0x%x return 0x%x\n",
						bsize, ret);
					perror("");
					exit(1);
				}
			}
			total += bsize;
		}
	}
	else
	{	/* sequential */
#if !defined(RANDOMSIZES)
		reqsize = bsize;
#endif
		while (repeats-- && !sigstop)
		{
			if (backwards)
				where = fsize - reqsize;
			else if (offset)
				where = offset;
			else
				where = 0;
			if (!usesyssgi)
				lseek(fd, where * 512, 0);
			for (i = 0; (i < nblox) && !sigstop; i++)
			{
#if RANDOMSIZES
				reqsize = siorand() % bsize + 1;
#endif
#if TIMEMAX
				gettimeofday(&timeb_s, NULL);
#endif
				if (verbose) printwhere();
				if ((oflags == O_RDONLY) || copy)
				{
					if (usesyssgi)
						ret = syssgi(SGI_READB, fd, buf, where, reqsize);
					else
						ret = read(fd, buf, reqsize*512) / 512;
					if (ret != reqsize)
					{
						if (ret == 0)
						{
							fprintf(stderr,
							    "%s: 0 bytes read - "
							    "assuming completed\n",
								fname);
							i = nblox;
							continue;
						}
						else if (ret > 0)
						{
							fprintf(stderr,
							    "%s: short read (%d)\n",
								fname, ret/512);
							total += ret / 512;
							continue;
						}
						fprintf(stderr,"read failure on %s at ", fname);
						printwhere();
						fprintf(stderr, "Request 0x%x return 0x%x\n",
							reqsize, ret);
						perror("");
						exit(1);
					}
				}
				if (oflags != O_RDONLY)
				{
					/* dirty_buffer(buf, reqsize*512); */
					if (usesyssgi)
						ret = syssgi(SGI_WRITEB, fd, buf, where, reqsize);
					else
						ret = write(fd, buf, reqsize*512) / 512;
					if (ret != reqsize)
					{
						fprintf(stderr,"write failure on %s at ", fname);
						printwhere();
						fprintf(stderr, "Request 0x%x return 0x%x\n",
							reqsize, ret);
						perror("");
						exit(1);
					}
				}
				if (copy)
				{
					if (usesyssgi)
						ret = syssgi(SGI_WRITEB, fd2, buf, where, reqsize);
					else
						ret = write(fd2, buf, reqsize*512) / 512;
					if (ret != reqsize)
					{
						fprintf(stderr,"write failure on %s at ", fname2);
						printwhere();
						fprintf(stderr, "Request 0x%x return 0x%x\n",
							reqsize, ret);
						perror("");
						exit(1);
					}
					total += reqsize;
				}

				total += reqsize;
				if (backwards)
				{
					where -= reqsize;
					if (!usesyssgi)
						lseek(fd, where * 512, 0);
				}
				else if (seq_forw_seek)
					where += seq_forw_seek;
				else
					where += reqsize;

#if TIMEMAX
				gettimeofday(&timeb_e, NULL);
				time_diff = 
					(timeb_e.tv_sec * 1000000LL + timeb_e.tv_usec) -
					(timeb_s.tv_sec * 1000000LL + timeb_s.tv_usec);
				time_diff = time_diff / 1000LL;
				if (time_diff < TIMEBINS)
					++timebins[time_diff];
				else
					printf("%s: Exceeded %dmS\n", fname, TIMEBINS);
#endif
			}
		}
	}

	if (timeit)
		time1 = times(&timebuf);

	if (pipesync)
	{
		if (!sigstop)
		{
			fprintf(stderr,"Warning: completed I/O before signal\n");
			while (!sigstop)
				usleep(1000);
		}
		if (write(opfd, abuf, 1) != 1)
		{
			fprintf(stderr,"Error writing to sync pipe\n");
			fprintf(stderr,"opfd %d errno %d fname %s\n", 
			    opfd, errno, fname);
			perror("");
			exit(1);
		}
		if (read(ipfd, abuf, 1) != 1)
		{
			perror("Error reading from sync pipe");
		}
	}

#if TIMEMAX
	printf("TIMEMAX %s: ", fname);
	for (i = 0; i < TIMEBINS; ++i) {
		if (i % 10 == 0)
			printf("\n%4d: ", i);
		printf("%4d ", timebins[i]);
	}
	printf("\n");
#endif

	if (timeit)
	{
		et = (double) (time1 - time0) / 100;
		if (et > 1)
		{
			if (wilborn_wynn)
				printf("%.2f %.1f\n", ((double) total/1953.1) / et,
				       ((double) total / bsize) / et);
			else {
				printf("%.2f s  %s  %lu blocks", et, fname, total);
				printf("  %.0f KB/s (%.2f MB/s)  IO/s %.2f\n",
				       ((double) total / 2) / et,
				       ((double) total / 1953.1) / et,
				       ((double) total / bsize) / et);
			}
		}
		else
			printf("sio: elapsed time < 20ms, result invalid\n");
		fflush(stdout);
	}

	if (sigsio)
	{
		int result;

		if (random || (bsize < 32))
			result = -(total / bsize);
		else
			result = total;
		write(opfd, &result, sizeof(result));
	}

	exit(0);
}

int
parse_nodenums(char *arg, int *nodenums, char ***nodes)
{
	int i;
	char *p, *q;
	int nodenum;

	for (p = arg, *nodenums = 0; *p; ++p) {
		if (*p == ':')
			++*nodenums;
	}
	++*nodenums;
	p = arg;
	*nodes = (char **)calloc(*nodenums + 1, sizeof(char *));
	for (i = 0; i < *nodenums; ++i) {
		(*nodes)[i] = (char *)malloc(strlen("/hw/nodenum/XXX"));
		nodenum = strtol(p, &q, 10);
		if (p == q)
			return(-1);
		p = ++q;
		sprintf((*nodes)[i], "/hw/nodenum/%d", nodenum);
	}
	return(0);
}

void
numa_placement(int numnodes, char *node_list[])
{
	int            i;
	pmo_handle_t  *mldlist,mldset;
	policy_set_t   policy_set;
	pmo_handle_t   pm;
	raff_info_t   *rafflist;
	int            rc;
	
#ifdef NUMA_DEBUG
	printf("There are %d nodes\n", numnodes);
#endif
	mldlist  = (pmo_handle_t  *)malloc(numnodes*sizeof(pmo_handle_t));
	for(i=0;i<numnodes;i++)
		mldlist[i] = mld_create(0,1);
	mldset = mldset_create(mldlist, numnodes);
	rafflist = (raff_info_t *)malloc(numnodes*sizeof(raff_info_t));
	for (i=0;i<numnodes;i++){
		rafflist[i].resource = (void*)node_list[i];
		rafflist[i].reslen = (unsigned short) strlen(node_list[i]);
		rafflist[i].restype = RAFFIDT_NAME;
		rafflist[i].radius = 1;
		rafflist[i].attr = RAFFATTR_ATTRACTION;
	}
	
	rc = mldset_place(mldset, TOPOLOGY_PHYSNODES,(raff_info_t *)rafflist, 
			  numnodes, RQMODE_ADVISORY);
	if (rc < 0) {
		perror("mldset_place failed");
		exit(1);
	}
#ifdef NUMA_DEBUG
        {
		dev_t node_dev;
		char devname[128];
		int length = 128;
		for (i = 0; i < numnodes ; i++) {
			node_dev = __mld_to_node(mldlist[i]);
			printf("memory %d is placed on %s\n", i, dev_to_devname(node_dev,  
										devname, &length));
		}
	}
#endif
	pm_filldefault(&policy_set);
	policy_set.placement_policy_name = "PlacementRoundRobin";
	policy_set.placement_policy_args = (void*)mldset;
	pm = pm_create(&policy_set);
	if (pm < 0) {
		perror("pm_create failed");
		exit(1);
	}
	rc = pm_setdefault(pm,MEM_DATA);
	if (rc < 0) {
		perror("pm_setdefault failed");
		exit(1);
	}
	free(mldlist);
}
