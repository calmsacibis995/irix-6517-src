/* Copyright 1986-1995 Silicon Graphics Inc., Mountain View, CA. */
#ident	"$Revision: 2.51 $"

#ifndef _LBOOT_H
#define _LBOOT_H

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <setjmp.h>
#include <string.h>
#include <bstring.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdarg.h>

/* max IO req level, 0, 8-10 are local intr, 1-7 are VME  */
#define MAXIPL	11

/*
 * epochs in system versions
 *	As old versions are forgotten, MIN_VERSION should be changed,
 *	`unifdef -DMIN_VERSION=xxx` should be run on all of the source,
 *	and the resulting debris should be cleaned up.
 */
#define VERSION_VME	1		/* new VME interrupt structures */
#define VERSION_DEVSWFLAGS 2		/* devsw struct replaces 'semdriv'
					 * with 'flags' */
#define VERSION_NONET	3		/* no more network processor */

#define MIN_VERSION 	3		/* limit on ancient history */
#define MAX_VERSION	3		/* most advanced this lboot knows */
#define MAX_VERSION_MSG "This lboot can accept VERSIONs of at most 3"

#define	DRV_MAXMAJORS	32	/* max major #s in one master file */
#define MAXTAGS		32		/* max # of TUNE-TAGs */

/*
 * Kernel structure
 *
 * This structure is linked to the driver linked-list so that all optional 
 * headers may be easily accessed.  In some cases, only the driver optional
 * headers are required -- "driver" is the head of this linked-list.
 * In other cases, all optional headers must be accessed -- "kernel" 
 * is the head of this linked-list.
 *
 * This structure must exactly match struct driver up to the point marked
 * in struct driver.  This is done to allow routines such as eval() and
 * initdata() to be passed a "struct kernel" pointer in place of a "struct
 * driver" pointer.
 */
struct	kernel
	{
	struct driver  *next;
	struct master  *opthdr;		/* mkboot() header */
	int		flag;		/* empty flags field */
	char           *mname;		/* master name */
	char           *dname;		/* kernel object */
	unsigned char	nctl;		/* zero */
	unsigned char	int_major;	/* zero */
	int		magic;		/* object file format */
	};

extern struct kernel kernel;
extern struct driver *driver;

/*
 * The kernel and driver linked list is illustrated below.
 *
 *         kernel                    driver
 *        +-------+                 +-------+
 *        |   X   |                 |   X   |
 *        +-- | --+                 +-- | --+
 *            |                         |
 *            V                         V
 *        +-------------+           +-------------+              +-------------+
 *        |struct kernel|           |struct driver|              |struct driver|
 *        |             |           |             |              |             |
 *        |      X----------------> |      X-------------------> |     NULL    |
 *        |             |           |             |              |             |
 *    +----------X      |       +----------X      |          +----------X      |
 *    |   +-------------+       |   |             |          |   |             |
 *    |                         |   |             |          |   |             |
 *    |                         |   +-------------+          |   +-------------+
 *    |                         |                            |
 *    |   +-------------+       |   +-------------+          |   +-------------+
 *    |   |             |       |   |             |          |   |             |
 *    +-> |struct master|       +-> |struct master|          +-> |struct master|
 *        |             |           |             |              |             |
 *        |             |           |             |              +-------------+
 *        +-------------+           |             |
 *                                  +-------------+
 */

/*
 * Driver structure
 *
 * Each driver (or module) existing in /boot is represented by an instance of
 * this structure.  It is built at initialization.  All object files found are
 * referred to as drivers throughout this file -- even though modules (NOTADRV)
 * are sometimes treated specially.
 */
struct	driver {
	struct driver  *next;
	struct master  *opthdr;		/* mkboot() header */
	int		flag;		/* flags */
	char           *mname;		/* master name */
	char           *dname;		/* driver object */
	unsigned char	nctl;		/* number of controllers */
	unsigned char	int_major;	/* internal major # */
	int		magic;		/* object file format */

	/* end of common elements between struct driver and struct kernel */
	struct lboot_edt     *edtp;		/* pointer to edt structure */
};

#define	MAXCNTL		16	/* maximum controllers allowed per device */

/*
 * Object file format magic # tags.
 */
#define ELF_OBJECT	1
#define	COFF_OBJECT	2

/*
 * Flags
 */
#define	LOAD	0x80		/* load this driver */
#define	INEDT	0x40		/* this driver matched in EDT */
#define LOADLAST 0x20		/* load this driver at end */
#define	INCLUDE	0x10		/* /etc/system: include this driver */
#define	EXCLUDE	0x08		/* /etc/system: exclude this driver */
#define DYNLOAD 0x04		/* load this driver dynamically */


struct lboot_vme_intrs {
	struct lboot_vme_intrs *v_next;
	char		*v_vname;	/* Name of int routine tied */
					/* to this interrupt. */
	unsigned char	v_vec;		/* The vme vector tied */
					/* to this interrupt. */
	unsigned char	v_brl;		/* The interrupt priority level */
					/* associated with this interrupt. */
	unsigned char	v_unit;		/* The software identifier */
					/* associated with v_vec. */
	
};

typedef unsigned long long kernptr_t;	/* support for 64-bit pointers */

struct lboot_iospace {
        unchar          ios_type;       /* io space type on the adapter */
        ulong_t         ios_iopaddr;    /* io space base address */
        ulong           ios_size;
	kernptr_t	ios_vaddr;	/* kernel virtual address */
};

#define NBASE 3
struct lboot_edt {
	struct lboot_vme_intrs *v_next;
	struct lboot_edt	*e_next;
	char		*e_init;
	unsigned char	v_cpuintr;	/* the cpu to send intr to */
	unsigned char	v_setcpuintr;	/* above value is valid if set */
	unsigned char	v_cpusyscall;	/* the cpu to run syscalls on */
	unsigned char	v_setcpusyscall;
        uint_t          e_bus_type;     
        uint_t          e_adap;         
        uint_t          e_ctlr;         
	struct lboot_iospace  e_space[NBASE];
};



# define	min(x,y)	(((x)<=(y))? (x) : (y))
# define	max(x,y)	(((x)>=(y))? (x) : (y))

#if _MIPS_SZPTR == 64
#define	LBOOT_LSEEK_MASK	0x7fffffffffffffff
#else
#define	LBOOT_LSEEK_MASK	0x7fffffff
#endif

typedef	unsigned char	boolean;
#define	TRUE	1
#define	FALSE	0
#define LSIZE 1024


#define EQUAL(a,b) (!(strcmp(a,b)))


enum error_num {	/* error message numbers */
	ER0, ER1, ER2, ER7, ER13, ER15, ER16, ER18, ER19,
	ER20, ER21, ER22, ER23, ER24, ER32, ER33, ER36, ER37,
	ER49, ER53, ER55, ER100, ER101, ER102, ER103, ER104, 
	ER105, ER106, ER107, ER108, ER109, ER110, ER120, ER121, ER122
};

/*
 * Global variables
 */
extern int Debug;		/* debugging flag */
extern int Verbose;		/* verbose output flag */
extern int Smart;		/* try to be smart */
extern int Autoconfig;		/* just do it or ask about it first */
extern int Autoreg;		/* do autoregister only */
extern int Noautoreg;		/* do autoconfig, but don't autoreg */
extern int Dolink;		/* ignore 'd' master file option */
extern int Nogo;		/* just see if we need an autoconfig */

extern int do_mreg_diags;	/* enable pre-exit warnings */

extern char *cc;		/* cc */
extern char *ccopts;		/* cc options */
extern char *ld;		/* ld */
extern char *ldopts;		/* ln options */
extern int errno;		/* C library error number */
extern char *cwd_slash;		/* current working directory */

extern int interrupts;		/* number of interrupt routines required */
extern int IRTNSIZE;		/* size of an interrupt routine */

extern struct driver *driver;	/* head of struct driver linked list */

extern int              driver_text;	/* total size of driver text */
extern int              driver_symbol;	/* max # of symbols/driver */
extern int              cdevcnt;
extern int              bdevcnt;
extern int		fmodcnt;
extern int		nfstype;

extern char             *root;		/* root prefix for file searches */
extern char		*toolpath;	/* path prefix for cc and ld */
extern char             *etcsystem;	/* /etc/system path name */
extern char		*kernel_master;	/* /unix path name */
extern char		*unix_name;	/* unix-new */
extern char             *slash_boot;	/* /boot directory name */
extern char             *slash_runtime;
extern char             *master_dot_d;	/* master file directory name */
extern char		*stune_file;	/* user modifiable tuning file */
extern char		tmp_dev_admin_dot_c[];	/* device admin data file */
extern char		tmp_drv_admin_dot_c[];	/* driver admin data file */
extern dev_t            rootdev;
extern dev_t            dumpdev;
extern dev_t            swapdev;
extern char		*bootrootfile;
extern char		*bootswapfile;
extern char		*bootdumpfile;
extern daddr_t          swplo;
extern int              nswap;
extern int		system_version;
extern char		cpuipl[];
extern char		*tune_tag[];
extern int		tune_num;


extern char		*do_tunetag(int, char **, dev_t *);
extern boolean		blankline(char *); /* is a line entirely blank? */
extern void		error(enum error_num, ...);
					/* error handling routine */
extern void		exclude(char *); /* mark a driver to be excluded */
extern void		include(char *, int); /* mark a driver to be included */
extern char		*lcase(char *);	/* uppercase string to lower case */
extern void		loadunix(void);
extern struct master	*mkboot(char *);/* get pointer to master structure */
extern char		*mymalloc(size_t);/* malloc with error checking */
extern char		*mycalloc(size_t,size_t);
extern void		myexit(int);	/* exit with diags */
extern void		read_and_check(int, char *, char *, unsigned);
					/* read(2) with error checking */
extern struct driver	*searchdriver(char *);
extern char		*concat(char *, char*);
extern char		*concat_free(char *, char*);
extern int		dcl_fnc(FILE *, int, char*);
extern boolean		function(char *prefix, char *name, char *symbol);
extern void		symbol_scan(char *,
				    void (*fun)(char *, void *, void *, void *),
				    void *, void *, void *);
extern struct tunemodule *gfind(char *);
extern struct tune	*tunefind(char *);
extern struct		tunemodule *gfind(char *);
extern void		find_kernel_sanity(char *, void *, void *, void *);
extern int		sys_select(struct dirent *);
extern int		parse_system(void);
extern void		do_mregister(void);
extern void		gen_tune(struct driver *, char *);
extern void		print_tune(FILE *);
extern void		rdstune(void);
extern int		suffix(char *, char *);
extern void		intrlock_sanity_check(void);
extern int		lock_on_cpu(struct master *, struct driver *);
extern void		do_mlist(void);
extern void		do_mloadreg(int, char *[], int);
extern void		do_munloadreg(int, int[], int);
extern int		parse(char **, unsigned, char *);
extern int		exprobe_parse(int, char **);
extern int		exprobe(void);
extern void		free_Vtuple_list(void);
extern int		exprobesp_parse(int, char **, int);
extern int		exprobesp(int, int);
extern void		free_Ntuple_list(void);
extern int		get_base(int, char *);
extern struct tune *	tfind(char *keyword);

extern void		init_dev_admin(void);
extern void		print_dev_admin(void);
extern 	char		**device_admin(char *,char **,int *,FILE *);
extern 	void		generate_dev_admin_table_head(FILE *);
extern 	void		generate_drv_admin_table_head(FILE *);
extern 	void		generate_dev_admin_table_tail(FILE *);
extern 	void		generate_drv_admin_table_tail(FILE *);
#endif /* !_LBOOT_H */
