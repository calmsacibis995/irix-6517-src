/*
 * ckpt_savemem.c
 *
 *      Routines for saving a processes memory state
 *
 *
 * Copyright 1995, Silicon Graphics, Inc.
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

#ident "$Revision: 1.88 $"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/vfs.h>
#include <sys/procfs.h>
#include <sys/ckpt_procfs.h>
#include <sys/mman.h>
#include <sys/syssgi.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/ckpt_sys.h>
#include "ckpt.h"
#include "ckpt_internal.h"


static int ckpt_write_thread_mappings(ckpt_memobj_t *, ckpt_obj_t *,
			   	      ckpt_prop_t *, ckpt_thread_t *, int);
static int ckpt_write_thread_memobj(ckpt_obj_t *, ckpt_thread_t *,
				    ckpt_prop_t *);

/*
 * operate on a specific thread
 */
int
ckpt_savemem_ioctl(ckpt_obj_t *co, ckpt_thread_t *ct, int cmd, void *arg)
{
	int rv;
	prthreadctl_t prt;

	prt.pt_tid = ct->ct_id;
	prt.pt_cmd = cmd;
	prt.pt_flags = PTFS_ALL | PTFD_EQL;
	prt.pt_data = arg;

	if ((rv = ioctl(co->co_prfd, PIOCTHREAD, &prt)) < 0) {
		return (-1);
	}
	return (rv);
}
/*
 * Routines for dealing with Posix unnamed semaphore
 */
ckpt_pul_t *
ckpt_get_pusema(ckpt_pul_t *pup)
{
	if (pup == NULL)
		pup = cpr.ch_pusema;
	else
		pup = pup->pul_next;

	return (pup);
}

static int
ckpt_add_pusema(ckpt_obj_t *co, ckpt_memmap_t *memmap, usync_ckpt_t *pusync)
{
	ckpt_pul_t *pup;
	int found = 0;
	ckpt_obj_t *proc;
	/*
	 * First check existence, add if not found
	 */
	for (pup = cpr.ch_pusema; pup; pup = pup->pul_next) {
		if ((pup->pul_mapid == memmap->cm_mapid)&&
		    (pup->pul_pusema.pu_usync.off == pusync->off)) {
			/*
			 * already exists
			 */
			found = 1;
			break;
		}
	}
	/*
	 * If it's been found, then only add to the list if the
	 * notify pid matches the target procs pid.  Notify pid will
	 * need to reregister notification at restart
	 */
	if ((found)&&(pup->pul_pusema.pu_usync.notify != co->co_pid))
		return (0);
	/*
	 * usync driver does lazy notify pid cleanup, so if pid is non-zero,
	 * make sure it's in the group of procs being stopped
	 */
	if (pusync->notify) {

		found = 0;

		FOR_EACH_PROC(co->co_ch->ch_first, proc) {
			if (pusync->notify == proc->co_pid) {
				found = 1;
				break;
			}
		}
		if (!found)
			/*
			 * pids not in the group...might be an error in proc
			 * grouping, but is a valid state, so clear pid and
			 * continue
			 */
			pusync->notify = 0;
	}
	pup = (ckpt_pul_t *)malloc(sizeof(ckpt_pul_t));
	if (pup == NULL) {
		cerror("malloc (%s)\n", STRERR);
		return (-1);
	}
	pup->pul_mapid = memmap->cm_mapid;
	pup->pul_pusema.pu_magic = CKPT_MAGIC_PUSEMA;
	pup->pul_pusema.pu_owner = co->co_pid;
	pup->pul_pusema.pu_uvaddr = (caddr_t)memmap->cm_vaddr + pusync->voff;
	pup->pul_pusema.pu_usync = *pusync;
	pup->pul_next = cpr.ch_pusema;
	cpr.ch_pusema = pup;

	IFDEB1(cdebug("add Posix unamed sema:pid %d voff %lx)\n",
		pup->pul_pusema.pu_owner, pup->pul_pusema.pu_uvaddr));

	return (0);
}

static int
ckpt_save_pusema(ckpt_obj_t *co, ckpt_memmap_t *memmap)
{
	ckpt_pusema_arg_t arg;
	usync_ckpt_t *pusync;
	int count;
	/*
	 * Get semaphore count
	 */
	arg.count = 0;
	arg.uvaddr = memmap->cm_vaddr;
	arg.bufaddr = (caddr_t)(-1L);

	if ((count = ioctl(co->co_prfd, PIOCCKPTPUSEMA, &arg)) < 0) {
		cerror("PIOCCKPTPUSEMA (%s)\n", STRERR);
		return (-1);
	}
	if (count == 0)
		return (0);

	pusync = (usync_ckpt_t *)malloc(count*sizeof(usync_ckpt_t));
	if (pusync == NULL) {
		cerror("malloc (%s)\n", STRERR);
		return (-1);
	}
	arg.count = count;
	arg.uvaddr = memmap->cm_vaddr;
	arg.bufaddr = (caddr_t)pusync;

	if ((count = ioctl(co->co_prfd, PIOCCKPTPUSEMA, &arg)) < 0) {
		cerror("PIOCCKPTPUSEMA (%s)\n", STRERR);
		return (-1);
	}
	assert(count == arg.count);


	while (--count >= 0) {
		if (pusync->voff <= memmap->cm_len) {

			if (ckpt_add_pusema(co, memmap, pusync) < 0) {
				free(arg.bufaddr);
				return (-1);
			}
		}
		pusync++;
	}
	free(arg.bufaddr);
	return (0);
}

/*
 * Routines for dealing with detecting memory that has been added using
 * PR_ATTACHADDR
 */
ckpt_prattach_t *
ckpt_get_prattach(ckpt_prattach_t *prp)
{
	if (prp == NULL)
		prp = cpr.ch_prattach;
	else
		prp = prp->pr_next;

	while (prp) {
		if (prp->pr_attach != -1)
			return (prp);

		prp = prp->pr_next;
	}
	return (NULL);
}

static int
ckpt_prattach_unresolved(void)
{
	ckpt_prattach_t *prp = cpr.ch_prattach;

	while (prp) {
		if ((prp->pr_attach != (pid_t)-1)&&(prp->pr_owner == (pid_t)-1))
			return (1);
		prp = prp->pr_next;
	}
	return (0);
}

static int
ckpt_lookup_prattach(ckpt_obj_t *co, ckpt_memmap_t *memmap)
{
	ckpt_prattach_t *prp = cpr.ch_prattach;

	while (prp) {
		if ((prp->pr_attach == co->co_pid)&&
		    (prp->pr_localvaddr == (caddr_t)memmap->cm_vaddr))
			return (1);
		prp = prp->pr_next;
	}
	return (0);
}
/*
 * Can only prattach from share group or ones self, so a match on a
 * region includes memory mapid + either match on share group or pid
 */
static int
ckpt_prattach_match(ckpt_obj_t *co, ckpt_memmap_t *memmap, ckpt_prattach_t *prp)
{
	if (prp->pr_mapid != memmap->cm_mapid)
		return (0);

	if (IS_SPROC(co->co_pinfo)) {
		if (prp->pr_shaddr == co->co_pinfo->cp_psi.ps_shaddr)
			return(1);
		else
			return (0);
	}
	if (prp->pr_owner == co->co_pid || prp->pr_attach == co->co_pid)
		return (1);
	else
		return (0);
	/* NOT REACHED */
}

static int
ckpt_is_prattach(	ckpt_obj_t *co,
			ckpt_memobj_t *memobj,
			ckpt_memmap_t *memmap,
			ulong_t refcnt)
{
	ckpt_prattach_t *prp;		/* current element in the list */
	ckpt_prattach_t	*pprp;		/* pointer to previous element */
	ckpt_prattach_t	*ownerp;		/* owner template */
	int claim;
	int attach;
	int ownerhits;
	/*
	 * Only interested in regions with refcnt > 1
	 *
	 * Claim ownership if can recreate an equivalent mapping.
	 * PR_ATTAHADDR creates either a STACK or MEM mapping.
	 *
	 * It doesn't really matter with a STACK mapping who creates and who
	 * does PR_ATTACHADDR so first one to 'see' it, claims it.
	 *
	 * MEM mappings really only happen in 3 ways that we should see:
	 * 1. PR_ATTACHADDR
	 * 2. execmap of mis-aligned executible
	 * 3. sbrk covering area that user had unmapped.
	 *
	 * All this is complicated, of course, by the fact that a PR_ATTACHADDR
	 * could have been done on memory created by 2, 3 above
	 *
	 * Detecting/handling case 2:
	 * So, if we have MEM *and* EXECFILE set, see if region has a vnode.
	 * If not, can be created via execmap, so claim ownership.
	 *
	 * Detecting/handling case 3:
	 * If we have MEM and !EXECFILE and the virtual address is between 
	 * brkbase and brkbase + brksize, we claim it.  Must be ours.
	 *
	 * And a final complication is that after PR_ATTAHADDR, a process
	 * can fork, giving the child a CW copy..i.e. private copy, of
	 * the region with a pregion type still set to MEM.  This case
	 * we'll handle by sub'ing a MAP_PRIVATE mapping of the underlying
	 * vnode.
	 */
	if (refcnt == 1)
		return (0);

	if (memmap->cm_flags & CKPT_MEM) {

		claim = 0;
		attach = 1;

		if ((memmap->cm_flags & CKPT_EXECFILE)&&
		    ((memmap->cm_xflags & CKPT_VNODE) == 0)) {
			claim = 1;
			attach = 0;
		}
		if (memmap->cm_vaddr >= memobj->cmo_brkbase &&
		    memmap->cm_vaddr < memobj->cmo_brkbase + memobj->cmo_brksize) {
			claim = 1;
			attach = 0;
		}
	} else if (memmap->cm_flags & CKPT_STACK) {
		/*
		 * Stacks created by kernel have RWX permission
		 */
#define PROT_ALL (PROT_READ | PROT_WRITE | PROT_EXEC)
		if ((memmap->cm_maxprots & PROT_ALL) == PROT_ALL)
			claim = 1;
		else
			claim = 0;
		attach = 1;
	} else {
		claim = 1;
		attach = 0;
	}

	ownerhits = 0;
	ownerp = NULL;
	pprp = NULL;

	for (prp = cpr.ch_prattach; prp; pprp = prp, prp = prp->pr_next) {
		if (ckpt_prattach_match(co, memmap, prp)) {

			if (claim && (prp->pr_owner == -1)) {
				ownerhits++;
				/*
				 * The entry must've been added by an attach
				 */
				prp->pr_owner = co->co_pid;
				prp->pr_vaddr = memmap->cm_vaddr;

			} else if (attach && (prp->pr_attach == -1)) {
				/*
				 * entry added by owner
				 */
				assert(prp->pr_owner != -1);
				prp->pr_attach = co->co_pid;
				prp->pr_localvaddr = (caddr_t)memmap->cm_vaddr;
				prp->pr_prots = memmap->cm_maxprots;

				return (1);
			} else if (attach &&( prp->pr_owner != -1)) {
				/*
				 * entry has owner and attacher, we must be an
				 * additional attacher
				 */
				assert(refcnt > 2);
				ownerp = prp;
			}
		}
	}
	if (ownerhits)
		/*
		 * Is the owner and recorded owner info in at least one
		 * entry
		 */
		return (0);

	prp = (ckpt_prattach_t *)malloc(sizeof(ckpt_prattach_t));
	if (prp == NULL) {
		cerror("cannot malloc (%s)\n", STRERR);
		return (-1);
	}
	prp->pr_magic = CKPT_MAGIC_PRATTACH;
	prp->pr_mapid = memmap->cm_mapid;
	prp->pr_shaddr = co->co_pinfo->cp_psi.ps_shaddr;
	prp->pr_next = NULL;
	if (pprp)
		pprp->pr_next = prp;
	else
		cpr.ch_prattach = prp;

	if (claim) {
		prp->pr_vaddr = memmap->cm_vaddr;
		prp->pr_owner = co->co_pid;
		prp->pr_attach = -1;

		return (0);

	}
	if (ownerp) {
		prp->pr_mapid = ownerp->pr_mapid;
		prp->pr_vaddr = ownerp->pr_vaddr;
		prp->pr_owner = ownerp->pr_owner;
		prp->pr_attach = co->co_pid;
		prp->pr_localvaddr = (caddr_t)memmap->cm_vaddr;
		prp->pr_prots = memmap->cm_maxprots;

		return (1);
	}
	prp->pr_mapid = memmap->cm_mapid;
	prp->pr_vaddr = (caddr_t)-1L;
	prp->pr_owner = -1;
	prp->pr_attach = co->co_pid;
	prp->pr_localvaddr = (caddr_t)memmap->cm_vaddr;
	prp->pr_prots = memmap->cm_maxprots;

	return (1);
}

static int
ckpt_resolve_sproc_attach(ckpt_obj_t *co)
{
	ckpt_memobj_t memobj, *mo = &memobj;
	ckpt_thread_t *ct = co->co_thread;
	tid_t tid = ct->ct_id;
	caddr_t saddr;		/* region start address */
	ckpt_mstat_arg_t ckpt_mstat;
	struct stat	statbuf;
	prmap_sgi_arg_t prmaparg;
	prmap_sgi_t     *prmap;
	prmap_sgi_arg_t ckptmap;
	ckpt_getmap_t	*ckpt_getmap;
	ckpt_memmap_t	memmap;
	int nsegs;		/* number of attribute segments */
	int npregs;		/* number of pregions */
	int i;
	int refcnt;
	int domap = 0;
	int local_only;

	if (co->co_flags & CKPT_CO_SADDR) {
		local_only = 0;
		mo->cmo_brkbase = co->co_pinfo->cp_psi.ps_brkbase;
		mo->cmo_brksize = co->co_pinfo->cp_psi.ps_brksize;
	} else {
		local_only = 1;
		mo->cmo_brkbase = 0;
		mo->cmo_brksize = 0;
	}
	mo->cmo_stackbase = co->co_pinfo->cp_psi.ps_stackbase;
	mo->cmo_stacksize = co->co_pinfo->cp_psi.ps_stacksize;

	prmaparg.pr_vaddr = NULL;
	prmaparg.pr_size = 0;

	if (ckpt_savemem_ioctl(co, ct, PIOCNMAP, &nsegs) < 0) {
		cerror("PIOCNMAP (%s)\n", STRERR);
		return (-1);
	}
	/*
	 * Allocate 1 extra for null termination and 1 extra for
	 * brkbase segment
	 */
	prmaparg.pr_vaddr = calloc(nsegs + 2, sizeof(prmap_sgi_t));
	prmaparg.pr_size = (nsegs+2) * sizeof(prmap_sgi_t);
	prmap = (prmap_sgi_t *)prmaparg.pr_vaddr;
	/*
	 * PIOCMAP_SGI retuns actual number of segments
	 */
	if ((nsegs = ckpt_savemem_ioctl(co, ct, PIOCMAP_SGI, &prmaparg)) < 0) {
		cerror("PIOCMAP_SGI (%s)\n", STRERR);
		free(prmap);
		return (-1);
	}
	/*
	 * Number of pregs is <= nsegs
	 */
	ckptmap.pr_vaddr = calloc(nsegs, sizeof(ckpt_getmap_t));
	ckptmap.pr_size = nsegs * sizeof(ckpt_getmap_t);
	ckpt_getmap = (ckpt_getmap_t *)ckptmap.pr_vaddr;

	if ((npregs = ckpt_savemem_ioctl(co, ct, PIOCCKPTGETMAP, &ckptmap)) < 0) {
		cerror("PIOCCKPTGETMAP (%s)\n", STRERR);
		goto errout;
	}
	/*
	 * Now start working through attached virtual space...
	 */
	for (i = 0; i < npregs; ckpt_getmap++, i++) {

		bzero(&memmap, sizeof(memmap));

		saddr = ckpt_getmap->m_vaddr;

		memmap.cm_magic = CKPT_MAGIC_MEMMAP;

		memmap.cm_vaddr = saddr;
		memmap.cm_len = ckpt_getmap->m_size;
		memmap.cm_mapid = ckpt_getmap->m_mapid;
		memmap.cm_maxprots = ckpt_getmap->m_prots;
		memmap.cm_flags = ckpt_getmap->m_flags;
		memmap.cm_maxoff = ckpt_getmap->m_maxoff;
		memmap.cm_pid = 0;
		memmap.cm_rdev = 0;
		memmap.cm_shmid = ckpt_getmap->m_shmid;
		memmap.cm_auxptr = NULL;
		memmap.cm_auxsize = 0;
		memmap.cm_mflags = 0;
		memmap.cm_xflags = 0;
		memmap.cm_cflags = 0;
		memmap.cm_nmodmin = 0;
		memmap.cm_nmod = 0;
		memmap.cm_modlist = NULL;
		memmap.cm_pagesize = getpagesize();
		memmap.cm_fd = -1;
		memmap.cm_tid = tid;

		if (ckpt_getmap->m_flags & CKPT_AUTOGROW)
			memmap.cm_mflags |= MAP_AUTOGROW;
		if (ckpt_getmap->m_flags & CKPT_LOCAL)
			memmap.cm_mflags |= MAP_LOCAL;
		if (ckpt_getmap->m_flags & CKPT_AUTORESRV)
			memmap.cm_mflags |= MAP_AUTORESRV;

		memmap.cm_foff = prmap->pr_off;
		memmap.cm_nsegs = 0;
		/*
		 * stat underlying vnode
		 */
		ckpt_mstat.statvers = _STAT_VER;
		ckpt_mstat.vaddr = memmap.cm_vaddr;
		ckpt_mstat.sbp = &statbuf;

		if (ckpt_savemem_ioctl(co, ct, PIOCCKPTMSTAT, &ckpt_mstat) == 0) {
			memmap.cm_xflags |= CKPT_VNODE;
			memmap.cm_rdev = statbuf.st_rdev;

			if (ckpt_mapattr_special(co, co->co_ch, &memmap, 
			    &statbuf) < 0) {
				cerror("could not get special attributes: %s\n", STRERR);
				goto errout;
			}
		} else if (oserror() != ENOENT) {
			cerror("PIOCCKPTMSTAT (%s)\n", STRERR);
			goto errout;
		}
		/*
		 * Things we don't save:
		 * -memory used for checkpointing
		 * -sproc shared mappings if another share member has already
		 *  done it
		 * Unless:
		 * cm_pid indicates this proc must save mapping
		 */
		domap = 1;

		if (IS_CKPTRESERVED(memmap.cm_vaddr)||
		   (local_only && !(ckpt_getmap->m_flags & CKPT_LOCAL)))
			domap = 0;

		if (memmap.cm_pid)
			domap = ((memmap.cm_pid == co->co_pid)&&
				 (co->co_thread == ct));

		IFDEB1(cdebug("ckpt_resolve_prattach: i=%d cm_xflags=%x\n",
			i, memmap.cm_xflags));

		if (!domap)
			continue;

		refcnt = (int)CKPT_REFCNT(prmap);

		if ((memmap.cm_flags & CKPT_MAPFILE) &&
		    ((memmap.cm_mflags & (MAP_SHARED|MAP_PRIVATE)) == 0))
			/*
			 * mmap sets COW for private mappings, so if
			 * we get here without PRIVATE, COW was not
			 * set, so must be SHARED
			 */
			memmap.cm_mflags |= MAP_SHARED;

		if (ckpt_is_prattach(co, mo, &memmap, refcnt) < 0)
			goto errout;
	}
	free(ckptmap.pr_vaddr);
	free(prmaparg.pr_vaddr);
	return (0);

errout:
	free(ckptmap.pr_vaddr);
	free(prmaparg.pr_vaddr);

	return (-1);
}
/*
 * Examine each sproc, look for and resolve all prattach memory.  If can't
 * means that a share member is exiting, we don't have it in our list of procs
 * because it's exiting and not visible, and it has not yet released its vm.
 * In that case, retry.
 */
int
ckpt_resolve_prattach(ch_t *ch)
{
	ckpt_obj_t *co;
	int retries = 0;
#define MAXRETRY 60
again:
	for (co = ch->ch_first; co; co = co->co_next) {
		if (IS_SPROC(co->co_pinfo)) {
			/* 
			 * Sprocs are single threaded
			 */
			if (ckpt_resolve_sproc_attach(co) < 0)
				return (-1);
		}
	}
	if (ckpt_prattach_unresolved() == 0)
		return (0);
	/*
	 * Hmm.  Couldn't resolve it all. The assumption here is that we're racing with
	 * an exiting process.   Yield the processor, increment retry count, etc
	 */
#ifdef DEBUG
	cdebug("Unresolved prattach, retry count %d\n", retries);
#endif
	if (retries++ > MAXRETRY) {
		setoserror(ECKPT);
		cerror("Could not resolve attached memory segments\n");
		return (-1);
	}
	sleep(retries);

	goto again;
	/*
	 * NOTREACHED
	 */
}
/*
 * Routines for dealing with svr4/inherited shared memory
 *
 * For example, mmap /dev/zero with MAP_SHARED, then fork
 */
static int
ckpt_depend_add(pid_t spid, pid_t dpid)
{
	int i;
	ckpt_dpl_t *dpl = &cpr.ch_depend;
	ckpt_dpl_t *tail = NULL;

	while (dpl && dpl->dpl_count == CKPT_DPLCHUNK) {
		tail = dpl;
		dpl = dpl->dpl_next;
	}
	if (!dpl) {
		assert(tail);

		dpl = (ckpt_dpl_t *)malloc(sizeof(ckpt_dpl_t));
		if (!dpl) {
			cerror("malloc");
			return (-1);
		}
		dpl->dpl_count = 0;
		dpl->dpl_next =  NULL;
		tail->dpl_next = dpl;
	}
	i = dpl->dpl_count++;

	dpl->dpl_depend[i].dp_magic = CKPT_MAGIC_DEPEND;
	dpl->dpl_depend[i].dp_spid = spid;
	dpl->dpl_depend[i].dp_dpid = dpid;

	return (0);
}

int
ckpt_get_dependcnt(void)
{
	ckpt_dpl_t *dpl = &cpr.ch_depend;
	int count = 0;

	while (dpl) {
		count += dpl->dpl_count;
		dpl = dpl->dpl_next;
	}
	return (count);
}
	
ckpt_dpl_t *
ckpt_get_depend(ckpt_dpl_t *dpl)
{
	if (dpl == NULL)
		dpl = &cpr.ch_depend;
	else
		dpl = dpl->dpl_next;
	return (dpl);
}

static ckpt_ismmem_t *ismlist = NULL;

static int
ckpt_is_ismmem(ckpt_obj_t *co, ckpt_memmap_t *memmap, ulong_t refcnt)
{
	ckpt_ismmem_t *ismp;
	ckpt_ismmem_t *new;
	/*
	 * cull down the candidates
	 */
	if (refcnt == 1)
		return (0);
	if ((memmap->cm_flags & CKPT_MAPFILE) == 0)
		return (0);
	if ((memmap->cm_mflags & MAP_SHARED) == 0)
		return (0);
	for (ismp = ismlist; ismp; ismp = ismp->ism_next) {
		if ((memmap->cm_mapid == ismp->ism_mapid) &&
		    (co->co_pid != ismp->ism_pid) && 
		    (memmap->cm_vaddr == ismp->ism_vaddr)) {
		    /* the same: region_id, vaddr, and not same proc */
			if (memmap->cm_len != ismp->ism_len) {
				cnotice("shared memory hit with len "
					"mis-match\n");
				IFDEB1(cdebug("pid %d %d  len 0x%0x 0x%0x\n",
					co->co_pid, ismp->ism_pid, 
					memmap->cm_len, ismp->ism_len));
				/* don't checkpoint, we are in trouble */
				return (-1);
			}
			if (ckpt_depend_add(ismp->ism_pid, co->co_pid) < 0)
				return (-1);

			memmap->cm_pid = ismp->ism_pid;
			IFDEB1(cdebug("dep ismmem: spid %d dpid %d addr  "
				"0x%0llx len 0x%0llx\n",co->co_pid,ismp->ism_pid,
				memmap->cm_vaddr,memmap->cm_len));

			return (1);
		}
	}
	new = (ckpt_ismmem_t *)malloc(sizeof(ckpt_ismmem_t));
	if (new == NULL) {
		cerror("malloc");
		return (-1);
	}
	new->ism_mapid = memmap->cm_mapid;
	new->ism_pid = co->co_pid;
	new->ism_vaddr = memmap->cm_vaddr;
	new->ism_len = memmap->cm_len;
	new->ism_next = ismlist;
	ismlist = new;

	IFDEB1(cdebug("add ismmem: pid %d addr: 0x%0llx len 0x%0llx\n",
			co->co_pid, memmap->cm_vaddr, memmap->cm_len));
	return (0);
}

/*
 * Routines for dealing wqith system V shared mem (shmXXX)
 */
static ckpt_shm_t *shmlist;

/*
 * Add a new element to shmlist
 */
static int
ckpt_shmlist_new(caddr_t mapid, int shmid, pid_t pid)
{
	ckpt_shm_t *shmp;

	shmp = (ckpt_shm_t *)malloc(sizeof(ckpt_shm_t));
	if (shmp == NULL) {
		cerror("malloc");
		return (-1);
	}
	shmp->shm_mapid = mapid;
	shmp->shm_id = shmid;
	shmp->shm_pid = pid;
	shmp->shm_next = shmlist;
	shmlist = shmp;

	return (0);
}
/*
 * lookup shmlist entry on mapid
 */
static ckpt_shm_t *
ckpt_shm_lookup_mapid(caddr_t mapid)
{
	ckpt_shm_t *shmp;

	for (shmp = shmlist; shmp; shmp = shmp->shm_next) {

		if (mapid == shmp->shm_mapid)
			return (shmp);
	}
	return (NULL);
}
/*
 * lookup shmlist entry on shmid
 */
static ckpt_shm_t *
ckpt_shm_lookup_shmid(int shmid)
{
	ckpt_shm_t *shmp;

	for (shmp = shmlist; shmp; shmp = shmp->shm_next) {

		if (shmid == shmp->shm_id)
			return (shmp);
	}
	return (NULL);
}

static int
ckpt_shm_is_handled(ckpt_obj_t *co, ckpt_memmap_t *memmap)
{
	ckpt_shm_t *shmp;
	struct shmid_ds *shmdsp;
	int rv;

	shmp = ckpt_shm_lookup_mapid(memmap->cm_mapid);
	if (shmp) {

		if (ckpt_depend_add(shmp->shm_pid, co->co_pid) < 0)
                               return (-1);

		memmap->cm_cflags |= CKPT_DEPENDENT;

		return (1);
	}
	/*
	 * Fetch interesting info about shared mem and attach to memmap struct
	 */
	shmdsp = (struct shmid_ds *)malloc(sizeof(struct shmid_ds));
	if (shmdsp == NULL) {
		cerror("malloc");
		return (-1);
	}
	rv = shmctl(memmap->cm_shmid, IPC_STAT, shmdsp);
	if (rv < 0) {
		free(shmdsp);
		if (oserror() != EINVAL) {
			cerror("shmctl");
			return (-1);
		}
		/*
		 * get here when IPC_RMID has been done
		 */
		memmap->cm_shmid = -1;	/* any id will do */
	} else {
		memmap->cm_auxptr = shmdsp;
		memmap->cm_auxsize = sizeof(struct shmid_ds);
	}
	/*
	 * Add this shared mem to the list
	 */
	if (ckpt_shmlist_new(memmap->cm_mapid, memmap->cm_shmid, co->co_pid))
		return (-1);

	return (0);
}
/*
 * We get here to save sysV shared mem that is *not* mapped into any
 * process.  That is, shmget was done, but shmat was not
 */
static int
ckpt_write_shm(ckpt_obj_t *co, ckpt_prop_t *pp, int shmid)
{
	ckpt_shmobj_t shmobj;
	int shmfd;
	void *shmaddr;
	size_t offset;
	size_t shmsz;
	size_t iosize;
	long rc;

	shmobj.shm_magic = CKPT_MAGIC_SHM;
	shmobj.shm_id = shmid;

	if (shmctl(shmid, IPC_STAT, &shmobj.shm_ds) < 0) {
		if (oserror() != EINVAL) {
			cerror("shmctl");
			return (-1);
		}
		/*
		 * id no longer exists...do nothing
		 */
		IFDEB1(cdebug("shmid %d removed\n", shmid));

		return (0);
	}
	shmaddr = shmat(shmid, NULL, SHM_RDONLY);
	if (shmaddr == (void *)(-1L)) {
		cerror("shmat id %d (%s)\n", shmid, STRERR);
		return (-1);
	}
	ckpt_set_shmpath(co, shmobj.shm_path, shmid);

	shmfd = open(shmobj.shm_path, O_WRONLY|O_CREAT, S_IRUSR);
	if (shmfd < 0) {
		cerror("open shm (%s)\n", STRERR);
		shmdt(shmaddr);
		return (-1);
	}
	IFDEB1(cdebug("fchown file %s: uid %d, gid %d\n", shmobj.shm_path,
		shmobj.shm_ds.shm_perm.uid, shmobj.shm_ds.shm_perm.gid));
	if (fchown(shmfd, shmobj.shm_ds.shm_perm.uid,
					shmobj.shm_ds.shm_perm.gid)) {
		shmdt(shmaddr);
		unlink(shmobj.shm_path);
		close(shmfd);
		cerror("chown shm (%s)\n", STRERR);
		return (-1);
	}

	offset = 0;
	shmsz = shmobj.shm_ds.shm_segsz;

	while (shmsz) {

		iosize = min(shmsz, CKPT_IOBSIZE);

		if (write(shmfd, (char *)shmaddr + offset, iosize) < 0) {
			cerror("write shm (%s)\n", STRERR);
			shmdt(shmaddr);
			unlink(shmobj.shm_path);
			close(shmfd);
			return (-1);
		}
		shmsz -= iosize;
		offset += iosize;
	}
	close(shmfd);
	shmdt(shmaddr);

	CWRITE(co->co_sfd, &shmobj, sizeof(shmobj), 1, pp, rc);
	if (rc < 0) {
		cerror("write chm to statefile (%s)\n", STRERR);
		return (-1);
	}
	return (0);
}

int
ckpt_save_shm(ckpt_obj_t *co, ckpt_prop_t *pp)
{
	ckpt_shm_arg_t arg;
	int count;
	int i;

	if (co->co_flags & CKPT_CO_ZOMBIE)
		return (0);

	count = ioctl(co->co_prfd, PIOCCKPTSHM, NULL);
	if (count < 0) {
		cerror("PIOCCKPTSHM (%s)\n", STRERR);
		return (-1);
	}
	IFDEB1(cdebug("shared mem id count %d\n", count));

	if (count == 0)
		return (0);

	arg.count = count;
	arg.shmid = (int *)malloc(count * sizeof(int));

	if (arg.shmid == NULL) {
		cerror("malloc (%s)\n", STRERR);
		return (-1);
	}
	count = ioctl(co->co_prfd, PIOCCKPTSHM, &arg);
	if (count < 0) {
		cerror("PIOCCKPTSHM (%s)\n", STRERR);
		free(arg.shmid);
		return (-1);
	}
	if (count != arg.count) {
		cerror("shmid count mis-match (%d vs. %d)\n", count, arg.count);
		setoserror(ECKPT);
		free(arg.shmid);
		return (-1);
	}
	for (i = 0; i < count; i++) {

		IFDEB1(cdebug("shmid[%d]:%d\n", i, arg.shmid[i]));

		if (ckpt_shm_lookup_shmid(arg.shmid[i]) == NULL) {
			/*
			 * we need to save this one
			 */
			IFDEB1(cdebug("!found shmid %d\n", arg.shmid[i]));

			if (ckpt_shmlist_new((caddr_t)(-1L), arg.shmid[i], co->co_pid)) {
				free(arg.shmid);
				return (-1);
			}
			if (ckpt_write_shm(co, pp, arg.shmid[i]) < 0 ) {
				free(arg.shmid);
				return (-1);
			}
		}
	}
	free(arg.shmid);
	return (0);
}

static int
ckpt_read_shm(ckpt_ta_t *tp, char *path, char *shmaddr, size_t size)
{
	int shmfd;
	size_t iosize;

	shmfd = ckpt_open(tp, path, O_RDONLY, 0);
	if (shmfd < 0) {
		cerror("open shm image file %s (%s)\n", path, STRERR);
		return (-1);
	}
	while (size) {

		iosize = min(size, CKPT_IOBSIZE);

		if (read(shmfd, shmaddr, iosize) < 0) {
			cerror("read shm image file (%s)\n", STRERR);
			close(shmfd);
			return (-1);
		}
		shmaddr += iosize;
		size -= iosize;
	}
	close (shmfd);
	return (0);
}

int
ckpt_restore_shm(ckpt_ta_t *tp, ckpt_magic_t magic)
{
	ckpt_shmobj_t shmobj;
	int id = 0;
	void *shmaddr;

	if (read(tp->sfd, &shmobj, sizeof(shmobj)) < 0) {
		cerror("restore shm read object (%s)\n", STRERR);
		return (-1);
	}
	if (shmobj.shm_magic != magic)  {
		cerror("incorrect shm object magic %8.8s (v.s. %8.8s)\n",
                        &shmobj.shm_magic, &magic);
		setoserror(EINVAL);
		return (-1);
	}
	id = shmget_id(	shmobj.shm_ds.shm_perm.key, 
			shmobj.shm_ds.shm_segsz,
			(int)shmobj.shm_ds.shm_perm.mode|IPC_CREAT,
			shmobj.shm_id);
	if (id < 0) {
		if (oserror() == ENOSPC) {
			cerror("sys V shared memory restore failed for shmid:" 
			"%d: id in use, return id %d, err %d \n", 
						shmobj.shm_id, id, oserror());
		} else {
			cerror("sys V shared memory restore failed for shmid:"
			"%d, return id %d: err %d\n", 
						shmobj.shm_id, id, oserror());
		}
		return (-1);
	}
	shmaddr = shmat(id, NULL, 0);

	if (shmaddr == (void *)(-1L)) {
		cerror("shmat id %d failed (%s)\n", id, STRERR);
		shmctl(IPC_RMID, id);
		return (-1);
	}
	if (ckpt_read_shm(tp, shmobj.shm_path, (char *)shmaddr, shmobj.shm_ds.shm_segsz)) {
		shmctl(IPC_RMID, id);
		return (-1);
	}
	shmdt(shmaddr);

	return (0);
}

static int
ckpt_remap_disp(	ckpt_obj_t *co,
			struct stat *sb,
			char *path,
			ckpt_memmap_t *memmap,
			ch_t *ch)
{
	int handled;
	dev_t dev;
	ino_t ino;
	mode_t mode;

	if (memmap->cm_xflags & (CKPT_PHYS|CKPT_PRATTACH|CKPT_ISMEM)) {
		memmap->cm_cflags |= CKPT_REMAP;

	} else if (memmap->cm_xflags & CKPT_SHMEM) {
		memmap->cm_cflags |= CKPT_REMAP;
		if ((handled = ckpt_shm_is_handled(co, memmap)) < 0)
			return (-1);
		if (!handled)
			memmap->cm_cflags |= CKPT_SAVEALL;

	} else if ((memmap->cm_flags & (CKPT_MAPFILE|CKPT_EXECFILE)) == 0) {
		/*
		 * Strictly anonymous.
		 */
		memmap->cm_cflags |= (CKPT_SAVEMOD|CKPT_ANON);

	} else if (memmap->cm_mflags & MAP_SHARED) {
		/*
		 * Shared executibles get turned into private mappings
		 * when/if they get write enabled. Just remap the file
		 *
		 * Otherwise check maxprots.  If can't be write
		 * enabled, just remap
		 *
		 * Otherwise is or can be write enabled.  Need to
		 * save contents.  If contents saved by
		 * handling of file, just remap.  Otherwise
		 * save all (and remap)
		 */
		int filemode;

		dev = sb->st_dev;
		ino = sb->st_ino;
		mode = sb->st_mode;

		memmap->cm_cflags |= CKPT_REMAP;

		filemode = ckpt_attr_get_mode(ch, path, dev, ino, mode,
					memmap->cm_xflags & CKPT_UNLINKED, 0);

		if (((memmap->cm_flags & CKPT_EXECFILE) == 0) &&
		     (memmap->cm_maxprots & PROT_WRITE) &&
		     (filemode != CKPT_MODE_REPLACE) &&
		     (filemode != CKPT_MODE_SUBSTITUTE) &&
		     (filemode != CKPT_MODE_RELOCATE))
			memmap->cm_cflags |= CKPT_SAVEALL;

	} else { /* MAP_PRIVATE */

		memmap->cm_cflags |= CKPT_REMAP|CKPT_SAVEMOD;
	}

	if(memmap->cm_xflags & CKPT_FETCHOP) {
		/* always save modified pages for pregions with fetchop */
		memmap->cm_cflags &= ~CKPT_SAVEALL;
		memmap->cm_cflags |= CKPT_SAVEMOD;
	}

	if (memmap->cm_xflags & CKPT_SPECIAL)
		return (ckpt_remap_disp_special(memmap));

	return (0);
}

/* 
 * find out if vaddr is in fetchop seg.
 */
static int 
ckpt_addr_in_fetchop(ckpt_memmap_t *memmap, ckpt_seg_t *mapseg, caddr_t vaddr)
{
	int i;
	for (i = 0; i < memmap->cm_nsegs; i++) {
		if ((mapseg[i].cs_notcached != 0) && 
		    (vaddr >= (caddr_t)mapseg[i].cs_vaddr) && 
		    (vaddr<(caddr_t) mapseg[i].cs_vaddr+ mapseg[i].cs_len)) {
			IFDEB1(cdebug("fetchop seg: 0x%llx len = 0x%x\n",
				mapseg[i].cs_vaddr, mapseg[i].cs_len));
			return 1;
		}
	}
	return 0;
}

/*
 * ckpt_save_modified
 *
 * Save modified pages for a mapping
 */
static int
ckpt_save_modified(ckpt_obj_t *co, ckpt_thread_t *ct, 
				ckpt_memmap_t *memmap, ckpt_seg_t *mapseg)
{
	prio_t prio;		/* /proc io request struct */
	caddr_t	*addrlist;	/* list of modified page addresses */
	prpgd_sgi_t *prpgdp;
	pgd_t *pgdp;
	long npages;
	long nmpages;
	long midx;
	long pidx;
	caddr_t vaddr;
	size_t len;
	char *membuf;
	ssize_t rc;
	int mfd = co->co_mfd;
	size_t maxio = CKPT_IOBSIZE;

	nmpages = memmap->cm_nmodmin;
	npages = (long)memmap->cm_len / memmap->cm_pagesize;

	IFDEB1(cdebug("Saving modified %llx size %x cflag %x mflags %x"
		"flags %x xflags %x\n", memmap->cm_vaddr,
		memmap->cm_len, memmap->cm_cflags,
		memmap->cm_mflags, memmap->cm_flags,
		memmap->cm_xflags));

	/*
	 * If min number mod'd matches total page count, then all pages
	 * are modified, so do a save all instead
	 */
	if ((nmpages == npages) && !(memmap->cm_xflags & CKPT_FETCHOP)) {
		memmap->cm_nmod = npages;
		memmap->cm_cflags &= ~CKPT_SAVEMOD;
		memmap->cm_cflags |= CKPT_SAVEALL;
		return (ckpt_save_all(co, ct, memmap));
	}
	/*
	 * allocate the max possible size
	 */
	addrlist = (caddr_t *)malloc(npages * sizeof(caddr_t));
	if (addrlist == NULL) {
		cerror("malloc (%s)\n", STRERR);
		return (-1);
	}
	prpgdp = (prpgd_sgi_t *)malloc(sizeof(prpgd_sgi_t) +
					(npages*sizeof(pgd_t)));
	if (prpgdp == NULL) {
		cerror("malloc (%s)\n", STRERR);
		free(addrlist);
		return (-1);
	}
	bzero(prpgdp, sizeof(prpgd_sgi_t) + (npages*sizeof(pgd_t)));
	prpgdp->pr_vaddr = memmap->cm_vaddr;
	prpgdp->pr_pglen = npages;

	if (ckpt_savemem_ioctl(co, ct, PIOCPGD_SGI, prpgdp) < 0) {
		cerror("PIOCPGD_SGI (%s)\n", STRERR);
		free(prpgdp);
		free(addrlist);
		return (-1);
	}
	/*
	 * Find modified pages
	 */
	midx = 0;

	for (pidx = 0, pgdp = prpgdp->pr_data, vaddr = memmap->cm_vaddr;
		pidx < npages; pidx++, pgdp++, vaddr += memmap->cm_pagesize) {
		if (pgdp->pr_flags&(PGF_ISDIRTY|PGF_WRTHISTORY|PGF_ANONBACK)) {
			if(!(memmap->cm_xflags & CKPT_FETCHOP))
				addrlist[midx++] = vaddr;
			else if(!ckpt_addr_in_fetchop(memmap, mapseg, vaddr))
				addrlist[midx++] = vaddr;
		}
	}
#ifdef DEBUG_NMPAGES
	assert(npages >= nmpages);
	/* number of mod'd from cm_nmod may have missed some */
	/* a mis-match might not be an error, but it is noteworthy */
	if (midx != nmpages) {
		cnotice("ckpt_save_modified:midx %d, nmpages %d\n", midx, nmpages);
	}
	assert((memmap->cm_mflags & MAP_SHARED)||(midx >= nmpages));
#endif
	memmap->cm_nmod = nmpages = midx;

	IFDEB1(cdebug("npages=%d nmpages=%d\n", npages, nmpages));

	free(prpgdp);

	if (nmpages == 0) {
		free(addrlist);
		return (0);
	}
	/*
	 * If all the pages are modified, do a save all
	 */
	if (nmpages == npages) {
		free(addrlist);
		memmap->cm_cflags &= ~CKPT_SAVEMOD;
		memmap->cm_cflags |= CKPT_SAVEALL;
		return (ckpt_save_all(co, ct, memmap));
	}
	membuf = memalign(memmap->cm_pagesize, maxio);
	/*
	 * In case swap memory is tight...
	 */
	while (membuf == NULL && maxio > memmap->cm_pagesize) {
		maxio = maxio >> 1;
		membuf = memalign(memmap->cm_pagesize, maxio);
	}
	if (membuf == NULL) {
		cerror("malloc (%s)\n", STRERR);
		return (-1);
	}
	/*
	 * Read from proc, write to statefile
	 */
	vaddr = addrlist[0];
	len = memmap->cm_pagesize;

	for (midx = 1; midx < nmpages; midx++) {
		/*
		 * Build as big a request as possible
		 */
		if ((addrlist[midx] == (vaddr + len)) &&
		    ((len + memmap->cm_pagesize) <= maxio))  {
			len += memmap->cm_pagesize;
			continue;
		}
		/*
		 * Current address can't be coalesced with previous.
		 * Write out acumulated request and re-init
		 */
		prio.pi_offset = (off64_t)vaddr;
		prio.pi_base = membuf;
		prio.pi_len = (ssize_t)len;

		if (ckpt_savemem_ioctl(co, ct, PIOCREAD, &prio) < 0) {
			cerror("read proc image (%s)\n", STRERR);
			goto bad;
		}
		rc = write(mfd, membuf, len);
		if (rc < 0) {
			cerror("Failed to write (%s)\n", STRERR);
			goto bad;
		}
		vaddr = addrlist[midx];
		len = memmap->cm_pagesize;
	}
	/*
	 * Flush dangling request
	 */
	prio.pi_offset = (off64_t)vaddr;
	prio.pi_base = membuf;
	prio.pi_len = (ssize_t)len;

	if (ckpt_savemem_ioctl(co, ct, PIOCREAD, &prio) < 0) {
		cerror("read proc image (%s)\n", STRERR);
		goto bad;
	}
	rc = write(mfd, membuf, len);
	if (rc < 0) {
		cerror("Failed to write (%s)\n", STRERR);
		goto bad;
	}
	memmap->cm_modlist = addrlist;

	/* save fetchop pages if any */
	if(memmap->cm_xflags & CKPT_FETCHOP) {
		if(ckpt_save_special(co, memmap, mapseg)) {
			cerror("ckpt_save_fetchop: %s\n", STRERR);
			goto bad;
		}
	}
	free(membuf);
	return (0);
bad:
	free(membuf);
	free(addrlist);
	return (-1);
}

int 
ckpt_save_all(ckpt_obj_t *co, ckpt_thread_t *ct, ckpt_memmap_t *memmap)
{
	prio_t prio;		/* /proc io request struct */
	ulong_t npasses;
	ulong_t i;
	char *membuf;
	ulong_t bufsize;
	ssize_t rc;
	int mfd = co->co_mfd;
	struct stat sb;
	/*
	 * If it's a mapped file, stat underlying object and
	 * limit length based upon it's size
	 */
	if (memmap->cm_flags & CKPT_MAPFILE) {
		ckpt_mstat_arg_t ckpt_mstat;

                ckpt_mstat.statvers = _STAT_VER;
                ckpt_mstat.vaddr = memmap->cm_vaddr;
                ckpt_mstat.sbp = &sb;

                if (ckpt_savemem_ioctl(co, ct, PIOCCKPTMSTAT, &ckpt_mstat) < 0) {
			cerror("stat failure on %s (%s)\n", memmap->cm_path, STRERR);
			return (-1);
		}
		if (sb.st_size != 0)
			memmap->cm_savelen = min(sb.st_size, memmap->cm_len);
		else
			memmap->cm_savelen = memmap->cm_len;
#ifdef DEBUG
		if (memmap->cm_savelen < memmap->cm_len) {
			printf("file %s savelen %ld len %ld\n",
				memmap->cm_path, memmap->cm_savelen, memmap->cm_len);
		}
#endif
	} else
		memmap->cm_savelen = memmap->cm_len;

	bufsize = min(CKPT_IOBSIZE, memmap->cm_savelen);
	membuf = memalign(memmap->cm_pagesize, bufsize);
	/*
	 * In case swap memory is tight...
	 */
	while (membuf == NULL && bufsize > memmap->cm_pagesize) {
		bufsize = bufsize >> 1;
		membuf = memalign(memmap->cm_pagesize, bufsize);
	}
	if (membuf == NULL) {
		cerror("malloc (%s)\n", STRERR);
		return (-1);
	}
	npasses = memmap->cm_savelen / bufsize;

	prio.pi_offset = (off64_t)memmap->cm_vaddr;
	prio.pi_base = membuf;
	prio.pi_len = (ssize_t)bufsize;

	IFDEB1(cdebug("save_all %llx size %x cflag %x mflags %x"
		"flags %x xflags %x\n", memmap->cm_vaddr, memmap->cm_len, 
		memmap->cm_cflags, memmap->cm_mflags, memmap->cm_flags, 
		memmap->cm_xflags));

	for (i = 0; i < npasses; i++) {
		if (ckpt_savemem_ioctl(co, ct, PIOCREAD, &prio) < 0) {
			cerror("read proc image:save all %d (%s)\n", oserror(), STRERR);
			free(membuf);
			return (-1);
		}
		rc = write(mfd, membuf, bufsize);
		if (rc < 0) {
			cerror("Failed to write (%s)\n", STRERR);
			free(membuf);
			return (-1);
		}
		prio.pi_offset += bufsize;
	}
	if (bufsize = (memmap->cm_savelen % bufsize)) {

		prio.pi_len = (ssize_t)bufsize;

		if (ckpt_savemem_ioctl(co, ct, PIOCREAD, &prio) < 0) {
			cerror("read proc image:save all (%s)\n", STRERR);
			free(membuf);
			return (-1);
		}
		rc = write(mfd, membuf, bufsize);
		if (rc < 0) {
			cerror("Failed to write (%s)\n", STRERR);
			free(membuf);
			return (-1);
		}
	}
	free(membuf);
	return (0);
}

/* ARGSUSED */
static int
ckpt_handle_mapdev(	ckpt_obj_t *co,
			ch_t *ch,
			ckpt_prop_t *pp,
		    	ckpt_memmap_t *memmap,
			struct stat *statbuf)
{
	long rc;
	ckpt_f_t f;
	/*
	 * Check for special handling.
	 */
	rc = ckpt_handle_mapspecial(co, ch, memmap);
	if (rc != 0)
		return ((int)rc);

	f.cf_magic = CKPT_MAGIC_MAPFILE;
	f.cf_flags = CKPT_FLAGS_MAPFILE;
	f.cf_rdev = memmap->cm_rdev;
	f.cf_mode = CKPT_MODE_IGNORE;
	f.cf_fmode = statbuf->st_mode;
	f.cf_fd = -1;
	f.cf_auxptr = NULL;
	f.cf_auxsize = 0;

	switch (memmap->cm_maxprots & (PROT_READ|PROT_WRITE)) {
	case PROT_READ:
		f.cf_fflags = O_RDONLY;
		break;
	case PROT_WRITE:
		f.cf_fflags = O_WRONLY;
		break;
	case PROT_READ|PROT_WRITE:
		f.cf_fflags = O_RDWR;
		break;
	}
	f.cf_offset = 0;
	f.cf_length = 0;
	f.cf_flp = NULL;
	f.cf_fdflags = 0;

	ckpt_setpaths(co, &f, NULL, statbuf->st_rdev, statbuf->st_ino,
			statbuf->st_mode, co->co_pid, 0);
	ckpt_setmempath(memmap->cm_path, &f);

	memmap->cm_fd = ckpt_pfd_lookup(co, statbuf->st_dev, statbuf->st_ino,
					memmap->cm_maxprots);

	if (ckpt_add_unlinked(co, f.cf_path) < 0)
		return (-1);

	ckpt_warn_unsupported(co, &f, -1, memmap->cm_vaddr);

	CWRITE(co->co_sfd, &f, sizeof(ckpt_f_t), 1, pp, rc);
	if (rc < 0) {
		cerror("failed to write per file header \n", STRERR);
		return (-1);
	}
	return (0);
}

static int
ckpt_dump_mapfile(	ckpt_obj_t *co,
			ckpt_thread_t *ct,
			ckpt_prop_t *pp,
			ckpt_f_t *mapfile,
		    	ckpt_memmap_t *memmap,
		    	struct stat *statbuf)
{
	int memfd;
	char *path;
	pid_t handled;

	if ((memmap->cm_flags & (CKPT_EXECFILE|CKPT_MAPFILE)) == 0)
		return (0);
	if (memmap->cm_maxoff == memmap->cm_foff) {
		/* maps 0 length of the file. Clear the map flag*/
		memmap->cm_flags &= ~CKPT_EXECFILE;
		return 0;
	}
	/*
	 * Clear MAPFILE if it's a /dev/zero mapping,
	 * otherwise get the pathname
	 */
	if (memmap->cm_flags & CKPT_MAPZERO) {
		memmap->cm_flags &= ~CKPT_MAPFILE;
		return (0);
	}
	/*
	 * Check for device mapping
	 */
	if (S_ISCHR(statbuf->st_mode))
		return (ckpt_handle_mapdev(co, co->co_ch, pp, memmap, statbuf));

	/*
	 * For mapped files, devs and executibles, get the pathname
	 *
	 * Note that an mmap of /dev/zero can look like a mapped file.
	 * So, we check for that case.
	 */
	memfd = ckpt_savemem_ioctl(co, ct, PIOCOPENM, &memmap->cm_vaddr);
	if (memfd < 0) {
		cerror("PIOCOPENM (%s), vaddr %x\n", STRERR, memmap->cm_vaddr);
		return (-1);
	}
	IFDEB1(cdebug("memfd=%d cm_vaddr=%x st_dev=%d st_ino=%d\n", 
		memfd, memmap->cm_vaddr, statbuf->st_dev, statbuf->st_ino));
	switch (ckpt_pathname(co, memfd, &path)) {
	case 0:
		break;
	case 1:
		/*
		 * Do *not* set CKPT_FLAGS_UNLINKED!
		 */
		memmap->cm_xflags |= CKPT_UNLINKED;
		break;
	default:
		(void)close(memfd);
		return (-1);
	}
#ifdef DEBUG
	{
	struct stat sb;
	fstat(memfd, &sb);
	if ((sb.st_mode & S_IFMT) != S_IFREG) {
		cerror("pid %d vaddr %lx mapping wrong file type %s %x\n",
		co->co_pid, memmap->cm_vaddr,  path, sb.st_mode);
		return (-1);
		}
	}
#endif
	mapfile->cf_magic = CKPT_MAGIC_MAPFILE;
	mapfile->cf_flags = CKPT_FLAGS_MAPFILE;

	/*
	 * If this checkpoint is done due to coming system upgrade and the DSOs
	 * and executables will not be the same in the new release, we need to
	 * save everything here
	 */
	if ((cpr_flags & CKPT_CHECKPOINT_UPGRADE) &&
		(memmap->cm_flags & CKPT_EXECFILE))
		mapfile->cf_mode = CKPT_MODE_SUBSTITUTE;
	else
		mapfile->cf_mode = ckpt_attr_get_mode(co->co_ch,
						path,
						statbuf->st_dev,
			    			statbuf->st_ino,
						statbuf->st_mode,
						memmap->cm_xflags & CKPT_UNLINKED,
						0);

	handled = ckpt_file_is_handled(co, statbuf->st_dev, statbuf->st_ino, -1, 0);
	if (handled < 0)
		return (-1);

	ckpt_setpaths(	co,
			mapfile,
			path,
			statbuf->st_dev,
			statbuf->st_ino,
			statbuf->st_mode,
			handled? handled : co->co_pid,
			memmap->cm_xflags & CKPT_UNLINKED);
	ckpt_setmempath(memmap->cm_path, mapfile);

	free(path);

	IFDEB1(cdebug("mem cm_path=%s\n", memmap->cm_path));

	memmap->cm_fd = ckpt_pfd_lookup(co, statbuf->st_dev, statbuf->st_ino,
					memmap->cm_maxprots);

	if (handled) {
		(void)close(memfd);
		return (0);
	}
	if (memmap->cm_xflags & CKPT_UNLINKED) {
		if (ckpt_add_unlinked(co, mapfile->cf_path) < 0)
			return (-1);
	}
	mapfile->cf_fmode = statbuf->st_mode;
	mapfile->cf_length = statbuf->st_size;
	mapfile->cf_atime = statbuf->st_atim;
	mapfile->cf_mtime = statbuf->st_mtim;
	/*
	 * If we're handling the file, must not have an open fd
	 */
	mapfile->cf_fd = -1;
	mapfile->cf_fflags = 0;
	mapfile->cf_offset = -1;

	ckpt_dump_one_ofile(mapfile, memfd);

	(void)close(memfd);

	return (1);
}

static int
ckpt_display_a_mapfile(	ckpt_obj_t *co,
			ckpt_thread_t *ct,
			ckpt_f_t *mapfile,
		    	ckpt_memmap_t *memmap,
		    	struct stat *statbuf,
			int count_only)
{
	int memfd;
	char *path;
	char buf[CPATHLEN];

	if ((memmap->cm_flags & CKPT_MAPFILE) == 0)
		return (0);
	/*
	 * Clear MAPFILE if it's a /dev/zero mapping,
	 * otherwise get the pathname
	 */
	if (memmap->cm_flags & CKPT_MAPZERO) {
		memmap->cm_flags &= ~CKPT_MAPFILE;
		return (0);
	}
	/*
	 * Check for device mapping
	 */
	if (S_ISCHR(statbuf->st_mode))
		return (0);

	if (count_only)
		return (1);

	/*
	 * For mapped files, devs and executibles, get the pathname
	 *
	 * Note that an mmap of /dev/zero can look like a mapped file.
	 * So, we check for that case.
	 */
	memfd = ckpt_savemem_ioctl(co, ct, PIOCOPENM, &memmap->cm_vaddr);
	if (memfd < 0) {
		cerror("PIOCOPENM (%s)\n", STRERR);
		return (-1);
	}
	IFDEB1(cdebug("memfd=%d cm_vaddr=%x st_dev=%d st_ino=%d\n", 
		memfd, memmap->cm_vaddr, statbuf->st_dev, statbuf->st_ino));
	switch (ckpt_pathname(co, memfd, &path)) {
	case 0:
		break;
	case 1:
		/*
		 * Do *not* set CKPT_FLAGS_UNLINKED!
		 */
		memmap->cm_xflags |= CKPT_UNLINKED;
		break;
	default:
		close(memfd);
		return (-1);
	}

	mapfile->cf_mode = ckpt_attr_get_mode(co->co_ch,
					path,
					statbuf->st_dev,
			    		statbuf->st_ino,
					statbuf->st_mode,
					memmap->cm_xflags & CKPT_UNLINKED,
					0);

	sprintf(buf, " [PID %d: MD %2d] FILE: \"%s\": %s ",
        	co->co_pid, memfd, path, ckpt_action_str(mapfile->cf_mode));
        IFDEB1(cdebug("display mapfile: %s\n", buf));
	if (write(pipe_rc, buf, sizeof(buf)) < 0) {
		cerror("Failed to pass up openfile info (%s)\n", STRERR);
		return (-1);
	}
	free(path);
	close(memfd);
	return (1);
}

#define IS_BREAK(prmap, psinfo)                             \
        (((prmap)->pr_mflags & MA_BREAK)||                      \
            (((prmap)->pr_vaddr >= (psinfo)->ps_brkbase)&&   \
             ((prmap)->pr_vaddr < (psinfo)->ps_brkbase + (psinfo)->ps_brksize)))

/*
 * ckpt_savemem
 *
 * Save the memory image of the process opened with fd prfd into the statefile
 * opened with fd sfd
 */
int
ckpt_write_mapfiles(ckpt_obj_t *co, ckpt_prop_t *pp)
{
	ckpt_thread_t *thread;
	ckpt_psi_t	*p = &co->co_pinfo->cp_psi;
	ckpt_memobj_t	*mo = &co->co_memobj;
	int local_only = 0;

	bzero(mo, sizeof(*mo));
	mo->cmo_magic = CKPT_MAGIC_MEMOBJ;

	if (co->co_flags & CKPT_CO_ZOMBIE)
		return (0);

	/*
	 * If it's a shared address sproc, and memory is already handled,	
	 * do MAP_LOCAL mappings only
	 */
	if (IS_SPROC(co->co_pinfo) && IS_SADDR(co->co_pinfo))
		local_only = (co->co_flags & CKPT_CO_SADDR)? 0 : 1;

	if (local_only) {
		mo->cmo_brkbase = 0;
		mo->cmo_brksize = 0;
	} else {
		mo->cmo_brkbase = p->ps_brkbase;
		mo->cmo_brksize = p->ps_brksize;
	}
	mo->cmo_stackbase = p->ps_stackbase;
	mo->cmo_stacksize = p->ps_stacksize;

	for (thread = co->co_thread; thread; thread = thread->ct_next) {

		if (ckpt_write_thread_mappings(mo, co, pp, thread, local_only))
			return (-1);
	}
	return (0);
}

static int
ckpt_write_thread_mappings(ckpt_memobj_t *mo,
			   ckpt_obj_t *co,
			   ckpt_prop_t *pp,
			   ckpt_thread_t *ct,
			   int local_only)
{
	tid_t tid = ct->ct_id;
	caddr_t saddr;		/* region start address */
	caddr_t eaddr;		/* region end address */
	ckpt_mstat_arg_t ckpt_mstat;
	struct stat	statbuf;
	prmap_sgi_arg_t prmaparg;
	prmap_sgi_t     *prmap;
	prmap_sgi_arg_t ckptmap;
	ckpt_getmap_t	*ckpt_getmap;
	ckpt_memmap_t	*memmap;
	ckpt_seg_t	**mapseg, *segalloc, *segptr;
	ckpt_f_t	*mapfile, *maplist;	/* mapped file info */
	ckpt_psi_t	*p = &co->co_pinfo->cp_psi;
	int sfd = co->co_sfd;	/* state file descriptor */
	int nsegs;		/* number of attribute segments */
	int npregs;		/* number of pregions */
	int nmaps = 0;		/* actual number of pregions */
	int nskipped = 0;	/* number of areas to skip */
	int i, handled, isism;
	caddr_t prevaddr;
	ssize_t rc;
	int refcnt;
	int domap = 0;
	/*
	 * Only do shared mappings for the first thread encountered
	 */
	if (co->co_thread != ct)
		local_only = 1;

	prmaparg.pr_vaddr = NULL;
	prmaparg.pr_size = 0;

	if (ckpt_savemem_ioctl(co, ct, PIOCNMAP, &nsegs) < 0) {
		cerror("PIOCNMAP (%s)\n", STRERR);
		return (-1);
	}
	/*
	 * Allocate 1 extra for null termination and 1 extra for
	 * brkbase segment
	 */
	prmaparg.pr_vaddr = calloc(nsegs + 2, sizeof(prmap_sgi_t));
	prmaparg.pr_size = (nsegs+2) * sizeof(prmap_sgi_t);
	prmap = (prmap_sgi_t *)prmaparg.pr_vaddr;
	/*
	 * PIOCMAP_SGI retuns actual number of segments
	 */
	if ((nsegs = ckpt_savemem_ioctl(co, ct, PIOCMAP_SGI, &prmaparg)) < 0) {
		cerror("PIOCMAP_SGI (%s)\n", STRERR);
		free(prmap);
		return (-1);
	}
	/*
	 * Number of pregs is <= nsegs
	 */
	ckptmap.pr_vaddr = calloc(nsegs, sizeof(ckpt_getmap_t));
	ckptmap.pr_size = nsegs * sizeof(ckpt_getmap_t);
	ckpt_getmap = (ckpt_getmap_t *)ckptmap.pr_vaddr;

	if ((npregs = ckpt_savemem_ioctl(co, ct, PIOCCKPTGETMAP, &ckptmap)) < 0) {
		cerror("PIOCCKPTGETMAP (%s)\n", STRERR);
		goto errout;
	}
	mo->cmo_nmaps += npregs;
	ct->ct_nmemmap = npregs;

	if ((memmap = ct->ct_memmap = (ckpt_memmap_t *)
		calloc(npregs, sizeof(ckpt_memmap_t))) == NULL) {
		cerror("Failed to allocate memory (%s)\n", STRERR);
		goto errout;
	}
	if ((mapseg = ct->ct_mapseg = (ckpt_seg_t **)
		calloc(npregs, sizeof(ckpt_seg_t *))) == NULL) {
		cerror("Failed to allocate memory (%s)\n", STRERR);
		goto errout;
	}
	segalloc = segptr = (ckpt_seg_t *)calloc(nsegs, sizeof(ckpt_seg_t));
	if (segptr == NULL) {
		cerror("Failed to allocate memory (%s)\n", STRERR);
		goto errout;
	}
	/*
	 * Number of mapped files <= npregs
	 */
	maplist = mapfile = (ckpt_f_t *)calloc(npregs, sizeof(ckpt_f_t));
	if (mapfile == NULL) {
		cerror("Failed to allocate memory (%s)\n", STRERR);
		goto errout;
	}
	/*
	 * Now start working through attached virtual space...
	 */
	for (i = 0; i < npregs; ckpt_getmap++, i++) {
		saddr = ckpt_getmap->m_vaddr;
		eaddr = saddr + ckpt_getmap->m_size;

		memmap[i].cm_magic = CKPT_MAGIC_MEMMAP;

		memmap[i].cm_vaddr = saddr;
		memmap[i].cm_len = ckpt_getmap->m_size;
		memmap[i].cm_mapid = ckpt_getmap->m_mapid;
		memmap[i].cm_maxprots = ckpt_getmap->m_prots;
		memmap[i].cm_flags = ckpt_getmap->m_flags;
		memmap[i].cm_maxoff = ckpt_getmap->m_maxoff;
		memmap[i].cm_pid = 0;
		memmap[i].cm_rdev = 0;
		memmap[i].cm_shmid = ckpt_getmap->m_shmid;
		memmap[i].cm_auxptr = NULL;
		memmap[i].cm_auxsize = 0;
		memmap[i].cm_mflags = 0;
		memmap[i].cm_xflags = 0;
		memmap[i].cm_cflags = 0;
		memmap[i].cm_nmodmin = 0;
		memmap[i].cm_nmod = 0;
		memmap[i].cm_modlist = NULL;
		memmap[i].cm_pagesize = getpagesize();
		memmap[i].cm_fd = -1;
		memmap[i].cm_tid = tid;

		if (ckpt_getmap->m_flags & CKPT_AUTOGROW)
			memmap[i].cm_mflags |= MAP_AUTOGROW;
		if (ckpt_getmap->m_flags & CKPT_LOCAL)
			memmap[i].cm_mflags |= MAP_LOCAL;
		if (ckpt_getmap->m_flags & CKPT_AUTORESRV)
			memmap[i].cm_mflags |= MAP_AUTORESRV;

		if (ckpt_getmap->m_flags & CKPT_MAPFILE) {
			if (ckpt_getmap->m_flags & CKPT_MAPPRIVATE)
				memmap[i].cm_mflags |= MAP_PRIVATE;
			else
				memmap[i].cm_mflags |= MAP_SHARED;
		}
		memmap[i].cm_foff = prmap->pr_off;
		memmap[i].cm_nsegs = 0;
		/*
		 * stat underlying vnode
		 */
		ckpt_mstat.statvers = _STAT_VER;
		ckpt_mstat.vaddr = memmap[i].cm_vaddr;
		ckpt_mstat.sbp = &statbuf;

		if (ckpt_savemem_ioctl(co, ct, PIOCCKPTMSTAT, &ckpt_mstat) == 0) {
			memmap[i].cm_xflags |= CKPT_VNODE;
			memmap[i].cm_rdev = statbuf.st_rdev;

			if (ckpt_mapattr_special(co, co->co_ch, &memmap[i], 
			    &statbuf) < 0) {
				cerror("Address %lx:", memmap->cm_vaddr);
				cerror("could not get special attributes: %s\n",
									 STRERR);
				goto errout;
			}
		} else if (oserror() != ENOENT) {
			cerror("PIOCCKPTMSTAT (%s)\n", STRERR);
			goto errout;
		}
		/*
		 * Things we don't save:
		 * -memory used for checkpointing
		 * -sproc shared mappings if another share member has already
		 *  done it
		 * Unless:
		 * cm_pid indicates this proc must save mapping
		 */
		domap = 1;

		if (IS_CKPTRESERVED(memmap[i].cm_vaddr)||
		   (local_only && !(ckpt_getmap->m_flags & CKPT_LOCAL)))
			domap = 0;

		if (memmap[i].cm_pid)
			domap = ((memmap[i].cm_pid == co->co_pid)&&
				 (co->co_thread == ct));

		IFDEB1(cdebug("ckpt_savemem: i=%d cm_xflags=%x cm_cflags %x\n",
			i, memmap[i].cm_xflags, memmap[i].cm_cflags));

		mapseg[i] = segptr;

		refcnt = (int)CKPT_REFCNT(prmap);

		prevaddr = 0;

		while ((prmap->pr_vaddr < eaddr)&&
		       ((prmap->pr_vaddr > prevaddr)||(prmap->pr_vaddr == 0))&&
		       (nsegs > 0)) {

#ifdef DEBUG
			if (prmap->pr_vaddr < saddr) {
				cnotice("Pid %d:saddr %lx:vaddr %lx\n", 
					co->co_pid, saddr, prmap->pr_vaddr);
			}
#endif
			assert(prmap->pr_vaddr >= saddr);

			/*
			 * track previous address in case encounter local -> shared
			 * mapping transition
			 */
			prevaddr = prmap->pr_vaddr;
			/*
			 * The mod size is not reliable.  It misses stuff
			 * that is on swap with no pte entry, so
			 * may under-report.  It could still be useful,
			 * so track it anyway
			 */
			memmap[i].cm_nmodmin += prmap->pr_msize;

			if ((prmap->pr_mflags & (MA_PHYS|MA_SHARED)) == (MA_PHYS|MA_SHARED)) {
				if ((memmap[i].cm_mflags & MAP_PRIVATE) == 0)
					memmap[i].cm_flags |= MAP_SHARED;

			} else if (prmap->pr_mflags & MA_SHARED) {
				assert(!(memmap[i].cm_mflags & MAP_PRIVATE));
				memmap[i].cm_mflags |= MAP_SHARED;
			}
			if (prmap->pr_mflags & MA_COW) {
				assert(!(memmap[i].cm_mflags & MAP_SHARED));
				memmap[i].cm_mflags |= MAP_PRIVATE;
			}
			if (prmap->pr_mflags & MA_PHYS)
				memmap[i].cm_xflags |= CKPT_PHYS;

			if (prmap->pr_mflags & MA_PRIMARY) {
				memmap[i].cm_mflags |= MAP_PRIMARY;
			}
			if (IS_BREAK(prmap, p))
				memmap[i].cm_xflags |= CKPT_BREAK;
			if (prmap->pr_mflags & MA_SHMEM)
				memmap[i].cm_xflags |= CKPT_SHMEM;

			segptr->cs_vaddr = prmap->pr_vaddr;
			segptr->cs_len = prmap->pr_size;
			segptr->cs_prots = 0;

			if (prmap->pr_mflags & MA_READ)
				segptr->cs_prots |= PROT_READ;
			if (prmap->pr_mflags & MA_WRITE)
				segptr->cs_prots |= PROT_WRITE;
			if (prmap->pr_mflags & MA_EXEC)
				segptr->cs_prots |= PROT_EXEC;

			if (prmap->pr_mflags & MA_NOTCACHED)
				segptr->cs_notcached = CKPT_NONCACHED;
			else
				segptr->cs_notcached = 0;
			/*
			 * fetchop overrides usual non-cached setting
			 */
			if (prmap->pr_mflags & MA_FETCHOP) {
				memmap[i].cm_xflags |= CKPT_FETCHOP;
				segptr->cs_notcached = CKPT_NONCACHED_FETCHOP;
			}
			segptr->cs_lockcnt = prmap->pr_lockcnt;
			/*
			 * Look for policy modules
			 */
			if (domap) {
				int is_shared = 
				(memmap[i].cm_flags&CKPT_MAPZERO) &&
				(memmap[i].cm_mflags&MAP_SHARED) && (refcnt>1);
				if (ckpt_get_pmodule(co, segptr, is_shared))
					return (-1);
			}
			memmap[i].cm_nsegs++;

			prmap++;
			segptr++;
			nsegs--;
		}
#ifdef NOTYET
		if (memmap[i].cm_xflags & CKPT_PHYS)
			memmap[i].cm_foff = (off_t)ckpt_getmap->m_physaddr;
#endif
		if (!domap) {
			nskipped++;
			memmap[i].cm_xflags = CKPT_SKIP;
			continue;
		}
#ifdef DEBUG
		if ((memmap[i].cm_flags & CKPT_MAPFILE) &&
		    ((memmap[i].cm_mflags & (MAP_SHARED|MAP_PRIVATE)) == 0))
			/*
			 * mmap sets COW for private mappings, so if
			 * we get here without PRIVATE, COW was not
			 * set, so must be SHARED
			 */
			assert(0);
#endif
		if (ckpt_lookup_prattach(co, &memmap[i])) {
			memmap[i].cm_xflags |= CKPT_PRATTACH;
			memmap[i].cm_cflags |= CKPT_DEPENDENT;
			/* don't do CKPT_FETCHOP setup, it depends
			 * on someone else do it.
			 */
			memmap[i].cm_xflags &= ~(CKPT_FETCHOP);

		} else {
			isism = ckpt_is_ismmem(co, &memmap[i], refcnt);
			if (isism < 0)
				goto errout;
			if (isism) {
				memmap[i].cm_xflags |= CKPT_ISMEM;
				memmap[i].cm_cflags |= CKPT_DEPENDENT;
				/* don't do CKPT_FETCHOP setup, it depends
				 * on someone else do it.
				 */
				memmap[i].cm_xflags &= ~(CKPT_FETCHOP);
			} else {
				if ((handled = ckpt_dump_mapfile(co,
								ct,
								pp,
								mapfile,
								&memmap[i],
								&statbuf)) < 0)
				goto errout;

				if (handled) {
					nmaps++;
					mapfile++;
				}
			}
		}
		if (ckpt_remap_disp(co, &statbuf, mapfile->cf_path,
			&memmap[i], co->co_ch) < 0)
			goto errout;
		/*
		 * Check for Posix unnamed semaphores
		 */
		if (ckpt_getmap->m_flags & CKPT_PUSEMA) {
			if (ckpt_save_pusema(co, &memmap[i]) < 0)
				goto errout;
		}
	}
	assert(nsegs == 0);

	CWRITE(sfd, maplist, nmaps * sizeof(ckpt_f_t), nmaps, pp, rc);
	if (rc < 0) {
		cerror("Failed to write out the mapped file header (%s)\n", STRERR);
		goto errout;
	}
	mo->cmo_nmaps -= nskipped;

	free(ckptmap.pr_vaddr);
	free(prmaparg.pr_vaddr);
	return (0);

errout:
	free(ckptmap.pr_vaddr);
	free(prmaparg.pr_vaddr);
	free(ct->ct_memmap);
	free(ct->ct_mapseg);
	free(segalloc);

	ct->ct_memmap = NULL;
	ct->ct_mapseg = NULL;

	return (-1);
}

static int ckpt_display_a_mapfile(ckpt_obj_t *, ckpt_thread_t *ct, ckpt_f_t *,
    	ckpt_memmap_t *, struct stat *, int);
int
ckpt_display_mapfiles(ckpt_obj_t *co, int count_only)
{
	ckpt_thread_t *ct;
	ckpt_mstat_arg_t ckpt_mstat;
	struct stat	statbuf;
	prmap_sgi_arg_t prmaparg;
	prmap_sgi_t     *prmap;
	prmap_sgi_arg_t ckptmap;
	ckpt_getmap_t	*ckpt_getmap;
	ckpt_memmap_t	memmap;
	ckpt_f_t	mapfile;
	int prfd = co->co_prfd;	/* /proc file descriptor */
	int nsegs;		/* number of attribute segments */
	int npregs;		/* number of pregions */
	int nmaps = 0;		/* actual number of pregions */
	int i, handled;

	if (co->co_flags & CKPT_CO_ZOMBIE)
		return (0);

	if (ioctl(prfd, PIOCNMAP, &nsegs) < 0) {
		cerror("PIOCNMAP (%s)\n", STRERR);
		return (0);
	}
	/*
	 * Allocate 1 extra for null termination and 1 extra for
	 * brkbase segment
	 */
	prmaparg.pr_vaddr = calloc(nsegs + 2, sizeof(prmap_sgi_t));
	prmaparg.pr_size = (nsegs+2) * sizeof(prmap_sgi_t);
	prmap = (prmap_sgi_t *)prmaparg.pr_vaddr;
	/*
	 * PIOCMAP_SGI retuns actual number of segments
	 */
	if ((nsegs = ioctl(prfd, PIOCMAP_SGI, &prmaparg)) < 0) {
		cerror("PIOCMAP_SGI (%s)\n", STRERR);
		free(prmap);
		return (0);
	}
	/*
	 * Number of pregs is <= nsegs
	 */
	ckptmap.pr_vaddr = calloc(nsegs, sizeof(ckpt_getmap_t));
	ckptmap.pr_size = nsegs * sizeof(ckpt_getmap_t);
	ckpt_getmap = (ckpt_getmap_t *)ckptmap.pr_vaddr;

	if ((npregs = ioctl(prfd, PIOCCKPTGETMAP, &ckptmap)) < 0) {
		cerror("PIOCCKPTGETMAP (%s)\n", STRERR);
		goto errout;
	}

	for (i = 0; i < npregs; ckpt_getmap++, i++) {
		memmap.cm_vaddr = ckpt_getmap->m_vaddr;
		memmap.cm_flags = ckpt_getmap->m_flags;
		memmap.cm_xflags = 0;

		/*
		 * stat underlying vnode
		 */
		ckpt_mstat.statvers = _STAT_VER;
		ckpt_mstat.vaddr = memmap.cm_vaddr;
		ckpt_mstat.sbp = &statbuf;

		if (ioctl(co->co_prfd, PIOCCKPTMSTAT, &ckpt_mstat) == 0) {
			if (ckpt_is_special(statbuf.st_rdev))
				continue;
		} else if (oserror() != ENOENT) {
			cerror("PIOCCKPTMSTAT (%s)\n", STRERR);
			goto errout;
		}

		if ((handled = ckpt_display_a_mapfile(co,
						ct,
						&mapfile,
						&memmap,
						&statbuf,
						count_only)) < 0)
			goto errout;

		if (handled)
			nmaps++;
	}
	free(ckptmap.pr_vaddr);
	free(prmaparg.pr_vaddr);
	return (nmaps);

errout:
	free(ckptmap.pr_vaddr);
	free(prmaparg.pr_vaddr);
	return (0);
}

int
ckpt_write_memobj(ckpt_obj_t *co, ckpt_prop_t *pp)
{
	ckpt_memobj_t *mo = &co->co_memobj;
	ckpt_thread_t *thread;
	int sfd = co->co_sfd;
	ssize_t rc;

	CWRITE(sfd, mo, sizeof(ckpt_memobj_t), 1, pp, rc);
	if (rc < 0) {
		cerror("Failed to write memob header (%s)\n", STRERR);
		return (-1);
	}
	if (co->co_flags & CKPT_CO_ZOMBIE)
		return (0);

	for (thread = co->co_thread; thread; thread = thread->ct_next) {

		if (ckpt_write_thread_memobj(co, thread, pp) < 0)
			return (-1);
	}
	return (0);
}

static int
ckpt_write_thread_memobj(ckpt_obj_t *co,
			 ckpt_thread_t *ct,
			 ckpt_prop_t *pp)
{
	ckpt_memmap_t *memmap = ct->ct_memmap;
	ckpt_seg_t **mapseg = ct->ct_mapseg;
	int npregs = ct->ct_nmemmap;
	int sfd = co->co_sfd;
	int i;
	ssize_t rc = 0;

	for (i = 0; i < npregs; i++) {
		/*
		 * Write pregion and segment headers to statefile
		 */
		if (memmap[i].cm_xflags & CKPT_SKIP)
			continue;

		if (memmap[i].cm_cflags & CKPT_DEPENDENT)
			continue;

		IFDEB1(cdebug("Saving addr1 %llx size %x cflag %x mflags %x"
			"flags %x xflags %x\n", memmap[i].cm_vaddr, 
			memmap[i].cm_len, memmap[i].cm_cflags, 
			memmap[i].cm_mflags, memmap[i].cm_flags, 
			memmap[i].cm_xflags));

		/*
		 * Deal with file/memory contents
		 */
		if (memmap[i].cm_cflags & CKPT_SAVEALL)
			rc = ckpt_save_all(co, ct, &memmap[i]);
		else if (memmap[i].cm_cflags & CKPT_SAVEMOD)
			rc = ckpt_save_modified(co, ct, &memmap[i], mapseg[i]);
		else if (memmap[i].cm_cflags & CKPT_SAVESPEC)
			rc = ckpt_save_special(co, &memmap[i], mapseg[i]);
		if (rc < 0) {
			cerror("Failed to save mem/file contents (flag=%x) (%s)\n",
				memmap[i].cm_cflags, STRERR);
			goto errout;
		}
		CWRITE(sfd, &memmap[i], sizeof(ckpt_memmap_t), 0, pp, rc);
		if (rc < 0) {
			cerror("Failed to write memmap (%s)\n", STRERR);
			goto errout;
		}
		if (memmap[i].cm_auxptr) {
			CWRITE(sfd, memmap[i].cm_auxptr, memmap[i].cm_auxsize,
				0, pp, rc) ;
			if (rc < 0) {
				cerror("Failed to write cm_auxptr (%s)\n", STRERR);
				goto errout;
			}
		}
		CWRITE(sfd, mapseg[i], memmap[i].cm_nsegs*sizeof(ckpt_seg_t),
			0, pp, rc);
		if (rc < 0) {
			cerror("Failed to write mapsegs (%s)\n", STRERR);
			goto errout;
		}
		if (memmap[i].cm_modlist) {
			CWRITE(sfd, memmap[i].cm_modlist,
				memmap[i].cm_nmod * sizeof(caddr_t), 0, pp, rc);
			free(memmap[i].cm_modlist);
			if (rc < 0) {
				cerror("Failed to write (%s)\n", STRERR);
				return (-1);
			}
		}
	}
	for (i = 0; i < npregs; i++) {
		/*
		 * Write pregion info for iherited shared mem regions.
		 * Since these are shared with owner, don't worry about
		 * contents...owner gets that.
		 */
		if (memmap[i].cm_xflags & CKPT_SKIP)
			continue;

		if ((memmap[i].cm_cflags & CKPT_DEPENDENT) == 0)
			continue;

		if ((memmap[i].cm_xflags & (CKPT_ISMEM|CKPT_SHMEM)) == 0)
			continue;
		/*
		 * On the off chance we iherited mapping with aux attrs
	 	 */
		if (memmap[i].cm_auxptr) {
			free(memmap[i].cm_auxptr);
			memmap[i].cm_auxsize = 0;
		}

		IFDEB1(cdebug("Saving addr2 %x size %x flag %x\n", 
		memmap[i].cm_vaddr, memmap[i].cm_len, memmap[i].cm_cflags));

		CWRITE(sfd, &memmap[i], sizeof(ckpt_memmap_t), 0, pp, rc);
		if (rc < 0) {
			cerror("write (%s)\n", STRERR);
			goto errout;
		}
		CWRITE(sfd, mapseg[i], memmap[i].cm_nsegs * sizeof(ckpt_seg_t),
			0, pp, rc);
		if (rc < 0) {
			cerror("write (%s)\n", STRERR);
			goto errout;
		}
	}
	/*
	 * Write pregion info for prattach'd regions
	 */
	for (i = 0; i < npregs; i++) {
		if (memmap[i].cm_xflags & CKPT_SKIP)
			continue;

		if ((memmap[i].cm_xflags & CKPT_PRATTACH) == 0)
			continue;

		IFDEB1(cdebug("Saving addr3 %x size %x flag %x\n", 
		memmap[i].cm_vaddr, memmap[i].cm_len, memmap[i].cm_cflags));

		if (memmap[i].cm_cflags & CKPT_SAVEALL)
			rc = ckpt_save_all(co, ct, &memmap[i]);
		else if (memmap[i].cm_cflags & CKPT_SAVEMOD)
			rc = ckpt_save_modified(co, ct, &memmap[i], mapseg[i]);
		if (rc < 0)
			goto errout;

		CWRITE(sfd, &memmap[i], sizeof(ckpt_memmap_t), 0, pp, rc);
		if (rc < 0) {
			cerror("write (%s)\n", STRERR);
			goto errout;
		}
		/*
		 * On the off chance we attached a special mapping
	 	 */
		if (memmap[i].cm_auxptr) {
			free(memmap[i].cm_auxptr);
			memmap[i].cm_auxsize = 0;
		}
		CWRITE(sfd, mapseg[i], memmap[i].cm_nsegs*sizeof(ckpt_seg_t),
			0, pp, rc);
		if (rc < 0) {
			cerror("write (%s)\n", STRERR);
			goto errout;
		}
		assert(memmap[i].cm_modlist == NULL);
	}
	free(ct->ct_memmap);
	free(ct->ct_mapseg[0]);
	free(ct->ct_mapseg);

	ct->ct_memmap = NULL;
	ct->ct_mapseg = NULL;

	return (0);

errout:
	free(ct->ct_memmap);
	free(ct->ct_mapseg[0]);
	free(ct->ct_mapseg);

	ct->ct_memmap = NULL;
	ct->ct_mapseg = NULL;

	return (-1);
}

