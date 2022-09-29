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
#ifndef _MLOADPVT_H
#define _MLOADPVT_H

#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.18 $"

/*
** mloadpvt.h - private header for SGI module loader.
*/

/* 
 * m_data for driver modules
 */
typedef struct cfg_driver {
	mod_driver_t	d;

	void	(*d_init)();
	void	(*d_start)();
	void	(*d_halt)();

	int	(*d_ropen)();		/* real open routine */
	int	(*d_rclose)();		/* real close routine */
	int	(*d_rattach)();		/* real attach routine */
	int	(*d_rdetach)();		/* real detach routine */
	int	(*d_unload)();		/* unload routine */
	int	(*d_unreg)();		/* unregister routine */
	int	(*d_reg)();		/* register routine */
	int	d_id;			/* identifier for this driver */
	struct bdevsw *d_bdevsw;	/* bdevsw pointer */
	struct cdevsw *d_cdevsw;	/* cdevsw pointer */
} cfg_driver_t;

struct cred;
struct queue;
typedef int (*drvopenfunc_t)(dev_t *, int, int, struct cred *);
typedef int (*drvclosefunc_t)(dev_t, int, int, struct cred *);
typedef int (*drvattachfunc_t)(vertex_hdl_t);
typedef int (*drvdetachfunc_t)(vertex_hdl_t);
typedef int (*strdrvopenfunc_t)(struct queue *, dev_t *, int,int,struct cred *);
typedef int (*strdrvclosefunc_t)(struct queue *, int, struct cred *);

#define d_edt	d.d_edtp
#define d_lck	d.d_cpulock
#define d_typ	d.d_type

/* 
 * m_data for streams modules
 */
typedef struct cfg_streams {
	mod_streams_t		s;
	int			s_fmod;		/* fmodsw entry number */
	struct streamtab	*s_info;	/* address of info struct */
	void			*s_strinfo;	/* strtab for registered func */
	strdrvopenfunc_t	s_ropen;	/* real open routine */
	strdrvclosefunc_t	s_rclose;	/* real close routine */
	int			(*s_unload)(void);	/* unload routine */
} cfg_streams_t;

/* 
 * m_data for the idbg module
 */
typedef struct cfg_idbg {
	mod_idbg_t	i;
	idbgfunc_t      *i_func;	/* idbg function structure */
	idbgfunc_t      *i_oldfunc;	/* old idbg function structure */
} cfg_idbg_t;

/* 
 * m_data for filesystem modules
 */
typedef struct cfg_fsys {
	mod_fsys_t	f;
	int		f_vfs;		/* vfs switch entry number */
	long		vsw_flag;
	int		(*d_unload)();	/* unload routine */
} cfg_fsys_t;

/* 
 * m_data for the lib module
 */
typedef struct cfg_lib {
	mod_lib_t	l;
} cfg_lib_t;

/* 
 * m_data for the symtab module
 */
typedef struct cfg_symtab {
	mod_symtab_t	s;
} cfg_symtab_t;

/* 
 * m_data for the enet module
 */
typedef struct cfg_enet {
	mod_enet_t	e;
	int		(*d_unload)();	/* unload routine */
} cfg_enet_t;

#define	e_edt	e.e_edtp


/* Object file structs and defines for COFF and ELF */

/* 64-bit kernel */

#if _MIPS_SIM == _ABI64
#define	Elf_Ehdr	Elf64_Ehdr
#define Elf_Shdr	Elf64_Shdr
#define Elf_Sym		Elf64_Sym
#define Elf_Rel		Elf64_Rel
#define Elf_Rela	Elf64_Rela
#define	Elf_Word	Elf64_Word

#define	ELF_ST_BIND	ELF64_ST_BIND
#define	ELF_ST_TYPE	ELF64_ST_TYPE
#define	ELF_ST_INFO	ELF64_ST_INFO

#define	ELF_R_SYM	ELF64_R_SYM
#define	ELF_R_TYPE	ELF64_R_TYPE
#define	ELF_R_INFO	ELF64_R_INFO

#define ELFCLASS	ELFCLASS64
#define ELF_ABI		ABI_IRIX5_64

typedef	__uint64_t	ADDR;
typedef	__int64_t	elfarg_t;

#else	/* 32-bit kernel */

#define	Elf_Ehdr	Elf32_Ehdr
#define Elf_Shdr	Elf32_Shdr
#define Elf_Sym		Elf32_Sym
#define Elf_Rel		Elf32_Rel
#define Elf_Rela	Elf32_Rela
#define	Elf_Word	Elf32_Word

#define	ELF_ST_BIND	ELF32_ST_BIND
#define	ELF_ST_TYPE	ELF32_ST_TYPE
#define	ELF_ST_INFO	ELF32_ST_INFO

#define	ELF_R_SYM	ELF32_R_SYM
#define	ELF_R_TYPE	ELF32_R_TYPE
#define	ELF_R_INFO	ELF32_R_INFO

#define ELFCLASS	ELFCLASS32
#define ELF_ABI		ABI_IRIX5

typedef	__uint32_t	ADDR;
typedef	__int32_t	elfarg_t;

#endif	/* _ABI64 */

/* 
 * Special ELF sections. Only a subset of sections that we care
 * about for dynamic loading are included. NOTE - if this changes, 
 * change wordlist in melf.c also. 
 */
typedef enum {
	SEC_TEXT, SEC_TEXTREL, SEC_TEXTRELA, SEC_DATA, SEC_DATAREL, 
	SEC_DATARELA, SEC_RODATA, SEC_RODATAREL, SEC_RODATARELA, SEC_BSS, 
	SEC_SYMTAB, SEC_SHSTRTAB, SEC_STRTAB,
	SEC_LAST_ENUM
} section_index;

/* table of memory requirement for special sections */
typedef struct {
	char *name;
	section_index sec;		/* special section */
} key_section_info;

typedef struct elfobjinfo {
	Elf_Ehdr	*ehdr;		/* ELF header */
	Elf_Shdr	*shdr;		/* section header list*/
	Elf_Sym		*symtab;	/* symbol table */
	Elf_Rel		*text_reloctab;
	Elf_Rela	*text_relocatab;
	Elf_Rel		*data_reloctab;
	Elf_Rela	*data_relocatab;
	Elf_Rel		*rodata_reloctab;
	Elf_Rela	*rodata_relocatab;
	char		*stringtab;	/* string table */
	char		*shstringtab;	/* section header string table */
	void		**scn_dataptrs;
	int		text_scn;
	int		data_scn;
	int		rodata_scn;
	int		bss_scn;
	int		key_sections[SEC_LAST_ENUM]; /* shdr list indices */
} elf_obj_info_t;


/* COFF */

typedef struct coffobjinfo {
        struct filehdr	*fh;		/* see filehdr.h */
        struct aouthdr	*ah;		/* see aouthdr.h */
        struct scnhdr	*sh;		/* see scnhdr.h */
	struct reloc	*reloctab;	/* relocation entry table - see 
					   reloc.h */
	pHDRR 		symheader;	/* symbolic header struct - see 
					   sym.h */
	pEXTR 		symtab;		/* symbol table - see sym.h */
	char 		*stringtab;	/* external string table */
	void		**scn_dataptrs;	/* list of section data pointers */
	int		text_scn;	/* scn # used to index into 
					   scn_dataptrs */
	int		rdata_scn;	/* scn # used to index into 
					   scn_dataptrs */
	int		data_scn;	/* scn # used to index into 
					   scn_dataptrs */
	int		bss_scn;	/* scn # used to index into 
					   scn_dataptrs */
} coff_obj_info_t;


/*
 * ml_sym and ml_undef are used when loading 2 or more modules  which 
 * cross-reference each other. NOTE: ml_sym will also be used by symmon
 * and dbx.
 */

typedef struct ml_undef {
	char	*addr;			/* address of jal to undefined proc */
	char	*str;			/* name of undefined symbol */
	struct ml_undef *next;		/* next ml_undef struct in list */
} ml_undef_t;

typedef struct ml_sym {
	__psunsigned_t m_addr;
	__uint32_t m_off;
} ml_sym_t;

typedef struct ml_info {
	int		flags;
	struct ml_info 	*ml_next;	/* next module info struct */
	void		*ml_obj;	/* object file info struct */
	void  		*ml_symtab;	/* module's symbol table */
	char 	  	*ml_stringtab;	/* module's string list */
	cfg_desc_t 	*ml_desc;	/* config descriptor */
	char		*ml_base;	/* base memory for text/data/bss */
	char		*ml_text;	/* relocd text/data/bss w/i ml_base */
	char		*ml_end;	/* highest address used by module */
	int	  	ml_nsyms;	/* number of symbols */
	int		ml_strtabsz;	/* size of string table */
} ml_info_t;

extern ml_info_t *mlinfolist;

#define	ML_COFF		0x0001		/* coff object */
#define ML_ELF		0x0002		/* elf object */

/* TODO - these are from lboot - shouldn't they go to a sys/ header file ? */
#define	MAJOR_DONTCARE	0x1ff

/*
 * mloaddebug 
 */

extern __psint_t mloaddebug;		/* from mload.c */

#define mprintf(x)	if (mloaddebug) printf x

#define	MNODEBUG	0x0000	/* don't print any debug info */
#define	MFAILURES	0x0001	/* print failure messages */
#define MUNDEF		0x0002	/* print info about undefines */
#define	MNOLOAD		0xdead	/* don't allow load, register, unload, unreg */

#define	MMANUNLOAD	0	/* unload called from user process */
#define	MAUTOUNLOAD	1	/* unload called via autounload */


#if !defined MIPSEB && !defined MIPSEL
	****** ERROR -- MUST BE ONE OR THE OTHER !!
#endif /* !defined MIPSEB && !defined MIPSEL */

#if defined MIPSEB
#define HighShortOf( longIntPtr ) ( ( (short int *)( longIntPtr ) )[0] ) 
#define LowShortOf( longIntPtr ) ( ( (short int *)( longIntPtr ) )[1] )
#else /* !defined MIPSEL */
#define HighShortOf( longIntPtr ) ( ( (short int *)( longIntPtr ) )[1] )
#define LowShortOf( longIntPtr ) ( ( (short int *)( longIntPtr ) )[0] )
#endif

#if defined MIPSEB
#define UHighShortOf(longIntPtr)(((unsigned short int *)(longIntPtr))[0])
#define ULowShortOf(longIntPtr)(((unsigned short int *)(longIntPtr))[1])
#else /* !defined MIPSEL */
#define UHighShortOf(longIntPtr)(((unsigned short int *)(longIntPtr))[1])
#define ULowShortOf(longIntPtr)(((unsigned short int *)(longIntPtr))[0])
#endif

#ifdef __cplusplus
}
#endif
#endif /* _MLOADPVT_H */
