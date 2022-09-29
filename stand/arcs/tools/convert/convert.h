/* $Header: /proj/irix6.5.7m/isms/stand/arcs/tools/convert/RCS/convert.h,v 1.4 1996/08/13 18:37:13 poist Exp $ */
/* $Log: convert.h,v $
 * Revision 1.4  1996/08/13 18:37:13  poist
 * addition of IP30 formats, version number support on command line.
 *
 * Revision 1.3  1995/11/23  21:13:46  pap
 * added support to read in pure binary files (-p option)
 *
 * Revision 1.2  1994/11/09  22:03:11  sfc
 * Merge sherwood/redwood up and including redwood version 1.2
 * > ----------------------------
 * > revision 1.2
 * > date: 93/07/15 19:46:18;  author: igehy;  state: ;  lines: +13 -10
 * > Made compile -fullwarn.
 * > ----------------------------
 *
 * Revision 1.2  1993/07/15  19:46:18  igehy
 * Made compile -fullwarn.
 *
 * Revision 1.1  1987/09/21  11:14:00  huy
 * Initial revision
 * */

#define TRUE 1
#define FALSE 0
#define OK 0
#define NOT_OK 1

/*
 * Control flags for match routine
 */
#define IGNORECASE 1
#define EXACTMATCH 2

/*
 * Standard type definitions
 */
typedef unsigned long address;

/*
 * Object file type control structures
 */

enum object_file_type { BERKELEY = 0, COFF = 1, ELF = 2, PURE = 3, MIPS = 4 };

struct object_file_type_table_descriptor {

	short magic;
	enum object_file_type type;
};

struct object_file_descriptor {

	enum object_file_type type;
	int (*initialize)(int);
	int (*read)(char *, int, int);
	int (*close)(int);
};

/*
 * Output format control structures
 */

struct format_descriptor {

	int blocking_factor;
	int (*initialize)(int);
	int (*convert)(char *, int);
	int (*write)(void);
	int (*terminate)(void);
	int (*setVersion)(int);

};

/*
 * Physical address of at which to load next byte
 */
extern unsigned long start_address;
extern unsigned long load_address;
extern int byte_number;
extern char *progname;
extern struct object_file_type_table_descriptor object_file_type_table[];
extern struct object_file_descriptor object_files[];
extern struct object_file_descriptor *object;
extern struct format_descriptor formats[];
extern struct format_descriptor *format;
extern int my_sex;			/* Host byte sex */
extern int swap;			/* True if headers and object of opposite sex */
extern int short_number;		/* added to split shorts HN 7/87 */
