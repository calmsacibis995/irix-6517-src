#ident "$Revision: 1.16 $"

#ifdef SIM
#define	_KERNEL 1
#endif
#include <sys/types.h>
#include <sys/uuid.h>
#include <sys/vfs.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/buf.h>
#ifdef SIM
#undef _KERNEL
#endif
#include <sys/pda.h>
#include <sys/errno.h>
#include <sys/debug.h>
#ifdef SIM
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include "sim.h"
#else
#include <sys/systm.h>
#endif
#include "xfs_macros.h"
#include "xfs_types.h"
#include "xfs_inum.h"
#include "xfs_log.h"
#ifndef SIM
#include "xfs_cxfs.h"
#endif
#include "xfs_sb.h"
#include "xfs_trans.h"
#include "xfs_dir.h"
#include "xfs_mount.h"
#include "xfs_utils.h"
#include "xfs_error.h"

#ifdef DEBUG
int	xfs_etrap[XFS_ERROR_NTRAP] = {
#ifdef SIM
	EFSCORRUPTED,
#else
	0,
#endif
};

int
xfs_error_trap(int e)
{
	int i;

	if (!e)
		return 0;
	for (i = 0; i < XFS_ERROR_NTRAP; i++) {
		if (xfs_etrap[i] == 0)
			break;
		if (e != xfs_etrap[i])
			continue;
#ifdef SIM
		abort();
#else
		cmn_err(CE_NOTE, "xfs_error_trap: error %d", e);
		debug_stop_all_cpus((void *)-1LL);
		debug("xfs");
#endif
		break;
	}
	return e;
}
#endif

#if !defined(SIM) && (defined(DEBUG) || defined(INDUCE_IO_ERROR))

int	xfs_etest[XFS_NUM_INJECT_ERROR];
int64_t	xfs_etest_fsid[XFS_NUM_INJECT_ERROR];
char *	xfs_etest_fsname[XFS_NUM_INJECT_ERROR];
extern int xfs_get_fsinfo(int fd, char **fsname, int64_t *fsid);

void
xfs_error_test_init(void)
{
	bzero(xfs_etest, sizeof(xfs_etest));
	bzero(xfs_etest_fsid, sizeof(xfs_etest_fsid));
	bzero(xfs_etest_fsname, sizeof(xfs_etest_fsname));
}

int
xfs_error_test(int error_tag, int *fsidp, char *expression,
	       int line, char *file, unsigned long randfactor)
{
	int i;
	int64_t fsid;

	if (random() % randfactor)
		return 0;

	bcopy(fsidp, &fsid, sizeof(fsid_t));

	for (i = 0; i < XFS_NUM_INJECT_ERROR; i++)  {
		if (xfs_etest[i] == error_tag && xfs_etest_fsid[i] == fsid) {
			cmn_err(CE_WARN,
	"Injecting error (%s) at file %s, line %d, on filesystem \"%s\"\n",
				expression, file, line, xfs_etest_fsname[i]);
			return 1;
		}
	}

	return 0;
}

int
xfs_errortag_add(int error_tag, int fd)
{
	int i;
	int error;
	int len;
	int64_t fsid;
	char *fsname;

	if ((error = xfs_get_fsinfo(fd, &fsname, &fsid)))
		return error;

	for (i = 0; i < XFS_NUM_INJECT_ERROR; i++)  {
		if (xfs_etest_fsid[i] == fsid && xfs_etest[i] == error_tag) {
			cmn_err(CE_WARN, "XFS error tag #%d on", error_tag);
			return 0;
		}
	}

	for (i = 0; i < XFS_NUM_INJECT_ERROR; i++)  {
		if (xfs_etest[i] == 0) {
			cmn_err(CE_WARN, "Turned on XFS error tag #%d",
				error_tag);
			xfs_etest[i] = error_tag;
			xfs_etest_fsid[i] = fsid;
			len = strlen(fsname);
			xfs_etest_fsname[i] = kmem_alloc(len + 1, KM_SLEEP);
			strcpy(xfs_etest_fsname[i], fsname);
			return 0;
		}
	}

	cmn_err(CE_WARN, "error tag overflow, too many turned on");

	return 1;
}

int
xfs_errortag_clear(int error_tag, int fd)
{
	int i;
	int error;
	int64_t fsid;
	char *fsname;

	if ((error = xfs_get_fsinfo(fd, &fsname, &fsid)))
		return error;

	for (i = 0; i < XFS_NUM_INJECT_ERROR; i++) {
		if (xfs_etest_fsid[i] == fsid && xfs_etest[i] == error_tag) {
			xfs_etest[i] = 0;
			xfs_etest_fsid[i] = 0LL;
			kmem_free(xfs_etest_fsname[i],
				  strlen(xfs_etest_fsname[i]) + 1);
			xfs_etest_fsname[i] = NULL;
			cmn_err(CE_WARN, "Cleared XFS error tag #%d",
				error_tag);
			return 0;
		}
	}

	cmn_err(CE_WARN, "XFS error tag %d not on", error_tag);

	return 1;
}

int
xfs_errortag_clearall_umount(int64_t fsid, char *fsname, int loud)
{
	int i;
	int cleared = 0;

	for (i = 0; i < XFS_NUM_INJECT_ERROR; i++) {
		if ((fsid == 0LL || xfs_etest_fsid[i] == fsid) &&
		     xfs_etest[i] != 0) {
			cleared = 1;
			cmn_err(CE_WARN, "Clearing XFS error tag #%d",
				xfs_etest[i]);
			xfs_etest[i] = 0;
			xfs_etest_fsid[i] = 0LL;
			kmem_free(xfs_etest_fsname[i],
				  strlen(xfs_etest_fsname[i]) + 1);
			xfs_etest_fsname[i] = NULL;
		}
	}

	if (loud || cleared)
		cmn_err(CE_WARN,
			"Cleared all XFS error tags for filesystem \"%s\"",
			fsname);

	return 0;
}

int
xfs_errortag_clearall(int fd)
{
	int error;
	int64_t fsid;
	char *fsname;

	if ((error = xfs_get_fsinfo(fd, &fsname, &fsid)))
		return error;

	return xfs_errortag_clearall_umount(fsid, fsname, 1);
}
#endif /* DEBUG || INDUCE_IO_ERROR */

#ifndef SIM
static void
xfs_fs_vcmn_err(int level, xfs_mount_t *mp, char *fmt, va_list ap)
{
	char	*newfmt;
	int	len = 16 + mp->m_fsname_len + strlen(fmt);

	newfmt = kmem_alloc(len, KM_SLEEP);
	sprintf(newfmt, "Filesystem \"%s\": %s", mp->m_fsname, fmt);
	icmn_err(level, newfmt, ap);
	kmem_free(newfmt, len);
}

void
xfs_fs_cmn_err(int level, xfs_mount_t *mp, char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	xfs_fs_vcmn_err(level, mp, fmt, ap);
	va_end(ap);
}

void
xfs_cmn_err(uint64_t panic_tag, int level, xfs_mount_t *mp, char *fmt, ...)
{
	va_list ap;

	if (xfs_panic_mask && (xfs_panic_mask & panic_tag)
	    && (level & CE_ALERT)) {
		level &= ~CE_ALERT;
		level |= CE_PANIC;
		cmn_err(CE_ALERT|CE_SYNC,
			"Transforming an alert into a panic.");
	}
	va_start(ap, fmt);
	xfs_fs_vcmn_err(level, mp, fmt, ap);
	va_end(ap);
}
#endif
