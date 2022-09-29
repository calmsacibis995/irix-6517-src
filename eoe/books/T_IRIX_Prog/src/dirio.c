/*
|| dirio: program to test and demonstrate direct I/O.
||
|| dirio  [-o outfile] [ -m {b|s|d} ] [ -b bsize ] [ -n recs ] [ -i ]
||
||	-o outfile		output file pathname, default $TEMPDIR/dirio.out
||
||	-m {b|s|d}		file mode: buffered (default), synchronous, or direct
||
||  -b bsize		blocksize for each write, default 512
||
||  -n recs         how many writes to do, default 1000
||
||  -i              display info from fcntl(F_DIOINFO)
||
*/
#include <errno.h>		/* for perror() */
#include <stdio.h>		/* for printf() */
#include <stdlib.h>		/* for getenv(), malloc(3c) */
#include <sys/types.h>	/* required by open() */
#include <unistd.h>		/* getopt(), open(), write() */
#include <sys/stat.h>	/* ditto */
#include <fcntl.h>		/* open() and fcntl() */

int main(int argc, char **argv)
{
	char*		tmpdir;			/* ->name string of temp dir */
	char*		ofile = NULL; /* argument name of file path */
	int			oflag = 0;		/* -m b/s/d result */
	size_t		bsize = 512;	/* blocksize */
	void*       buffer;			/* aligned buffer */
	int			nwrites = 1000;	/* number of writes */
	int			ofd;			/* file descriptor from open() */
	int			info = 0;		/* -i option default 0 */
	int			c;				/* scratch var for getopt */
	char		outpath[128];	/* build area for output pathname */	
	struct dioattr dio;

	/*
	|| Get the options
	*/
	while ( -1 != (c = getopt(argc,argv,"o:m:b:n:i")) )
	{
		switch (c)
		{
		case 'o': /* -o outfile */
		{
			ofile = optarg;
			break;
		}
		case 'm': /* -m mode */
		{
			switch (*optarg)
			{
			case 'b' : /* -m b buffered i.e. normal */
				oflag = 0;
				break;
			case 's' : /* -m s synchronous (but not direct) */
				oflag = O_SYNC;
				break;
			case 'd' : /* -m d direct */
				oflag = O_DIRECT;
				break;
			default:
				fprintf(stderr,"? -m %c\n",*optarg);
				return -1;			
			}
			break;
		}
		case 'b' : /* blocksize */
		{
			bsize = strtol(optarg, NULL, 0);
			break;
		}
		case 'n' : /* number of writes */
		{
			nwrites = strtol(optarg, NULL, 0);
			break;
		}
		case 'i' : /* -i */
		{
			info = 1;
			break;
		}
		default:
			return -1;
		} /* end switch */
	} /* end while */
	/*
	|| Ensure a file path
	*/
	if (ofile)
		strcpy(outpath,ofile);
	else
	{
		tmpdir = getenv("TMPDIR");
		if (!tmpdir)
			tmpdir = "/var/tmp";
		strcpy(outpath,tmpdir);
		strcat(outpath,"/dirio.out");
	}
	/*
	|| Open the file for output, truncating or creating it
	*/
	oflag |= O_WRONLY | O_CREAT | O_TRUNC;
	ofd = open(outpath,oflag,0644);
	if (-1 == ofd)
	{
		char msg[256];
		sprintf(msg,"open(%s,0x%x,0644)",outpath,oflag);
		perror(msg);
		return -1;
	}
	/*
	|| If applicable (-m d) get the DIOINFO for the file and display.
	*/
	if (oflag & O_DIRECT)
	{
		(void)fcntl(ofd,F_DIOINFO,&dio);
		if (info)
		{
		printf("dioattr.d_mem    : %8d (0x%08x)\n",dio.d_mem,dio.d_mem);
		printf("dioattr.d_miniosz: %8d (0x%08x)\n",dio.d_miniosz,dio.d_miniosz);
		printf("dioattr.d_maxiosz: %8d (0x%08x)\n",dio.d_maxiosz,dio.d_maxiosz);
		}
		if (bsize < dio.d_miniosz)
		{
			fprintf(stderr,"bsize %d too small\n",bsize);
			return -2;
		}
		if (bsize % dio.d_miniosz)
		{
			fprintf(stderr,"bsize %d is not a miniosz-multiple\n",bsize);
			return -3;
		}
		if (bsize > dio.d_maxiosz)
		{
			fprintf(stderr,"bsize %d too large\n",bsize);
			return -4;
		}
	}
	else
	{ /* set a default alignment rule */
		dio.d_mem = 8;
	}
	/*
	|| Get a buffer aligned the way we need it.
	*/
	buffer = memalign(dio.d_mem,bsize);
	if (!buffer)
	{
		fprintf(stderr,"could not allocate buffer\n");
		return -5;
	}
	bzero(buffer,bsize);
	/*
	|| Write the number of records requested as fast as we can.
	*/
	for(c=0; c<nwrites; ++c)
	{
		if ( bsize != (write(ofd,buffer,bsize)) )
		{
			char msg[80];
			sprintf(msg,"%d th write(%d,buffer,%d)",c+1,ofd,bsize);
			perror(msg);
			break;
		}
	}
	/*
	|| To level the playing field, sync the file if not sync'd already.
	*/
	if (0==(oflag & (O_DIRECT|O_SYNC)))
		fdatasync(ofd);

	close(ofd);
	return 0;
}
