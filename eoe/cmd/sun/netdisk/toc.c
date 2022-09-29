/*
 * @(#)toc.c	1.1 88/06/08 4.0NFSSRC; from 1.4 87/09/21 D/NFS
 * Copyright (c) 1987 by Sun Microsystems, Inc.
 */

#include <rpc/rpc.h>
#include "toc.h"


bool_t
xdr_stringp(xdrs, objp)
	XDR *xdrs;
	stringp *objp;
{
	if (!xdr_string(xdrs, objp, ~0)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_string_list(xdrs, objp)
	XDR *xdrs;
	string_list *objp;
{
	if (!xdr_array(xdrs, (char **)&objp->string_list_val, (u_int *)&objp->string_list_len, ~0, sizeof(stringp), xdr_stringp)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_file_type(xdrs, objp)
	XDR *xdrs;
	file_type *objp;
{
	if (!xdr_enum(xdrs, (enum_t *)objp)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_media_type(xdrs, objp)
	XDR *xdrs;
	media_type *objp;
{
	if (!xdr_enum(xdrs, (enum_t *)objp)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_tapeaddress(xdrs, objp)
	XDR *xdrs;
	tapeaddress *objp;
{
	if (!xdr_int(xdrs, &objp->volno)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->label, ~0)) {
		return (FALSE);
	}
	if (!xdr_int(xdrs, &objp->file_number)) {
		return (FALSE);
	}
	if (!xdr_int(xdrs, &objp->size)) {
		return (FALSE);
	}
	if (!xdr_int(xdrs, &objp->volsize)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->dname, ~0)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_diskaddress(xdrs, objp)
	XDR *xdrs;
	diskaddress *objp;
{
	if (!xdr_int(xdrs, &objp->volno)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->label, ~0)) {
		return (FALSE);
	}
	if (!xdr_int(xdrs, &objp->offset)) {
		return (FALSE);
	}
	if (!xdr_int(xdrs, &objp->size)) {
		return (FALSE);
	}
	if (!xdr_int(xdrs, &objp->volsize)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->dname, ~0)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_Address(xdrs, objp)
	XDR *xdrs;
	Address *objp;
{
	if (!xdr_media_type(xdrs, &objp->dtype)) {
		return (FALSE);
	}
	switch (objp->dtype) {
	case TAPE:
		if (!xdr_tapeaddress(xdrs, &objp->Address_u.tape)) {
			return (FALSE);
		}
		break;
	case DISK:
		if (!xdr_diskaddress(xdrs, &objp->Address_u.disk)) {
			return (FALSE);
		}
		break;
	default:
		if (!xdr_nomedia(xdrs, &objp->Address_u.junk)) {
			return (FALSE);
		}
		break;
	}
	return (TRUE);
}




bool_t
xdr_contents(xdrs, objp)
	XDR *xdrs;
	contents *objp;
{
	if (!xdr_char(xdrs, &objp->junk)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_standalone(xdrs, objp)
	XDR *xdrs;
	standalone *objp;
{
	if (!xdr_string_list(xdrs, &objp->arch)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->size)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_executable(xdrs, objp)
	XDR *xdrs;
	executable *objp;
{
	if (!xdr_string_list(xdrs, &objp->arch)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->size)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_amorphous(xdrs, objp)
	XDR *xdrs;
	amorphous *objp;
{
	if (!xdr_u_int(xdrs, &objp->size)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_Install_tool(xdrs, objp)
	XDR *xdrs;
	Install_tool *objp;
{
	if (!xdr_string_list(xdrs, &objp->belongs_to)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->loadpoint, ~0)) {
		return (FALSE);
	}
	if (!xdr_bool(xdrs, &objp->moveable)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->size)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->workspace)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_Installable(xdrs, objp)
	XDR *xdrs;
	Installable *objp;
{
	if (!xdr_string_list(xdrs, &objp->arch_for)) {
		return (FALSE);
	}
	if (!xdr_string_list(xdrs, &objp->soft_depends)) {
		return (FALSE);
	}
	if (!xdr_bool(xdrs, &objp->required)) {
		return (FALSE);
	}
	if (!xdr_bool(xdrs, &objp->desirable)) {
		return (FALSE);
	}
	if (!xdr_bool(xdrs, &objp->common)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->loadpoint, ~0)) {
		return (FALSE);
	}
	if (!xdr_bool(xdrs, &objp->moveable)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->size)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->pre_install, ~0)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->install, ~0)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_file_kind(xdrs, objp)
	XDR *xdrs;
	file_kind *objp;
{
	if (!xdr_enum(xdrs, (enum_t *)objp)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_Information(xdrs, objp)
	XDR *xdrs;
	Information *objp;
{
	if (!xdr_file_kind(xdrs, &objp->kind)) {
		return (FALSE);
	}
	switch (objp->kind) {
	case CONTENTS:
		if (!xdr_contents(xdrs, &objp->Information_u.Toc)) {
			return (FALSE);
		}
		break;
	case AMORPHOUS:
		if (!xdr_amorphous(xdrs, &objp->Information_u.File)) {
			return (FALSE);
		}
		break;
	case STANDALONE:
		if (!xdr_standalone(xdrs, &objp->Information_u.Standalone)) {
			return (FALSE);
		}
		break;
	case EXECUTABLE:
		if (!xdr_executable(xdrs, &objp->Information_u.Exec)) {
			return (FALSE);
		}
		break;
	case INSTALLABLE:
		if (!xdr_Installable(xdrs, &objp->Information_u.Install)) {
			return (FALSE);
		}
		break;
	case INSTALL_TOOL:
		if (!xdr_Install_tool(xdrs, &objp->Information_u.Tool)) {
			return (FALSE);
		}
		break;
	default:
		if (!xdr_futureslop(xdrs, &objp->Information_u.junk)) {
			return (FALSE);
		}
		break;
	}
	return (TRUE);
}




bool_t
xdr_distribution_info(xdrs, objp)
	XDR *xdrs;
	distribution_info *objp;
{
	if (!xdr_string(xdrs, &objp->Title, ~0)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->Source, ~0)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->Version, ~0)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->Date, ~0)) {
		return (FALSE);
	}
	if (!xdr_int(xdrs, &objp->nentries)) {
		return (FALSE);
	}
	if (!xdr_int(xdrs, &objp->dstate)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_volume_info(xdrs, objp)
	XDR *xdrs;
	volume_info *objp;
{
	if (!xdr_Address(xdrs, &objp->vaddr)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_toc_entry(xdrs, objp)
	XDR *xdrs;
	toc_entry *objp;
{
	if (!xdr_string(xdrs, &objp->Name, ~0)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->LongName, ~0)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->Command, ~0)) {
		return (FALSE);
	}
	if (!xdr_Address(xdrs, &objp->Where)) {
		return (FALSE);
	}
	if (!xdr_file_type(xdrs, &objp->Type)) {
		return (FALSE);
	}
	if (!xdr_Information(xdrs, &objp->Info)) {
		return (FALSE);
	}
	return (TRUE);
}


