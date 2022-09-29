/*	Silicon Graphics - System V - July 1987	*/
/*	S-record down load program from host to target machine via RS232 */
/*	Usage: sload Prom_device  */

#include <termio.h>
#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <a.out.h>

char sload_buf[1024];
int baudrate = 9600;
int verbose;
int reccount;

struct baud_pair {
	int baud;
	unsigned short c_cflag;
} map_table[] = {

	{1200, B1200},
	{2400, B2400},
	{4800, B4800},
	{9600, B9600},
	{19200, B19200},
	{38400, B38400}
};

#define NUMBAUD		(sizeof(map_table) / sizeof(struct baud_pair))


main (argc, argv)
register argc;
register char **argv;
{
	register FILE *fdProm, *fdInProm;
	register int c;
	struct termio term, prom, newterm, newprom;
	extern char *optarg;
	int baud;

	if (argc < 2 || argc >4) {
		fprintf(stderr,"usage: sload prom_device [-v] [ baudrate ]\n");
		exit(1);
	}

	argv++;	
	argc--;
	if ((fdInProm = fopen (*argv, "r")) == 0)
		exit (perror (*argv));
	if ((fdProm = fopen (*argv, "w")) == 0)
		exit (perror (*argv));
	setbuf (fdProm, NULL);		/* unbuffer output to Prom */

	while( --argc ) {
		argv++;
		if( strcmp(*argv,"-v") == 0 ) {
			printf("Verbose mode!\n");
			verbose= 1;
		}
		else {
			sscanf(*argv,"%d",&baudrate);
			if ( !(baud = inmap_table(baudrate)) ) {
				fprintf(stderr,"Unacceptable baudrate %d!\n",baudrate);
				exit(1);
			}
			printf("Down-load port baud rate is set at %d!\n",baudrate);
		}
	}
		
		
		
#if NOTDEF
	setbuf (stdin, NULL);
	setbuf (stdout, NULL);

	/* XXX not necessary?? */
	ioctl(0, TCGETA, &term);
	ioctl(0, TCGETA, &newterm);
	newterm.c_iflag &= ~(ICRNL | IXOFF);
	newterm.c_oflag &= ~(OLCUC);
	newterm.c_lflag &= ~(ICANON | ISIG | ECHO);
	newterm.c_cc[VEOF] = '\01';
	newterm.c_cc[VEOL] = '\0';
	ioctl(0, TCSETAW, &newterm);
#endif

	ioctl(fileno(fdProm), TCGETA, &prom);
	ioctl(fileno(fdProm), TCGETA, &newprom);
	newprom.c_iflag &= ~(IXOFF);
	newprom.c_oflag &= ~(OPOST | OLCUC);
	newprom.c_cflag &= ~CBAUD;
	newprom.c_cflag |= map_table[baud].c_cflag;
	newprom.c_lflag &= ~(ICANON | ISIG | ECHO);
	newprom.c_cc[VEOF] = '\01';
	newprom.c_cc[VEOL] = '\0';
	ioctl(fileno(fdProm), TCSETAW, &newprom);

	sload (fdProm,fdInProm);
/*	
	ioctl(0, TCSETAW, &term);
*/
	ioctl(fileno (fdProm), &prom);
	fclose (fdProm);
	fclose (fdInProm);
	exit (0);
}


/*
 Assumes file to be down loaded is already in S-rec format, just download
 one record at a time and look for ACK from Prom  to decide whether it is
 necessary to retransmit that record
*/
sload (fdProm,fdInProm)
FILE *fdProm, *fdInProm;
{

	char filename [80];
	register FILE *fd;
	register i;
	register char c;
	
	printf ("File to download: ");
	i = 0;
	while ((c = getchar ()) != '\r' && c != '\n')
		filename [i++] = c, putchar (c);
	filename [i] = 0;
	puts ("\r");
	if ((fd = fopen (filename, "r")) == NULL)
		return (printf ("\r", perror (filename)));
	rec_xfer (fd,fdProm,fdInProm); 
	fclose (fd);
	return(0);
}

/*
  Read fdObj for S-rec, transmit to fdProm, read fdInProm for 'ACK', which
 indicates that the S-record was transmitted correctly, if not then retransmit
 the same S-rec.
*/
#define	ACK	0x6
#define	NAK	0x15

rec_xfer(fdObj, fdProm, fdInProm)
register FILE *fdProm, *fdInProm, *fdObj;
{
	char *sbuf = &sload_buf[0];		/* temp buffer to hold s-rec */
	char c;
	char done;

	do {
		done = read_srec(sbuf, fdObj);
loop:
		if ( fputs(sbuf,fdProm) == -1 ) {
			fprintf(stderr,"Error writing to Prom device!\n");
			exit(1);
		}
		if ( (c = (fgetc(fdInProm) & 0xff)) == NAK ) {	
			fprintf(stderr,"Restransmit S-record!\n");
			goto loop;
		}
		else if ( c == ACK ) {
			fprintf(stderr,"\r");
			if( verbose &&  (reccount/10)*10 == reccount )
				printf("Done %d records\r",reccount);
			continue;
		}
		else fprintf(stderr,"Un-recognized character received from Prom!\n");
	}
	while ( !done );
	printf("\n");
}


/*
 * in-line implementation of get_pair for speed
 */
#define DIGIT(c)	((c)>'9'?((c)>='a'?(c)-'a':(c)-'A')+10:(c)-'0')
#define	get_pair(fd) \
	( \
		c1 = fgetc(fd) & 0x7f, *p++ = (char)c1, \
		c2 = fgetc(fd) & 0x7f, *p++ = (char)c2, \
		byte = (DIGIT(c1) << 4) | DIGIT(c2), \
		csum += byte, \
		byte \
	)

read_srec(sbuf,fd)
register char *sbuf;
register FILE *fd;
{
	register length, address;
	register c1, c2, byte;
	int i, type, save_csum;
	static done = 0;
	int csum, client_pc;
	char *p = sbuf;

	reccount++;
	while ((*p = ((char)fgetc(fd) & 0x7f)) != 'S') {
		if (*p == EOF) {
			/*
			* Only check for errors here,
			* if user gave a bad device problem
			* will usually be caught.
			*/
			fprintf(stderr,"EOF on record %d before type 7 record!\n",reccount);
			return(-1);
		}
		continue;
	}
	csum = 0;
	type = fgetc(fd);
	p++;
	*p++ = (char)type;
	length = get_pair(fd);
	if (length < 0 || length >= 256) {
		printf("record %d: invalid length\n", reccount);
		return(-1);
	}
	length--;	/* save checksum for later */
	switch (type) {

		case '0':	/* initial record, ignored */
			while (length-- > 0)
				get_pair(fd);
			break;

		case '3':	/* data with 32 bit address */
			address = 0;
			for (i = 0; i < 4; i++) {
				address <<= 8;
				address |= get_pair(fd);
				length--;
			}
			while (length-- > 0)
				address = get_pair(fd);
			break;

		case '7':	/* end of data, 32 bit initial pc */
			address = 0;
			for (i = 0; i < 4; i++) {
				address <<= 8;
				address |= get_pair(fd);
				length--;
			}
			client_pc = address;
			if (length)
				printf("record %d: type 7 record with unexpected data\n", reccount);
			done = 1;
			printf("Done: %d records, initial pc: 0x%x\n", reccount-1, client_pc);
			break;
		
		default:
			printf("record %d: unknown record type, %c\n",
			    reccount, type);
			break;
		}
	save_csum = (~csum) & 0xff;
	csum = get_pair(fd);
	if (csum != save_csum) 
		fprintf(stderr,"record %d: checksum error, calculated 0x%x, received 0x%x\n", reccount, save_csum, csum);
	if (done) 
		return(1);
	return(0);
}


inmap_table(x)
{
	int i;

	for( i=0; i<NUMBAUD; i++ ) {
		if( map_table[i].baud == x )
			break;
	}
	if( i == NUMBAUD )
		return(0);
	return(i);
}
