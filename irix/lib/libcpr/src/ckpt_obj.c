/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995 Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.33 $"

#include <fcntl.h>
#include <pwd.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <bstring.h>
#include <sys.s>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/cred.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/procfs.h>
#include <sys/syssgi.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/ckpt_procfs.h>
#include <sys/ckpt_sys.h>
#include <sys/sysmp.h>
#include <sys/schedctl.h>
#include "ckpt.h" /* for now */
#include "ckpt_internal.h"

struct ckpt_prop_handle cpr_share_prop_table[] = {
	0,	CKPT_MAGIC_CHILDINFO,	"treeinfo",
	ckpt_write_treeinfo,	NULL,	
	ckpt_read_treeinfo,	NULL,
	NULL,			NULL,

	1,	CKPT_MAGIC_DEPEND,	"shmem dep",
	ckpt_write_depend,	NULL,	
	ckpt_restore_depend,	NULL,
	NULL,			NULL,

	2,	CKPT_MAGIC_PRATTACH,	"attach",
	ckpt_write_prattach,	NULL,	
	ckpt_restore_prattach,	NULL,
	NULL,			NULL,

	3,	CKPT_MAGIC_UNLINKED,	"unlinked",
	ckpt_write_unlinked,	NULL,	
	ckpt_restore_unlinked,	NULL,
	NULL,			NULL,

	4,	CKPT_MAGIC_PUSEMA,	"Posix sema",
	ckpt_write_pusema,	NULL,
	ckpt_restore_pusema,	NULL,
	NULL,			NULL,

	5,	CKPT_MAGIC_MLDLINK,	"mldlink",
	ckpt_save_mldlink,	NULL,
	ckpt_restore_mldlink,	NULL,
	NULL,			NULL,

	6,	CKPT_MAGIC_HINV,	"hinv",
	ckpt_save_hinv,		NULL,
	ckpt_read_hinv,		NULL,
	NULL,			NULL,

	7,	CKPT_MAGIC_PMI,		"page_migr",
	ckpt_save_pmi,		NULL,
	ckpt_restore_pmi,	NULL,
	NULL,			NULL,
};
/*
 * Per process objects.  Ordering in this table is important
 */
struct ckpt_prop_handle cpr_proc_prop_table[] = {
	0,	CKPT_MAGIC_CHILDINFO,	"childinfo",
	ckpt_write_childinfo,	NULL,	
	ckpt_read_childinfo,	NULL,
	NULL,			NULL,

	1,	CKPT_MAGIC_PROCINFO,	"procinfo",
	ckpt_write_procinfo,	NULL,	
	ckpt_read_procinfo,	NULL,
	NULL,			ckpt_stat_procinfo,

	2,	CKPT_MAGIC_THREAD,	"thread",
	ckpt_write_thread,	NULL,	
	ckpt_read_thread,	NULL,
	NULL,			NULL,

	3,	CKPT_MAGIC_CTXINFO,	"context",
	ckpt_write_context,	NULL,	
	ckpt_read_context,	NULL,
	NULL,			ckpt_stat_context,

	4,	CKPT_MAGIC_SIGINFO,	"siginfo",
	ckpt_write_siginfo,	NULL,	
	ckpt_read_siginfo,	NULL,
	NULL,			NULL,

	5,	CKPT_MAGIC_THRDSIGINFO,	"thrdsiginfo",
	ckpt_write_thread_siginfo,	NULL,	
	ckpt_read_thread_siginfo,	NULL,
	NULL,			NULL,

	6,	CKPT_MAGIC_OPENSPECS,	"openspecs",
	ckpt_write_openspecs,	NULL,
	ckpt_restore_files,	NULL,
	ckpt_remove_files,	ckpt_stat_files,

	7,	CKPT_MAGIC_OPENPIPES, 	"openpipes",
	ckpt_write_openpipes,	NULL,
	ckpt_restore_pipes,	NULL,
	NULL,			NULL,

	8,	CKPT_MAGIC_OPENFILES,	"openfiles",
	ckpt_write_openfiles,	NULL,
	ckpt_restore_files,	NULL,
	ckpt_remove_files,	ckpt_stat_files,

	9,	CKPT_MAGIC_OTHER_OFDS,	"other openfds",
	ckpt_write_fds,		NULL,
	ckpt_restore_fds,	NULL,
	NULL,			ckpt_stat_files,

	10,	CKPT_MAGIC_PMODULE,	"pmodule",
	ckpt_save_pmodule,	NULL,
	ckpt_restore_pmodule,	NULL,
	NULL,			NULL,

	11,	CKPT_MAGIC_PMDEFAULT,	"pmdefault",
	ckpt_save_pmdefault,	NULL,
	ckpt_restore_pmdefault,	NULL,
	NULL,			NULL,

	12,	CKPT_MAGIC_MLDSET,	"mldset",
	ckpt_save_mldset,		NULL,
	ckpt_restore_mldset,	NULL,
	NULL,			NULL,

	13,	CKPT_MAGIC_MLD,		"mldomain",
	ckpt_save_mld,		NULL,
	ckpt_restore_mld,	NULL,
	NULL,			NULL,

	14,	CKPT_MAGIC_MAPFILE,	"mapfiles",
	ckpt_write_mapfiles,	NULL,
	ckpt_restore_files,	NULL,
	ckpt_remove_files,	NULL,

	15,	CKPT_MAGIC_MEMOBJ,	"memobj",
	ckpt_write_memobj,	NULL,
	ckpt_restore_memobj,	NULL,
#ifdef DEBUG_MEMOBJ
	NULL,			ckpt_stat_memobj,
#else
	NULL,			NULL,
#endif

	16, CKPT_MAGIC_SHM,		"sysVshm",
	ckpt_save_shm,		NULL,
	ckpt_restore_shm,	NULL,
	NULL,
};


#define	CKPT_ACTION_RESTART	1
#define	CKPT_ACTION_REMOVE	2
#define	CKPT_ACTION_STAT	3

static struct strmap rev_map[] = {
	CKPT_REVISION_NONE,	"No CPR Revision Number",
	CKPT_REVISION_10,	"SGI CPR Revision 1.0",
	CKPT_REVISION_11,	"SGI CPR Revision 1.1",
	CKPT_REVISION_12,	"SGI CPR Revision 1.2",
};

#define	TABLE_SIZE(table)	(sizeof(table)/sizeof(ckpt_phandle_t))
#define FOR_EACH_PROPERTY(table, p)    for (p = &table[0]; \
	p <= &table[TABLE_SIZE(table)-1]; p++)

/*
 * Special abort condition: allow childinfo and procinfo to finish if we
 * we have started writing them when receiving abort request.
 */
#define	SKIP_THIS_OBJ(ch, magic)	((ch->ch_flags & CKPT_PCI_ABORT) && \
					magic != CKPT_MAGIC_CHILDINFO &&   \
					magic != CKPT_MAGIC_PROCINFO)

static int ckpt_fill_prop_cache(ckpt_ta_t *);
static int ckpt_handle_proc_property(char, ckpt_ta_t *, ckpt_magic_t, int *);

static ckpt_pci_t pci;

void
ckpt_share_prop_table_fixup(ckpt_magic_t magic, int (*func_addr)())
{
	ckpt_phandle_t *phand;

	FOR_EACH_PROPERTY(cpr_share_prop_table, phand) {
		if (phand->prop_magic == magic) {
			phand->prop_restart = func_addr;
			break;
		}
	}
}

void
ckpt_proc_prop_table_fixup(ckpt_magic_t magic, int (*func_addr)())
{
	ckpt_phandle_t *phand;

	FOR_EACH_PROPERTY(cpr_proc_prop_table, phand) {
		if (phand->prop_magic == magic) {
			phand->prop_restart = func_addr;
			break;
		}
	}
}

int
ckpt_write_one_share_prop(ch_t *ch, ckpt_magic_t magic)
{
	ckpt_phandle_t *phand;
	ckpt_prop_t prop;
	int rc;

	/*
	 * find the right property handle
	 */
	FOR_EACH_PROPERTY(cpr_share_prop_table, phand) {
		if (phand->prop_checkpoint == NULL)
			continue;

		if (phand->prop_magic == magic)
			break;
	}
	if (phand > &cpr_share_prop_table[TABLE_SIZE(cpr_share_prop_table)-1]) {
		cerror("Don't know how to process property %s\n", phand->prop_name);
		return (-1);
	}
	bzero(&prop, sizeof(ckpt_prop_t));
	prop.prop_offset = ch->ch_propoffset;
	/*
	 * Write out the pid.state file first for each property
	 */
	if (phand->prop_checkpoint == NULL)
		return (0);

	/*
	 * Initialize property index
	 */
	prop.prop_magic = phand->prop_magic;
	strcpy(prop.prop_name, phand->prop_name);

	if (SKIP_THIS_OBJ(ch, phand->prop_magic))
		return (0);

	rc = (*phand->prop_checkpoint)(ch, &prop);

	IFDEB1(cdebug("%s callback return: %d\n", phand->prop_name, rc));
	/*
	 * record the state property header into the pid.index file
	 */
	if (ckpt_write_propindex(ch->ch_ifd, &prop, &ch->ch_nprops, &ch->ch_propoffset)) 
		return (-1);
	return (rc);
}
int
ckpt_write_share_properties(ch_t *ch)
{
	ckpt_phandle_t *phand;
	ckpt_prop_t prop;
	int rc = 0;

	bzero(&prop, sizeof(ckpt_prop_t));
	prop.prop_offset = ch->ch_propoffset;

	FOR_EACH_PROPERTY(cpr_share_prop_table, phand) {
		/*
		 * Write out the pid.state file first for each property
		 */
		if (phand->prop_checkpoint == NULL)
			continue;
		/*
		 * childinfo saved in the begining so that the statefile
		 * contains the min info for our proc tree
		 */
		if (phand->prop_magic == CKPT_MAGIC_CHILDINFO)
			continue;
		/*
		 * Initialize property index
		 */
		prop.prop_magic = phand->prop_magic;
		strcpy(prop.prop_name, phand->prop_name);

		if (SKIP_THIS_OBJ(ch, phand->prop_magic))
			continue;

		rc = (*phand->prop_checkpoint)(ch, &prop);

		IFDEB1(cdebug("%s callback return: %d\n", phand->prop_name, rc));
		/*
		 * record the state property header into the pid.index file
		 */
		if (ckpt_write_propindex(ch->ch_ifd, &prop, &ch->ch_nprops, &ch->ch_propoffset) ||
		    rc < 0) {
			if (oserror() == ENOSPC)
				return (-1);
			break;
		}
	}
	/*
	 * Go back to the top and write out the header again for nprops
	 */
	if (lseek(ch->ch_ifd, 0, SEEK_SET) < 0) {
		cerror("Cannot seek indexfile (%s)\n", STRERR);
		return (-1);
	}
	pci.pci_prop_count = ch->ch_nprops;
	pci.pci_flags = ch->ch_flags;

	if (write(ch->ch_ifd, &pci, sizeof(ckpt_pci_t)) < 0) {
		cerror("Failed to write share index header (%s)\n", STRERR);
		return (-1);
	}
	return (rc);
}

int
ckpt_write_pci(ch_t *ch)
{
	char path[CPATHLEN];

	sprintf(path, "%s/%s.%s", ch->ch_path, STATEF_SHARE, STATEF_INDEX);
	if ((ch->ch_ifd = open(path, O_CREAT|O_RDWR, S_IRUSR|S_IRGRP|S_IROTH))
		== -1) {
		cerror("failed to open %s: errno %d\n", path, oserror());
		return (-1);
	}
	sprintf(path, "%s/%s.%s", ch->ch_path, STATEF_SHARE, STATEF_STATE);
	if ((ch->ch_sfd = open(path, O_CREAT|O_RDWR, S_IRUSR|S_IRGRP|S_IROTH))
		== -1) {
		cerror("failed to open %s: errno %d\n", path, oserror());
		close(ch->ch_ifd);
		return (-1);
	}
	if (fchown(ch->ch_ifd, getuid(), getgid()) ||
	    fchown(ch->ch_sfd, getuid(), getgid())) {
		cerror("Failed to change owner (%s)\n", STRERR);
		return (-1);
	}
	pci.pci_magic = CKPT_MAGIC_INDEX;
	pci.pci_ruid = getuid();
	pci.pci_rgid = getgid();
	pci.pci_revision = CKPT_REVISION_CURRENT;
	pci.pci_id = ch->ch_id;
	pci.pci_type = ch->ch_type;
	pci.pci_nproc = ch->ch_nproc;
	pci.pci_ntrees = ch->ch_ntrees;
	pci.pci_prop_count = 0;

	if (write(ch->ch_ifd, &pci, sizeof(ckpt_pci_t)) < 0) {
		cerror("Failed to write share index header (%s)\n", STRERR);
		close(ch->ch_ifd);
		close(ch->ch_sfd);
		return (-1);
	}
	if (ckpt_write_one_share_prop(ch, CKPT_MAGIC_CHILDINFO)) {
		close(ch->ch_ifd);
		close(ch->ch_sfd);
		return (-1);
	}
	return (0);
}

int
ckpt_write_proc_properties(ckpt_obj_t *co)
{
	ckpt_ppi_t ppi;
	ckpt_phandle_t *phand;
	ckpt_prop_t prop;
	int rc = 0;

	assert(co->co_prfd >= 0);

	if (ckpt_exam_fdsharing_sproc(co))
		return (-1);

	bzero(&prop, sizeof(ckpt_prop_t));
	FOR_EACH_PROPERTY(cpr_proc_prop_table, phand) {

		if (phand->prop_checkpoint == NULL)
			continue;
		/*
		 * Initialize property index
		 */
		prop.prop_magic = phand->prop_magic;
		strcpy(prop.prop_name, phand->prop_name);

		if (SKIP_THIS_OBJ(co->co_ch, phand->prop_magic))
			continue;

		IFDEB1(cdebug("\nProcess Object: %s\n", prop.prop_name));

		rc = (*phand->prop_checkpoint)(co, &prop);

		IFDEB1(cdebug("%s callback return: %d\n", prop.prop_name, rc));
		/*
		 * Must write out the propindex even checkpoint failed. Otherwise
		 * we cannot unwind.
		 *
		 * record the state property header into the pid.index file
		 */
		if (ckpt_write_propindex(co->co_ifd, &prop, &co->co_nprops, NULL) ||
		    rc < 0)
			break;
	}
	/*
	 * Go back to the top and write out the header
	 */
	if (lseek(co->co_ifd, 0, SEEK_SET) < 0) {
		cerror("Cannot seek indexfile (%s)\n", STRERR);
		return (-1);
	}
	ppi.ppi_magic = CKPT_MAGIC_INDEX;
	ppi.ppi_pid = co->co_pid;
	ppi.ppi_prop_count = co->co_nprops;

	if (write(co->co_ifd, &ppi, sizeof(ckpt_ppi_t)) < 0) {
		cerror("Failed to write indexfile header for pid %d\n", co->co_pid);
		return (-1);
	}
	return (rc);
}

int
ckpt_read_share_property(cr_t *cp, ckpt_ta_t *tp, ckpt_magic_t magic, int *misc)
{
	ckpt_prop_t *prop;
	ckpt_phandle_t *phand;
	int i, rc;
	int sfd = (tp)? tp->ssfd : cp->cr_sfd;

	cp->cr_handle_count = TABLE_SIZE(cpr_share_prop_table);
	if (cp->cr_index == NULL) {
		cerror("shared index was not yet read. Abort...\n");
		return (-1);
	}

	/*
	 * find the right property handle
	 */
	FOR_EACH_PROPERTY(cpr_share_prop_table, phand) {
		if (phand->prop_restart == NULL)
			continue;

		if (phand->prop_magic == magic)
			break;
	}
	if (phand > &cpr_share_prop_table[TABLE_SIZE(cpr_share_prop_table)-1]) {
		cerror("Don't know how to process property %s\n", phand->prop_name);
		return (-1);
	}

	/*
	 * go thru all property with the given magic
	 */
	for (i = 0, prop = cp->cr_index; i < cp->cr_pci.pci_prop_count;
		i++, prop++) {
		int n;

		if (magic != prop->prop_magic || prop->prop_quantity == 0)
			continue;

		IFDEB1(cdebug("restarting prop %s, quantity %d, offset 0x%x\n",
			prop->prop_name, prop->prop_quantity, prop->prop_offset));

		/*
		 * Point to the correct property in the statefile
		 */
		if (lseek(sfd, (off_t)prop->prop_offset, SEEK_SET) < 0) {
			cerror("Cannot seek indexfile (%s)\n", STRERR);
			return (-1);
		}
		for (n = 0; n < prop->prop_quantity; n++) {
			if (rc = (*phand->prop_restart)(cp, tp, magic, misc))
				return (rc);
		}
	}
	return (0);
}

int
ckpt_read_proc_property(ckpt_ta_t *tp, ckpt_magic_t magic)
{
	return (ckpt_handle_proc_property(CKPT_ACTION_RESTART, tp, magic, NULL));
}

int
ckpt_remove_proc_property(ckpt_ta_t *tp, ckpt_magic_t magic)
{
 	return (ckpt_handle_proc_property(CKPT_ACTION_REMOVE, tp, magic, NULL));
}

int
ckpt_stat_proc_property(ckpt_ta_t *tp, ckpt_magic_t magic, ckpt_stat_t *sp)
{
 	return (ckpt_handle_proc_property(CKPT_ACTION_STAT, tp, magic, (int *)sp));
}

static int
ckpt_handle_proc_property(char action, ckpt_ta_t *tp, ckpt_magic_t magic, int *misc)
{
	ckpt_prop_t *prop;
	ckpt_phandle_t *phand;
	int i, rc;

	if (tp->index == NULL) {
		IFDEB1(cdebug("fill in the property index cache\n"));
		if (ckpt_fill_prop_cache(tp))
			return (-1);
	}

	/*
	 * find the right property handle
	 */
	FOR_EACH_PROPERTY(cpr_proc_prop_table, phand) {
		switch (action) {
		case CKPT_ACTION_RESTART:
			if (phand->prop_restart == NULL)
				continue;
			break;
		case CKPT_ACTION_REMOVE:
			if (phand->prop_remove == NULL)
				continue;
			break;
		case CKPT_ACTION_STAT:
			if (phand->prop_stat == NULL)
				continue;
			break;
		}
		if (phand->prop_magic == magic)
			break;
	}
	if (phand > &cpr_proc_prop_table[TABLE_SIZE(cpr_proc_prop_table)-1]) {
		cnotice("Don't know how to handle property %s\n", &magic);
		return (0);
	}
	/*
	 * go thru all property with the given magic
	 */
	for (i = 0, prop = tp->index; i < tp->ixheader.ppi_prop_count;
		i++, prop++) {
		int n;

		if (magic != prop->prop_magic || prop->prop_quantity == 0)
			continue;

		IFDEB1(cdebug("handling prop %s, quantity %d, offset 0x%x\n",
			prop->prop_name, prop->prop_quantity, prop->prop_offset));

		/*
		 * Point to the correct property in the statefile
		 */
		if (lseek(tp->sfd, (off_t)prop->prop_offset, SEEK_SET) < 0) {
			cerror("Cannot seek statefile for magic %s (%s)\n",
				prop->prop_name, STRERR);
			return (-1);
		}
		for (n = 0; n < prop->prop_quantity; n++) {
			switch (action) {
			case CKPT_ACTION_RESTART:
				if (rc = (*phand->prop_restart)(tp, magic))
					return (rc);
				break;
			case CKPT_ACTION_REMOVE:
				if (rc = (*phand->prop_remove)(tp, magic))
					return (rc);
				break;
			case CKPT_ACTION_STAT:
				if (rc = (*phand->prop_stat)
					(tp, magic, (ckpt_stat_t *)misc))
					return (rc);
				break;
			}
		}
	}
	return (0);
}

int
ckpt_write_propindex(int ifd, ckpt_prop_t *pp, int *count, int *offset)
{
	/*
	 * No this type of object
	 */
	if (pp->prop_quantity == 0)
		return (0);

	if (write(ifd, pp, sizeof(ckpt_prop_t)) < 0) {
		cerror("Failed to write index %s (%s)\n", pp->prop_name, STRERR);
		return (-1);
	}
	IFDEB1(cdebug("Property: magic=%x name=%s off=0x%x ulen=0x%x quan=%d\n",
		pp->prop_magic, pp->prop_name, pp->prop_offset,
		pp->prop_len, pp->prop_quantity));
	if (offset)
		*offset += pp->prop_len;
	pp->prop_offset += pp->prop_len;
	pp->prop_len = 0;
	pp->prop_quantity = 0;
	(*count)++;
	return (0);
}

static int
ckpt_fill_prop_cache(ckpt_ta_t *tp)
{
	size_t size;
	extern cr_t cr;

	cr.cr_procprop_count = TABLE_SIZE(cpr_proc_prop_table);
	/*
	 * read in the per-proc header and all property index into a cache 
	 */
	if (read(tp->ifd, &tp->ixheader, sizeof(ckpt_ppi_t)) < 0) {
		cerror("read (%s)\n", STRERR);
		return (-1);
	}
	if (tp->ixheader.ppi_magic != CKPT_MAGIC_INDEX) {
		cerror("Incorrect per-proc header magic %x (vs %x)\n",
			tp->ixheader.ppi_magic, CKPT_MAGIC_INDEX);
		return (-1);
	}
	IFDEB1(cdebug("property count: %d\n", tp->ixheader.ppi_prop_count));

	/*
	 * Check if the statefile is corrupted 
	 */
	if (tp->ixheader.ppi_prop_count == 0) {
		off_t cur, count;

		cnotice("Processing corrupted statefile\n");
		cur = lseek(tp->ifd, 0, SEEK_CUR);
		count = lseek(tp->ifd, 0, SEEK_END);
		count -= sizeof(ckpt_ppi_t);
		count /= sizeof(ckpt_prop_t);
		tp->ixheader.ppi_prop_count = (int)count;
		lseek(tp->ifd, cur, SEEK_SET);
	}
	size = sizeof(ckpt_prop_t) * tp->ixheader.ppi_prop_count;
	if ((tp->index = (ckpt_prop_t *)malloc(size)) == NULL) {
		cerror("Failed to allocate memory (%s)\n", STRERR);
		return (-1);
	}
	if (read(tp->ifd, tp->index, size) < 0) {
		cerror("ckpt_fill_prop_cache: read error (%s)\n", STRERR);
		return (-1);
	}
	return (0);
}

char *
rev_to_str(ckpt_rev_t rev)
{
	int i;

	for (i = 0; i < sizeof (rev_map)/sizeof (struct strmap); i++) {
		if (rev_map[i].id == rev)
			return (rev_map[i].s_id);
	}
	return (NULL);
}
