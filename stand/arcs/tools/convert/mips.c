/* $Header: /proj/irix6.5.7m/isms/stand/arcs/tools/convert/RCS/mips.c,v 1.4 1994/11/09 22:03:33 sfc Exp $ */
/* $Log: mips.c,v $
 * Revision 1.4  1994/11/09 22:03:33  sfc
 * Merge sherwood/redwood up and including redwood version 1.6
 * > ----------------------------
 * > revision 1.6
 * > date: 93/10/12 22:48:03;  author: jeffs;  state: ;  lines: +6 -3
 * > remove #endif XXX crud.
 * > ----------------------------
 * > revision 1.5
 * > date: 93/10/12 00:36:14;  author: jeffs;  state: ;  lines: +10 -7
 * > Minor clean-ups for -fullwarn.
 * > ----------------------------
 * > revision 1.4
 * > date: 93/07/15 19:48:05;  author: igehy;  state: ;  lines: +57 -33
 * > Uses Unix file manipulators instead of stdio.
 * > ----------------------------
 *
 * Revision 1.6  1993/10/12  22:48:03  jeffs
 * remove #endif XXX crud.
 *
 * Revision 1.5  1993/10/12  00:36:14  jeffs
 * Minor clean-ups for -fullwarn.
 *
 * Revision 1.4  1993/07/15  19:48:05  igehy
 * Uses Unix file manipulators instead of stdio.
 *
 * Revision 1.3  1992/09/30  17:55:53  jfk
 * Added a minor hack for binary record format.
 *
 * Revision 1.2  92/08/14  18:33:12  stever
 * Added lots more sections to be converted.  Stuff like rdata, lit4, lit8...
 * 
 * Revision 1.1  87/09/21  11:14:10  huy
 * Initial revision
 *  */

/******************************************************************************* *
 *	This module opens, reads, and closes MIPS coff object Files	       *
 *
 *******************************************************************************
 */
#define S3_DATA_RECORD 0x03
#define JFKBINARY      0x07

#define mips 1
#define LANGUAGE_C 1
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "filehdr.h"
#include "aouthdr.h"
#include "scnhdr.h"
#include "sex.h"
#include "convert.h"
/* #define DEBUG	*/

/*
 *  foward declarations
 */
static int	GetNextSection(int);

/*
 *  Mips Object File data structures
 */
static struct filehdr file_header;
static struct aouthdr optional_header;
static struct scnhdr section;

/*
 *  Module specific data
 */

static long section_headers;		/* file pointer to start of sections */
static long next_header;

struct section_type {
	long flags;
	char name[ 8 ];
};

static struct section_type section_types[] = {
	(long)STYP_TEXT, {_TEXT},
	(long)STYP_INIT, {_INIT},
	(long)STYP_RDATA, {_RDATA},
	(long)STYP_DATA, {_DATA},
	(long)STYP_LIT8, {_LIT8},
	(long)STYP_LIT4, {_LIT4},
	(long)STYP_SDATA, {_SDATA}
};

#define N_SECTION_TYPES (sizeof section_types /  \
			     sizeof(struct section_type ))


static int sti;			/* Section Table index */

static unsigned long section_size;

extern int my_sex;

int
MipsInitialize(int fd)
{
	int error;

	/*
	 *  Read in the headers
	 */

	if ((error = read(fd, (char *)&file_header, sizeof(struct filehdr))) 
						!= sizeof(struct filehdr)) {
		fprintf(stderr, "Could not read file.\n");
		return(1);
	}

	/* Fixup if of opposite sex */
	if ( file_header.f_magic == SMIPSEBMAGIC || 
	     file_header.f_magic == SMIPSELMAGIC )  {
		swap = TRUE;
		swap_filehdr(&file_header,my_sex);
	}

	/* Read optional header if consistent */
	if (file_header.f_opthdr != sizeof(struct aouthdr) ) {
		fprintf(stderr,
		 	"Error: File and optional headers inconsistent\n" );
		return -3;
	}

	if (read(fd, (char *)&optional_header, sizeof(struct aouthdr))
					!= sizeof(struct aouthdr))
		return -2;

	/* Fixup if of opposite sex */
	if ( swap )  swap_aouthdr(&optional_header,my_sex);

	start_address = (unsigned)optional_header.entry;
#ifdef DEBUG
	printf("text_start= %x data_start= %x\n",optional_header.text_start,optional_header.data_start);
#endif

	/*
	 * Open the first text section
	 */
	next_header = section_headers = lseek(fd, 0, SEEK_CUR);
	sti = 0;
	return (GetNextSection(fd));
}


/*
 *  MipsRead - Read data from a Mips/Coff Object file.
 *
 *  Inputs:  buffer - address of buffer for data
 *	     length - number of bytes to read
 *  Outputs: buffer - gets the data
 *  returns: >0 - Ok number of bytes read
 *	     =0 - End of file
 *	     <0 - Error code
 */
int
MipsRead(char *buffer, int length, int fd)
{
	int error;

	/* Check for end of section */
	if (section_size <= 0) {

		/* Get the next section */
		error = GetNextSection(fd);

		if (error < 0 ) return error;	/* Error? */
		if (error > 0 ) return 0;		/* Eof?   */
	}

	/* Is there enough data in the buffer to satisfy the request? */
	if ( length > section_size ) 
		length = section_size;
	section_size -= length;

	/* Read the data */
	if ( length > 0 &&
	     ((error = read(fd, buffer, length)) != length)) {
		/* Physical end of file or read error should cause death */
		fprintf(stderr, "Could not read file.\n");
		return(1);
	}
	

	/* Normal exit - return # of characters read */
	return length;
}
/*
 *  Mips Close - No special cleanup needed
 */
int
MipsClose(int fd)
{
	return OK;
}

/*
 *GetNextSection - Set up the next section of interest for reading
 *
 * Inputs:  None
 * Outputs: None
 * Returns: <0 if error occured
 *	    =0 if next section opened
 *	    >0 if no more sections of interest to open
 */
static int
GetNextSection(int fd)
{

	register int i;
	register int error;
	extern address record_address;
	extern unsigned int data_record;

	while ( sti < N_SECTION_TYPES ) {

		/* Position to next header to be read */
		if (lseek(fd, next_header, 0) == 0) {
			fprintf(stderr, "Could not seek file.\n");
			return(1);
		}

		for ( i = file_header.f_nscns; i > 0; i-- ) {

			if ((error = read(fd, (char *)&section,
			   sizeof(struct scnhdr))) != sizeof(struct scnhdr)) {
				fprintf(stderr, "Could not read file.\n");
				return(1);
			}
			
			/* Fixup if of opposite sex */
			if ( swap ) swap_scnhdr(&section,my_sex);

			/* Is this a header of a section to be loaded? */
			if ( section.s_flags & section_types[ sti ].flags &&
			    !strcmp( section.s_name, section_types[sti].name)) {

				/* Got one */
				next_header = lseek(fd, 0, SEEK_CUR);
				if (section.s_vaddr != section.s_paddr)
					return -4;
				load_address = section.s_paddr;
				section_size = section.s_size;
				if (data_record == S3_DATA_RECORD ||
				    data_record == JFKBINARY)
					record_address = load_address;
#ifdef DEBUG
printf("data_record = %x\n",data_record);
printf("\nsection: %s load_address = 0x%x section_size = 0x%x\n\n",section.s_name,load_address,section_size);
#endif
				if (lseek(fd, section.s_scnptr, 0))
					return OK;
				else {
					fprintf(stderr, "Could not seek file.\n");
					return(1);
				}
			}
		}

		sti += 1;
		next_header = section_headers;
	}

	/* Here if no section found */
	return 1;
}
	
