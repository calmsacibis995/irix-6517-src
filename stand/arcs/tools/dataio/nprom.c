/* nprom.c
 *
 * $Revision: 1.14 $
 *
 * TODO:
 *		Currently, if an error occurs while handler is programming a
 *		tube of parts, nprom loses track of the number of parts it
 *		has programmed. 
 *		(This is not a serious deficiency, as the actual number of
 *		parts successfully programmed appears on the LCD display of
 *		the handler). 
 *		Need to write/fix code so it queries the handler for the 
 *		number of parts programmed before it quits.
 * HISTORY:
 *
 * mod 14oct93 hal Preserve initial tty settings when switching to raw tty
 *		   input (settings which were not explicitly set were getting
 *		   clobbered with garbage from the stack).
 * mod 07oct93 hal With Robert Stephen's help, identified & fixed a bug where
 *		   the openprom() routine stomped on the kernel tty driver's
 *		   concept of what the XON/XOFF characters were (we were
 *		   clobbering them to zero).  We should not receive XON/XOFF
 *		   characters anymore, but I'll leave a printf in there just
 *		   in case.
 * mod 29sep93 hal Incorporate spencer's bug fixes; don't require -h flag
 *		   (for handler/labeler) when the PAL JEDEC format is used
 *		   (this is the default format, and may also be specified by
 *		    the -J flag).
 * mod 18apr89 davidl Major update of nprom to be able to talk to the Data I/O
 *		      Unisite 40 programmer (which has the same remote computer
 *		      programming language as the 29B) and to control the 
 *		      Autolabel 2000 handler/labeler.
 * mod 27jan88 hal Fix bug in the "Set set size", "Set word size" capability;
 *		   the device block size was incorrectly getting set to the
 *		   length of the input file (which is going to be larger than
 *		   what can be contained in a single prom); it needed to be set
 *		   to the maximum device size.  Since the Programmer only seems
 *		   to work correctly if we use the device size defaults, we
 *		   don't bother setting device size when either the set size or
 *		   word size flags have been used.
 * mod 26jan88 hal Support prom devices larger than 64K by adding a fifth hex
 *		   digit capability to Sendhex().
 * mod 17nov87 hal Modify JEDEC downloads so that we transmit a computed check
 *		   sum when compiled with SVR3.  The checksum placed into the
 *		   JEDEC file by ABEL is currently generated with VAX byte
 *		   ordering.  If we ever port ABEL to a 68K or MIPS byte order
 *		   machine, we could switch back to using the file's checksum
 *		   (although the computed checksum will always work).
 * mod 26oct87 dwong Fix the SVR3 tty settings so that they work properly.
 * mod 15oct87 jsm Support Intel MCS-85 files as well as Intel-MCS86 files.
 * mod 19may87 jsm Add support for Intel MCS-86 format PROM files.
 * mod 18may87 hal Add support for JEDEC files and alternative DATA I/O 
 *		   prom burner device ports.
 * mod 04may87 hal Add SVR3 support as a compile-time option. (doesn't work yet)
 *
 * nprom: prom/pal burning program for DATA I/O.
 *
 * Two versions of nprom exist; this one should be a superset of the other, now
 * that it supports Intel-MCS85 format PROM files.  The old version of nprom
 * required that you run "nglean" or "nfuse" to convert a formatted file into
 * raw binary; this version simply accepts the Intel format files.
 * 		hal 15oct87
 *
 *	-- found in ~milo/burn.
 *	   Another version lives in ~bruce/src.
 *	   We will attempt to reconcile the two versions, after we figure
 *	   out and document what each does...
 *		bert 5/15/85
 *
 *	-- Other versions found in ~donovan/tmp and ~nyles/c
 *	   A proto version is rumored to exist in bart's old files, but I
 *	   haven't located it yet.
 *		bert 5/21/85
 *
 *
 *	usage:  nprom [flags] < inputfile
 *
 *	flags:
 *		-a	Alternative DATA I/O device flag.
 *			This flag should be followed by the name of the RS-232
 *			device to be used for downloading data.  By default,
 *			nburn will send data to/from /dev/prom.  If you specify
 *			"-a/dev/prom2", data will be sent to/from the DATA I/O
 *			in Building 3's lab.
 *
 *		-b	blank check flag.
 *			This flag has meaning only if "-w" is also specified.
 *			If set, it causes a test to be run in the prom burner
 *			("the blank check") that checks if the prom is clean
 *			before prom burning starts.
 *			If not set, the prom burner still does an "illegal bit
 *			check" to ensure that the original contents of the prom
 *			and the new data are consistent.
 *
 *		-c	Family/pinout code flag.
 *			If set, a parameter follows (with or without a space
 *			as a separator) that identifies the family/pinout code
 *			for the prom that the DATAI/O machine needs to know.
 *			This parameter is a 4-digit hex number.  If the part
 *			type is given with the -t flag, the family/pinout code 
 *			need not be specified explicitly.
 *			The -c option is useful if: the prom part type is not
 *			in the "devicecodes" file; if the table has the wrong
 *			family/pinout code in it; if you want to experiment with
 *			different family/pinout codes.
 *
 *			DATAI/O company will tell you family/pinout codes for
 *			newly released devices.  (408) 437-9600 is their number.
 *			Sometimes a new adaptor gadget is required as well.
 *
 *			Upon calling DATA I/O, I was referred to their product
 *			support number: 1-800-247-5700.  I talked with a guy
 *			named Mike Swensen.  This was for computer remote 
 *		        control questions; you use the other number to get 
 *			family/pinout codes. (18may87 hal)
 *
 *		-d:	debug flag.
 *			This echos to standard output all data sent to the
 *			prom burner.
 *
 *		-f:	fast flag.
 *			When set, we'll attempt to use 19.2 KBaud for quick
 *			transfers.  This only seems to work for PROMs, as
 *			PALs don't seem to get programmed correctly.
 *
 *		-l	Label flag.
 *			This flag is used only when the -R flag is set.
 *			This flag should be followed by a string identifier
 *			for the device being programmed, (e.g. INTVEC or
 *			070-0223-002 for a device named INTVEC).
 *
 *		-h	Handler flag.
 *			This flag indicates the handler is present, so set
 *			up the handler before attempting communication with
 *			the Data I/O machine.
 *
 *		-n	(like -n in make)
 *			Does not actually burn a prom/pal, but goes through
 *			some of the motions and returns the device code
 *			based on the other flags and based on whatever the
 *			prom burner thinks the device code is.
 *
 *		-r	read flag.
 *			If the flag is set, the program will read the contents
 *			of a previously burned prom into the internal memory of
 *			the prom burner for purposes of verification or the
 *			production of copies.  The contents are printed on
 *			standard output.  If the flag is not set, the -w flag
 *			applies.
 *
 *		-p	pal checksum flag
 *			This flag should be followed by the checksum of the
 *			pal that is being programmed.  Since the checksum of
 *			data in the Data I/O ram does not match the checksum
 *			of the pal, we must pass the pal checksum externally.
 *			This flag is intended to be used in a production
 *			environment along with the -R flag.
 *
 *		-s	start address flag.
 *			If set, a parameter follows (with or without a space
 *			as a separator) that identifies the starting device
 *			address for burning/reading the prom.  All contents of
 *			the prom before the specified start address will remain
 *			untouched.
 *			The start address is specified in decimal -- not sure
 *			why.
 *			
 *		-Z	swap bytes flag.
 *			After loading the Data I/O with data, swap the
 *			bytes in its ram.
 *
 *		-t	type flag.
 *			If set, a parameter follows (with or without a space
 *			as a separator) that identifies the type of prom alpha-
 *			betically, e.g. as AMPAL16L8 or as TI27128A.
 *			This type is looked up in a table that is read in from
 *			the "devicecode" file.  The prom program finds the
 *			family/pinout code for the prom from the same file.
 *			(see -c flag).
 *
 *		-v:	verbose flag.
 *			Causes some extra output, but far short of full
 *			debugging information.
 *
 *		-w	write flag.
 *			If the flag is set, the program will burn a prom based
 *			on desired contents (provided by standard input).
 *			This is the default.
 *
 *			HACK:
 *			for backward compatibility, specifying -w will also turn
 *			on the -X flag.
 *
 *		-D	decimal format flag.
 *			input format on standard input (with -w), or output
 *			format on standard output (without -w) will be decimal.
 *			Default format is unformatted.
 *
 *		-F      Security fuse option
 *
 *		-I      Intel 86 hex format option.
 *			input on standard input (with -w) will be in standard
 *			MCS-86 hexadecimal object format.
 *
 *		-L      Intel 85 hex format option.
 *			input on standard input (with -w) will be in standard
 *			MCS-85 hexadecimal object format.
 *
 *		-J	JEDEC format option.
 *			input format on standard input (with -w) will be a
 *			standard JEDEC file as generated by ABEL or PALASM.
 *
 *		-R	Operator Repeat flag. This flag should be followed by
 *			the name of the RS-232 port that the operator is running
 *			on. With this option, nprom will prompt the operator
 *			whether or not to burn another copy of the PLD.
 *			By default, nburn will prompt for input from /dev/ttyd0.
 *			This option is useful when burning more than one
 *			copy of a PLD.
 *
 *		-S:	set Set size.
 *			If set, a parameter follows (with or without a space
 *			as a separator) that specifies the set size.
 *			This is used if the number of words (see -W option) in
 *			the input file is more than fits in a single prom and
 *			you want to split up the address range in contiguous
 *			pieces.  The set size is the number of such pieces.
 *			So, the difference between word size and set size is
 *			that word size controls interleaving and set size
 *			controls partitioning of the address range into
 *			appropriately sized blocks.
 *			Example: if you want to download a file of 16K words,
 *			each word being 2 bytes wide, in order to program them
 *			into a set of 4Kx8 bit PROMs, you would select a set
 *			size of 4 (= 16K/4K) and a word size of 16 (= 2*8).
 *
 *			Setting the set size also changes the default for the
 *			word size from 8 to 16, for unknown historical reasons.
 *
 *		-O:	octal format flag.
 *			input format on standard input (with -w), or output
 *			format on standard output (without -w) will be octal.
 *			Default format is unformatted.
 *
 *		-U	unformatted (binary format) flag.
 *			input format on standard input will be in binary, each
 *			byte of input corresponding directly to the data to be
 *			burned at a single address in the prom.
 *			This is the default format (but see also -w ).
 *
 *		-W:	set Word size.
 *			If set, a parameter follows (with or without a space
 *			as a separator) that specifies the word size.
 *			The word size is the number of bits (must be a multiple
 *			of 8) that have the same prom address; e.g. if the word
 *			size is 16, you want all odd-numbered bytes in the input
 *			file to go to the first (8-bit wide) prom and all even-
 *			numbered bytes to the second prom.  So the word size
 *			selects the degree of interleaving of bytes over
 *			different proms.
 *
 *		-X:	hexadecimal format flag.
 *			input format on standard input (with -w), or output
 *			format on standard output (without -w) will be hex.
 *			Default format is unformatted.
 *
 */

#define EOS	'\0'
#define STX	 02
#define ETX	 03
#define EOL	'\n'
#define CR	'\r'
#define TAB	'\t'
#define BLANK	' '
#define xCR	0x0D
#define xNL	0x0A

#define FALSE	0
#define TRUE	1

#define OK	0
#define FAIL	1

#define EQUAL	0
#define streq(a,b)	(strcmp(a,b)==EQUAL)

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <bstring.h>
#include <string.h>
#include <fcntl.h>
#define NULLARG		0
#define Eputc(c)	putc(c,stderr)
#define Eprintf(f,i)	fprintf(stderr,f,i)
#define Warn(str)	Eprintf("%s\n",str)
#define Message(f,i)	{if(vflg)Eprintf(f,i);}
#define Fail(f,i)	{Eprintf(f,i); exit( FAIL );}
int 	fpd;		/* file descriptors for prom device */
FILE	*fpw;					/* prom device, for writing */
FILE	*fpr;					/* prom device, for reading */
FILE	*fpusr_w;				/* usr tty device, for writing */
FILE	*fpusr_r;				/* usr tty device, for reading */

#define PrepareToReceive	fflush(fpw)	/* to make sure prom has gotten
						 * all output buffered up for it
						 * before reading its response
						 */
#define PrepareToReceiveUsr	fflush(fpusr_w)	/* to make sure usr port has gotten
						 * all output buffered up for it
						 * before reading its response
						 */
#define Receiveusrchar()	fgetc(fpusr_r)
#define Receiveval(p)	fscanf(fpr,"%x",p)	/* p must be pointer to int */
char	icvt[] = "%x";		/* format for reading standard input */
char	ocvt[] = "%02x\n";	/* format for printing standard output */
#define setinputformat(c)	icvt[1] = c	/* c is 'd', 'o' or 'x' */
#define setoutputformat(c)	ocvt[3] = c	/* c is 'd', 'o' or 'x' */
#define readval(p)		scanf(icvt,p)	/* p must be pointer to int */
#define printval(x)		printf(ocvt,x)

#define bool	int

bool	dflg = FALSE;			/* debug flag */
bool	dldelay = FALSE;

#define steparg()	--argc, ++argv
#define arg		argv[0]
#define dscan(s,pv)	sscanf(s,"%d",pv)
#define xscan(s,pv)	sscanf(s,"%x",pv)
#define getnumarg(pv)	{ if(arg[2]!=EOS) dscan(&arg[2],pv);	\
			  else { steparg(); dscan(&arg[0],pv); }}
#define gethexarg(pv)	{ if(arg[2]!=EOS) xscan(&arg[2],pv);	\
			  else { steparg(); xscan(&arg[0],pv); }}
#define getstrarg(ps)	{ if(arg[2]!=EOS) ps = &arg[2];		\
			  else { steparg(); ps = &arg[0];      }}


unsigned char	*data;

#define XFMT	0x50	/* translation format: ASCII hex space STX ETX */
#define JFMT	0x91	/* translation format: Jedec (full) */
#define I85FMT	0x83	/* MCS-85 format   */
#define I86FMT	0x88	/* MCS-86 format   */

#define PROMFILE "/dev/prom"
#define USRDEV "/dev/ttyd0"

char *devName;		/* Alternative DATA I/O device (e.g "/dev/prom2"). */
char *usrport;		/* user's login port that is running this program */
char *Label;		/* label (description) of device being programmed */
char *Labelfile;	/* Autolabel file of device being programmed */
char *palchecksum;	/* check sum of pal being programmed */
FILE *fplabel;		/* file pointer to label file */

bool aflg;
bool fast;		/* 19.2 KBaud switch "-f" command line opt */
bool ALflg;		/* flag indicating Autolabel file name was passed */
bool	usrRepeatflag = FALSE;	/* flag to prompt usr to burn
				 * another copy of the PLD. */
bool	palcsflag = FALSE;	/* flag indicating pal checksum was passed */
bool	labelflag = FALSE;	/* flag indicating device label was passed */


struct trans {
	char	type[12];
	int	code;
} tab[256];


void openprom(void);
void openusrport(void);
void Getchecksum(void);
void checkresponse(char *s);
void checkhandresp(char *s);
int check_pl(void);
char Receivechar(void);
char GetChar(int fd);
unsigned int Receivehex(void);
void Sendchar(char c);
void Sendcmd(char c);
void Sendhcmd(int x);
void dputchar(unsigned char c);
void Sendhex(int x);
void Send6hex(int x);
void Send2hex(int x);
void Send3dec(int d);
void dprinthex(unsigned int x);
void fail(char *m);
void readdevicecodes(void);
int devlookup(char *dname);


int
main (argc,argv)
int argc;
char *argv[];
{
    int	code;			/* 4 hex digits, specifying family/
				 * pinout code of device and identifying
				 * the device to the prom burner
				 */
    unsigned int  bitmask;	/* number of (rightmost) bits of input
				 * to burn at any given prom address.
				 */
    int	startaddr = 0;		/* starting device address for burning.
				 * defaults to zero, but can be set
				 * with "-s" flag.
				 */
    int	maxaddr;		/* max address of prom/pal device.
				 * Its value is obtained from the prom
				 * burner as a response from sending the
				 * family/pinout code to it.
				 */
    int	ndata;			/* block size.  This is the range of addresses
				 * that we want to burn or read.  It is the
				 * number of bytes of data on input (when
				 * burning a prom).
				 * Its value is derived from the starting
				 * address and the max address of the device.
				 */
    int	width;			/* width (in bits) of device (usually 8 for
				 * proms); obtained from the prom burner,
				 * as a response from sending the family/
				 * pinout code to it.
				 * This is the number of bits that have
				 * the same address.
				 */
    int	defaultbit;		/* unprogrammed state of the device; obtained
				 * from the prom burner, as a response from
				 * sending the family/pinout code to it.
				 */
    int setsize = 1;
    int wordsize = 8;

#define CHR_FMT	0		/* character, not binary */
#define UN_FMT	1		/* unformated binary */
#define INTEL_FMT 2
#define JEDEC_FMT 3
#define SPEC_FMT 4		/* specified format */
#define AL_LABELCON	0x01
#define AL_SETUPLABEL	0x02
#define AL_LABEL	0x03
#define AL_FIRMREV	0x04
#define AL_EXREMOTE	0x05
#define AL_LABELPLACE	0x06
#define AL_BINASSIGN	0x07
#define AL_BINLMAP	0x08
#define AL_SERIALIO	0x09
#define AL_TRANSPARENT	0x10
#define AL_EXTRANSP	0x19
#define MAXTUBEPARTS	0x16

    int		InFormat = UN_FMT;  /* Default to unformatted input */
    bool	SecurityFuse = FALSE;	/* Default to no security fuse */
    int		I86Flg;		/* Default to unformatted input */
    bool	bflg = FALSE;	/* blankcheck flag */
    bool	hflg = FALSE;	/* Handler/Labeler flag */
    bool	rflg = FALSE;	/* read flag */
    bool	nflg = FALSE;	/* no, echo family&pinout */
    bool	vflg = FALSE;	/* verbose flag */
    bool	Sflg = FALSE;	/* set size flag */
    bool	Wflg = FALSE;	/* word size flag */
    bool	Zflg = FALSE;	/* swap bytes flag */
    bool	codeset = FALSE;/* helps avoid setting family/pinout
				 * code inconsistently.
				 */

    int		format_byte;	/* specified format */

    int	sum;			/* checksum computed by this program */
    int	osum;			/* checksum computed by prom burner */
				/* (both should come out the same) */
    int	partslabeled;		/* # of parts programmed and labeled */
    int i, j;
    int lblfilefd;
    int	val;
    int tubeparts = 0;		/* # of parts in tube when using Handler */
    int PLDcopy = 0;		/* # of copies of the PLD made thus far */
    unsigned char	*dap;
    unsigned char	*response;
    unsigned char	*handptr;
    unsigned char	c;
    unsigned char usrchar[80];
    unsigned char tmpstring[80];
	int bytesRead, no_bytesRead;
	unsigned char ldata;	/* tmp char from autolabel label file */

    fast = FALSE;		/* Default to 9600 baud */
    aflg = FALSE;		/* aflg (Alternative DATA I/O device flag) 
				 * is declared as a global. 
				 */
    ALflg = FALSE;		/* Autolabel file flag */
    /* scan command line for arguments */

    steparg();			/* skipping over program name */
    while( argc>0 ) {
	if( arg[0] != '-' )
		Fail("nprom: bad option %s\n",&arg[0]);

	switch( arg[1] ) {

	case 'a': 			/* alternative Data I/O device flag */
		getstrarg(devName);
		aflg = TRUE;
		break;

	case 'f':			/* 19.2KBaud flag */
		fast = TRUE;
		printf("Make sure the programmer baud rate is set to 15...\n");
		break;

	case 'h':			/* Handler/Labeler present */
		hflg = TRUE;
		break;

	case 't': {			/* set device type */
		char	*dname;

		if( codeset )
			fail("code already set, Goodbye");
		getstrarg(dname);
		code = devlookup(dname);
		codeset = TRUE;
		break;
	}

	case 'c':			/* set family/pinout code */
		if( codeset )
			fail("code already set, Goodbye");
		gethexarg(&code);
		codeset = TRUE;
		break;

	case 's':			/* specify start address */
		getnumarg(&startaddr);
		break;

	case 'b':			/* perform optional blank check */
		bflg = TRUE;
		break;

	case 'r':			/* read, not write, a prom */
		rflg = TRUE;
		break;

	case 'w':			/* write, not read, a prom */
		rflg = FALSE;
		break;

	case 'A': 			/* alternative Data I/O device flag */
		getstrarg(Labelfile);
		ALflg = TRUE;
		break;

	case 'L':			/* Intel MCS-85 input format option */
		InFormat = INTEL_FMT;
		I86Flg = FALSE;
		break;

	case 'F':			/* Security fuse option */
		SecurityFuse = TRUE;
		break;

	case 'I':			/* Intel MCS-86 input format option */
		InFormat = INTEL_FMT;
		I86Flg = TRUE;
		break;

	case 'J':			/* JEDEC input format option */
		InFormat = JEDEC_FMT;
		break;

	case 'T':
		InFormat = SPEC_FMT;
		gethexarg(&format_byte);
		break;

	case 'U':			/* unformatted input option */
		InFormat = UN_FMT;
		break;

	case 'D':			/* decimal format flag */
		InFormat = CHR_FMT;
		setinputformat('d');
		setoutputformat('d');
		break;

	case 'X':			/* hexadecimal format flag */
		InFormat = CHR_FMT;
		setinputformat('x');
		setoutputformat('x');
		break;

	case 'O':			/* octal format flag */
		InFormat = CHR_FMT;
		setinputformat('o');
		setoutputformat('o');
		break;

	case 'R':			/* prompt usr to burn another prom */
		getstrarg(usrport);  
		usrRepeatflag = TRUE;
		break;

	case 'l':			/* description (label) of device */
		getstrarg(Label);  
		labelflag = TRUE;
		break;

	case 'p':			/* checksum of pal device */
		getstrarg(palchecksum);  
		palcsflag = TRUE;
		break;

	case 'S':			/* set setsize */
		Sflg = TRUE;
		getnumarg(&setsize);

		if( setsize > 8 )
			fail("can't handle set size this big");

		if( !Wflg )
			wordsize = 16;
		break;

	case 'W':			/* set wordsize */
		Wflg = TRUE;
		getnumarg(&wordsize);
		if( wordsize&7 )
			fail("expect word size to be multiple of 8");
		break;

	case 'n':			/* suppress actual burning */
		nflg = TRUE;
		vflg = TRUE;
		break;
	
	case 'v':
		vflg = TRUE;		/* verbose flag */
		break;
	
	case 'd':
		dflg = TRUE;		/* debug flag */
		vflg = TRUE;		/* debug implies verbose */
		break;

	case 'Z':			/* swap bytes */
		Zflg = TRUE;
		break;

	default:
		fail("unknown option, Goodbye");
	}

	steparg();
    }

    if( setsize * wordsize > 64 )
	fail("burner can't handle this many sets and words");

    Message("family&pinout = %4.4x\n", code);

    openusrport();
    openprom();		/* open unisite (or handler) tty port */

    if (hflg)		/* Set up handler */
    {
	/* If we are using the handler, we must download label information */
	if (hflg) {
		/* clear char buffer */
		for (i=0; i<10; i++)
			usrchar[i] = 0;
		tubeparts = 1;  /* default is to program 1 part */
		if (usrRepeatflag)
		{
		    Eprintf("How many parts to program? ",NULLARG);
		    PrepareToReceiveUsr;
		    response = usrchar;
	
		    while( *response=Receiveusrchar() ) {
#ifdef NOTYET
			fprintf(stderr,"I just read %c (= 0x%x)\n", *response, *response);
#endif
			if( (*response==EOL)||(*response==CR) )
				break;
			response++;
		    }
		    *response = 0x0; /* terminate the string */
		    sscanf(usrchar,"%d",&tubeparts);
		}

		fprintf(stderr,"Program %d parts (%s)\n\r",tubeparts,Label);
	}

	/* See if handler is alive */
	if (dflg)
	{
		fprintf(stderr,"See if handler is alive (#1)\n\r");
	}
	Sendhcmd(AL_FIRMREV);		
	Sendchar(xCR);
	checkhandresp("exit handler transparent mode");
	if (dflg)
	{
		fprintf(stderr,"See if handler is alive (#2)\n\r");
	}
	Sendhcmd(AL_FIRMREV);	
	checkhandresp("Check Autolabel firmware revision");
	Sendchar(xCR);

	/* Now put handler into transparent mode */
	if (dflg)
	{
		fprintf(stderr,"Put handler in transparent mode\n\r");
	}
	Sendhcmd(AL_TRANSPARENT);		
	Sendchar(AL_EXTRANSP);		/* 0x19 == Ctrl y */
	checkhandresp("enter handler transparent mode");
    }

    Sendcmd('H');	
    checkresponse("initial prompt");

    Sendcmd('^');		
    checkresponse("clear all of RAM");

    if( codeset ) {
	if (code > 0xffff) {
	    Send6hex(code);
	}
	else {
	    Sendhex(code);
	}
	Sendcmd('@');		
	checkresponse("setting family&pinout");
    }

    switch(InFormat) 
    {
	case JEDEC_FMT:
	    Sendchar('0');
	    Send2hex(JFMT);           /* prepare for jedec fmt  */
	    Sendcmd('A');	
	    checkresponse("setting jedec format");
		break;

	default:
	    Sendchar('0');
	    Send2hex(XFMT);           /* prepare for hex fmt  */
	    Sendcmd('A');		
	    checkresponse("setting hex format");
		break;
    }

    Sendhex( 0 );
    Sendcmd(';');		
    checkresponse("setting dummy block size");

    Sendhex( 0 );
    Sendcmd(':');		
    checkresponse("setting dummy start addr");

    if( codeset ) {
	if (code > 0xffff) {
	    Send6hex(code);
	}
	else {
	    Sendhex(code);
	}
	Sendcmd('@');		checkresponse("setting family&pinout");
    }

    if( Sflg | Wflg ) {
	/* apparently need to set set/word sizes twice and read responses
	 * twice.  not clear to me (bert) who/what requires this.
	 */

	Sendchar('E');
	Sendchar('1');
	Sendcmd (']');
	Sendcmd ( setsize+'0' );	checkresponse("setting set size");

	Sendchar('E');
	Sendchar('2');
	Sendcmd (']');
	Sendchar( '0'+(wordsize/10) );
	Sendcmd ( '0'+(wordsize%10) );	checkresponse("setting word size");
    }

    /*
     * The 'R' command sends its response first, followed by the status
     * information.
     */
    Sendcmd('R');
    checkresponse("reading device size");

    PrepareToReceive;
    maxaddr	= Receivehex();			/* assume hex followed by '/' */
    width	= Receivehex();	 		/* assume hex followed by '/' */
    defaultbit  = Receivechar() - '0';		/* assume single digit */

    Message("maxaddr = %d, ", maxaddr); 
    Message("width = %d bits, ", width); 
    Message("unprogrammed state = %d\n", defaultbit); 


    if( nflg ) {
	Message("returning without programming device\n",NULLARG);
	exit( OK );
    }

    bitmask = ~((1<<width)-1);

    data = malloc(maxaddr+1);

    ndata = maxaddr - startaddr + 1;
    if( ndata<1 )
	Fail("nprom: startaddr (%d) exceeds device end\n",startaddr);

   if( !rflg ) {			/* write prom, after reading input */
	unsigned char *dapmax = data + ndata;

	if(InFormat!=JEDEC_FMT) {
	    if( InFormat==UN_FMT ) {
		int	nchunk;

		/* read binary data in big blocks for maximum efficiency */
		/* read as much as is there, but never more than ndata bytes */

		dap = data;
		while( ( nchunk=read(0,dap,ndata) ) > 0 ) {
			dap += nchunk, ndata -= nchunk;
			if( dflg )
				printf("\njust read %d data bytes on input\n",
					nchunk);
		}
		if( dflg )
			printf("\ndone reading unformatted input\n");

		dapmax = dap;
/*
 *		while( dap<dapmax )
 *			*dap++ = 0;
 */
	    } else if (InFormat==CHR_FMT) {
		int result;

		/*
		 * Read formatted input, either hexadecimal, decimal or octal.
		 * The format to be used is selected thru -X -D or -O options.
		 */
		
		for( dap=data ; dap<dapmax ; dap++ ) {
			result = readval(&val);
			if (result == EOF) {
				do {
					*dap = -defaultbit;
				} while (++dap<dapmax);
				break;
			}
			if (result != 1)
				fail("input not in selected input format");
			if (val & bitmask)
				fail("data wider than device");
			*dap = val;
		}

		/*
		 * We have now read all expected data from input to our memory.
		 * Check whether there is more data on input:
		 */

		if (result != EOF && readval(&val) != EOF )
			fail("unexpected extra data on input");
	    }
	}

	Sendhex( 0 );
	Sendcmd('<');		checkresponse("setting address of ram");
	if (InFormat==UN_FMT
	    || InFormat==CHR_FMT) {
	    ndata = dap - data;
	    /*
	     * Use the default device size if the user is using the set size
             * or word size features of nprom.  The default device size will
             * be the maximum capacity of the device (which is what we want).
	     */
	    if (Wflg)
	    if( Sflg | Wflg ) {
		fprintf(stderr, "skipping past setting of device size.\n");
	    } else {
		fprintf(stderr, "Number of bytes to transfer = 0x%x\n", ndata);
		Sendhex(ndata);
		Sendcmd(';');		
		checkresponse("setting device size");
	    }
	}

	Sendhex(startaddr);
	Sendcmd(':');		checkresponse("setting device start address");

	/*
	 * Transmit contents of our memory to DATAI/O ram.
	 */
Message("Now downloading data to Unisite ram\n",NULLARG);

	if (InFormat==JEDEC_FMT) {
	    int bytesRead, no_bytesRead;
	    unsigned char data;

#ifdef SVR3
	    sum = 0;
#endif
	    if (hflg) {
		Sendchar('I'); 
		Sendchar(0xd); 
	    } else {
		Sendchar('E');
		Sendchar('B');
		Sendcmd(']');
	    }

	    /* See Data I/O Unisite User Note for Data Download delay --DL */
	    /* Delay for at least 20 milliseconds */
	    sginap(3);

	    no_bytesRead =0;
	    while(( bytesRead=read(0, &data, 1) ) > 0 ) {
#ifdef SVR3
		if (data == ETX) {
		    sum += data;
		    no_bytesRead++;
		    Sendchar(data);	/* send ETX to the programmer */
		    /* 
		     * Ignore the transmit checksum encoded into the file by
		     * ABEL since it was calculated with VAX byte ordering.
		     * Instead, transmit a calculated checksum.
		     */
		    sum &= 0xffff;		/* checksum is mod 2^16 */
		    Message("\ncomputed checksum is %04x\n", sum);
		    Sendhex(sum);
		    break;
		}
		sum += data;
#endif
		if (data != 0x0) {
			Sendchar(data);	/* send that char to the programmer */
			no_bytesRead++;
		}
	    }
	    if( dflg )
		fprintf(stderr, "\ndone reading JEDEC input\n");

	} else if (InFormat==INTEL_FMT) {
	    int bytesRead;
	    unsigned char data;

	    if (I86Flg) {
		Send2hex(I86FMT);	/* Prepare for intel format if asked */
	    } else {
		Send2hex(I85FMT);	/* Prepare for intel format if asked */
	    }
    	    Sendcmd('A');
            checkresponse("setting Intel-86 format");

	    Sendcmd('I');	/* "input" data from computer to DATAI/O ram */

	    /* See Data I/O Unisite User Note for Data Download delay --DL */
	    /* Delay for at least 20 milliseconds */
	    sginap(20);

	    Sendchar(STX);

	    while(( bytesRead=read(0, &data, 1) ) > 0 ) {
		    Sendchar(data);	/* send that char to the programmer */
	    }
	    if (dflg)
		fprintf(stderr, "\ndone reading Intel-86 input\n");

	} else if (InFormat==SPEC_FMT) {
	    int bytesRead;
	    unsigned char data;

	    Send2hex(format_byte);	/* tell the box which format */
    	    Sendcmd('A');
            checkresponse("setting format");

	    Sendcmd('I');		/* "input" data to DATAI/O ram */

	    /* See Data I/O Unisite User Note for Data Download delay --DL */
	    /* Delay for at least 20 milliseconds */
	    sginap(20);

	    Sendchar(STX);

	    while(( bytesRead=read(0, &data, 1) ) > 0 ) {
		    Sendchar(data);	/* send that char to the programmer */
	    }
	    if (dflg)
		fprintf(stderr, "\nfinished reading input\n");

	} else {
	    Sendcmd ('I');	/* "input" data from computer to DATAI/O ram */

	    /* See Data I/O Unisite User Note for Data Download delay --DL */
	    /* Delay for at least 20 milliseconds */
	    sginap(20);

	    Sendchar(STX);
	    Sendchar('$');

	    Sendchar('A');
	    Sendhex ( 0 );
	    Sendcmd (',');
	}

	sum = 0;
	if (InFormat==UN_FMT
	    || InFormat==CHR_FMT) {
	    for( dap=data ; dap<dapmax ; dap++ ) {
		Send2hex(*dap);
		Sendchar(' ');
		sum += *dap;
	    }
	}

	if (InFormat != JEDEC_FMT) {
	    Sendchar(ETX); 	
	}

	if (InFormat==JEDEC_FMT) {
	    checkresponse("End of JEDEC download");
				    /* Programmer automatically checks
				       for valid checksum  */
        } else if (InFormat==INTEL_FMT) {
	    checkresponse("End of Intel-86 download");
				    /* Programmer automatically checks
				       for valid checksum  */

	} else if (InFormat==SPEC_FMT) {
	    checkresponse("End of download");
				    /* Programmer automatically checks
				       for valid checksum  */

	} else {
	    sum &= 0xffff;		/* checksum is mod 2^16 */
	    Message("\ncomputed checksum is %04x\n", sum);
	    /*
	     * Prepare to verify checksum
	     */
	    Sendchar('$'); 
	    Sendchar('S'); 
	    Sendhex (sum);
	    Sendcmd (',');		
	    checkresponse("DATAI/O checksum");
	}

	Message("The data has been downloaded to DATAI/O ram.\n",NULLARG);
	if (usrRepeatflag == TRUE) {
		PLDcopy = 0;
	}
	if (Zflg == TRUE) {
	    Sendchar('A');
	    Sendchar('7');
	    Sendcmd (']');		
	    Message("Swapping bytes in DATAI/O ram.\n",NULLARG);
	    checkresponse("DATAI/O byte swap");
	}

	/*
	 * All data is now in the DATAI/O internal ram.
	 * Now perform some device tests and then burn the prom.
	 */

	    if (hflg) {
		/* Exit transparent mode */
		if (dflg)
		{
		    fprintf(stderr,"Take handler out of transparent mode\n\r");
		}
		Sendchar(AL_EXTRANSP);		/* 0x19 == Ctrl y */
		checkhandresp("exit handler transparent mode");
	    }

	do {
	    if (hflg) {
		if (usrRepeatflag) 
		{
		    Eprintf("Insert a tube of blank devices in the proper Handler Tube Guide.\n",NULLARG);
		    Eprintf("Press <CR> when ready: ",NULLARG);
		    PrepareToReceiveUsr;
		    response = usrchar;
    	
		    while( *response=Receiveusrchar() ) {
#ifdef NOTYET
			fprintf(stderr,"I just read %c (= 0x%x)\n", *response, *response);
#endif
			if( (*response==EOL)||(*response==CR) )
				break;
			response++;
		    }
		    *response = 0x0; /* terminate the string */
		}

#ifdef NOTYET
		/* Setup handler bins */
		Sendhcmd(AL_BINASSIGN);
		Send2hex(0x01);	/* Bin #1 */
		Send3dec(1);	/* Cat 1 (Good Part) */

		Sendhcmd(AL_BINASSIGN);
		Send2hex(0x02);	/* Bin #2 */
		Send3dec(1);	/* Cat 1 (Good Part) */

		Sendhcmd(AL_BINASSIGN);
		Send2hex(0x03);	/* Bin #3 */
		Send3dec(2);	/* Cat 2 (Part Upside Down) */

		Sendhcmd(AL_BINASSIGN);
		Send2hex(0x04);	/* Bin #4 */
		Send3dec(28);	/* All other failures */

		/* Setup bin label mapping */
		Sendhcmd(AL_BINLMAP);
		Send3dec(5);	/* Label Bins 1 and 2 */
#endif
		/* Set up label on handler */
		Sendhcmd(AL_SETUPLABEL);
		Send2hex(MAXTUBEPARTS);
	
		/* Read info from autolabel label file */
		if (dflg)
		{
			fprintf(stderr,"Opening file %s\n",Labelfile);
		}
		if ((lblfilefd = open(Labelfile, O_RDONLY)) == -1)
		{
			fprintf(stderr,"Error opening label file %s\n",Labelfile);
			exit(0);
		}

		if (dflg)
		{
			fprintf(stderr,"Sending the autolabel file (%s)\n", Labelfile);
		}
		while(( bytesRead=read(lblfilefd, &ldata, 1) ) > 0 ) { 
	/*	    fprintf(stderr,"%c",ldata); */
		    Sendchar(ldata);	/* send label data to the handler */
		}
		checkhandresp("Setup labeler");

		/* Display bin info to operator */
		fprintf(stderr,"Bin 1 is for GOOD PARTS\n");
		fprintf(stderr,"Bin 2 is for GOOD PARTS\n");
		fprintf(stderr,"Bin 3 is for PARTS that are UPSIDE-DOWN\n");
		fprintf(stderr,"Bin 4 is for BAD PARTS \(Programming failed, ");
		fprintf(stderr,"Verify failed, Illegal bit test \n");
		fprintf(stderr, "                        failed, or some other problem)\n");

		/* Tell Handler/Labeler to program and label the parts */
	/*	fprintf(stderr,"Tell Handler/Labeler to program and label the parts\n\r"); */
		Sendhcmd(AL_LABEL);
		Send2hex(tubeparts);
		checkhandresp("Label parts");

#ifdef NOTYET
		for (i=1; i<tubeparts+1; i++)
		{
			Sendhcmd(AL_LABEL);
			Send2hex(1);
			checkhandresp("Label parts");

			/* Clear tmpstring */
			for (j = 0; j < 80; j++)
			{
				tmpstring[j] = 0;
			}

			/* Request # of parts programmed/labeled */
			Sendchar('#'); 

			partslabeled = check_pl();
		/*	fprintf(stderr,"partslabeled = %\n\rd",partslabeled); */
			if ( i != partslabeled)
			{
				i = partslabeled;
			}
		}
#endif

	    	if (usrRepeatflag == TRUE) {
		    PLDcopy++;
		    Eprintf("Tube %d completed.\n",PLDcopy);
		    Eprintf("\nDo you want to program another tube of this device? ",NULLARG);
		    PrepareToReceiveUsr;
		    response = usrchar;

		    while( *response=Receiveusrchar() ) {
			if( (*response==EOL)||(*response==CR) )
			    break;
			response++;
		    }
		    *response = 0x0; /* terminate the string */

		    switch(usrchar[0])
		    {
			case 'n':
			case 'N':
				Eprintf("Programming completed for this part\n",
					NULLARG);
				usrRepeatflag = FALSE;
				break;
			default:
				usrRepeatflag = TRUE;
				break;
		    }
	    	}
	    } else {
		if (usrRepeatflag == TRUE) {
		Eprintf("Insert a blank device in the proper Data I/O socket.\n",NULLARG);
		Eprintf("Press <CR> when ready: ",NULLARG);
		PrepareToReceiveUsr;
		response = usrchar;
	
		    while( *response=Receiveusrchar() ) {
#ifdef NOTYET
			fprintf(stderr,"I just read %c (= 0x%x)\n", *response, *response);
#endif
			if( (*response==EOL)||(*response==CR) )
				break;
			response++;
		    }
		*response = 0x0; /* terminate the string */
		}

		Message("Now testing device...",NULLARG);

		Sendcmd('T');	     checkresponse("DATAI/O illegal bit test");

		if( bflg ) {
		    Sendcmd('B');    checkresponse("DATAI/O prom blank check");
		}
	
		if (SecurityFuse) {
		    Sendchar('3');
		    Sendchar('2');
		    Sendchar('7');
		    Sendcmd(']');
		    checkresponse("enable security fuse");
		    Sendchar('2');
		    Sendchar('2');
		    Sendchar('6');
		    Sendcmd(']');
		    checkresponse("enable vector test only");
		}

		Message("device tested. \nNow programming device...",NULLARG);

		Sendcmd('P');		checkresponse("DATAI/O programming");

		Message("device programmed. \nNow verifying device...",NULLARG);

		Sendcmd('V');		checkresponse("DATAI/O verifying");
		Message("device verified.\n",NULLARG);
		if (labelflag == TRUE)
		{
			Eprintf("%s ", Label);
		}

		Getchecksum();

	    	if (usrRepeatflag == TRUE) {
		    PLDcopy++;
		    Eprintf("Copy %d completed successfully.\n",PLDcopy);
		    Eprintf("\nDo you want to program another copy of this device? ",NULLARG);
		    PrepareToReceiveUsr;
		    response = usrchar;

		    while( *response=Receiveusrchar() ) {
			if( (*response==EOL)||(*response==CR) )
			    break;
			response++;
		    }
		    *response = 0x0; /* terminate the string */

		    switch(usrchar[0])
		    {
			case 'n':
			case 'N':
				Eprintf("Programming completed for this part\n",
					NULLARG);
				usrRepeatflag = FALSE;
				break;
			default:
				usrRepeatflag = TRUE;
				break;
		    }
	    	}
	    }
	} while (usrRepeatflag == TRUE);

	if (SecurityFuse) {
	    Sendchar('0');
	    Sendchar('2');
	    Sendchar('7');
	    Sendcmd(']');
	    checkresponse("disable security fuse");
	    Sendchar('0');
	    Sendchar('2');
	    Sendchar('6');
	    Sendcmd(']');
	    checkresponse("re-enable fuse & vector test");
	}

	exit( OK );
    } else {
	/* read prom into ram */

	Sendhex(0);
	Sendcmd('<');		checkresponse("setting ram starting address");

	Sendhex(ndata);
	Sendcmd(';');		checkresponse("setting device size");

	Sendhex(startaddr);
	Sendcmd(':');		checkresponse("setting device start address");

	Sendcmd('L');		checkresponse("DATAI/O loading prom to ram");

	Sendcmd('O');		/* tell DATAI/O to start output */

	PrepareToReceive;
	dap = data;
	sum = 0;
	for( ;; ) {
		if( Receiveval(&val) == 0 ) {

			/* The DATAI/O is not sending us hex numbers anymore.
			 * Let's go find out what is going on:
			 */
			switch( Receivechar() ) {

			case '>':		/* DATAI/O sends prompt */
				fail("unexpected >");

			case '$':		/* DATAI/O says end-of-data */
				switch( Receivechar() ) {

				case 'S':	/* DATAI/O sending check sum */
					Receiveval(&osum);
					goto donereading;

				case 'A': {	/* Apparently an expected
						 * message, but I (bert) don't
						 * know what it means.
						 */
						int	junk;

						Receiveval(&junk);

						/* is the fall-through
						 * deliberate? -- bert
						 */
					}
				}
			default:
				continue;
			}
		}
		*dap++ = val;		/* is this needed? -- bert 06/23/86 */
		sum += val;
		if( InFormat==UN_FMT )
			putchar( val );
		else
			printval( val );
	}
donereading:
	{
		int	exitflg = OK;

		sum &= 0xffff;			/* checksum computed mod 2^16 */
		if( sum!=osum ) {
			/* checksum error */
			exitflg = FAIL;
			Eprintf("my checksum = 0x%x, ",sum);
			Eprintf("DATAI/O checksum = 0x%x\n",osum);
		}
		if( ndata != (dap-data) ) {
			/* insufficient data received */
			exitflg = FAIL;
			Eprintf("requested %d, ",ndata);
			Eprintf("received %d\n",(dap-data));
		}
		exit(exitflg);
	}
    }
}



/*
 * openprom -- open communications with DATAI/O prom burner
 *
 * The DATAI/O prom burner can be connected (and normally is) "remote" over an
 * RS232 line.
 * Programs can then communicate to it as if it were a terminal, using
 * standard i/o and using the proper line discipline.
 *
 * mod 04may87 hal Add SVR3 support as a compile-time option.
 */


#ifdef SVR3

#include <termio.h>
#include <termios.h>
struct termio tty_setting;

#else

#include <sgtty.h>
struct sgttyb vec;

#endif

void
openprom(void)
{
	int ldisc;

	if (aflg)
	    fpd = open(devName, O_RDWR);
	else
	    fpd = open(PROMFILE, O_RDWR);

	if( fpd<0 )
		fail("cannot open prom device");
	else if (dflg)
		printf("opened prom device\n");

#ifdef SVR3

    ioctl(fpd, TCGETA, &tty_setting);

    /*
     * If IXON is set, start/stop output control is enabled.  A received STOP
     * character will suspend output and a received START character will 
     * restart output.  All start/stop characters are ignored and not read.
     */

    tty_setting.c_iflag |= IXON | ISTRIP;	/* Use XON/XOFF flow control */
    tty_setting.c_oflag &= ~OPOST;	/* No post-processing of output */
    if (fast)
        /* crank it up! */
	tty_setting.c_cflag = (B19200 | CS8 | CREAD | HUPCL | CLOCAL);
    else
	tty_setting.c_cflag = (B9600 | CS8 | CREAD | HUPCL | CLOCAL);

    tty_setting.c_lflag = 0;	/* No line discipline necessary */
    tty_setting.c_cc[VMIN] = 0; /* raw mode tty */
    tty_setting.c_cc[VTIME] = 0; /* raw mode tty */
    tty_setting.c_cc[VSTART] = CSTART;
    tty_setting.c_cc[VSTOP] = CSTOP;
			 
    /*
     * When opening a modem flow control device (such as /dev/ttyf4), you
     * must send a break character before twiddling the output modes.  
     * Otherwise, the programmer gets confused & reports an "input overrun".
     */
    ioctl(fpd, TCSBRK, 0); /* Send a break character */

    if (dflg)
	printf("sent break\n");

    tcflush(fpd, TCIOFLUSH); /* Flush input and output queues */

    ioctl(fpd, TCSETAF, &tty_setting);
    if (dflg)
	printf("sent setaf\n");

#else
	vec.sg_ispeed = B9600;
	vec.sg_ospeed = B9600;
	vec.sg_flags  = ANYP|TANDEM|CRMOD;
	ioctl(fpd, TIOCSETP, &vec); 
#endif
	if( (fpr=fdopen(fpd,"r")) == NULL )
		fail( "cannot open prom device for reading");
	if( (fpw=fdopen(fpd,"w")) == NULL )
		fail("cannot open prom device for writing");
}

void
openusrport(void)
{
	int	dp;
	int ldisc;

	if (usrRepeatflag) {
	    dp = open(usrport, 2);
	    Eprintf("Opening usr port %s\n",usrport);
	}
	else
	    return;
/*	    dp = open(USRDEV, 2); */

	if( dp<0 )
		Fail("cannot open device %s", usrport);

	if( (fpusr_r=fdopen(dp,"r")) == NULL )
		Fail("cannot open device %s for reading", usrport);
	if( (fpusr_w=fdopen(dp,"w")) == NULL )
		Fail("cannot open device %s for writing", usrport);
}

void
Getchecksum(void)
{
	char c;
	char csbuf[14];
	int j,i=0;

	Sendchar('S');		
	Sendchar(xCR);		
	PrepareToReceive;
	while( (c=Receivechar()) != '>' )  /* Get chars until we see a prompt */
	{
		csbuf[i] = c;
/*		fprintf(stderr,"i = %x, c = %c (0x%x)\n", i, csbuf[i], csbuf[i]); */
		i++;
	}
	csbuf[i] = 0;  /* Terminate the string */


	Eprintf("Checksum: %s\n", csbuf);
}

/* checkresponse
 *
 * When sending a command to the prom burner, it usually returns with a prompt.
 * We intercept this prompt and interpret any error messages that come in.
 * The argument to "checkresponse" is a string to be printed out if something
 * goes wrong.
 *
 */
void
checkresponse(char *s)
{
	char c;
	int errorcode = 0;

#ifdef DEBUG
	if (dflg)
	    fprintf(stderr,"\ncheckresponse: waiting for '%s'\n", s);
#endif

	while (c = Receivechar()) {
#ifdef DEBUG
	    if (dflg)
		fprintf(stderr,"\ncheckresponse: %c (0x%x)\n", c, c);
#endif
	    switch(c) {
		case 0x13:
		case 0x11:
		    /*
		     * We should never actually see XON or XOFF, but
		     * just in case we do, delay a bit, then proceed.
		     *
		     * We had a bug in openprom() which clobbered the 
		     * kernel driver's concept of what XON/XOFF were, which
		     * is why (for a time) nprom received them from the
		     * DataI/O.
		     */
printf("XON/XOFF received.\n");
		    sginap(10);
		    continue;
		    break;
		case '>':  /* Data I/O prompt */
		    c = Receivechar();
		    if((c!=EOL) && (c != xCR))	
			fprintf(stderr,"checkresponse: got > prompt but no CR! (c=0x%x)\n",c);
		    return;
		case 'F':  /* Not Prompt -- failure of some sort */
		    c = Receivechar();  /* get CR */
		    Eprintf("\n\rFailure: %s\n",s);
		    Sendcmd('F');	/* provoke prom burner into displaying
					 * error code
					 */
		    fprintf(stderr,"Error Status Word = ");
		    PrepareToReceive;
		    while( c=Receivechar() ) {
			if (c == '>') {
				c = Receivechar();
		    		if((c!=EOL) && (c != xCR))	
				    fprintf(stderr,"checkresponse: got > prompt but no CR! (c=0x%x)\n",c);
				break;
			}
			else {
/* 				fprintf(stderr,"0x%02x\n\r",c); */
		    		if((c!=EOL) && (c != xCR))	
				   fprintf(stderr,"%c",c);
				else if (c==xCR)
				   fprintf(stderr,"\n\r",c);
/*				Sendcmd('H'); */
			}

		    }
		    fprintf(stderr,"\n");


		    Sendcmd('X');	/* terminate current transaction with
					 * the prom burner
					 */

		    fprintf(stderr,"CRC Error Code = ");
		    PrepareToReceive;
		    while( c = Receivechar() ) {
			    switch(c) {
				case '>':
				    break;
				case xNL:
				    Eputc(xNL);
				    break;
				default:
				    Eputc(c);
			    }
			    if( c==xNL ) {
				    break;
			    }
		    }
		    exit( FAIL );
		    break;

		case xNL:  /* End of Data I/O prompt */
		case xCR:  /* End of Data I/O prompt */
#ifdef DEBUG
		    if (dflg)
			fprintf(stderr,"\ncheckresponse: got CR!\n", c);
#endif
		    return;

		default: 
		    fprintf(stderr,
			"checkresponse(%s):  bad char was %c (0x%x)\n",
			s,c,c);
		    return;
		    break;
	    }
	}

	Eprintf("Failure: %s\n",s);
	if( c!='F' ) {
		Eprintf("bad char was %c\n",c);
		Eprintf("%o\n",c);
	}
	Sendcmd('F');			/* provoke prom burner into displaying
					 * error code
					 */

	PrepareToReceive;
	if( Receiveval(&errorcode) != 1 )
		fail("couldn't read error code");
	Eprintf("error code: 0x%x", errorcode);
	switch(errorcode)
	{
		case 0x20:
			Eprintf("\tDevice is non-blank\n",NULLARG);
			break;

		case 0x21:
			Eprintf("\tIllegal bit\n",NULLARG);
			break;

		case 0x22:
			Eprintf("\tUnable to program device\n",NULLARG);
			break;

		case 0x23:
			Eprintf("\tFirst-Pass Verify data error\n",NULLARG);
			break;

		case 0x24:
			Eprintf("\tSecond-Pass Verify error\n",NULLARG);
			break;

		case 0x25:
			Eprintf("\t\n",NULLARG);
			break;

		case 0x26:
			Eprintf("\tUnable to program device\n",NULLARG);
			break;

		case 0x27:
			Eprintf("\tEnd of User RAM exceeded\n",NULLARG);
			break;

		case 0x28:
			Eprintf("\tFatal Device Programming Error\n",NULLARG);
			break;

		case 0x29:
			Eprintf("\tNon-fatal Device Programming Error\n",NULLARG);
			break;

		case 0x2A:
			Eprintf("\tDevice Insertion Error\n",NULLARG);
			break;

		case 0x2B:
			Eprintf("\tStructured Test Error (Vcc nominal/low)\n",NULLARG);
			break;

		case 0x2C:
			Eprintf("\tStructured Test Error (Vcc high)\n",NULLARG);
			break;

		case 0x2D:
			Eprintf("\tSocket Module for device not installed\n",NULLARG);
			break;

		case 0x2E:
			Eprintf("\tProgramming Hardware has not passed self-test\n",NULLARG);
			break;

		case 0x2F:
			Eprintf("\tInsufficient pin dirver boards installed for the selected device\n",NULLARG);
			break;

		case 0x30:
			Eprintf("\tDevice algorithm not found\n",NULLARG);
			break;

		case 0x31:
			Eprintf("\tDevice over-current fault\n",NULLARG);
			break;

		case 0x40:
			Eprintf("\tI/O Initialization error\n",NULLARG);
			break;

		case 0x41:
			Eprintf("\tSerial-framing error\n",NULLARG);
			break;

		case 0x42:
			Eprintf("\tSerial-overrun error\n",NULLARG);
			break;

		case 0x43:
			Eprintf("\tSerial framing/overrun error\n",NULLARG);
			break;

		case 0x46:
			Eprintf("\tI/O timeout error\n",NULLARG);
			break;

		case 0x52:
			Eprintf("\tData verify error\n",NULLARG);
			break;

		case 0x81:
			Eprintf("\tSerial-parity error\n",NULLARG);
			break;

		case 0x82:
			Eprintf("\tData download checksum error\n",NULLARG);
			break;

		case 0x84:
			Eprintf("\tI/O (data translation) format error\n",NULLARG);
			break;

		case 0x88:
			Eprintf("\tInvalid number of parameters\n",NULLARG);
			break;

		case 0x89:
			Eprintf("\tIllegal parameter value\n",NULLARG);
			break;

		case 0x8B:
			Eprintf("\tUnable to program device\n",NULLARG);
			break;

		case 0x8E:
			Eprintf("\tUnable to program device\n",NULLARG);
			break;

		case 0x8F:
			Eprintf("\tUnable to program device\n",NULLARG);
			break;

		case 0x90:
			Eprintf("\tIllegal I/O format\n",NULLARG);
			break;

		case 0x94:
			Eprintf("\tData record error\n",NULLARG);
			break;

		case 0x97:
			Eprintf("\tUnable to program device\n",NULLARG);
			break;

		case 0x98:
			Eprintf("\tEnd of device exceeded\n",NULLARG);
			break;

		case 0x9A:
			Eprintf("\tAlgoritm diskette cannot be found\n",NULLARG);
			break;

		case 0x9B:
			Eprintf("\tIncompatible system/algorithm diskettes\n",NULLARG);
			break;

		case 0x9C:
			Eprintf("\tInvalid command for this mode\n",NULLARG);
			break;

	}


	Sendcmd('X');			/* terminate current transaction with
					 * the prom burner
					 */

	PrepareToReceive;
	Eputc(Receivechar());
	Eputc(Receivechar());
	Eputc(Receivechar());
	while( c=Receivechar() ) {
		Eputc(c);
		if( c==EOL ) {
		    Eputc(xNL);
			break;
		}
	}
	exit( FAIL );
}

/* checkhandresp
 *
 * When sending a command to the handler, it usually returns R followed by
 * some characters, and terminated with a newline.
 * We intercept this prompt and interpret any error messages that come in.
 * The argument to "checkresponse" is a string to be printed out if something
 * goes wrong.
 *
 */
void
checkhandresp(char *s)
{
	char c;
	char *errptr;
	char errtmp[10];
	int errcode = 0;
	int i;

	/* Clear errtmp */
	for (i = 0; i < 80; i++) {
		errtmp[i] = 0;
	}

	while (c=Receivechar()) {
#ifdef DEBUG
	    if (dflg)
		fprintf(stderr,"\ncheckhandresp: got %c (0x%x)\n", c, c);
#endif
	    switch(c) {
		case 'R':  /* Normal Handler response */
		    if (dflg) {
		    	fprintf(stderr,"checkhandresp:  %c",c);
		    }
		    while ((c=Receivechar()) != EOL && c != xNL) {
		    	if (dflg) {
				fprintf(stderr,"%c",c);
		    	}
		    }
		    if (dflg) {
		    	fprintf(stderr,"\n");
		    }
		    return;

		case '#':  /* Not Prompt -- failure of some sort */
		    errptr = errtmp;
		    fprintf(stderr,"Handler ERROR: %c",c);
		    i = 0;

		    while (((c=Receivechar()) != EOL)
			   && (c != xCR) && (c != xNL)) {
			*errptr++ = c;
			fprintf(stderr,"%c",c);
		    }

		    fprintf(stderr,"\n");
		    *errptr = 0; /* terminate the string */
		    sscanf(errtmp,"%d",&errcode);

		    switch(errcode) {
			case 0:			/* Error Cleared */
				fprintf(stderr,"\n\r       Error has been cleared!\n");
				Sendchar('!');  /* Reset the Handler */
				/* Need 500 ms delay after Handler Reset cmd */
				sginap(500);
				return;

			case 1:			/* Label Door Open */
				fprintf(stderr,"\n\r       The Label Door is open\n");
				break;

			case 2:			/* Label Jam */
				fprintf(stderr,"\n\r       The Label is jammed.\n\r");
				fprintf(stderr,"       Please press START on the Handler/Labeler (Green square button)\n");
				break;

			case 3:			/* Part Jam at Singulator */
				fprintf(stderr,"\n\r       There is a PART JAM at one of the SINGULATORS\n");
				fprintf(stderr,"\r       Clear the jam to continue.\n\r");
				break;

			case 4:			/* Label Door Open */
				fprintf(stderr,"\n\r       Cam motor malfunction on the Handler/Labeler\n");
				fprintf(stderr,"\n\r       Call for service on the Handler/Labeler\n");
				Sendchar('!');  /* Reset the Handler */
				/* Need 500 ms delay after Handler Reset cmd */
				sginap(500);
				break;

			case 5:			/* Part Jam in Label Area */
				fprintf(stderr,"\n\r       There is a PART JAM in the Label Area\n");
				fprintf(stderr,"\r       Clear the jam  and press START (square green button) to continue.\n\r");
				break;

			case 6:			/* Sort Motor Malfunction */
				fprintf(stderr,"\n\r       Sort Mort Malfunction--Press START\n");
				break;

			case 7:			/* Part Jam at Lower Bin */
				fprintf(stderr,"\n\r       There is a PART JAM in the Lower Bin Area\n");
				fprintf(stderr,"\r       Clear the jam to continue.\n\r");
				break;

			case 8:			/* No Receiving Tube */
				fprintf(stderr,"\n\r       No Receiving tube available\n");
				break;

			case 9:			/* Part Jam at Lower Bin */
				fprintf(stderr,"\n\r       There is a PART JAM in the Lower Bin Area\n");
				fprintf(stderr,"\r       Clear the jam and press START (square green button)  to continue.\n\r");
				break;

			case 10:		/* No Labels */
				fprintf(stderr,"\n\r       There are no Labels on the Label spool\n");
				fprintf(stderr,"\n\r       Install a spool of labels and press START (square green button) to continue.\n\r");
				break;

			case 11:		/* Checksum Error */
				fprintf(stderr,"\n\r       Checksum Error with the label file\n");
				Sendchar('!');  /* Reset the Handler */
				/* Need 500 ms delay after Handler Reset cmd */
				sginap(500);
				break;

			case 12:		/* Receiving Tube full */
				fprintf(stderr,"\n\r       Receiving Tube is full\n");
				break;

			case 13:		/* Lift Track for Label Calibration */
				fprintf(stderr,"\n\r       Lift Track for Label Calibration\n");
				break;

			case 14:		/* Next SN too large for field */
				fprintf(stderr,"\n\r       Next Serial Number is too large for the field\n");
				Sendchar('!');  /* Reset the Handler */
				/* Need 500 ms delay after Handler Reset cmd */
				sginap(500);
				break;

			case 15:		/* No data received within */
						/* time allowed */
				fprintf(stderr,"\n\r       No Data Received within Time allowed.  Press a Key to restart\n");
				Sendchar('!');  /* Reset the Handler */
				/* Need 500 ms delay after Handler Reset cmd */
				sginap(500);
				break;

			case 16:		/* Invalid Data Format */
				fprintf(stderr,"\n\r       Invalid Data Format for Label File\n");
				Sendchar('!');  /* Reset the Handler */
				/* Need 500 ms delay after Handler Reset cmd */
				sginap(500);
				break;

			case 17:		/* Did Not receive Part Width */
				fprintf(stderr,"\n\r       Did not receive Part Width data\n");
				Sendchar('!');  /* Reset the Handler */
				/* Need 500 ms delay after Handler Reset cmd */
				sginap(500);
				break;

			case 18:		/* Did not receive # of Pins */
				fprintf(stderr,"\n\r       Did not receive Number of Pins data\n");
				Sendchar('!');  /* Reset the Handler */
				/* Need 500 ms delay after Handler Reset cmd */
				sginap(500);
				break;

			case 19:		/* Track is lifted or part */
						/* under roller */
				fprintf(stderr,"\n\r       Track is lifted or part is under roller\n");
				break;

			case 20:		/* Remote Computer Not Ready */
				fprintf(stderr,"\n\r       Remote Computer Not Ready\n");
				Sendchar('!');  /* Reset the Handler */
				/* Need 500 ms delay after Handler Reset cmd */
				sginap(500);
				break;

			case 21:		/* Socket Motor Malfunction */
				fprintf(stderr,"\n\r       Socket Motor Malfunction--Press START (green square key)\n");
				Sendchar('!');  /* Reset the Handler */
				/* Need 500 ms delay after Handler Reset cmd */
				sginap(500);
				break;

			case 22:		/* Part Jam in Socket */
				fprintf(stderr,"\n\r       Part Jam in Socket\n");
				break;

			case 23:	    /* Handler Port is Disconnected */
				fprintf(stderr,"\n\r       Handler Port is disconnected\n");
				break;

			case 24:	/* Category X Bin Not Available */
				fprintf(stderr,"\n\r       Category X Bin not available\n");
				break;

			case 25:	/* Serial Communication Failure */
				fprintf(stderr,"\n\r       Serial Communication Failure\n");
				Sendchar('!');  /* Reset the Handler */
				/* Need 500 ms delay after Handler Reset cmd */
				sginap(500);
				break;

			case 26:		/* ERROR received */
				fprintf(stderr,"\n\r       Error Received, Press any key to retry\n");
				break;

			case 27:		/* Programmer Not Ready */
				fprintf(stderr,"\n\r       Programmer Not Ready\n");
				break;

			case 99:		/* Illegal Remote Command */
				fprintf(stderr,"\n\r       Illegal Remote Command\n");
				break;

		    }

		    fprintf(stderr,"\n");
		    break;
		   /*  return; */

		case xCR:  /* End of Handler prompt */
		    if (dflg)
		    {
			fprintf(stderr,"checkhandresp: 0x0D\n");
		    }
		    return;

		case xNL:  /* End of Handler prompt */
		    if (dflg)
		    {
			fprintf(stderr,"checkhandresp: 0x0A\n");
		    }
		    return;

		case '?':  /* Handler is still in transparent mode */
		    c = Receivechar();  /* Get CR */
		    Sendcmd('H');
		    checkresponse("Handler still in transparent mode");
	/*	    c = Receivechar(); */
	/*	    fprintf(stderr,"checkhandresp: Sending 0x%02x to exit transparent mode...\n\r", AL_EXTRANSP); */
		    Sendchar(AL_EXTRANSP);
		    while ((c=Receivechar()) != EOL && c != xNL)
		    {
		    	if (dflg)
		    	{
				fprintf(stderr,"%c",c);
		    	}
		    }
		    Sendhcmd(AL_FIRMREV);
		    while ((c=Receivechar()) != EOL && c != xNL)
		    {
		    	if (dflg)
		    	{
				fprintf(stderr,"%c",c);
		    	}
		    }
		    return;

		default: 
/*		    fprintf(stderr,"checkhandresp:  bad char was %c (0x%x)\n",c,c); */
		    fprintf(stderr,"Handler Communication ERROR:  bad char was %c (0x%x)\n",c,c);
		    break;
	    }
	}
	exit( FAIL );
}

/* check_pl
 *
 * Check # of parts labeled.
 * When sending the # command to the handler, it usually returns R followed by
 * 4 ascii characters, and terminated with a newline.
 * We intercept this prompt and interpret any error messages that come in.
 *
 */
int
check_pl(void)
{
	char c;
	char *errptr;
	char errtmp[10];
	int errcode = 0;
	int partslabeled = 0;
	int i;

	/* Clear errtmp */
	for (i = 0; i < 80; i++)
	{
		errtmp[i] = 0;
	}

	while (c=Receivechar()) {
#ifdef DEBUG
	    if (dflg)
		fprintf(stderr,"\ncheck_pl: just got %c (0x%x)\n", c, c);
#endif
	    switch(c) {
		case 'R':  /* Normal Handler response */
		    errptr = errtmp;
#ifdef DEBUG
		    if (dflg)
			fprintf(stderr,"Checking # of parts labeled: %c",c);
#endif
		    i = 0;

		    while (((c=Receivechar()) != EOL)
			   && (c != xCR) && (c != xNL)) {
			*errptr++ = c;
#ifdef DEBUG
			if (dflg)
			    fprintf(stderr,"%c",c);
#endif
		    }

#ifdef DEBUG
		    if (dflg)
			fprintf(stderr,"\n");
#endif
		    *errptr = 0; /* terminate the string */
		    sscanf(errtmp,"%d",&partslabeled);

		    if (dflg) {
#ifdef DEBUG
			fprintf(stderr,"partslabeled = %d\n",partslabeled);
#endif
		    	fprintf(stderr,"\n");
		    }
		    return partslabeled;

		case '#':  /* Not Prompt -- failure of some sort */
		    errptr = errtmp;
		    fprintf(stderr,"Handler ERROR: %c",c);
		    i = 0;

		    while (((c=Receivechar()) != EOL)
			   && (c != xCR) && (c != xNL)) {
			*errptr++ = c;
			fprintf(stderr,"%c",c);
		    }

		    fprintf(stderr,"\n");
		    *errptr = 0; /* terminate the string */
		    sscanf(errtmp,"%d",&errcode);

		    switch(errcode) {
			case 0:			/* Error Cleared */
				fprintf(stderr,"\n\r       Error has been cleared!\n");
				Sendchar('!');  /* Reset the Handler */
				/* Need 500 ms delay after Handler Reset cmd */
				sginap(500);
		    		return partslabeled;

			case 1:			/* Label Door Open */
				fprintf(stderr,"\n\r       The Label Door is open\n");
				break;

			case 2:			/* Label Jam */
				fprintf(stderr,"\n\r       The Label is jammed.\n\r");
				fprintf(stderr,"       Please press START on the Handler/Labeler (Green square button)\n");
				break;

			case 3:			/* Part Jam at Singulator */
				fprintf(stderr,"\n\r       There is a PART JAM at one of the SINGULATORS\n");
				fprintf(stderr,"\r       Clear the jam to continue.\n\r");
				break;

			case 4:			/* Label Door Open */
				fprintf(stderr,"\n\r       Cam motor malfunction on the Handler/Labeler\n");
				fprintf(stderr,"\n\r       Call for service on the Handler/Labeler\n");
				Sendchar('!');  /* Reset the Handler */
				/* Need 500 ms delay after Handler Reset cmd */
				sginap(500);
				break;

			case 5:			/* Part Jam in Label Area */
				fprintf(stderr,"\n\r       There is a PART JAM in the Label Area\n");
				fprintf(stderr,"\r       Clear the jam  and press START (square green button) to continue.\n\r");
				break;

			case 6:			/* Sort Motor Malfunction */
				fprintf(stderr,"\n\r       Sort Mort Malfunction--Press START\n");
				break;

			case 7:			/* Part Jam at Lower Bin */
				fprintf(stderr,"\n\r       There is a PART JAM in the Lower Bin Area\n");
				fprintf(stderr,"\r       Clear the jam to continue.\n\r");
				break;

			case 8:			/* No Receiving Tube */
				fprintf(stderr,"\n\r       No Receiving tube available\n");
				break;

			case 9:			/* Part Jam at Lower Bin */
				fprintf(stderr,"\n\r       There is a PART JAM in the Lower Bin Area\n");
				fprintf(stderr,"\r       Clear the jam and press START (square green button)  to continue.\n\r");
				break;

			case 10:		/* No Labels */
				fprintf(stderr,"\n\r       There are no Labels on the Label spool\n");
				fprintf(stderr,"\n\r       Install a spool of labels and press START (square green button) to continue.\n\r");
				break;

			case 11:		/* Checksum Error */
				fprintf(stderr,"\n\r       Checksum Error with the label file\n");
				Sendchar('!');  /* Reset the Handler */
				/* Need 500 ms delay after Handler Reset cmd */
				sginap(500);
				break;

			case 12:		/* Receiving Tube full */
				fprintf(stderr,"\n\r       Receiving Tube is full\n");
				break;

			case 13:		/* Lift Track for Label Calibration */
				fprintf(stderr,"\n\r       Lift Track for Label Calibration\n");
				break;

			case 14:		/* Next SN too large for field */
				fprintf(stderr,"\n\r       Next Serial Number is too large for the field\n");
				Sendchar('!');  /* Reset the Handler */
				/* Need 500 ms delay after Handler Reset cmd */
				sginap(500);
				break;

			case 15:		/* No data received within */
						/* time allowed */
				fprintf(stderr,"\n\r       No Data Received within Time allowed.  Press a Key to restart\n");
				Sendchar('!');  /* Reset the Handler */
				/* Need 500 ms delay after Handler Reset cmd */
				sginap(500);
				break;

			case 16:		/* Invalid Data Format */
				fprintf(stderr,"\n\r       Invalid Data Format for Label File\n");
				Sendchar('!');  /* Reset the Handler */
				/* Need 500 ms delay after Handler Reset cmd */
				sginap(500);
				break;

			case 17:		/* Did Not receive Part Width */
				fprintf(stderr,"\n\r       Did not receive Part Width data\n");
				Sendchar('!');  /* Reset the Handler */
				/* Need 500 ms delay after Handler Reset cmd */
				sginap(500);
				break;

			case 18:		/* Did not receive # of Pins */
				fprintf(stderr,"\n\r       Did not receive Number of Pins data\n");
				Sendchar('!');  /* Reset the Handler */
				/* Need 500 ms delay after Handler Reset cmd */
				sginap(500);
				break;

			case 19:		/* Track is lifted or part */
						/* under roller */
				fprintf(stderr,"\n\r       Track is lifted or part is under roller\n");
				break;

			case 20:		/* Remote Computer Not Ready */
				fprintf(stderr,"\n\r       Remote Computer Not Ready\n");
				Sendchar('!');  /* Reset the Handler */
				/* Need 500 ms delay after Handler Reset cmd */
				sginap(500);
				break;

			case 21:		/* Socket Motor Malfunction */
				fprintf(stderr,"\n\r       Socket Motor Malfunction--Press START (green square key)\n");
				Sendchar('!');  /* Reset the Handler */
				/* Need 500 ms delay after Handler Reset cmd */
				sginap(500);
				break;

			case 22:		/* Part Jam in Socket */
				fprintf(stderr,"\n\r       Part Jam in Socket\n");
				break;

			case 23:	    /* Handler Port is Disconnected */
				fprintf(stderr,"\n\r       Handler Port is disconnected\n");
				break;

			case 24:	/* Category X Bin Not Available */
				fprintf(stderr,"\n\r       Category X Bin not available\n");
				break;

			case 25:	/* Serial Communication Failure */
				fprintf(stderr,"\n\r       Serial Communication Failure\n");
				Sendchar('!');  /* Reset the Handler */
				/* Need 500 ms delay after Handler Reset cmd */
				sginap(500);
				break;

			case 26:		/* ERROR received */
				fprintf(stderr,"\n\r       Error Received, Press any key to retry\n");
				break;

			case 27:		/* Programmer Not Ready */
				fprintf(stderr,"\n\r       Programmer Not Ready\n");
				break;

			case 99:		/* Illegal Remote Command */
				fprintf(stderr,"\n\r       Illegal Remote Command\n");
				break;

		    }

		    fprintf(stderr,"\n");
		    break;
		   /*  return; */

		case xCR:  /* End of Handler prompt */
		    if (dflg)
		    {
			fprintf(stderr,"checkhandresp: 0x0D\n");
		    }
		    return partslabeled;

		case xNL:  /* End of Handler prompt */
		    if (dflg)
		    {
			fprintf(stderr,"checkhandresp: 0x0A\n");
		    }
		    return partslabeled;

		case '?':  /* Handler is still in transparent mode */
		    c = Receivechar();  /* Get CR */
		    Sendcmd('H');
		    checkresponse("Handler still in transparent mode");
/*		    c = Receivechar(); */
	/*	    fprintf(stderr,"checkhandresp: Sending 0x%02x to exit transparent mode...\n\r", AL_EXTRANSP); */
		    Sendchar(AL_EXTRANSP);
		    while ((c=Receivechar()) != EOL && c != xNL)
		    {
		    	if (dflg)
		    	{
				fprintf(stderr,"%c",c);
		    	}
		    }
		    Sendhcmd(AL_FIRMREV);
		    while ((c=Receivechar()) != EOL && c != xNL)
		    {
		    	if (dflg)
		    	{
				fprintf(stderr,"%c",c);
		    	}
		    }
		    return partslabeled;

		default: 
/*		    fprintf(stderr,"checkhandresp:  bad char was %c (0x%x)\n",c,c); */
		    fprintf(stderr,"Handler Communication ERROR:  bad char was %c (0x%x)\n",c,c);
		    break;
	    }
	}
	exit( FAIL );
}

char
Receivechar(void)
{
	char *c;
	char buf[80];
	int error = 0;
	c = buf;
	*c = GetChar(fpd);
	if(dflg) {
		fprintf(stderr,"Receivechar: I just got 0x%x = %c\n", *c, *c);
	}
	return *c;
}

char
GetChar(int fd)
/* int fd = file descripter of open prom device */
{
#ifdef NOTDEF
	int rmask, wmask, emask, nfound;
	char c;

	nfound = rmask = wmask = emask = 0;
	rmask = wmask |= (1<<fd);

	if (dflg) {
		fprintf(stderr,"GetChar:  fd = 0x%x, rmask = emask = 0x%x\n",
		    fd, rmask, emask);
		fprintf(stderr,"GetChar:  about to select....\n");
	}
/*	PrepareToReceive; */
	/* This will block until a char is ready on fd */
	if ((nfound = select(32,&rmask,0,&emask,0)) == -1)
	{
		fprintf(stderr, "Select error in GetChar\n");
	}
	if (dflg) {
		fprintf(stderr,"GetChar:  nfound = 0x%x\n",nfound);
	}
	if (nfound > 0)
	{
		read(fd, &c, 1); 
/*		fprintf(stderr,"GetChar: just read 0x%x (%c)\n", c,c); */
		return c;
	}
#else
	fd_set readfds;
	char c;
	int nfound;

	FD_ZERO(&readfds); /* Be sure to clear all bits in readfds */
	FD_SET(fd, &readfds); /* Then set the file descriptor of interest */

	if (dflg) {
	    fprintf(stderr,"GetChar:  fd = 0x%x, readfds = 0x%x\n",fd, readfds);
	}

	/* This will block until a char is ready on fd */
	if ((nfound = select(FD_SETSIZE,&readfds,0,0,0)) == -1)
	{
		fprintf(stderr, "Select error in GetChar\n");
	}
	if (dflg) {
		fprintf(stderr,"GetChar:  nfound = 0x%x\n",nfound);
	}
	if (nfound > 0)
	{
		read(fd, &c, 1); 
/*		fprintf(stderr,"GetChar: just read 0x%x (%c)\n", c,c); */
		return c;
	}
#endif
}


/* Receivehex
 *
 * read alpha and convert to hex, swallowing first character that is not
 * part of a hex number
 * any leading EOLs or blanks are ignored.
 *
 */
unsigned int
Receivehex(void)
{
	unsigned char c;
	unsigned int x;

	x = 0;
	while( ((c = Receivechar()) == EOL) || (c == ' ') )
		;
	for( ; ; ) {
		if( c>='0' && c<='9' ) {
			x = (x<<4) + c - '0';
			c = Receivechar();
		}
		else if( c>='A' && c<='F' ) {
			x = (x<<4) + c - 'A' + 10;
			c = Receivechar();
		}
		else if( c>='a' && c<='f' ) {
			x = (x<<4) + c - 'a' + 10;
			c = Receivechar();
		}
		else
			break;
	}
	return( x );
}

void
Sendchar(char c)
{
	int i, error;
	char *cp;
	cp = &c;
	PrepareToReceive;
	if (dflg)
		dputchar(c);
	if ((error = write(fpd,cp,1)) != 1)
		fprintf(stderr,"Sendchar: write returned 0x%x!\n",error);
	if (dldelay)
	    sginap(20);
} 

void
Sendcmd(char c)
{
	int i, error;
	char *cp;
	cp = &c;
	if (dflg)
		fprintf(stderr,"\n++%c\n",c);
	if ((error = write(fpd,cp,1)) != 1)
		fprintf(stderr,"Sendcmd: write returned 0x%x!\n",error);
	Sendchar(xCR); 
} 

void
Sendhcmd(int x)
{
	if (dflg)
		fprintf(stderr,"\n++@%02x\n",x);
	Sendchar('@');
	Send2hex(x);
} 


/* dputchar
 *	debug routine corresponding to Sendchar
 */
void
dputchar(unsigned char c)
{
	if( c>=BLANK && c<=127 )
		putchar(c);
	else
		printf("{0x%x}\n",c);	/* should happen only for STX, ETX */
	fflush(stdout);
}



/* make sure that what you send to the prom device has all upper case hex
 * characters in it 
 */

char hexchars[] = "0123456789ABCDEF";

void
Sendhex(int x)
{
	int k,l,m;

	m =  x & 0xf;
	l =  (x>>4) & 0xf;
	k =  (x>>8) & 0xf;
	x =  x>>12;

	Sendchar(hexchars[x]);
	Sendchar(hexchars[k]);
	Sendchar(hexchars[l]);
	Sendchar(hexchars[m]);
}

void
Send6hex(int x)
{
	int k,l,m,n,o;

	m =  x & 0xf;
	l =  (x>>4) & 0xf;
	k =  (x>>8) & 0xf;
	n =  (x>>12) & 0xf;
	o =  (x>>16) & 0xf;
	x =  (x>>20) & 0xf;

	Sendchar(hexchars[x]);
	Sendchar(hexchars[o]);
	Sendchar(hexchars[n]);
	Sendchar(hexchars[k]);
	Sendchar(hexchars[l]);
	Sendchar(hexchars[m]);
}

void
Send2hex(int x)
{
	int m;

	m = x & 0xf;
	x = (x>>4) & 0xf;

	Sendchar(hexchars[x]);
	Sendchar(hexchars[m]);
}

void
Send3dec(int d)
{
	char decstrng[4];

	sprintf(decstrng,"%03d",d);
/*	fprintf(stderr,"dec value to send = %s\n",decstrng); */

	Sendchar(decstrng[0]);
	Sendchar(decstrng[1]);
	Sendchar(decstrng[2]);
}


/* dprinthex
 *		debug routine corresponding to Sendhex
 */
void
dprinthex(unsigned int x)
{
	putchar( hexchars[x>>12] );
	putchar( hexchars[(x>>8)&0xf] );
	putchar( hexchars[(x>>4)&0xf] );
	putchar( hexchars[x&0xf] );
	fflush(stdout);
}

void
fail(char *m)
{
	Eprintf("nprom: %s\n",m);
	exit( FAIL );
}

void
readdevicecodes(void)
{
	FILE	*devcodes;
	int	i, j;
	char	c;
	int	x;

	devcodes = fopen("devicecodes","r");
	if( devcodes==NULL ) {
	    devcodes = fopen("/usr/local/bin/dataio/devicecodes","r");
	    if( devcodes==NULL )
		fail("can't open devicecodes file, Goodbye");
	}
	i = 0;
	j = 0;
	for( ;; ) {
		switch( c=fgetc(devcodes) ) {
		    case EOF:
			tab[i].type[j] = EOS;
			tab[i].code = 0;
			return;

		    case '#':			/* comment line */
			while( (c=fgetc(devcodes)) !=EOL )
				;
			break;

		    case EOL:
		    case BLANK:
		    case TAB:
			tab[i].type[j] = EOS;
			while( (c=fgetc(devcodes)) == BLANK || c == TAB )
				;
			x = 0;
			for( ;; ) {
				if( c>='0' && c<='9' )
					x = (x<<4) + c - '0';
				else if( c>='A' && c<='F' )
					x = (x<<4) + c - 'A' + 10;
				else if( c>='a' && c<='f' )
					x = (x<<4) + c - 'a' + 10;
				else
					break;

				c = fgetc(devcodes);
			}
			tab[i].code = x;
			i++;
			j = 0;
			break;

		    default:
			tab[i].type[j++] = c;
			break;
		}
	}
}

int
devlookup(char *dname)
{
	struct trans *pt;

	readdevicecodes();
	for( pt=tab ; pt->type[0] != EOS ; pt++ ) {
		if( streq(pt->type,dname) )
			return( pt->code );
	}
	if( pt->type[0] == EOS ) {
		Eprintf("unknown type %s\n",dname);
		Warn("known types are:");
		for( pt=tab ; pt->type[0] != EOS ; ) {
			Eprintf("%s, ",pt->type);
			if((++pt - tab)%7 == 0)
				Warn("");
		}
		fail("file 'devicecodes' does not include specified device");
	}
}

