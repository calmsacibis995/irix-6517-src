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

/*
 *	SGI module loader routines.
 */

#ident	"$Revision: 1.160 $"

#include <sys/types.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <sys/uio.h>
#include <sys/pathname.h>
#include <sys/vnode.h>
#include <sys/edt.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/cred.h>
#include <sys/ddi.h>
#include <sys/driver.h>
#include <sys/kabi.h>
#include <sys/stream.h>
#include <sys/strids.h>
#include <sys/strsubr.h>
#include <sys/runq.h>
#include <sys/vfs.h>
#include <sys/pda.h>
#include <sys/sysmacros.h>
#include <elf.h>
#include <aouthdr.h>
#include <filehdr.h>
#include <scnhdr.h>
#include <sym.h>
#include <symconst.h>
#include <reloc.h>
#include <sys/kmem.h>
#include <sys/fstyp.h>
#include <sys/idbg.h>
#include <sys/mload.h>
#include <sys/mloadpvt.h>
#include <sys/capability.h>
#include <sys/vmereg.h>
#include <string.h>
#include <ksys/sthread.h>
#include <sys/iograph.h>

__psint_t mloaddebug = 0;		/* see mloadpvt.h */

/*#define	MTRACE	1*/

#ifdef DEBUG
#define bfill(p,c,v) \
	{char *pt = (char *)p; int ct = c; while (ct--) *pt++ = v;}
#endif

static char *mversion = M_VERSION;

/* prototypes */

/* generic functions */
static int  mload (cfg_desc_t *);
static int  munload (long, int);
static int  mregister (cfg_desc_t *);
static int  munregister (long);
static int  mlist(void *, int, rval_t *);

/* driver specific */
static int  drv_init (int);
static int  drv_load (ml_info_t *);
static int  drv_unload (cfg_desc_t *);
static int  drv_reg (ml_info_t *);
static int  drv_unreg(cfg_desc_t *);
static int  drv_makedata(cfg_desc_t *);
static void drv_freedata(cfg_desc_t *);
static int  drv_id(cfg_desc_t *);
static int  drv_lock(cfg_desc_t *);
static void drv_unlock(cfg_desc_t *, int);
static void drv_copydata(cfg_desc_t *, void *);

/* streams specific */
static int  str_init (int);
static int  str_load (ml_info_t *);
static int  str_unload (cfg_desc_t *);
static int  str_reg (ml_info_t *);
static int  str_unreg (cfg_desc_t *);
static int  str_makedata(cfg_desc_t *);
static void str_freedata(cfg_desc_t *);
static int  str_id(cfg_desc_t *);
static int  str_lock(cfg_desc_t *);
static void str_unlock(cfg_desc_t *, int);
static void str_copydata(cfg_desc_t *, void *);

/* idbg specific */
static int  idbg_init (int);
static int  idbg_load (ml_info_t *);
static int  idbg_unload (cfg_desc_t *);
static int  idbg_reg (ml_info_t *);
static int  idbg_unreg (cfg_desc_t *);
static int  idbg_makedata(cfg_desc_t *);
static void idbg_freedata(cfg_desc_t *);
static int  idbg_id(cfg_desc_t *);
static int  idbg_lock(cfg_desc_t *);
static void idbg_unlock(cfg_desc_t *, int);
static void idbg_copydata(cfg_desc_t *, void *);

/* generic library  specific */
static int  lib_init (int);
static int  lib_load (ml_info_t *);
static int  lib_unload (cfg_desc_t *);
static int  lib_reg (ml_info_t *);
static int  lib_unreg (cfg_desc_t *);
static int  lib_makedata(cfg_desc_t *);
static void lib_freedata(cfg_desc_t *);
static int  lib_id(cfg_desc_t *);
static int  lib_lock(cfg_desc_t *);
static void lib_unlock(cfg_desc_t *, int);
static void lib_copydata(cfg_desc_t *, void *);

/* generic symtab  specific */
static int  symtab_init (int);
static int  symtab_load (ml_info_t *);
static int  symtab_unload (cfg_desc_t *);
static int  symtab_reg (ml_info_t *);
static int  symtab_unreg (cfg_desc_t *);
static int  symtab_makedata(cfg_desc_t *);
static void symtab_freedata(cfg_desc_t *);
static int  symtab_id(cfg_desc_t *);
static int  symtab_lock(cfg_desc_t *);
static void symtab_unlock(cfg_desc_t *, int);
static void symtab_copydata(cfg_desc_t *, void *);
static int  symtab_hold(void);
static int  symtab_rele(void);

/* file system specific */
static int  fsys_init (int);
static int  fsys_load (ml_info_t *);
static int  fsys_unload (cfg_desc_t *);
static int  fsys_reg (ml_info_t *);
static int  fsys_unreg (cfg_desc_t *);
static int  fsys_makedata(cfg_desc_t *);
static void fsys_freedata(cfg_desc_t *);
static int  fsys_id(cfg_desc_t *);
static int  fsys_lock(cfg_desc_t *);
static void fsys_unlock(cfg_desc_t *, int);
static void fsys_copydata(cfg_desc_t *, void *);

/* ethernet  specific */
static int  enet_init (int);
static int  enet_load (ml_info_t *);
static int  enet_unload (cfg_desc_t *);
static int  enet_reg (ml_info_t *);
static int  enet_unreg (cfg_desc_t *);
static int  enet_makedata(cfg_desc_t *);
static void enet_freedata(cfg_desc_t *);
static int  enet_id(cfg_desc_t *);
static int  enet_lock(cfg_desc_t *);
static void enet_unlock(cfg_desc_t *, int);
static void enet_copydata(cfg_desc_t *, void *);

typedef struct mloadops {
    int 	(*ml_init)(int);
    int 	(*ml_load)(ml_info_t *);
    int 	(*ml_unload)(cfg_desc_t *);
    int 	(*ml_reg)(ml_info_t *);
    int 	(*ml_unreg)(cfg_desc_t *);
    int 	(*ml_makedata)(cfg_desc_t *);
    void 	(*ml_freedata)(cfg_desc_t *);
    int 	(*ml_id)(cfg_desc_t *);
    int 	(*ml_lock)(cfg_desc_t *);
    void 	(*ml_unlock)(cfg_desc_t *, int);
    void 	(*ml_copydata)(cfg_desc_t *, void *);
} mloadops_t;

static mloadops_t drv_ops = {drv_init, drv_load, drv_unload, drv_reg, drv_unreg,
    drv_makedata, drv_freedata, drv_id, drv_lock, drv_unlock, drv_copydata};
static mloadops_t str_ops = {str_init, str_load, str_unload, str_reg, str_unreg,
    str_makedata, str_freedata, str_id, str_lock, str_unlock, str_copydata};
static mloadops_t idbg_ops = {idbg_init, idbg_load, idbg_unload, idbg_reg,
    idbg_unreg, idbg_makedata, idbg_freedata, idbg_id, idbg_lock, idbg_unlock,
    idbg_copydata};
static mloadops_t lib_ops = {lib_init, lib_load, lib_unload, lib_reg, lib_unreg,
    lib_makedata, lib_freedata, lib_id, lib_lock, lib_unlock, lib_copydata};
static mloadops_t symtab_ops = {symtab_init, symtab_load, symtab_unload, symtab_reg, symtab_unreg,
    symtab_makedata, symtab_freedata, symtab_id, symtab_lock, symtab_unlock, symtab_copydata};
static mloadops_t fsys_ops = {fsys_init, fsys_load, fsys_unload, fsys_reg, 
    fsys_unreg, fsys_makedata, fsys_freedata, fsys_id, fsys_lock,
    fsys_unlock, fsys_copydata};
static mloadops_t enet_ops = {enet_init, enet_load, enet_unload, enet_reg,
    enet_unreg, enet_makedata, enet_freedata, enet_id, enet_lock, enet_unlock,
    enet_copydata};


/* This array is indexed by module type: M_DRIVER = 1, M_STREAMS = 2, etc. */

static void free_edt (medt_t *);

static mloadops_t *mlops[] = {NULL, &drv_ops, &str_ops, &idbg_ops, \
	&lib_ops, &fsys_ops, &enet_ops, &symtab_ops};
static int mltypes = sizeof(mlops)/sizeof(mlops[0]) - 1;

#define ML_LOAD(ml)	(*mlops[ml->ml_desc->m_type]->ml_load)(ml)
#define ML_UNLOAD(desc)	(*mlops[desc->m_type]->ml_unload)(desc)
#define ML_REG(ml)	(*mlops[ml->ml_desc->m_type]->ml_reg)(ml)
#define ML_UNREG(desc)	(*mlops[desc->m_type]->ml_unreg)(desc)
#define ML_MAKEDATA(desc)	(*mlops[desc->m_type]->ml_makedata)(desc)
#define ML_FREEDATA(desc)	(*mlops[desc->m_type]->ml_freedata)(desc)
#define ML_ID(ml)	(*mlops[ml->ml_desc->m_type]->ml_id)(ml->ml_desc)
#define ML_LOCK(ml)	(*mlops[ml->ml_desc->m_type]->ml_lock)(ml->ml_desc)
#define ML_UNLOCK(ml,sp)	\
	(*mlops[ml->ml_desc->m_type]->ml_unlock)(ml->ml_desc,sp)
#define ML_COPYDATA(desc,ptr)	(*mlops[desc->m_type]->ml_copydata)(desc, ptr)

/* symbol table, relocation */
static int  mloadfile (ml_info_t *);
static void munloadfile (ml_info_t *);
static int  mreadfile (ml_info_t *, int);
static int  makeminfo (cfg_desc_t *, ml_info_t **); 
static void freeminfo (ml_info_t *);
static int  mreloc (ml_info_t *);
static int  mfindname (ml_info_t *, char *, long *);
static int  mfindexactname(ml_info_t *, char *, long *);
static int  mversion_chk (ml_info_t *);

/* ELF */
extern int  mreadelf (ml_info_t *, vnode_t *);
extern int  mreadelf32 (ml_info_t *, vnode_t *);
extern int  mreadelf64 (ml_info_t *, vnode_t *);
extern void mfreeelfobj (ml_info_t *);
extern int  doelfrelocs (ml_info_t *);
extern int  msetelfsym (ml_info_t *, int);
extern int melf_findaddr(char *, __psunsigned_t **, int, sema_t *);
extern char *melf_findname(__psunsigned_t *, int, sema_t *);
extern int melf_findexactname(ml_info_t *, char *, long *);
extern int mreadelfsymtab (ml_info_t *, vnode_t *);

/* edt routines */
static int makeedtdata(medt_t *, medt_t *);
static int medtinit (ml_info_t *, medt_t *);

/* autounload routines */
static void mtimerstart (cfg_desc_t *, int);
static void mtimerstop (cfg_desc_t *);
void mautounload (int *);
static void munldd(void);		/* autounload daemon */
static sema_t munlddwake;

/* run-time symbol table stuff */
extern char arg_kernname[];
static int mload_rtsyms(ml_info_t **);
static ml_info_t *rtsymmod = 0;		/* ptr to rtsymtab module */
static char *kernname = NULL;
static int unixsymtab_avail = 0;	/* unix symtab available for autoload */
static int mlunixsymtab = 0;		/* unix symtab loaded via ml */
static int check_rtsyms(ml_info_t *);

/* diskless tag */
extern int diskless;			/* from os/startup.c */

/* misc debugging */
void idbg_maddr(__psint_t *, char *);
void idbg_mname(__psunsigned_t *, char *);
int idbg_melfname(ml_info_t *, __psunsigned_t *, char *);
int idbg_melfaddr(ml_info_t *, __psint_t *, char *);

ml_info_t *mlinfolist = 0;		/* head of ml_info structs - one for 
					   each loaded module. */
sema_t mlinfosem;

/*
 * TODO:
 *
 * - error handling 
 *	- what if a the same symbol name gets loaded > once ???
 *	- what if a module gets loaded twice with 2 diff major #s ?
 * - lboot 
 *	- make cdevsw/bdevsw arrays of pointers for faster searching
 * - edt
 *	- handle other types of e_bus_info
 * - need a kern_malloc routine that will only return k0
 * - implement capability to load drivers which reference each other
 * - unload
 * 	- enhance unload to unload a module that other loaded modules might
 *	have linked with
 *	- unload from kernel structs
 */

/*
 * sgi_mconfig - load, unload, register or unregister a driver, file system, 
 * stream module, etc.
 */
int
sgi_mconfig (int cfg_cmd, void *desc, int len, rval_t *rvp)
{
	int error = 0;

	if ((cfg_cmd == CF_LOAD || cfg_cmd == CF_UNLOAD ||
	    cfg_cmd == CF_REGISTER || cfg_cmd == CF_UNREGISTER) &&
	    !_CAP_ABLE(CAP_DEVICE_MGT))
		return EPERM;

	switch (cfg_cmd) {
		case CF_LOAD:
			if (mloaddebug != 0xdead)
				error = mload(desc);
			else
				error = MERR_LOADOFF;
			break;
		case CF_UNLOAD:
			if (mloaddebug != 0xdead)
				error = munload((long)desc, MMANUNLOAD);
			else
				error = MERR_LOADOFF;
			break;
		case CF_REGISTER:
			if (mloaddebug != 0xdead)
				error = mregister(desc);
			else
				error = MERR_LOADOFF;
			break;
		case CF_UNREGISTER:
			if (mloaddebug != 0xdead)
				error = munregister((long)desc);
			else
				error = MERR_LOADOFF;
			break;
		case CF_LIST:
			error = mlist(desc, len, rvp);
			break;
		case CF_DEBUG:
			mloaddebug = (__psint_t) desc;
			break;
		default:
			error = MERR_UNKNOWN_CFCMD;
			break;
	}

	return (error);
}

int
sgi_symtab (int sym_cmd, void *uname, void *uaddr)
{
    int error = 0;
    size_t count;
    char *name;
    __psunsigned_t *address;

    switch (sym_cmd) {
	case SYM_ADDR:
	case SYM_ADDR_ALL:
	    name = kern_malloc(MAXSYMNAME);
#if _MIPS_SIM == _ABI64
	    if (!ABI_IS_IRIX5_64(get_current_abi())) {
		error = MERR_UNSUPPORTED;
		goto addr_done;
	    } else
#endif
	    if (error = copyinstr(uname, name, MAXSYMNAME, &count))
		    goto addr_done;

	    if ((error = st_findaddr(name, &address)) != 0)
		    goto addr_done;

	    if (copyout(&address, uaddr, sizeof address))
		error = MERR_SYMEFAULT;

addr_done: 
	    kern_free (name);
	    return error;

	case SYM_NAME:
	case SYM_NAME_ALL:
#if _MIPS_SIM == _ABI64
	    if (!ABI_IS_IRIX5_64(get_current_abi())) {
		error = MERR_UNSUPPORTED;
		goto name_done;
	    } else
#endif
	    if (copyin(uaddr, &address, sizeof address)) {
		error = MERR_SYMEFAULT;
		goto name_done;
	    }

	    if ((error = st_findname(address, &name)) != 0)
		    goto name_done;

	    if (strlen(name)+1 > MAXSYMNAME)
		    error = MERR_ENAMETOOLONG;
	    else if (copyout(name, uname, strlen(name)+1))
		    error = MERR_COPY;

name_done: 
	    return error;

	default:
	    return MERR_UNKNOWN_SYMCMD;
    }
}

/*
 * st_findaddr()
 *
 * Attempt to find name in available symbol table modules (saving matching
 * symbol addr in paddr if found).
 *
 * If existing symtabs don't have it, then try autoloading kernel rtsymtab
 * if it's not already loaded and we're allowed to load it.
 *
 * Return 0 on sucess, MERR_ value on failure.
 */
int
st_findaddr(char *name, __psunsigned_t **paddr)
{
	extern int mload_auto_rtsyms;
	int rc = 0;
	
	/* search list of available runtime symtabs */
	rc = melf_findaddr(name, paddr, M_SYMTAB, &mlinfosem);
	if (1 == rc) rc = MERR_FINDADDR;	/* fixup return code */
	if (rc == MERR_FINDADDR && rtsymmod == 0 && mload_auto_rtsyms == 1) {
		/* autoload kernel rtsymtab w/timeout and re-search */
		if ((rc = mload_rtsyms(&rtsymmod)) == 0) {
			rc = melf_findaddr(name, paddr, M_SYMTAB, &mlinfosem);
			if (rc == 1) rc = MERR_FINDADDR;	/* fixup again */
		}
	}

	return (rc);
}

/*
 * st_findname()
 *
 * Attempt to find addr in available symbol table modules (saving matching
 * symbol name if found).
 *
 * If existing rtsymtabs don't have it, then try autoloading kernel rtsymtab
 * if it's not already loaded and we're allowed to load it.
 *
 * Return 0 on success, MERR_ value on failure.
 */
int
st_findname(__psunsigned_t *addr, char **name)
{
	extern int mload_auto_rtsyms;
	int rc = 0;
	char *namep;

	/* search list of available runtime symtabs */
	namep = melf_findname(addr, 0, &mlinfosem);
	if (namep == NULL && rtsymmod == 0 && mload_auto_rtsyms == 1) {
		/* autoload kernel rtsymtab w/timeout and research */
		if ((rc = mload_rtsyms(&rtsymmod)) == 0) {
			namep = melf_findname(addr, 0, &mlinfosem);
		}
	}
	if (namep == NULL) rc = MERR_FINDNAME;
	*name = namep;
	return (rc);
}

/*
 * Initialize mload variables
 */
void
mload_init(void)
{
	int idbase = 0;
	int type;
	extern int munldd_pri;

	for (type = 1; type <= mltypes; ++type) {
		idbase += (*mlops[type]->ml_init)(idbase);
		/* round up to 1000 */
		idbase = ((idbase + 999) / 1000) * 1000;	
	}

	initnsema(&mlinfosem, 1, "mlinfo");
	initnsema(&munlddwake, 0, "mlunldd");

	/*
	 * Set up kernel name for runtime symbol table.
	 *
	 * Autoload works only if sash has setup the kernel name
	 * for us.  We could default to /unix, but that might
	 * lead to a crash if we've booted something different.
	 *
	 * Different machines produce different types of boot strings,
	 * including:
	 *
	 *	autoboot:
	 *
	 *	IP20:	scsi(0)disk(1)rdisk(0)partition(0)/unix
	 *	IP19:	dksc(1,2,0)unix
	 *	IP26:	scsi(0)disk(1)rdisk(0)partition(0)//unix
	 *
	 * Note that when booting the kernel directly from the sash
	 * prompt, the unix file name is *not* setup as a fully
	 * qualified path.
	 */

	if (diskless == 1) {
		kernname = "/unix";
		unixsymtab_avail = 1;
	} else if (arg_kernname[0]) {
	    if (strncmp(arg_kernname, "bootp()", strlen("bootp")) != 0) {
		if (kernname = strstr(arg_kernname, "/unix"))
			unixsymtab_avail = 1;
		else if (kernname = strstr(arg_kernname, "//unix"))
			unixsymtab_avail = 1;
		else if (kernname = strstr(arg_kernname, ")unix")) {
			/*
			 * PV # 520546
			 * Since kernname is really pointing to the
			 * actual array arg_kernname, this was
 			 * overwriting the original kern var.
			 * Assuming arg_kernname - 100 can accomodate
			 * the extra '/'.
			 */
                        char *tmpkernname = kern_malloc (strlen(kernname) + 2);
			kernname++ ;
                        strcpy (tmpkernname, "/");
                        strcat (tmpkernname, kernname);
                        strcpy (kernname, tmpkernname);
                        kern_free (tmpkernname);
			unixsymtab_avail = 1;
		} else if (kernname = strstr(arg_kernname, "unix")) {
			char *tmpkernname = kern_malloc (strlen(kernname) + 2);
			strcpy (tmpkernname, "/");
			strcat (tmpkernname, kernname); 
			strcpy (kernname, tmpkernname);
			kern_free (tmpkernname);
			unixsymtab_avail = 1;
		} else {
			cmn_err_tag(294,CE_WARN, "Cannot load runtime symbol table from kernname %s. Loadable modules will not be registered or loaded.", arg_kernname);
		}
	    } else
		cmn_err_tag (295,CE_WARN, "Cannot load runtime symbol table from bootp'ed kernel.\n         Loadable modules will not be registered or loaded.");
	} else
		cmn_err_tag (134,CE_WARN, "Kernname environment variable not set by sash. Runtime symbol table not loaded. Loadable modules will not be registered or loaded.");

	sthread_create("munldd", 0, 512 * sizeof (void *), 0, munldd_pri, KT_PS,
				(st_func_t *)munldd, 0, 0, 0, 0);
}

/*
 * mload - load a driver, file system, streams module, etc.
 */
static int
mload (cfg_desc_t *udesc) 
{
	int error;
	ml_info_t *minfo;

#ifdef	MTRACE
	printf ("mload\n");
#endif	/* MTRACE */

	if (error = makeminfo (udesc, &minfo))
		return error;

	/*
	 * If there isn't a unix symtab manually loaded or if there isn't a 
	 * runtime symbol table available to be autoloaded, for example, 
	 * we bootp'ed a kernel and can't then load a symbol table from it, 
	 * then don't allow any modules to be loaded, unless its a SYMTAB 
	 * type module.
	 */

	if ((minfo->ml_desc->m_type != M_SYMTAB) &&
		(!unixsymtab_avail && !mlunixsymtab)) {
		freeminfo (minfo);
		return MERR_NOSYMTABAVAIL;
	}

	error = ML_LOAD(minfo);

	if (error) {
		freeminfo (minfo);
		return error;
	}

	initnsema_mutex(&minfo->ml_desc->m_transsema, "m_trans");
	minfo->ml_desc->m_id = ML_ID(minfo);
	udesc->m_id = minfo->ml_desc->m_id;		/* user info */
	minfo->ml_desc->m_flags &= ~M_TRANSITION;
	minfo->ml_desc->m_flags |= M_LOADED;

	return 0;
}

/*
 * munload - unload a driver, file system, streams module, etc.
 */
static int
munload (long id, int unloadtype)
{
	ml_info_t *m;
	int error = 0;
	cfg_desc_t *desc;
	int sp;

	psema(&mlinfosem, PZERO);

	if (unloadtype == MAUTOUNLOAD) {
		/* search for entry marked by munldd/still safe to unload */
		for (m = mlinfolist; m; m = m->ml_next) {
			if ((m->ml_desc->m_tflags & M_EXPIRED) &&
			    !(m->ml_desc->m_flags & M_NOAUTOUNLOAD))
				break;
		}

		if (m) {
			sp = ML_LOCK(m);
			m->ml_desc->m_tflags &= ~M_EXPIRED;
			m->ml_desc->m_flags &= ~M_NOAUTOUNLOAD;
		}

		/* should never reach this!!! */
		if (!m) {
			vsema(&mlinfosem);
			return MERR_BADID;	/* ignoring this anyway */
		}
	} else {
		/* search for module by id */
		for (m = mlinfolist; m && (m->ml_desc->m_id != id);
		     m = m->ml_next)
			;	/* empty */

		if (!m) {
			vsema(&mlinfosem);
			return MERR_BADID;
		}
		sp = ML_LOCK(m);
	}

	desc = m->ml_desc;
	if (desc->m_timeoutid) {
		desc->m_flags |= M_NOAUTOUNLOAD;
		untimeout (desc->m_timeoutid);
		desc->m_timeoutid = 0;

	}
	if (desc->m_refcnt || (desc->m_flags & M_TRANSITION))
		error = EBUSY;
	if (!(desc->m_flags & M_LOADED))
		error = MERR_NOTLOADED;

	if (error) {
		ML_UNLOCK(m, sp);
		vsema(&mlinfosem);
		return error;
	}

	desc->m_flags |= (M_UNLOADING|M_TRANSITION);
	ML_UNLOCK(m, sp);
	vsema(&mlinfosem);

	/*
	 * Grab the transition sema4 and check that the refcount is
	 * still zero, in case someone beat us to it and is using the
	 * driver.  
	 */
	psema(&desc->m_transsema, PZERO);
	if (desc->m_refcnt) {
		sp = ML_LOCK(m);
		desc->m_flags &= ~(M_TRANSITION | M_UNLOADING);
		ML_UNLOCK(m, sp);
		vsema(&desc->m_transsema);
		error = EBUSY;
		return error;
	}
	error = ML_UNLOAD(desc);
	vsema(&desc->m_transsema);

	/* Unload function failed for some reason.  Leave module loaded. */
	if (error) {
		sp = ML_LOCK(m);
		desc->m_flags &= ~(M_UNLOADING|M_TRANSITION);
		ML_UNLOCK(m, sp);
		return error;
	}

	/* 
	 * If the module is not registered, then free all allocated
	 * data and remove the minfo struct. If the module is registered,
	 * leave the minfo struct in the list, but free up allocated
	 * space for ml_text, ml_symtab and ml_stringtab, which get 
	 * reloaded.
	 */
	if (!(desc->m_flags & M_REGISTERED)) {
		freesema(&desc->m_transsema);
		freeminfo (m);
	} else {
		sp = ML_LOCK(m);
		if (m->ml_base) {
			kern_free (m->ml_base);
			m->ml_base = 0;
			m->ml_text = 0;
		}
		if (m->ml_symtab) {
			kern_free (m->ml_symtab);
			m->ml_symtab = 0;
		}
		if (m->ml_stringtab) {
			kern_free (m->ml_stringtab);
			m->ml_stringtab = 0;
		}
		m->ml_end = 0;
		desc->m_flags &= ~(M_LOADED|M_TRANSITION|M_UNLOADING);
		ML_UNLOCK(m, sp);
	}

	return 0;
}

/*
 * mregister - register a driver, file system, streams module, etc.
 */
static int
mregister (cfg_desc_t *udesc)
{
	int error;
	ml_info_t *minfo;
	
	if (error = makeminfo (udesc, &minfo))
		return error;

	/*
	 * If there isn't a unix symtab manually loaded or if there isn't a 
	 * runtime symbol table available to be autoloaded, for example, 
	 * we bootp'ed a kernel and can't then load a symbol table from it, 
	 * then don't allow any modules to be registered, unless its a SYMTAB 
	 * type module.
	 */

	if ((minfo->ml_desc->m_type != M_SYMTAB) &&
		(!unixsymtab_avail && !mlunixsymtab)) {
		freeminfo (minfo);
		return MERR_NOSYMTABAVAIL;
	}

	error = ML_REG(minfo);

	if (error) { 
		freeminfo (minfo);
		return error;
	}

	initnsema_mutex(&minfo->ml_desc->m_transsema, "m_trans");
	minfo->ml_desc->m_id = ML_ID(minfo);
	udesc->m_id = minfo->ml_desc->m_id;		/* user info */
	minfo->ml_desc->m_flags &= ~M_TRANSITION;
	minfo->ml_desc->m_flags |= M_REGISTERED;

	return 0;
}

/*
 * munregister - unregister a driver, file system, streams module, etc.
 */
static int
munregister (long id)
{
	ml_info_t *m;
	int error = 0;
	cfg_desc_t *desc;
	int sp;

	psema(&mlinfosem, PZERO);
	for (m = mlinfolist; m && (m->ml_desc->m_id != id); m = m->ml_next)
		;	/* empty */

	if (!m) {
		vsema(&mlinfosem);
		return MERR_BADID;
	}

	sp = ML_LOCK(m);
	desc = m->ml_desc;
	if (desc->m_refcnt || (desc->m_flags & (M_TRANSITION|M_LOADED)))
		error = EBUSY;
	if (!(desc->m_flags & M_REGISTERED))
		error = MERR_NOTREGED;

	if (error) {
		ML_UNLOCK(m, sp);
		vsema(&mlinfosem);
		return error;
	}

	desc->m_flags |= M_TRANSITION;
	ML_UNLOCK(m, sp);
	vsema(&mlinfosem);

	error = ML_UNREG(desc);

	/* free all allocated data */
	if (error)
		desc->m_flags &= ~M_TRANSITION;
	else {
		freesema(&desc->m_transsema);
		freeminfo (m);
	}
	return error;
}

/*
 * mlist - copy out info about loaded or registered modules
 */
static int
mlist (void *info, int len, rval_t *rvp)
{
	ml_info_t *m;
	mod_info_t *kmodp, *mod = (mod_info_t *) info;
	cfg_desc_t *cfg;
	int count = 0;

	/* If we don't get a pointer, just compute the amount of memory
	 * necessary for the data
	 */
	if (!info) {

		psema(&mlinfosem, PZERO);
		m = mlinfolist;
		while (m) {
		    count += sizeof(mod_info_t);
		    m = m->ml_next;
		}
		vsema(&mlinfosem);

		rvp->r_val1 = count;
		return 0;
	}

	kmodp = kern_malloc(sizeof *kmodp);
	count = len;

	psema(&mlinfosem, PZERO);
	m = mlinfolist;

	while (m && count >= sizeof(mod_info_t)) {

		cfg = m->ml_desc;

		strncpy (kmodp->m_fname, cfg->m_fname, sizeof(kmodp->m_fname));
		strncpy (kmodp->m_prefix, cfg->m_prefix, sizeof(kmodp->m_prefix));
		if (cfg->m_delay == M_UNLDDEFAULT)
			kmodp->m_delay = module_unld_delay;
		else
			kmodp->m_delay = cfg->m_delay;
		kmodp->m_flags = cfg->m_flags;
		kmodp->m_type = cfg->m_type;
		kmodp->m_id = cfg->m_id;
		kmodp->m_cfg_version = cfg->m_cfg_version;

		ML_COPYDATA(cfg, &kmodp->m_dep);

		if (copyout(kmodp, mod, sizeof(mod_info_t))) {
			vsema(&mlinfosem);
			kern_free(kmodp);
			return MERR_COPY;
		}
		count -= sizeof(mod_info_t);
		mod++;

		m = m->ml_next;
	}
	vsema(&mlinfosem);
	kern_free(kmodp);

	rvp->r_val1 = len - count;
	return 0;
}

/*
 * BEGIN driver support
 */

static int  driver_check (ml_info_t *);
static int  driver_hook (ml_info_t *);
static int  driver_init (ml_info_t *);
static void driver_free (cfg_desc_t *);
static void driver_regstub(cfg_desc_t *);

static int  drvstub_open(dev_t *, int, int, cred_t *);
static int  drvstub_close(dev_t, int, int, cred_t *);
static int  drvstub_attach(vertex_hdl_t);
static int  drvstub_detach(vertex_hdl_t);
static int  drvstub_stropen(queue_t *, dev_t *, int, int, cred_t *);
static int  drvstub_strclose(queue_t *, int, cred_t *);
static int  drvstub_strload(queue_t *, dev_t *, int, int, cred_t *);

int	    mload_driver_load(cfg_desc_t *desc);
void	    mload_driver_hold(cfg_desc_t *desc);
void	    mload_driver_release(cfg_desc_t *desc);

#define DRVTBSIZE	32	/* must be power of 2 */
static cfg_desc_t *drvtab[DRVTBSIZE];

#define DRVTBHASHDEVSW(devsw) ((unsigned long)(devsw)>>3 & (DRVTBSIZE - 1))

#define DRVTBHASHD(desc) \
	(((cfg_driver_t *)(desc)->m_data)->d_cdevsw != NULL) ? \
		(DRVTBHASHDEVSW((((cfg_driver_t *)(desc)->m_data))->d_cdevsw)) : \
		(DRVTBHASHDEVSW((((cfg_driver_t *)(desc)->m_data))->d_bdevsw))

static int cdevswstart, bdevswstart;
static int majorstart, majormax, majorinvalid;
static mutex_t majorsem;
static int drvidbase;	/* unique id base */

static lock_t drvlock;

static int drvstub_flag = D_MP;
static struct module_info drvstub_minfo = 
    {STRID_DRVLOAD, "DRVLOAD", 0, INFPSZ, 128, 16 };
static struct qinit drvstub_rinit = 
    {NULL, NULL, drvstub_strload, NULL, NULL, &drvstub_minfo, NULL};
static struct qinit drvstub_winit = 
    {NULL, NULL, NULL, NULL, NULL, &drvstub_minfo, NULL};
static struct streamtab drvstub_strinfo = 
    {&drvstub_rinit, &drvstub_winit, NULL, NULL};


#define DRVMAXDRIVERS 256
static cfg_desc_t *drv_id_inuse[DRVMAXDRIVERS];


/* A zero flags field indicates an empty devsw entry
 */
#define bdev_empty(m) (bdevsw[m].d_flags == 0)
#define cdev_empty(m) (cdevsw[m].d_flags == 0)

/*
 * drv_init - initialize private data for loaded drivers
 *	base is its first allowable id number for modules
 *	returns number of unique ids it will use
 *
 * N.B.  This must run after vfsinit() because it assumes getudev()
 * 	 will return the first valid major number after the dummy
 *	 major numbers have been allocated for file systems.
 */
static int
drv_init(int base)
{
#ifdef	MTRACE
	printf ("drv_init\n");
#endif	/* MTRACE */

	cdevswstart = cdevcnt;
	bdevswstart = bdevcnt;
	majorstart = getudev();

	majormax = L_MAXMAJ;		/* from sysmacros.h */
	majorinvalid = MAJOR_DONTCARE; 	/* value provide by lboot */

	/* protection for major array */
	mutex_init(&majorsem, MUTEX_DEFAULT, "mlmajor");
	spinlock_init (&drvlock, "mldriver");

	ASSERT (bdevmax <= majorinvalid);
	ASSERT (cdevmax <= majorinvalid);

	drvidbase = base;
	return DRVMAXDRIVERS;
}


static int
drv_alloc_id(cfg_desc_t *desc)
{
	int i;

	ASSERT(desc != NULL);
	for (i=0; i<DRVMAXDRIVERS; i++)
		if (drv_id_inuse[i] == NULL) {
			drv_id_inuse[i] = desc;
			return(i);
		}

	return(-1);
}


static void
drv_free_id(int id)
{
	ASSERT((0 <= id) && (id < DRVMAXDRIVERS));
	ASSERT(drv_id_inuse[id]);
	drv_id_inuse[id] = NULL;
}


/*
 * drv_id - return id for the module.
 */
static int
drv_id (cfg_desc_t *desc)
{
	cfg_driver_t *drv = (cfg_driver_t *)desc->m_data;
	return drvidbase + drv->d_id;
}


static void
dinsert(cfg_desc_t *desc)
{
	u_int hash = DRVTBHASHD(desc);
#ifdef DEBUG
	cfg_desc_t *tmp;

	/* This desc should not already be in the list, but check anyway. */
	tmp = drvtab[hash];
	while (tmp) {
		ASSERT (desc != tmp);
		tmp = tmp->m_next;
	}
#endif
	desc->m_next = drvtab[hash];
	drvtab[hash] = desc;
}

static void
ddelete(cfg_desc_t *desc)
{
	cfg_desc_t *d, *prev = 0;
	u_int hash = DRVTBHASHD(desc);

	d = drvtab[hash];
	while (d) {
		if (d == desc) {
			if (!prev)
				drvtab[hash] = d->m_next;
			else
				prev->m_next = d->m_next;
			break;
		}
		prev = d;
		d = d->m_next;
	}
}

#if NOTDEF
static cfg_desc_t *
dfind_devsw(devsw_t *devsw)
{
	cfg_desc_t *desc;

	ASSERT(devsw != NULL);

	desc = drvtab[DRVTBHASHDEVSW(devsw)];
	while (desc) {
		cfg_driver_t *drv = desc->m_data;
		if ((((devsw_t *)drv->d_cdevsw == devsw)) ||
		    ((devsw_t *)drv->d_bdevsw == devsw))
			return desc;

		desc = desc->m_next;
	}
	return 0;
}
#endif /* NOTDEF */


/* This is ugly and only works for a *loaded* streams driver */
static cfg_desc_t *
dfind_minfo(void *minfo)
{
	cfg_desc_t *desc;
	struct cdevsw *my_cdevsw;
	int i;

	for (i=0; i<DRVTBSIZE; i++) {
		for (desc = drvtab[i]; desc; desc = desc->m_next) {
			cfg_driver_t *drv = desc->m_data;
			if ((my_cdevsw=drv->d_cdevsw) == NULL)
				continue;

			if (my_cdevsw->d_str == NULL)
				continue;

			if (my_cdevsw->d_str->st_rdinit->qi_minfo ==
				(struct module_info *)minfo) {
					return desc;
			}
		}
	}
	return 0;
}


/* ARGSUSED1 */
static int
drv_lock (cfg_desc_t *desc)
{
	return mp_mutex_spinlock(&drvlock);
}

/* ARGSUSED1 */
static void
drv_unlock (cfg_desc_t *desc, int sp)
{
	mp_mutex_spinunlock(&drvlock, sp);
}

static void 
drv_copydata(cfg_desc_t *desc, void *ptr)
{
	bcopy(desc->m_data, ptr, sizeof(mod_driver_t));
	((mod_driver_t *)ptr)->d_edtp = 0;
}

static void
drv_freedata(cfg_desc_t *desc)
{
	cfg_driver_t *drv = desc->m_data;

	if (drv) {
		free_edt(drv->d_edt);
		drv_free_id(drv->d_id);
		kern_free (drv);
		desc->m_data = 0;
	}
}

/* 
 * drv_makedata - create the data structure specific to drivers
 * 	this could also be used to validate any driver specific parameters
 */
static int
drv_makedata(cfg_desc_t *desc)
{
	int error;
	cfg_driver_t *drv;
	medt_t *medtpfrom, *medtpto;

	/* malloc m_data for driver and copyin */
	drv = kern_calloc(1, sizeof(cfg_driver_t));
	if (copyin(desc->m_data, drv, sizeof(mod_driver_t))) {
		kern_free(drv);
		return MERR_COPY;
	}
	if (medtpfrom = drv->d_edt) {
		medtpto = kern_calloc(1, sizeof(medt_t));
		if (error = makeedtdata (medtpfrom, medtpto)) {
			free_edt(medtpto);
			kern_free(drv);
			return error;
		}
		drv->d_edt = medtpto;
	}

	if ((drv->d_id = drv_alloc_id(desc)) < 0) {

		kern_free(drv->d_edt);
		kern_free(drv);
		return(MERR_NOMAJORS);
	}

	drv->d_id += drvidbase;

	desc->m_data = drv;
	return 0;
}


#define DRV_LOAD	0x1
#define DRV_REGISTER	0x2

/*
 * drv_do_load_or_reg - load and/or register a driver
 *
 * Bring in driver text.  Call init and register routines.  
 * If this was only supposed to be a REGISTER, then push out the text
 * and set up a stub to load-on-demand.
 */
static int
drv_do_load_or_reg(ml_info_t *minfo, int action)
{
	int (*reg)(void);
	int error = 0;
	cfg_desc_t *desc = minfo->ml_desc;
	cfg_driver_t *drv = (cfg_driver_t *) minfo->ml_desc->m_data;
	int driver_loaded = 0; /* we loaded & linked driver */
	int driver_initted = 0; /* we initialized driver */

#ifdef	MTRACE
	printf ("drv_load\n");
#endif	/* MTRACE */

	mutex_lock(&majorsem, PZERO);

	/* do sanity checks */
	if (error = driver_check (minfo))
		goto out;

	/* read the .o file, create a symbol table, and relocate it */
	if (error = mloadfile (minfo))
		goto out;

	driver_loaded = 1;

	if (mfindname (minfo, "reg", (long *)&reg))
		reg = NULL;

	/* set up a bdevsw/cdevsw for this driver */
	if (error = driver_hook(minfo)) {
		munloadfile(minfo);
		goto out;
	}

	/* TODO - will we need to keep reloctab around for unloading modules 
	 *	that have linked with other loaded modules ??? */

	if ((action == DRV_LOAD) || reg) {
		/* init kernel driver structs and initialize driver */
		if (error = driver_init (minfo)) {
			driver_free(desc);
			munloadfile(minfo);
			goto out;
		}

		driver_initted = 1;
	}

	if (reg) {
		int regerr;
		if (regerr = (*reg)())
			cmn_err_tag(296,CE_WARN, "Registration failed (%d): %s\n", regerr, desc->m_fname);
	}

	/* If we were loaded but we weren't really supposed to be, try to unload */
	if ((action != DRV_LOAD) && driver_loaded) {

		if (driver_initted) {
			int (*unload)(void) = drv->d_unload;

			if (unload) {
				ASSERT(IS_KSEG0(unload) || IS_KSEG2(unload));
				error = (*unload)();
				if (!error)
					driver_initted = 0;
			}
			ddelete(desc);
		}

		if (!driver_initted) {
			munloadfile(minfo);
			driver_loaded = 0;
		}
	}

	if (!driver_loaded && (action == DRV_REGISTER)) {
		/* connect a stub to the devsw to load the driver on open */
		driver_regstub (desc);
	}

out:
	mutex_unlock(&majorsem);
	return error;
}

/*
 * drv_load - dynamically load a driver
 */
static int
drv_load (ml_info_t *minfo)
{
	return(drv_do_load_or_reg(minfo, DRV_LOAD));
}


/*
 * Fill in drv->d_bdevsw with an appropriate descriptor.
 * On failure, returns 0.
 * On success, returns major# (if alloc'ed from table) or 1 if generic alloc.
 */
static int
alloc_bdevsw(cfg_driver_t *drv)
{
	int bdev;

	if (drv->d.d_nmajors > 0) {
		for (bdev = bdevswstart; bdev < bdevmax; bdev++) {
			if (bdev_empty(bdev)) {
				drv->d_bdevsw = &bdevsw[bdev];
				return bdev;
			}
		}
		return(0);
	} else {
		drv->d_bdevsw = kern_calloc(1, sizeof(struct bdevsw));
		return(1);
	}

}

static void
free_bdevsw(cfg_driver_t *drv)
{
	struct bdevsw *my_bdevsw = drv->d_bdevsw;

	ASSERT(my_bdevsw != NULL);
#ifdef DEBUG
	bfill(my_bdevsw, sizeof(*my_bdevsw), 0xea);
#endif
	my_bdevsw->d_flags = 0;
	if (drv->d.d_nmajors == 0)
		kern_free(drv->d_bdevsw);
	drv->d_bdevsw = NULL;
}

/*
 * Fill in drv->d_cdevsw with an appropriate descriptor.
 * On failure, returns 0.
 * On success, returns major# (if alloc'ed from table) or 1 if generic alloc.
 */
static int
alloc_cdevsw(cfg_driver_t *drv)
{
	int cdev;

	if (drv->d.d_nmajors > 0) {
		for (cdev = cdevswstart; cdev < cdevmax; cdev++) {
			if (cdev_empty(cdev)) {
				drv->d_cdevsw = &cdevsw[cdev];
				return cdev;
			}
		}
		return(0);
	} else {
		drv->d_cdevsw = kern_calloc(1, sizeof(struct cdevsw));
		return(1);
	}

}

static void
free_cdevsw(cfg_driver_t *drv)
{
	struct cdevsw *my_cdevsw = drv->d_cdevsw;

	ASSERT(my_cdevsw != NULL);
#ifdef DEBUG
	bfill(my_cdevsw, sizeof(*my_cdevsw), 0xea);
#endif
	my_cdevsw->d_flags = 0;

	if (drv->d.d_nmajors == 0)
		kern_free(drv->d_cdevsw);
	drv->d_cdevsw = NULL;
}

/*
 * Fill in drv->d_cdevsw and drv->d_bdevsw with appropriate descriptors.
 * On failure, returns 0.
 * On success, returns major# (if alloc'ed from table) or 1 if generic alloc.
 */
static int
alloc_bcdevsw(cfg_driver_t *drv)
{
	int bcdev;
	int bcdevmax = MIN(cdevmax, bdevmax);
	int bcdevmin = MAX(cdevswstart, bdevswstart);

	if (drv->d.d_nmajors > 0) {
		for (bcdev = bcdevmin; bcdev < bcdevmax; bcdev++) {
			if (bdev_empty(bcdev) && cdev_empty(bcdev)) {
				drv->d_bdevsw = &bdevsw[bcdev];
				drv->d_cdevsw = &cdevsw[bcdev];
				return bcdev;
			}
		}
		return(0);
	} else {
		drv->d_bdevsw = kern_calloc(1, sizeof(struct bdevsw));
		drv->d_cdevsw = kern_calloc(1, sizeof(struct cdevsw));
		return(1);
	}

}

static void
free_bcdevsw(cfg_driver_t *drv)
{
	free_bdevsw(drv);
	free_cdevsw(drv);
}

static int
alloc_major(void)
{
	int m;

	for (m = majorstart; m < majormax; m++) {
		if (MAJOR[m] == majorinvalid)
			return m;
	}

	return 0;
}

static int
rsrv_major(int m)
{
	if (MAJOR[m] == majorinvalid)
		return m;

	return 0;
}

static void
free_major(cfg_driver_t *drv)
{
	int i;

	for (i=0; i<drv->d.d_nmajors; i++)
		MAJOR[drv->d.d_majors[i]] = majorinvalid;
}

/* 
 * driver_insert - Make the driver accessible through a device switch
 * entry.  For hwgraph-aware drivers, fill in the driver's devsw pointer
 * entries.  For older drivers, fill in entries in the cdevsw and bdevsw
 * tables for the internal major number corresponding to this driver.
 * Hold the spinlock and update entire entry at one time.
 */
static void
driver_insert(cfg_desc_t *desc, void *cdevswp, void *bdevswp)
{
	cfg_driver_t *drv = desc->m_data;
	int sp = mp_mutex_spinlock(&drvlock);
	struct cdevsw *my_cdevsw;
	struct bdevsw *my_bdevsw;

#ifdef	MTRACE
	printf ("driver_insert\n");
#endif	/* MTRACE */

	if (drv->d_typ & (MDRV_CHAR|MDRV_STREAM)) {
		my_cdevsw = drv->d_cdevsw;
		ASSERT(my_cdevsw != NULL);
		bcopy (cdevswp, my_cdevsw, sizeof(struct cdevsw));
		my_cdevsw->d_driver = desc->m_device_driver;

		/* Ensure requested CPU exists, if not goto bootmaster */
		if (!cpu_isvalid(my_cdevsw->d_cpulock))
			my_cdevsw->d_cpulock = masterpda->p_cpuid;
	}

	if (drv->d_typ & MDRV_BLOCK) {
		my_bdevsw = drv->d_bdevsw;
		ASSERT(my_bdevsw != NULL);
		bcopy (bdevswp, my_bdevsw, sizeof(struct bdevsw));
		my_bdevsw->d_driver = desc->m_device_driver;

		/* Ensure requested CPU exists, if not goto bootmaster */
		if (!cpu_isvalid(my_bdevsw->d_cpulock))
			my_bdevsw->d_cpulock = masterpda->p_cpuid;
	}

	/* 
	 * If the driver is registered, then its desc was already added
	 * to the hash tables, so don't add it again.
	 */
	if (!(desc->m_flags & M_REGISTERED))
		dinsert(desc);

	mp_mutex_spinunlock(&drvlock, sp);
}

/*
 * driver_check - sanity check before loading the driver
 */
static int
driver_check (ml_info_t *minfo)
{
	cfg_driver_t *drv;
	int i;
	char *prefix = minfo->ml_desc->m_prefix;

#ifdef	MTRACE
	printf ("driver_check\n");
#endif	/* MTRACE */

	drv = (cfg_driver_t *) minfo->ml_desc->m_data;

	/* The type currently has to be MDRV_CHAR, MDRV_STREAM, MDRV_BLOCK.
	 * Reject bogus combinations.
	 */
	drv->d_typ &= MDRV_CHAR|MDRV_STREAM|MDRV_BLOCK;
	if (drv->d_typ == (MDRV_CHAR|MDRV_STREAM|MDRV_BLOCK) ||
	    drv->d_typ == (MDRV_STREAM|MDRV_BLOCK) ||
	    drv->d_typ == (MDRV_STREAM|MDRV_CHAR) ||
	    !drv->d_typ)
		return MERR_ILLDRVTYPE;


	/*
	 * Verify that the driver's prefix isn't already in use.
	 */
	if (device_driver_get(prefix) != DEVICE_DRIVER_NONE)
			return MERR_MAJORUSED; /* use diff code? */

	for (i=0; i<drv->d.d_nmajors; i++) {
		if (drv->d.d_majors[i] == MAJOR_ANY) {
			if (!(drv->d.d_majors[i] = alloc_major())) {
				for (; i>=0; i--) {
					MAJOR[drv->d.d_majors[i]] = majorinvalid;
				}
				return MERR_NOMAJORS;
			}
		} else {
			/* sanity check major */
			if (drv->d.d_majors[i] > majormax)
				return MERR_ILLMAJOR;

			if (!rsrv_major(drv->d.d_majors[i]))
				return MERR_MAJORUSED;
		}
	}

	/* If the driver is loaded with the same major number, it will
	 * fail rsrv_major().  How do we handle multiple instances of
	 * a driver in the major table?
	 */

	return 0;
}


/*
 *	allocate major number/devsw entry (for drv->d.d_majors)
 */
static int
driver_hook(ml_info_t *minfo)
{
	cfg_driver_t *drv = (cfg_driver_t *) minfo->ml_desc->m_data;
	cfg_desc_t *desc = minfo->ml_desc;
	device_driver_t driver;
	int i, dev;
	int pri;

	/*
	 * Obtain bdevsw/cdevsw descriptors.  For hwgraph-aware drivers,
	 * these can be alloc'ed.  For older drivers, we find an unused 
	 * entry in the bdevsw/cdevsw table.
	 */
	if ((drv->d_typ & (MDRV_CHAR|MDRV_BLOCK)) == (MDRV_CHAR|MDRV_BLOCK)) {
		if (!(dev = alloc_bcdevsw(drv))) {
			free_major(drv);
			return MERR_SWTCHFULL;
		}
	} else if (drv->d_typ & (MDRV_CHAR|MDRV_STREAM)) {
		if (!(dev = alloc_cdevsw(drv))) {
			free_major(drv);
			return MERR_SWTCHFULL;
		}
	} else { 	/* if (drv->d_typ & MDRV_BLOCK) */
		if (!(dev = alloc_bdevsw(drv))) {
			free_major(drv);
			return MERR_SWTCHFULL;
		}
	}

	for (i=0; i<drv->d.d_nmajors; i++) {
		ASSERT(drv->d.d_majors[i] != MAJOR_ANY);
		ASSERT(MAJOR[drv->d.d_majors[i]] == majorinvalid);
		MAJOR[drv->d.d_majors[i]] = dev;
	}

	driver = device_driver_alloc(desc->m_prefix);
	desc->m_device_driver = driver;
	ASSERT(driver != DEVICE_DRIVER_NONE);

	pri = device_driver_sysgen_thread_pri_get(desc->m_prefix);
	device_driver_thread_pri_set(driver, pri);

	device_driver_devsw_put(driver, drv->d_bdevsw, drv->d_cdevsw);

	return 0;
}

/*
 * driver_init - hook into kernel data structures and initialize
 */
static int
driver_init (ml_info_t *minfo)
{
	struct cdevsw tmpcdev, *cdevswp;
	struct bdevsw tmpbdev, *bdevswp;
	void (*func)(void) = 0;
	cfg_driver_t *drv;
	cfg_desc_t *desc = minfo->ml_desc;
	int *devflag;
	int error;

#ifdef	MTRACE
	printf ("driver_init\n");
#endif	/* MTRACE */

	drv = desc->m_data;

	/* Check for version string. */
	if (error = mversion_chk (minfo))
		return error;

	/* All drivers contain a devflag, get it here.  Reject D_OLD
	 * type drivers.
	 */
	if (mfindname (minfo, "devflag", (long *)&devflag))
		return MERR_NODEVFLAG;
	if (*devflag & D_OBSOLD)
		return MERR_NODEVFLAG;

	/* Get the unload and unreg routines. */
	if (mfindname (minfo, "unload", (long *)&drv->d_unload))
		drv->d_unload = 0;
	if (mfindname (minfo, "unreg", (long *)&drv->d_unreg))
		drv->d_unreg = 0;
	if (mfindname (minfo, "reg", (long *)&drv->d_reg))
		drv->d_reg = 0;

	/* setup temp cdevsw */
	cdevswp = &tmpcdev;
	cdevswp->d_cpulock = drv->d_lck;
	cdevswp->d_flags = devflag;
	cdevswp->d_open = (int (*)(dev_t *, int, int, struct cred *))nodev;
	cdevswp->d_close = (int (*)(dev_t, int, int, struct cred *))nodev;
	cdevswp->d_read = (int (*)(dev_t, struct uio *, struct cred *))nodev;
	cdevswp->d_write = (int (*)(dev_t, struct uio *, struct cred *))nodev;
	cdevswp->d_ioctl = nodev;
	cdevswp->d_mmap = (int (*)(dev_t, off_t, int))nodev;
	cdevswp->d_map = nodev;
	cdevswp->d_unmap = nodev;
	cdevswp->d_poll = nulldev;
	cdevswp->d_attach = (int (*)(dev_t))nulldev;
	cdevswp->d_detach = (int (*)(dev_t))nulldev;
	cdevswp->d_enable = (int (*)(dev_t))nulldev;
	cdevswp->d_disable = (int (*)(dev_t))nulldev;
	cdevswp->d_ttys = 0;

	/* hook into cdevsw/bdevsw */
	if (drv->d_typ & MDRV_STREAM) {
		ASSERT(!(drv->d_typ & (MDRV_CHAR|MDRV_BLOCK)));

		if  (mfindname (minfo, "info", (long *)&cdevswp->d_str))
		    return MERR_NOINFO;

		drv->d_ropen = cdevswp->d_str->st_rdinit->qi_qopen;
		drv->d_rclose = cdevswp->d_str->st_rdinit->qi_qclose;
		cdevswp->d_str->st_rdinit->qi_qopen = drvstub_stropen;
		cdevswp->d_str->st_rdinit->qi_qclose = drvstub_strclose;

		mfindname (minfo, "attach", (long *)&cdevswp->d_attach);
		mfindname (minfo, "detach", (long *)&cdevswp->d_detach);

		cdevswp->d_desc = desc;
	}
	if (drv->d_typ & MDRV_CHAR) {
		ASSERT(!(drv->d_typ & MDRV_STREAM));

		/* Look for I/O routines and hook into cdevsw. If any
		 * of these required functions are missing, return an
		 * error now.  Hook the open function with a
		 * local function that can be used to prevent the race
		 * condition between open and unload.
		 */
		if (mfindname (minfo, "open", (long *)&drv->d_ropen))
		    return MERR_NOOPEN;
		cdevswp->d_open = drvstub_open;

		if (mfindname (minfo, "close", (long *)&drv->d_rclose))
		    return MERR_NOCLOSE;
		cdevswp->d_close = drvstub_close;

		/* Look for optional routines. */
		mfindname (minfo, "read", (long *)&cdevswp->d_read);
		mfindname (minfo, "write", (long *)&cdevswp->d_write);
		mfindname (minfo, "ioctl", (long *)&cdevswp->d_ioctl);
		mfindname (minfo, "mmap", (long *)&cdevswp->d_mmap);
		mfindname (minfo, "map", (long *)&cdevswp->d_map);
		mfindname (minfo, "unmap", (long *)&cdevswp->d_unmap);
		mfindname (minfo, "poll", (long *)&cdevswp->d_poll);
		mfindname (minfo, "attach", (long *)&cdevswp->d_attach);
		mfindname (minfo, "detach", (long *)&cdevswp->d_detach);
		mfindname (minfo, "enable", (long *)&cdevswp->d_enable);
		mfindname (minfo, "disable", (long *)&cdevswp->d_disable);

		cdevswp->d_str = 0;

		cdevswp->d_desc = desc;
	}
	if (drv->d_typ & MDRV_BLOCK) {
		ASSERT(!(drv->d_typ & MDRV_STREAM));

		/* set flags and cpulock */
		bdevswp = &tmpbdev;
		bdevswp->d_cpulock = drv->d_lck;
		bdevswp->d_flags = devflag;

		/* Look for I/O routines and hook into cdevsw. If any
		 * of these required functions are missing, return an
		 * error now.  Hook the open function with a
		 * local function that can be used to prevent the race
		 * condition between open and unload.
		 */
		if (mfindname (minfo, "open", (long *)&drv->d_ropen))
		    return MERR_NOOPEN;
		bdevswp->d_open = drvstub_open;

		if (mfindname (minfo, "close", (long *)&drv->d_rclose))
		    return MERR_NOCLOSE;
		bdevswp->d_close = drvstub_close;

		if (mfindname (minfo, "strategy", (long *)&bdevswp->d_strategy))
		    return MERR_NOSTRAT; 
		if (mfindname (minfo, "size", (long *)&bdevswp->d_size))
		    return MERR_NOSIZE;

		/* Look for optional routines.  If any is missing give it
		 * a reasonable default.
		 */
		if (mfindname (minfo, "size64", (long *)&bdevswp->d_size64))
		    bdevswp->d_size64 = (int (*)(dev_t, daddr_t *))nulldev;
		if (mfindname (minfo, "print", (long *)&bdevswp->d_print))
		    bdevswp->d_print = (int (*)(dev_t, char *))nodev;
		if (mfindname (minfo, "map", (long *)&bdevswp->d_map))
		    bdevswp->d_map = (int (*)(struct vhandl *, off_t, size_t, uint_t))nodev;
		if (mfindname (minfo, "unmap", (long *)&bdevswp->d_unmap))
		    bdevswp->d_unmap = (int (*)(struct vhandl *))nodev;
		if (mfindname (minfo, "dump", (long *)&bdevswp->d_dump))
		    bdevswp->d_dump = (int (*)(dev_t, int, daddr_t, caddr_t, int))nodev;

		bdevswp->d_desc = desc;
	}

	/*
	 * Call the driver's init or start function if it has one;
	 * it is not required.
	 */
	if (mfindname (minfo, "init", (long *)&func) == 0)
		(*func)();

	if (drv->d_edt) {
		if (error = medtinit (minfo, drv->d_edt))
			return error;
	}

	if (mfindname (minfo, "start", (long *)&func) == 0)
		(*func)();

	/* Lastly enter the temp devsw entries into the device's cdevsw/bdevsw.
	 * This way everything is initialized and ready to go as soon as a user
	 * has access to the driver.
	 */
	driver_insert(desc, cdevswp, bdevswp);

	return 0;
}

/*
 * driver_free - free up table entries allocated for driver
 */
static void
driver_free (cfg_desc_t *desc)
{
	cfg_driver_t *drv = desc->m_data;
	int sp = mp_mutex_spinlock(&drvlock);

	device_driver_free(desc->m_device_driver);

	free_major(drv);
	ddelete(desc);

	if ((drv->d_typ & (MDRV_CHAR|MDRV_BLOCK)) == (MDRV_CHAR|MDRV_BLOCK))
		free_bcdevsw(drv);
	else if (drv->d_typ & (MDRV_CHAR|MDRV_STREAM))
		free_cdevsw(drv);
	else
		free_bdevsw(drv);

	mp_mutex_spinunlock(&drvlock, sp);
}

/*
 * drv_unload - unload a driver from the kernel
 */
static int
drv_unload (cfg_desc_t *desc)
{
	cfg_driver_t *drv = (cfg_driver_t *)desc->m_data;
	int (*unload)(void);
	int error;

	unload = drv->d_unload;

	/* If there's no unload routine, don't unload.  This allows some
	 * degree of compatibility with old drivers without unload functions.
	 */
	if (!unload)
		return MERR_NOUNLD;

	/*
	 * If we're loaded but not registered, try to unregister.
	 * If the unregister fails, then we can't unload.
	 */
	if (!(desc->m_flags & M_REGISTERED))
		if (drv_unreg(desc))
			return MERR_UNREGFAIL;

	/* Attempt to unload the driver.  If its unload function
	 * fails, then bail out.  XXX Driver may have a <prefix>halt
	 * entry, maybe it should be called too?
	 */
	ASSERT(IS_KSEG0(unload) || IS_KSEG2(unload));
	error = (*unload)();
	if (error)
		return MERR_UNLDFAIL;

	/* If the driver has been registered, then leave it registered.
	 */
	if (desc->m_flags & M_REGISTERED) {
		driver_regstub(desc);
	} else
		driver_free(desc);

	return 0;
}

static int
drvstub_open(dev_t *devp, int mode, int a1, cred_t *a2)
{
	cfg_desc_t *desc = NULL;
	int sp, rv;
	struct bdevsw *my_bdevsw;
	struct cdevsw *my_cdevsw;

#ifdef	MTRACE
	printf ("drvstub_open\n");
#endif	/* MTRACE */

	sp = mp_mutex_spinlock(&drvlock);
	my_cdevsw = get_cdevsw(*devp);
	if (my_cdevsw != NULL)
		desc = my_cdevsw->d_desc;
	else {
		my_bdevsw = get_bdevsw(*devp);
		if (my_bdevsw != NULL)
			desc = my_bdevsw->d_desc;
	}
	if (!desc) {
		mp_mutex_spinunlock(&drvlock, sp);
		cmn_err_tag(297,CE_WARN, "Failed to dynamically load module for device 0x%x.", *devp);
		return ENXIO;
	}

	desc->m_refcnt++;
	mp_mutex_spinunlock(&drvlock, sp);
	psema(&desc->m_transsema, PZERO);

	ASSERT((getmajor(*devp) != majorinvalid) || (dev_is_vertex(*devp)));

	rv = (*(drvopenfunc_t)((cfg_driver_t *)desc->m_data)->d_ropen)
			(devp, mode, a1, a2);

	sp = mp_mutex_spinlock(&drvlock);
	desc->m_refcnt--;
	mp_mutex_spinunlock(&drvlock, sp);
	vsema(&desc->m_transsema);

	return rv;
}

/*
 * drvstub_attach - stub that loads a registered block or char driver on
 * 	first attach.  If the load fails, the driver remains registered.
 */
/*REFERENCED*/
static int
drvstub_attach(vertex_hdl_t attach_vhdl)
{
	cfg_desc_t *desc = NULL;
	int sp;
	struct bdevsw *my_bdevsw;
	struct cdevsw *my_cdevsw;

#ifdef	MTRACE
	printf ("drvstub_attach\n");
#endif	/* MTRACE */

	sp = mp_mutex_spinlock(&drvlock);
	my_cdevsw = get_cdevsw(attach_vhdl);
	if (my_cdevsw != NULL)
		desc = my_cdevsw->d_desc;
	else {
		my_bdevsw = get_bdevsw(attach_vhdl);
		if (my_bdevsw != NULL)
			desc = my_bdevsw->d_desc;
	}
	if (!desc) {
		mp_mutex_spinunlock(&drvlock, sp);
		cmn_err_tag(298,CE_WARN, "Failed to dynamically attach module for device 0x%x.", attach_vhdl);
		return ENXIO;
	}

	desc->m_refcnt++;
	mp_mutex_spinunlock(&drvlock, sp);
	psema(&desc->m_transsema, PZERO);

	(*(drvattachfunc_t)((cfg_driver_t *)desc->m_data)->d_rattach)
			(attach_vhdl);

	sp = mp_mutex_spinlock(&drvlock);
	desc->m_refcnt--;
	mp_mutex_spinunlock(&drvlock, sp);
	vsema(&desc->m_transsema);

	return 0;
}

/*REFERENCED*/
static int
drvstub_detach(vertex_hdl_t detach_vhdl)
{
	cfg_desc_t *desc = NULL;
	int sp, rv;
	struct cdevsw *my_cdevsw;
	struct bdevsw *my_bdevsw;

close_retry:
	sp = mp_mutex_spinlock(&drvlock);
	my_cdevsw = get_cdevsw(detach_vhdl);
	if (my_cdevsw != NULL)
		desc = my_cdevsw->d_desc;
	else {
		my_bdevsw = get_bdevsw(detach_vhdl);
		if (my_bdevsw != NULL)
			desc = my_bdevsw->d_desc;
	}
	ASSERT(desc);
	if (desc->m_flags & M_TRANSITION) {
		/* Someone is trying to unload the driver.  The driver will
		 * reject the unload request because the device is still
		 * open -- it must be, this close hasn't executed yet.
		 * Retry.  Eventually the unload request will fail which
		 * will clear the M_TRANSITION flag and this close can
		 * be completed.
		 */
		mp_mutex_spinunlock(&drvlock, sp);
		goto close_retry;
	}
	desc->m_refcnt++;
	mp_mutex_spinunlock(&drvlock, sp);

	rv = (*(drvdetachfunc_t)((cfg_driver_t *)desc->m_data)->d_rclose)(detach_vhdl);

	sp = mp_mutex_spinlock(&drvlock);
	desc->m_refcnt--;
	mp_mutex_spinunlock(&drvlock, sp);

	return rv;
}

static int
drvstub_close(dev_t dev, int a1, int a2, cred_t *a3)
{
	cfg_desc_t *desc = NULL;
	int sp, rv;
	struct cdevsw *my_cdevsw;
	struct bdevsw *my_bdevsw;

close_retry:
	sp = mp_mutex_spinlock(&drvlock);
	my_cdevsw = get_cdevsw(dev);
	if (my_cdevsw != NULL)
		desc = my_cdevsw->d_desc;
	else {
		my_bdevsw = get_bdevsw(dev);
		if (my_bdevsw != NULL)
			desc = my_bdevsw->d_desc;
	}
	ASSERT(desc);
	if (desc->m_flags & M_TRANSITION) {
		/* Someone is trying to unload the driver.  The driver will
		 * reject the unload request because the device is still
		 * open -- it must be, this close hasn't executed yet.
		 * Retry.  Eventually the unload request will fail which
		 * will clear the M_TRANSITION flag and this close can
		 * be completed.
		 */
		mp_mutex_spinunlock(&drvlock, sp);
		goto close_retry;
	}
	desc->m_refcnt++;
	mp_mutex_spinunlock(&drvlock, sp);

	ASSERT((getmajor(dev) != majorinvalid) || (dev_is_vertex(dev)));

	rv = (*(drvclosefunc_t)((cfg_driver_t *)desc->m_data)->d_rclose)(dev, a1, a2, a3);

	sp = mp_mutex_spinlock(&drvlock);
	desc->m_refcnt--;
	mp_mutex_spinunlock(&drvlock, sp);

	return rv;
}

/*
 * mload_driver_load
 */
int
mload_driver_load(cfg_desc_t *desc)
{
	ml_info_t *m;
	int sp;
	int error = 0;

#ifdef	MTRACE
	printf ("mload_driver_load\n");
#endif	/* MTRACE */

	sp = drv_lock(desc);

	/*
	 * If there's no runtime unix symbol table available to be 
	 * autoloaded or no unix symbol table loaded via ml, giveup.
	 */

	if (!unixsymtab_avail && !mlunixsymtab) {
		drv_unlock(desc, sp);
		cmn_err_tag(299,CE_WARN, "%s: Failed to dynamically load module: no runtime symbol table available.",
			desc->m_fname);
		return ENXIO;
	}

	ASSERT(desc->m_flags & M_REGISTERED);
	desc->m_flags |= M_TRANSITION;
	drv_unlock(desc, sp);
	psema(&desc->m_transsema, PZERO);

	/*
	 * Someone may have beat us to load the driver, so check if
	 * already loaded.
	 */
	if (desc->m_flags & M_LOADED) {
		vsema(&desc->m_transsema);
		return 0;
	}

	m = desc->m_info;

	/* read the .o file, create a symbol table, and relocate it */
	if (error = mloadfile (m)) {
		sp = drv_lock(desc);
		desc->m_flags &= ~M_TRANSITION;
		drv_unlock(desc, sp);
		vsema(&desc->m_transsema);
		if (error == MERR_BADMAGIC)
			cmn_err_tag(300,CE_WARN, "%s: Failed to dynamically load module. Object file format not ELF.", 
				desc->m_fname);
		else
			cmn_err_tag(301,CE_WARN, "%s: Failed to dynamically load module. Cannot read or relocate object.", 
				desc->m_fname);
		return ENOEXEC;
	}

	/* init kernel driver structs and initialize driver */
	if (driver_init (m)) {
		sp = drv_lock(desc);
		desc->m_flags &= ~M_TRANSITION;
		drv_unlock(desc, sp);
		vsema(&desc->m_transsema);
		cmn_err_tag(302,CE_WARN, "%s: Failed to dynamically load module. Failed to initialize driver.",
			    desc->m_fname);
		return EIO;
	}

	ASSERT(desc->m_flags & M_REGISTERED);
	sp = drv_lock(desc);
	desc->m_flags |= M_LOADED;
	desc->m_flags &= ~M_TRANSITION;
	drv_unlock(desc, sp);
	vsema(&desc->m_transsema);

	return 0;
}

void
mload_driver_hold(cfg_desc_t *desc)
{
	int sp;

	sp = drv_lock(desc);
	desc->m_refcnt++;
	drv_unlock(desc, sp);
}

void
mload_driver_release(cfg_desc_t *desc)
{
	int sp;

	sp = drv_lock(desc);
	desc->m_refcnt--;
	drv_unlock(desc, sp);
}

static int
drvstub_stropen(queue_t *q, dev_t *devp, int flag, int sflag, cred_t *cr)
{
	cfg_desc_t *desc;
	int sp, rv;
	struct cdevsw *my_cdevsw = get_cdevsw(*devp);

#ifdef	MTRACE
	printf ("drvstub_stropen\n");
#endif	/* MTRACE */

	sp = mp_mutex_spinlock(&drvlock);
	desc = my_cdevsw->d_desc;
	if (!desc) {
		mp_mutex_spinunlock(&drvlock, sp);
		cmn_err_tag(303,CE_WARN, "Failed to dynamically load module for device 0x%x.", *devp);
		return ENXIO;
	}

	desc->m_refcnt++;
	mp_mutex_spinunlock(&drvlock, sp);
	psema(&desc->m_transsema, PZERO);

	ASSERT(getmajor(*devp) != majorinvalid);

	rv = (*(strdrvopenfunc_t)((cfg_driver_t *)desc->m_data)->d_ropen)(q, devp,
	    flag, sflag, cr);

	sp = mp_mutex_spinlock(&drvlock);
	desc->m_refcnt--;
	mp_mutex_spinunlock(&drvlock, sp);
	vsema(&desc->m_transsema);

	return rv;
}

static int
drvstub_strclose(queue_t *q, int flag, cred_t *cr)
{
	cfg_desc_t *desc;
	int sp, rv;

close_retry:
	sp = mp_mutex_spinlock(&drvlock);
	desc = dfind_minfo(q->q_qinfo->qi_minfo);	/*ugh*/
	ASSERT(desc);
	if (desc->m_flags & M_TRANSITION) {
		/* Someone is trying to unload the driver.  The driver will
		 * reject the unload request because the device is still
		 * open -- it must be, this close hasn't executed yet.
		 * Retry.  Eventually the unload request will fail which
		 * will clear the M_TRANSITION flag and this close can
		 * be completed.
		 */
		mp_mutex_spinunlock(&drvlock, sp);
		goto close_retry;
	}
	desc->m_refcnt++;
	mp_mutex_spinunlock(&drvlock, sp);

	rv = (*(strdrvclosefunc_t)((cfg_driver_t *)desc->m_data)->d_rclose)(q, flag, cr);

	sp = mp_mutex_spinlock(&drvlock);
	desc->m_refcnt--;
	mp_mutex_spinunlock(&drvlock, sp);

	return rv;
}

/*
 * drvstub_strload - stub that loads a registered stream driver on
 * 	first open.  If the load fails, the driver remains registered.
 */
static int
drvstub_strload(queue_t *q, dev_t *devp, int flag, int sflag, cred_t *cr)
{
	ml_info_t *m;
	struct cdevsw *my_cdevsw = get_cdevsw(*devp);
	cfg_desc_t *desc;
	int sp;
	struct streamtab *d_str;
	int error = 0;

#ifdef	MTRACE
	printf ("drvstub_strload\n");
#endif	/* MTRACE */

	sp = mp_mutex_spinlock(&drvlock);
	desc = my_cdevsw->d_desc;
	if (!desc) {
		mp_mutex_spinunlock(&drvlock, sp);
		cmn_err_tag(304,CE_WARN, "Failed to dynamically load module for device 0x%x.", *devp);
		return ENXIO;
	}

	ASSERT(!(desc->m_flags & M_LOADED));
	ASSERT(desc->m_flags & M_REGISTERED);
	desc->m_flags |= M_TRANSITION;
	mp_mutex_spinunlock(&drvlock, sp);
	psema(&desc->m_transsema, PZERO);

	m = desc->m_info;

	/* read the .o file, create a symbol table, and relocate it */
	if (error = mloadfile (m)) {
		sp = mp_mutex_spinlock(&drvlock);
		desc->m_flags &= ~M_TRANSITION;
		mp_mutex_spinunlock(&drvlock, sp);
		vsema(&desc->m_transsema);
		if (error == MERR_BADMAGIC)
			cmn_err_tag(305,CE_WARN, "%s: Failed to dynamically load module. Object file format not ELF.", desc->m_fname);
		else
			cmn_err_tag(306,CE_WARN, "%s: Failed to dynamically load module. Cannot read or relocate object.", desc->m_fname);
		return ENOEXEC;
	}

	/* init kernel driver structs and initialize driver */
	if (driver_init (m)) {
		sp = mp_mutex_spinlock(&drvlock);
		desc->m_flags &= ~M_TRANSITION;
		mp_mutex_spinunlock(&drvlock, sp);
		vsema(&desc->m_transsema);
		cmn_err_tag(307,CE_WARN, "%s: Failed to dynamically load module. Failed to initialize driver.",
			    desc->m_fname);
		return EIO;
	}

	/* Replace the queue with its new values */
	my_cdevsw = get_cdevsw(*devp);
	d_str = my_cdevsw->d_str;
	setq(q, d_str->st_rdinit, d_str->st_wrinit);

	ASSERT(desc->m_flags & M_REGISTERED);
	sp = mp_mutex_spinlock(&drvlock);
	desc->m_flags |= M_LOADED;
	desc->m_flags &= ~M_TRANSITION;
	mp_mutex_spinunlock(&drvlock, sp);
	vsema(&desc->m_transsema);

	return drvstub_stropen(q, devp, flag, sflag, cr);
}

/*
 * driver_regstub - create a driver stub to load the driver later
 */
static void
driver_regstub(cfg_desc_t *desc)
{
	struct cdevsw tmpcdev, *cdevswp = &tmpcdev;
	struct bdevsw tmpbdev, *bdevswp = &tmpbdev;
	cfg_driver_t *drv = desc->m_data;

	/* Fill in dummy devflags and devload function in devsw
	 */
	if (drv->d_typ & (MDRV_CHAR|MDRV_STREAM)) {
		bzero (cdevswp, sizeof *cdevswp);
		cdevswp->d_flags = &drvstub_flag;

		if (drv->d_typ & MDRV_CHAR) {
			cdevswp->d_open = NULL;
			cdevswp->d_attach = NULL;
		} else
			cdevswp->d_str = &drvstub_strinfo;

		cdevswp->d_desc = desc;
	}
	if (drv->d_typ & MDRV_BLOCK) {
		bzero (bdevswp, sizeof *bdevswp);
		bdevswp->d_flags = &drvstub_flag;
		bdevswp->d_open = NULL;

		bdevswp->d_desc = desc;
	}

	driver_insert(desc, cdevswp, bdevswp);
}

/*
 * drv_reg - register a driver to be loaded later
 */
static int
drv_reg(ml_info_t *minfo)
{
	return(drv_do_load_or_reg(minfo, DRV_REGISTER));
}

/*
 * drv_unreg - unregister a driver that was to be loaded later.
 * We need to temporarily load text of the driver just to call
 * its unregister routine, if it exists.
 */
static int
drv_unreg(cfg_desc_t *desc)
{
	ml_info_t *minfo = desc->m_info;
	cfg_driver_t *drv = desc->m_data;
	int (*unreg)(void);
	int (*reg)(void);
	int error = 0;
	int driver_initted = 0;
	int driver_loaded = 0;

	if (desc->m_flags & M_LOADED) {
		unreg = drv->d_unreg;
		reg = drv->d_reg;
	} else {
		if (error = mloadfile (minfo))
			return error;
	
		driver_loaded = 1;

		if (mfindname (minfo, "unreg", (long *)&unreg)) {
			unreg = NULL;
			if (mfindname (minfo, "reg", (long *)&reg))
				reg = NULL;
		} else {
			if (error = driver_init (minfo)) {
				goto out;
			}
			reg = drv->d_reg;
			driver_initted = 1;
		}
	}

	/* Give driver an opportunity to respond to unregister. */
	if (unreg) {
		ASSERT(IS_KSEG0(unreg) || IS_KSEG2(unreg));
		error = (*unreg)();
		if (error) {
#if DEBUG
			cmn_err(CE_WARN, "Unregister failed (%d): %s\n", error, desc->m_fname);
#endif /* DEBUG */
			error = MERR_UNREGBUSY;
			goto out;
		}
	} else {
		/* If there's no unreg routine, but there is a reg routine, fail */
		if (reg) {
			error = MERR_UNREGBUSY;
			goto out;
		}
	}

	if (driver_initted) {
		int (*unload)(void) = drv->d_unload;

		if (unload) {
			ASSERT(IS_KSEG0(unload) || IS_KSEG2(unload));
			error = (*unload)();
			if (error) {
				/* Shouldn't really happen -- odd driver */
				cmn_err_tag(308,CE_WARN, "Unregistered, but unload failed (%d): %s\n", error, desc->m_fname);
				error = MERR_UNREGBUSY;
				goto out;
			}
		}
	}

	if (!(desc->m_flags & M_LOADED)) {
		driver_free(desc);
		driver_initted = 0;
	}

out:
	if (driver_loaded)
		munloadfile(minfo);

	if (driver_initted)
		driver_regstub (desc);

	return error;
}

static void
devupdate(cfg_desc_t *desc)
{
	int sp;

	sp = mp_mutex_spinlock(&drvlock);
	desc->m_refcnt++;
	/*
	 * If the refcnt is 1 and its a registered module and it has
	 * an unload timeout, then check to see if we should cancel 
	 * the auto-unload timer.
	 */
	if ((desc->m_refcnt == 1) && (desc->m_delay != M_NOUNLD))
		mtimerstop (desc);	
	mp_mutex_spinunlock(&drvlock, sp);
}

/*
 * devhold - increment a refcnt
 *
 * NOTE - called from specfs, clone, gentty, lv, etc. via cdhold/bdhold.
 *	Also called from qattach. mlqdetach is called to decrement the
 *	refcnt, since we only have a q to figure out who it is.
 *
 * MORE NOTES
 *
 * Hold/Rele routines
 * ------------------
 * 
 * The set of routines specdevhold, specdevrele, cdhold, cdrele, bdhold, bdrele,
 * fmhold and fmrele perform locking so that refcnts for loadable drivers can be
 * incremented and decremented. The refcnts are used to determine if we can
 * unload a driver or not. The locking also insures that drivers will not be
 * unloaded during a new open on a device.
 * 
 * When a device is opened and closed, specdevhold and specdevrele make
 * calls to cdhold/cdrele or bdhold/bdrele as appropriate to increment or
 * decrement the refcnt.
 * 
 * For other drivers that call open and close routines directly, they must
 * call the appropriate hold/rele calls directly.
 * 
 * cdhold and cdrele are called from io/clone.c and io/gentty.c:
 * 
 * io/clone.c - called from clnopen, which calls qi_qopen.
 * io/gentty.c - called from syopen, which calls either stropen or cdopen.
 * 
 * bdhold and bdrele are called from io/lv.c:
 * 
 * io/lv.c - called from lvopen and lvioctl, which calls bdopen.
 * 
 * For streams, fmhold and fmrele are called within the io/streams code.
 * 
 * fmhold and fmrele are also called from io/sad.c:
 * 
 * io/sad.c -  calls fmhold/fmrele from apush_iocdata, where fmodsw is
 * examined.
 *
 */

static int
devhold(struct bdevsw *my_bdevsw, struct cdevsw *my_cdevsw)
{
	int sp;
	cfg_desc_t *desc;

	ASSERT((my_bdevsw && !my_cdevsw) || (!my_bdevsw && my_cdevsw));
	
	sp = mp_mutex_spinlock(&drvlock);
	if (my_bdevsw)
		desc = my_bdevsw->d_desc;
	if (my_cdevsw)
		desc = my_cdevsw->d_desc;
	if (!desc) {
		mp_mutex_spinunlock(&drvlock, sp);
		return ENXIO;
	}
	if (desc->m_flags & M_UNLOADING) {
		mp_mutex_spinunlock(&drvlock, sp);
		return EBUSY;
	} else if (desc->m_flags & M_TRANSITION) {
		mp_mutex_spinunlock(&drvlock, sp);
		psema(&desc->m_transsema, PZERO);
		devupdate(desc);
		vsema(&desc->m_transsema);
	} else {
		mp_mutex_spinunlock(&drvlock, sp);
		devupdate(desc);
	}
	return 0;
}

/*
 * devrele - decrement a refcnt
 *
 * NOTE - called from specfs, clone, gentty, lv, etc. via cdhold/bdhold.
 */
static void
devrele(struct bdevsw *my_bdevsw, struct cdevsw *my_cdevsw)
{
	int sp;
	cfg_desc_t *desc;

	ASSERT((my_bdevsw && !my_cdevsw) || (!my_bdevsw && my_cdevsw));
	
	sp = mp_mutex_spinlock(&drvlock);
	if (my_bdevsw)
		desc = my_bdevsw->d_desc;
	if (my_cdevsw)
		desc = my_cdevsw->d_desc;
	ASSERT(desc && !(desc->m_flags & M_TRANSITION));
	desc->m_refcnt--;
	/*
	 * If the refcnt is 0 and its a registered module and it has
	 * an unload timeout, then start the auto-unload timer.
	 */
	if ((desc->m_refcnt == 0) && (desc->m_delay != M_NOUNLD) &&
	    (desc->m_flags & M_REGISTERED)) {
		if (desc->m_delay != M_UNLDDEFAULT)
			mtimerstart (desc, desc->m_delay*M_UNLDVAL);
		else if (module_unld_delay*M_UNLDVAL)
			mtimerstart (desc, module_unld_delay*M_UNLDVAL);
	}
	mp_mutex_spinunlock(&drvlock, sp);
}

static int
cdevhold(struct cdevsw *my_cdevsw)
{
	return(devhold(NULL, my_cdevsw));
}

static int
bdevhold(struct bdevsw *my_bdevsw)
{
	return(devhold(my_bdevsw, NULL));
}

static void
cdevrele(struct cdevsw *my_cdevsw)
{
	devrele(NULL, my_cdevsw);
}

static void
bdevrele(struct bdevsw *my_bdevsw)
{
	devrele(my_bdevsw, NULL);
}

/*
 * cdhold - increment a refcnt for a char driver
 *
 * NOTE: called from specfs, clone and gentty.
 */
int 
cdhold(struct cdevsw *my_cdevsw)
{
	return cdstatic(my_cdevsw) ? 0 : cdevhold(my_cdevsw);
}

/*
 * cdrele - decrement a refcnt for a char driver
 *
 * NOTE: called from specfs, clone and gentty.
 */
void 
cdrele(struct cdevsw *my_cdevsw)
{
	if (!cdstatic(my_cdevsw))
		cdevrele(my_cdevsw);
}

/*
 * bdhold - increment a refcnt for a block driver
 *
 * NOTE: called from specfs and lv.
 */
int 
bdhold(struct bdevsw *my_bdevsw)
{
	return bdstatic(my_bdevsw) ? 0 : bdevhold(my_bdevsw);
}

/*
 * bdrele - decrement a refcnt for a block driver
 *
 * NOTE: called from specfs and lv.
 */
void 
bdrele(struct bdevsw *my_bdevsw)
{
	if (!bdstatic(my_bdevsw))
		bdevrele(my_bdevsw);
}


/*
 * mautounload - auto unload registered modules
 *
 * All loadable modules that are registered are auto-unloaded after
 * x time unless they have been specifically configured not to or
 * unless they don't have an unload routine.
 *
 * After the last close, a timer is started using m_delay from the
 * modules desc as the timeout. m_delay is normally set to the default
 * held by module_unld_delay, unless a specific value was passed in using the
 * ml command when the module was registered. If m_delay is set to
 * M_NOUNLD, then the driver isn't auto-unloaded. When the timer expires,
 * mautounload is called, which awakens the munldd daemon.  This daemon
 * can operate with a thread context, and so handle the unload operations.
 */


/*
 * mautounload()
 * This is the timeout handler.
 */
void
mautounload (int *tflags)
{
	*tflags = M_EXPIRED;
	vsema(&munlddwake);
}

static void
munldd(void)
{
	for ( ; ; ) {
		/* rock-a-bye baby ... */
		psema(&munlddwake, PZERO);

		/* should *never* have an error in this case */
		(void) munload (0, MAUTOUNLOAD);
	}
}

static void
mtimerstop (cfg_desc_t *desc)
{
	if (desc->m_timeoutid) {
		desc->m_flags |= M_NOAUTOUNLOAD;
		untimeout (desc->m_timeoutid);
		desc->m_timeoutid = 0;
	}
}

static void
mtimerstart (cfg_desc_t *desc, int delay)
{
	desc->m_flags &= ~M_NOAUTOUNLOAD;
	desc->m_timeoutid = timeout (mautounload, &desc->m_tflags, delay);
}

/*
 * medtinit - initialization for drivers with edt structs
 *
 * NOTE - this routine is shared with M_ENET type modules.
 */
static int
medtinit (ml_info_t *minfo, medt_t *medtp)
{
	edt_t *edtp;
	int *func;
	cpu_cookie_t ocpu;
	int i;
	int adaps;

	while (medtp) {
		edtp = medtp->edtp;
		/* make sure v_cpuintr specifies a valid cpu */
		if( !cpu_isvalid(edtp->v_cpuintr) )
			edtp->v_cpuintr = masterpda->p_cpuid;

		if (edtp->e_bus_type == ADAP_VME ) {
			vme_intrs_t *intrs = edtp->e_bus_info;

			/* if a hardwired interrupt, get an interrupt handler */
			if (intrs && 
				mfindname (minfo, "intr", (long *)&intrs->v_vintr)) {
				if (intrs->v_vec)
					/* Fatal if an intr vector was
					 * specified in VECTOR line, and
					 * there is no interrupt handler.
					 */
					return MERR_NOINTR;

				else 
					/* If there is no interrupt 
					 * handler for the driver, keep this
					 * field zero. This tends
					 * to get passed to vme_ivec_set 
					 * routines. 
					 */
					intrs->v_vintr = 0;

			}
			/* Initialize adap and io interrupt level. */
			if (!vme_init_adap (edtp))
				return MERR_BADADAP;

		} else if ((edtp->e_bus_type == ADAP_GIO) || (edtp->e_bus_type == ADAP_EISA)) {

			if (mfindname (minfo, "intr", (long *)&func))
				return MERR_NOINTR;

		}

		if (mfindname (minfo, "edtinit", (long *)&edtp->e_init) == 0) {

			ocpu = setmustrun(edtp->v_cpuintr);

			if( edtp->e_adap != -1 ) {
				(*edtp->e_init)(edtp);
			} else {
				adaps = readadapters(edtp->e_bus_type);
				for( i = 0 ; i < adaps ; i++ ) {
					edtp->e_adap = (edtp->e_bus_type == ADAP_VME) ?
						VMEADAP_ITOX(i) : i;

					(*edtp->e_init)(edtp);
				}
				edtp->e_adap = -1;
			}

			restoremustrun(ocpu);
		}
		medtp = medtp->medt_next;
	}
	return 0;
}

/*
 * makeedtdata - copy in edt structs
 *
 * NOTE - this routine is shared with M_ENET type modules.
 */
static int
makeedtdata(medt_t *medtpfrom, medt_t *medtpto)
{
	medt_t *ep;
	edt_t *edtp;

	/*
	 * If there are multiple ctrls, then lboot produces a list
	 * of edt structs for each driver. For each edt, do a
	 * copyin.
	 */

	ep = medtpfrom;
	while (ep) {
		medtpto->edtp = edtp = kern_calloc (1, sizeof (edt_t));
		medtpto->medt_next = (medt_t *)0;
		if (copyin(ep->edtp, edtp, sizeof(edt_t)))
			return MERR_COPY;

		/* TODO - handle other ADAP types */
		switch (ep->edtp->e_bus_type) {
		case ADAP_VME:
			{
			vme_intrs_t *intrs = 0;

			/* Its possible to have a driver without e_bus_info. */
			if (ep->edtp->e_bus_info != NULL) {
				intrs = kern_calloc(1, sizeof(vme_intrs_t));
				if (copyin(ep->edtp->e_bus_info, intrs,
						sizeof(vme_intrs_t))) {
					kern_free(intrs);
					return MERR_COPY;
				}
				intrs->v_vintr = 0;
				edtp->e_bus_info = intrs;
			}
			break;
			}
		case ADAP_EISA:		/* will probably have e_bus_info ??? */
		case ADAP_GIO:
		default:
			edtp->e_bus_info = 0;
			break;
		}
		if (ep = ep->medt_next) {
			medtpto->medt_next = kern_calloc (1, sizeof (medt_t));
			medtpto = medtpto->medt_next;
		}
	}

	return 0;
}

/*
 * END driver support
 *
 * BEGIN streams support
 */

#define STRTBSIZE	16	/* must be power of 2 */
static cfg_desc_t *strtab[STRTBSIZE];

static int strstub_open(queue_t *, dev_t *, int, int, cred_t *);
static int strstub_close(queue_t *, int, cred_t *);
static int strstub_load(queue_t *, dev_t *, int, int, cred_t *);
static int strstub_wput(queue_t *, mblk_t *);
static int strstub_rput(queue_t *, mblk_t *);

#define STRTBHASH(minfo)	\
	(unsigned)(((__psunsigned_t)(minfo) >> 4) & (STRTBSIZE - 1))
#define STRTBHASHQ(q) 	 STRTBHASH((q)->q_qinfo->qi_minfo)
#define STRTBHASHF(fmod) STRTBHASH(fmodsw[fmod].f_str->st_rdinit->qi_minfo)
#define STRTBHASHD(desc)	\
    STRTBHASH(((cfg_streams_t *)((desc)->m_data))->s_info->st_rdinit->qi_minfo)

static int fmodidbase;	/* unique id base */
static int fmodswstart;
static mutex_t fmodsem;	 

static lock_t strlock;

static int strstub_flag = D_MP;
static struct module_info strstub_minfo = 
    {STRID_STRLOAD, "STRLOAD", 0, INFPSZ, 128, 16 };
static struct qinit strstub_rinit = 
    {strstub_rput, NULL, strstub_load, NULL, NULL, &strstub_minfo, NULL};
static struct qinit strstub_winit = 
    {strstub_wput, NULL, NULL, NULL, NULL, &strstub_minfo, NULL};
static struct streamtab strstub_strinfo = 
    {&strstub_rinit, &strstub_winit, NULL, NULL};

static int
str_init(int base)
{
	fmodswstart = fmodcnt;

	/* protection for fmodsw array */
	mutex_init(&fmodsem, MUTEX_DEFAULT, "mlfmod");
	spinlock_init (&strlock, "mlstreams");

	fmodidbase = base;
	return fmodmax;
}

static int
str_id (cfg_desc_t *desc)
{
	cfg_streams_t *fm = (cfg_streams_t *)desc->m_data;
	return fmodidbase + fm->s_fmod;
}

/* ARGSUSED1 */
static int
str_lock (cfg_desc_t *desc)
{
	return mp_mutex_spinlock(&strlock);
}

/* ARGSUSED1 */
static void
str_unlock (cfg_desc_t *desc, int sp)
{
	mp_mutex_spinunlock(&strlock, sp);
}

static void 
str_copydata(cfg_desc_t *desc, void *ptr)
{
	bcopy(desc->m_data, ptr, sizeof(mod_streams_t));
}

static void
sinsert(cfg_desc_t *desc)
{
	u_int hash = STRTBHASHD(desc);

	desc->m_next = strtab[hash];
	strtab[hash] = desc;
}

static void
sdelete(cfg_desc_t *desc)
{
	cfg_desc_t *d, *prev = 0;
	u_int hash = STRTBHASHD(desc);

	d = strtab[hash];
	while (d) {
		if (d == desc) {
			if (!prev)
				strtab[hash] = d->m_next;
			else
				prev->m_next = d->m_next;
			break;
		}
		prev = d;
		d = d->m_next;
	}
	ASSERT(d);
}

static cfg_desc_t *
sfindq(queue_t *q)
{
	cfg_desc_t *desc;

	desc = strtab[STRTBHASHQ(q)];
	while (desc) {
		cfg_streams_t *str = desc->m_data;
		if (str->s_info->st_rdinit->qi_minfo == q->q_qinfo->qi_minfo)
			return desc;
		desc = desc->m_next;
	}
	return 0;
}

#define fmod_empty(m) (fmodsw[m].f_flags == 0)

static cfg_desc_t *
sfindf(int fmod)
{
	cfg_desc_t *desc;

	if (fmod_empty(fmod))
		return 0;

	desc = strtab[STRTBHASHF(fmod)];
	while (desc) {
		cfg_streams_t *str = desc->m_data;
		if (str->s_info->st_rdinit->qi_minfo == 
		    fmodsw[fmod].f_str->st_rdinit->qi_minfo)
			return desc;
		desc = desc->m_next;
	}
	return 0;
}

static int
alloc_fmod(void)
{
	int fmod;

	for (fmod = fmodswstart; fmod < fmodmax; fmod++) {
		if (fmod_empty(fmod))
			return fmod;
	}

	return 0;
}

static void
free_fmod(int fmod)
{
	ASSERT(fmod >= fmodswstart);
	ASSERT(fmod < fmodmax);
#ifdef DEBUG
	bfill(&fmodsw[fmod],sizeof(fmodsw[0]),0xea);
#endif
	fmodsw[fmod].f_flags = 0;
}

/* 
 * streams_insert - enter the new entry into the fmodsw.  Hold the 
 * spinlock and update entire entry at one time.
 */
static void
streams_insert(cfg_desc_t *desc, int fmod, struct fmodsw *fmodswp)
{
	int sp = mp_mutex_spinlock(&strlock);

	bcopy (fmodswp, &fmodsw[fmod], sizeof *fmodswp);
	sinsert(desc);

	mp_mutex_spinunlock(&strlock, sp);
}

/*
 * streams_free - free up table entries allocated for streams module
 */
static void
streams_free (cfg_desc_t *desc)
{
	cfg_streams_t *str = desc->m_data;
	int sp = mp_mutex_spinlock(&strlock);

	if (str->s_fmod) {
		free_fmod(str->s_fmod);
		if (str->s_info)
			sdelete(desc);
	}

	mp_mutex_spinunlock(&strlock, sp);
}


/*
 * streams_check - sanity check before loading the streams module
 *	allocate fmodsw entry (str->s_fmod)
 */
static int
streams_check (ml_info_t *minfo)
{
	cfg_streams_t *str = (cfg_streams_t *)minfo->ml_desc->m_data;
	int cnt;

	/* Reject entry without a name */
	if (!str->s.s_name[0])
		return MERR_NOSTRNAME;

	/* make sure this module is not a duplicate */
	for (cnt = 0; cnt < fmodmax; ++cnt)
		if (!fmod_empty(cnt) &&
		    (str->s.s_name[0] == fmodsw[cnt].f_name[0]) &&
		    !strncmp(str->s.s_name, fmodsw[cnt].f_name, FMNAMESZ+1))
			return MERR_STRDUP;

	if (!(str->s_fmod = alloc_fmod()))
		return MERR_SWTCHFULL;

	return 0;
}

/*
 * streams_init - hook into kernel data structures and initialize
 */
static int
streams_init (ml_info_t *minfo)
{
	struct fmodsw fm;
	void (*func)(void) = 0;
	cfg_streams_t *str;
	int error;

#ifdef	MTRACE
	printf ("streams_init\n");
#endif	/* MTRACE */

	/* Check for version string. */
	if (error = mversion_chk (minfo))
		return error;

	str = (cfg_streams_t *) minfo->ml_desc->m_data;

	/* create fmodsw entry for module
	 */
	strncpy(fm.f_name, str->s.s_name, FMNAMESZ+1);

	/* Get devflag, reject D_OLD type modules.
	 */
	if (mfindname (minfo, "devflag", (long *)&fm.f_flags))
		return MERR_NODEVFLAG;
	if (*(fm.f_flags) & D_OBSOLD)
		return MERR_DOLD;

	if (mfindname (minfo, "info", (long *)&fm.f_str))
		return MERR_NOINFO;
	str->s_info = fm.f_str;		/* save for hashing later */

	/* Get the unload routine. */
	if (mfindname (minfo, "unload", (long *)&str->s_unload))
		str->s_unload = 0;

	/* Call the driver's init or start function if it has one;
	 * it is not required.
	 */
	if (mfindname (minfo, "init", (long *)&func) == 0)
		(*func)();
	if (mfindname (minfo, "start", (long *)&func) == 0)
		(*func)();

	/* Redirect the open and close functions so we can keep a reference
	 * count on the module.
	 */
	str->s_ropen = fm.f_str->st_rdinit->qi_qopen;
	str->s_rclose = fm.f_str->st_rdinit->qi_qclose;
	str->s_info->st_rdinit->qi_qopen = strstub_open;
	str->s_info->st_rdinit->qi_qclose = strstub_close;

	/* insert module into fmodsw and strtab */
	streams_insert(minfo->ml_desc, str->s_fmod, &fm);
	return 0;
}

/*
 * streams_regstub - create a streams stub to load the module later
 */
static int
streams_regstub(cfg_desc_t *desc)
{
	cfg_streams_t *str = desc->m_data;
	struct fmodsw fm;

#ifdef	MTRACE
	printf ("streams_regstub\n");
#endif	/* MTRACE */

	ASSERT(str->s_fmod >= fmodswstart);
	ASSERT(str->s_fmod < fmodmax);

	bzero(&fm, sizeof(fm));

	strncpy(fm.f_name, str->s.s_name, FMNAMESZ+1);
	fm.f_flags = &strstub_flag;
	fm.f_str = str->s_strinfo;	/* dummy strinfo struct */
	str->s_info = fm.f_str;		/* save for hashing later */

	streams_insert(desc, str->s_fmod, &fm);

	return 0;
}

static int
strstub_wput(queue_t *q, mblk_t *mp)
{
	putnext(q, mp);
	return 0;
}
static int
strstub_rput(queue_t *q, mblk_t *mp)
{
	putnext(q, mp);
	return 0;
}

/*
 * strstub_load - stub that loads a registered module on first open
 *	if the load fails, the module remains registered
 */
static int
strstub_load(queue_t *q, dev_t *devp, int flag, int sflag, cred_t *cr)
{
	ml_info_t *m;
	cfg_desc_t *desc;
	cfg_streams_t *str;
	int sp;
	int error = 0;

#ifdef	MTRACE
	printf ("strstub_load\n");
#endif	/* MTRACE */

	sp = mp_mutex_spinlock(&strlock);
	desc = sfindq(q);
	if (!desc) {
		mp_mutex_spinunlock(&strlock, sp);
		cmn_err_tag(309,CE_WARN, "Failed to dynamically load streams module for queue_t 0x%x.", q);
		return ENXIO;
	}
	/*
	 * If there's no runtime unix symbol table available to be 
	 * autoloaded or no unix symbol table loaded via ml, giveup.
	 */

	if (!unixsymtab_avail && !mlunixsymtab) {
		mp_mutex_spinunlock(&strlock, sp);
		cmn_err_tag(310,CE_WARN, "%s: Failed to dynamically load module: no runtime symbol table available.",
			desc->m_fname);
		return ENXIO;
	}

	ASSERT(!(desc->m_flags & M_LOADED));
	ASSERT(desc->m_flags & M_REGISTERED);
	desc->m_flags |= M_TRANSITION;
	mp_mutex_spinunlock(&strlock, sp);
	psema(&desc->m_transsema, PZERO);

	m = desc->m_info;

	/* read the .o file, create a symbol table, and relocate it */
	if (error = mloadfile (m)) {
		desc->m_flags &= ~M_TRANSITION;
		vsema(&desc->m_transsema);
		if (error == MERR_BADMAGIC)
			cmn_err_tag(311,CE_WARN, "%s: Failed to dynamically load module. Object file format not ELF.", desc->m_fname);
		else
			cmn_err_tag(312,CE_WARN, "%s: Failed to dynamically load module. Cannot read or relocate object.", desc->m_fname);
		return ENOEXEC;
	}

	streams_free(desc);	/*XXX open could be rejected before init */

	/* init kernel driver structs and initialize driver */
	if (streams_init (m)) {
		/* 
		 * If the driver has been registered, restore everything
		 * that was undone from streams_free, since it stays 
		 * registered. 
		 */
		if (desc->m_flags & M_REGISTERED) {
			if (streams_regstub(desc))
				desc->m_flags &= ~M_REGISTERED;
		}
		desc->m_flags &= ~M_TRANSITION;
		vsema(&desc->m_transsema);
		cmn_err_tag(313,CE_WARN, "%s: Failed to dynamically load module. Failed to initialize driver.",
			desc->m_fname);
		return EIO;
	}

	/* Replace the queue with its new values */
	str = desc->m_data;
	setq(q, str->s_info->st_rdinit, str->s_info->st_wrinit);

	ASSERT(desc->m_flags & M_REGISTERED);
	desc->m_flags |= M_LOADED;
	desc->m_flags &= ~M_TRANSITION;
	vsema(&desc->m_transsema);

	/* Open the device for the syscall.
	 */
	return strstub_open(q, devp, flag, sflag, cr);
}

static int
strstub_open(queue_t *q, dev_t *devp, int flag, int sflag, cred_t *cr)
{
	cfg_desc_t *desc;
	int sp, rv;

#ifdef	MTRACE
	printf ("strstub_open\n");
#endif	/* MTRACE */

	sp = mp_mutex_spinlock(&strlock);
	desc = sfindq(q);
	if (!desc) {
		mp_mutex_spinunlock(&strlock, sp);
		cmn_err_tag(314,CE_WARN, "Failed to dynamically load streams module for queue_t 0x%x.", q);
		return ENXIO;
	}

	desc->m_refcnt++;
	mp_mutex_spinunlock(&strlock, sp);
	psema(&desc->m_transsema, PZERO);

	rv = (*((cfg_streams_t *)desc->m_data)->s_ropen)(q, devp,
	    flag, sflag, cr);

	vsema(&desc->m_transsema);
	return rv;
}

static int
strstub_close(queue_t *q, int flag, cred_t *cr)
{
	cfg_desc_t *desc;
	int sp, rv;

	sp = mp_mutex_spinlock(&strlock);
	desc = sfindq(q);
	ASSERT(desc);
	ASSERT(!(desc->m_flags & M_TRANSITION));
	desc->m_refcnt--;
	mp_mutex_spinunlock(&strlock, sp);

	rv = (*((cfg_streams_t *)desc->m_data)->s_rclose)(q, flag, cr);

	return rv;
}

static void
str_freedata(cfg_desc_t *desc)
{
	if (desc->m_data) {
		cfg_streams_t *str = desc->m_data;

		if (str->s_strinfo)
			kern_free(str->s_strinfo);
		kern_free(desc->m_data);
		desc->m_data = 0;
	}
}

static int
str_makedata(cfg_desc_t *desc)
{
	cfg_streams_t *str;
	struct streamtab *strinfo;
	struct qinit *rinit, *winit;
	struct module_info *minfo;

#ifdef	MTRACE
	printf ("str_makedata\n");
#endif	/* MTRACE */

	/* malloc m_data for stream and copyin */
	str = kern_calloc(1, sizeof(cfg_streams_t));
	if (copyin(desc->m_data, str, sizeof(mod_streams_t))) {
		kern_free(str);
		return MERR_COPY;
	}

	strinfo = kern_malloc(sizeof(struct module_info) +	/* minfo */
	    		   sizeof(struct qinit) +		/* rinit */
	    		   sizeof(struct qinit) +		/* winit */
			   sizeof(struct streamtab));		/* strinfo */

	rinit = (struct qinit *)&strinfo[1];
	winit = &rinit[1];
	minfo = (struct module_info *)&rinit[2];

	bcopy(&strstub_strinfo, strinfo, sizeof(struct streamtab));
	bcopy(&strstub_rinit, rinit, sizeof(struct qinit));
	bcopy(&strstub_winit, winit, sizeof(struct qinit));
	bcopy(&strstub_minfo, minfo, sizeof(struct module_info));
	strinfo->st_rdinit = rinit;
	strinfo->st_wrinit = winit;
	rinit->qi_minfo = minfo;
	winit->qi_minfo = minfo;

	str->s_strinfo = strinfo;

	desc->m_data = str;
	return 0;
}

/*
 * str_load - dynamically load a streams module
 */
static int
str_load (ml_info_t *minfo)
{
	int error = 0;
	cfg_desc_t *desc = minfo->ml_desc;

#ifdef	MTRACE
	printf ("str_load\n");
#endif	/* MTRACE */

	mutex_lock(&fmodsem, PZERO);

	/* do sanity checks */
	if (error = streams_check (minfo)) {
		mutex_unlock(&fmodsem);
		return error;
	}

	/* read the .o file, create a symbol table, and relocate it */
	if (error = mloadfile (minfo))
		goto cleanup;

	/* init kernel driver structs and initialize driver */
	if (error = streams_init (minfo))
		goto cleanup;

	/* TODO - will we need to keep reloctab around for unloading modules 
	 *	that have linked with other loaded modules ??? */

cleanup:

	/* Undo any fmodsw stuff done by streams_check */
	mutex_unlock(&fmodsem);
	if (error) 
		streams_free (desc);
	return error;
}

/*
 * str_unload - unload a streams module from the kernel
 */
static int
str_unload (cfg_desc_t *desc)
{
	cfg_streams_t *str = (cfg_streams_t *)desc->m_data;
	int error;

	/* Most streams modules allocate/free data in their open/close
	 * functions and don't need an unload entrypoint.  However, call
	 * one if it exists
	 */
	if (str->s_unload) {
	    /* Attempt to unload the module.  If its unload function
	     * fails, then bail out.
	     */
	    ASSERT(IS_KSEG0(str->s_unload) || IS_KSEG2(str->s_unload));
	    if (error = (*str->s_unload)())
		    return error;
	}

	/* If the driver has been registered, then leave it registered.
	 */
	if (desc->m_flags & M_REGISTERED) {
		streams_free(desc);	/*XXX open could be rejected here */
		if (streams_regstub(desc))
			desc->m_flags &= ~M_REGISTERED;
	}
	else
		streams_free(desc);

	return 0;
}

/*
 * str_reg - register a streams module to be loaded later
 */
static int
str_reg(ml_info_t *minfo)
{
	int error = 0;
	cfg_desc_t *desc = minfo->ml_desc;

	mutex_lock(&fmodsem, PZERO);

	/* do sanity checks */
	if (error = streams_check (minfo))
		goto out;

	/* connect a stub to the devsw to load the driver on open */
	error = streams_regstub (desc);
out:
	mutex_unlock(&fmodsem);
	return error;
}


/*
 * str_unreg - unregister a streams module to be loaded later
 */
static int
str_unreg(cfg_desc_t *desc)
{
	streams_free(desc);
	return 0;
}

static void
strupdate(cfg_desc_t *desc)
{
	int sp;

	sp = mp_mutex_spinlock(&strlock);
	desc->m_refcnt++;
	/*
	 * If the refcnt is 1 and its a registered module and it has
	 * an unload timeout, then check to see if we should cancel 
	 * the auto-unload timer.
	 */
	if ((desc->m_refcnt == 1) && (desc->m_delay != M_NOUNLD))
		mtimerstop (desc);	
	mp_mutex_spinunlock(&strlock, sp);
}

/*
 * fmhold - increment a refcnt
 *
 * NOTE - called from streams code.
 */
int
fmhold(int fmod)
{
	int sp;
	cfg_desc_t *desc;

	if (fmstatic(fmod))
		return 0;

	sp = mp_mutex_spinlock(&strlock);
	desc = sfindf(fmod);
	if (!desc) {
		mp_mutex_spinunlock(&strlock, sp);
		return ENXIO;
	}
	if (desc->m_flags & M_UNLOADING) {
		mp_mutex_spinunlock(&strlock, sp);
		return EBUSY;
	} else if (desc->m_flags & M_TRANSITION) {
		mp_mutex_spinunlock(&strlock, sp);
		psema(&desc->m_transsema, PZERO);
		strupdate(desc);
		vsema(&desc->m_transsema);
	} else {
		mp_mutex_spinunlock(&strlock, sp);
		strupdate(desc);
	}
	return 0;
}

/*
 * fmrele - decrement a refcnt
 *
 * NOTE - called from streams code.
 */
void
fmrele(int fmod)
{
	int sp;
	cfg_desc_t *desc;

	if (fmstatic(fmod))
		return;

	sp = mp_mutex_spinlock(&strlock);
	desc = sfindf(fmod);
	ASSERT(desc && !(desc->m_flags & M_TRANSITION));
	desc->m_refcnt--;
	/*
	 * If the refcnt is 0 and its a registered module and it has
	 * an unload timeout, then start the auto-unload timer.
	 */
	if ((desc->m_refcnt == 0) && (desc->m_delay != M_NOUNLD) &&
	    (desc->m_flags & M_REGISTERED)) {
		if (desc->m_delay != M_UNLDDEFAULT)
			mtimerstart (desc, desc->m_delay*M_UNLDVAL);
		else if (module_unld_delay*M_UNLDVAL)
			mtimerstart (desc, module_unld_delay*M_UNLDVAL);
	}
	mp_mutex_spinunlock(&strlock, sp);
}

/*
 * mlqdetach - called from qdetach to decrement the refcnt if this
 *	is a loadable module. The refcnt gets incremented in devhold,
 *	via the cdhold call from qattach. We call mlqdetach from qdetach
 *	rather than cdrele because we only have a q to work with.
 *
 */
void
mlqdetach (queue_t *q)
{
	int strsp;
	cfg_desc_t *desc = 0;

	/* 
	 * Since we don't know if this is a streams module or a streams
	 * driver, hold both locks.
	 */
	strsp = mp_mutex_spinlock(&strlock);
	nested_spinlock(&drvlock);
	if (desc = sfindq (q))
		desc->m_refcnt--;
	else if (desc = dfind_minfo(q->q_qinfo->qi_minfo))
		desc->m_refcnt--;
	/*
	 * If the refcnt is 0 and its a registered module and it has
	 * an unload timeout, then start the auto-unload timer.
	 */
	if (desc && (desc->m_refcnt == 0) && (desc->m_delay != M_NOUNLD) &&
	    (desc->m_flags & M_REGISTERED)) {
		if (desc->m_delay != M_UNLDDEFAULT)
			mtimerstart (desc, desc->m_delay*M_UNLDVAL);
		else if (module_unld_delay*M_UNLDVAL)
			mtimerstart (desc, module_unld_delay*M_UNLDVAL);
	}
	nested_spinunlock(&drvlock);
	mp_mutex_spinunlock(&strlock, strsp);
	return;
}

/*
 * END streams support
 *
 * BEGIN file system support
 */

static int  filesys_check (ml_info_t *);
static int  filesys_init (ml_info_t *);

#define	FSYSTBSIZE	32	/* must be a power of 2 */
static cfg_desc_t *fsystab[FSYSTBSIZE];

#define FSYSTBHASHIM(ifsys) ((unsigned)(ifsys) & (FSYSTBSIZE - 1))
#define FSYSTBHASHD(desc) FSYSTBHASHIM(((cfg_fsys_t *)((desc)->m_data))->f_vfs)

static int vfsidbase;	/* unique id base */
static mutex_t fsyssem;	 

/* XXX the following line was cut with the adoption of a vfs maintained
 * lock on the vfssw table. There are possible issues here,  but since all
 * of this _apears_ to be dead code,  the point is moot. -szy
static lock_t fsyslock;
 */


static int
fsys_init(int base)
{
	/* protection for vfssw array */
	mutex_init(&fsyssem, MUTEX_DEFAULT, "mlfmod");
	
/* XXX the following line was cut with the adoption of a vfs maintained
 * lock on the vfssw table. There are possible issues here,  but since all
 * of this _apears_ to be dead code,  the point is moot. -szy
	spinlock_init (&fsyslock, "fsyslock");
 */

	vfsidbase = base;
	return vfsmax;
}

static int
fsys_id(cfg_desc_t *desc)
{
	cfg_fsys_t *fsys = (cfg_fsys_t *)desc->m_data;
	return vfsidbase + fsys->f_vfs;
}

static void
finsert(cfg_desc_t *desc)
{
	u_int hash = FSYSTBHASHD(desc);

	desc->m_next = fsystab[hash];
	fsystab[hash] = desc;
}

/*ARGSUSED*/
static int
fsys_lock (cfg_desc_t *desc)
{
/* XXX The following used to be "return splock(fsyslock);", but was changed
 * to the below with the adoption of a vfs maintained lock on the vfssw table. 
 * It is not clear,  given other users of that lock, that allowing unknown
 * code to grab thse lock with this call is safe,  but since all of this 
 * _apears_ to be dead code,  the point is moot. -szy
 */
    return vfssw_spinlock();	
}

/*ARGSUSED*/
static void
fsys_unlock (cfg_desc_t *desc, int sp)
{
/* XXX The following used to be "spunlock(fsyslock, sp);", see comment above
 * in fsys_lock for details and possible issues.
 */
	vfssw_spinunlock(sp);
	
}

/*ARGSUSED*/
static void 
fsys_copydata(cfg_desc_t *desc, void *ptr)
{
	bcopy(desc->m_data, ptr, sizeof(mod_fsys_t));
}

static int 
fsys_makedata(cfg_desc_t *desc)
{
	cfg_fsys_t *fsys;

	fsys = kern_calloc(1, sizeof(cfg_fsys_t));
	if (copyin(desc->m_data, fsys, sizeof(mod_fsys_t))) {
		kern_free(fsys);
		return MERR_COPY;
	}
	desc->m_data = fsys;

	return 0;
}

static void 
fsys_freedata(cfg_desc_t *desc)
{
	if (desc->m_data) {
		kern_free(desc->m_data);
		desc->m_data = 0;
	}
}

static int
fsys_alloc(void)
{
	int nfsys;
	int s;
	int retval = 0;

	s = vfssw_spinlock();
	for (nfsys=1; nfsys<vfsmax; nfsys++)
		if (vfssw[nfsys].vsw_init == 0 &&
				(vfssw[nfsys].vsw_name == NULL ||
				vfssw[nfsys].vsw_name[0] == '\0')) {
			retval = nfsys;
		   }
		   
	vfssw_spinunlock(s);
	return retval;
}

static void
fsys_free(int i)
{
	int s;
	s = vfssw_spinlock();

	vfssw[i].vsw_init = 0;
	vfssw[i].vsw_name = "";

	vfssw_spinunlock(s);
}

/*
 * filesys_insert - enter the new entry into the vfssw. Hold the 
 * spinlock and update the entire entry at one time.
 */
static void
filesys_insert(cfg_desc_t *desc, struct vfssw *vfsswp)
{
	cfg_fsys_t *fsys = (cfg_fsys_t *) desc->m_data;
	int s;	

#ifdef	MTRACE
	printf ("filesys_insert\n");
#endif	/* MTRACE */

	s = vfssw_spinlock();
	  bcopy (vfsswp, &vfssw[fsys->f_vfs], sizeof(struct vfssw));
	vfssw_spinunlock(s);
	
	finsert(desc);
}

/*
 * filesys_init - hook into kernel data structures and initalize
 */
static int
filesys_init (ml_info_t *minfo)
{
	struct vfssw tmpvfssw, *vfsswp;
	cfg_fsys_t *fsys = (cfg_fsys_t *) minfo->ml_desc->m_data;
	extern struct vnodeops dead_vnodeops;
	int error;

#ifdef	MTRACE
	printf ("filesys_init\n");
#endif	/* MTRACE */

	/* Check for version string. */
	if (error = mversion_chk (minfo))
		return error;

	/*
	 * Create vfssw entry for module and hook routines into vfssw.
	 */
	vfsswp = &tmpvfssw;
	vfsswp->vsw_flag = 0;
	vfsswp->vsw_fill = &dead_vnodeops;
	vfsswp->vsw_name = kern_malloc (FSTYPSZ+1);
	strncpy(vfsswp->vsw_name, fsys->f.vsw_name, FSTYPSZ+1);
	if (mfindname (minfo, "init", (long *)&vfsswp->vsw_init))
		return MERR_NOINIT;
	if (mfindname (minfo, "vfsops", (long *)&vfsswp->vsw_vfsops))
		return MERR_NOVFSOPS;
	if (mfindname (minfo, "vnodeops", (long *)&vfsswp->vsw_vnodeops))
		return MERR_NOVNODEOPS;
	if (mfindname (minfo, "unload", (long *)&fsys->d_unload))
		fsys->d_unload = 0;

	/* Call the init routine. */
	(*vfsswp->vsw_init)(vfsswp, fsys->f_vfs);

	/* Insert the vfs entry into the switch */
	filesys_insert(minfo->ml_desc, vfsswp);

	return 0;
}

/*
 *
 * filesys_check - sanity checks before loading the file system module
 *	allocate vfssw entry (fs->f_vfs)
 */
static int
filesys_check (ml_info_t *minfo)
{
	cfg_fsys_t *fsys = (cfg_fsys_t *)minfo->ml_desc->m_data;
	int nfsys;
	int s;
	
	/* Reject entry without a name */
	if (!fsys->f.vsw_name[0])
		return MERR_NOFSYSNAME;

	/* Check to see if a fsys with same name is installed */
	s = vfssw_spinlock();
	  for (nfsys=1; nfsys<vfsmax; nfsys++)
		if (strcmp (vfssw[nfsys].vsw_name, fsys->f.vsw_name) == 0) {
			vfssw_spinunlock(s);
			return MERR_DUPFSYS;
		}
	vfssw_spinunlock(s);
	
	/* Get an entry in the vfssw table */
	if (!(fsys->f_vfs = fsys_alloc()))
		return MERR_SWTCHFULL;
	return 0;
}

/*
 * fsys_load - dynamically load a file system
 */
static int
fsys_load (ml_info_t *minfo)
{
	int error = 0;

#ifdef	MTRACE
	printf ("fsys_load\n");
#endif

	mutex_lock(&fsyssem, PZERO);

	/* do sanity checks */
	if (error = filesys_check (minfo))
		goto cleanup;

	/* read the .o file, create a symbol table, and relocate it */
	if (error = mloadfile (minfo))
		goto cleanup;

	/* init kernel driver structs and initialize driver */
	if (error = filesys_init (minfo))
		goto cleanup;

cleanup:
	mutex_unlock(&fsyssem);
	return error;
}

/*
 * fsys_unload - unload a file system from the kernel
 */
/*ARGSUSED*/
static int
fsys_unload (cfg_desc_t *desc)
{
	cfg_fsys_t *fsys = (cfg_fsys_t *)desc->m_data;
	int (*unload)(void);
	struct vfs *vfsp;
	bhv_desc_t *bdp;
	int error;

	unload = fsys->d_unload;
	if (!unload)
		return (MERR_NOUNLD);

	for (vfsp = rootvfs; vfsp != NULL; vfsp = vfsp->vfs_next) {
		bdp = bhv_lookup_unlocked(VFS_BHVHEAD(vfsp), 
					vfssw[fsys->f_vfs].vsw_vfsops);
		if (bdp)
			return (MERR_UNLDFAIL);
	}

	ASSERT(IS_KSEG0(unload) || IS_KSEG2(unload));
	error = (*unload)();
	if (error)
		return (MERR_UNLDFAIL);

	fsys_free(fsys->f_vfs);

	return (0);
}

/*
 * fsys_reg - register a file system to be loaded later
 */
/*ARGSUSED*/
static int
fsys_reg(ml_info_t *minfo)
{
	return MERR_UNSUPPORTED;
}

/*
 * fsys_unreg - unregister a file system to be loaded later
 */
/*ARGSUSED*/
static int
fsys_unreg(cfg_desc_t *desc)
{
	return MERR_UNSUPPORTED;
}

/*
 * END file system support
 *
 * BEGIN idbg support
 */

static ml_info_t *current_idbg;
static int idbgidbase;
#define NIDBGS	20	/* number of diff xxxidbg modules that can be loaded */
static int idbgids[NIDBGS];
static mutex_t idbgsem;

static int
idbg_init(int base)
{
	int i;

	current_idbg = 0;
	idbgidbase = base;
	for (i=0; i<NIDBGS; i++)
		idbgids[i] = 0;
	mutex_init(&idbgsem, MUTEX_DEFAULT, "mlidbg");
	return 1;
}

/*ARGSUSED*/
static int
idbg_id(cfg_desc_t *desc)
{
	return desc->m_id;
}

/*ARGSUSED*/
static int
idbg_lock (cfg_desc_t *desc)
{
	return 0;
}

/*ARGSUSED*/
static void
idbg_unlock (cfg_desc_t *desc, int sp)
{
	return;
}

/*ARGSUSED*/
static void 
idbg_copydata(cfg_desc_t *desc, void *ptr)
{
	return;
}

static int 
idbg_makedata(cfg_desc_t *desc)
{
	cfg_idbg_t *idbg;

	idbg = kern_calloc(1, sizeof(cfg_idbg_t));
	if (copyin(desc->m_data, idbg, sizeof(mod_idbg_t))) {
		kern_free(idbg);
		return MERR_COPY;
	}
	idbg->i_func = kern_calloc(1, sizeof(idbgfunc_t));
	desc->m_data = idbg;

	return 0;
}

static void 
idbg_freedata(cfg_desc_t *desc)
{
	if (desc->m_data) {
		cfg_idbg_t *idbg = desc->m_data;
		if (idbg->i_func)
			kern_free(idbg->i_func);
		kern_free(desc->m_data);
		desc->m_data = 0;
	}
}

/*
 * debug_init - hook into kernel data structures and initialize
 */
static int
debug_init (ml_info_t *minfo)
{
	cfg_idbg_t *idbg = minfo->ml_desc->m_data;
	idbgfunc_t *funcs = idbg->i_func;

	/* 
	 * If we're loading idbg.o, we need to replace the stubs and do
	 * some inititalization. If we're loading an xxxidbg.o module,
	 * just load it and call its init routine.
	 */

	if (minfo == current_idbg) {

		/* Get the idbg routines. 
		 */
		if (mfindexactname (minfo, "_idbg_setup", (long *)&funcs->setup))
			return MERR_FINDNAME;
		if (mfindexactname (minfo, "_idbg_copytab", (long *)&funcs->copytab))
			return MERR_FINDNAME;
		if (mfindexactname (minfo, "_idbg_tablesize", (long *)&funcs->tablesize))
			return MERR_FINDNAME;
		if (mfindexactname (minfo, "_idbg_dofunc", (long *)&funcs->dofunc))
			return MERR_FINDNAME;
		if (mfindexactname (minfo, "_idbg_error", (long *)&funcs->error))
			return MERR_FINDNAME;
		if (mfindexactname (minfo, "_idbg_switch", (long *)&funcs->iswitch))
			return MERR_FINDNAME;
		if (mfindexactname (minfo, "_idbg_addfunc", (long *)&funcs->addfunc))
			return MERR_FINDNAME;
		if (mfindexactname (minfo, "_idbg_delfunc", (long *)&funcs->delfunc))
			return MERR_FINDNAME;
		if (mfindexactname (minfo, "_qprintf", (long *)&funcs->qprintf))
			return MERR_FINDNAME;
		if (mfindexactname (minfo, "__prvn", (long *)&funcs->prvn))
			return MERR_FINDNAME;
#if IP20 || IP22 || IP26 || IP28 || EVEREST
		if (mfindexactname (minfo, "_idbg_wd93", (long *)&funcs->wd93))
			return MERR_FINDNAME;
#endif
		if (mfindexactname (minfo, "_idbg_late_setup", (long *)&funcs->late_setup))
			return MERR_FINDNAME;
		if (mfindexactname (minfo, "_idbg_unload", (long *)&funcs->unload))
			return MERR_FINDNAME;
		if (mfindexactname (minfo, "_idbg_addfssw", (long *)&funcs->addfssw))
			return MERR_FINDNAME;
		if (mfindexactname (minfo, "_idbg_delfssw", (long *)&funcs->delfssw))
			return MERR_FINDNAME;
		if (mfindexactname (minfo, "_printflags", (long *)&funcs->printflags))
			return MERR_FINDNAME;
		if (mfindexactname (minfo, "_padstr", (long *)&funcs->padstr))
			return MERR_FINDNAME;
		if (mfindexactname (minfo, "idbg_mrlock", (long *)&funcs->pmrlock))
			return MERR_FINDNAME;
		if (mfindexactname (minfo, "_prsymoff", (long *)&funcs->prsymoff))
			return MERR_FINDNAME;
	
		symtab_hold();				/* bump rtsymtab refcnt */
		funcs->setup();
		funcs->late_setup();
		idbg->i_oldfunc = idbgfuncs;		/* save old table */
		idbgfuncs = funcs;			/* set new table */

	/*
	 * We're loading an xxxidbg.o module, so just bump the idbg module's
	 * refcnt and call xxxidbg's init routine.
	 */

	} else {
		void (*func)(void) = 0;

		current_idbg->ml_desc->m_refcnt++;
		if (mfindname (minfo, "init", (long *)&func) == 0)
			(*func)();
	}

	return 0;
}

static void
debug_freeid (ml_info_t *minfo)
{
	if (minfo == current_idbg)
		minfo->ml_desc->m_id = idbgids[0] = 0;
	else
		idbgids[minfo->ml_desc->m_id - idbgidbase] = 0;
}

/*
 * debug_check - do sanity checks
 */
static int
debug_check (ml_info_t *minfo)
{
	int i;

	if (strcmp(minfo->ml_desc->m_prefix, "idbg") == 0) {
		if (current_idbg != 0 || 		/* already loaded */
			idbgfuncs != &idbgdefault ||	/* linked in */
			!idbgfuncs->stubs) {		/* stubs are not linked */
			return MERR_IDBG;
		}
		minfo->ml_desc->m_id = idbgids[0] = idbgidbase;
		current_idbg = minfo;
	} else {
		/* idbg.o must be loaded first */
		if (!current_idbg)
			return MERR_NOIDBG;
		for (i=0; i<NIDBGS; i++)
			if (idbgids[i] == 0)
				break;
		if (i == NIDBGS)
			return MERR_NOLIBIDS;
		minfo->ml_desc->m_id = idbgids[i] = idbgidbase + i;
	}

	return 0;
}

/*
 * idbg_load - dynamically load the idbg module
 */
static int
idbg_load (ml_info_t *minfo)
{
	int error = 0;

	mutex_lock(&idbgsem, PZERO);

	/* do sanity checks */
	if (error = debug_check (minfo))
		goto cleanup;

	/* read the .o file, create a symbol table, and relocate it */
	if (error = mloadfile (minfo))
		goto cleanup;

	/* init kernel driver structs and initialize driver */
	if (error = debug_init (minfo))
		goto cleanup;

cleanup:
	if (error) {
		debug_freeid (minfo);
		if (current_idbg == minfo->ml_desc->m_info)
			current_idbg = 0;
	}
		
	mutex_unlock(&idbgsem);
	return error;
}

/*
 * idbg_unload - unload the idbg module
 */
static int
idbg_unload (cfg_desc_t *desc)
{
	cfg_idbg_t *idbg = (cfg_idbg_t *)desc->m_data;
	int (*unload)(void);


	mutex_lock(&idbgsem, PZERO);

	if (current_idbg == desc->m_info) {
		unload = idbg->i_func->unload;
		if (IS_KSEG0(unload) || IS_KSEG2(unload)){
			if ((*unload)()) {
				mutex_unlock(&idbgsem);
				return MERR_UNLDFAIL;
			}
		}

		idbgfuncs = idbg->i_oldfunc;	/* restore old table */
		current_idbg = 0;
		idbgids[0] = 0;
		symtab_rele();			/* decrement rtsymtab refcnt */
	} else {
                if (mfindname (desc->m_info, "unload", (long *)&unload) == 0) {
                        (*unload)();
			current_idbg->ml_desc->m_refcnt--;
			idbgids[desc->m_id - idbgidbase] = 0;
		} else {
			mutex_unlock(&idbgsem);
			return MERR_UNLDFAIL;
		}
	}

	mutex_unlock(&idbgsem);
	return 0;
}

/*
 * idbg_reg - register a module to be loaded later
 */
/*ARGSUSED*/
static int
idbg_reg(ml_info_t *minfo)
{
	return MERR_IDBGREG;
}

/*
 * idbg_unreg - unregister a module to be loaded later
 */
/*ARGSUSED*/
static int
idbg_unreg(cfg_desc_t *desc)
{
	return MERR_IDBGREG;
}

/*
 * END idbg support
 *
 * BEGIN library support
 *
 * A library is just a module of routines/data that other modules
 * can link against. If st_findaddr doesn't find a symbol in the kernel
 * symbol table, it looks thru the list of minfo structs of type M_LIB.
 * A library module can not be unloaded, registered or unregistered.
 *
 */

#define	NLIBS	10	/* number of different libraries that can be loaded */
static int libidbase;
static int libids[NLIBS];
static mutex_t libsem;

static int
lib_init(int base)
{
	int i;

	libidbase = base;
	for (i=0; i<NLIBS; i++)
		libids[i] = 0;
	mutex_init(&libsem, MUTEX_DEFAULT, "mllib");
	return 1;
}

static int
lib_id(cfg_desc_t *desc)
{
	cfg_lib_t *lib = (cfg_lib_t *)desc->m_data;
	return lib->l.id;

}

/*ARGSUSED*/
static int
lib_lock (cfg_desc_t *desc)
{
	return 0;
}

/*ARGSUSED*/
static void
lib_unlock (cfg_desc_t *desc, int sp)
{
	return;
}

/*ARGSUSED*/
static void 
lib_copydata(cfg_desc_t *desc, void *ptr)
{
	return;
}

static int 
lib_makedata(cfg_desc_t *desc)
{
	cfg_lib_t *lib;

	lib = kern_calloc(1, sizeof(cfg_lib_t));
	if (copyin(desc->m_data, lib, sizeof(mod_lib_t))) {
		kern_free(lib);
		return MERR_COPY;
	}
	desc->m_data = lib;

	return 0;
}

static void 
lib_freedata(cfg_desc_t *desc)
{
	if (desc->m_data) {
		kern_free(desc->m_data);
		desc->m_data = 0;
	}
}

/*
 * library_init - hook into kernel data structures and initialize
 */
/*ARGSUSED*/
static int
library_init (ml_info_t *minfo)
{
	void (*func)(void) = 0;

	/* Call the library's init function, if it has one. */

	if (mfindname (minfo, "init", (long *)&func) == 0)
		(*func)();

	return 0;
}

/*
 * library_check - do sanity checks
 */
static int
library_check (ml_info_t *minfo)
{
	int i;
	cfg_lib_t *lib = (cfg_lib_t *)minfo->ml_desc->m_data;

	for (i=0; i<NLIBS; i++)
		if (libids[i] == 0)
			break;
	if (i == NLIBS) {
		return MERR_NOLIBIDS;
	}
	lib->l.id = libids[i] = libidbase + i;
	return 0;
}

/*
 * lib_load - dynamically load a libary module
 */
static int
lib_load (ml_info_t *minfo)
{
	int error = 0;

#ifdef	MTRACE
	printf ("lib_load\n");
#endif
	mutex_lock(&libsem, PZERO);

	/* do sanity checks */
	if (error = library_check (minfo))
		goto cleanup;

	/* read the .o file, create a symbol table, and relocate it */
	if (error = mloadfile (minfo))
		goto cleanup;

	/* init kernel driver structs and initialize driver */
	if (error = library_init (minfo))
		goto cleanup;

cleanup:
	mutex_unlock(&libsem);
	return error;
}

/*
 * lib_reg - register a library to be loaded later
 */
/*ARGSUSED*/
static int
lib_reg(ml_info_t *minfo)
{
	return MERR_LIBREG;	/* libs can't be registered */
}

/*
 * lib_unreg - unregister a library to be loaded later
 */
/*ARGSUSED*/
static int
lib_unreg(cfg_desc_t *desc)
{
	return MERR_LIBREG;	/* libs can't be unregistered */
}

/*
 * lib_unload - unload the lib module
 */
/*ARGSUSED*/
static int
lib_unload (cfg_desc_t *desc)
{
	return MERR_LIBUNLD;	/* libs can't be unloaded */
}

/*
 * END library support
 *
 * BEGIN symtab support
 *
 */

#define	NSYMTABS	40	/* number of different symtabs that can be loaded */
static int symtabidbase;
static int symtabids[NSYMTABS];
static lock_t symtablock;
static sema_t symtabsem;

static int
symtab_init(int base)
{
	int i;

	symtabidbase = base;
	for (i=0; i<NSYMTABS; i++)
		symtabids[i] = 0;
	initnsema_mutex(&symtabsem, "mlsymtab");
	return 1;
}

static int
symtab_id(cfg_desc_t *desc)
{
	cfg_symtab_t *symtab = (cfg_symtab_t *)desc->m_data;

	return symtab->s.id;

}

static int
symtab_allocid (void)
{
	int i;

	for (i=0; i<NSYMTABS; i++)
		if (symtabids[i] == 0)
			break;
	if (i == NSYMTABS) {
		return 0;
	}
	symtabids[i] = i + symtabidbase;
	return (symtabids[i]);
}

static void
symtab_freeid (int id)
{
	int i;

	for (i=0; i < NSYMTABS; i++) {
		if (symtabids[i] == id) {
			symtabids[i] = 0;
		}
	}
}

/*ARGSUSED*/
static int
symtab_lock (cfg_desc_t *desc)
{
	return mp_mutex_spinlock(&symtablock);
}

/*ARGSUSED*/
static void
symtab_unlock (cfg_desc_t *desc, int sp)
{
	mp_mutex_spinunlock(&symtablock, sp);
}

/*ARGSUSED*/
static void 
symtab_copydata(cfg_desc_t *desc, void *ptr)
{
	return;
}

static int 
symtab_makedata(cfg_desc_t *desc)
{
	cfg_symtab_t *symtab;

	symtab = kern_calloc(1, sizeof(cfg_symtab_t));
	if (copyin(desc->m_data, symtab, sizeof(mod_symtab_t))) {
		kern_free(symtab);
		return MERR_COPY;
	}
	desc->m_data = symtab;
	return 0;
}

static void 
symtab_freedata(cfg_desc_t *desc)
{
	if (desc->m_data) {
		kern_free(desc->m_data);
		desc->m_data = 0;
	}
}

/*
 * symboltab_check - do sanity checks
 */
static int
symboltab_check (ml_info_t *minfo)
{
	ml_info_t *m;
	int id;
	cfg_symtab_t *symtab = (cfg_symtab_t *)minfo->ml_desc->m_data;

	if (!(id = symtab_allocid()))
		return MERR_NOSYMTABIDS;
	symtab->s.id = id;

	/* make sure we don't have a symtab for this file already */
	for (m = mlinfolist; m ; m = m->ml_next) {
		if (m->ml_desc->m_type == M_SYMTAB && m != minfo &&
		    !strcmp(m->ml_desc->m_fname, minfo->ml_desc->m_fname)) {
			return MERR_DUPSYMTAB;
		}

		/* can only have one unix symtab at a time */
		if (m->ml_desc->m_type == M_SYMTAB && m != minfo &&
		    ((symtab->s.type == M_UNIXSYMTAB) && 
		     (mlunixsymtab || rtsymmod)))
			return MERR_DUPSYMTAB;
	}

	return 0;
}

/*
 * symtab_load - dynamically load a symtab module
 */
static int
symtab_load (ml_info_t *minfo)
{
	int error = 0;
	cfg_desc_t *desc = (cfg_desc_t *)minfo->ml_desc;
	cfg_symtab_t *symtab = (cfg_symtab_t *)minfo->ml_desc->m_data;

	psema(&symtabsem, PZERO);

	/* do sanity checks */
	if (error = symboltab_check (minfo)) {
		goto cleanup;
	}

	/* read the .o file, create a symbol table, and relocate it */
	if (error = mreadfile (minfo, 1)) {
		goto cleanup;
	}

	/* if this is a unix symtab, check that it matches our kernel image */
	if (symtab->s.type == M_UNIXSYMTAB) {
		if (check_rtsyms(minfo)) {
			error = MERR_SYMTABMISMATCH;
			goto cleanup;
		}
		mlunixsymtab = 1;
	}

	
	/* set up local symbol table for the module */
	if (error = msetelfsym (minfo, 1)) {
		goto cleanup;
	}

	/* set timer going on autounload if needed */
	if (desc->m_delay != M_NOUNLD) {
		mtimerstart(desc, desc->m_delay * M_UNLDVAL);
	}
cleanup:
	if (error) {
		if (symtab->s.id) {
			symtab_freeid (symtab->s.id);
			symtab->s.id = 0;
		}
	}
	vsema(&symtabsem);
	mfreeelfobj(minfo);
	return error;
}

/*
 * symtab_reg - register a symtab to be loaded later
 */
/*ARGSUSED*/
static int
symtab_reg(ml_info_t *minfo)
{
	return MERR_SYMTABREG;	/* symtabs can't be registered */
}

/*
 * symtab_unreg - unregister a symtab to be loaded later
 */
/*ARGSUSED*/
static int
symtab_unreg(cfg_desc_t *desc)
{
	return MERR_SYMTABREG;	/* symtabs can't be unregistered */
}

/*
 * symtab_unload - unload the symtab module
 */
/*ARGSUSED*/
static int
symtab_unload (cfg_desc_t *desc)
{
	int i;
	cfg_symtab_t *symtab = (cfg_symtab_t *)desc->m_data;

	for (i=0; i < NSYMTABS; i++) {
		if (symtabids[i] == symtab->s.id) {
			symtabids[i] = 0;
		}
	}

	if (desc->m_info == rtsymmod)
		rtsymmod = 0;
	if (symtab->s.type == M_UNIXSYMTAB) {
		mlunixsymtab = 0;
	}

	return 0;
}

static int
symtab_hold (void)
{
	int sp; 
	cfg_desc_t *desc;

	if (rtsymmod) {
		sp = mp_mutex_spinlock(&symtablock);
		desc = rtsymmod->ml_desc;
	} else
		return 0;

	if (desc->m_flags & (M_UNLOADING|M_TRANSITION)) {
		mp_mutex_spinunlock(&symtablock, sp);
		return EBUSY;
	}

	desc->m_refcnt++;
	mp_mutex_spinunlock(&symtablock, sp);
	return 0;
}

static int
symtab_rele (void)
{
	int sp = mp_mutex_spinlock(&symtablock);

	if (rtsymmod)
		rtsymmod->ml_desc->m_refcnt--;
	mp_mutex_spinunlock(&symtablock, sp);
	return 0;
}

/*
 * END symtab support
 *
 * BEGIN ethernet support
 *
 * An ethernet driver is different enough from char/block/streams
 * drivers to justify having its own module type.
 *
 */

#define	NENET	10	/* number of diff enet modules that can be loaded */
static int enetidbase;
static int enetids[NLIBS];
static mutex_t enetsem;

static int
enet_init(int base)
{
	int i;

	enetidbase = base;
	for (i=0; i<NENET; i++)
		enetids[i] = 0;
	mutex_init(&enetsem, MUTEX_DEFAULT, "mlenet");
	return 1;
}

static int
enet_id(cfg_desc_t *desc)
{
	cfg_enet_t *enet = (cfg_enet_t *)desc->m_data;
	return enet->e.id;

}

/*ARGSUSED*/
static int
enet_lock (cfg_desc_t *desc)
{
	return 0;
}

/*ARGSUSED*/
static void
enet_unlock (cfg_desc_t *desc, int sp)
{
	return;
}

/*ARGSUSED*/
static void 
enet_copydata(cfg_desc_t *desc, void *ptr)
{
	return;
}

static int 
enet_makedata(cfg_desc_t *desc)
{
	cfg_enet_t *enet;
	medt_t *medtpfrom, *medtpto;
	edt_t *edtp;
	int error;

	enet = kern_calloc(1, sizeof(cfg_enet_t));
	if (copyin(desc->m_data, enet, sizeof(mod_enet_t))) {
		kern_free(enet);
		return MERR_COPY;
	}

	if (medtpfrom = enet->e_edt) {
		medtpto = kern_calloc(1, sizeof(medt_t));
		if (error = makeedtdata (medtpfrom, medtpto)) {
			enet_freedata(desc);
			return error;
		}
		enet->e_edt = medtpto;
	}
	else {
		/*
		 * Create a dummy edt
		 */
		medtpto = kern_calloc(1, sizeof(medt_t));
		medtpto->edtp = edtp = kern_calloc (1, sizeof (edt_t));

		edtp->e_bus_type = ADAP_PCI;
		edtp->v_cpuintr = 0;
		edtp->e_adap = 0;
		edtp->e_ctlr = 0;

		medtpto->medt_next = NULL;
		enet->e_edt = medtpto;
	}
	desc->m_data = enet;
	return 0;
}

static void
enet_freedata(cfg_desc_t *desc)
{
	cfg_enet_t *enet = desc->m_data;

	if (enet) {
		free_edt(enet->e_edt);
		kern_free (enet);
		desc->m_data = 0;
	}
}

static void
free_edt (medt_t *medt)
{
	medt_t *lmedt;

	while (medt) {
		if (medt->edtp->e_bus_type==ADAP_VME &&
			medt->edtp->e_bus_info) {
			kern_free (medt->edtp->e_bus_info);
		}
		kern_free (medt->edtp);
		lmedt = medt;
		medt = medt->medt_next;
		kern_free (lmedt);
	}
}

/*
 * ethernet_init - initialize and call ethernet edtinit
 */
/*ARGSUSED*/
static int
ethernet_init (ml_info_t *minfo)
{
	cfg_enet_t *enet = minfo->ml_desc->m_data;
	int error;

#ifdef	MTRACE
	printf ("ethernet_init\n");
#endif	/* MTRACE */

	/* Check for version string. */
	if (error = mversion_chk (minfo))
		return error;

	/* 
	 * Get the unload routine if there is one. 
	 */
	if (mfindname (minfo, "unload", (long *)&enet->d_unload))
		enet->d_unload = 0;

	error = medtinit (minfo, enet->e_edt);
	return (error);
}

/*
 * ethernet_check - do sanity checks
 */
static int
ethernet_check (ml_info_t *minfo)
{
	int i;
	cfg_enet_t *enet = (cfg_enet_t *)minfo->ml_desc->m_data;

	for (i=0; i<NENET; i++)
		if (enetids[i] == 0)
			break;
	if (i == NENET)
		return MERR_NOENETIDS;
	enet->e.id = enetids[i] = enetidbase + i;
	return 0;
}

/*
 * enet_load - dynamically load an ethernet module
 */
static int
enet_load (ml_info_t *minfo)
{
	int error = 0;

#ifdef	MTRACE
	printf ("enet_load\n");
#endif
	mutex_lock(&enetsem, PZERO);

	/* do sanity checks */
	if (error = ethernet_check (minfo))
		goto cleanup;

	/* read the .o file, create a symbol table, and relocate it */
	if (error = mloadfile (minfo))
		goto cleanup;

	/* init kernel driver structs and initialize driver */
	if (error = ethernet_init (minfo))
		goto cleanup;

	mutex_unlock(&enetsem);
	return (error);

cleanup:
	enetids[((cfg_enet_t *)minfo->ml_desc->m_data)->e.id - enetidbase] = 0;
	mutex_unlock(&enetsem);
	return error;
}

/*
 * enet_reg - register an ethernet module to be loaded later
 */
/*ARGSUSED*/
static int
enet_reg(ml_info_t *minfo)
{
	return MERR_ENETREG;	/* enet modules can't be registered */
}

/*
 * enet_unreg - unregister an ethernet module
 */
/*ARGSUSED*/
static int
enet_unreg(cfg_desc_t *desc)
{
	return MERR_ENETUNREG;	/* enet modules can't be unregistered */
}

/*
 * enet_unload - unload the enet module
 */
/*ARGSUSED*/
static int
enet_unload (cfg_desc_t *desc)
{
	cfg_enet_t *enet = (cfg_enet_t *)desc->m_data;
	int (*unload)(void);
	int error;

	unload = enet->d_unload;
	if (!unload)
		return (MERR_NOUNLD);

	ASSERT(IS_KSEG0(unload) || IS_KSEG2(unload));
	error = (*unload)();
	if (error)
		return (MERR_UNLDFAIL);

	enetids[enet->e.id - enetidbase] = 0;

	return (0);
}

/*
 * END ethernet support
 */


/**************************************************************************/


/*
 * mreloc - load and relocate a module
 */
static int
mreloc (ml_info_t *minfo)
{
	return (doelfrelocs (minfo));
}

/*
 * mloadfile - read .o file, create symbol table, and relocate
 */
static int
mloadfile (ml_info_t *minfo)
{
	int error;

#ifdef	MTRACE
	printf ("mloadfile\n");
#endif	/* MTRACE */

	/* read the .o file */
	if (error = mreadfile (minfo, 0))
		goto cleanup;

	/* set up local symbol table for the module */
	if (error = msetelfsym (minfo, 0))
		goto cleanup;

	/* relocate and link in module */
	if (error = mreloc (minfo))
		goto cleanup;

cleanup:
	mfreeelfobj (minfo);

	return error;
}

static void
munloadfile (ml_info_t *minfo)
{
	if (minfo->ml_base)
		kern_free (minfo->ml_base);
	if (minfo->ml_symtab)
		kern_free (minfo->ml_symtab);
	if (minfo->ml_stringtab)
		kern_free (minfo->ml_stringtab);

	minfo->ml_base = NULL;
	minfo->ml_symtab = NULL;
	minfo->ml_stringtab = NULL;
	minfo->ml_nsyms = 0;
	minfo->ml_strtabsz = 0;
}

/*
 * mreadfile - read a .o
 */
static int
mreadfile (ml_info_t *minfo, int symtab)
{
	int tmperror, error = 0;
	vnode_t *vp, *openvp;

#ifdef	MTRACE
	mprintf (("mreadfile %s &fname=0x%x\n", minfo->ml_desc->m_fname,
		&minfo->ml_desc->m_fname));
#endif	/* MTRACE */

	/* Find and read the object file
	 */
	if (lookupname (minfo->ml_desc->m_fname, UIO_SYSSPACE,
	    FOLLOW, NULLVPP, &vp, NULL))
		return MERR_ENOENT;

	if (vp->v_type != VREG)
		return MERR_VREG;

	openvp = vp;
	VOP_OPEN(openvp, &vp, FREAD, get_current_cred(), error);
	if (error) {
		error = MERR_VOP_OPEN;
		goto close_vp;
	}

	if (symtab) {
		if ((error = mreadelfsymtab (minfo, vp)) == MERR_BADMAGIC) {
			mfreeelfobj (minfo);
		}
	} else {
		if ((error = mreadelf (minfo, vp)) == MERR_BADMAGIC) {
			mfreeelfobj (minfo);
		}
	}

close_vp:
	VOP_CLOSE(vp, FREAD, L_TRUE, get_current_cred(), tmperror);
	if (tmperror && !error)
		error = tmperror;
	VN_RELE(vp);
	return (error);
}

/*
 * makeminfo - create a descriptor for a new module and put it on 
 * 	      the module list.
 */
static int
makeminfo (cfg_desc_t *udesc, ml_info_t **m) 
{
	ml_info_t *minfo, *newinfo = 0;
	cfg_desc_t *newdesc = 0;
	char *tmpfname = 0, *ufname;
	int error;
	size_t count;
	
#ifdef	MTRACE
	printf ("makeminfo\n");
#endif	/* MTRACE */

	/* malloc a descriptor struct and copyin */
	newdesc = kern_calloc (1, sizeof(cfg_desc_t));

	if (copyin(udesc, newdesc, sizeof(cfg_desc_t))) {
		error = MERR_COPY;
		goto out;
	}

	newdesc->m_id = -1;
	newdesc->m_info = 0;
	newdesc->m_refcnt = 0;
	newdesc->m_next = 0;

	ufname = newdesc->m_fname;
	newdesc->m_fname = 0;
	newdesc->m_fnamelen = 0;

	/* validate info provided by user */
	if (!newdesc->m_prefix[0]) {
		error = MERR_NOPREFIX;
		goto out;
	} else if (!newdesc->m_type) {
		error = MERR_NOMODTYPE;
		goto out;
	} else if (newdesc->m_type > mltypes) {
		error = MERR_BADMODTYPE;
		goto out;
	}

	if (newdesc->m_cfg_version != M_CFG_CURRENT_VERSION) {
		error = MERR_BADCFGVERSION;
		goto out;
	}

	tmpfname = kern_malloc(MAXPATHLEN);
	if (error = copyinstr(ufname, tmpfname, MAXPATHLEN, &count))
		goto out;

	newdesc->m_fname = kern_malloc (count + 1);
	strcpy (newdesc->m_fname, tmpfname);
	newdesc->m_fnamelen = count;

	kern_free (tmpfname);
	tmpfname = 0;		/* for cleanup */

	/* allocate an ml_info struct for this module */
	newinfo = kern_calloc (1, sizeof(ml_info_t));
	newinfo->ml_desc = newdesc;
	newdesc->m_flags |= M_TRANSITION;
	newdesc->m_info = newinfo;

	error = ML_MAKEDATA(newdesc);
	if (error)
		goto out;

	psema(&mlinfosem, PZERO);

	minfo = mlinfolist;
	if (minfo) {
		while (minfo->ml_next)
			minfo = minfo->ml_next;
		minfo->ml_next = newinfo;
		minfo = minfo->ml_next;
	} else {
		minfo = newinfo;
		mlinfolist = minfo;
	}
	minfo->ml_next = 0;

	vsema(&mlinfosem);

	*m = minfo;
	return 0;

out:
	if (newdesc) {
		if (newdesc->m_fname)
			kern_free(newdesc->m_fname);
		kern_free(newdesc);
	}
	if (tmpfname)
		kern_free(tmpfname);
	*m = 0;
	return error;
}

/*
 * freeminfo - find minfo struct in module list and free it
 */
static void
freeminfo (ml_info_t *m)
{
	ml_info_t *minfo, *mlast;

	ASSERT(m);

	psema(&mlinfosem, PZERO);

	minfo = mlast = mlinfolist;
	while (minfo && (minfo != m)) {
		mlast = minfo;
		minfo = minfo->ml_next;
	}
	ASSERT(minfo);

	if (mlast == m)		/* first element */
		mlinfolist = m->ml_next;
	else
		mlast->ml_next = m->ml_next;

	vsema(&mlinfosem);

	/* free up all allocated data */
	if (m->ml_desc) {
		if (m->ml_desc->m_fname) {
			kern_free (m->ml_desc->m_fname);
		}
		ML_FREEDATA(m->ml_desc);
		kern_free (m->ml_desc);
		m->ml_desc = 0;
	}
	munloadfile(m);
	kern_free (m);
}

/*
 * mfindname - look for the function name, <prefix><func> in the symbol
 * 	table.
 */
static int
mfindname (ml_info_t *minfo, char *name, long *entry)
{
	char func[M_PREFIXLEN+20]; /* XXX */

	strcpy (func, minfo->ml_desc->m_prefix);
	strcat (func, name);
	return mfindexactname(minfo, func, entry);
}

static int
mfindexactname(ml_info_t *minfo, char *name, long *entry)
{
	return melf_findexactname (minfo, name, entry);
}

/*
 * mversion_chk - look for <prefix>mversion in loaded module and
 *	check version.
 */
static int
mversion_chk (ml_info_t *minfo)
{
	long *vers;
	char *str;

	if (mfindname (minfo, "mversion", (long *)&vers))
		return MERR_NOVERSION;
	str = (char *) *vers;

	if (strcmp (mversion, str) == 0)
		return 0;
	return MERR_BADVERSION;
}

/*
 * mload_rtsyms()
 *
 * This routine is responsible for setting up the run-time
 * symbol table.  We basically emulate the work of the ml
 * command and its related kernel interfaces (e.g. mload()).
 *
 * Return 0 on sucess, MERR_* value on failure.
 */
static int
mload_rtsyms (ml_info_t **m)
{
	cfg_desc_t *desc;
	ml_info_t *minfo, *mp;
	mod_symtab_t *symtab;
	int rc = 0;

	/*
	 * If there isn't a runtime symbol table available (we bootp'ed
	 * a kernel or the kernel we booted isn't there any more, etc.,
	 * return an error.
	 */
	if (!unixsymtab_avail)
		return (MERR_NOSYMTABAVAIL);

	/* allocate requisite space */
	desc = kern_calloc(1, sizeof (*desc));
	minfo = kern_calloc(1, sizeof (*minfo));
	symtab = kern_calloc(1, sizeof (*symtab));
	bzero(minfo, sizeof (*minfo));
	bzero(desc, sizeof (*desc));

	/* initialize cfg_desc_t information */
	desc->m_cfg_version = M_CFG_CURRENT_VERSION;
	desc->m_alpha_cfg_version = M_ALPHA_CFG_CURRENT_VERSION;
	desc->m_fnamelen = strlen(kernname) + 1;
	desc->m_fname = kern_malloc(desc->m_fnamelen);

	/* initialize mod_symtab_t */
	symtab->id = 0;
	strcpy(symtab->symtab_name, 
		((desc->m_fnamelen <= sizeof(symtab->symtab_name))
		? kernname : "/unix"));
	symtab->type = M_UNIXSYMTAB;

	strcpy(desc->m_fname, kernname);
	desc->m_data = (void *)symtab;
	strcpy(desc->m_prefix, "symtab");
	desc->m_type = M_SYMTAB;
	desc->m_delay = module_unld_delay;	/* XXX default */
	desc->m_id = -1;

	/* initialize ml_info_t information */
	minfo->ml_desc = desc;
	desc->m_flags |= M_TRANSITION;
	desc->m_info = minfo;

	/* now link into list */
	psema(&mlinfosem, PZERO);
	mp = mlinfolist;
	if (mp) {
		while (mp->ml_next)
			mp = mp->ml_next;
		mp->ml_next = minfo;
		mp = mp->ml_next;
	} else {
		mp = minfo;
		mlinfolist = mp;
	}
	mp->ml_next = 0;

	vsema(&mlinfosem);	

	/* now call symtab_load() */
	if ((rc = ML_LOAD(minfo)) != 0) {
		freeminfo (minfo);
		return (rc);
	}

	/* looks good... */
	minfo->ml_desc->m_id = ML_ID(minfo);
	minfo->ml_desc->m_flags &= ~M_TRANSITION;
	minfo->ml_desc->m_flags |= M_LOADED;
	minfo->ml_desc->m_refcnt = 0;

	/* save our ml_info_t for the rtsymtab */
	*m = minfo;
	
	return (rc);
}

/*
 * check_rtsyms()
 *
 * This routine ensures that the specified run-time
 * symtab module matches our in-core kernel image.
 *
 * Return 1 on failure, 0 on success.
 */
static int
check_rtsyms(ml_info_t *ml)
{
	elf_obj_info_t *obj = (elf_obj_info_t *)ml->ml_obj;
	Elf_Sym *sym;
	char *np;
	int size;
	int i;
	extern char end[], edata[], etext[];

	size = obj->shdr[obj->key_sections[SEC_SYMTAB]].sh_size
		/ sizeof(Elf_Sym);

	for (i = 0; i < size; i++) {
		sym = &obj->symtab[i];
		np = (char *)obj->stringtab + sym->st_name;
		if (np == (char *)NULL)
			continue;
		if (strcmp(np, "end") == 0) {
			if ((__psunsigned_t)sym->st_value != (__psunsigned_t)end)
				return (1);
		} else if (strcmp(np, "edata") == 0) {
			if ((__psunsigned_t)sym->st_value != (__psunsigned_t)edata)
				return (1);
		} else if (strcmp(np, "etext") == 0) {
			if ((__psunsigned_t)sym->st_value != (__psunsigned_t)etext)
				return (1);
		}
	}
	return (0);
}

/*
 * Debugging 
 *
 * The following routines are called by idbg.c, so they need to be
 * included here, rather than in midbg.c.
 */

/*
 * kp maddr - fetch symbol addr from a loaded module symbol table
 */

void
idbg_maddr(__psint_t *addr, char *name)
{
	ml_info_t *m = mlinfolist;

	while (m) {
		if (m->flags & ML_ELF)
			if (idbg_melfaddr (m, addr, name))
				return;
		m = m->ml_next;
	}
	*addr = 0;
	return;
}

/*
 * kp mname - fetch symbol name from a loaded module symbol table
 *	offst - address is passed in, offset is returned
 *
 *	NOTE: this routine depends on the ml_symtab being sorted by
 *		value.
 */

void
idbg_mname(__psunsigned_t *offst, char *name)
{
	ml_info_t *m;

	for (m = mlinfolist; m; m = m->ml_next) {
		if (!m->ml_nsyms)
			continue;
		if (m->flags & ML_ELF) {
			if (idbg_melfname (m, offst, name))
				return;
		}
	}
	*offst = 0;
	*name = 0;
	return;
}

int
idbg_melfname(ml_info_t *m, __psunsigned_t *offst, char *name)
{
	__psunsigned_t addr = *offst;
	ml_sym_t *symtab, *sym, *end, *last;

	symtab = (ml_sym_t *) m->ml_symtab;
	sym = (ml_sym_t *) &symtab[0];
	end = (ml_sym_t *) &symtab[m->ml_nsyms-1];

	/* If address is below first symbol or above the end of
	 * data, it shouldn't be found in this table.
	 */
	if (addr < sym->m_addr || addr > end->m_addr) {
		return 0;
	}

	for (last = sym; sym <= end; sym++) {
		/* XXX hack to get around ABS syms like _physmem_start */
		if (sym->m_addr == 0 && sym->m_off == 0)
			return 0;
		if (addr < sym->m_addr) {
			*offst = addr - last->m_addr;
			strcpy (name, m->ml_stringtab + last->m_off);
			return 1;
		}
		last = sym;
	}

	return 0;
}

int
idbg_melfaddr(ml_info_t *m, __psint_t *addr, char *name)
{
	char *targetname, *curname;
	ml_sym_t *sym;
	int i;

	sym = (ml_sym_t *) m->ml_symtab;

	for (i = 0; i < m->ml_nsyms; i++, sym++ ) {
		targetname = name;
		curname = &m->ml_stringtab[sym->m_off];

		while (*targetname == *curname) {
		    if (*targetname == 0) {
			*addr = sym->m_addr;
			return 1;
		    }
		    targetname++;
		    curname++;
		}
	}

	return 0;
}
