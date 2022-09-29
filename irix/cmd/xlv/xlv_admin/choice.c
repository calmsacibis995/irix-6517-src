
/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.38 $"


#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bstring.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uuid.h>
#include <sys/capability.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/syssgi.h>
#include <xlv_oref.h>
#include <sys/sysmacros.h>
#include <xlv_error.h>
#include <xlv_utils.h>
#include <sys/debug.h>
#include <sys/xlv_vh.h>
#include <sys/xlv_attr.h>
#include <tcl.h>
#include <pfmt.h>
#include <sys/dvh.h>
#include <xlv_lab.h>
#include "xlv_make_int.h"
#include "xlv_admin.h"

static int remove_kernel(void);
static int xlv_del_labels(void);
static int xlv_print_kernel(void);
static int xlv_print_labels(void);

extern int xlv_lower_errno;
extern int Plex_Licensing;
extern int expert_mode;

/**********************************************************************
 *
 *	The following are not supported as on-line operations.
 *	printf(" 4.	Add a partition to an existing ve \n");
 *	printf(" 24.	Remove a partition from an existing ve \n");
 *
 ***********************************************************************
 */

void
do_begin(void)
{
	pfmt(stderr, MM_NOSTD,  
		":2:**************** XLV Administration Menu **********\n");
	pfmt(stdout, MM_NOSTD, 
		":3:................ Add Existing Selections...........\n");
	pfmt(stdout, MM_NOSTD, 
		":6:1.	Add a ve to an existing plex. \n");
	pfmt(stdout, MM_NOSTD, 
		"::2.	Add a ve at the END of an existing plex. \n");
	if (Plex_Licensing) {
		pfmt(stdout, MM_NOSTD,
		":5:3.	Add a plex to an existing volume. \n");
	}
	pfmt(stdout, MM_NOSTD, 
		":7:................ Detach Selections................\n");
	pfmt(stdout, MM_NOSTD, 
		":10:11.	Detach a ve from an existing plex. \n");
	pfmt(stdout, MM_NOSTD, 
		":9:12.	Detach a plex from an existing volume. \n");
	pfmt(stdout, MM_NOSTD, 
		":11:................ Remove Selections................\n");
	pfmt(stdout, MM_NOSTD, 
		":14:21.	Remove a ve from an existing plex. \n");
	if (Plex_Licensing) {
	   pfmt(stdout, MM_NOSTD, 
		":13:22.	Remove a plex from an existing volume. \n");
	}
	pfmt(stdout, MM_NOSTD, 
		":15:................ Delete Selections................\n");
	pfmt(stdout, MM_NOSTD, 
		":16:31.	Delete an object. \n");
	pfmt(stdout, MM_NOSTD, 
		"::32.	Delete all XLV disk labels. \n");

#if 0
	pfmt(stdout, MM_NOSTD, 
		"::35.	Delete kernel table \n");
	pfmt(stdout, MM_NOSTD, 
		"::36.	Delete all disk labels AND the kernel table\n");
#endif
	pfmt(stdout, MM_NOSTD, 
		":19:................ Show Selections................\n");
	pfmt(stdout, MM_NOSTD, 
		":20:41.	Show object by name and type, only. \n");
	pfmt(stdout, MM_NOSTD, 
		":22:42.	Show information for an object. \n");
	if (expert_mode) {
		pfmt(stdout, MM_NOSTD, 
		":21:43.	Show information for all objects. \n");
		pfmt(stdout, MM_NOSTD, 
		"::44.	Show disk labels for XLV objects. \n");
		pfmt(stdout, MM_NOSTD, 
		"::45.	Show kernel table for XLV objects. \n");
	}
#if 0
	pfmt(stdout, MM_NOSTD, 
		":23:46.	Show blockmap for a volume. \n");
#endif

	pfmt(stdout, MM_NOSTD, 
		"::................ Set Selections................\n");
	pfmt(stdout, MM_NOSTD, 
		"::51.	Set nodename for an object. \n");
	pfmt(stdout, MM_NOSTD, 
		":28:................ Exit ................\n");
	pfmt(stdout, MM_NOSTD, 
		":29:99.	Exit \n");
}


int
do_choice(int choice, char *name)
{
	int 			rval;
	char			yval[20];

	rval = errno = 0;

	switch (choice) {
		case 1:
			rval = xlv_e_add(CHOICE_VE, name);
			break;

		case 2:
			rval = xlv_e_add(CHOICE_VE_PLEX_END, name);
			break;

		case 3:
			rval = xlv_e_add(CHOICE_PLEX, name);
			break;

		case 11:
			rval = xlv_dettach(CHOICE_VE, name);
			break;

		case 12:
			rval = xlv_dettach(CHOICE_PLEX, name);
			break;

		case 21:
			rval = xlv_remove(CHOICE_VE, name);
			break;

		case 22:
			rval = xlv_remove(CHOICE_PLEX, name);
			break;

		case 31:
			rval = xlv_delete(name);
			break;

		case 32:	/* XXXjleong "Yes" should be case insensitive */
			get_val (yval, VAL_DELETE_LABELS);
			if (strcmp(STR_YES, yval) != 0) {
				if (strcasecmp("Y",yval) == 0) {
					pfmt(stdout, MM_NOSTD, 
	":: Please enter the full word 'yes' to confirm this operation. \n");
				} else {
					pfmt(stdout, MM_NOSTD, 
	":: XLV Disk labels were not removed. \n");
				}
				return(TCL_OK);
			}
			rval = xlv_del_labels();
			break;

		case 35:
			rval = remove_kernel();
			break;

		case 36:
			rval = xlv_del_labels();
			rval = remove_kernel();
			break;
		case 41:
			rval = xlv_prnt_list(FMT_SHORT);
			break;

		case 42:
			rval = xlv_prnt_name(name);
			break;

		case 43:
			rval = xlv_prnt_list(FMT_LONG);
			break;

		case 44:
			rval = xlv_print_labels();
			break;

		case 45:
			rval = xlv_print_kernel();
			break;

		case 46:
			rval = xlv_prnt_blockmap(name);
			break;

		case 51:
			rval = set_nodename(name);
			break;

#ifdef JLEONG
		case 52:	/* XXXjleong set volume element state */
			rval = set_ve_state(name);
			break;
#endif

		case 99:
			exit(0);
			break;

		default:
			pfmt(stdout, MM_NOSTD, 
			     ":32: Invalid choice '%d'; please try again. \n",
			     choice);
	}

	return(rval);

} /* end of do_choice() */


static int
xlv_del_labels(void)
{

	xlv_vh_entry_t	*vh, *vhlist;
	int		status;

	vh = vhlist = NULL;
	status = -1;

	xlv_lab2_create_vh_info_from_hinv (&vhlist, NULL, &status);
	if (vhlist) {
		for (vh = vhlist; vh != NULL; vh = vh->next) {
			status = xlv_delete_xlv_label(vh, vhlist);
			printf("Deleted XLV label on %s ... %s\n",
				vh->vh_pathname,
				(status == 0) ? "done" : "failed");
		}

		xlv_lab2_destroy_vh_info(&vhlist, &status);

	} else {
		printf("XLV labels not found on any disks.\n");
	}

	return(0);
}


static int
xlv_print_labels(void)
{
	xlv_vh_entry_t	*vh, *vhlist;
	int		status;

	vh = vhlist = NULL;
	status = -1;

	xlv_lab2_create_vh_info_from_hinv (&vhlist, NULL, &status);

	if (vhlist) {
		for (vh = vhlist; vh != NULL; vh = vh->next) {
			xlv_print_vh (vh, XLV_PRINT_BASIC, 1);
		}
		xlv_lab2_destroy_vh_info(&vhlist, &status);

	} else {
		printf("XLV labels not found on any disks.\n");
	}

	return(0);
}

static int
xlv_print_kernel(void)
{
	xlv_tab_subvol_t	*svp;
	xlv_attr_req_t		req;
	int			status;
	int			count;

	/*
	 * First we need to get a XLV cursor.
	 */
	if (0 > syssgi(SGI_XLV_ATTR_CURSOR, &cursor)) {
		printf("Failed to get a XLV cursor\n");
		return(1);
	}

	if (NULL == (svp = get_subvol_space())) {
		printf("Cannot malloc a subvolume entry.\n");
		return(-1);
	}

	req.attr = XLV_ATTR_SUBVOL;
	req.ar_svp = svp;

	status = 0;
	count = 0;
	while (status == 0) {
		status = syssgi(SGI_XLV_ATTR_GET, &cursor, &req);
		if (status < 0) {
			if (ENFILE == oserror())
				/* end of enumeration */
				break;
			perror("syssgi(SGI_XLV_ATTR_GET) failed.");
		} else {
			if (count) printf("\n");
			xlv_tab_subvol_print(req.ar_svp, 0);
			count++;
		}
	}
	
	free_subvol_space(svp);

	printf("\nThere are %d subvolumes.\n", count);

	return (0);
}


static int
remove_kernel(void)
{
	int	rval;
	cap_t	ocap;
	cap_value_t cap_device_mgt = CAP_DEVICE_MGT;

	ocap = cap_acquire(1, &cap_device_mgt);
	rval = syssgi (SGI_XLV_SET_TAB,
		       (xlv_tab_vol_t *)NULL, (xlv_tab_t *)NULL);
	cap_surrender(ocap);
	if (rval < 0) {
#ifdef DEBUG
		printf("DBG: syssgi(XLV_SET_TAB) failed\n");
		printf("DBG: rval=%d errno=%d xlv_lower_errno=%d\n",
			rval,errno,xlv_lower_errno);
#endif
		pfmt(stdout, MM_NOSTD, ":: Failed to delete kernel table \n");
	} else {
		pfmt(stdout, MM_NOSTD, ":: Deleted kernel table \n");
	}

	return(rval);
}
