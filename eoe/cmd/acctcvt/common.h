/*
 * acctcvt/common.h
 *	Common declarations and structs for acctcvt
 *
 * Copyright 1997, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Revision: 1.4 $"

#ifndef _ACCTCVT_COMMON_H_
#define _ACCTCVT_COMMON_H_

#include <limits.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/extacct.h>
#include <sys/sat.h>
#include <sys/sat_compat.h>
#include <sys/param.h>


/*
 * Data formats
 */
typedef enum fmt {
	fmt_none,		/* Format unknown/unspecified */
	fmt_ea62,		/* IRIX 6.2 Extended Accounting (SAT 3.1) */
	fmt_ea64,		/* IRIX 6.4 Extended Accounting (SAT 3.1++) */
	fmt_ea65,		/* IRIX 6.5 Extended Accounting (SAT 4.0) */
	fmt_svr4,		/* SVR4 format */
	fmt_text,		/* Human readable text - original format */
	fmt_acctcom,		/* Human readable text - acctcom style */
	fmt_awk,		/* Awkable text */
} fmt_t;


/*
 * Input descriptor
 */
typedef enum in_src {		/* Input source */
	in_none,			/* unknown */
	in_file,			/* read from file */
	in_stdin,			/* read from stdin */
} in_src;

typedef struct input {		/* Input descriptor */
	fmt_t	format;			/* Input format */
	in_src	source;			/* Input source */
	char	*name;			/* Name of input file */

	/* private fields */
	FILE	*file;			/* File descriptor for input */
	char	*unread;		/* Unread buffer */
	int	unreadlen;		/* # of bytes of unread data */
	int	unreadbufsize;		/* Total length of unread buffer */
} input_t;

/* input_t functions */
void	CloseInput(input_t *);
int	InputEOF(input_t *);
int	InputError(input_t *);
int	OpenInput(input_t *);
int	Read(input_t *, void *, int);
int	Unread(input_t *, void *, int);


/*
 * Output descriptor
 */
typedef enum out_dest {		/* Output destination */
	out_none,			/* unknown */
	out_file,			/* write to a file */
	out_stdout,			/* write to stdout */
	out_cmd,			/* write to another process */
} out_dest;

typedef struct output {		/* Output descriptor */
	fmt_t	 format;		/* Output format */
	out_dest dest;			/* Output destination */
	char	 *name;			/* Name of output file or command */

	/* private fields */
	FILE	 *file;			/* File descriptor for output */
} output_t;

/* output_t functions */
void	CloseOutput(output_t *);
int	OpenOutput(output_t *);
int	Print(output_t *, const char *, ...);
int	Write(output_t *, void *, int);



/*
 * Canonical data records
 *	All input data is converted to this format before subsequently
 *	being converted to the selected output format. These are all
 *	based on the current version of SAT.
 */
#define MAX_IMAGE_LEN	4096
typedef struct image {		/* Raw record image */
	char	data[MAX_IMAGE_LEN];	/* Actual record data */
	int	len;			/* Length of raw data image */
	int	type;			/* Record type, ala SAT 4.0 */
	fmt_t	format;			/* Record format */
} image_t;


#define MAX_DATA_LEN 4096
typedef struct header {		/* File header */
	struct sat_filehdr fh;		/* Standard SAT stuff */
	char	*tz;			/* Timezone spec for this machine */
	char	*hostname;		/* Host name of this machine */
	char	*domainname;		/* Domain name for this machine */
	char	**users;		/* User names */
	char	**groups;		/* Group names */
	char	**hosts;		/* Host entries */
	int	*hostids;		/* Host IDs */

	char	vardata[MAX_DATA_LEN];	/* Variable-length data from header */
} header_t;

int	BuildFakeHeader(header_t *);
void	FreeHeader(header_t *);
int	ParseHeader(image_t *, header_t *);
int	ReadHeader(input_t *, image_t *);
int	WriteHeader(output_t *, header_t *);


typedef struct proc_acct {	/* Process accounting data */
	struct sat_proc_acct	pa;	/* General info */
} proc_acct_t;

typedef struct sess_acct {	/* Session accounting data */
	struct sat_session_acct	sa;	/* General info */
	sat_token_size_t spilen;	/* Size of service provider info */
	int	spifmt;			/* Format of service provider info */
	char	*spi;			/* Service provider info */
} sess_acct_t;

typedef struct record {		/* Generic data record */
	sat_token_size_t size;		/* Size of record */
	uint8_t	rectype;		/* Record type */
	uint8_t	outcome;		/* Outcome flag */
	uint8_t	sequence;		/* Sequence # */
	uint8_t	error;			/* Error info */
	time_t	clock;			/* Clock when record was created */
	uint8_t	ticks;			/* Hundredths of seconds at "clock" */
	uint8_t syscall;		/* Associated system call */
	uint16_t subsyscall;		/* Subsystem of system call */
	uid_t	satuid;			/* UID for audit purposes */
	char	*command;		/* Associated command name */
	char	*cwd;			/* Current working directory */
	dev_t	tty;			/* TTY device */
	pid_t	ppid;			/* Parent's process ID */
	pid_t	pid;			/* Process ID */
	uid_t	euid;			/* Effective UID */
	gid_t	egid;			/* Effective GID */
	uid_t	ruid;			/* Real UID */
	gid_t	rgid;			/* Real GID */
	uid_t	ouid;			/* "Owner" UID */
	gid_t	ogid;			/* "Owner" GID */
	int	numgroups;		/* Number of entries in "groups" */
	gid_t	*groups;		/* Other valid groups */
	cap_set_t   caps;		/* Capabilities */
	cap_value_t priv;		/* Capability used */
	uint8_t	privhow;		/* How priv was used */
	uint8_t	status;			/* Exit status */
	char	*custom;		/* custom SAT record text */

	unsigned int gotowner:1;	/* Flag: ouid/ogid fields valid */
	unsigned int gotcaps:1;		/* Flag: caps field is valid */
	unsigned int gotpriv:1;		/* Flag: priv/privhow fields valid */
	unsigned int gotstat:1;		/* Flag: stat field is valid */
	unsigned int gotext:1;		/* Flag: extended acct fields valid */

	struct acct_timers  timers;	/* Timers */
	struct acct_counts  counts;	/* Counts */

	union {				/* Type-specific data */
		proc_acct_t p;			/* process acct stuff */
		sess_acct_t s;			/* session acct stuff */
	} data;
} record_t;
/* A note on the "Owner" UID/GID: for flushed accounting records,  */
/* the real & effective UID/GID are those belonging to the process */
/* that did the flush. The "owner" UID/GID belongs to the actual   */
/* process (or an actual process, in the case of array sessions)   */
/* whose resources are being reported.				   */

void	FreeRecord(record_t *);
int	ParseRecord(const image_t *, record_t *);
int	ReadRecord(input_t *, image_t *);
int	WriteRecord(output_t *, record_t *);


/* SAT file header functions */
int	ParseHeader_SAT(image_t *, header_t *);
int	ReadHeader_SAT(input_t *, image_t *);
int	WriteHeader_SAT(output_t *, header_t *);

/* IRIX 6.2 extended accounting record functions */
int	ParseRecord_EA62(const image_t *, record_t *);
int	ReadRecord_EA62(input_t *, image_t *);
int	WriteRecord_EA62(output_t *, record_t *);

/* IRIX 6.4 extended accounting record functions */
int	ParseRecord_EA64(const image_t *, record_t *);
int	ReadRecord_EA64(input_t *, image_t *);
int	WriteRecord_EA64(output_t *, record_t *);

/* IRIX 6.5 extended accounting record functions */
int	ParseRecord_EA65(const image_t *, record_t *);
int	ReadRecord_EA65(input_t *, image_t *);
int	WriteRecord_EA65(output_t *, record_t *);

/* SVR4 record functions */
int	ParseRecord_SVR4(const image_t *, record_t *);
int	ReadRecord_SVR4(input_t *, image_t *);
int	WriteRecord_SVR4(output_t *, record_t *);

/* Text record functions */
int	WriteHeader_text(output_t *, header_t *);
int	WriteRecord_text(output_t *, record_t *);

/* Other function prototypes */
char	*Cmatch(char *, char *);
char	*GroupName(gid_t, char *, fmt_t);
FILE	*StartChildProcess(char *);
char	*UserName(uid_t, char *, fmt_t);


/*
 * Internal macros
 */
#define InternalError() fprintf(stderr,					\
				"%s: internal error at line %d of %s\n",\
				MyName, __LINE__, __FILE__);

/* Options for text_acctcom version. */
#define MEANSIZE   01
#define KCOREMIN   02
#define SEPTIME    04
#define IORW       010
#define AIO        020
#define ASH        040
#define GID        0100
#define PID        0200
#define PRID       0400
#define SPI        01000
#define STATUS     02000
#define TTY        04000
#define WAITTIME   010000

/* Length of the field separator - for text_awk version. */
#define FIELDSEP_LENGTH 6

/* Convert to kcore-minutes. */
#define MINT(kilobytes)  ((double) kilobytes / (60 * HZ))

/* Convert to kilobytes.  Page size is 4k. */
#define KCORE(clicks)  (clicks * 4)

/* Debugging flags */
#define DEBUG_DETAILS	0x00000001 	/* Show less relevant details */
#define DEBUG_INVALID	0x00000002 	/* Warn about invalid data */
#define DEBUG_UNKNOWN	0x00000004	/* Warn about unrecognized tokens */
#define DEBUG_RECORD	0x00000010	/* Show info about records */
#define DEBUG_TOKEN	0x00000020	/* Show info about tokens */
#define DEBUG_SKIPPED	0x00000040	/* Show info about skipped records */
#define DEBUG_OPTS	0x00000080	/* Show parsed options */

#define DEBUG_ALL	-1		/* Show all debugging messages */
#define DEBUG_DEFAULT	(DEBUG_UNKNOWN | DEBUG_INVALID)
#define DEBUG_VERBOSE	(DEBUG_DEFAULT | DEBUG_DETAILS)

extern unsigned long Debug;


/* Other global variables */
extern char *MyName;	/* The name we were invoked with, for error messages */
extern int align[4];	/* Alignment offsets */

extern ash_t    ashcut;
extern char    *cname;
extern int      combine;
extern double   cpucut;
extern char     fieldsep[FIELDSEP_LENGTH];
extern int      hdr_page;
extern accum_t  iocut;
extern double   memcut;
extern int      no_header;
extern int      option;
extern int      rppage;
extern double   syscut;

#endif  /* _ACCTCVT_COMMON_H_ */
