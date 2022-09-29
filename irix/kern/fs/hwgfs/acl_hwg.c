#include <sys/types.h>
#include <sys/attributes.h>
#include <sys/acl.h> 
#include <sys/errno.h>
#include <sys/systm.h>
#include <ksys/hwg.h>
#ifdef DEBUG
#include <sys/cmn_err.h>
#endif

#ident "$Revision: 1.2 $"

int
acl_hwg_iaccess(vertex_hdl_t vhdl, uid_t uid, gid_t gid, mode_t mode,
		mode_t filemode, struct cred *cr)
{
	acl_t acl;
	int exportinfo;
	arbitrary_info_t info;
	graph_error_t rv;
#ifdef DEBUG
	int error = 0;
#endif

	/* look up SGI_ACL_FILE */
	rv = hwgraph_info_get_exported_LBL(vhdl, SGI_ACL_FILE, &exportinfo,
					   &info);

	/*
	 * If the file has no ACL, report error.
	 */
	if (rv != GRAPH_SUCCESS ||
	    exportinfo == INFO_DESC_PRIVATE ||
	    exportinfo == INFO_DESC_EXPORT ||
	    exportinfo != sizeof(struct acl) ||
	    (acl = (acl_t) info) == NULL ||
	    acl->acl_cnt == ACL_NOT_PRESENT)
		acl = NULL;

	if (acl == NULL)
		return(-1);

	acl_sync_mode(filemode, acl);
#ifdef DEBUG
	if (error = acl_access(uid, gid, acl, mode, cr)) {
		cmn_err(CE_NOTE, "acl_hwg_iaccess: %s(%d) acl_access = %d",
			__FILE__, __LINE__, error);
	}
	return error;
#else
	return acl_access(uid, gid, acl, mode, cr);
#endif
}

__inline static int
acl_match_name(const char *name)
{
	size_t len = strlen(name) + 1;

	return (len == sizeof(SGI_ACL_FILE) && !bcmp(name, SGI_ACL_FILE, len));
}

/*ARGSUSED*/
int
acl_hwg_get(vertex_hdl_t vhdl, char *name, char *value, int *valuelenp,
	    int flags)
{
	if (acl_match_name(name)) {
		if ((flags & ATTR_ROOT) == 0)
			return(ENOATTR);

		if (*valuelenp < sizeof(struct acl)) {
			*valuelenp = sizeof(struct acl);
			return(E2BIG);
		}
	}
	return(-1);
}

int
acl_hwg_match(const char *name, int flags)
{
	if (acl_match_name(name)) {
		if ((flags & ATTR_ROOT) == 0)
			return(ENOATTR);
	}
	return(0);
}
