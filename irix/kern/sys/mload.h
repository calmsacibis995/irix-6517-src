/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef _MLOAD_H
#define _MLOAD_H

#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.59 $"

/*
** mload - SGI module loader.
*/

#include <sys/types.h>
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/fstyp.h>
#include <sys/stream.h>
#include <sys/sema.h>

/*
 * medt_t - lboot can pass one or more edt's per driver, so mload
 *		needs a list of edt structs.
 */

typedef struct mload_edt {
	struct edt	*edtp;
	struct mload_edt *medt_next;
} medt_t;

#define	DRV_MAXMAJORS	32

/*
 * mod_driver - driver module information shared with user
 */
typedef struct mod_driver {
	medt_t	*d_edtp;		/* edt struct */
	int	d_cpulock;		/* CPU ID for locking */	
	char	d_type;			/* char, block, stream */
	int	d_nmajors;		/* number of major numbers */
	major_t	d_majors[DRV_MAXMAJORS];/* driver major numbers */
} mod_driver_t;

/* mod_driver types (bit mask) */
#define MDRV_CHAR	0x1	/* Character device */
#define MDRV_BLOCK	0x2	/* Block device     */
#define MDRV_STREAM	0x4	/* Streams driver   */

/*
 * mod_streams - streams module information shared with user
 */
typedef struct mod_streams {
	char	s_name[FMNAMESZ+1];
} mod_streams_t;


/*
 * mod_idbg - idbg module information shared with user
 */
typedef struct mod_idbg {
	int	i_value;
} mod_idbg_t;

/*
 * mod_fsys - filesystem module information shared with user
 */
typedef struct mod_fsys {
	char	vsw_name[FSTYPSZ+1];
} mod_fsys_t;

/*
 * mod_lib - lib module information shared with user
 */
typedef struct mod_lib {
	int	id;
} mod_lib_t;

/*
 * mod_symtab - symtab module information shared with user
 */
typedef struct mod_symtab {
	int	id;
	char	symtab_name[20];
	char	type;
} mod_symtab_t;

#define M_UNIXSYMTAB	1

/*
 * mod_enet - enet module information shared with user
 */
typedef struct mod_enet {
	int	id;
	medt_t	*e_edtp;		/* edt struct */
} mod_enet_t;


#define M_PREFIXLEN	16

#define M_CFG_CURRENT_VERSION	6
#define M_ALPHA_CFG_CURRENT_VERSION	1

#define	M_VERSION	"mload version 7.0"
#define	M_ALPHA_VERSION	"mload version 1.0"

/*
 * cfg_desc - one of these is filled in for the module and passed to syssgi.
 */
typedef struct cfg_desc {
	char	m_cfg_version;			/* should be M_CFG_CURRENT_VERSION */
	char	m_alpha_cfg_version;		/* should be M_ALPHA_CFG_CURRENT_VERSION */
	char	*m_fname;			/* object file name */
	int	m_fnamelen;			/* includes '\0' */
	void	*m_data;			/* module type specific data */
	char	m_prefix[M_PREFIXLEN];		/* driver/fsys prefix */
	clock_t	m_delay;			/* min ticks before unloading module */
	long	m_id;				/* identifier */
	int	m_flags;			/* misc flags */
	int	m_tflags;			/* timeout state */
	int	m_refcnt;			/* number of callers in open/push */
	struct ml_info *m_info;			/* back pointer to minfo struct */
	struct cfg_desc *m_next;		/* next descriptor in hash list */
	char	m_type;				/* module type */
	int	m_timeoutid;			/* id for timeout call for auto unld */
	sema_t	m_transsema;			/* sema for transitions */
	struct device_driver_s *m_device_driver;/* device driver handle */
} cfg_desc_t;

/*
 * mod_info - constant sized structure to copyout list of loaded modules
 */
union mod_dep {
	mod_driver_t d;
	mod_streams_t s;
	mod_fsys_t f;
	mod_idbg_t i;
	mod_lib_t l;
	mod_symtab_t y;
	mod_enet_t e;
};

/*
 * structure for use with syssgi(CF_LIST)
 */
typedef struct mod_info {
	char	m_cfg_version;
	char	m_alpha_cfg_version;
	char	m_fname[MAXPATHLEN];
	char	m_prefix[M_PREFIXLEN];
	clock_t	m_delay;
	int	m_flags;
	int	m_id;
	char	m_type;
	union mod_dep	m_dep;
} mod_info_t;

#define m_driver m_dep.d
#define m_stream m_dep.s
#define m_fsys m_dep.f
#define m_idbg m_dep.i
#define	m_lib m_dep.l
#define m_symtab m_dep.y

/* configuration command */
#define	CF_LOAD		1		/* load module command */
#define	CF_UNLOAD	2		/* unload module command */
#define	CF_REGISTER	3		/* register module command */
#define	CF_UNREGISTER	4		/* unregister module command */
#define	CF_LIST		5		/* module list command */
#define	CF_DEBUG	6		/* turn verbose error msgs on|off */

/* symbol table command */
#define SYM_ADDR	6		/* get address of symbol */
#define SYM_NAME	7		/* get name of symbol */
#define SYM_ADDR_ALL	8		/* get addr of symbol, all modules */
#define SYM_NAME_ALL	9		/* get name of symbol, all modules */

/* module type */
#define	M_DRIVER	1		/* driver type module */
#define	M_STREAMS	2		/* streams type module */
#define	M_IDBG		3		/* idbg */
#define	M_LIB		4		/* generic library */
#define	M_FILESYS	5		/* file system type module */
#define M_ENET		6		/* ethernet type module */
#define	M_SYMTAB	7		/* symtab type module */
#define	M_OTHER		99		/* other type module */

/* misc flags */
#define	M_NOINIT	1		/* load but don't initialize */
#define M_INITED	2		/* module has been initialized */
#define M_SYMDEBUG	4		/* include statics in symtab */

#define M_REGISTERED	0x10		/* module registered but not loaded */
#define M_LOADED	0x20		/* module loaded */
#define M_TRANSITION	0x40		/* module in transition - stay away */
#define M_UNLOADING	0x80		/* module unloading */
#define M_NOAUTOUNLOAD	0x100		/* do not auto unload */

/* timeout flags */
#define M_EXPIRED	0x1		/* pending autounload timeout */

/* m_delay values */
#define M_NOUNLD	-2		/* don't auto-unload this module */
#define M_UNLDDEFAULT	-1		/* use the default timeout value */
#define M_UNLDVAL	60*100		/* auto-unload timeout val */

#define MAJOR_ANY	0xffffffff	/* load module with any major number */

/*
 * mload errors
 */


/* EPERM		1 */
#define MERR_ENOENT	2	/* No such file or directory */
#define MERR_ENAMETOOLONG 3 	/* path name is too long */
#define	MERR_VREG	4	/* file not a regular file */
/* EIO			5 */
/* ENXIO		6 */
#define	MERR_VOP_OPEN	7	/* VOP_OPEN failed */
/* ENOEXEC		8 */
#define	MERR_VOP_CLOSE	9	/* VOP_CLOSE failed */
#define	MERR_COPY	10	/* error on copyin/copyout */

/* coff errors */
#define MERR_BADMAGIC	11	/* bad magic number in header */
#define MERR_BADFH	12	/* error reading file header */
#define MERR_BADAH	13	/* error reading a.out header */
#define MERR_BADSH	14	/* error reading section header */
#define MERR_BADTEXT	15	/* error reading text */
/* EBUSY		16 */
#define MERR_BADDATA	17	/* error reading data */
#define MERR_BADREL	18	/* error reading relocation records */
#define MERR_BADSYMHEAD	19	/* error reading sym header */
#define MERR_BADEXSTR	20	/* error reading external string table */
#define MERR_BADEXSYM	21	/* error reading external symbol table */
#define MERR_NOSEC	22	/* error reading file hdr sections */
#define	MERR_NOSTRTAB	23	/* no string table found for object */
#define	MERR_NOSYMS	24	/* no symbols found for object */

/* elf errors */
#define MERR_BADARCH	25	/* bad EF_MIPS_ARCH type */
#define	MERR_SHSTR	26	/* err reading section hdr string table */
#define MERR_SYMTAB	27	/* error reading section symbol table */
#define MERR_STRTAB	28	/* error reading section string table */
#define MERR_NOTEXT	29	/* no text or textrel section found */
#define MERR_SHNDX	30	/* bad section index */

#define MERR_UNKNOWN_CFCMD 31	/* unknown CF_ command */
#define MERR_UNKNOWN_SYMCMD 32	/* unknown CF_ command */

#define	MERR_FINDADDR	33	/* address not found in symbol tables */
#define	MERR_FINDNAME	34	/* name not found in symbol tables */
#define	MERR_SYMEFAULT	35	/* symtab address fault */

#define MERR_NOELF	36	/* ELF not supported */
#define MERR_UNSUPPORTED 37 	/* this feature is unsupported */
#define MERR_LOADOFF	38	/* dyn loading turned off */

#define	MERR_BADID	39	/* module id not found */
#define	MERR_NOTLOADED	40	/* module not loaded */
#define	MERR_NOTREGED	41	/* module not registered */
#define	MERR_OPENBUSY	42	/* can't open - busy */
#define	MERR_UNLOADBUSY 43	/* can't unload - busy */
#define	MERR_UNREGBUSY	44	/* can't unregister - busy */
#define MERR_ILLDRVTYPE	45	/* illegal driver type */
#define	MERR_NOMAJOR	46	/* d_nmajors passed in 0, should be at least 1 */
#define	MERR_NOMAJORS	47	/* out of major numbers */
#define	MERR_ILLMAJOR	48	/* major number < 0 or > maxmajors */
#define	MERR_MAJORUSED	49	/* major number already in use */
#define MERR_SWTCHFULL	50	/* switch table full */
#define	MERR_UNLDFAIL	51	/* unload function failed */
#define	MERR_DOLD	52	/* D_OLD drivers not loadable */

#define	MERR_NODEVFLAG	53	/* no xxxdevflag found */
#define	MERR_NOINFO	54	/* no xxxinfo found */
#define	MERR_NOOPEN	55	/* no xxxopen found */
#define	MERR_NOCLOSE	56	/* no xxxclose found */
#define	MERR_NOMMAP	57	/* no xxxmmap found */
#define	MERR_NOSTRAT	58	/* no xxxstrategy found */
#define	MERR_NOSIZE	59	/* no xxxsize found */
#define	MERR_NOUNLD	60	/* no xxxunload function found */
#define	MERR_NOVFSOPS	61	/* no xxx_vfsops found in sym tab */
#define	MERR_NOVNODEOPS	62	/* no xxx_vnodops found in sym tab */
#define	MERR_NOINIT	63	/* no xxx_init found in sym tab */
#define	MERR_NOINTR	64	/* no xxxintr found in sym tab */
#define MERR_NOEDTINIT	65	/* no xxxedtinit found in sym tab */

#define	MERR_NOSTRNAME	66	/* missing stream name */
#define	MERR_STRDUP	67	/* duplicate streams name */
#define MERR_NOPREFIX	68	/* no prefix sent in descriptor */
#define MERR_NOMODTYPE	69	/* no module type sent in descriptor */
#define MERR_BADMODTYPE	70	/* bad module type sent in descriptor */
#define	MERR_BADCFGVERSION 71 /* cfg version out of sync with kernel */
#define	MERR_BADVERSION	72	/* version mismatch */
#define	MERR_NOVERSION	73	/* no version string found */

#define	MERR_BADLINK	74	/* can't resolve all symbols */
#define	MERR_BADJMP	75	/* addr outside jal range - use jalr */
#define	MERR_BADRTYPE	76	/* bad relocation type */
#define	MERR_BADADTYPE	77	/* bad adend type */
#define	MERR_GP		78	/* can't handle GP - use -G 0 */
#define	MERR_BADSC	79	/* unexpected storage class */
#define	MERR_REFHI	80	/* unexpected REFHI */
#define	MERR_NORRECS	81	/* no relocation records found */
#define	MERR_SCNDATA	82	/* bad data section encountered */
#define MERR_COMMON	83	/* common storage class found */
#define MERR_JMP256	84	/* tried to reloc j in 256MB boundary */

#define	MERR_LIBUNLD	85	/* library modules can't be unloaded */
#define	MERR_LIBREG	86	/* library modules can't be reg/unreg */
#define	MERR_NOLIBIDS	87	/* out of library module ids */

#define	MERR_IDBG	88	/* error loading idbg.o module */
#define	MERR_IDBGREG	89	/* idbg modules can't be reg/unreg */

#define	MERR_NOENETIDS	90	/* out of enet module ids */
#define MERR_ENETREG	91	/* enet drivers can't be registered */
#define MERR_ENETUNREG	92	/* enet drivers can't be unregistered */
#define MERR_ENETUNLOAD	93	/* enet drivers can't be unloaded */

#define	MERR_NOFSYSNAME	94	/* no file system name specified */
#define	MERR_DUPFSYS	95	/* fsys already in vfssw */

#define	MERR_VECINUSE	96	/* v_vec specfied in edt is used */
#define	MERR_BADADAP	97	/* no adapter found */
#define	MERR_NOEDTDATA	98	/* no edt data for enet driver */

#define	MERR_ELF64	99	/* 64bit kernel requires 64bit loadable obj */
#define	MERR_ELF32	100	/* 32bit kernel requires 32bit loadable obj */
#define	MERR_ELF64COFF	101	/* 64bit kernel requires ELF 64bit obj */
#define	MERR_ET_REL	102	/* must be a relocatable object */

#define	MERR_SYMTABREG	103	/* symtab modules can't be reg/unreg */
#define	MERR_NOSYMTABIDS 104	/* out of symtab module ids */
#define MERR_NOSYMTAB	105	/* no symbol table found in object */
#define MERR_DUPSYMTAB	106	/* dup symtab filename */
#define MERR_NOSYMTABAVAIL 107	/* no runtime symtab available to load */
#define MERR_SYMTABMISMATCH 108	/* symbol table doesn't not match kernel */
#define MERR_NOIDBG	109	/* idbg.o must be loaded before xxxidbg.o */
#define	MERR_ELFN32	110	/* N32 kernel requires N32 loadable obj */
#define MERR_UNREGFAIL  111	/* unregister failed */

#define MAXSYMNAME	128		/* maximum length of symbol name */

#ifdef	_KERNEL

/* external functions for accessing the runtime symbol table */
extern int st_findaddr(char *, __psunsigned_t **);
extern int st_findname(__psunsigned_t *, char **);

#include <sys/systm.h>
extern int sgi_mconfig(int, void *, int, rval_t *);
extern int sgi_symtab (int, void *, void *);

extern void mlqdetach (queue_t *);

extern int module_unld_delay;		/* default delay for auto-unload */

#endif	/* _KERNEL */

#ifdef __cplusplus
}
#endif
#endif /* _MLOAD_H */
