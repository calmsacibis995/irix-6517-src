
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <ckpt_internal.h>
#include <ckpt_revision.h>
#include "librestart.h"

int
ckpt_rxlate_segobjs(ckpt_rev_t revision, int sfd, ckpt_seg_t *segobj, int nsegs)
{
	if (revision < CKPT_REVISION_CURRENT) {
		/*
		 * All prior revisions have same format
		 */
		ckpt_seg_old_t	oldseg;

		while (--nsegs >= 0) {
			if (read(sfd, &oldseg, sizeof(oldseg)) < 0) {
				ckpt_perror("read segobj", ERRNO);
				return (-1);
			}
			segobj->cs_vaddr = oldseg.cs_vaddr;
			segobj->cs_len = oldseg.cs_len;
			segobj->cs_prots = oldseg.cs_prots;
			segobj->cs_notcached = oldseg.cs_notcached;
			segobj->cs_pmhandle = -1;
			segobj++;
		}
	} else {
		if (read(sfd, segobj, nsegs*sizeof(ckpt_seg_t)) < 0) {
			ckpt_perror("read segobj", ERRNO);
			return (-1);
		}
	}
	return (0);
}

int
ckpt_rxlate_memmap(ckpt_rev_t revision, int sfd, ckpt_memmap_t *memmap)
{
	ckpt_memmap_old_t old;

	if (revision < CKPT_REVISION_CURRENT) {
		if (read(sfd, &old, sizeof(ckpt_memmap_old_t)) < 0) {
			ckpt_perror("read memmap", ERRNO);
			return (-1);
		}
		memmap->cm_magic = old.cm_magic;
		memmap->cm_mapid = old.cm_mapid;
		memmap->cm_vaddr = old.cm_vaddr;
		memmap->cm_len = old.cm_len;
		memmap->cm_maxprots = old.cm_maxprots;
		memmap->cm_mflags = old.cm_mflags;
		memmap->cm_flags = old.cm_flags;

		if (old.cm_xflags & OLD_CKPT_PRIMARY) {
			memmap->cm_mflags |= MAP_PRIMARY;
			old.cm_xflags &= ~OLD_CKPT_PRIMARY;
		}
		memmap->cm_xflags = old.cm_xflags;
		memmap->cm_cflags = old.cm_cflags;
		memmap->cm_nsegs = old.cm_nsegs;
		memmap->cm_nmodmin = old.cm_nmodmin;

		if (memmap->cm_cflags & CKPT_SAVEALL)
			memmap->cm_savelen = old.cm_len;
		else
			memmap->cm_nmod = old.cm_nmodold;

		memmap->cm_modlist = old.cm_modlist;
		memmap->cm_pagesize = old.cm_pagesize;
		memmap->cm_pid = old.cm_pid;
		memmap->cm_tid = 0;
		memmap->cm_rdev = old.cm_rdev;
		memmap->cm_shmid = old.cm_shmid;
		memmap->cm_foff = old.cm_foff;
		memmap->cm_maxoff = old.cm_maxoff;
		memmap->cm_auxptr = old.cm_auxptr;
		memmap->cm_auxsize = old.cm_auxsize;
		memmap->cm_fd = old.cm_fd;
		/*
		 * Force NULL termination just in case have garbage
		 */
		old.cm_path[MAXPATHLEN-1] = 0;
		ckpt_strcpy(memmap->cm_path, old.cm_path);
	} else {
		if (read(sfd, memmap, sizeof(ckpt_memmap_t)) < 0) {
			ckpt_perror("read memmap", ERRNO);
			return (-1);
		}
	}
	return (0);
}

int
ckpt_rxlate_pusema(ckpt_rev_t revision, int sfd, ckpt_pusema_t *pusema)
{
	if (revision < CKPT_REVISION_CURRENT) {
		ckpt_pusema_old_t old;

		if (read(sfd, &old, sizeof(old)) < 0) {
			ckpt_perror("read pusema", ERRNO);
			return (-1);
		}
		pusema->pu_magic = old.pu_magic;
		pusema->pu_owner = old.pu_owner;
		pusema->pu_uvaddr = old.pu_uvaddr;
		pusema->pu_usync.off = old.pu_usync.off;
		pusema->pu_usync.voff = old.pu_usync.voff;
		pusema->pu_usync.notify = old.pu_usync.notify;
		pusema->pu_usync.value = old.pu_usync.value;
		pusema->pu_usync.handoff = 0;
		pusema->pu_usync.policy = USYNC_POLICY_PRIORITY;
	} else {
		if (read(sfd, pusema, sizeof(ckpt_pusema_t)) < 0) {
			ckpt_perror("read pusema", ERRNO);
			return (-1);
		}
	}
	return (0);
}
