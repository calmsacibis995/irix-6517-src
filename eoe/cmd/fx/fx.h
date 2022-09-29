#include <sys/param.h>	/* for types.h and BBSIZE */
#include <sys/dkio.h>
#include <sys/dvh.h>
#include <ctype.h>

#ifdef ARCS_SA
extern int errno;
#include <arcs/errno.h>
#else
#include <errno.h>
#endif

#ifdef _STANDALONE
#ifdef ARCS_SA
# define vhchksum(vh,len)	vh_checksum((struct volume_header *)vh)
#else
# define vhchksum(vh,len)	vh_checksum(vh,len)
#endif
#endif

/*	NOTE that this should NOT be used to size arrays relating to
	disk activity if possible, since other block sizes are now
	suppported by the scsi driver, and someday for the other
	drivers also.
*/
#define DEV_BSIZE BBSIZE

/*	ideally, this should come from param.h, when we finally decide
	what it should be...
*/
#define MAX_BSIZE 4096	/* max block size we support */

/*
 * definitions for a simple menu system.  generally, a menu is an
 * array of items.
 *
 * items are selected by entering a unique prefix or an index number.
 *
 * there are 3 anticipated menu interactions:
 * a) where the selected item sets a Parameter:  the index of the item
 * is returned.  there is an associated value.
 * b) where the selected item makes a choice of Function:  the function
 * is called via callsub(), which sets up its argument vector, prompt,
 * and quit label.
 * c) where the selected item asks for Help:  the help function is called.
 * help functions take 1 parameter, which is 0 if the help function is to
 * explain itself, 1 if the function is to explain its subordinates.
 *
 * in any case, the value of the selected item is used to index into
 * an array of objects specific to the menu.
 */

/* Extension by Dave H 3/21/88. We now define 2 modes for fx: a 'full'
   mode for expert users, and a reduced, safe, mode for normal release.
   Each item is marked to show whether it is available only in 'expert'
   mode, a new field is added to carry this information. */

struct item
{
	char *name;			/* name of item */
	int value;			/* value for item */
	int expert;			/* 1 = expert only, 0 = always */
	int uniqchars;		/* number of chars needed for uniqueness;
			filled in at run time for menus */
};
typedef struct item ITEM;

struct menu
{
	struct item *items;		/* list of items */
	const char *title;		/* menu title for printing */
	char *name;			/* menu name for prompting */

	void (*initfunc)(void);		/* function to call for menu if
			* non-null, called each time menu is displayed. */
	int nitems;			/* length of item list */
	int maxwidth;		/* max width of item names */
	struct menu *dotdot;	/* parent menu */
};
typedef struct menu MENU;

struct funcval
{
	void (*func)(void);		/* function for a func item */
	char **helpstrings;		/* helpstrings for a func item */
	struct menu *menu;		/* menu for a menu item */
};
typedef struct funcval FUNCVAL;

/*	NOTE that it is almost always inappropriate to use sizeof(CBLOCK)
	or sizeof some variable of type CBLOCK (except when zero'ing or
	filling).  Usually one would want DP(&vh)->dp_secbytes,
	or something like that.
*/
struct cblock
{
	char c[MAX_BSIZE];		/* block of data */
};
typedef struct cblock CBLOCK;

struct strbuf
{
	char c[128];		/* max size string */
};
typedef struct strbuf STRBUF;


/* include all the struct definitions, etc. (after all the typedefs) */
#include "prototypes.h"


/* partition 2 was originally going to be a small /usr, 3 was going
 * to be a 'leftover', and 5 was going to be a 'big' usr partition.
 * Earlier fx versions made 2, 5, and 6 the same. */

#define PTNUM_FSROOT		0		/* root partition */
#define PTNUM_FSSWAP		1		/* swap partition */
#define PTNUM_FSUSR		6		/* usr partition */
#define PTNUM_FSALL		7		/* one big partition */
#define PTNUM_VOLHDR		8		/* volume header partition */
#define PTNUM_VOLUME		10		/* entire volume partition */
#define PTNUM_XFSLOG		15		/* log partition for /root */



# define VP(x)		((struct volume_header *)x)
# define DP(x)		(&VP(x)->vh_dp)
# define PT(x)		(VP(x)->vh_pt)
# define DT(x)		(VP(x)->vh_vd)


/*
 * MAX_DRIVE, MAX_CONTROLLER, and MAX_LUN are used to restrict user input
 * and to provide guidelines by limiting the names generated when opening
 * a disk.  They're not tied to a particular I/O architecture.
 * See also irix/include/diskinfo.h.
 */
# define MAX_DRIVE	128
# define MAX_CONTROLLER	((1LL << 32) - 1LL)
# define MAX_LUN	256

# define MAX_LINE	(sizeof (STRBUF))
# define MAX_VEC	(MAX_LINE/2)
# define INBUFSIZE	(MAX_LINE+MAX_VEC*sizeof(char*))


/* global declarations */


extern int expert;	/* only nondestructive tests allowed in
			 * nonexpert mode */
extern CBLOCK sg;				/* sgi disk label */
extern struct volume_header vh;			/* current vh block */
extern char *driver;				/* driver name */
extern uint drivernum;				/* driver number */
extern int changed;				/* flag - label changed */
extern uint ctlrno;				/* controller number */
extern uint driveno;				/* drive number */
extern int scsilun;				/* LUN for scsi disks */
extern uint ctlrtype;
extern int ioretries;		/* error retries */
extern int isrootdrive;	/* see pt.c */
extern int majorchange;
extern int formatrequired;
extern int doing_auto;
extern unchar *extrack, *chktrack, defpat[];
extern size_t defpatsz;
extern int exercising;
extern uint partnum;
extern int device_name_specified;
extern char device_name[128];

#define SCSI_NAME	"dksc"			/* Name of scsi disk driver */
#define SCSI_NUMBER	0			/* Internal number for scsi */

#ifndef _STANDALONE
/* smfd floppy under unix only, and only if SCSI defined. */
#define SMFD_NAME	"fd"			/* prefix for floppy disk devices */
#define SMFD_NUMBER	3			/* Internal number for smfd */
#endif


/* prototypes within fx */

#ifdef ARCS_SA
int open(char *, int);
void fflush(void *);
#define strerror(ERRNO)	arcs_strerror(ERRNO)
#endif
