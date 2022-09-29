/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.13 $"

#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <bstring.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <xlv_oref.h>
#include <xlv_utils.h>
#include <sys/dvh.h>
#include <sys/xlv_vh.h>
#include <xlv_lab.h>
#include <sys/debug.h>
#include <diskinfo.h>

#include "drv_test.h"

/*
 * Takes object descriptions and create an xlv_tab and xlv_vol_tab
 * from them.
 *
 * Each volume is created in 2 passes. 
 * The first pass, triggered when an object is first defined,
 * calls xlv_mk_obj().
 * The second pass, triggered when all the objects that make up
 * a single volume have been defined, calls xlv_mk_vol_done().
 * We use a 2-pass parser because there may be forward references.
 *
 * After all the volumes have been defined, xlv_mk_done() is
 * called to generate the final output.
 *
 * Note: many of the routines in this module malloc memory from heap 
 * but does not free it.  This is OK as this is part of an admin
 * utility that is expected to terminate.
 */

/*
 * The main structures themselves.  This is the output.
 */

/* We do not define them here.  We want to use the one defined in
 * xlv_ktab.c.
 */
#if 0
struct xlv_tab_vol_s	*xlv_tab_vol = NULL;
struct xlv_tab_s 	*xlv_tab = NULL;
#endif


/*
 * Data structures that we populate for each volume.
 *
 * curr_vol - current volume that we are defining.
 * curr_subvol - first subvolume that we have defined for
 *               current volume.
 * plex_list_head - plexes defined for current volume.
 * vol_elmnt_nodes - volume elements defined for current volume.
 *
 * These data structures should be cleared after each volume
 * has been processed.
 */

static xlv_tab_vol_entry_t *curr_vol;
static int	curr_subvol = -1;
 
typedef struct plex_node_s {
    struct plex_node_s	*next;
    boolean_t		part_of_a_subvol;
    xlv_tab_plex_t	plex;
} plex_node_t;

static plex_node_t	*plex_list_head = NULL;

typedef struct vol_elmnt_node_s {
    struct vol_elmnt_node_s	*next;
    boolean_t			part_of_a_plex;
    xlv_tab_vol_elmnt_t		vol_elmnt;
} vol_elmnt_node_t;

static vol_elmnt_node_t *vol_elmnt_nodes = NULL;


static void mk_vol_1 (char *obj_name, char *flag_name[],
		    char *flag_value[], int num_flags);
static void mk_subvol_1 (char *obj_name, char *flag_name[],
		    char *flag_value[], int num_flags,
		    char *component[], int num_components,
		    int subvol_type);
static void mk_plex_1 (char *obj_name, char *flag_name[],
		    char *flag_value[], int num_flags,
		    char *component[], int num_components);
static void mk_vol_elmnt_1 (char *obj_name, char *flag_name[],
		    char *flag_value[], int num_flags,
		    char *component[], int num_components);
static void mk_vol_2 (void);
static void mk_subvol_2 (void);
static void mk_plex_2 (void);
static void check_for_unreferenced_elmnts (void);


/*
 * This routine is called by the lexical analyzer whenever 
 * an object is defined.  This dispatches to the first-pass
 * processing routine for the specified object type.
 * 
 * This is the main routine for the first-pass, collecting
 * all the pieces that make up a volume.
 */
void xlv_mk_obj (int obj_type, char *obj_name, char *flag_name[],
		 char *flag_value[], int flag_index,
		 char *component[], int component_index)
{
    switch (obj_type) {
	case XLV_OBJ_TYPE_VOL:
	    mk_vol_1(obj_name, flag_name, flag_value,
			 flag_index);
	    break;
	case XLV_OBJ_TYPE_LOG_SUBVOL:
	    mk_subvol_1(obj_name, flag_name, flag_value, flag_index, 
		component, component_index, XLV_SUBVOL_TYPE_LOG);
	    break;
	case XLV_OBJ_TYPE_DATA_SUBVOL:
	    mk_subvol_1(obj_name, flag_name, flag_value, flag_index, 
		component, component_index, XLV_SUBVOL_TYPE_DATA);
	    break;
	case XLV_OBJ_TYPE_RT_SUBVOL:
	    mk_subvol_1(obj_name, flag_name, flag_value, flag_index, 
		component, component_index, XLV_SUBVOL_TYPE_RT);
	    break;
	case XLV_OBJ_TYPE_PLEX:
	    mk_plex_1(obj_name, flag_name, flag_value,
		flag_index, component, component_index);
	    break;
	case XLV_OBJ_TYPE_VOL_ELMNT:
	    mk_vol_elmnt_1(obj_name, flag_name, flag_value,
		flag_index, component, component_index);
	    break;
	default:
	    ASSERT (0);		/* Illegal object type */
	    break;
    }
}


/*
 * This routine is called by the lexical analyzer after all
 * the objects that make up a volume have been defined.  It's job 
 * is to merge together all the objects defined during the first 
 * pass and resolve any forward references.
 *
 * This is the main routine for the second-pass.
 */
void xlv_mk_vol_done (void)
{
    mk_vol_2 ();
    mk_subvol_2 ();
    mk_plex_2 ();

    /* Now clear all the state about this volume */
    curr_vol = NULL;
    curr_subvol = -1;

    /* Note that we will orphan any plex and vol_elment nodes
       that have not been resolved.  But that's ok as the memory
       would be freed when this program exits. */
    plex_list_head = NULL;
    vol_elmnt_nodes = NULL;
}


/* 
 * This routine outputs the results.  It is called by the lexical
 * analyzer after all the volumes have been defined.
 */
void xlv_mk_done (void)
{
    int			v, sv;
    int			status;
    xlv_tab_vol_entry_t	*vol;

    check_for_unreferenced_elmnts ();

    /* We want to defer writing the disk labels until everything
       is OK since aborting the operation in the middle may leave
       them in an inconsistent state. */

#if 0
    /* Create all the required devices.  We can do this now since device
       node mismatches can be easily fixed up. */
    for (v = 0, vol = xlv_tab_vol->vol;
	 (! xlv_encountered_errors()) && (v < xlv_tab_vol->num_vols);
	 v++, vol++) {
            if (xlv_create_nodes (vol)) {
                xlv_error ("Cannot create device nodes for %.*s.", vol->name);
	    }
    }

    /* Now we're committed, write the labels */
    for (v = 0, vol = xlv_tab_vol->vol;
         (! xlv_encountered_errors() ) && (v < xlv_tab_vol->num_vols);
	 v++, vol++) {
	status = -1;

	/* Create the subvolume device nodes. */

	xlv_lab2_write (vol, &(vol->uuid), &status);
	if (status != 0) {
	    xlv_error ("Cannot write labels for %.*s",
		    sizeof(xlv_name_t), vol->name);
	    xlv_error ("Disk labels may be inconsistent.");
	}
    }
#endif

    /* Print the result */
    xlv_tab_print (xlv_tab_vol, xlv_tab);

    /* Compute the subvolume sizes and the block maps */

    for (sv=0; sv < xlv_tab->num_subvols; sv++) {
	xlv_tab->subvolume[sv].subvol_size = 
              xlv_tab_subvol_size (&xlv_tab->subvolume[sv]);
	xlv_tab->subvolume[sv].block_map = xlv_tab_create_block_map  
               (&xlv_tab->subvolume[sv]);
    }

    /* Invoke the driver test program */
    drv_test();
}



/*
 * First-pass processing for a vol object.
 * We add the vol entry to xlv_vol_tab, and fill in the
 * volume data.
 * Left unresolved are the pointers to the subvolume
 * entries.  Instead, these pointers will contain the
 * subvolume names (which will be resolved during the
 * second pass).
 */
static void mk_vol_1 (char *obj_name, char *flag_name[],
		    char *flag_value[], int num_flags)
{
    xlv_tab_vol_entry_t *vol;
    unsigned 		st;

    /* Initialize xlv_tab_vol if this is the first time */
    if (xlv_tab_vol == NULL) {
	xlv_tab_vol = (xlv_tab_vol_t *) malloc(sizeof(xlv_tab_vol_t));
	xlv_tab_vol->num_vols = 0;
	xlv_tab_vol->max_vols = 1;
    }

    /* Grow the table if necessary */
    if (xlv_tab_vol->num_vols == xlv_tab_vol->max_vols) {
	xlv_tab_vol = (xlv_tab_vol_t *) realloc (xlv_tab_vol,
	    sizeof(xlv_tab_vol_t) /* already contains 1 entry */ + 
	    (sizeof(xlv_tab_vol_entry_t) * xlv_tab_vol->num_vols));
	xlv_tab_vol->max_vols++;
    }

    /* Add this entry */
    vol = &(xlv_tab_vol->vol[xlv_tab_vol->num_vols]);
    uuid_create (&(vol->uuid), &st);
    if (st != 0)
	xlv_error ("Cannot create uuid.");
    strncpy (vol->name, obj_name, sizeof(xlv_name_t));
    vol->flags = 0;
    vol->sector_size = 512;

    /* Remember that we are defining this volume. */
    curr_vol = vol;

    xlv_tab_vol->num_vols++;

}



/*
 * First-pass processing for a subvol object.
 * We add the subvol entry to xlv_tab, and fill in the
 * subvolume data.  
 * Left unresolved are the pointers to the plexes and
 * volume.
 */

static void mk_subvol_1 (char *obj_name, char *flag_name[],
		    char *flag_value[], int num_flags,
		    char *component[], int num_components,
		    int subvol_type)
{
    xlv_tab_subvol_t	*subvol;
    int			i;
    unsigned	 	st;

    /* Initialize xlv_tab if this is the first time */
    if (xlv_tab == NULL) {
        xlv_tab = (xlv_tab_t *) malloc(sizeof(xlv_tab_t));
        xlv_tab->num_subvols = 0;
        xlv_tab->max_subvols = 1;
    }

    /* Grow the table if necessary */
    if (xlv_tab->num_subvols == xlv_tab->max_subvols) {
        xlv_tab = (xlv_tab_t *) realloc (xlv_tab,
            sizeof(xlv_tab_t) /* already contains 1 entry */ +
            (sizeof(xlv_tab_subvol_t) * xlv_tab->num_subvols));
        xlv_tab->max_subvols++;
    }

    /* Record the first subvolume being defined for this volume.
       This is where we would start the processing during the
       second pass. */
    if (curr_subvol == -1) curr_subvol = xlv_tab->num_subvols;

    subvol = &(xlv_tab->subvolume[xlv_tab->num_subvols]);
    subvol->flags = 0;
    subvol->vol_p = NULL; 
    uuid_create (&(subvol->uuid), &st);
    if (st != 0)
        xlv_error ("Cannot create uuid.");

    subvol->subvol_type = subvol_type;
    subvol->dev = 0;	/* To be filled in after we generate the
			   device special files.  Later. */

    subvol->num_plexes = num_components;

    /*
     * Since the references may not be resolved yet,
     * we just store the name of the plex instead of
     * the address of the real structure.
     */
    for (i = 0; i < num_components; i++) {
        subvol->plex[i] = (xlv_tab_plex_t *)component[i];
    }

    xlv_tab->num_subvols++;
    
}


/*
 * First-pass processing for a plex object.
 * We allocate the memory for the plex (based upon the
 * number of vol elmnts), and fill in the plex information.
 * This routine allocates the correct amount of memory
 * to hold all the vol elment information but does not
 * initialize them.  (The definitions for the volume
 * elements may occur later.)
 *
 * This routine does fill in the vol elmnt names fields
 * for all the volume elements so that we can match them
 * during the second pass.
 *
 * Since the subvolume that owns this plex may not have
 * been defined yet, we do not link this plex to a subvolume.
 * Instead, we use a separate linked list of plexes.
 */

static void mk_plex_1 (char *obj_name, char *flag_name[],
		    char *flag_value[], int num_flags,
		    char *component[], int num_components)
{
    plex_node_t		*plex_node, *prev;
    xlv_tab_plex_t	*plex;
    int			i;
    unsigned		st;

    plex_node = (plex_node_t *) malloc (
	sizeof(plex_node_t) + 
	((num_components-1)*sizeof(xlv_tab_vol_elmnt_t)) );

    plex_node->next = plex_list_head;
    plex_list_head = plex_node;
    plex_node->part_of_a_subvol = B_FALSE;

    plex = &(plex_node->plex);
    plex->flags = 0;
    uuid_create (&(plex->uuid), &st);
    if (st != 0)
	xlv_error ("Cannot create uuid.");
    
    strncpy (plex->name, obj_name, sizeof(xlv_name_t));
    plex->num_vol_elmnts = num_components;
    plex->max_vol_elmnts = num_components;

    for (i = 0; i < num_components; i++) {
	strncpy (plex->vol_elmnts[i].name, 
		 component[i], sizeof(xlv_name_t));
    }

}



/*
 * First-pass processing for a volume element object.
 * A volume element should not have any unresolved
 * references.
 * We link the volume element structure in a linked
 * list for matching with the proper plex during
 * the second pass.
 */

static void mk_vol_elmnt_1 (char *obj_name, char *flag_name[],
		    char *flag_value[], int num_flags,
		    char *component[], int num_components)
{
    vol_elmnt_node_t	*ve_node;
    xlv_tab_vol_elmnt_t	*ve;
    int			i;
    unsigned	        st;
    long long 		min_blocks, blocks;

    ve_node = (vol_elmnt_node_t *) malloc (sizeof(vol_elmnt_node_t));
    ve_node->next = vol_elmnt_nodes;
    vol_elmnt_nodes = ve_node;
    ve_node->part_of_a_plex = B_FALSE;

    ve = &(ve_node->vol_elmnt);
    uuid_create (&(ve->uuid), &st);
    if (st != 0)
        xlv_error ("Cannot create uuid.");

    strncpy (ve->name, obj_name, sizeof(xlv_name_t));
    ve->type = XLV_VE_TYPE_STRIPE;		/* stripe, just like before */
    ve->grp_size = num_components;
    ve->stripe_unit_size = 0;
    ve->state = XLV_VE_STATE_EMPTY;

    for (i = 0; i < num_components; i++) {
        ve->disk_parts[i].dev = name_to_dev (component[i]);
	if (ve->disk_parts[i].dev == 0)
	    xlv_error ("Cannot access %.*s.",
               sizeof(xlv_name_t), component[0]);
	uuid_create (&(ve->disk_parts[i].uuid), &st);
	if (st != 0)
            xlv_error ("Cannot create uuid.");
    }

    /* If not specified by flag, assume that we'll take the whole
       partition.  These numbers are relative, the second pass will
       make them absolute.  
       If the volume element is striped, make them all the same.*/
    min_blocks = findsize (component[0]);

    if (min_blocks > 0x7fffffffLL)
	min_blocks = -1LL;		/* force error */
    ve->disk_parts[0].part_size = min_blocks;

    /* If dev == 0, then we've already reported the device as
       being inaccessible, so we don't need to report any other
       errors (like unable to get partition size, tracksize, etc.) 
       on the non-existent device. */
    if ((min_blocks == -1LL) && (ve->disk_parts[0].dev != 0)) {
        xlv_error ("Cannnot determine size of %.*s.",
                sizeof(xlv_name_t), component[0]);
	min_blocks = 0;
    }

    for (i = 1; i < num_components; i++) {
	blocks = findsize (component[i]);
	if (blocks > 0x7fffffffLL)
	    blocks = -1LL;		/* force error */
	if ((blocks == -1LL) && (ve->disk_parts[i].dev != 0)) {
	    xlv_error ("Cannot determine size of %.*s.",
		sizeof(xlv_name_t), component[i]);
	}
	else
	{
	    ve->disk_parts[i].part_size = blocks;
	    if (blocks != min_blocks) {
	        xlv_error ("Partition sizes for %.*s are not the same, truncating.",
		    sizeof(xlv_name_t), obj_name);
	        min_blocks = (min_blocks < blocks) ? min_blocks : blocks;
	    }
	}
    }

    /* If the volume element is striped, and the stripe width is not
       specified by a flag (step=), then we default to the tracksize
       of the first element of the stripe set. */
    if (num_components > 1) {
        ve->stripe_unit_size = findtrksize (component[0]);
        if ((ve->stripe_unit_size == -1) && (ve->disk_parts[0].dev != 0))
            xlv_error ("Cannot determine tracksize of %.*s.",
                sizeof(xlv_name_t), component[0]);
    }

    /* Note that the second pass routine assumes that start_block_no == 0
       when relative block numbers are used. */
    ve->start_block_no = 0;
    ve->end_block_no = (min_blocks * num_components) - 1LL;

}



/*
 * Second-pass routines to resolve the pointers in the
 * volume and subvolume structures.
 *
 * mk_vol_2() goes through all the volume entries and
 * resolves the string names of subvolumes into pointers
 * to the actual subvol records.  The inner loop routine
 * is mk_vol_2_resolve_subvol().  When the corresponding
 * subvolume structure is found, the link back to the
 * owning volume is also set.
 */

static xlv_tab_subvol_t *mk_vol_2_resolve_subvol (
	int subvol_type, xlv_tab_vol_entry_t *vp)
{
    int		sv;

    if (curr_subvol == -1) {
	xlv_error ("At least one subvolume must be defined.");
	return ((xlv_tab_subvol_t *) NULL);
    }

    for (sv = curr_subvol; sv <= xlv_tab->num_subvols; sv++) {
	if (xlv_tab->subvolume[sv].subvol_type == subvol_type) {

	    /* Establish a backward link from the subvol to
	       the volume also. */
	    xlv_tab->subvolume[sv].vol_p = vp;
	    return (& (xlv_tab->subvolume[sv]));
	}
    }

    return ((xlv_tab_subvol_t *) NULL);
}

static void mk_vol_2 (void)
{
    xlv_tab_vol_entry_t	*vp;

    vp = curr_vol;
    if (vp == NULL) {
	xlv_error ("Volume name not specified.\n");
	return;
    }
    vp->log_subvol = mk_vol_2_resolve_subvol (XLV_SUBVOL_TYPE_LOG, vp);
    vp->data_subvol = mk_vol_2_resolve_subvol (XLV_SUBVOL_TYPE_DATA, vp);
    vp->rt_subvol = mk_vol_2_resolve_subvol (XLV_SUBVOL_TYPE_RT, vp);
}



/*
 * Second-pass routines to resolve the pointers in the
 * subvolume structure.
 *
 * mk_subvol_2() goes through all the subvolume entries
 * for the current volume and resolves the string names of the 
 * plexes into pointers to the actual plex records by calling 
 * mk_subvol_2_resolve_plex().
 *
 * mk_subvol_2_resolve_plex() also marks each plex that
 * is associated with a subvolume so that we can check
 * for plexes that don't belong to any subvolume.
 */

static xlv_tab_plex_t *mk_subvol_2_resolve_plex (
        char *name)
{
    plex_node_t		*p;

    if (name == NULL)
        return ((xlv_tab_plex_t *) NULL);

    for (p = plex_list_head; p != NULL; p = p->next) {
	/*
	 * If the plex is not already part of some other
	 * subvolume and the name matches.
	 */
	if ((p->part_of_a_subvol == B_FALSE) &&
            (strncmp (p->plex.name,
                     name, sizeof(xlv_name_t) ) == 0)) {

	    p->part_of_a_subvol = B_TRUE;
            return (&(p->plex));
        }
    }

    return ((xlv_tab_plex_t *) NULL);
}


static void mk_subvol_2 (void)
{
    int			sv, p;
    xlv_tab_subvol_t	*svp;
    xlv_tab_plex_t	*plex;

    if (curr_subvol != -1) {
	for (sv = curr_subvol, svp = &(xlv_tab->subvolume[sv]);
	     sv < xlv_tab->num_subvols;
	     sv++, svp++) {

	    /* This can be either because we were missing a 
	       volume directive or there has been too many 
	       subvolumes defined. */
	    if (svp->vol_p == NULL)
	        xlv_error ("Subvolume is not part of any volume.");

	    for (p = 0; p < svp->num_plexes; p++) {
	        svp->plex[p] = mk_subvol_2_resolve_plex (
		    (char *)(svp->plex[p]) );
	    }

	    /* Compute the maximum length of this subvolume in
	       blocks. */
	    svp->subvol_size = xlv_tab_subvol_size (svp);
	}
    }
}


/*
 * Second-pass routines to fill in the volume element
 * fields in the plex structure.  During the first pass,
 * the plexes and the volume element information were
 * filled in and linked into separate lists.
 * Since the volume element information is allocated
 * within the plex structure, we need to copy them.
 *
 * mk_plex_2_resolve_vol_elmnt() also marks each volume
 * element that is processed so that we can identify
 * volume elements that don't belong to any plexes.
 */
static int mk_plex_2_resolve_vol_elmnt (
	xlv_tab_vol_elmnt_t *vol_elmnt)
{
    vol_elmnt_node_t	*ve;

    if (vol_elmnt->name == NULL) {
	bzero (vol_elmnt, sizeof(xlv_tab_vol_elmnt_t));
	return (-1);
    }

    for (ve = vol_elmnt_nodes; ve != NULL; ve = ve->next) {
        /*
         * If the vol elmnt is not already part of some
         * plex and the name matches.
         */
        if ((ve->part_of_a_plex == B_FALSE) &&
            (strncmp (ve->vol_elmnt.name,
                     vol_elmnt->name, 
		     sizeof(xlv_name_t) ) == 0)) {
            ve->part_of_a_plex = B_TRUE;

	    /* Copy the fields */
	    bcopy (&(ve->vol_elmnt), vol_elmnt, sizeof(xlv_tab_vol_elmnt_t));

            return (0);
        }
    }

    return (-1);

}


static void mk_plex_2 (void)
{
    plex_node_t		*pnode;
    int			v;
    xlv_tab_vol_elmnt_t *ve;
    long long		block_no;

    for (pnode = plex_list_head; pnode != NULL;
	 pnode = pnode->next) {

	block_no = 0;	/* Starting block_no within plex of next vol elmnt */
	for (v = 0, ve = &(pnode->plex.vol_elmnts[v]); 
	     v < pnode->plex.num_vol_elmnts; 
	     v++, ve++) {
	    if (mk_plex_2_resolve_vol_elmnt(
		&(pnode->plex.vol_elmnts[v]) )) {
		xlv_error ( 
"Cannot resolve volume element %.*s in plex %.*s.",
sizeof(xlv_name_t), (char *) &(pnode->plex.vol_elmnts[v]),
sizeof(xlv_name_t), pnode->plex.name);
	    }

            /* Make the relative block numbers absolute. */
            if (ve->start_block_no == 0) {
                ve->start_block_no += block_no;
                ve->end_block_no += block_no;
                block_no = ve->end_block_no + 1;
            }

            /* Need to check to make sure the block ranges don't overlap */
	    
	}
    }

}
    

/*
 * Go through the node and volume element lists and make sure
 * that they have all been associated with the current volume.
 */
void check_for_unreferenced_elmnts ()
{
    plex_node_t		*plex_node;
    vol_elmnt_node_t    *ve_node;

    /* Check for plexes that don't belong to any subvolume. */
    for (plex_node = plex_list_head; plex_node != NULL;
         plex_node = plex_node->next) {
        if (plex_node->part_of_a_subvol == B_FALSE)
            xlv_error ("Plex %.*s is not part of a subvolume.",
                    sizeof(xlv_name_t), plex_node->plex.name);
    }

    /* Check for volume elements that have not been associated with
       any plex. */
    for (ve_node = vol_elmnt_nodes; ve_node != NULL;
         ve_node = ve_node->next) {
        if (ve_node->part_of_a_plex == B_FALSE)
            xlv_error ("Volume element %.*s is not part of a plex",
                    sizeof(xlv_name_t), ve_node->vol_elmnt.name);

    }
}

