/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995 Silicon Graphics, Inc.	  	*
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.26 $"

#include <stdio.h>
#include <limits.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/dir.h>
#include "ckpt.h"
#include "ckpt_internal.h"
#include "ckpt_revision.h"

static int ckpt_stat_one_statefile(pid_t);
static int ckpt_stat_proc_index(const char *);

static ckpt_stat_t *sp_last, *sp_first;
static ckpt_hinv_t hinv;

/*
 * ckpt_attr support
 */
int
ckpt_getattr(const char *path, int attr, ckpt_attr_t *aptr)
{
	char pathname[MAXPATHLEN];
	ckpt_pci_t pci;
	int ifd;

	if (attr & ~(CKPT_ATTR_ID|CKPT_ATTR_TYPE|CKPT_ATTR_REV)) {
		cerror("Invalid attr parameter %x", attr);
		setoserror(EINVAL);
		return -1;
	}
	sprintf(pathname, "%s/%s.%s", path, STATEF_SHARE, STATEF_INDEX);

	ifd = open(pathname, O_RDONLY);
	if (ifd < 0) {
		cerror("open %s (%s)", path, STRERR);
		return -1;
	}
	if (read(ifd, &pci, sizeof(pci)) < 0) {
		cerror("read %s (%s)", path, STRERR);
		close(ifd);
		return (-1);
	}
	close(ifd);

	if (pci.pci_magic != CKPT_MAGIC_INDEX) {
		cerror("invalid state file %s", path);
		setoserror(EINVAL);
		return -1;
	}
	if (attr & CKPT_ATTR_ID)
		aptr->ckpt_id = pci.pci_id;
	if (attr & CKPT_ATTR_TYPE)
		aptr->ckpt_type = pci.pci_type;
	if (attr & CKPT_ATTR_REV)
		aptr->ckpt_revision = pci.pci_revision;

	return (0);
}
int
ckpt_stat(const char *path, struct ckpt_stat **sp)
{
	int rc;

	if ((rc = ckpt_check_directory(path)))
		return (rc);

	sp_last = sp_first = NULL;

	/* scan the directory */
	if ((rc = ckpt_stat_proc_index(path)) == 0) {
		*sp = sp_first;
		return (0);
	}
	if (rc != EACCES) {
		cerror("corrupted statefile directory %s\n", path);
	}

	/* error case, free memory */
	while (sp_first) {
		ckpt_stat_t *next;

		next = sp_first->cs_next;
		free(sp_first);
		sp_first = next;
	}
	*sp = NULL;
	return (rc);
}

static int
ckpt_stat_proc_index(const char *dname)
{
	int i, rc;

	if (rc = ckpt_load_shareindex(dname))
		goto cleanup;

	if (ckpt_read_share_property(&cr, NULL, CKPT_MAGIC_HINV, (int *)&hinv))
		goto cleanup;

	if (ckpt_read_share_property(&cr, NULL, CKPT_MAGIC_CHILDINFO, NULL))
		goto cleanup;

	/*
	 * stat'ing each of the tree roots
	 */
	IFDEB1(cdebug("no. of treeroots: %d\n", cr.cr_pci.pci_ntrees));
	for (i = 0; i < cr.cr_pci.pci_ntrees; i++) {
		if (rc = ckpt_stat_one_statefile(*(cr.cr_roots+i)))
			goto cleanup;
	}
cleanup:
	free(cr.cr_roots);
	close(cr.cr_ifd);
	close(cr.cr_sfd);
	return (rc);
}

static int
ckpt_stat_one_statefile(pid_t pid)
{
	ckpt_stat_t *sp_new;
	ckpt_ta_t ta;
	int i, rc;

	if ((sp_new = calloc(1, sizeof(ckpt_stat_t))) == NULL) {
		cerror("Failed to allocate memory (%s)\n", STRERR);
		return (-1);
	}
	sp_new->cs_next = NULL;
	if (sp_first == NULL && sp_last == NULL)
		sp_first = sp_last = sp_new;
	else {
		sp_last->cs_next = sp_new;
		sp_last = sp_new;
	}
	if (ckpt_load_target_info(&ta, pid))
		goto cleanup;

	/*
	 * Fill in the time and other stat(2) info
	 */
	if (fstat(ta.sfd, &sp_last->cs_stat) < 0) {
		cerror("Failed to read statefile %s: %s\n", cr.cr_path, STRERR);
		goto cleanup;
	}
	if (rc = ckpt_restart_perm_check2(&(ta.pi), ta.sfd)) {
		cerror("cannot read %s: permission denied\n", cr.cr_path);
		goto cleanup;
	}
	/*
	 * Stat'ing files
	 */
	sp_last->cs_cpuboard = hinv.ch_board;
	sp_last->cs_cpu = hinv.ch_cpu;
	sp_last->cs_abi = -1;

	if (rc = ckpt_stat_proc_property(&ta, CKPT_MAGIC_PROCINFO, sp_last))
		goto cleanup;
	if (rc = ckpt_stat_proc_property(&ta, CKPT_MAGIC_OPENSPECS, sp_last))
		goto cleanup;
	if (rc = ckpt_stat_proc_property(&ta, CKPT_MAGIC_OPENFILES, sp_last))
		goto cleanup;
	if (rc = ckpt_stat_proc_property(&ta, CKPT_MAGIC_OTHER_OFDS, sp_last))
		goto cleanup;
	if (rc = ckpt_stat_proc_property(&ta, CKPT_MAGIC_CTXINFO, sp_last))
		goto cleanup;
#ifdef DEBUG_MEMOBJ
	if (rc = ckpt_stat_proc_property(&ta, CKPT_MAGIC_MEMOBJ, sp_last))
		goto cleanup;
#endif
	/*
	 * read any children we have
	 */
	IFDEB1(cdebug("no. of children for pid %d: %d\n", ta.nchild, pid));
	for (i = 0; i < ta.nchild; i++) {
		IFDEB1(cdebug("read statefile for pid %d\n", ta.cpid[i]));
		if (rc = ckpt_stat_one_statefile(ta.cpid[i]))
			goto cleanup;
	}
cleanup:
	ckpt_close_ta_fds(&ta);
	return (rc);
}

int
ckpt_stat_files(ckpt_ta_t *tp, ckpt_magic_t magic, ckpt_stat_t *sp)
{
	ckpt_f_t file;

	if (read(tp->sfd, &file, sizeof (ckpt_f_t)) == -1) {
		cerror("failed to read the per-file header\n");
		return (-1);
	}
	if (file.cf_magic != magic) {
		cerror("incorrect per openfile magic %s (vs %s)\n", 
			&file.cf_magic, &magic);
		return (-1);
	}
	if (file.cf_auxsize)
		lseek(tp->sfd, file.cf_auxsize, SEEK_CUR);

	IFDEB1(cdebug("read openfile=%s\n", file.cf_path));

	sp->cs_nfiles++;
	return (0);
}

int
ckpt_stat_procinfo(ckpt_ta_t *tp, ckpt_magic_t magic, ckpt_stat_t *sp)
{
	ckpt_pi_t pi;

	/*
	 * read CPR main header to prepare for changing identity
	 */
	if (ckpt_rxlate_procinfo(cr.cr_pci.pci_revision, tp, magic, &pi) < 0)
	        return (-1);

	sp->cs_revision = cr.cr_pci.pci_revision;
	sp->cs_id = cr.cr_pci.pci_id;
	sp->cs_type = cr.cr_pci.pci_type;
	sp->cs_pid = pi.cp_pid;
	sp->cs_ppid = pi.cp_ppid;
	sp->cs_pgrp = pi.cp_pgrp;
	sp->cs_sid = pi.cp_sid;
	if (pi.cp_comm[0])
		strncpy(sp->cs_comm, pi.cp_comm, PSCOMSIZ);
	else
		strcpy(sp->cs_comm, "<defunct>");
	if (pi.cp_psargs[0])
		strncpy(sp->cs_psargs, pi.cp_psargs, PSARGSZ);
	else
		strcpy(sp->cs_psargs, "<defunct>");
	strncpy(sp->cs_cdir, pi.cp_cdir, PATH_MAX);
#ifdef DEBUG_MEMOBJ
	printf("pid %d stack limit %lx %lx\n",
		pi.cp_pid,
		pi.cp_psi.ps_rlimit[RLIMIT_STACK].rlim_cur,
		pi.cp_psi.ps_rlimit[RLIMIT_STACK].rlim_max);
#endif
	return (0);
}
#ifdef DEBUG_MEMOBJ

#include <sys/ipc.h>
#include <sys/shm.h>

/* ARGSUSED */
int
ckpt_stat_memobj(ckpt_ta_t *tp, ckpt_magic_t magic, ckpt_stat_t *sp)
{
	ckpt_memobj_t memobj;
	int nmaps;
	ckpt_memmap_t memmap;
	off_t segobj_size;

	segobj_size = ckpt_segobj_size(cr.cr_pci.pci_revision);

        if (read(tp->sfd, &memobj, sizeof(ckpt_memobj_t)) < 0) {
                cerror("read memobj");
		return (-1);
        }
        if (memobj.cmo_magic != magic) {
                cerror("cmo_magic");
		return (-1);
        }
        nmaps = memobj.cmo_nmaps;

	printf("memobj nmaps %d stackbase %lx stacksize %d\n",
		nmaps, memobj.cmo_stackbase, memobj.cmo_stacksize);

        while (--nmaps >= 0) {
                if (read(tp->sfd, &memmap, sizeof(ckpt_memmap_t)) < 0) {
                        cerror("read memmap");
			return (-1);
                }
                if (memmap.cm_magic != CKPT_MAGIC_MEMMAP) {
                        cerror("cm_magic");
			return (-1);
                }
		printf("memmap vaddr %lx len %x mflags %x flags %x xflags %x cflags %x\n",
			memmap.cm_vaddr,
			memmap.cm_len,
			memmap.cm_mflags,
			memmap.cm_flags,
			memmap.cm_xflags,
			memmap.cm_cflags);
		if (memmap.cm_xflags & CKPT_SHMEM) {
			printf("\t SYS V SHM:");
			if (memmap.cm_cflags & CKPT_DEPENDENT) {
				printf("shmat mapid %lx\n", memmap.cm_mapid);
				lseek(tp->sfd, (off_t)memmap.cm_auxsize, SEEK_CUR);
			} else if (memmap.cm_auxptr) {
				struct shmid_ds shmds;

				read(tp->sfd, &shmds, sizeof(shmds));
				printf("shmget mapid %lx key %x shmflg %o\n", 
					memmap.cm_mapid,
					shmds.shm_perm.key,
					shmds.shm_perm.mode);
			} else {
				printf("shmget mapid %lx key PRIVATE\n",
					memmap.cm_mapid);
				lseek(tp->sfd, (off_t)memmap.cm_auxsize, SEEK_CUR);
			}
		} else
			lseek(tp->sfd, (off_t)memmap.cm_auxsize, SEEK_CUR);

		lseek(tp->sfd, (off_t)(memmap.cm_nsegs*segobj_size), SEEK_CUR);
		if (memmap.cm_modlist)
			lseek(tp->sfd, (off_t)(memmap.cm_nmod * sizeof(caddr_t)), SEEK_CUR);
	}
	return (0);
}
#endif

/* ARGSUSED */
int
ckpt_stat_context(ckpt_ta_t *tp, ckpt_magic_t magic, ckpt_stat_t *sp)
{
	ckpt_context_t context;
#ifdef DEBUG_CONTEXT
	greg_t *gregp = tp->ctxt.cc_ctxt.uc_mcontext.gregs;
#endif
	if (sp->cs_abi >= 0)
		/*
		 * already got it
		 */
		return (0);

	if (ckpt_rxlate_context(cr.cr_pci.pci_revision, tp, magic, &context) < 0)
		return (-1);

	IFDEB1(cdebug("stat context (magic %8.8s)\n", &magic));

	if (context.cc_magic != magic) {
		cerror("mismatched magic %8.8s (vs. %8.8s)\n",
			&context.cc_magic, &magic);
		setoserror(EINVAL);
		return (-1);
	}
	sp->cs_abi = context.cc_ctxt.cc_abi;

#ifdef DEBUG_CONTEXT
	printf("epc=%lx sr=%x v0=%ld, a0=%ld, a1=%lx,"
			"a2=%ld a3=%ld s2=%lx t0=%lx t8=%lx ra=%lx\n",
			gregp[CXT_EPC],
			gregp[CXT_SR],
			gregp[CXT_V0],
			gregp[CXT_A0],
			gregp[CXT_A1],
			gregp[CXT_A2],
			gregp[CXT_A3],
			gregp[CXT_S2],
			gregp[CXT_T0],
			gregp[CXT_T8],
			gregp[CXT_RA]);
#endif
	return 0;
}
