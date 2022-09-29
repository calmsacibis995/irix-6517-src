#ifndef __XLV_UTILS_H__
#define __XLV_UTILS_H__

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
#ident "$Revision: 1.47 $"

#ifndef YES
#define YES 1
#endif

#ifndef NO
#define NO 0
#endif

#define XLV_DEV_BLKPATH		"/dev/xlv/"
#define XLV_DEV_RAWPATH		"/dev/rxlv/"

/*
 * Functions to return a string corresponding to the state.
 */
extern char *xlv_vol_state_str(unsigned int state);
extern char *xlv_ve_state_str(unsigned int state);

/*
 * Get and set the root directory as perceived by libxlv.
 */
extern char	*xlv_getrootname(void);
extern char	*xlv_setrootname(char *name);

/*
 * Set the maximum number of open volume header file descriptors
 * which can be kept open at any one time.
 */
#define XLV_MAXFDS        1000
#define XLV_MINFDS        16
#define XLV_FDS_FOR_APPL  20

extern int xlv_set_maxfds(int max);

/*
 * Generate a pathname rooted at the specified root directory and
 * store that name in "rooted_pn". The passed-in pathname "fqpn" must
 * be a fully qualified pathname.
 */
extern char	*xlv_root_pathname(char *rooted_pn, char *fqpn);

extern int   xlv_create_node_pair (xlv_name_t devname, char *nodename,
				   dev_t devno, xlv_tab_t *xlv_tab);
extern dev_t xlv_uuid_to_dev (uuid_t *uuid);
extern dev_t name_to_dev (char *pathname);
extern int   roundblk (int cnt, int dev_bsize);

/*
 * Allocate and free space for a volume and subvolume.
 * This just allocates the maximum space needed.
 */
extern xlv_tab_subvol_t    *get_subvol_space(void);
extern xlv_tab_vol_entry_t *get_vol_space(void);
extern int                 free_subvol_space(xlv_tab_subvol_t *svp);
extern int                 free_vol_space(xlv_tab_vol_entry_t *vp);

/*
 * Deallocate the volume and subvolume tables.
 */
extern void	free_tab_subvol(xlv_tab_t **tab_sv_p);
extern void	free_tab_vol(xlv_tab_vol_t **tab_vol_p);

/*
 * A table of is used to track the correspondence between
 * subvolume uuids and XLV device minor number. This table
 * is used for keeping state across multiple xlv_assemble
 * calls. A subvolume must always be assigned the same
 * device number for the life-time of the current system
 * session.
 *
 * The "index" into this table corresponds to a XLV
 * device minor number. 
 */
#define XLV_MAX_VOLS		50
#define XLV_MAX_SUBVOLS		(XLV_MAX_VOLS * XLV_SUBVOLS_PER_VOL)
#define XLV_TAB_UUID_BOOT_DATA	0
#define XLV_TAB_UUID_BOOT_LOG	1
#define XLV_TAB_UUID_BOOT_RT	2
#define XLV_TAB_UUID_RSVD	4
#define XLV_TAB_UUID_SIZE	(XLV_MAX_SUBVOLS + XLV_TAB_UUID_RSVD)
#define XLV_TAB_UUID_GROW	(3 * XLV_SUBVOLS_PER_VOL)


/*
 * Query the kernel for XLV configuration information
 */
extern int		get_max_kernel_locks (void);
extern __uint64_t	get_kernel_sv_maxsize(void);

extern dev_t		xlv_get_rootdevice(void);


/*
 * Get and set the node name of the local system.
 */
extern char	*xlv_getnodename(void);
extern int	xlv_setnodename(char *name);

extern int	xlv_check_xlv_tab(xlv_tab_vol_t *tab_vol, xlv_tab_t *xlv_tab);

/* NOTE: FOLLOWING 2 ROUTINEs are NOT reentrant !! */
extern char *devtopath (dev_t dev, int alias);
extern char *pathtovhpath (char *p);

extern void	xlv_error (char *format, ...);
extern int	xlv_encountered_errors (void);

struct Tcl_Interp;
extern void set_interp_result_str (struct Tcl_Interp *interp,
				   char *format, ...);
extern void set_interp_result (struct Tcl_Interp *interp, 
			       unsigned status_code, ...);
extern char *xlv_error_inq_text (unsigned status_code);


extern __int64_t xlv_tab_subvol_size (xlv_tab_subvol_t *subvol_entry);
extern int xlv_tab_subvol_depth (xlv_tab_subvol_t *subvol_entry);

extern int xlv_subvol_open_by_dev (dev_t dev,
				   char  *subvol_name,
				   char  *tmpdir,
				   int   flags);

#if (0)
extern int xlv_subvol_open (char *subvol_name, int flags);
#endif

typedef struct {
	__int64_t	start_blkno;
	__int64_t	end_blkno;
	int		plex_ve_idx[XLV_MAX_PLEXES];
} ve_table_entry_t;

extern int xlv_fill_subvol_ve_table (
	xlv_tab_subvol_t *svp,
	ve_table_entry_t *table,
	int		 max_entries);

/*
 * Functions to administer the disk partition structure.
 */
struct inventory_s;
extern void xlv_add_device(struct inventory_s *, char *, dev_t);

extern void xlv_fill_dp(xlv_tab_disk_part_t *dp, dev_t dev);


/*
 * Printing functions.
 */

#define	XLV_PRINT_BASIC	0x0
#define	XLV_PRINT_UUID	0x1
#define	XLV_PRINT_TIME	0x2
#define	XLV_PRINT_MASK	XLV_PRINT_UUID|XLV_PRINT_TIME
#define	XLV_PRINT_ALL	XLV_PRINT_MASK

extern void xlv_tab_print (xlv_tab_vol_t	*xlv_tab_vol,
			   xlv_tab_t		*xlv_tab,
			   int			p_mode);

extern void xlv_tab_vol_print (xlv_tab_vol_entry_t *vol_entry, int p_mode);
extern void xlv_tab_subvol_print (xlv_tab_subvol_t *subvol_entry, int p_mode);

struct xlv_vh_entry_s;
extern void xlv_print_vh (struct xlv_vh_entry_s *vh, int p_mode, int secondary);

/*
 * Syslog interface
 */
extern void xlv_openlog(const char *ident, int logstat, int logfac);
extern void xlv_closelog(void);

/*
 * Add a volume element to a plex. The plex structure is grown if
 * it cannot accommodate another volume element. The new volume
 * element is initialize to the "stale" state so it can be updated.
 *
 * If "ve_idx" is < 0 then the caller wants this function to calculate
 * the insertion point. This would be the case if there is only one
 * and there is no need to check for volume element size across rows.  *
 * Note: This assumes that the given volume element has been check
 * that it is the same size as other volume elements in the same
 * row in different plexes.
 *
 * Returns non-zero upon failure.
 */
#define	_E_VE_INVALID_IDX	-1	/* "ve_idx" is a invalid */
#define	_E_VE_OVERLAP		-2	/* overlaps with neighboring ve's */
#define	_E_VE_NO_MEMORY		-3	/* couldn't grow plex */
extern int xlv_add_ve_to_plex (
	xlv_tab_plex_t		**plexpp,
	xlv_tab_vol_elmnt_t	*vep,
	int			ve_idx);

/*
 * Extract the given volume element and compact the table.
 */
extern int xlv_rem_ve_from_plex(xlv_tab_plex_t *plexp, int ve_idx);

/*
 * Check to see if plexing is enabled.
 */
extern int check_plexing_license (char *root);


extern int xlv_filter_complete_vol(xlv_tab_vol_t *xlv_tab_vol, int quiet);
extern int xlv_filter_valid_vol(xlv_tab_vol_t *xlv_tab_vol, int quiet);
extern void xlv_remove_vol(xlv_tab_vol_t *tab_vol, int vol_idx);

extern int xlv_check_partition_type(int		subvol_type,
				    int		partition_type,
				    int		xlv_make_flag);

extern void xlv_print_type_check_msg(const char	*template,
				     char	*pathname,
				     const char	*reason,
				     dev_t	devnum,
				     dev_t	part,
				     const char	*force);
#endif /* __XLV_UTILS_H__ */
