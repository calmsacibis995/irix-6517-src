/**************************************************************************
 *                                                                        *
 *            Copyright (C) 1993-1994, Silicon Graphics, Inc.             *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.15 $"
/*
 * Set the state of an XLV volume.
 */
#include <bstring.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <sys/debug.h>
#include <sys/syssgi.h>
#include <sys/sysmacros.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/dvh.h>
#include <sys/stat.h>
#include <sys/xlv_vh.h>
#include <xlv_oref.h>
#include <xlv_lab.h>
#include <xlv_cap.h>

#define	ME	"xlv_set_primary"

int vflag = 0;
int aflag = 1;

char *volname = NULL;



int
ve_set_state(xlv_oref_t *oref, void *arg)
{
        xlv_tab_vol_elmnt_t     *ve;

        ve = XLV_OREF_VOL_ELMNT(oref);

	ve->state = *((unsigned char *)arg);
	return 0;		/* continue */
}


void
set_state (xlv_oref_t		*plex_oref,
	   unsigned char	ve_state)
{
	xlv_for_each_ve_in_plex (plex_oref, (XLV_OREF_PF)ve_set_state,
				 (void *) &ve_state);
}

typedef struct {
        dev_t           device;                 /* input */
	boolean_t	done;			/* output */
} find_dev_t;



int
find_dev_func(xlv_oref_t *oref, find_dev_t *find_dev)
{
	xlv_tab_vol_elmnt_t	*ve;
	xlv_tab_plex_t		*plex;
	xlv_tab_subvol_t	*subvol;
	xlv_oref_t		plex_oref;
	unsigned		d, p;

	ve = XLV_OREF_VOL_ELMNT(oref);

	/*
	 * Note that we assume that either the root drive has not
	 * moved or that xlv_assemble has updated the labels.
	 */
	for (d=0; d < ve->grp_size; d++) {
		if (DP_DEV(ve->disk_parts[d]) == find_dev->device) {
			plex = XLV_OREF_PLEX(oref);
			if (plex == NULL)
				continue;
			subvol = XLV_OREF_SUBVOL(oref);
			if (subvol == NULL)
				continue;

			XLV_OREF_COPY(oref, &plex_oref);
			XLV_OREF_PLEX(&plex_oref) = plex;
			set_state (&plex_oref, XLV_VE_STATE_ACTIVE);

			for (p=0; p < XLV_MAX_PLEXES; p++) {
				if (! subvol->plex[p])
					continue;

				if (subvol->plex[p] != plex) {
					XLV_OREF_PLEX(&plex_oref) = 
						subvol->plex[p];
					set_state (&plex_oref,
						   XLV_VE_STATE_STALE);
				}
			}
			find_dev->done = B_TRUE;
			return 1;	/* stop search */
		}

	}

	return 0;		/* continue search */

}


void
make_primary (xlv_vh_entry_t	*vh_list,
	      xlv_oref_t	*oref_list,
	      dev_t		primary_device)
{
	xlv_oref_t		*oref;
	int			status;
	find_dev_t		find_dev;

	find_dev.device = primary_device;
	find_dev.done = B_FALSE;

	for (oref = oref_list; oref; oref = oref->next) {
		if (NULL == XLV_OREF_VOL(oref))
			continue;

		xlv_for_each_ve_in_vol (
			oref, (XLV_OREF_PF)find_dev_func, (void *)&find_dev);

		if (find_dev.done) {
			xlv_lab2_write_oref (
				&vh_list, oref, NULL,
				&status, XLV_LAB_WRITE_FULL);
			break;
		}

	}
} /* end of make_primary() */


void
read_disk_labels(
	xlv_vh_entry_t	**vh_list_p,
	xlv_oref_t	**oref_list_p)
{
	xlv_vh_entry_t	*vh_list = NULL;
	xlv_oref_t	*oref_list = NULL;
	int	status;

	xlv_lab2_create_vh_info_from_hinv (&vh_list, NULL, &status);

	if (vh_list) {
		/*
		 * Don't want incomplete volumes so filter them out.
		 */
		xlv_vh_list_to_oref (
			vh_list, &oref_list, NULL, NULL, 1, XLV_READ_ALL);
	}

	*vh_list_p = vh_list;
	*oref_list_p = oref_list;

} /* end of read_disk_labels() */


static void
usage(void)
{
	fprintf(stderr, "usage: %s device_name\n", ME);
	exit(1);
}


void
main (int argc, char **argv)
{
	xlv_vh_entry_t	*vh_list;
	xlv_oref_t	*oref_list;
	char		*device_name;
	struct stat	stat_buf;
	dev_t		dev;

	if (cap_envl(0, CAP_DEVICE_MGT, 0) == -1) {
		fprintf(stderr, "%s: must be started by super-user\n", argv[0]);
		exit(1);
	}

	vh_list = NULL;
	oref_list = NULL;

	if (argc < 2)
		usage();

	device_name = argv[1];

	/*
	 * Make sure device exists.
	 */
	if (stat (device_name, &stat_buf) < 0) {
		perror ("Cannot access device: ");
		exit (-1);
	}
	dev = stat_buf.st_rdev;


	/*
	 * Go through the list of volume elements, find the plexes
	 * that belong to the same volume as this disk part, and mark 
	 * all of them plexes stale except for the plex that contains
	 * this disk part. The plex that contains this disk part 
	 * becomes clean.
	 */

	read_disk_labels(&vh_list, &oref_list);

	if (0 == vh_list) {
		exit(1);
	}

	make_primary(vh_list, oref_list, dev);

	exit(0);
}
