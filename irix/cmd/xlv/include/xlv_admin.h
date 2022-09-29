#ifndef __XLV_ADMIN_H__
#define __XLV_ADMIN_H__

/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1993, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.3 $"

/*
 * Declarations for the admin interface to the XLV volume manager.
 */

typedef struct {
	dev_t	device;
	uuid_t	disk_part_uuid;
} disk_part_t;


typedef struct {
	uuid_t		vol_elmnt_uuid;
	xlv_name_t	vol_elmnt_name;
	unsigned char	num_stripes;
	short		stripe_width;
	long long	start_block_no;
	long long	end_block_no;
	disk_part_t 	disk_parts[XLV_MAX_DISK_PARTS_PER_VE];
} vol_elmnt_info_t, *vol_elmnt_info_p_t;

extern xlv_disk_part_init (
	char *,			/* IN device_pathname */
	int *);			/* OUT status */

/*
 * Labels: disk_part, vol_elmnt.  (not associated with anything else).
 */
extern xlv_vol_elmnt_create (
	vol_elmnt_info_t *,	/* IN vol_elmnt_info */ 
	uuid_t *, 		/* OUT vol_elmnt_uuid */ 
	int *);	 		/* OUT status */

/*
 * Labels: disk part, vol elmnt, plex.
 *         If this is last vol_elmnt, affects subvol, vol.
 */
extern xlv_vol_elmnt_destroy (
	uuid_t,			/* IN vol_elmnt_uuid */
	boolean_t,		/* IN force */
	int *);			/* OUT status */

extern xlv_vol_elmnt_get_info (
	uuid_t,                 /* IN vol_elmnt_uuid */
	vol_elmnt_info_t *,     /* OUT vol_elmnt_info */
	int *);                 /* OUT status */

/*
 * Labels: disk_part.
 * Can set: vol_elmnt_name.
 */
extern xlv_vol_elmnt_set_info (
	uuid_t,			/* IN vol_elmnt_uuid */
	vol_elmnt_info_t *,	/* IN vol_elmnt_info */
	int *);			/* OUT status */
 
/*
 * No effect on labels. This change does not survive reboots.
 */
extern xlv_vol_elmnt_on (
	uuid_t);		/* IN vol_elmnt_uuid */

extern xlv_vol_elmnt_off (
	uuid_t);		/* IN vol_elmnt_uuid */

/*
 * Labels: disk_part, vol_elmnt, plex.
 *         Also need to disable writes while this is going on.
 */
extern xlv_vol_elmnt_swap (
	uuid_t,			/* IN vol_elmnt1_uuid */
	uuid_t,			/* IN vol_elmnt2_uuid */
	int *);			/* OUT status */

/*
 * Labels: depending upon what's being done, may affect up to volume.
 */
extern xlv_associate (
	uuid_t,			/* IN parent_uuid */
	uuid_t,			/* IN child_uuid */
	int *);			/* OUT status */

extern xlv_dissociate (
	uuid_t,			/* IN uuid */
	int *);			/* OUT status */

typedef struct {
	uuid_t      	plex_uuid;
	xlv_name_t  	plex_name;
	plex_state_t    plex_state;	/* readonly */
	long		num_vol_elmnts;
	boolean_t		maintain_plex_consistency;
	boolean_t		use_write_log;
	boolean_t		may_be_sparse;

	/* The following field is only used by xlv_plex_get_info
	   to return the plex geometry. */
	uuid_t		vol_elmnt[XLV_MAX_VE_PER_PLEX];
} plex_info_t;


/*
 * Labels: none. A plex really has no existence until a disk
 * part is associated with it.
 * Keep it in a file.
 */
extern xlv_plex_create (
	plex_info_t *,		/* IN plex_info */
	uuid_t *,		/* OUT uuid */
	int *);			/* OUT status */ 

extern xlv_plex_destroy (
	uuid_t,			/* IN uuid */
	boolean_t,		/* IN force */
	int *);                 /* OUT status */

extern xlv_plex_get_info (
	uuid_t,			/* IN uuid */
	plex_info_t *,		/* OUT plex_info */
	int *);                 /* OUT status */

/*
 * Labels: would affect all disk parts that belong to this
 * plex.
 */
extern xlv_plex_set_info (
	uuid_t,			/* IN uuid */
	plex_info_t *,		/* IN plex_info */
	int *);                 /* OUT status */

typedef struct {
	uuid_t		subvol_uuid;
	xlv_name_t	subvol_name;
	short           subvol_type;    /* log, data.. */
	dev_t		device;

	/* The following field is only used by xlv_subvol_get_info
	   to return the subvol geometry. */
	short		num_plexes;
	uuid_t		plexes[XLV_MAX_PLEXES];	
} subvol_info_t;

extern xlv_subvol_create (
	subvol_info_t *,	/* IN subvol_info */
	uuid_t *,		/* OUT uuid */
        int *);                 /* OUT status */

extern xlv_subvol_destroy (
	uuid_t,			/* IN uuid */
	boolean_t,		/* IN force */
	int *);                 /* OUT status */

extern xlv_subvol_get_info (
	uuid_t,			/* IN uuid */
	subvol_info_t *,	/* IN subvol_info */
	int *);                 /* OUT status */

extern xlv_subvol_set_info (
	uuid_t,			/* IN uuid */
	subvol_info_t *,	/* IN subvol_info */
	int *);                 /* OUT status */

typedef struct {
        uuid_t          vol_uuid;
        xlv_name_t      vol_name;
	boolean_t		rootable;
	boolean_t		bootable;
	boolean_t		automount;
	short		sector_size;
	uuid_t		log_subvol_uuid;
	uuid_t		data_subvol_uuid;
	uuid_t		rt_subvol_uuid;
} vol_info_t;

extern xlv_vol_create (
	vol_info_t *,		/* IN vol_info */
	uuid_t *,		/* OUT uuid */
	int *);			/* OUT status */

extern xlv_vol_destroy (
	uuid_t,			/* IN uuid */
	boolean_t,		/* IN force */
        int *);                 /* OUT status */

extern xlv_vol_get_info (
	uuid_t,			/* IN uuid */
	vol_info_t *,		/* OUT vol_info */
        int *);                 /* OUT status */

extern xlv_vol_set_info (
	uuid_t,			/* IN uuid */
	vol_info_t *,		/* IN vol_info */
        int *);                 /* OUT status */


extern xlv_name_to_uuid (
	xlv_name_t *,		/* IN name */
	int *,			/* OUT uuid_count */
	uuid_t **,		/* OUT uuid[] (malloc'd structure */
        int *);                 /* OUT status */

extern xlv_devt_to_uuid (
	dev_t,			/* IN dev_t */
	uuid_t *);		/* OUT uuid */

extern xlv_register_interest (
	uuid_t,			/* IN uuid */
        void (*)(void *),	/* IN func_ptr */
        int *);                 /* OUT status */


#endif /* __XLV_ADMIN_H__ */
