/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.3 $"

#include <ctype.h>
#include <stdlib.h>
#include <sys/debug.h>
#include <sys/types.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <xlv_oref.h>

#include "xlv_mgr.h"

/*
 * Parse the given name to produce a cursor representing the specified
 * object. If the object does not exist the "oref" field in the cursor
 * is set to NULL.
 *
 * volume	vol_name
 *		nodename.vol_name
 *
 * subvolume	subvol_name
 *		vol_name.{log,data,rt}
 *		nodename.vol_name.{log,data,rt}
 *
 * plex		plex_name
 *		vol_name.{log,data,rt}.#
 *		nodename.vol_name.{log,data,rt}.#
 *
 * ve		ve_name
 *		plex_name.#
 *		vol_name.{log,data,rt}.#.#
 *		nodename.vol_name.{log,data,rt}.#.#
 */
int
parse_name (	/* XXXjleong return specific error codes */
	char			*name,		/* IN - name to parse */
	int			type,		/* IN - expected type of name */
	xlv_mgr_cursor_t	*cursor)	/* OUT */
{
#define	MAX_NAME_PARTS	6
	char	buf[512];
	char	*part[MAX_NAME_PARTS] = {NULL, NULL, NULL, NULL, NULL, NULL};
	char	*nodename = NULL;
	char	*objname = NULL;
	char	*sv_part = NULL;
	char	*plex_part = NULL;
	char	*ve_part = NULL;
	int	sv_type = -1;
	int	plex_no = -1;
	int	ve_no = -1;
	int	retval = XLV_MGR_STATUS_OKAY;
	int	idx;
	uint	mark;
	int	obj_type = XLV_OBJ_TYPE_NONE;

	strcpy (buf, name);

	idx = 0;
	part[0] = strtok(buf, ".");
	while (part[idx] != NULL) {
		idx++;
		if (idx == MAX_NAME_PARTS)
			break;
		part[idx] = strtok(NULL, ".");
	}
	if (idx == MAX_NAME_PARTS || part[0] == NULL)
		return (XLV_MGR_STATUS_FAIL);

	if (idx == 1) {
		/*
		 * Standalone object
		 */
		objname = part[0];
		obj_type = type;
	} else {
		/*
		 * name is in the dot notation eg: vol.plex.ve
		 */
		switch(type) {
		    case XLV_OBJ_TYPE_VOL:
		    {
			if (idx == 2) {			/* node.vol */
				nodename = part[0];
				objname = part[1];
			} else {
				retval = XLV_MGR_STATUS_FAIL;
				goto error;
			}
			break;
		    }
		    case XLV_OBJ_TYPE_SUBVOL:
		    {
			if (idx == 3) {
				/* node.vol.sv_type */
				nodename  = part[0];
				objname   = part[1];
				sv_part   = part[2];
			} else if (idx == 2) {
				/* vol.sv_type */
				objname   = part[0];
				sv_part   = part[1];
			} else {
				retval = XLV_MGR_STATUS_FAIL;
				goto error;
			}
			break;
		    }
		    case XLV_OBJ_TYPE_PLEX:
		    {
			if (idx == 4) {
				/* node.vol.sv_type.plex_no */
				nodename  = part[0];
				objname   = part[1];
				sv_part   = part[2];
				plex_part = part[3];
			} else if (idx == 3) {
				/* vol.sv_type.plex_no */
				objname   = part[0];
				sv_part   = part[1];
				plex_part = part[2];
			} else if (idx == 2) {
				/* Not Supported: sv.plex_no */
				objname   = part[0];
				plex_part = part[1];
			} else {
				retval = XLV_MGR_STATUS_FAIL;
				goto error;
			}
			break;
		    }
		    case XLV_OBJ_TYPE_VOL_ELMNT:
		    {
			if (idx == 5) {
				/* node.vol.sv_type.plex_no.ve_no */
				nodename  = part[0];
				objname   = part[1];
				sv_part   = part[2];
				plex_part = part[3];
				ve_part   = part[4];
			} else if (idx == 4) {
				/* vol.sv_type.plex_no.ve_no */
				objname   = part[0];
				sv_part   = part[1];
				plex_part = part[2];
				ve_part   = part[3];
			} else if (idx == 3) {
				/* Not Supported: sv.plex_no.ve_no */
				objname   = part[0];
				plex_part = part[1];
				ve_part   = part[2];
			} else if (idx == 2) {
				/* plex.ve_no */
				objname   = part[0];
				ve_part   = part[1];
			} else {
				retval = XLV_MGR_STATUS_FAIL;
				goto error;
			}
			break;
		    }
		default:
			retval = XLV_MGR_STATUS_FAIL;
			goto error;
		}

		/*
		 * Check for valid subvolume type and plex|ve number
		 */
		if (sv_part) {
			if (1 == strlen(sv_part) && isdigit(sv_part[0])) {
				/*
				 * subvolume type is a number
				 */
				sv_type = atoi(sv_part);
ASSERT(XLV_SUBVOL_TYPE_LOG < XLV_SUBVOL_TYPE_DATA);
ASSERT(XLV_SUBVOL_TYPE_DATA < XLV_SUBVOL_TYPE_RT);
				if (sv_type > XLV_SUBVOL_TYPE_RT) {
					retval = XLV_MGR_STATUS_FAIL;
					goto error;
				}
			} else {
				/*
				 * subvolume type is a string
				 */
				if (!strcmp(sv_part, "log"))
					sv_type = XLV_SUBVOL_TYPE_LOG;
				else if (!strcmp(sv_part, "data"))
					sv_type = XLV_SUBVOL_TYPE_DATA;
				else if (!strcmp(sv_part, "rt"))
					sv_type = XLV_SUBVOL_TYPE_RT;
				else {
					retval = XLV_MGR_STATUS_FAIL;
					goto error;
				}
			}
		}
		if (plex_part) {
			mark = strlen(plex_part);
			while (mark > 0) {
				mark--;
				if (!isdigit((int)plex_part[mark])) {
					retval = XLV_MGR_STATUS_FAIL;
					goto error;
				}
			}
			plex_no = atoi(plex_part);
		}
		if (ve_part) {
			mark = strlen(ve_part);
			while (mark > 0) {
				mark--;
				if (!isdigit((int)ve_part[mark])) {
					retval = XLV_MGR_STATUS_FAIL;
					goto error;
				}
			}
			ve_no = atoi(ve_part);
		}
	}

	/*
	 * Find the object and fill the cursor.
	 * If the object is unknown then the oref field is NULL.
	 */
	cursor->oref = find_object_in_table(objname, obj_type, NULL);
	cursor->nodename = (nodename) ? strdup(nodename) : NULL ;
	cursor->objname = strdup(objname);
	cursor->type = type;				/* XXX */
	cursor->sv_type = sv_type;
	if (sv_type != -1 && cursor->oref) {
		xlv_tab_vol_entry_t *volp = XLV_OREF_VOL(cursor->oref);

		if (XLV_SUBVOL_TYPE_DATA == sv_type)
			XLV_OREF_SUBVOL(cursor->oref) = volp->data_subvol;
		else if (XLV_SUBVOL_TYPE_LOG == sv_type)
			XLV_OREF_SUBVOL(cursor->oref) = volp->log_subvol;
		else /* XLV_SUBVOL_TYPE_RT == sv_type */
			XLV_OREF_SUBVOL(cursor->oref) = volp->rt_subvol;
	}
	/*
	 * XXX Should XLV_OREF_PLEX(cursor->oref) be set?
	 * XXX Should XLV_OREF_SET_VOL_ELMNT(cursor->oref) be set?
	 */
	cursor->plex_no = plex_no;
	cursor->ve_no = ve_no;

	/*FALLTHROUGH*/
error:
	return (retval);

#undef	MAX_NAME_PARTS
} /* end of parse_name() */
