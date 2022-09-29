#ident "$Revision: 1.11 $"

/*
 * dev -- read/write test on devices
 */

#include <arcs/io.h>
#include <arcs/errno.h>
#include <libsc.h>

/* default buffer */
				
#define BLK_SIZE 4096
char read_buf[BLK_SIZE];
char write_buf[BLK_SIZE];

/* dev [-d pattern] [-w wbuf] [-r rbuf] [-b blkcnt] device */
int
dev_test(int argc, char **argv)
{
	ULONG src_fd, dst_fd;
	int totcnt, discnt;
	char *wbuf = 0;
	char *rbuf = 0;
	unsigned blocknum = 0;
	unsigned pattern = 0;
	unsigned int count;
	unsigned int data;
	unsigned int *ptr, *ptr2;

	argc--; argv++;

	for (; argc > 0 && **argv == '-'; argc--, argv++) {
		switch ((*argv)[1]) {
		case 'd':	/* set data patttern */
			if (--argc <= 0)
				return(1);
			switch (*atobu(*++argv, &pattern)) {
			case NULL:
				break;
			default:
				return(1);
			}
			printf("Test pattern is 0x%x\n", pattern);
			break;

		case 'b':	/* set number of blocks to be read/written */
			if (--argc <= 0)
				return(1);
			if (*atobu(*++argv, &blocknum) || blocknum <= 0 ) {
				printf("bad count: %s\n", *argv);
				return(0);
			}

			printf("Read/Write test for %u blocks of 4K\n",blocknum); 
			break;

		case 'r':	/* read buffer address */
			if (--argc <= 0)
				return(1);
			if (*atobu(*++argv, (unsigned *)&rbuf)) {
				printf("bad address: %s\n", *argv);
				return(0);
			}
			printf("4K Read buffer at 0x%x\n",rbuf); 
			break;

		case 'w':	/* write buffer address */
			if (--argc <= 0)
				return(1);
			if (*atobu(*++argv, (unsigned *)&wbuf)) {
				printf("bad address: %s\n", *argv);
				return(0);
			}
			printf("4K Write buffer at 0x%x\n",wbuf); 
			break;
			
		default:
			return(1);
		}
	}

	if (argc != 1)
		return(1);

	if (blocknum == 0 )
		return(0);
	if (pattern == 0) {
		data = 0;
		printf("Test pattern is inverted address\n");
	}

	if ( rbuf == 0 )
		rbuf = read_buf;
	if ( wbuf == 0 )
		wbuf = write_buf;

	if (Open ((CHAR *)*argv, OpenWriteOnly, &dst_fd) != ESUCCESS) {
		printf("couldn't open %s for writing\n", *argv);
		return(0);
	}

	totcnt = 0;
	discnt = 0;

	for ( count = 0; count < blocknum; count++ ) {
		ULONG wcnt;

		/* init wbuf with data pattern */
		if (pattern != 0)
			for (ptr = (unsigned int *)wbuf; 
				ptr < (unsigned int *)(&wbuf[4*1024]);
				ptr++) 
				*ptr = pattern;
		if (pattern == 0)
			for (ptr = (unsigned int *)wbuf; 
				ptr < (unsigned int *)(&wbuf[4*1024]);
				ptr++) 
				*ptr = ~data++;
				
		if (Write(dst_fd, wbuf, BLK_SIZE, &wcnt) != ESUCCESS
		    || wcnt != BLK_SIZE) {
			printf("write error, ");
			prcuroff(dst_fd);
			break;
		}
		printf (".");
		if (++discnt == 64) {
			printf("\n");
			discnt = 0;
		}
		totcnt ++;
	}
	/* done write */
	printf("\n%d (0x%x) blocks written\n", totcnt, totcnt);
	Close (dst_fd);

	if (Open ((CHAR *)*argv, OpenReadOnly, &src_fd) != ESUCCESS) {
		printf("couldn't open %s to verify written data\n", *argv);
		return(0);
	}
	/* now verify data */
	totcnt = 0;
	discnt = 0;
	data = 0;	/* used to generate addr pattern */

	for ( count = 0; count < blocknum; count++ ) {
		ULONG rcnt;

		/* init wbuf with data pattern */
		if (pattern != 0)
			for (ptr = (unsigned int *)wbuf; 
				ptr < (unsigned int *)(&wbuf[4*1024]);
				ptr++) 
				*ptr = pattern;
		if (pattern == 0)
			for (ptr = (unsigned int *)wbuf; 
				ptr < (unsigned int *)(&wbuf[4*1024]);
				ptr++) 
				*ptr = ~data++;
				
		/* zero out read buffer */
		bzero(rbuf,BLK_SIZE);

		if (Read(src_fd, rbuf, BLK_SIZE, &rcnt) != ESUCCESS 
		    || rcnt != BLK_SIZE) {
			printf("read error, ");
			prcuroff(src_fd);
			break;
		}
		/* verify data */
		for (ptr = (unsigned int *)wbuf, ptr2 = (unsigned int *)rbuf;
			ptr < (unsigned int *)(&wbuf[4*1024]);
			ptr++, ptr2++) {
			if ( *ptr != *ptr2 ) {
				printf("Data error, block = %u, expected %x , actual %x\n",count, *ptr, *ptr2);
				printf("Write Address = %x, Read address = %x\n",ptr, ptr2);
				printf("Start of Write Buffer = %x, Start of Read buffer = %x\n",wbuf, rbuf);
			}
		}
		printf (".");
		if (++discnt == 64) {
			printf("\n");
			discnt = 0;
		}
		totcnt ++;
	}
	/* done read */
	printf("\n%d (0x%x) blocks read\n", totcnt, totcnt);
	Close (src_fd);
	return(0);
}
