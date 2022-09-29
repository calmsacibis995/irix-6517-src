/*
 * xlv_cmds.c
 *
 *          Module to generate xlv_make(1m) commands required to
 *          generate a given object.
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Revision: 1.6 $"

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/uuid.h>
#include <stdio.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <xlv_oref.h>
#include <xlv_utils.h>


static int _xlv_make_ve (FILE *sink, xlv_tab_vol_elmnt_t *vep, char *name);
static int _xlv_make_plex (FILE *sink, xlv_tab_plex_t *plexp, char *name);
static int _xlv_make_subvol (FILE *sink, xlv_tab_subvol_t *svp);


/*
 * Output takes one of two forms:
 *	ve [NAME] -force -start SSS -stripe -stripe_unit NNN disk_part_name
 *	ve [NAME] -force -start SSS disk_part_name
 *
 * Returns 0 on success and 1 on failure.
 */
static int
_xlv_make_ve (FILE *sink, xlv_tab_vol_elmnt_t *vep, char *name)
{
	char	*ve_name;
	char	*d_pathname;
	int	d;

	ve_name = (name == NULL) ? " " : name;

	if (vep->state == XLV_VE_STATE_INCOMPLETE) {
		fprintf (sink, "ve %s *** INCOMPLETE ***\n", ve_name);
		return (1);
	}

	fprintf (sink, "ve %s -force -start %llu",
		ve_name, vep->start_block_no);

	if (vep->type == XLV_VE_TYPE_STRIPE) {
		fprintf (sink, " -stripe -stripe_unit %d",
			vep->stripe_unit_size);
	}

	/*
	 * devtopath() must return a valid pathname for
	 * a "complete" volume element.
	 */
	for (d = 0; d < vep->grp_size; d++) {
		/*
		 * Using aliases for device pathnames.
		 */
		d_pathname = devtopath(DP_DEV(vep->disk_parts[d]), 1);
		fprintf (sink, " %s", d_pathname);
	}
	fprintf (sink, "\n");

	return (0);
	
} /* end of _xlv_make_ve() */


/*
 *	plex [NAME]
 *
 * Returns 0 on success and 1 on failure.
 */
static int
_xlv_make_plex (FILE *sink, xlv_tab_plex_t *plexp, char *name)
{
	int	i;
	int	retval = 0;

	if (name)
		fprintf (sink, "plex %s\n", name);
	else
		fprintf (sink, "plex\n");

	for (i = 0; i < plexp->num_vol_elmnts && !retval; i++) {
		retval = _xlv_make_ve(sink, &plexp->vol_elmnts[i], NULL);
	}

	return (retval);

} /* end of _xlv_make_plex() */


/*
 *	data | log | rt
 *
 * Returns 0 on success and 1 on failure.
 */
static int
_xlv_make_subvol (FILE *sink, xlv_tab_subvol_t *svp)
{
	char	*sv_type;
	int	p;
	int	retval = 0;

	switch (svp->subvol_type) {
	    case XLV_SUBVOL_TYPE_LOG:	sv_type = "log"; break;
	    case XLV_SUBVOL_TYPE_DATA:	sv_type = "data"; break;
	    case XLV_SUBVOL_TYPE_RT:	sv_type = "rt"; break;
	}
	fprintf (sink, "%s\n", sv_type);

	for (p = 0; p < XLV_MAX_PLEXES && !retval; p++) {
		if (svp->plex[p])
			retval = _xlv_make_plex(sink, svp->plex[p], NULL);
	}

	return (retval);

} /* end of _xlv_make_subvol() */


/*
 * Generate the xlv_make(1m) needed to produce the given object.
 *
 * Returns 0 on success and 1 on failure.
 */
int
xlv_oref_to_xlv_make_cmds (FILE *sink, xlv_oref_t *oref)
{
	 xlv_tab_vol_elmnt_t	*vep;
	 xlv_tab_plex_t		*plexp;
	 xlv_tab_subvol_t	*svp;
	 xlv_tab_vol_entry_t	*vp;
	 int			retval = 0;

	if (oref == NULL)
		return (1);

	/*
	 * First print out the top-level name and then all the
	 * volume elements.
	 */
        if (vp = XLV_OREF_VOL(oref)) {
		if (vp->name == NULL) {
			ASSERT (0);
			return (1);
		}
		if (vp->state == XLV_VOL_STATE_MISS_COMMON_PIECE
		    || vp->state == XLV_VOL_STATE_MISS_UNIQUE_PIECE) {
#ifdef DEBUG
			printf("DBG: vol %s is missing pieces.\n", vp->name);
#endif
			return (1);
		}
		fprintf (sink, "vol %s\n", vp->name);
		if (svp = vp->data_subvol) {
			retval = _xlv_make_subvol(sink, svp);
		}
		if (!retval && (svp = vp->log_subvol)) {
			retval = _xlv_make_subvol(sink, svp);
		}
		if (!retval && (svp = vp->rt_subvol)) {
			retval = _xlv_make_subvol(sink, svp);
		}
        }

        else if (plexp = XLV_OREF_PLEX(oref)) {
		if (plexp->name == NULL) { ASSERT (0); return (1); }
		retval = _xlv_make_plex(sink, plexp, plexp->name);
        }

        else if (vep = XLV_OREF_VOL_ELMNT(oref)) {
		if (vep->veu_name == NULL) { ASSERT (0); return (1); }
		retval = _xlv_make_ve (sink, vep, vep->veu_name);
	}

        else {
		ASSERT (0);     /* object must have a top-level name. */
		retval = 1;
	}

	fprintf (sink, "end\n");
	return (retval);

} /* end of xlv_oref_to_xlv_make_cmds() */
