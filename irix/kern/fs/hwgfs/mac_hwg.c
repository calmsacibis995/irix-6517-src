#include <sys/types.h>
#include <sys/attributes.h>
#include <sys/mac.h> 
#include <sys/mac_label.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <ksys/hwg.h>
#ifdef DEBUG
#include <sys/cmn_err.h>
#endif

#ident "$Revision: 1.1 $"

static void *
mac_hwg_get_internal(vertex_hdl_t vhdl, char *name, int type)
{
	int exportinfo;
	arbitrary_info_t info;
	graph_error_t rc;
	void *lbl;
	extern mac_t mac_low_high_lp;

	/* look up name */
	rc = hwgraph_info_get_exported_LBL(vhdl, name, &exportinfo, &info);

	/* If the file has no MAC label, report error. */
	if (rc != GRAPH_SUCCESS ||
	    exportinfo == INFO_DESC_PRIVATE ||
	    exportinfo == INFO_DESC_EXPORT ||
	    (type == 0 && !(lbl = (void *) mac_add_label((mac_t) info))) ||
	    (type == 1 && !(lbl = (void *) msen_add_label((msen_t) info))) ||
	    (type == 2 && !(lbl = (void *) mint_add_label((mint_t) info))))
		lbl = NULL;

	if (lbl != NULL)
		return(lbl);
	else if (type == 1)
		return(msen_from_mac(mac_low_high_lp));
	else if (type == 2)
		return(mint_from_mac(mac_low_high_lp));
	else
		return(mac_low_high_lp);
}

int
mac_hwg_iaccess(vertex_hdl_t vhdl, mode_t mode, struct cred *cr)
{
	mac_t mac;
#ifdef DEBUG
	int error;
#endif

	mac = mac_hwg_get_internal(vhdl, SGI_MAC_FILE, 0);
	if (mac == NULL)
		return(EACCES);

#ifdef DEBUG
	if (error = mac_access(mac, cr, mode)) {
		cmn_err(CE_NOTE, "mac_hwg_iaccess: %s(%d) mac_access = %d",
			__FILE__, __LINE__, error);
	}
	return error;
#else
	return mac_access(mac, cr, mode);
#endif
}

__inline static int
mac_match_name(const char *name, int *type)
{
	size_t len = strlen(name) + 1;
	int t = -1;

	if (len == sizeof(SGI_MAC_FILE) && !bcmp(name, SGI_MAC_FILE, len))
		t = 0;
	else if (len == sizeof(SGI_BLS_FILE) && !bcmp(name, SGI_BLS_FILE, len))
		t = 1;
	else if (len == sizeof (SGI_BI_FILE) && !bcmp(name, SGI_BI_FILE, len))
		t = 2;
	if (type != NULL)
		*type = t;
	return (t != -1);
}

int
mac_hwg_get(vertex_hdl_t vhdl, char *name, char *value, int *valuelenp,
	    int flags)
{
	int type;

	if (mac_match_name(name, &type)) {
		ssize_t size;
		void *lbl;

		if ((flags & ATTR_ROOT) == 0)
			return(ENOATTR);

		lbl = mac_hwg_get_internal(vhdl, name, type);
		if (lbl == NULL)
			return(EACCES);

		if (type == 0)
			size = mac_size((mac_t) lbl);
		else if (type == 1)
			size = msen_size((msen_t) lbl);
		else if (type == 2)
			size = mint_size((mint_t) lbl);

		if (*valuelenp < size) {
			*valuelenp = size;
			return(E2BIG);
		}

		bcopy(lbl, value, *valuelenp = size);
		return(0);
	}
	return(-1);
}

int
mac_hwg_match(const char *name, int flags)
{
	if (mac_match_name(name, (int *) NULL)) {
		if ((flags & ATTR_ROOT) == 0)
			return(ENOATTR);
	}
	return(0);
}
