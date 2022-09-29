/************************************************************************
 *  This program tests tape drives for "hanging" by repeatedly opening,
 *  closing, writing to, and reading from the tape random sized blocks
 *  in each partition.  Between one open() and close(), from 2 to
 *  MAX_BLOCKS blocks of from SIZE_FACTOR to SIZE_FACTOR * (BYTE_MULT - 1)
 *  will be written or read.
 *
 *  Options:
 *  -e specifies change tape on eot.
 *  -i specifies prompt on reaching eot.
 *  -m specifies the total number of bytes to write per pass.
 *  -n specifies the total number of passes though the tape.
 *  -s specifies stop on error mode.
 *  -v specifies verbose mode.
 *************************************************************************/ 


#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/mtio.h>
#include <sys/stat.h>
#include <errno.h>

#define PATTERN_SIZE 6
#define MAX_BLOCKS 100 		
#define BYTE_MULT 60                    /* determines maximum blocksize  */
#define SIZE_FACTOR 1024
#define SIZE (SIZE_FACTOR/sizeof(int)*BYTE_MULT)

struct data_info  {		        /* data_info holds information   */
	int  partition;			/* file # */
	int  blocksize;	                /* pertinant to reading back     */
	int  number_of_blocks;	        /* and checking the information  */
	struct data_info *next;   	/* written to the tape           */
};
struct queue {
	struct data_info *front;        /* queue is just pointers to     */
	struct data_info *rear;         /* data_info cells               */
};
struct data_info *qalloc();	        /* allocates nodes for the queue */
struct queue    Q;	        	/* global queue                  */

struct eotblock {
	int partition;			/* the partition #		*/
	int blocknum;			/* block # in partition 	*/
	int blocksize;			/* actual block size returned   */
} eotblock;

int  pattern[6] = {0xa5, 0x5a, 0xa5, 0x6bd, 0xbd6, 0xdb6};
int  totalerr = 0;                      /* global error count            */
int   partitions;     			/* partition number 		 */
int   num_bytes;      			/* total number of bytes written */
int   passes = 1;     			/* the number of passes 	 */
int   verbose = 0;    			/* true if verbose mode specified*/
int   stoperr = 0;			/* true if stop on error is specified */
int   dorandom = 0;			/* true if files read back in random */
char  devname[60];       		/* holds the device name */
int   current_file;			/* current file on tape */
int   current_block;			/* current record in a file */
int   bsf_flag = 0;			/* if true then can backspace a file */
int   bsr_flag = 0;			/* if true then can backspace a rec */
int   max_bytes = 5000000; 		/* maximum number of bytes per pass*/
int   debug = 1;
int   data_pattern;
char  eot;
int   tapecount;
char  errmsg[256];

int   write(), read();

#define dprintf(x) ((debug) ? fprintf x : 0)

main(argc, argv)
 int  argc;
 char **argv;
{
	void  makenull();     /* Queue routines                             */
	void  dequeue();      /* makenull intitializes queue,enqueue        */
	void  enqueue();      /* adds elements to it, dequeue and first     */
	void  first();        /* remove elements, empty returns true if     */
	int   empty();        /* queue is empty.                            */
	void  get_blocks();   /* gets random block and partition sizes      */
	void  zap();	      /* flushes buffer                             */
	void  terror();	      /* prints error message and exits program     */
	void  qerror();	      /* prints queue specific error message        */
	void  rewinds();      /* rewinds the tape                           */
	void  pattern_buf();  /* loads hex pattern into a buffer            */
	void  data_print();   /* prints current status of program           */
	int   pattern_comp(); /* accepts pattern and blocksize,returns true */
                              /* if pattern is correct                      */
	int   strtest();      /* test to see if device is tape or file      */
	int   fd;             /* file descriptor for the tape drive         */
	int   c;	      /* holds command line argument                */
	int   interactive=0;  /* stop on eot (write or read)                */
	extern int optind;
	extern char *optarg;
        
	while ((c = getopt (argc, argv, "esvrn:m:")) != EOF)
		switch (c) {
		case 'e':
			eot = 1;
			break;
		case 'i':
			interactive = 1;
			break;
		case 'm':
			max_bytes = atoi(optarg);
			break;
		case 'n':
			passes = atoi(optarg);
			break;
		case 'r':
			dorandom = 1;
			break;
		case 's':
			stoperr = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case '?':
			optind = 100;
			break;
		default:
			fprintf(stderr,"incorrect option:%c,%d\n",c,c);
			break;
		}
	if (dorandom && eot) {
		fprintf(stderr,"'r' flag can not be used with 'e' flag\n");
		exit(1);
	}
	if (optind >= argc)  {
		fprintf(stderr,"Usage: bash [-e -v -s -r -n number of passes -m maximum number of bytes] /dev/tapename\n");
		exit(1);
	}
	strcpy(devname, argv[optind]); /* devname is first arg after options */
	srand((unsigned) time(NULL));  /* seed the randomizer */
        /******************************************************************
	 * the main function first enters a write-loop in which it writes *
         * a random number of randomly sized blocks of pattern to the tape*
 	 * in addition it saves these random values in a queue (Q) so that*
 	 * a check can be made during the read-loop to see if the values  *
	 * were written correctly.  The read-loop goes through the same   *
 	 * steps as the write-loop to re-create the random blocks         * 
	 ******************************************************************/
	while (passes--) {
		sequential_write();
		if (interactive) {
			printf("Hit return to continue ");
			getchar();
		}
		if (dorandom)
			random_read();
		else	
			sequential_read();
		if (interactive) {
			printf("Hit return to continue ");
			getchar();
		}
	}
	fprintf(stdout,"\n%s%d%s\n", "Tape check: ",totalerr," total errors.");

}


sequential_write()
{
	int   num_of_blocks;  /* number of blocks in partition              */
	int   blocksize;      /* number of integers in block                */
	char  *stat;	      /* holds the current operation(read,write,etc.*/
	int   fd;
	int   ecode;

	partitions = 0;	/* intializations */
	makenull(&Q);
	num_bytes = 0;
	blocksize = 0;
	num_of_blocks = 0;

	stat = "Rewinding";
	data_print(num_of_blocks,blocksize,partitions,verbose,stat);
	rewinds(devname);
	/*              write-loop                            */
	while (num_bytes <= max_bytes) {
		partitions++;
		get_blocks(&num_of_blocks, &blocksize);
		num_bytes += num_of_blocks * blocksize * sizeof(int);
		if ((fd = open(devname, O_WRONLY)) == -1) {
			fprintf(stderr,"%s%s\n", "can't open: ", devname);
			exit(1);
		}
		stat = "Writing";
		data_print(num_of_blocks, blocksize, partitions,
								 verbose, stat);
		ecode = write_blocks(num_of_blocks, blocksize, fd, partitions);
		enqueue(partitions, num_of_blocks, blocksize, &Q);
		close(fd);
		if (ecode == ENOSPC)
			break;
	}
}

sequential_read()
{
	int   num_of_blocks;  /* number of blocks in partition              */
	int   blocksize;      /* number of integers in block                */
	char  *stat;	      /* holds the current operation(read,write,etc.*/
	int   done;	      /* total number of bytes read                 */
	int   fd;
	int   ecode;

	done = 0;
	partitions = 0;     /* initializations */
	blocksize = 0;
	num_of_blocks = 0;

	if (tapecount) {
		fprintf(stdout,"Insert the first tape then press RETURN\n");
		fscanf(stdin,"%c",stat);
	}
	stat = "Rewinding";
	data_print(num_of_blocks,blocksize,partitions,verbose,stat);
	rewinds(devname);
        /*              read-loop                             */
	while (done < num_bytes) {
		partitions++;
		first(&num_of_blocks, &blocksize, &Q);
		dequeue(&Q);
		if ((fd = open(devname, O_RDONLY)) == -1) {
			fprintf(stderr,"%s%s\n", "can't open: ", devname);
			exit(1);
		}
		stat = "Reading";
		data_print(num_of_blocks,blocksize,partitions,verbose,stat);
		ecode = read_blocks(num_of_blocks, blocksize, fd, partitions);
		done += num_of_blocks * blocksize * sizeof(int);
		if (ecode == ENOSPC) {
			rewinds(0,fd);
			close(fd);
			return;
		}
		else {
			struct mtop mtcom;
			mtcom.mt_op = MTFSF;
			mtcom.mt_count = 1;
			if (ioctl(fd, MTIOCTOP, &mtcom) == -1) {
				perror("file forward");
				close(fd);
				exit(1);
			}
		}
		close(fd);
	}
}

random_read()
{
	int p1 = 1;
	int p2 = partitions;
	int num_of_blocks, blocksize;	
	int found;
	int done = 0;
	int fd;
	char *stat;
	struct mtop mtcom;

	/* if only wrote 1 partition then call sequential_read */
	if (partitions == 1) {
		sequential_read();
		return;
	}
	/* figure out the kind of ops can be done on this device */
	tape_ops(devname);
	/* rewind */
	rewinds(devname);
	current_file = 1;
	/* XXX only do random if partition > 1 */

	while ( (p1 <= p2) && (done < num_bytes) ) {
		/* remove the node with partition # = p1 */
		found = search_dequeu(p1, &num_of_blocks, &blocksize, &Q);
		if ( found == 0 )
			return(0);	/* done */
		if ((fd = open(devname, O_RDONLY)) == -1) {
			fprintf(stderr,"%s%s\n", "can't open: ", devname);
			exit(1);
		}
		stat = "Reading";
		data_print(num_of_blocks,blocksize,p1,verbose,stat);
		search_for_file(fd, p1);
		random_recs_read(num_of_blocks, blocksize, fd, 
						stoperr, verbose, p1);
		done += num_of_blocks*blocksize*sizeof(int);

		/* Skip over file mark if not EOT */
		if (p1 != eotblock.partition) {
			dprintf(
				(stdout,"skipping to next partition\n"));
			mtcom.mt_op = MTFSF;
			mtcom.mt_count = 1;
			if (ioctl(fd, MTIOCTOP, &mtcom) == -1) {
				perror("file forward");
				close(fd);
				exit(1);
			}
		}
		current_file = p1+1;
		close(fd);

		/* remove the node with partition # = p2 */
		found = search_dequeu(p2, &num_of_blocks, &blocksize, &Q);
		if ( found == 0 )
			return(0);	/* done */
		if ((fd = open(devname, O_RDONLY)) == -1) {
			fprintf(stderr,"%s%s\n", "can't open: ", devname);
			exit(1);
		}
		stat = "Reading";
		data_print(num_of_blocks,blocksize,p2,verbose,stat);
		search_for_file(fd, p2);
		random_recs_read(num_of_blocks, blocksize, fd, 
						stoperr, verbose, p2);
		done += num_of_blocks*blocksize*sizeof(int);
		/* Skip over file mark if not EOT */
		if (p2 != eotblock.partition) {
			dprintf(
				(stdout,"skipping to next partition\n"));
			mtcom.mt_op = MTFSF;
			mtcom.mt_count = 1;
			if (ioctl(fd, MTIOCTOP, &mtcom) == -1) {
				perror("file forward");
				close(fd);
				exit(1);
			}
		}
		current_file = p2+1;
		close(fd);

		/* update p1, p2 */
		p1 += 1;
		p2 -= 1;
	}
}
	
/************************************************************************
 * write_blocks writes pattern of random size to tape. As parameters    *
 * it receives random numbers for num_of_blocks and blocksize. It sends *
 * blocksize and a buffer to pattern_buf which fills the buffer with    *
 * a pattern of length blocksize*sizeof(int) errors are returned if     *
 * a write error occurs or an incorrect number of bytes are written.    *
 * It iterates this process num_of_blocks times.                        * 
 ************************************************************************/
write_blocks(num_of_blocks, blocksize, fd, partn)
 int  num_of_blocks;   /* number of blocks per partition */
 int  blocksize;       /* random multiple of 128, always <= 2048 */ 
 int  fd;	       /* file descriptor for tape drive */	
 int  partn;

{
	int  buffer[SIZE];
	int  bites;
	int  dummy;
	int curblock= 0;

	while (num_of_blocks-- > 0) {
		zap(buffer);
		pattern_buf(blocksize, buffer, curblock++, partn);
		bites = blocksize * sizeof(int);
		if (bites != (dummy = myio(write, &fd, buffer, bites, eot)))
			if (dummy == -1) {
				/* 
				** the only error is ENOSPC
				** lets update the eotblock
				*/
				eotblock.partition = partn;
				eotblock.blocknum = curblock - 1;
				eotblock.blocksize = blocksize;
				fprintf(stdout,"Writing at end of media: part: %d, block: %d, blocksize: %d\n",partn, curblock - 1, blocksize);
				return(ENOSPC);
			}
			else {
				sprintf(errmsg,"error: short write, part: %d, block: %d, blocksize: %d bytes_left: %d\n",partn, curblock-1, blocksize, dummy);
				terror(errmsg);
 			}	
	}
	return(0);
}

/**********************************************************************
 * read_blocks reads the blocks back from the tape and recreates the  *
 * buffer that write_blocks used, then compares it to what it read.   *   
 * An error occurs if the correct number of bytes were not read or if *
 * the bytes that were read dont match the bytes that were retrieved  *
 * from pattern_buf.                                                  *
 **********************************************************************/
read_blocks(num_of_blocks, blocksize, fd, partn)
 int  num_of_blocks; /* number of blocks per partition */
 int  blocksize;     /* random mult of 128 <= 2048 */
 int  fd;            /* file descriptor for tape drive */
 int  partn;
{
	int  buffer[SIZE]; 
        int  bites;     /* blocksize * sizeof(int) */
	int  dummy;     /* for perror */
	int  curblock = 0;

	zap(buffer);
	while (num_of_blocks-- > 0) {
		bites = blocksize * sizeof(int);
		if (bites != (dummy = myio(read, &fd, buffer, bites, eot))) {
			if ((dummy == -1) || (dummy == 0)) {
				/* 
				** the only error is ENOSPC
				** lets check the eotblock
				*/
				if (eotblock.partition != partn ||
					eotblock.blocknum != curblock ||
					eotblock.blocksize != blocksize) {
				if (eotblock.partition == 0) 
				sprintf(errmsg,"error: no EOT during write but geting EOT during read\npartn: %d, block: %d, blocksize: %d\n",partn, curblock, blocksize);
				else
				sprintf(errmsg,"error: EOT during read is at different place from EOT during write\nread EOT at partn: %d, block: %d, blocksize: %d\nwrite EOT at partn: %d, block: %d, blocksize: %d\n",partn, curblock, blocksize, eotblock.partition, eotblock.blocknum, eotblock.blocksize);
				fprintf(stdout,"%s",errmsg);
				exit(1);
				}
				return(ENOSPC);	/* everythng is wondeful */
			}
			else {
				sprintf(errmsg,"error: short read, part: %d, block: %d, blocksize: %d bytes_left: %d\n",partn, curblock, blocksize, dummy);
				terror(errmsg);
			}
		}
		pattern_comp(buffer, blocksize, curblock++, partn);
	}
}

/**********************************************************************
 * verify_block reads one block back from the tape and recreates the  *
 * buffer that write_blocks used, then compares it to what it read.   *   
 * An error occurs if the correct number of bytes were not read or if *
 * the bytes that were read dont match the bytes that were retrieved  *
 * from pattern_buf.                                                  *
 **********************************************************************/
verify_block(blocksize, fd, curblock, partn)
 int  blocksize;     /* random mult of 128 <= 2048 */
 int  fd;            /* file descriptor for tape drive */
 int  curblock, partn;
{
	int  buffer[SIZE]; 
        int  bites;     /* blocksize * sizeof(int) */
	int  dummy;     /* for perror */

	zap(buffer);
	bites = blocksize * sizeof(int);
	if (bites != (dummy = myio(read, &fd, buffer, bites, eot))) {
		if ( (dummy == -1) || (dummy == 0) ) {
			/* 
			** the only error is ENOSPC
			** lets check the eotblock
			*/
			if (eotblock.partition != partn ||
				eotblock.blocknum != curblock ||
				eotblock.blocksize != blocksize) {
				if (eotblock.partition == 0) 
				sprintf(errmsg,"error: no EOT during write but geting EOT during read\npartn: %d, block: %d, blocksize: %d\n",partn, curblock, blocksize);
				else
				sprintf(errmsg,"error: EOT during read is at different place from EOT during write\nread EOT at partn: %d, block: %d, blocksize: %d\nwrite EOT at partn: %d, block: %d, blocksize: %d\n",partn, curblock, blocksize, eotblock.partition, eotblock.blocknum, eotblock.blocksize);
				fprintf(stdout,"%s",errmsg);
				exit(1);
			}
			fprintf(stdout,"read EOT and write EOT matched!\n");
			return;
		}
		else {
			sprintf(errmsg,"error: short read, part: %d, block: %d, blocksize: %d bytes_left: %d\n",partn, curblock, blocksize, dummy);
			terror(errmsg);
		}
		pattern_comp(buffer, blocksize, curblock, partn);
	}
}

/*
 * read all records from a file in non-sequential order  
 * if bsr_flag is not set then read records in sequential order
 */
random_recs_read(num_of_blocks, blocksize, fd, stoperr, verbose, partn)
{
	int blk1 = 0;
	int blk2 = num_of_blocks;

	if (!bsr_flag) {
		read_blocks(num_of_blocks, blocksize, fd, partn);
		return;
	}
	current_block = 0;
	while ( blk1 <= blk2 ) {
		search_for_block(fd, blk1);
		verify_block(blocksize, fd, blk1, partn);
		current_block = blk1+1;
		if (blk1 == blk2)
			return(0);
		search_for_block(fd, blk2); 	
		verify_block(blocksize, fd, blk2, partn);
		current_block = blk2+1;
		blk1 += 1;
		blk2 -= 1;
	}
	dprintf(
		(stdout,"\n") );
}
	
/*
 *  pattern_comp compares what is being read with what was written 
 */
 int 
pattern_comp(buffer, blocksize, curblock, partn)
 int  buffer[];   /* pattern to be checked */
 int  blocksize;  /* blocksize to recreate correct pattern */
 int  curblock, partn;
{
	int  match[SIZE];  /* holds recreated pattern */
	register int  i;   /* loop variable */ 
	int  val = 1;      /* val holds what patterncomp returns */
	int  count = 0;    
	int tmp;

	pattern_buf(blocksize, match, curblock, partn);
	for (i = 0; i <= (blocksize - 1); i++) 
		if (match[i] != buffer[i]) {
			if (verbose) {
				if (count < 3) {
					sprintf(errmsg,"error: word=%x expect=%x actual=%x part: %d, block: %d, blocksize: %d\n",
							i, match[i], buffer[i],
						partn, curblock, blocksize);
					terror(errmsg);
				}
				count++;
				totalerr++;
			}
		val = 0;
	        }
	if (count) {
		sprintf(errmsg,"Total error of block:%d partn:%d is %d, Partn_id:%d Block_id:%d BLocksize:%d\n",
		curblock, partn, count, buffer[0],buffer[1],blocksize);
		terror(errmsg);
	}
	return (val);
}
/*
 * pattern_buf inserts the pattern into a buffer by looping through  
 * blocksize many times and filling the buffer with pattern.
 */
 void
pattern_buf(blocksize, patternbuf, curblock, partn)
 int  blocksize;   /* determines how large the buffer will be */
 int  patternbuf[];/* patternbuffer of size SIZE */
 int  partn, curblock; /* will be used as the ID of this buffer */
{
	int  mod, patterns;
	register int i, j, n;
	static int data_pattern;

	data_pattern = 0;
	patternbuf[0] = partn;
	patternbuf[1] = curblock;
	for (i=2; i<blocksize; i++) {
		patternbuf[i] = ~data_pattern++;
	}

}
                        /* queue functions */
 void
enqueue(partition, num_of_blocks, blocksize, q)
 int partition;   /* file # */
 int blocksize;    /* blocksize is a multiple of 128 <= 2048 */
 int num_of_blocks;/* number of blocks per partition */
 struct queue   *q;
{
	struct data_info *temp;

	temp = qalloc();
	temp->partition = partition;
	temp->number_of_blocks = num_of_blocks;
	temp->blocksize = blocksize;
	q->rear->next = temp;
	q->rear = q->rear->next;
}
 void
makenull(q)
 struct queue   *q;
{
	q->front = qalloc();
	q->front->next = NULL;
	q->rear = q->front;
	q->front->partition = -1;
}
 int
empty(q)
 struct queue   *q;
{
	return (q->front == q->rear);
}

 void 
first(num_of_blocks, blocksize, q)
 struct queue   *q;
 int            *num_of_blocks;
 int            *blocksize;
{
	if (empty(q))
		qerror();
	else {
		*blocksize = q->front->next->blocksize;
		*num_of_blocks = q->front->next->number_of_blocks;
	}
}
 void 
dequeue(q)
 struct queue   *q;
{
	struct data_info  *p;
	if (empty(q))
		qerror();
	else   {
		p = q->front;
		q->front = q->front->next;
		free(p);
	}
}

search_dequeu(parn, num_of_blocks, block_size, q)
 int parn;
 int *num_of_blocks, *block_size;
 struct queue *q;
{
	struct data_info *p;
	struct data_info *prev;

	if (empty(q))
		qerror();
	else {
		p = q->front;
		while (p != NULL) {
			if (p->partition != parn) {
				prev = p;
				p = p->next;
				continue;
			}
			if (p == q->front) 
				q->front = p->next;
			else
				prev->next = p->next;	
			*num_of_blocks = p->number_of_blocks;
			*block_size = p->blocksize;
			free(p);
			return(1);
		}
	}
	return(0);
}

		
/* end queue routines */

/* qalloc allocates space for nodes in Q */
 struct 
data_info *qalloc()
{
	return (struct data_info *) malloc(sizeof(struct data_info));
}

/* get_blocks determines the number and size of blocks to be written */
 void 
get_blocks(num_of_blocks, blocksize)
 int *num_of_blocks;
 int *blocksize;
{
	int x;
	x = 0;
	while (x == 0) {
		x = rand()%MAX_BLOCKS;
	}
	*num_of_blocks = x;
	x = 0;
	while (x == 0) {
		x = rand() % BYTE_MULT;
	}
	*blocksize = x * SIZE_FACTOR/sizeof(int);	/* in word */
	*num_of_blocks += 1;	/* at least 2 blocks */
}
/*
 * zap initializes the buffer 
 */
 void 
zap(buffer)
 int  buffer[];
{
	register int i;

	for (i = 0; i < SIZE; i++)
		buffer[i] = NULL;
}

void
terror(msg)
char *msg;
{
	if (verbose)
		fprintf(stdout,"%s",msg);
	if (stoperr) {
		exit(1);
	}
}

/*
 *  qerror prints error message when queue is empty
 */
 void 
qerror()
{
	fprintf(stderr,"%s\n", "Queue Error: queue is empty");
	exit(1);
}
/*
 *  rewinds rewinds the tape in the specified device 
 */
 void 
rewinds(devname, given_fd)
 char *devname;
 int given_fd;
{
 	int fd;
        struct mtop  mtcom;
	struct stat *buf;
	int dummy;
        
	buf = (struct stat *) malloc(sizeof(struct stat));
	if (devname != (char *)0) {
		if ((fd = open(devname, O_RDONLY)) == -1) {
			fprintf(stderr,"%s%s\n", "can't open: ", devname);
			exit(1);
		}
	}
	else {
		fd = given_fd;
	}
	if ((dummy = fstat(fd,buf)) == -1) {
		perror("stat");
		exit(1);
        }
	if (buf->st_rdev != -1) {
		mtcom.mt_op = MTREW;
		mtcom.mt_count = 1;
		if (ioctl(fd, MTIOCTOP, &mtcom) == -1) {
			perror("rewind");
			close(fd);
			exit(1);
		}
		if (devname != 0)
			 close(fd);
	} else {
		fprintf(stderr, "not  tape device\n");
		exit(1);
	}
}
/*
 * data_print prints out the current blocksize, number of blocks, status, and
 * partition if verbose, else just partition and status.
 */
 void 
data_print(num_of_blocks, blocksize, partitions,verbose, stat)
 int  num_of_blocks;
 int  blocksize;			/* in ints */
 int  partitions;
 int  verbose;
 char *stat;
{
	blocksize = blocksize * sizeof(int);
   	if (verbose) {
		fprintf(stdout,"%s%02d", " Partion number: ", partitions);
		fprintf(stdout,"%s%s", ", Status: ", stat);
		fprintf(stdout,"%s%d", ", Blocksize: ", blocksize);
		fprintf(stdout,"%s%d", ", Number of blocks: ", num_of_blocks);
		fprintf(stdout,"\n");
	}

}
/* strtest checks to see if the file is a device (tape)     */
 int 
strtest(s)
 char  *s;
{
	char *t;
	int  i;

	i = 1;
	t = "/dev";
	for (; *s == *t; s++, t++, i++)
		if (i == 4)
			return 0;
	return 1;
}

/* 
 * After writing, check to see what kind of tape ops are allowed
 * before reading
 */
tape_ops(devname)
 char *devname;
{
 	int fd;
        struct mtop  mtcom;
	struct stat *buf;
	int dummy;
       
	buf = (struct stat *) malloc(sizeof(struct stat));
	if ((fd = open(devname, O_RDONLY)) == -1) {
		fprintf(stderr,"%s%s\n", "can't open: ", devname);
		exit(1);
	}
	if ((dummy = fstat(fd,buf)) == -1) {
		perror("stat");
		exit(1);
        }
	if (buf->st_rdev != -1) {	/* device file */
		/* rewind should work */
		mtcom.mt_op = MTREW;
		mtcom.mt_count = 1;
		if (ioctl(fd, MTIOCTOP, &mtcom) == -1) {
			perror("rewind");
			close(fd);
			exit(1);
		}
		/* record forward should work */
		mtcom.mt_op = MTFSR;
		mtcom.mt_count = 2;
		if (ioctl(fd, MTIOCTOP, &mtcom) == -1) {
			perror("record forward");
			close(fd);
			exit(1);
		}
		/* record reverse might work */
		mtcom.mt_op = MTBSR;
		mtcom.mt_count = 1;
		bsr_flag = 1;
		if (ioctl(fd, MTIOCTOP, &mtcom) == -1) {
			perror("record reverse");
			if (errno != EINVAL) {
				close(fd);
				exit(1);
			}
			bsr_flag = 0;
		}
		/* file forward should work */
		mtcom.mt_op = MTFSF;
		mtcom.mt_count = 2;
		if (ioctl(fd, MTIOCTOP, &mtcom) == -1) {
			perror("file forward");
			close(fd);
			exit(1);
		}
		/* file reverse might work */
		mtcom.mt_op = MTBSF;
		mtcom.mt_count = 1;
		bsf_flag = 1;
		if (ioctl(fd, MTIOCTOP, &mtcom) == -1) {
			perror("file reverse");
			if (errno != EINVAL) {
				close(fd);
				exit(1);
			}
			bsf_flag = 0;
		}
	
		close(fd);
		if (bsf_flag)
			dprintf(
				(stdout,"bsf_flag set\n") );
		if (bsr_flag) {
			dprintf(
				(stdout,"bsr_flag set\n") );
			bsr_flag = 0;
		}
	} else {
		fprintf(stderr,"Not a tape device\n");
		exit(1);
	}
}


/*
 * search_for_file
 */
search_for_file(fd, filenum)
{
	extern 	int current_file;
        struct mtop  mtcom;
	int fdiff;

	dprintf(
		(stdout,"search file: filenunm = %d current_file = %d\n",
					filenum, current_file) );
	if (bsf_flag == 0) {	/* can't go backward */
		if (filenum < current_file) {
			dprintf( 
				(stdout,"REW\n") );
			mtcom.mt_op = MTREW;
			if (ioctl(fd, MTIOCTOP, &mtcom) == -1) {
				perror("rewind");
				close(fd);
				exit(1);
			}
			current_file = 1;
		}
	}
			
	if (filenum >= current_file) {
		fdiff = filenum - current_file;
		mtcom.mt_op = MTFSF;
		dprintf(
			(stdout,"FSF %d\n",fdiff) );
	}
	else {
		mtcom.mt_op = MTBSF;
		fdiff = current_file - filenum + 1;	
		dprintf( 
			(stdout,"BSF %d\n",fdiff) );
	}
	if (fdiff == 0)
		return(0);
	mtcom.mt_count = fdiff;
	if (ioctl(fd, MTIOCTOP, &mtcom) == -1) {
		perror("file search");
		close(fd);
		exit(1);
	}
	/* if searched for a file mark in reverse then must do
	a record forward to get pass the file mark */
	if (mtcom.mt_op == MTBSF) {
		mtcom.mt_op = MTFSF;
		mtcom.mt_count = 1;
		if (ioctl(fd, MTIOCTOP, &mtcom) == -1) {
			perror("file search");
			close(fd);
			exit(1);
		}
	}
		
}

/*
 * search_for_block
 */
search_for_block(fd, recnum)
{
	extern 	int current_block;
        struct mtop  mtcom;
	int bdiff;

	dprintf(
		(stdout, "current rec = %d recnum = %d\n",current_block, recnum) );
	if (recnum >= current_block) {
		bdiff = recnum - current_block;
		mtcom.mt_op = MTFSR;
		dprintf(
			(stdout,"FSR %d\n",bdiff) );
	}
	else {
		mtcom.mt_op = MTBSR;
		bdiff = current_block - recnum;	
		dprintf(
			(stdout,"BSR %d\n",bdiff) );
	}
	if (bdiff == 0)
		return(0);
	mtcom.mt_count = bdiff-2;
	if (ioctl(fd, MTIOCTOP, &mtcom) == -1) {
		perror("rec search");
		close(fd);
		exit(1);
	}
}

/*
 * Read/Write routine handle end of tape 
 */
myio(fn, fdptr, ptr, n, eot)
int	(*fn)();
int	*fdptr;
char	*ptr;
int	n;
char	eot;		/* 0 -> don't change tapes */
{
	int     fd = *fdptr;
	int	i;
	int	j;
	int	tty;
	static	char what[80];
	char	*s;
	extern int errno;

	i = (*fn)(fd, ptr, n);
	if (i != n ) {
		fflush(stdout);
		fprintf(stderr,"myio(%s): wanted %d bytes, got %d\n",
			   fn == read ? "read" : "write", n, i);
		if (i < 0) {
			fprintf(stderr,"\terrno is %d", errno);
			perror(" ");
		}
		fflush(stderr);
		if ( (i != -1) && !((i == 0) && (fn == read)) ) {
			fprintf(stderr,"bash: short %s, wanted %d, got back %d\n",(fn == read) ? "read" : "write",n,i);
			exit(1);
		}
		if ( i == -1 && errno != ENOSPC ) 
			exit(1);
		if ( eot == 0 )
			return(i);
		if ((tty = open("/dev/tty", 2)) < 0 && !isatty(tty = 0)) {
			fprintf(stderr, "bash: cannot prompt for new tape\n");
			exit(1);
		}
		/* close output device */
		close(fd);
		for (;;) {
			static char prompt[] =
			    "\007Change tape and press RETURN (or continue):";

			fflush(stdout);
			write(tty, prompt, sizeof prompt);
			what[sizeof what-1] = '\0';
			for (j=0; j<sizeof what-1; j++) {
				errno = 0;
				if (read(tty, what+j, 1) != 1) {
					if (errno > 0)
						perror("bash: no new tape");
					else {
						fprintf(stderr,
						    "bash: no new tape\n");
					}
					fprintf(stderr, "bash: tape %s error\n",
					    fn == read ? "read" : "write");
					exit(1);
				}
				if (debug > 1) {
					fprintf(stderr,"Read '%c'\n", what[j]);
					fflush(stderr);
				}
				if (what[j] == '\n') {
					what[j] = '\0';
					break;
				}
			}
			if (debug) {
				fprintf(stderr,"input '%s'\n",what);
				fflush(stderr);
			}
			if (!strcmp(what,"continue")) {
				if (debug) {
					fprintf(stderr,
					    "continuing on read error\n");
					fflush(stderr);
				}
				break;
			}
#ifdef NOTDEF
			/* Extremely dangerous toxenism. */
			if (what[0])
				usefile = what;
#endif
			if (!strcmp(devname,"-"))
				*fdptr = dup(fn == read ? 0 : 1);
			else {
				if (fn == read)
					*fdptr = open(devname,0);
				else {
					*fdptr = open(devname,1);
					if (*fdptr < 0)
						*fdptr = creat(devname,0666);
				}
			}
			if (*fdptr < 0) {
				fprintf(stderr,"bash: can't %s ",
				  fn == read ? "open" : "creat");
				perror(devname);
				fflush(stderr);
			} else {
				i = myio(fn, fdptr, ptr, n, eot);
				tapecount++;
				break;
			}
		}
		if (tty > 0)
			(void) close(tty);
	}
	return (i);
}
