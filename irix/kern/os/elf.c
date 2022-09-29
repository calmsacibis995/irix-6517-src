/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.149 $"

/* NOTE that this file, for the 64-bit kernel, is compiled
 * twice: Once with ELF64 defined, and once with ELF64
 * left undefined. os/Makefile directs this compilation with
 * and without the additional ELF64 flag.
 *
 * The reason for this is that the Elf structure sizes and types
 * differ, but otherwise, the code is identical. The differences
 * are handled by the following definitions and typedefs in
 * the ELF32/ELF64 ifdef'd sections below. Thus, we have
 * one copy of the source and 2 copies of the object.
 *
 * If the compile mode is !_ABI64 this file is
 * compiled one time only - for elf support for o32 and n32
 * binaries.
 *
 * If the compile mode is _ABI64, the elfexec will read the
 * first 16 bytes of the target file, and decide based on
 * ELFCLASS whether to call elf64exec or elf32exec.
 */

/* Where this define is used, we are lengthening a 32 bit integer
 * to a pointer, which for 64 bit pointers generates a warning.
 * This cast first lengthens to a 64 bit int, then to a pointer,
 * so the compiler is quiet.
 */
#define	CADDR_T(x)		(caddr_t)(__psunsigned_t)(x)

#if !ELF64 || (ELF64 && _MIPS_SIM == _ABI64)		/* Whole file */

#include <sys/types.h>
#include <ksys/as.h>
#include <sys/auxv.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <elf.h>
#include <sys/errno.h>
#include <ksys/exception.h>
#include <sys/exec.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/mman.h>
#include <sys/numa.h>
#include <sys/param.h>
#include <os/numa/pm.h>
#include <sys/proc.h>
#include <sys/resource.h>
#include <sys/signal.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/tuneable.h>
#include <sys/uio.h>
#include <sys/vnode.h>
#include <ksys/vfile.h>
#include <ksys/fdt.h>
#include <ksys/vm_pool.h>
#include "os/proc/pproc_private.h"	/* XXX bogus */
#include "ksys/vpag.h"
#include <sys/ckpt.h>
#ifdef _SHAREII
#include	<sys/shareIIstubs.h>
#endif /* _SHAREII */

#ifdef _R5000_CVT_WAR
#include "as/as_private.h"	/* needed for pas_t, ppas_t 	*/
#include <sys/mtext.h>		/* processor workaround		*/
#ifndef OHW_R5KCVTL
#define OHW_R5KCVTL	0x8	/* R5000 cvt.[ds].l bug.  clean=1	*/
#endif /* OHW_R5KCVTL */
extern int R5000_cvt_war;
#endif /* _R5000_CVT_WAR */

/*
 * flags for mapelfexec
 */
#define ELFMAP_BRK	0x1	/* this mapping should be considered for
				 * the heap (brk(2))
				 */
#define ELFMAP_PRIMARY	0x2	/* this is the 'primary' mapping (a.out) */
#if ELF64
#define	Elf_Phdr	Elf64_Phdr
#define	Elf_Word	Elf64_Word

#define elfXXexec	elf64exec
#define elf2exec	elf64_2exec
#define elf2exec_complete	elf64_2exec_complete
#define getelfhead	getelf64head
#define mapelfexec	mapelf64exec
#define elfmap		elf64map
#define fuexarg		fuexarg64

#define ELFCLASS	ELFCLASS64
#define ELF_ABI		ABI_IRIX5_64

#define DEFUSRSTACK	DEFUSRSTACK_64

typedef	irix5_64_auxv_t	elf_auxv_t;

extern int fuexarg64(struct uarg *, caddr_t, uvaddr_t *, int);

 /* Holds availsmem needed for runtime loader */
extern int	runtime_loader_availsmem;
extern vnode_t	*runtime_loader_vp; /* Holds the vnode of runtime loader */

#else	/* ELF32 */

#if _MIPS_SIM != _ABI64
#define elfXXexec	elfexec
#else
#define elfXXexec	elf32exec
#endif

#define	Elf_Phdr	Elf32_Phdr
#define	Elf_Word	Elf32_Word

#define getelfhead	getelf32head
#define mapelfexec	mapelf32exec
#define ELFCLASS	ELFCLASS32
#define ELF_ABI		ABI_IRIX5
#define DEFUSRSTACK	DEFUSRSTACK_32

typedef	irix5_auxv_t	elf_auxv_t;

 /* Holds availsmem needed for runtime loader */
int	runtime_loader_availsmem = 0;

/*
 * We store the Vp of the runtime loader. If the runtime loader's vnode 
 * changes we recompute the runtime loader availsmem.
 */
vnode_t	*runtime_loader_vp; /* Holds the vnode of runtime loader */

#endif	/* ELF64 */

extern int elf32exec(struct vnode *, struct vattr *, struct uarg *, int);
extern int elf64exec(struct vnode *, struct vattr *, struct uarg *, int);
STATIC int getelfhead(Elf_Ehdr **, caddr_t *, long *, vnode_t *, struct uarg *);
STATIC int mapelfexec(struct vnode *, Elf_Ehdr *, caddr_t, Elf_Phdr **,
			Elf_Phdr **, Elf_Phdr **, int, caddr_t *,
			long *, vasid_t, int);
static int elf2exec(struct vnode *, struct uarg *, caddr_t,
			Elf_Ehdr *, caddr_t, int, uvaddr_t, int);
static int elf2exec_complete (struct vnode *, struct uarg *,
			 caddr_t, Elf_Ehdr *, caddr_t, int, uvaddr_t, int);
static pgno_t elf_get_availsmem(caddr_t, Elf_Ehdr *, uvaddr_t, struct uarg *);
static pgno_t elf_compute_availsmem(Elf_Ehdr *, caddr_t);

extern void setregs(caddr_t, int, int, int);

#ifdef _R5000_CVT_WAR
static int check_for_r5000_cvt_clean(
	struct vnode *vp,
	Elf_Phdr *phdr_base,
	int count,
	int *need_mtextp);
#endif /* _R5000_CVT_WAR */

/* For the 64-bit kernel, we only want 1 copy of the
 * following function. For the 32-bit kernel, this
 * bit of code goes away entirely.
 */
#if !ELF64 && _MIPS_SIM == _ABI64
int
elfexec(
	struct vnode *vp,
	struct vattr *vattrp,
	struct uarg *args,
	int level)
{
	unsigned char *e_ident = NULL;
	int error;
	char ident;

	error = exrdhead(vp, MAGIC_OFFSET, EI_NIDENT, (caddr_t *)&e_ident);
	if (e_ident) {
		ident = e_ident[EI_CLASS];
		exhead_free((caddr_t)e_ident);
	}

	if (error)
		return error;

	switch (ident) {
	case ELFCLASS32:
		error = elf32exec(vp, vattrp, args, level);
		break;
	case ELFCLASS64:
		error = elf64exec(vp, vattrp, args, level);
		break;
	default:
		error = ENOEXEC;
		break;
	}
	return error;
}
#endif



/* ARGSUSED */
int
elfXXexec(
	struct vnode *vp,
	struct vattr *vattrp,
	struct uarg *args,
	int level)
{
	Elf_Ehdr	*ehdrp = NULL;
	Elf_Phdr	*phdr;
	caddr_t		phdrbase = 0;
	int		error;
	long		phdrsize;
	int		i, hcnt;
	int		hasu = 0;
	int		hasdy = 0;
	int		size;
	caddr_t		vwinadr = NULL;
	uvaddr_t	newsp;
	int		nargs = args->ua_ncargs;
	int		sourceabi;
#ifdef TFP_PREFETCH_WAR
	int		has_prefetch = 0;
#endif
	pgno_t		exec_needed_availsmem;
	vpagg_t		*vpag;

	sourceabi = curuthread->ut_pproxy->prxy_abi;
	if ((error = getelfhead(&ehdrp, &phdrbase, &phdrsize, vp, args)) != 0)
		goto out;

	/* determine aux size now so that stack can be built
	 * in one shot (except actual copyout of aux image)
	 * and still have this code be machine independent.
	 *
	 * Additionally, since we are scanning the phdr anyway, look
	 * at the hardware workaround section now - the only other
	 * chance we scan the phdr is in mapelfexec - at which
	 * point the only message that can be returned to the
	 * user is 'killed', since we have removed the old a.out
	 * image by that time. Scanning here, we can return the
	 * EBADRQC errno, which csh/sh/ksh all look for explicitly.
	 */
	hcnt = ehdrp->e_phnum;
	for (i = 0; i < hcnt; i++) {
		phdr = (Elf_Phdr *)(phdrbase + (ehdrp->e_phentsize * i));
		switch(phdr->p_type) {
		case PT_INTERP:
			hasdy = 1;
			break;
		case PT_PHDR:
			hasu = 1;
			break;
#ifdef TFP_PREFETCH_WAR
		case PT_MIPS_OPTIONS:
		    {
			Elf_Options *eop;
			caddr_t buf;
			int off = 0;

			if (error = exrdhead(vp, (off_t)phdr->p_offset,
					     phdr->p_filesz, &buf)) {
				if (buf)
					exhead_free(buf);
				goto out;
			}
			while (off < phdr->p_filesz) {
				eop = (Elf_Options *)(buf + off);
				off += eop->size;
				if (eop->kind == ODK_HWPATCH &&
				    (eop->info & OHW_R8KPFETCH)) {
					has_prefetch = 1;
					break;
				}
				/*
				 * Some broken binaries have a null
				 * options section which we need to
				 * check for here otherwise we will
				 * loop here forever
				 */
				if(eop->size == 0) {
					if (buf)
						exhead_free(buf);
					goto out;
				}	
			}
			if (buf)
				exhead_free(buf);
			break;
		    }
#endif	/* TFP_PREFETCH_WAR */
		}

	}

#ifdef TFP_PREFETCH_WAR
	/* HW workaround: Mips4 binaries that have prefetch
	 * instructions cause problems on TFP. Such binaries
	 * have a PT_OPTIONS section with the OHW_R8KPFETCH
	 * turned on. If this is such a binary, and that
	 * binary is non-shared, then, on TFP, refuse to
	 * exec the binary.
	 *
	 * For MIPS4 binaries that have prefetch instruction,
	 * but are dynamically linked, then rld will fix up
	 * the a.out, as well as the dso, by rewriting the
	 * prefetchs to no-ops - so we let those pass. In this
	 * case we have to give some additional information
	 * back to rld - in the form of an AT_PFETCHFD entry in the
	 * auxiliary vector (auxv). This is to give rld an open
	 * file descriptor to the a.out, since rld does not
	 * have a full path.
	 */
	if (has_prefetch) {
		if (!hasdy) {
			extern int chk_ohw_r8kpfetch;

			cmn_err(CE_CONT,
		"Potential prefetch instruction sequence problem (pid %d).\n" 
		"Please run the program through 'r8kpp'.\n",
				current_pid());

			if (chk_ohw_r8kpfetch != 0) {
				error = EBADRQC;
				goto out;
			}
		} else {
			/* Since we are going to have rld to the PFETCH
			 * workaround, pass that information down to
			 * subsequent routines so we can generate an
			 * auxiliary vector entry of type AT_PFETCHFD.
			 */
			args->ua_execflags |= ADD_PFETCHFD;
		}
	}
#endif	/* TFP_PREFETCH_WAR */

	if (hasdy) {
#ifdef TFP_PREFETCH_WAR
		/* Space for additional AT_PFETCHFD descripter in auxv */
		if (has_prefetch && hasu)
			args->ua_auxsize = 8 * sizeof(elf_auxv_t);
		else
#endif
		if (hasu)
			args->ua_auxsize = 7 * sizeof(elf_auxv_t);
		else
			args->ua_auxsize = 4 * sizeof(elf_auxv_t);
	}

	size = btoc(3*nargs + args->ua_auxsize);

	/*
	 * vwinadr is pointer in kernel space of area for
	 * building exec arguments.  kvpalloc could fail
	 * if size breaks avail[rs]mem limits.
	 */
#ifdef _VCE_AVOIDANCE
	vwinadr = kvpalloc(size, VM_VACOLOR | VM_VM,
			colorof((DEFUSRSTACK_32 - ctob(size))));
#else
	vwinadr = kvpalloc(size, VM_UNSEQ | VM_VM, 0);
#endif
	if (vwinadr == NULL) {
		error = EAGAIN;
		goto out;
	}

	/* set default stack location */
	args->ua_stackaddr = (uvaddr_t)DEFUSRSTACK;
	if (error = fuexarg(args, vwinadr + (ctob(size) - 3*nargs -
			    args->ua_auxsize), &newsp, sourceabi)) {
		goto out;
	}

	/* Tentatively set curprocp->p_proxy.prxy_abi to abi of
	 * to-be-exec'd file. If the exec fails, it will be reset
	 * to 'sourceabi'. Can't set the abi before fuexarg since
	 * that routine may take a fault on the current stack and
	 * tfault will get confused about our page table format.
	 */

	curuthread->ut_pproxy->prxy_abi = args->ua_abiversion;

	VPROC_GETVPAGG(curvprocp, &vpag);
	if ((vpag) && (kt_basepri(curthreadp) == PBATCH)) {
		exec_needed_availsmem = 
			elf_get_availsmem(phdrbase, ehdrp, newsp, args);
		if ((args->ua_as_state.as_smem_reserved 
			= vm_pool_exec_reservemem(exec_needed_availsmem)) < 0) {
			error = EMEMRETRY;
			goto out; 
		}
	} else args->ua_as_state.as_smem_reserved = 0;


	return elf2exec(vp, args, phdrbase, ehdrp, vwinadr, size, newsp,
					sourceabi);

out:
	/* The exec failed, so reset curprocp->p_proxy.prxy_abi */
	curuthread->ut_pproxy->prxy_abi = sourceabi;
	if (ehdrp)
		exhead_free((caddr_t)ehdrp);

	if (phdrbase)
		exhead_free(phdrbase);

	if (vwinadr)
		kvpfree(vwinadr, size);

	if (error == 0)
		error = ENOEXEC;

	return error;
}


/*
 * 2nd half of elfXXexec - split up to conserve stack space - the fuexarg
 * can recurse alot..
 * The important thing about this routine is that it NOT touch user space
 * (and therefore potentially get a fault) since its stack frame is so large.
 * Assumption is that curprocp->p_proxy.prxy_abi has already been set to
 * the NEW abi
 */
/* ARGSUSED */
static int
elf2exec(
	struct vnode *vp,
	struct uarg *args,
	caddr_t phdrbase,
	Elf_Ehdr *ehdrp,
	caddr_t vwinadr,
	int size,
	uvaddr_t newsp,
	int sourceabi
	)
{
	int		error;
	uthread_t	*ut = curuthread;
	cell_t		cell = cellid();
	int		rmp;

	/*
	 * Remove old process image
	 * Don't save text parts of the address space if 1) changing abi, or
	 * 2) doing a remote exec
	 */
	rmp = (sourceabi != args->ua_abiversion) || (args->ua_cell != cell);
	if (error = remove_proc(UT_TO_PROC(ut), args, vp, rmp)) 
		goto bad2;

	/* decrement usage of old affinity link */
	kthread_afflink_decusg(UT_TO_KT(ut), &ut->ut_asid);

	as_exec_export(&args->ua_as_state, &ut->ut_asid);
	ASSERT(args->ua_as_state.as_tsave == NULL || args->ua_cell == cell);

	args->ua_phdrbase = phdrbase;
	args->ua_ehdrp = ehdrp;
	args->ua_vwinadr = vwinadr;
	args->ua_size = size;
	args->ua_newsp = newsp;
	args->ua_sourceabi = sourceabi;
	args->ua_elf2_proc = &elf2exec_complete;

	error = vproc_exec(UT_TO_VPROC(ut), args);
	return(error);

bad2:
	(void) sigtopid(current_pid(), SIGKILL, SIG_ISKERN, 0, 0, 0);
        cmn_err(CE_ALERT,
         "!%s[%d] was killed; the old process-image was not removed",
         proc_name((proc_handl_t *)ut->ut_proc),current_pid());

	/*
	 * Free up any pre-reserved memory if its a miser process.
	 */
	if (args->ua_as_state.as_smem_reserved > 0) 
		unreservemem(GLOBAL_POOL,
				args->ua_as_state.as_smem_reserved, 0, 0);

	/* The exec failed, so reset curprocp->p_proxy.prxy_abi */
	ut->ut_pproxy->prxy_abi = sourceabi;
	if (ehdrp)
		exhead_free((caddr_t)ehdrp);

	if (phdrbase)
		exhead_free(phdrbase);

	if (vwinadr)
		kvpfree(vwinadr, size);

	if (error == 0)
		error = ENOEXEC;
	return error;
}

int
elf2exec_complete(
	struct vnode *vp,
	struct uarg *args,
	caddr_t phdrbase,
	Elf_Ehdr *ehdrp,
	caddr_t vwinadr,
	int size,
	uvaddr_t newsp,
	int sourceabi
	)
{
	caddr_t		prog_base;
	char		*dlnp;
	int		dlnsize, error;
	int		fd = -1;
	long		voffset;
	uthread_t	*ut = curuthread;
	struct vnode	*nvp;
	Elf_Phdr	*dyphdr = NULL;
	Elf_Phdr	*stphdr = NULL;
	Elf_Phdr	*uphdr = NULL;
	Elf_Phdr	*junk = NULL;
	long		phdrsize;
	elf_auxv_t	elfargs[8];
	elf_auxv_t	*aux;
	int		postfixsize = 0;
	struct vattr	vattr;
	vasid_t		vasid;
	as_addspace_t	asd;
	as_deletespace_t as_del;
	/* REFERENCED */
	as_addspaceres_t asres;
	ckpt_handle_t	*ckptp = NULL;
#ifdef CKPT
	ckpt_handle_t	ckpt = NULL;
#endif
	int bad_reason = 0;
	char *bad_reason_list[10] = {
		"reason unspecified", /*0*/
		"cannot map exe into memory", /*1*/
		"cannot read exe header", /*2*/
		"not found", /*3*/
		"failed to open exe", /*4*/
		"doesn't have proper permissions", /*5*/
		"cannot get elf header", /*6*/
		"cannot get enough memory to map exe", /*7*/
		"failed to map exe", /*8*/
		"cannot add virtual address space" /*9*/
	};
	/*
	 * allocate a new AS
	 * NOTE: returns with AS locked...
	 */
	{
	struct aspm	*aspm;
        pm_t		*pm;

	/*
	 * Get new aspm - telling the numa module whether the thread 
	 * is mustrun.
	 */
	aspm = aspm_exec_create(vp, KT_ISMR(UT_TO_KT(ut)));

	as_exec_import(&vasid, &ut->ut_asid, &ut->ut_as, &args->ua_as_state,
		ut->ut_pproxy->prxy_abi, aspm, &pm);

	/* set up affinity links */
	kthread_afflink_exec(UT_TO_KT(ut), pm);
	aspm_releasepm(pm);
	}

	if (error = mapelfexec(vp, ehdrp, phdrbase, &uphdr, &dyphdr, &stphdr,
				ELFMAP_BRK|ELFMAP_PRIMARY,
				&prog_base, &voffset, vasid,
#ifdef CKPT
				args->ua_ckpt)) {
#else
				-1)) {
#endif
                /* cannot map executable into memory */
		bad_reason = 1 ;
		goto bad;
	}

	if (uphdr != NULL && dyphdr == NULL) {
                /* cannot map executable into memory,  */
		bad_reason = 1;
		goto bad;
	}

	if (dyphdr != NULL) {
		dlnsize = dyphdr->p_filesz;

		if (dlnsize > MAXPATHLEN || dlnsize <= 0) {
                /* cannot map executable into memory */
			bad_reason = 1;
			goto bad;
		}

		dlnp = NULL;
		error = exrdhead(vp, dyphdr->p_offset, dyphdr->p_filesz,
				(caddr_t *) &dlnp);
		if (error || (dlnp[dlnsize - 1] != '\0')) {
			if (dlnp)
				exhead_free(dlnp);
                /* cannot read exe header */
			bad_reason = 2;
			goto bad;
		}

#ifdef CKPT
		ckptp = (ckpt_enabled)? &ckpt : NULL;
#endif
		if (error = lookupname(dlnp, UIO_SYSSPACE, FOLLOW,
				       NULLVPP, &nvp, ckptp)) {
			cmn_err(CE_CONT,
				"Dynamic loader %s not found, error=%d\n",
				dlnp, error);
			exhead_free(dlnp);
			/* Dynamic loader not found */
			bad_reason = 3;
			goto bad;
		}
		exhead_free(dlnp);

		aux = elfargs;
		if (uphdr) {
			setexecenv(vp);
			aux->a_type = AT_PHDR;
			aux->a_un.a_val = uphdr->p_vaddr + voffset;
			aux++;
			aux->a_type = AT_PHENT;
			aux->a_un.a_val = ehdrp->e_phentsize;
			aux++;
			aux->a_type = AT_PHNUM;
			aux->a_un.a_val = ehdrp->e_phnum;
			aux++;
			aux->a_type = AT_ENTRY;
			aux->a_un.a_val = ehdrp->e_entry + voffset;
			aux++;
			postfixsize += 4 * sizeof(elf_auxv_t);

#ifdef TFP_PREFETCH_WAR
			if (args->ua_execflags & ADD_PFETCHFD) {
				/* For the R8KPFETCH workaround, we give
				 * an open file descriptor back to rld,
				 * so it can scan the a.out for the
				 * prefetch instruction.
				 */
				if (error = execopen(&vp, &fd)) {
					bad_reason = 4;
					goto bad;
				}

				aux->a_type = AT_PFETCHFD;
				aux->a_un.a_val = fd;
				aux++;
				postfixsize += sizeof(elf_auxv_t);
			}
#endif
		} else {
			if (error = execopen(&vp, &fd)) {
                        /* failed to open exe */
				bad_reason = 4;
				goto bad;
			}
			aux->a_type = AT_EXECFD;
			aux->a_un.a_val = fd;
			aux++;
			postfixsize += sizeof(elf_auxv_t);
#ifdef CKPT
			if (ckpt_enabled)
				ckpt_setfd(fd, args->ua_ckpt);
#endif
		}

		if ((error = execpermissions(nvp, &vattr, args)) != 0) {
			VN_RELE(nvp);
                        /* rld has insufficent permissions */
			bad_reason = 5;
			goto bad;
		}
		/*
		 * dynamic loader's that have the sticky bit on
		 * get to save pregions - others don't
		 */
		if ((vattr.va_mode & VSVTX) == 0)
			removetsave(AS_NOFLUSH|AS_TSAVE_LOCKED);

		exhead_free((caddr_t)ehdrp);
		exhead_free(phdrbase);
		ehdrp = NULL;		/* so we know whether or not to */
		phdrbase = NULL;	/* free them at the end */

		if ((error = getelfhead(&ehdrp, &phdrbase,
				&phdrsize, nvp, args)) != 0) {
			VN_RELE(nvp);
                        /* rld cannot get elf header */
			bad_reason = 6;
			goto bad;
		}

		/*
		 * map in dynamic loader
		 * For this, we want brkbase set up as well as
		 * nextaalloc and hiusrattach.
		 */

		/*
		 * Compute and store availsmem needed for mapping runtime
		 * loader the first time and whenever the runtime loader vp
		 * changes. We assume that libc's vnode will not be reclaimed
		 * easily and so it will change only when inst installs a new
		 * libc. 
		 */
		if ((runtime_loader_availsmem == 0) 
				|| (runtime_loader_vp != nvp)) {
			runtime_loader_availsmem = 
				elf_compute_availsmem(ehdrp, phdrbase);
			runtime_loader_vp = nvp;
		}

		error = mapelfexec(nvp, ehdrp, phdrbase, &junk, &junk, &junk,
				0, &prog_base, &voffset, vasid,
#ifdef CKPT
			(ckptp && *ckptp)? ckpt_lookup_add(nvp, *ckptp) : -1);

		ckptp = NULL;
#else
				NULL);
#endif
		VN_RELE(nvp);
		if (error) {
                        /* not enough available memory to map exe */
			bad_reason = 7;
			goto bad;
		}

		if (junk != NULL) {
                        /* failed to map exe  */
			bad_reason = 8;
			goto bad;
		}

		aux->a_type = AT_BASE;
		aux->a_un.a_val =
			(__psunsigned_t)prog_base & ~(ELF_MIPS_MAXPGSZ - 1);
		aux++;
		aux->a_type = AT_PAGESZ;
		aux->a_un.a_val = NBPP;
		aux++;
		aux->a_type = AT_NULL;
		aux->a_un.a_val = 0;
		postfixsize += 3 * sizeof(elf_auxv_t);
		ASSERT(postfixsize == args->ua_auxsize);
	} else {
		/*
		 * Only permit ghost regions for dynamic loaders' use
		 */
		removetsave(AS_NOFLUSH|AS_TSAVE_LOCKED);
	}
	/*
	 * Set up the users stack.
	 */
	if (postfixsize)
		bcopy(elfargs, args->ua_stackend, postfixsize);

#if ELF64 && _MIPS_SIM == _ABI64 && R10000
	setsr(getsr() | SR_UX);
#endif

	asd.as_op = AS_ADD_STACKEXEC;
	asd.as_addr = args->ua_stackaddr;
	asd.as_length = args->ua_stackaddr - newsp;
	asd.as_stackexec_contents = vwinadr + ctob(size);

	if (error = VAS_ADDSPACE(vasid, &asd, &asres)) {
               /* cannot add virtual address space */
		bad_reason = 9;
		goto bad;
	}

	curexceptionp->u_eframe.ef_sp = (k_machreg_t)newsp;


	VAS_UNLOCK(vasid);

	/*
	 * Free up any left over pre-reserved memory.
	 */
	if (args->ua_as_state.as_smem_reserved) {
		as_del.as_op = AS_DEL_EXEC_SMEM;
		VAS_DELETESPACE(vasid, &as_del, NULL);
	}

	/* For ELF64, tell setregs to enable extended fpu registers */
	setregs((caddr_t)(ehdrp->e_entry + voffset),
		((ehdrp->e_flags & EF_MIPS_ARCH) == EF_MIPS_ARCH_3),
		((ehdrp->e_flags & EF_MIPS_ARCH) == EF_MIPS_ARCH_4),
		ABI_IS(ABI_IRIX5_64|ABI_IRIX5_N32, ut->ut_pproxy->prxy_abi));

	if (!uphdr) {
		setexecenv(vp);
	}

	exhead_free((caddr_t)ehdrp);
	exhead_free(phdrbase);

	kvpfree(vwinadr, size);

	return 0;

bad:

	VAS_UNLOCK(vasid);
	/*
	 * Free up any left over pre-reserved memory of miser jobs.
	 */
	if (args->ua_as_state.as_smem_reserved) {
		as_del.as_op = AS_DEL_EXEC_SMEM;
		VAS_DELETESPACE(vasid, &as_del, NULL);
	}
#if CKPT
	if (ckptp && *ckptp)
		ckpt_lookup_free(*ckptp);
#endif

	if (fd != -1)		/* did we open the a.out yet */
		(void)execclose(fd);

	(void) sigtopid(current_pid(), SIGKILL, SIG_ISKERN, 0, 0, 0);
        cmn_err(CE_ALERT,
         "!%s[%d] was killed; Dynamic Loader %s",
         proc_name((proc_handl_t *)ut->ut_proc),current_pid(),
	 bad_reason_list[bad_reason]);

	/* The exec failed, so reset curprocp->p_proxy.prxy_abi */
	ut->ut_pproxy->prxy_abi = sourceabi;
	if (ehdrp)
		exhead_free((caddr_t)ehdrp);

	if (phdrbase)
		exhead_free(phdrbase);

	if (vwinadr)
		kvpfree(vwinadr, size);

	if (error == 0)
		error = ENOEXEC;

	return error;
}

STATIC int
getelfhead(
	Elf_Ehdr **ehdrp,
	register caddr_t *phdrbase,
	long *phdrsize,
	vnode_t	*vp,
	struct uarg *args)
{
	register Elf_Ehdr *ehdr;
	Elf_Word arch;
	int error;
#ifdef TRITON
	extern __uint32_t _irix5_mips4;
#endif /* TRITON */

	error = exrdhead(vp, (off_t) 0, sizeof(Elf_Ehdr), (caddr_t *) ehdrp);
	if (error)
		goto bad;
	ehdr = *ehdrp;
	args->ua_ehdrp_sz = sizeof(Elf_Ehdr);
	/*
   	 * We got here by the first two bytes in ident
	 */

	if (ehdr->e_ident[EI_MAG2] != ELFMAG2
	     || ehdr->e_ident[EI_MAG3] != ELFMAG3
	     || ehdr->e_ident[EI_CLASS] != ELFCLASS
	     || ehdr->e_ident[EI_DATA] != ELFDATA2MSB
	     || (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN)
	     || ehdr->e_phentsize == 0)
			goto bad;

	/*
	 * Check architecture
	 * XXX bad ld puts in 0xf for ARCH!
	 */
	arch = ehdr->e_flags & EF_MIPS_ARCH;
	if (arch != 0 &&
	    arch != EF_MIPS_ARCH &&
	    arch != EF_MIPS_ARCH_2 &&
#if MIPS4_ISA
	    (! (
#ifdef TRITON
		_irix5_mips4 != 0 &&
#endif /* TRITON */
		arch == EF_MIPS_ARCH_4)) &&
#endif
	    arch != EF_MIPS_ARCH_3) {
		error = EBADRQC;
		goto bad;
	}
	/*
	 * return the ABI version up to exec;
	 */
	if (ehdr->e_machine == EM_MIPS) {
		args->ua_abiversion = ELF_ABI;
		/*
		 * Only look at EF_MIPS_ABI2 bit if this is an ABI_IRIX5
		 * binary. Some versions of the compiler incorrectly set
		 * EF_MIPS_ABI2 for ABI_IRIX5_64 binaries.
		 */
		if ((ELF_ABI == ABI_IRIX5) &&
		    ((ehdr->e_flags & EF_MIPS_ABI2) == EF_MIPS_ABI2))
			args->ua_abiversion = ABI_IRIX5_N32;
	} else
		goto bad;

	*phdrsize = ehdr->e_phnum * ehdr->e_phentsize;
	error = exrdhead(vp, (off_t)ehdr->e_phoff, *phdrsize, (caddr_t *) phdrbase);
	if (error)
		goto bad;
	args->ua_phdrbase_sz = *phdrsize;

	return 0;

bad:
	if (error == 0)
		error = ENOEXEC;

	return error;
}

/* ARGSUSED */
STATIC int
mapelfexec(
	struct vnode *vp,
	Elf_Ehdr *ehdr,
	caddr_t phdrbase,
	Elf_Phdr **uphdr,
	Elf_Phdr **dyphdr,
	Elf_Phdr **stphdr,
	int mflags,
	caddr_t *prog_base,
	long *voffset,
	vasid_t vasid,
	int ckpt)
{
	Elf_Phdr *phdr;
	int i, prot, error;
	caddr_t addr;
	size_t zfodsz;
	int ptload = 0;
#ifdef _R5000_CVT_WAR
	/*
	 * Check if we need to use the mtext system to deal with problem
	 * CVT instructions in this exec. See os/mtext.c for the gory details.
	 */
	int	need_mtext = 0;		/* if 1, we will use mtext	*/
	if (R5000_cvt_war) {
		error = check_for_r5000_cvt_clean(vp, (Elf_Phdr *)phdrbase, ehdr->e_phnum, &need_mtext);
		if (error)
			return error;
	} /* if (R5000_cvt_war) */
#endif /* _R5000_CVT_WAR */

	*voffset = 0;		/* Do not remap. Use the pre-link address. */

	for (i=0; i < (int)ehdr->e_phnum; i++) {
		phdr = (Elf_Phdr *)(phdrbase + (ehdr->e_phentsize *i));

		switch (phdr->p_type) {
		case PT_LOAD:
			{
			int flags = 0;
			if ((*dyphdr != NULL) && (*uphdr == NULL))
				return 0;

			prot = PROT_NONE;
			if (phdr->p_flags & PF_R)
				prot |= PROT_READ;
			if (phdr->p_flags & PF_W) {
				prot |= PROT_WRITE;
				if (mflags & ELFMAP_BRK)
					flags |= MAP_BRK;
			}
			if (phdr->p_flags & PF_X)
				prot |= PROT_EXEC;

			if (phdr->p_flags & PF_MIPS_LOCAL)
				flags |= MAP_LOCAL;
			if (mflags & ELFMAP_PRIMARY)
				flags |= MAP_PRIMARY;

			addr = CADDR_T(phdr->p_vaddr) + *voffset;
			zfodsz = (size_t) phdr->p_memsz - phdr->p_filesz;

			/*
			 * Remember the address of the first segment.  This
			 * ABI specifies that this will be the one with
			 * lowest address.  This is used to setup the
			 * AT_BASE arg when loading a run time linker.
			 */
			if (ptload == 0) {
				*prog_base = addr;
				ptload = 1;
			}
#ifdef _R5000_CVT_WAR
			if (	need_mtext 		&&	/* potentially bad instructions	*/
				(prot & PROT_EXEC)	&&	/* executable pages only	*/
				!(prot & PROT_WRITE))	{	/* no writable pages		*/
				/*
				 * execmap the vnode.  if a page might have a bad
				 * cvt instruction, it will be mapped with a proxy vnode. 
				 */
				error = mtext_execmap_vnode(
					vp, addr, phdr->p_filesz, zfodsz,
					phdr->p_offset, prot, flags,
					vasid, ckpt);
				if (error)
					return error;
			} else 
#endif /* _R5000_CVT_WAR */
			if (error = execmap(vp, addr, phdr->p_filesz, zfodsz,
					    (off_t)phdr->p_offset, prot, flags,
					    vasid, ckpt))
				return error;

			break;
			}

		case PT_INTERP:
			if (ptload)
				return ENOEXEC;
			*dyphdr = phdr;
			break;

		case PT_SHLIB:
			*stphdr = phdr;
			break;

		case PT_PHDR:
			if (ptload)
				return ENOEXEC;
			*uphdr = phdr;
			break;

		case PT_NULL:
		case PT_DYNAMIC:
		case PT_NOTE:
			break;

#if MIPS4_ISA
		case PT_MIPS_OPTIONS:
		{
			Elf_Options *eop;
			caddr_t buf;
			int off = 0;

			if (exrdhead(vp, (off_t)phdr->p_offset,
					 	phdr->p_filesz, &buf))
				goto out;
			while (off < phdr->p_filesz) {
			    eop = (Elf_Options *)(buf + off);
			    off += eop->size;
			    if (eop->kind == ODK_EXCEPTIONS) {
#if NEVER
				/*
				 * Some versions of the ragnarok ld mistakenly
				 * mark binaries as SMM, so we can't trust it.
				 */
				if (eop->info & OEX_SMM)
				    curuthread->ut_pproxy->prxy_fp.pfp__fpflags
						|= P_FP_SMM_HDR;
#endif
				if (eop->info & OEX_PRECISEFP)
				    curuthread->ut_pproxy->prxy_fp.pfp_fpflags
						|= P_FP_PRECISE_EXCP_HDR;
				if (eop->info & OEX_DISMISS)
				    curuthread->ut_pproxy->prxy_fp.pfp_fpflags
						|= P_FP_SPECULATIVE;
			    }
			    /*
			     * Some broken binaries have a null
			     * options section which we need to
			     * check for here otherwise we will
			     * loop here forever
			     */
			    if (eop->size == 0) {
				    goto out;
			    }

			}
out:
			if (buf)
				exhead_free(buf);
			break;
		}
#endif	/* MIPS4_ISA */

		default:
			break;
		}
	}
	return 0;
}


/*
 * Read in an array of elf Phdr's and perform the mmap's indicated by the
 * PT_LOAD sections from the file referenced by fd.
 */

#define LCL_PHDRS	4

/* The normal 'count' argument to elfmap is 2 - put a reasonable
 * upper limit on the number of phdrs elfmap will allow. This
 * is to prevent calls to kmem_alloc with bogus arguments.
 */
#define __PHDR_MAX	10000

int
elfmap(int fd, Elf_Phdr *user_phdr, int count, rval_t *rvp)
{
	Elf_Phdr	lcl_phdr[LCL_PHDRS];
	Elf_Phdr	*phdr_base, *phdr;
	auto vfile_t	*fp;
	vnode_t		*vp;
	size_t		zfodsz;
	int		prot;
	int		num;
	int		error;
	int		tries = 0;
	int		flags;
	vasid_t		vasid;
#ifdef _R5000_CVT_WAR
	int		need_mtext = 0;
#endif /* _R5000_CVT_WAR */

	/*
	 * user_phdr points to the list of structs in user space.  We have
	 * to copy these in first.  count tells us how many contiguous
	 * structs there are.  Normally, there will only be two, so use
	 * the local array to save the allocation/deallocation overhead.
	 */
	if (count <= 0 || count > __PHDR_MAX)
		return EINVAL;

	if (count <= LCL_PHDRS)
		phdr_base = lcl_phdr;
	else
		phdr_base = (Elf_Phdr *)kmem_zalloc(sizeof(Elf_Phdr) * count,
								KM_SLEEP);
	if (copyin(user_phdr, phdr_base, sizeof(*phdr) * count) != 0) {
		error = EFAULT;
		goto out;
	}

	/*
	 * Make sure fd is valid.  Check for read permission to file.
	 */
	if (error = getf(fd, &fp))
		goto out;

	if ((fp->vf_flag & FREAD) == 0) {
		error = EBADF;
		goto out;
	}
	if (!VF_IS_VNODE(fp)) {
		error = EINVAL;
		goto out;
	}
	vp = VF_TO_VNODE(fp);
	error = check_dmapi_file(vp);
	if (error)
		goto out;
#ifdef _R5000_CVT_WAR
	/*
	 * Check if we need to use the mtext system to deal with problem
	 * CVT instructions in this exec. See os/mtext.c for the gory details.
	 */
	if (R5000_cvt_war) {
		error = check_for_r5000_cvt_clean(vp, phdr_base, count, &need_mtext);
		if (error)
			return error;
	} /* if (R5000_cvt_war) */
#endif /* _R5000_CVT_WAR */

	/*
	 * If process has shared address space, and is trying to map 
	 * a file in executable mode, we cannot let this file to have
	 * replicated pages. Problem is, since the shared address space has 
	 * already been created, we will have to go and attach the 
	 * executable as private space in order to support replication.
	 * So, for now we will not do this.
	 */
	if (curprocp->p_shaddr) {
		repl_dispose(vp);
	}
		
	as_lookup_current(&vasid);

	/*
	 * map in each of the PT_LOAD sections.  execmap will validate arguments
	 */
	VAS_LOCK(vasid, AS_EXCL);
try_again:
	tries++;

	for (num = count, phdr = phdr_base; num > 0; num--, phdr++) {
		if (phdr->p_type != PT_LOAD) {
			error = ENOEXEC;
			break;
		}

		prot = PROT_NONE;
		if (phdr->p_flags & PF_R)
			prot |= PROT_READ;
		if (phdr->p_flags & PF_W)
			prot |= PROT_WRITE;
		if (phdr->p_flags & PF_X)
			prot |= PROT_EXEC;

		if (phdr->p_flags & PF_MIPS_LOCAL)
			flags = MAP_LOCAL;
		else
			flags = 0;

		zfodsz = (size_t) phdr->p_memsz - phdr->p_filesz;
#ifdef _R5000_CVT_WAR
			if (	need_mtext 		&&	/* potentially bad instructions	*/
				(prot & PROT_EXEC)	&&	/* executable pages only	*/
				!(prot & PROT_WRITE))	{	/* no writable pages		*/
				/*
				 * execmap the vnode.  if a page might have a bad
				 * cvt instruction, it will be mapped with a proxy vnode. 
				 */
				error = mtext_execmap_vnode(
					vp, CADDR_T(phdr->p_vaddr), phdr->p_filesz, 
					zfodsz, phdr->p_offset, prot, flags, vasid, 
#if CKPT
					fp->vf_ckpt);
#else	/* CKPT */
					-1);
#endif 	/* CKPT	*/
			} else 
#endif /* _R5000_CVT_WAR */
		error = execmap(vp, CADDR_T(phdr->p_vaddr), phdr->p_filesz,
				zfodsz, phdr->p_offset, prot, flags, vasid,
#if CKPT
				fp->vf_ckpt);
#else
				-1);
#endif
		if (error)
			break;
	}
	VAS_UNLOCK(vasid);

	/*
	 * Work backwards and unmap anything that was mapped if we couldn't
	 * map them all.
	 */
	if (error) {
		as_deletespace_t asd;
		/* REFERENCED */
		int error2 = 0;

		for (num++, phdr--; num <= count; num++, phdr--) {
			asd.as_op = AS_DEL_MUNMAP;
			asd.as_munmap_start = CADDR_T(phdr->p_vaddr);
			asd.as_munmap_len = (size_t)phdr->p_memsz;
			asd.as_munmap_flags = 0;
			error2 += VAS_DELETESPACE(vasid, &asd, NULL);
		}
		ASSERT(error2 == 0);
	}

	/*
	 * Try an alternate address if we couldn't map in at the requested
	 * ones.  If the alternate address fails, it means something else is
	 * wrong with the dso.
	 */
	if (error == ENOMEM && tries == 1) {
		caddr_t	low, high, new_base;
		size_t align;
		as_getrangeattrin_t in;
		as_getrangeattr_t out;

		/*
		 * Find the highest and lowest address in the dso.  This tells
		 * us the size of the hunk of VM to look for.  Also find the
		 * largest p_align value.
		 */
		phdr = phdr_base;
		low  = CADDR_T(phdr->p_vaddr);
		high = CADDR_T(phdr->p_vaddr) + phdr->p_memsz;
		align = phdr->p_align;

		for (num = count-1, phdr++; num > 0; num--, phdr++) {
			if (CADDR_T(phdr->p_vaddr) < low)
				low = CADDR_T(phdr->p_vaddr);

			if (CADDR_T(phdr->p_vaddr) + phdr->p_memsz > high)
				high = CADDR_T(phdr->p_vaddr) + phdr->p_memsz;

			if (phdr->p_align > align)
				align = phdr->p_align;
		}

		in.as_color = 0;
		/*
		 * relock for getrangattr and hold while we spin around again
		 */
		VAS_LOCK(vasid, AS_EXCL);
		if (VAS_GETRANGEATTR(vasid, NULL, high - low + align,
						AS_GET_ADDR, &in, &out) == 0) {
			new_base = out.as_addr;
			/*
			 * Round the new base address up to the proper
			 * alignment.  Then go through the Phdrs and relocate
			 * the virtual addresses to the new base.
			 */
			new_base += (align - ((__psint_t)new_base & (align-1)))
					 & (align-1);

			for (num = count, phdr = phdr_base; num > 0;
								 num--, phdr++)

				phdr->p_vaddr = phdr->p_vaddr - (__psint_t)low +
						(__psint_t) new_base;

			goto try_again;
		}
		VAS_UNLOCK(vasid);
	}

	if (!error)
		rvp->r_val1 = phdr_base->p_vaddr;

out:
	if (phdr_base != lcl_phdr)
		kmem_free(phdr_base, sizeof(Elf_Phdr) * count);

	if (error == EAGAIN)
		nomemmsg("DSO loading");

	return error;
}

/*
 * The following routines precompute the availsmem needed to exec a given elf
 * file. It is used to determine if there is enough availsmem for
 * the exec to succeed. By doing the check early before deleting the 
 * old address space we can wait and retry the exec system call once there is 
 * enough availsmem. The retry is necessary for miser batch jobs which have 
 * not gone critical.
 */
static pgno_t
elf_get_availsmem(
	caddr_t phdrbase,
	Elf_Ehdr *ehdrp,
	uvaddr_t newsp,
	struct uarg *args)
{

	pgno_t	needed_availsmem;

	needed_availsmem = elf_compute_availsmem(ehdrp, phdrbase);
	needed_availsmem += btoc(args->ua_stackaddr - newsp);
	needed_availsmem += runtime_loader_availsmem;
	return needed_availsmem;
}

static pgno_t
elf_compute_availsmem(Elf_Ehdr *ehdr, caddr_t phdrbase)
{
	int	i;
	pgno_t	needed_smem = 0;
	Elf_Phdr *phdr;

	for (i=0; i < (int)ehdr->e_phnum; i++) {
		phdr = (Elf_Phdr *)(phdrbase + (ehdr->e_phentsize *i));
		switch (phdr->p_type) {
		case	PT_LOAD:
			if (phdr->p_flags & PF_W)
				needed_smem += 
					btoc(phdr->p_memsz + poff(phdr->p_vaddr));
				break;
		default :
			break;
		}
	}
	return needed_smem;
}

#ifdef _R5000_CVT_WAR
static int
check_for_r5000_cvt_clean(
	struct vnode *vp,
	Elf_Phdr *phdr_base,
	int count,
	int *need_mtextp)
{
	Elf_Phdr	*phdr;
	int		num;
	int	error = 0;
	/*
	 * check if the we need to apply the R5000 cvt.d.l and cvt.s.l workaround
	 */
	*need_mtextp = 0;
	if (R5000_cvt_war &&
	    ABI_IS(ABI_IRIX5_64|ABI_IRIX5_N32, get_current_abi())) {
		*need_mtextp = 1;
		for (num = count, phdr = phdr_base; num > 0; num--, phdr++) {
			if (phdr->p_type == PT_MIPS_OPTIONS) {
				Elf_Options *eop;
				caddr_t buf;
				int off = 0;
				
				if (error = exrdhead(vp, (off_t)phdr->p_offset,
						     phdr->p_filesz, &buf)) 
					goto out;
				while (off < phdr->p_filesz) {
					eop = (Elf_Options *)(buf + off);
					off += eop->size;
					if (eop->kind == ODK_HWPATCH &&
					    (eop->info & OHW_R5KCVTL)) {
						*need_mtextp = 0;
						break;
					}
				}
out:
				if (buf)
					exhead_free(buf);
				break;
			}
		}
	}
	return(error);
}
#endif /* _R5000_CVT_WAR */
#endif	/* !ELF64 || (ELF64 && _MIPS_SIM == _ABI64) */
