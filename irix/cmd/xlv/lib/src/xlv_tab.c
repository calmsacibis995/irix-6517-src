/**************************************************************************
 *                                                                        *
 *             Copyright (C) 1993-1994, Silicon Graphics, Inc.            *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.56 $"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bstring.h>
#include <time.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/debug.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <xlv_oref.h>
#include <xlv_utils.h>


static void xlv_tab_plex_print (
	xlv_tab_plex_t *plex_entry, char *plex_name, int p_mode);
static void xlv_tab_ve_print (
	xlv_tab_vol_elmnt_t *ve, char *ve_name, int p_mode);

extern void printpath (dev_t dev, int alias);

extern xlv_block_map_t *xlv_tab_create_block_map(xlv_tab_subvol_t *sv);

static int _xlv_check_subvol (xlv_tab_subvol_t *svp);
static int _xlv_check_plex (xlv_tab_plex_t *pp, int good);
static int _xlv_check_ve (xlv_tab_vol_elmnt_t *vep, int good);


/*
 * Check the xlv_tab for completeness of each component
 * (volume element, plex, subvolume, volume).
 */

/*
 * Check for a complete and "good" volume element. A volume is complete
 * if and only if all configured disk parts are there. And a volume
 * element is good when it is usable (not in the offline state).
 */
static int
_xlv_check_ve(xlv_tab_vol_elmnt_t *vep, int good)
{
	int			i;
	xlv_tab_disk_part_t	*dp;
	dev_t			null_dev;
	uint_t			status;

	/*
	 * Make sure the volume element exists.
	 */
	ASSERT(vep);
	if (uuid_is_nil(&vep->uuid, &status)) {
		vep->state = XLV_VE_STATE_INCOMPLETE;
		return (-1);
	}

	/*
	 * "good" check: make sure the volume element is usable.
	 */
	if (good && vep->state == XLV_VE_STATE_OFFLINE) {
		/*
		 * This volume element has been taken offline and
		 * thus is not usable.
		 *
		 * XXXjleong How does a volume element go from the
		 * offline state to a "good" state?
		 */
		return(-1);
	}

	null_dev = makedev(0,0);

	for (i = 0; i < vep->grp_size; i++) {
		dp = &vep->disk_parts[i];
		if (DP_DEV(*dp) == null_dev || dp->part_size == 0) {
			/*
			 * A disk part does not exist so this
			 * volume element is incomplete.
			 *
			 * Note: Could have used the disk uuid.
			 */
			vep->state = XLV_VE_STATE_INCOMPLETE;
			return (-1);
		}
	}

	/*
	 * The volume element now has all of its disk parts
	 * so make a state transition.
	 */

	if (vep->state == XLV_VE_STATE_INCOMPLETE)
		vep->state = XLV_VE_STATE_STALE;

	return (0);
}

/*
 * Check the completeness of the plex by checking that each volume element
 * in the plex is okay.
 *
 * Note: There is a dependency that the table of volume elements is always
 * keep compact. i.e. there are no holes in the table. When this function
 * finds a hole in the table, it concludes that a volume element is missing.
 */
static int
_xlv_check_plex(xlv_tab_plex_t *pp, int good)
{
	int			i;
	int			error;
	xlv_tab_vol_elmnt_t	*vep;

	error = 0;

	if (pp == NULL)
		return (-1);		/* missing plex */

	/*
	 * Scan the complete volume element table and set its state.
	 * There should be no holes in the table.
	 */
	for (i = 0; i < pp->num_vol_elmnts; i++) {
		vep = &pp->vol_elmnts[i];
		if (0 > _xlv_check_ve(vep, good)) {
			error++;
		}
	}

	return ((error) ? -1 : 0);
}

/*
 * Determine if the subvolume covers a "complete" address space.
 *
 * If there is only a single plex, then it must map out
 * a complete address space. There can be holes if there
 * are more than one plex. However, some combination of
 * the plex parts (volume elements) must cover the complete
 * address space.
 */
static int
_xlv_check_subvol(xlv_tab_subvol_t *svp)
{
	int		plex_cnt;
	int		missing;
	int		state;

	if (svp->num_plexes == 0) {
		/*
		 * XXXjleong 6/16/94 Set vol state to NONE?
		 */
		state = XLV_VOL_STATE_MISS_UNIQUE_PIECE;
		goto set_state;
	}

	for (plex_cnt=0, missing=0; plex_cnt < svp->num_plexes; plex_cnt++) {
		if ((svp->plex[plex_cnt] == NULL) ||
		    (_xlv_check_plex(svp->plex[plex_cnt], 0) < 0))
			missing++;	/* incomplete volume element */
	}

	if (missing == 0) {
		/*
		 * Everything is here but need to make sure that
		 * there are no holes in the address space.
		 */
		svp->subvol_size = xlv_tab_subvol_size(svp);
		state = (xlv_tab_create_block_map(svp)) ?
				XLV_VOL_STATE_COMPLETE :	 /* no holes */
				XLV_VOL_STATE_ALL_PIECES_BUT_HOLEY;

	} else if (plex_cnt == 1) {
		/*
		 * Something is missing and there are no mirrors.
		 */
		state = XLV_VOL_STATE_MISS_UNIQUE_PIECE;

	} else {
		/*
		 * The missing pieces may be mirrored.
		 */
		xlv_block_map_t *map;
		int		depth;

		depth = xlv_tab_subvol_depth(svp);

		if (depth != svp->subvol_depth) {
			/*
			 * At least one ve row must be missing.
			 *
			 * If subvol_depth == 0, then this is an
			 * old-style label, we can't say anything about it.
			 * Otherwise, go ahead with the check.
			 */
			ASSERT((svp->subvol_depth == 0) ||
			       (svp->subvol_depth > depth));
			state = XLV_VOL_STATE_MISS_UNIQUE_PIECE;
		} else {
			/*
			 * All ve rows are represented, but are they
			 * okay? Create a block map to check for holes
			 * in the address space.
			 */
			svp->subvol_size = xlv_tab_subvol_size(svp);

			map = xlv_tab_create_block_map(svp);
			state = (map) ? XLV_VOL_STATE_MISS_COMMON_PIECE :
					XLV_VOL_STATE_MISS_UNIQUE_PIECE;
		}
	}

	/*
	 * NOTE: There is a dependency on the order of volume states.
	 * The higher state value supercedes the lower one.
	 *
	 * For example, if the log subvolume is XLV_VOL_STATE_COMPLETE(1)
	 * and the data subvolume is XLV_VOL_STATE_MISS_UNIQUE_PIECE(3),
	 * then the volume state is XLV_VOL_STATE_MISS_UNIQUE_PIECE(3).
	 */
ASSERT(XLV_VOL_STATE_COMPLETE < XLV_VOL_STATE_MISS_COMMON_PIECE);
ASSERT(XLV_VOL_STATE_MISS_COMMON_PIECE < XLV_VOL_STATE_ALL_PIECES_BUT_HOLEY);
ASSERT(XLV_VOL_STATE_ALL_PIECES_BUT_HOLEY < XLV_VOL_STATE_MISS_UNIQUE_PIECE);

set_state:
	if (state > svp->vol_p->state)
		svp->vol_p->state = state;

	return ( (state == XLV_VOL_STATE_COMPLETE) ? 0 : -1 );
}


/*
 * Check the XLV subvolume table for invalid entries. An entry is
 * invalid if it has any missing pieces.
 *
 * Return the number of invalid entries.
 */
/* ARGSUSED */
int
xlv_check_xlv_tab (xlv_tab_vol_t	*tab_vol,
		   xlv_tab_t		*xlv_tab)
{
	int			found;
	int			error;
	xlv_tab_subvol_t	*svp, *last;

	found = error = 0;
	last = &xlv_tab->subvolume[xlv_tab->max_subvols];
	svp = &xlv_tab->subvolume[0];

	for ( ; (svp < last) && (found < xlv_tab->num_subvols); svp++) {
		if (! XLV_SUBVOL_EXISTS(svp))
			continue;
		if (0 > _xlv_check_subvol(svp))
			error++;
		found++;
	}

	return (error);
}

/*
 * Check the standalone list for invalid entries. An entry is
 * invalid if it has any missing pieces.
 *
 * Note that we don't check if the object is "good" in the sense
 * that the object is not "offline." We are only concern about
 * whether or not all the defined pieces are present.
 *
 * If <filter> is non-zero, any object that is missing pieces is removed
 *
 * Return the number of invalid entries.
 */
int
xlv_check_oref_pieces (xlv_oref_t **oref_pieces, int filter)
{
	xlv_oref_t		*oref_p;
	xlv_oref_t		*prev_oref_p = NULL;
	xlv_tab_plex_t		*pp;
	xlv_tab_vol_elmnt_t	*vep;

	char	*name;
	char	*type;
	int	removeit = 0;
	int	errors = 0;

	prev_oref_p = *oref_pieces;

	/*
	 * Run through the standalone list
	 */
	for(oref_p = *oref_pieces; oref_p != NULL; oref_p = oref_p->next) {
		if (oref_p->plex != NULL) {
			/*
			 * Standalone plex
			 */
			pp = oref_p->plex;
			if (0 > _xlv_check_plex(pp, 0)) {
				name = oref_p->plex->name;
				type = "Plex";
				removeit = filter;
			}

		} else {
			/*
			 * Standalone ve
			 */
			vep = oref_p->vol_elmnt;
			if (0 > _xlv_check_ve(vep, 0)) {
				name = oref_p->vol_elmnt->ve_u.name;
				type = "Ve";
				removeit = filter;
			}
		}

		if (removeit) {
			/*
			 * Remove the incomplete standalone object from
			 * the linked list. Note that the name and type
			 * variable should already be set from above.
			 */
			fprintf (stderr,
			"Removing XLV Standalone %s \"%s\": missing pieces\n",
				 type, name);
			if (prev_oref_p == *oref_pieces) {
				*oref_pieces = (*oref_pieces)->next;
				prev_oref_p = *oref_pieces;
			} else {
				prev_oref_p->next = oref_p->next;
			}
			errors++;
			removeit = 0;
		} else {
			prev_oref_p = oref_p;
		}
	}

	return (errors);
} /* end of xlv_check_oref_pieces() */


/*
 * Free all malloc'ed items and clear the subvolume structure.
 */
int
xlv_clear_subvol(xlv_tab_subvol_t *svp)
{
	int	i;

	for (i = 0; i < XLV_MAX_PLEXES; i++ ) {
		if (svp->plex[i])
			free(svp->plex[i]);
	}

	if (svp->block_map)
		free(svp->block_map);

	bzero(svp, sizeof(xlv_tab_subvol_t));

	return (0);
}


/*
 * Return the depth of the subvolume in number of volume elements.
 * 
 * Note: Dependency on volume elements across the row being the same size.
 * 	 Don't check for non-offline volume elements.
 */

typedef struct {
	__int64_t	start_blkno;
} ve_stack_entry_t;

int
xlv_tab_subvol_depth (xlv_tab_subvol_t *svp)
{
	ve_stack_entry_t	*ve_stack;
	xlv_tab_plex_t		*plex;
	int			p;
	unsigned		st;

	__int64_t	ve_start_blkno, stk_start_blkno;
	int	ve_idx;
	int	stk_idx;
	int	stk_size;
	int	stk_depth;

	stk_size = sizeof(ve_stack_entry_t) * XLV_MAX_VE_PER_PLEX;
	ve_stack = (ve_stack_entry_t *)  malloc (stk_size);
	bzero (ve_stack, stk_size);

	/*
	 * Go through all the plexes. Insert an entry for every
	 * volume address range that's not already in ve_stack.
	 */
	stk_depth = 0;
	for (p = 0; p < XLV_MAX_PLEXES; p++) {
		plex = svp->plex[p];
		if ((plex == NULL) || (uuid_is_nil(&plex->uuid, &st)))
			continue;		/* missing plex */
		
		/*
		 * for each volume element in this plex...
		 */
		stk_idx = 0;
		for (ve_idx = 0; ve_idx < plex->num_vol_elmnts; ve_idx++) {
			/*
			 * Check for the existence of a volume element.
			 */
			if (uuid_is_nil(&plex->vol_elmnts[ve_idx].uuid, &st))
				continue;

			ve_start_blkno =
				plex->vol_elmnts[ve_idx].start_block_no;

			if (stk_depth == 0) {
				/*
				 * First valid volume element that we see.
				 * For the rest of this plex, we'll be
				 * in append mode (stk_depth == stk_idx).
				 */
				ve_stack[0].start_blkno = ve_start_blkno;
				stk_depth = stk_idx = 1;
				continue;	/* next ve */
			}

			/*
			 * Find ve's corresponding ve_stack entry.
			 * Start the search from where the previous
			 * ve in this plex left off.
			 */
			for ( ; stk_idx < stk_depth; stk_idx++) {
				stk_start_blkno = ve_stack[stk_idx].start_blkno;
				if (ve_start_blkno == stk_start_blkno)
					goto next_ve_iteration;
				if (ve_start_blkno < stk_start_blkno)
					break;	/* insert this ve */
			}

			if (stk_idx == stk_depth) {
				/*
				 * Append mode. Add this entry the end
				 * of stack. And stay in append mode since
				 * all following ve's in this plex should
				 * be after this stack entry.
				 */
				ve_stack[stk_idx].start_blkno = ve_start_blkno;
				stk_idx++;	/* update stk_depth later */

			} else {
				/*
				 * Insert mode. Move all subsequent stack
				 * entries up one.
				 */
				int	i;
				for (i = stk_depth; i > stk_idx; i--) {
					ve_stack[i] = ve_stack[i-1];
				}
				ve_stack[stk_idx].start_blkno = ve_start_blkno;
			}
			stk_depth++;
next_ve_iteration:
			/* NULL - a NOP  */;

		} /* for each ve */

	} /* for each plex */

	return (stk_depth);

} /* xlv_tab_subvol_depth() */


/*
 * Return the size of the volume in blocks.
 * If there are multiple plexes, return the largest size.
 */
__int64_t
xlv_tab_subvol_size (xlv_tab_subvol_t *subvol_entry)
{
	xlv_tab_plex_t	*plex;
	int		p;
	int		index;
	__int64_t	last_block = 0;

	/*
	 * This code assumes that all the volume elements across a
	 * "row" are the same size and volume elements in a plex
	 * are in increasing block number order. Therefore, the last
	 * volume element of each plex potentially has last block
	 * address.
	 */
	for (p = 0; p < subvol_entry->num_plexes; p++) {
		plex = subvol_entry->plex[p];

		if (plex == NULL) continue;

		/*
		 * XXXjleong Need check for good plex?
		 */
		if (! (plex->num_vol_elmnts > 0))
			continue;
		index = plex->num_vol_elmnts - 1;
		if (plex->vol_elmnts[index].end_block_no > last_block) {
			last_block = plex->vol_elmnts[index].end_block_no;
		}
	}

	return (last_block+1);
}


static char *vol_state_txt[] = {
	"empty",
	"complete",
	"missing common piece",
	"has holes",
	"missing unique piece",
	"missing required subvolume"
};

char *
xlv_vol_state_str(unsigned int state)
{
	static char	unknown_vol_state[50];
	char		*state_string;

	if (state > XLV_VOL_STATE_MISS_REQUIRED_SUBVOLUME) {
		sprintf(unknown_vol_state, "unknown state %d", state);
		state_string = unknown_vol_state;
	} else {
		state_string = vol_state_txt[state];
	}

	return (state_string);	
}


void
xlv_tab_vol_print (xlv_tab_vol_entry_t *vol_entry, int p_mode)
{
	uint_t	status;
	char	*uuid_strp;

	fprintf (stdout, "\nVOL %.*s", sizeof(xlv_name_t), vol_entry->name);
	if (p_mode & XLV_PRINT_UUID) {
		uuid_to_string (&(vol_entry->uuid), &uuid_strp, &status);
		fprintf (stdout, "\t%s", uuid_strp);
		free (uuid_strp);
	}
	fprintf (stdout, "\tflags=0x%x, [%s]\t\t(node=%.*s)\n",
		 vol_entry->flags, vol_state_txt[vol_entry->state],
		 sizeof(xlv_name_t),
		 (vol_entry->nodename ? vol_entry->nodename : "NULL"));

	xlv_tab_subvol_print (vol_entry->log_subvol, p_mode);
	xlv_tab_subvol_print (vol_entry->data_subvol, p_mode);
	xlv_tab_subvol_print (vol_entry->rt_subvol, p_mode);
}


/* ARGSUSED */
void
xlv_tab_print ( xlv_tab_vol_t	*xlv_tab_vol,
		xlv_tab_t	*xlv_tab,
		int		p_mode)
{
	xlv_tab_vol_entry_t	*vol;
	int			v;

	for (v = 0, vol = xlv_tab_vol->vol; v < xlv_tab_vol->num_vols; 
	     v++, vol++) {
		xlv_tab_vol_print (vol, p_mode);
	}
}


static char *subvol_type_txt[] = {
	"",	/* 0-based. */
	"LOG",
	"DATA",
	"RT",
	"rsvd",
	"unknown",
};

static char *subvol_flag_txt[] = {
	"Char_IO",
	"FS_IO",
	"Block_IO",
	"Swap_IO",
	"Higher_driver",
	"Offline",
	"Copy_in_progress",
	"In_retry",
	"B8",
	"B9",
	"B10",
	"B11",
	"B12",
	"B13",
	"B14",
	"B15",
	/* Bits saved across reboots */
	"B16",
	"B17",
	"B18",
	"B19",
	"B20",
	"B21",
	"B22",
	"B23",
	"B24",
	"B25",
	"B26",
	"B27",
	"B28",
	"B29",
	"B30",
	"No_err_retry"
};

static char *subvol_openflag_txt[] = {
	"FREAD",		/* 0x1 */
	"FWRITE",		/* 0x2 */
	"FNDELAY",		/* 0x4 */
	"FAPPEND",		/* 0x8 */
	"FSYNC",		/* 0x10 */
	"FDSYNC",		/* 0x20 */
	"FRSYNC",		/* 0x40 */
	"FNONBLOCK",		/* 0x80 */
/* open-only modes */
	"FCREAT",		/* 0x100 */
	"FTRUNC",		/* 0x200 */
	"FEXCL",		/* 0x400 */
	"FNOCTTY",		/* 0x800 */
/* */
	"FASYNC",		/* 0x1000 */
	"FLARGEFILE",		/* 0x2000 */
	"B14",			/* 0x4000 */
	"FDIRECT",		/* 0x8000 */
	"B16",
	"B17",
	"B18",
	"B19",
	"B20",
	"B21",
	"B22",
	"B23",
	"B24",
	"B25",
	"B26",
	"B27",
	"B28",
	"B29",
	"B30",
	"B31"
};


/*
 * Print flag value and the corresponding bit meanings.
 * "name" is the flag name.
 * "flag" is the flag value.
 * "strings" is the string representation of which the meaning of each bit.
 */
void
xlv_print_flag(char *name, int flag, char **strings) 
{
	char	buf[512], *msg;
	uint	mask;
	uint	bit, bit_value;
	uint	count;

	msg = buf;
	buf[0] = 0;
	mask = flag;

	for (count=0, bit=0; mask > 0; bit++) {
		bit_value = 1<<bit;
		if (bit_value & mask) {
			if (count)
				msg += sprintf(msg, "|");
			msg += sprintf(msg, "%s", strings[bit]);
			mask &= ~bit_value;
			count++;
		}
	}

	fprintf (stdout, "\t%s=0x%x(%s)", name, flag, buf);
}


void
xlv_tab_subvol_print (xlv_tab_subvol_t *subvol_entry, int p_mode)
{
	unsigned	st;
	char		*uuid_strp;
	int		p;
	char		buf[4];

	if (subvol_entry == NULL)
		return;

	if (uuid_is_nil(&subvol_entry->uuid, &st))
		return;

	fprintf (stdout, "%s", subvol_type_txt[subvol_entry->subvol_type]);
	if (p_mode & XLV_PRINT_UUID) {
		uuid_to_string (&(subvol_entry->uuid), &uuid_strp, &st);
		fprintf (stdout, "\t%s", uuid_strp);
		free (uuid_strp);
	}
	xlv_print_flag ("flags", subvol_entry->flags, subvol_flag_txt);
	xlv_print_flag ("open_flag", subvol_entry->open_flag,
			subvol_openflag_txt);
	fprintf (stdout, "\tdevice=(%d, %d)",
		 major(subvol_entry->dev), minor(subvol_entry->dev));

	if (subvol_entry->flags & XLV_SUBVOL_PLEX_CPY_IN_PROGRESS) {
		/*
		 * The start and stop fields are daddr_t's. Be careful
		 * about printing their values.
		 */
		__int64_t start = subvol_entry->critical_region.start_blkno;
		__int64_t stop  = subvol_entry->critical_region.end_blkno;
		__int64_t done;

		if (stop == 0) {
			strcpy(buf, "??");
		} else {
			done = (start * 100) / subvol_entry->subvol_size;
			sprintf(buf, "%2lld", done);
		}
		fprintf(stdout,
			"\trevive[start=%lld, stop=%lld, ~%s%% revived]",
			start, stop, buf);
	}

	fprintf(stdout, "\n");

	for (p = 0; p < subvol_entry->num_plexes; p++) {
		sprintf(buf, "%d", p);
		xlv_tab_plex_print (subvol_entry->plex[p], buf, p_mode);
	}

}


static void
xlv_tab_plex_print (xlv_tab_plex_t *plex_entry, char *plex_name, int p_mode)
{
	int		ve;
	unsigned	st;
	char		*uuid_strp;
	char		buf[8];		/* max is "128" */
	char		*name;

	name = (plex_name) ? plex_name : plex_entry->name;

	if ((plex_entry == NULL) || uuid_is_nil(&(plex_entry->uuid), &st)) {
		fprintf(stdout, "PLEX %.*s <null>\n", sizeof(xlv_name_t), name);
		return;
	}

	fprintf(stdout, "PLEX %.*s", sizeof(xlv_name_t), name);

	if (p_mode & XLV_PRINT_UUID) {
		uuid_to_string (&(plex_entry->uuid), &uuid_strp, &st);
		fprintf (stdout, "\t%s", uuid_strp);
		free (uuid_strp);
	}
	fprintf (stdout, "\tflags=0x%x\n", plex_entry->flags);

	for (ve = 0; ve < plex_entry->num_vol_elmnts; ve++) {
		sprintf(buf, "%d", ve);
		xlv_tab_ve_print (&(plex_entry->vol_elmnts[ve]), buf, p_mode);
	}
}


static char *ve_state_txt[] = {
        "empty",
        "clean",
        "active",
        "stale",
	"offline",
	"incomplete"
};

char *
xlv_ve_state_str(unsigned int state)
{
	static char	unknown_ve_state[50];
	char		*state_string;

	if (state > XLV_VE_STATE_INCOMPLETE) {
		sprintf(unknown_ve_state, "unknown state %d", state);
		state_string = unknown_ve_state;
	} else {
		state_string = ve_state_txt[state];
	}

	return (state_string);	
}


static void
xlv_tab_ve_print (xlv_tab_vol_elmnt_t *ve, char *ve_name, int p_mode)
{
	int		d;
	unsigned	st;
	char		*uuid_strp;
	char		*name;

	name = (ve_name) ? ve_name : ve->veu_name;

	if ((ve == NULL) || uuid_is_nil(&(ve->uuid), &st)) {
		fprintf (stdout, "VE %.*s <null>\n", sizeof(xlv_name_t), name);
		return;
	}

	printf ("VE %.*s", sizeof(xlv_name_t), name);

	if (p_mode & XLV_PRINT_UUID) {
		uuid_to_string (&(ve->uuid), &uuid_strp, &st);
		printf ("\t%s", uuid_strp);
		free (uuid_strp);
	}
	if (ve_name && (p_mode & XLV_PRINT_TIME)) {
		/* volume or plex */
		char	*time_str;
		time_str = asctime(localtime(&ve->veu_timestamp));
		printf ("\t[%s]\ttime: %s",	/* "\n" embbeded in time_str */
			ve_state_txt[ve->state], time_str);
	} else {
		/* standalone volume element */
		printf ("\t[%s]\n", ve_state_txt[ve->state]);
	}

	printf ("\tstart=%llu, end=%llu, (%s)grp_size=%d",
		ve->start_block_no, ve->end_block_no,
		(ve->type == XLV_VE_TYPE_CONCAT) ? "cat" : "stripe",
		ve->grp_size);
	if (ve->type == XLV_VE_TYPE_CONCAT)
		printf ("\n");
	else
		printf (", stripe_unit_size=%d\n", ve->stripe_unit_size);

	for (d = 0; d < ve->grp_size; d++) {
		xlv_tab_disk_part_t *dp = &ve->disk_parts[d];

		printf ("\t");

		if (uuid_is_nil(&dp->uuid, &st)) {
			printf("NULL\n");	/* missing disk part */
			continue;
		}

		printpath (DP_DEV(*dp), 1);	/* use pathname alias */

		printf (" (%d blks)", dp->part_size);

		if (p_mode & XLV_PRINT_UUID) {
			uuid_to_string (&dp->uuid, &uuid_strp, &st);
			printf ("\t%s\n", uuid_strp);
			free (uuid_strp);
		} else {
			printf ("\n");
		}
		if (dp->n_paths > 1) {
			int	inx;

			printf("\t    { ");
			for (inx = 0; inx < dp->n_paths; inx ++) {
				if (inx != dp->active_path) {
					printpath(dp->dev[inx], 1);
					printf(" ");
				}
				else {
					printf("\" ");
				}
			}
			printf("}\n");
		}
	}

} /* end of xlv_tab_ve_print() */


static xlv_oref_t	null_oref;	/* initialized to all zero */

int
xlv_oref_is_null (xlv_oref_t *oref)
{
	return (! bcmp (oref, &null_oref, sizeof(xlv_oref_t)));
}


static int
xlv__oref_resolve_ve (xlv_oref_t *oref, xlv_tab_vol_elmnt_t *ve)
{
	int			d;
	xlv_tab_disk_part_t	*dp;
	unsigned		st;

	XLV_OREF_SET_VOL_ELMNT (oref, ve);
	if (uuid_equal (&ve->uuid, &oref->uuid, &st))
		return (0);

	for (d=0, dp=ve->disk_parts; d <= ve->grp_size; d++, dp++) {
		if (uuid_equal (&dp->uuid, &oref->uuid, &st)) {
	    		XLV_OREF_SET_DISK_PART (oref, dp);
	    		return (0);
		}
	}
	return (-1);
}


static int
xlv__oref_resolve_plex (xlv_oref_t *oref, xlv_tab_plex_t *plex)
{
	int			v;
	xlv_tab_vol_elmnt_t	*ve;
	unsigned		st;

	if (! plex)
		return (-1);

	XLV_OREF_SET_PLEX (oref, plex);
	if (uuid_equal (&plex->uuid, &oref->uuid, &st))
		return (0);

	for (v=0, ve=plex->vol_elmnts; v <= plex->num_vol_elmnts; v++, ve++) {
		if (xlv__oref_resolve_ve (oref, ve) == 0)
	    		return (0);
	}
	return (-1);
}

static int
xlv__oref_resolve_subvol (xlv_oref_t *oref, xlv_tab_subvol_t *subvol_entry)
{
	int 		p;
	unsigned	st;
	
	if (subvol_entry == NULL) return (-1);
	
	XLV_OREF_SET_SUBVOL (oref, subvol_entry);
	if (uuid_equal (&subvol_entry->uuid, &oref->uuid, &st))
		return (0);
	
	for (p=0; p <= subvol_entry->num_plexes; p++) {
		if (xlv__oref_resolve_plex (oref, subvol_entry->plex[p]) == 0)
			return (0);
	}
	return (-1);
}

int
xlv_oref_resolve (xlv_oref_t *oref, xlv_tab_vol_entry_t *tab_vol_entry)
{
	unsigned	st;

        XLV_OREF_SET_VOL (oref, tab_vol_entry);
        if (uuid_equal (&tab_vol_entry->uuid, &oref->uuid, &st)) 
    		goto found;
    
        if (xlv__oref_resolve_subvol (oref, tab_vol_entry->log_subvol) == 0)
	   	goto found;
        if (xlv__oref_resolve_subvol (oref, tab_vol_entry->data_subvol) == 0)
    		goto found;
        if (xlv__oref_resolve_subvol (oref, tab_vol_entry->rt_subvol) == 0)
    		goto found;
    
        return (-1);
    
found:
        return 0;
}
/*
 * Resolve the object with the specified uuid using the passed
 * in object link list.
 */
int
xlv_oref_resolve_from_list(xlv_oref_t *oref, xlv_oref_t *oref_list)
{
	xlv_oref_t	*p;
	int		found;

	for (found = -1, p = oref_list; (found < 0) && p; p = p->next) {

		if (XLV_OREF_VOL(p) != NULL) {
			found = xlv_oref_resolve(oref, XLV_OREF_VOL(p));
		}
		else if (XLV_OREF_SUBVOL(p) != NULL) {
			found = xlv__oref_resolve_subvol (
					oref, XLV_OREF_SUBVOL(p));
		}
		else if (XLV_OREF_PLEX(p) != NULL) {
			found = xlv__oref_resolve_plex (
					oref, XLV_OREF_PLEX(p));
		}
		else if (XLV_OREF_VOL_ELMNT(p) != NULL) {
			found = xlv__oref_resolve_ve (
					oref, XLV_OREF_VOL_ELMNT(p));
		}

	}

	return (found);
}


/*
 * The following routines converts an object, described by an
 * xlv_oref_t, into its string representation (relative name):
 *
 *    volume_name.[log|data|rt].plex_number.vol_elmnt_number.
 */


/*
 * Create the volume element portion of a relative name
 * and append it to the end of the input string.
 */

static void
xlv_oref_ve_to_str (xlv_oref_t      *oref,
                    char            *str)
{
	xlv_tab_vol_elmnt_t	*ve;
	xlv_tab_plex_t		*plex;
	__psint_t		index;
	char			ve_str[4];

	ve = XLV_OREF_VOL_ELMNT(oref);
	if (ve == NULL)
		return;

	plex = XLV_OREF_PLEX(oref);
	index = (ve - plex->vol_elmnts);
	ASSERT ((index >= 0) && (index <= plex->num_vol_elmnts));

	sprintf (ve_str, ".%d", index);
	strcat (str, ve_str);
}


/*
 * Create the portion of the relative name from plex downwards;
 * appends it to the input string.
 */

static void
xlv_oref_plex_to_str (xlv_oref_t      *oref,
                      char            *str)
{
	int			p;
	xlv_tab_plex_t		*plex;
	xlv_tab_subvol_t        *sv;
	char			plex_num_str[4];

	plex = XLV_OREF_PLEX(oref);
	if (plex == NULL)
		return;

	sv = XLV_OREF_SUBVOL(oref);

	for (p=0; p < sv->num_plexes; p++) {
		if (sv->plex[p] == plex)
			break;
	}

	sprintf (plex_num_str, ".%d", p);
	strcat (str, plex_num_str);

	xlv_oref_ve_to_str (oref, str);
}


/*
 * Creates the portion of the relative name from subvolume downwards;
 * appends it to the input string.
 */

static void
xlv_oref_subvol_to_str (xlv_oref_t	*oref,
                        char		*str)
{
	xlv_tab_vol_entry_t	*vol;
	xlv_tab_subvol_t	*sv;

	vol = XLV_OREF_VOL(oref);
	ASSERT (vol != NULL);

	sv = XLV_OREF_SUBVOL(oref);
	if (sv == NULL)
		return;

	if (vol->log_subvol == sv) {
		strcat (str, ".log");
	}
	else if (vol->data_subvol == sv) {
		strcat (str, ".data");
	}
	else if (vol->rt_subvol == sv) {
		strcat (str, ".rt");
	}
	else {
		ASSERT (0);  /* Inconsistent object reference. */
	}

	/*
	 * Now get the portion of the relative name from plex on down.
	 */
	xlv_oref_plex_to_str (oref, str);
}



/*
 * Returns the relative name of the object specified by oref.
 *
 * This routine will create the top-level name (which is absolute),
 * and then call internal routines to fill in the low-level
 * relative names (if appropriate).
 */

void
xlv_oref_to_string (xlv_oref_t	*oref,
		    char	*str)
{

	/*
	 * We print out the top-level object's name here and
	 * then, if there are lower level objects, call the
	 * appropriate subroutine to print the relative names.
	 */

	if (XLV_OREF_VOL(oref) != NULL) {
		ASSERT (XLV_OREF_VOL(oref)->name != NULL);
		strncpy (str, XLV_OREF_VOL(oref)->name, sizeof(xlv_name_t));
		xlv_oref_subvol_to_str (oref, str);
	}
	else if (XLV_OREF_PLEX(oref) != NULL) {
		ASSERT (XLV_OREF_PLEX(oref)->name != NULL);
		strncpy (str, XLV_OREF_PLEX(oref)->name, sizeof(xlv_name_t));
		xlv_oref_ve_to_str (oref, str);
	}
	else if (XLV_OREF_VOL_ELMNT(oref) != NULL) {
		ASSERT (XLV_OREF_VOL_ELMNT(oref)->veu_name != NULL);
		strncpy (str, XLV_OREF_VOL_ELMNT(oref)->veu_name, 
			 sizeof(xlv_name_t));
	}
	else
		ASSERT (0);	/* object must have a top-level name. */
}


/*
 * Function to print out a volume element.
 * 
 * Sample output:
 *	vol_wei.log.1.2	0000078c-0000-0000-0000-000000000000
 *	    start=0, end=1375, (stripe)grp_size=1, stripe_unit_size=0
 * 	    /dev/dsk/dks1d5s5 (xxx blks)  00000796-0000-0000-0000-000000000000
 */

static int
print_ve (xlv_oref_t	*oref,
	  void		*I_print_mode)
{
	char	ve_name[50];

	xlv_oref_to_string (oref, ve_name);

	xlv_tab_ve_print (
		XLV_OREF_VOL_ELMNT (oref), ve_name, *((int *)I_print_mode));

	return (0);	/* continue */
}


/*
 * Prints out the object specified by oref.
 *
 */

void
xlv_oref_print (xlv_oref_t *oref, int p_mode)
{
	int	print_mode = p_mode;

	/*
	 * Print out the top-level name first and then all the 
	 * volume elements.
	 */

        if (XLV_OREF_VOL(oref) != NULL) {
		xlv_tab_vol_entry_t *volp = XLV_OREF_VOL(oref);
		ASSERT (volp->name != NULL);
		printf ("VOL %s (%s)\t\t(node=%s)\n",
			volp->name, xlv_vol_state_str(volp->state),
			(volp->nodename ? volp->nodename : "NULL"));
		xlv_for_each_ve_in_obj (oref, print_ve, &print_mode);
        }
        else if (XLV_OREF_PLEX(oref) != NULL) {
		ASSERT (XLV_OREF_PLEX(oref)->name != NULL);
                printf ("PLEX %s\n", XLV_OREF_PLEX(oref)->name);
		xlv_for_each_ve_in_obj (oref, print_ve, &print_mode);
        }
        else if (XLV_OREF_VOL_ELMNT(oref) != NULL) {
		(void) print_ve (oref, &print_mode);
	}
        else
		ASSERT (0);     /* object must have a top-level name. */
}


/*
 * An iterator function that goes through an object and applies a
 * caller-supplied function to each volume element.
 */

void
xlv_for_each_ve_in_obj (
		xlv_oref_t  *oref,
        XLV_OREF_PF operation,
        void        *arg) 	/* arg to pass to operation */
{
        if (XLV_OREF_VOL(oref) != NULL) {
		xlv_for_each_ve_in_vol (oref, operation, arg);
        }
        else if (XLV_OREF_SUBVOL(oref) != NULL) {
		xlv_for_each_ve_in_subvol (oref, operation, arg);
        }
        else if (XLV_OREF_PLEX(oref) != NULL) {
		(void) xlv_for_each_ve_in_plex (oref, operation, arg); 
        }
        else if (XLV_OREF_VOL_ELMNT(oref) != NULL) {
		(void) (*operation)(oref, arg);
        }
        else
		ASSERT (0);		/* invalid object type */
}

void
xlv_for_each_ve_in_vol (
        xlv_oref_t  *I_oref,
        XLV_OREF_PF operation,
        void        *arg)        /* arg to pass to operation */
{
	xlv_oref_t  oref_store;
	xlv_oref_t  *oref;

	oref_store = *I_oref;
	oref = &oref_store;

	if (XLV_OREF_VOL(oref)->log_subvol != NULL) {
		XLV_OREF_SUBVOL(oref) = XLV_OREF_VOL(oref)->log_subvol;
		if ( xlv_for_each_ve_in_subvol(oref, operation, arg) )
			return;
	}
	if (XLV_OREF_VOL(oref)->data_subvol != NULL) {
                XLV_OREF_SUBVOL(oref) = XLV_OREF_VOL(oref)->data_subvol;
		if ( xlv_for_each_ve_in_subvol(oref, operation, arg) )
			return;
	}
	if (XLV_OREF_VOL(oref)->rt_subvol != NULL) {
		XLV_OREF_SUBVOL(oref) = XLV_OREF_VOL(oref)->rt_subvol;
		if ( xlv_for_each_ve_in_subvol(oref, operation, arg) )
			return;
	}
}

int
xlv_for_each_ve_in_subvol (
        xlv_oref_t  *I_oref,
        XLV_OREF_PF operation,
        void        *arg)        /* arg to pass to operation */
{
	xlv_oref_t		oref;
	xlv_tab_subvol_t	*subvol;
	int			p;
	int			end_scan = 0;	/* terminate with 1 */
	uint_t			st;

	oref = *I_oref;

	subvol = XLV_OREF_SUBVOL(&oref);
	if (subvol == NULL) {
		return 0;
	}

	if (uuid_is_nil(&subvol->uuid, &st)) {
		/*
		 * A good/valid subvolume must have a non-null uuid.
		 * 
		 * NOTE: When doing kernel (sub)volume enumeration,
		 * the kernel clears the uuid in the subvolume struct
		 * for subvolumes that do not exist.
		 */
		return 0;
	}

	for (p = 0; p < subvol->num_plexes && !end_scan; p++) {
		XLV_OREF_PLEX(&oref) = subvol->plex[p];
		end_scan = xlv_for_each_ve_in_plex (&oref, operation, arg);
	}
	return (end_scan);
}

int
xlv_for_each_ve_in_plex (
        xlv_oref_t  *I_oref,
        XLV_OREF_PF operation,
        void        *arg)        /* arg to pass to operation */
{
        xlv_oref_t      oref;
	xlv_tab_plex_t	*plex;
	int		ve;
	int		end_scan;

        oref = *I_oref;

	plex = XLV_OREF_PLEX(&oref);
	if (plex == NULL)
		return (0);		/* nothing there */

	for (ve=0; ve < plex->num_vol_elmnts; ve++) {
		XLV_OREF_VOL_ELMNT (&oref) = &(plex->vol_elmnts[ve]);
		end_scan = (*operation) (&oref, arg);
		if (end_scan)
			break;
	}
	return (end_scan);		/* 0 to continue with scan */
}

/*
 * Return non-zero if the oref is missing pieces.
 * Otherwise return zero.
 */
int
xlv_is_incomplete_oref(xlv_oref_t *oref)
{
	xlv_tab_vol_entry_t	*vp;
	xlv_tab_plex_t		*pp;
	xlv_tab_vol_elmnt_t	*vep;
	int			retval = 1;	/* Assume the worst. */

	if (vp = XLV_OREF_VOL(oref)) {
		retval = (vp->state == XLV_VOL_STATE_MISS_COMMON_PIECE
			|| vp->state == XLV_VOL_STATE_MISS_UNIQUE_PIECE);

	} else if (pp = XLV_OREF_PLEX(oref)) {
		int	ve;
		retval = 0;
		for(ve = 0, vep = &pp->vol_elmnts[0];
		    ve < pp->num_vol_elmnts;
		    ve++, vep++) {
			if (XLV_VE_STATE_INCOMPLETE == vep->state) {
				retval = 1;
				break;
			}
		}

	} else if (vep = XLV_OREF_VOL_ELMNT(oref)) {
		retval = (vep->state == XLV_VE_STATE_INCOMPLETE);
	}

	return(retval);

} /* end of xlv_is_incomplete_oref() */
