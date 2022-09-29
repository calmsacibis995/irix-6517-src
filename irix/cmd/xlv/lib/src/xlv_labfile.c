/**************************************************************************
 *                                                                        *
 *                Copyright (C) 1994, Silicon Graphics, Inc.              *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.28 $"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <bstring.h>
#include <sys/debug.h>
#include <sys/param.h>
#include <sys/uuid.h>

#include <errno.h>

#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/dvh.h>
#include <sys/xlv_vh.h>
#include <xlv_oref.h>
#include <xlv_lab.h>
#include <xlv_utils.h>


/*
 * Regenerate all the objects described by the volume labels in
 * vh_list. Return them in a linked list of object ref's.
 *
 * If tab_vol_p or tab_sv_p is not NULL, the referenced pointer is
 * set the malloc volume or subvolume table.
 *
 * Incomplete volumes are removed from the table when <filter> is set.
 */
int
xlv_vh_list_to_oref (
	xlv_vh_entry_t		*vh_list,
	xlv_oref_t		**O_oref_list,
	xlv_tab_vol_t		**tab_vol_p,		/* OUT */
	xlv_tab_t		**tab_sv_p,		/* OUT */
	int			filter,
	int			flags)			/* for reading vh */
{
	xlv_tab_vol_t       	*xlv_tab_vol;
        xlv_tab_t           	*xlv_tab;
	xlv_vh_entry_t          *incomplete_vh_list;
	xlv_oref_t		*oref_list, *oref;
	int			v, status;
	int			max_v, max_sv;
	size_t			size_v, size_sv;
	int			read_flags;

#define	_SUBVOL_GROWTH	(10*XLV_SUBVOLS_PER_VOL)

	max_sv = get_max_kernel_locks();
	if (max_sv < 0) {
		max_sv = 0;
	} else {
		max_sv -= _SUBVOL_GROWTH;	/* bump it up later */
	}

do_again:
	max_sv += _SUBVOL_GROWTH;
	max_v = max_sv / XLV_SUBVOLS_PER_VOL;

	size_v = sizeof(xlv_tab_vol_t) + (max_v-1)*sizeof(xlv_tab_vol_entry_t);
	size_sv = sizeof(xlv_tab_t) + (max_sv-1)*sizeof(xlv_tab_subvol_t);

	xlv_tab_vol = (xlv_tab_vol_t *) malloc (size_v);
	bzero(xlv_tab_vol, (int)size_v);
	xlv_tab_vol->max_vols = max_v;

	xlv_tab = (xlv_tab_t *) malloc (size_sv);
	bzero(xlv_tab, (int)size_sv);
	xlv_tab->max_subvols = max_sv;

	read_flags = flags;
	if (0 == (read_flags & XLV_READ_ALL)) {
		/*
		 * got to read something, so default to reading all
		 */
		read_flags |= XLV_READ_ALL;
	}
	xlv_vh_to_xlv_tab (vh_list, xlv_tab_vol, xlv_tab,
		&incomplete_vh_list, read_flags, &status);

	if (status == E2BIG) {
		/*
		 * The tables are not big enough to hold all the
		 * volumes so go back and try again with bigger tables. 
		 */
		free (xlv_tab_vol);
		free (xlv_tab);
		goto do_again;
	} else if (status == EAGAIN) {
		free (xlv_tab_vol);
		free (xlv_tab);

		return status;
	}

	ASSERT (!status);

	if (filter)
		/*
		 * Remove incomplete objects from xlv_tab_vol
		 */
		xlv_filter_complete_vol(xlv_tab_vol, 0);

	/*
	 * Create an oref for each volume.
	 */
	oref_list = NULL;
	for (v = 0; v < xlv_tab_vol->num_vols; v++) {
		oref = (xlv_oref_t *) malloc (sizeof (xlv_oref_t));

		XLV_OREF_INIT(oref);
		XLV_OREF_SET_VOL(oref, &(xlv_tab_vol->vol[v]));
		/*
		 * Also record the nodename in the oref itself.
		 */
		strncpy(oref->nodename, xlv_tab_vol->vol[v].nodename,
			XLV_NODENAME_LEN);

		oref->next = oref_list;
		oref_list = oref;
	}

	/*
	 * Create an oref for each piece that is not part of a
	 * volume.
	 */
	xlv_vh_to_xlv_pieces (vh_list, &oref_list, &status, filter);

	*O_oref_list = oref_list;
	if (tab_vol_p)
		*tab_vol_p = xlv_tab_vol;
	if (tab_sv_p)
		*tab_sv_p = xlv_tab;

	return status;

} /* end of xlv_vh_list_to_oref() */
