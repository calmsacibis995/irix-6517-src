/* $Header: /proj/irix6.5.7m/isms/stand/arcs/tools/convert/RCS/convert.c,v 1.9 1996/08/13 18:36:59 poist Exp $ */
/* $Log: convert.c,v $
 * Revision 1.9  1996/08/13 18:36:59  poist
 * addition of IP30 formats, version number support on command line.
 *
 * Revision 1.8  1995/11/23  21:13:39  pap
 * added support to read in pure binary files (-p option)
 *
 * Revision 1.7  1995/09/16  00:47:12  csm
 * Add pure to list of supported formats
 *
 * Revision 1.6  1994/11/09  22:03:01  sfc
 * Merge sherwood/redwood up and including redwood version 1.8
 * > ----------------------------
 * > revision 1.8
 * > date: 93/10/12 00:33:44;  author: jeffs;  state: ;  lines: +11 -7
 * > Minor clean-ups for -fullwarn.
 * > ----------------------------
 * > revision 1.7
 * > date: 93/07/21 22:22:17;  author: igehy;  state: ;  lines: +3 -3
 * > Lines on open were written incorrectly.
 * > ----------------------------
 * > revision 1.6
 * > date: 93/07/15 19:45:52;  author: igehy;  state: ;  lines: +60 -106
 * > Added ELF capabilities.
 * > ----------------------------
 * > revision 1.5
 * > date: 93/07/09 23:35:45;  author: travis;  state: ;  lines: +28 -6
 * > Added -Z option (similiar to EVEREST -z but different ;-) which was how
 * > it was before.  Remember to change those command files!
 * > ----------------------------
 *
 * Revision 1.8  1993/10/12  00:33:44  jeffs
 * Minor clean-ups for -fullwarn.
 *
 * Revision 1.7  1993/07/21  22:22:17  igehy
 * Lines on open were written incorrectly.
 *
 * Revision 1.5  1993/07/09  23:35:45  travis
 * Added -Z option (similiar to EVEREST -z but different ;-) which was how
 * it was before.  Remember to change those command files!
 *
 * Revision 1.4  1992/09/30  17:55:36  jfk
 * Added support for the binary record format.
 *
 * Revision 1.1  87/09/21  11:13:48  huy
 * Initial revision
 *  */

/*******************************************************************************
 *
 *	convert - Convert object files to downloadable format.
 *
 *  convert -z -Z -p -f <format> -b <byte number> file ...
 *
 *	where <format> is either "intelhex", "step" or "srec"
 * 	-z swizzles all words in the file before formatting)
 * 	-Z swizzles bytes within shorts in the file before formatting)
 *  -p read file in as pure binary, don't try to read the format
 *
 * This program converts BSD4.2 a.out or mips object files to downloadable
 * format suitable for the prom programmers.  The program is designed to
 * be easily expanadble for other output formats.
 *
 ******************************************************************************/

#define LANGUAGE_C 1
#define mips 1

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <malloc.h>
#include <stdlib.h>
#include <unistd.h>
#include "a.out.h"
#include "convert.h"
#include "stubs.h"
#include "elf.h"
#include "mips.h"
#include "intel_hex.h"
#include "s_rec.h"
#include "sex.h"
#include "binary.h"
#include "pure.h"
#include "racer.h"

extern int errno;
extern int sys_nerr;
extern char *sys_errlist[];

char bad_flag[] = "%s: %c is not a valid flag.\n";
char *progname;
 
/*
 * Foward declarations
 */
static int	Convert(int, int);
static int	GetObjectType(enum object_file_type *, int);
static int	Match(char *, char *, int);
static int	MatchArg(char *, char *[], int);

/*
 * Object file type control structures
 */

struct object_file_type_table_descriptor object_file_type_table[] = {

	MIPSEBMAGIC,	MIPS,		/* headers: BE - target: BE */
	MIPSELMAGIC,	MIPS,		/* headers: LE - target: LE */
	MIPSEBMAGIC_2,	MIPS,		/* MIPS2 BE -> BE */
	MIPSELMAGIC_2,	MIPS,		/* MIPS2 LE -> LE */
	MIPSEBMAGIC_3,  MIPS,		/* MIPS3 BE -> BE */
	MIPSELMAGIC_3, 	MIPS,		/* MIPS3 LE -> LE */	
	SMIPSEBMAGIC,	MIPS,		/* headers: LE - target: BE */
	SMIPSELMAGIC,	MIPS,		/* headers: BE - target: LE */

	OMAGIC, BERKELEY,
	NMAGIC, BERKELEY,
	ZMAGIC, BERKELEY,

	0x0550, COFF,
	0x0551, COFF,
	0x0570, COFF,
	0x0575, COFF,

	0x7f45, ELF
};

#define N_MAGICS (sizeof( object_file_type_table) /  \
		  sizeof(struct object_file_type_table_descriptor) )

struct object_file_descriptor object_files[] = {
	BERKELEY, StubObjInitialize, StubObjRead, StubObjClose,
	COFF, StubObjInitialize, StubObjRead, StubObjClose,
	ELF, ElfInitialize, ElfRead, ElfClose,
	PURE, PureInitialize, PureRead, PureClose,
	MIPS, MipsInitialize, MipsRead, MipsClose
};

#define N_OBJECT_TYPES (sizeof(object_files)/sizeof(struct object_file_descriptor))

struct object_file_descriptor *object;

/*
 * Output format control structures
 */

char *format_names[] = { 
	"intelhex", "step", "srec", "s1rec","s2rec", "s3rec", "stub",
	"binary", "pure", "rprom", "fprom", 0 
};

#define N_FORMATS ((sizeof( format_names ) / sizeof( char *)) )

struct format_descriptor formats[ N_FORMATS ] = {

	16,
	IntelhexInitialize,
	IntelhexConvert,
	IntelhexWrite,
	IntelhexTerminate,
	StubFmtSetVersion,

	4,
	IntelhexInitialize,
	StepConvert,
	IntelhexWrite,
	IntelhexTerminate,
	StubFmtSetVersion,

	16,
	S2RecordInitialize,
	SRecordConvert,
	SRecordWrite,
	SRecordTerminate,
	StubFmtSetVersion,

	16,
	S1RecordInitialize,
	SRecordConvert,
	SRecordWrite,
	SRecordTerminate,
	StubFmtSetVersion,

	16,
	S2RecordInitialize,
	SRecordConvert,
	SRecordWrite,
	SRecordTerminate,
	StubFmtSetVersion,

	32,
	S3RecordInitialize,
	SRecordConvert,
	SRecordWrite,
	SRecordTerminate,
	StubFmtSetVersion,

	16,
	StubFmtInitialize,
	StubFmtConvert,
	StubFmtWrite,
	StubFmtTerminate,
	StubFmtSetVersion,

	4096,
	BinInitialize,
	BinConvert,
	BinWrite,
	BinTerminate,
	StubFmtSetVersion,

	4096,
	PureInitialize,
	PureConvert,
	PureWrite,
	PureTerminate,
	StubFmtSetVersion,

	4096,
	RacerRpromInitialize,
	RacerConvert,
	RacerRpromWrite,
	RacerRpromTerminate,
	RacerSetVersion,

	4096,
	RacerFpromInitialize,
	RacerConvert,
	RacerFpromWrite,
	RacerFpromTerminate,
	RacerSetVersion,
};

struct format_descriptor *format = formats;

int byte_number = -1;

int short_number= -1;		/* added to allow split short HN 7/87 */
/*
 * Object file section globals
 */
unsigned long load_address;	/* Code loaded here */
unsigned long start_address;	/* Control passed here on execution */
long version = 0;		/* provided version number */

/*
 *  Sex variables
 */
int my_sex;			/* Host byte sex */
int swap;			/* True if headers and object of opposite sex */
int swizzle = 0;		/* Set to swizzle bytes with words */
int Swizzle = 0;		/* Set to swizzle bytes with shorts */

int buffer[ 1024 ];


main(int argc, char **argv)
{

	register int i = 1;
	register int j;
	char *cp;
	int status;
	int fd;			/* file descriptor */
	int pure = 0;

	/* Initialize */
	my_sex = gethostsex();
	progname = argv[ 0 ];

	/* Parse the flags - Note that all flags must precede all filenames */
	while ( i < argc ) {

		/* No flags - start processing filenames */
		if ( argv[i][0] != '-' ) {
			break;
		}

		/* process each flag */
		switch ( argv[i][1] ) {
		case 's':
			/* Allow the short number to be separated from the
			 *   flag by zero or more blanks
			 * 0 means Most significant 16 bits
			 * 2 means Least significant 16 bits
			 */
			cp = (argv[i][2] ? &argv[i][2]
				         : argv[ (++i < argc ? i :--i) ]);
			short_number = atoi( cp );
			break;

		case 'p':
			/* read the input as pure binary, forget about file format */
			pure = 1;
			break;

		case 'b':
			/* Allow the byte number to be separated from the
			 *   flag by zero or more blanks
			 */
			cp = (argv[i][2] ? &argv[i][2]
				         : argv[ (++i < argc ? i :--i) ]);
			byte_number = atoi( cp );
			break;

		case 'f':
			/* Allow the format specifier to separated from the
			 *   flag by zero or more blanks
			 */
			cp = (argv[i][2] ? &argv[i][2]
				         : argv[ (++i < argc ? i :--i) ]);

			/* Check for legal format specifier */
			if ((j = MatchArg(cp, format_names, IGNORECASE)) >= 0) {
				format = &formats[ j ];
				break;
			}
			else {
				fprintf( stderr,
					 "%s: Invalid format specification.\n",
					 progname );
				fprintf( stderr, "%s: Try ", progname );
				for ( j = N_FORMATS - 2; j >= 0; j-- ) {

					switch ( j ) {
					case 0:
						cp = "%s(default).\n";
						break;
					case 1:
						cp = "%s or ";
						break;
					default:
						cp = "%s, ";
					}

					fprintf(stderr, cp, format_names[ j ]);
				}
				exit( 1 );
			}
			break;
			
		case 'z':
			/* Swizzle data (and instructions) before formatting.
			 * The IP19 prom likes all bytes in little endian
			 * order.  Don't do any doubleword loads, though.
			 */
			swizzle = 1;
			break;
		case 'Z':
			/* Swizzle data (and instructions) before formatting.
			 * The IP19 prom likes all bytes in little endian
			 * order.  Don't do any doubleword loads, though.
			 */
			Swizzle = 1;
			break;
		case 'v':
			/* Set the version number of the output file.
			 * the command line argument is X.Y, where
			 * X takes the high 24 bits, and Y takes the
			 * low 8 bits
			 */
			{
				char * ptr;

				cp = (argv[i][2] ? &argv[i][2]
				         : argv[ (++i < argc ? i :--i) ]);

				version = strtol( cp, &ptr, 10 );
				version <<= 8;
				if ((*ptr != NULL) && (*++ptr != NULL)) {
					long t;
					t = strtol( ptr, NULL, 10 );
					version |= t & 0xff;
				}
			}
			break;
		default:
			fprintf( stderr, bad_flag, progname, argv[ i ][ 1 ] );
			exit(1);
		}

		/* Pick up next command line arg */
		i++;
	}

	/* Process each filename */
	if ( i >= argc ) {

		/* no file arguments - process contents of stdin */
		status = Convert(0, pure);
	}
	else {
		for ( ; i < argc; i++ ) {

			/* Open the next file */
			if ((fd = open( argv[i], O_RDONLY)) == -1) {
				fprintf(stderr, "Could not open %s.\n", argv[i]);
				break;
			}
			/* Do it */
			status = Convert(fd, pure);
			close(fd);
		}
	}

	/* Clean code here */
	exit( status );

} /* end of main */

static int
Convert(int fd, int pure)
{

	register int error;
	enum object_file_type type;
	int input_blocking_factor;
	int output_blocking_factor;
	int length;
	int convlen;
	int i;
	unsigned int temp;

	/* Obtain the type file type */
	if (pure)
		type = PURE;
	else if (error = GetObjectType(&type, fd)) 
		return error;
	object = &object_files[ (int)type ];

	/* Obtain blocking factor from output format */
	output_blocking_factor = format->blocking_factor;
	input_blocking_factor = byte_number < 0 ? output_blocking_factor 
						: 4 * output_blocking_factor;
	input_blocking_factor = short_number < 0 ? input_blocking_factor 
						: 2 * output_blocking_factor;

	(*format->setVersion)(version);

	/* Initialize the output conversion routines */
	if (error = (*format->initialize)(output_blocking_factor))
		return error;

	/* Obtain the lengths and addresses of the text and data sections */
	if (error = (*object->initialize)(fd)) 
		return error;

	/* Main processing loop */
	while ((length=(*object->read)((char *)buffer, input_blocking_factor,
				       fd)) > 0) {
		if (swizzle) {
			for (i = 0; i < input_blocking_factor; i+=4) {
				/* Swizzle for EVEREST */
				temp =  ((char *)buffer)[i + 0];
				temp |= ((char *)buffer)[i + 1] << 8;
				temp |= ((char *)buffer)[i + 2] << 16;
				temp |= ((char *)buffer)[i + 3] << 24;
				buffer[i/4] = temp; 
			}
		}
		if (Swizzle) {
			for (i = 0; i < input_blocking_factor; i+=4) {
				/* Swizzle for IP22/IP24 */
				temp =	((char *)buffer)[i + 0] << 16;
				temp |= ((char *)buffer)[i + 1] << 24;
				temp |= ((char *)buffer)[i + 2] <<  0;
				temp |= ((char *)buffer)[i + 3] <<  8;
				buffer[i/4] = temp; 
			}
		}
		/* Convert the the last record read to the output format */
		convlen = (byte_number < 0) ? length : length/4;
		convlen = (short_number < 0) ? convlen: length/2;
		if ((error = (*format->convert)((char *)buffer, convlen)))
			return error;

		/* Write the record out to the out file */
		if (error = (*format->write)())
			return error;
	}
	if (error = (*format->terminate)())
		return error;

	return ((length < 0) ? length : (*object->close)(fd));

} /* end of Convert */

static int
GetObjectType(enum object_file_type *type, int fd)
{
	register int i;
	union {
		short number;
		char c;
	} magic;
	int error;

	/* Get the magic number from the object file */
	if ( (error = read(fd, (char *)&magic, sizeof(magic))) == 0) {
		fprintf(stderr,"Could not read file.\n");
		return(1);
	}
	if ( (error = lseek(fd, 0, SEEK_SET)) != 0) {
		fprintf(stderr,"Could not seek file.\n");
		return(1);
	}

	/* find the type of the the object file */
	for ( i = 0; i < N_MAGICS; i++ ) {

		if ( object_file_type_table[ i ].magic == magic.number) {
			*type = object_file_type_table[ i ].type;
			return OK;
		}
	}

	fprintf(stderr,
		"%s: Unrecognized magic number: 0x%x\n",
		progname,
		magic.number);

	return NOT_OK;

} /* GetObjectType */	

static int
MatchArg(register char *arg, register char *table[], int flags)
{

	register int i;

	/* Loop thru each entry in the table */
	for ( i = 0; table[ i ]; i++) {

		if ( Match( arg, table[ i ], flags ) ) return i;
	}

	/* Here on mismatch */
	return -1;

} /* end MatchArg */

static int
Match(register char *a, register char *b, int flags)
{

	register char c, d;

	/* Loop until end of shorter string */
	for ( ; *a && *b; a++, b++ ) {

		/* Use temps to avoid trashing passed parms */
		c = *a;
		d = *b;

		/* Perform case insensitivity if needed */
		if ( flags & IGNORECASE ) {
			if ( isupper( c ) )
				c = tolower( c );
			if ( isupper( d ) )
				d = tolower( d );
		}

		/* Compare strings */
		if ( c != d ) return FALSE;
	}

	/* Here on match - check for extact match */
	return (flags & EXACTMATCH? *a == *b : TRUE);

} /* end Match */
