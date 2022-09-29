#ident "$Revision: 1.10 $"

/**************************************************************************
 *									  *
 *            Copyright (C) 1993-1994, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


#include <sys/types.h>
#include <sys/systm.h>
#include <sys/buf.h>

#include <sys/debug.h>
#include <sys/kabi.h>
#include <sys/xlate.h>

#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>

#include "xlv_ioctx.h"
#include "xlv_procs.h"
#include "xlv_xlate.h"

/*
 * These routines are used with copyin_xlate() to copy a xlv structure
 * from a 32 bit user space application to a 64 bit kernel.
 */

#if (_MIPS_SIM == _ABI64)
/*ARGSUSED*/
int
irix5_to_xlv_tab_vol_entry (
	 enum xlate_mode mode,
	 void		 *to,
	 int		 count,
	 xlate_info_t	 *info)
{
	COPYIN_XLATE_PROLOGUE (irix5_xlv_tab_vol_entry_s, xlv_tab_vol_entry_s);

	target->version = source->version;
	target->uuid = source->uuid;
	bcopy(source->name, target->name, sizeof(xlv_name_t));

	target->flags = source->flags;
	target->state = source->state;
	target->rsvd = source->rsvd;
	target->sector_size = source->sector_size;

	target->log_subvol = (struct xlv_tab_subvol_s *)
		to64bit((__psint_t)source->log_subvol);
	target->data_subvol = (struct xlv_tab_subvol_s *)
		to64bit((__psint_t)source->data_subvol);
	target->rt_subvol = (struct xlv_tab_subvol_s *)
		to64bit((__psint_t)source->rt_subvol);

	target->nodename = (void *)to64bit((__psint_t)source->nodename);

	return (0);
}

/*ARGSUSED*/
static int
irix5_o32_to_64_xlv_tab_subvol (
	enum xlate_mode	mode,
	void		*to,
	int		count,
	xlate_info_t	*info)
{
	int i;
	COPYIN_XLATE_PROLOGUE ( irix5_o32_xlv_tab_subvol_s, xlv_tab_subvol_s );

	target->version = source->version;
	target->flags = source->flags;
	target->open_flag = source->open_flag;
	target->vol_p = (struct xlv_tab_vol_entry_s *)
		to64bit((__psint_t) source->vol_p);
	target->uuid = source->uuid;
	target->subvol_type = source->subvol_type;
	target->subvol_depth = source->subvol_depth;
	target->subvol_size = source->subvol_size;
	target->dev = source->dev;

	target->critical_region.excluded_rqst = (caddr_t)
		to64bit((__psint_t) source->critical_region.excluded_rqst);
	target->critical_region.start_blkno =
		source->critical_region.start_blkno;
	target->critical_region.end_blkno =
		source->critical_region.end_blkno;
	target->critical_region.pending_requests = (struct buf *)
		to64bit((__psint_t) source->critical_region.pending_requests);

	target->err_pending_rqst = (struct buf *)
		to64bit((__psint_t) source->err_pending_rqst);
	target->rd_wr_back_start = source->rd_wr_back_start;
	target->rd_wr_back_end = source->rd_wr_back_end;
	target->initial_read_slice = source->initial_read_slice;
	target->num_plexes = source->num_plexes;

	for ( i = 0; i < XLV_MAX_PLEXES; i++) {
		target->plex[i] = (xlv_tab_plex_t *)
			to64bit((__psint_t) source->plex[i]);
	}
	
	target->block_map = (xlv_block_map_t *)
		to64bit((__psint_t) source->block_map);

	return( 0 );
}


/*ARGSUSED*/
static int
irix5_n32_to_64_xlv_tab_subvol (
	enum xlate_mode	mode,
	void		*to,
	int		count,
	xlate_info_t	*info)
{
	int i;
	COPYIN_XLATE_PROLOGUE(irix5_n32_xlv_tab_subvol_s, xlv_tab_subvol_s);

	target->version = source->version;
	target->flags = source->flags;
	target->open_flag = source->open_flag;
	target->vol_p = (struct xlv_tab_vol_entry_s *)
		to64bit((__psint_t) source->vol_p);
	target->uuid = source->uuid;
	target->subvol_type = source->subvol_type;
	target->subvol_depth = source->subvol_depth;
	target->subvol_size = source->subvol_size;
	target->dev = source->dev;

	target->critical_region.excluded_rqst = (caddr_t)
		to64bit((__psint_t) source->critical_region.excluded_rqst);
	target->critical_region.start_blkno =
		source->critical_region.start_blkno;
	target->critical_region.end_blkno =
		source->critical_region.end_blkno;
	target->critical_region.pending_requests = (struct buf *)
		to64bit((__psint_t) source->critical_region.pending_requests);

	target->err_pending_rqst = (struct buf *)
		to64bit((__psint_t) source->err_pending_rqst);
	target->rd_wr_back_start = source->rd_wr_back_start;
	target->rd_wr_back_end = source->rd_wr_back_end;
	target->initial_read_slice = source->initial_read_slice;
	target->num_plexes = source->num_plexes;

	for ( i = 0; i < XLV_MAX_PLEXES; i++) {
		target->plex[i] = (xlv_tab_plex_t *)
			to64bit((__psint_t) source->plex[i]);
	}
	
	target->block_map = (xlv_block_map_t *)
		to64bit((__psint_t) source->block_map);

	return( 0 );
}

#elif (_MIPS_SIM == _ABIN32)
/*ARGSUSED*/
static int
irix5_o32_to_n32_xlv_tab_subvol (
	enum xlate_mode	mode,
	void		*to,
	int		count,
	xlate_info_t	*info)
{
	int i;
	COPYIN_XLATE_PROLOGUE ( irix5_o32_xlv_tab_subvol_s, xlv_tab_subvol_s );

	target->version = source->version;
	target->flags = source->flags;
	target->open_flag = source->open_flag;
	target->vol_p = (struct xlv_tab_vol_entry_s *)source->vol_p;
	target->uuid = source->uuid;
	target->subvol_type = source->subvol_type;
	target->subvol_depth = source->subvol_depth;
	target->subvol_size = source->subvol_size;
	target->dev = source->dev;

	target->critical_region.excluded_rqst = (caddr_t)
		source->critical_region.excluded_rqst;
	target->critical_region.start_blkno =
		source->critical_region.start_blkno;
	target->critical_region.end_blkno =
		source->critical_region.end_blkno;
	target->critical_region.pending_requests = (struct buf *)
		source->critical_region.pending_requests;

	target->err_pending_rqst = (struct buf *)source->err_pending_rqst;
	target->rd_wr_back_start = source->rd_wr_back_start;
	target->rd_wr_back_end = source->rd_wr_back_end;
	target->initial_read_slice = source->initial_read_slice;
	target->num_plexes = source->num_plexes;

	for ( i = 0; i < XLV_MAX_PLEXES; i++) {
		target->plex[i] = (xlv_tab_plex_t *)source->plex[i];
	}
	
	target->block_map = (xlv_block_map_t *)source->block_map;

	return( 0 );
}

#elif (_MIPS_SIM == _ABIO32)

/*ARGSUSED*/
static int
irix5_n32_to_o32_xlv_tab_subvol (
	enum xlate_mode	mode,
	void		*to,
	int		count,
	xlate_info_t	*info)
{
	int i;
	COPYIN_XLATE_PROLOGUE ( irix5_n32_xlv_tab_subvol_s, xlv_tab_subvol_s );

	target->version = source->version;
	target->flags = source->flags;
	target->open_flag = source->open_flag;
	target->vol_p = (struct xlv_tab_vol_entry_s *)source->vol_p;
	target->uuid = source->uuid;
	target->subvol_type = source->subvol_type;
	target->subvol_depth = source->subvol_depth;
	target->subvol_size = source->subvol_size;
	target->dev = source->dev;

	target->critical_region.excluded_rqst = (caddr_t)
		source->critical_region.excluded_rqst;
	target->critical_region.start_blkno =
		source->critical_region.start_blkno;
	target->critical_region.end_blkno =
		source->critical_region.end_blkno;
	target->critical_region.pending_requests = (struct buf *)
		source->critical_region.pending_requests;

	target->err_pending_rqst = (struct buf *)source->err_pending_rqst;
	target->rd_wr_back_start = source->rd_wr_back_start;
	target->rd_wr_back_end = source->rd_wr_back_end;
	target->initial_read_slice = source->initial_read_slice;
	target->num_plexes = source->num_plexes;

	for ( i = 0; i < XLV_MAX_PLEXES; i++) {
		target->plex[i] = (xlv_tab_plex_t *)source->plex[i];
	}
	
	target->block_map = (xlv_block_map_t *)source->block_map;

	return( 0 );
}
#endif


#if (_MIPS_SIM == _ABI64)
int
irix5_to_xlv_tab_subvol (
	enum xlate_mode	mode,
	void		*to,
	int		count,
	xlate_info_t	*info)
{
	ASSERT(ABI_IS_IRIX5_N32(info->abi) || ABI_IS_IRIX5(info->abi));
	if (ABI_IS_IRIX5_N32(info->abi))
		return irix5_n32_to_64_xlv_tab_subvol(mode, to, count, info);
	else
		return irix5_o32_to_64_xlv_tab_subvol(mode, to, count, info);
}
#elif (_MIPS_SIM == _ABIN32)
int
irix5_to_xlv_tab_subvol (
	enum xlate_mode	mode,
	void		*to,
	int		count,
	xlate_info_t	*info)
{
	ASSERT(ABI_IS_IRIX5(info->abi));
	return irix5_o32_to_n32_xlv_tab_subvol(mode, to, count, info);
}
#elif (_MIPS_SIM == _ABIO32)
int
irix5_to_xlv_tab_subvol (
	enum xlate_mode	mode,
	void		*to,
	int		count,
	xlate_info_t	*info)
{
	ASSERT(ABI_IS_IRIX5_N32(info->abi));
	return irix5_n32_to_o32_xlv_tab_subvol(mode, to, count, info);
}
#endif

#if (_MIPS_SIM == _ABI64)
int
irix5_to_xlv_tab_plex (
        enum xlate_mode mode,
        void            *to,
        int             count,
        xlate_info_t    *info)
{
	int		i = 0;
	
        COPYIN_XLATE_VARYING_PROLOGUE(irix5_xlv_tab_plex_s, xlv_tab_plex_s, 
		sizeof(irix5_xlv_tab_plex_t) +
                ((count-1)*sizeof(xlv_tab_vol_elmnt_t)));
	
	ASSERT(count > 0);
	ASSERT(count == 1 || count <= source->max_vol_elmnts);

        target->flags = source->flags;
	target->uuid = source->uuid;
	bcopy(source->name, target->name, sizeof(xlv_name_t));
	target->num_vol_elmnts = source->num_vol_elmnts;
	target->max_vol_elmnts = source->max_vol_elmnts;
	
	/*
	 * XXX Should count or num_vol_elmnts be used?
	 */
	while (count) {
		target->vol_elmnts[i] = source->vol_elmnts[i];
		i++;
		count--;
	}
        return( 0 );
}
#endif	/* _ABI64 */


/*ARGSUSED*/
static int
irix5_o32_to_xlv_plex_copy_param (
	enum xlate_mode	mode,
	void		*to,
	int		count,
	xlate_info_t	*info)
{
	COPYIN_XLATE_PROLOGUE (
		irix5_o32_xlv_plex_copy_param_s, xlv_plex_copy_param_s );

	target->version = source->version;
	target->start_blkno = source->start_blkno;
	target->end_blkno = source->end_blkno;
	target->chunksize = source->chunksize;
	target->sleep_intvl_msec = source->sleep_intvl_msec;

	return( 0 );
}

/*ARGSUSED*/
static int
irix5_n32_to_xlv_plex_copy_param (
	enum xlate_mode	mode,
	void		*to,
	int		count,
	xlate_info_t	*info)
{
	COPYIN_XLATE_PROLOGUE (
		irix5_n32_xlv_plex_copy_param_s, xlv_plex_copy_param_s );

	target->version = source->version;
	target->start_blkno = source->start_blkno;
	target->end_blkno = source->end_blkno;
	target->chunksize = source->chunksize;
	target->sleep_intvl_msec = source->sleep_intvl_msec;

	return( 0 );
}

int
irix5_to_xlv_plex_copy_param (
	enum xlate_mode	mode,
	void		*to,
	int		count,
	xlate_info_t	*info)
{
	ASSERT(ABI_IS_IRIX5_N32(info->abi) || ABI_IS_IRIX5(info->abi));
	if (ABI_IS_IRIX5_N32(info->abi))
		return irix5_n32_to_xlv_plex_copy_param(mode, to, count, info);
	else
		return irix5_o32_to_xlv_plex_copy_param(mode, to, count, info);
}


/*
 * These routines are used with xlate_copyout() to copy a xlv structure
 * from a 64 bit (or n32) kernel to a 32 bit user space application.
 */

/*ARGSUSED*/
static int
xlv_geom_to_irix5_o32 (
	void			*from,
	int			count,
	register xlate_info_t	*info)
{
	XLATE_COPYOUT_PROLOGUE (
		xlv_disk_geometry_s, irix5_o32_xlv_disk_geometry_s );

	ASSERT(count == 1);

	target->version 	= source->version;
	target->subvol_size	= source->subvol_size;
	target->trk_size	= source->trk_size;

	return( 0 );
}

/*ARGSUSED*/
static int
xlv_geom_to_irix5_n32 (
	void			*from,
	int			count,
	register xlate_info_t	*info)
{
	XLATE_COPYOUT_PROLOGUE (
		xlv_disk_geometry_s, irix5_n32_xlv_disk_geometry_s );

	ASSERT(count == 1);

	target->version 	= source->version;
	target->subvol_size	= source->subvol_size;
	target->trk_size	= source->trk_size;

	return( 0 );
}

int
xlv_geom_to_irix5 (
	void			*from,
	int			count,
	register xlate_info_t	*info)
{
	ASSERT(ABI_IS_IRIX5_N32(info->abi) || ABI_IS_IRIX5(info->abi));
	if (ABI_IS_IRIX5_N32(info->abi))
		return xlv_geom_to_irix5_n32(from, count, info);
	else
		return xlv_geom_to_irix5_o32(from, count, info);
}



#if (_MIPS_SIM == _ABI64)
/*ARGSUSED*/
int
xlv_tab_subvol_to_irix5_o32 (
	void			*from,
	int			count,
	register xlate_info_t	*info)
{
	int i;
	XLATE_COPYOUT_PROLOGUE (xlv_tab_subvol_s, irix5_o32_xlv_tab_subvol_s);

	ASSERT(count == 1);

	target->version = source->version;
	target->flags = source->flags;
	target->open_flag = source->open_flag;
	target->vol_p = (__uint32_t) to32bit((__psint_t) source->vol_p);
	target->uuid = source->uuid;
	target->subvol_type = source->subvol_type;
	target->subvol_depth = source->subvol_depth;
	target->subvol_size = source->subvol_size;
	target->dev = source->dev;

	target->critical_region.excluded_rqst =
		(__uint32_t)to32bit((__psint_t)
			source->critical_region.excluded_rqst);
	target->critical_region.start_blkno =
		source->critical_region.start_blkno;

	target->critical_region.end_blkno =
		source->critical_region.end_blkno;

	target->critical_region.pending_requests =
		(__uint32_t)to32bit((__psint_t)
			source->critical_region.pending_requests);

	target->err_pending_rqst =
		(__uint32_t)to32bit((__psint_t) source->err_pending_rqst);

	target->rd_wr_back_start = source->rd_wr_back_start;
	target->rd_wr_back_end = source->rd_wr_back_end;
	target->initial_read_slice = source->initial_read_slice;
	target->num_plexes = source->num_plexes;

	/*
	 * Translate the 32 bits plex pointers but the plexes
	 * themselves are not translated.
	 */
	for (i = 0 ; i < XLV_MAX_PLEXES; i++) {
		target->plex[i] =
			(__uint32_t)to32bit((__psint_t)source->plex[i]);
	}
	
	target->block_map = (__uint32_t)to32bit((__psint_t)source->block_map);

	return (0);
} /* end of xlv_tab_subvol_to_irix5_o32() */

/*ARGSUSED*/
int
xlv_tab_subvol_to_irix5_n32 (
	void			*from,
	int			count,
	register xlate_info_t	*info)
{
	int i;
	XLATE_COPYOUT_PROLOGUE (xlv_tab_subvol_s, irix5_n32_xlv_tab_subvol_s);

	ASSERT(count == 1);

	target->version = source->version;
	target->flags = source->flags;
	target->open_flag = source->open_flag;
	target->vol_p = (__uint32_t) to32bit((__psint_t) source->vol_p);
	target->uuid = source->uuid;
	target->subvol_type = source->subvol_type;
	target->subvol_depth = source->subvol_depth;
	target->subvol_size = source->subvol_size;
	target->dev = source->dev;

	target->critical_region.excluded_rqst =
		(__uint32_t)to32bit((__psint_t)
			source->critical_region.excluded_rqst);
	target->critical_region.start_blkno =
		source->critical_region.start_blkno;

	target->critical_region.end_blkno =
		source->critical_region.end_blkno;

	target->critical_region.pending_requests =
		(__uint32_t)to32bit((__psint_t)
			source->critical_region.pending_requests);

	target->err_pending_rqst =
		(__uint32_t)to32bit((__psint_t) source->err_pending_rqst);

	target->rd_wr_back_start = source->rd_wr_back_start;
	target->rd_wr_back_end = source->rd_wr_back_end;
	target->initial_read_slice = source->initial_read_slice;
	target->num_plexes = source->num_plexes;

	/*
	 * Translate the 32 bits plex pointers but the plexes
	 * themselves are not translated.
	 */
	for (i = 0 ; i < XLV_MAX_PLEXES; i++) {
		target->plex[i] =
			(__uint32_t)to32bit((__psint_t)source->plex[i]);
	}
	
	target->block_map = (__uint32_t)to32bit((__psint_t)source->block_map);

	return (0);
} /* end of xlv_tab_subvol_to_irix5_n32() */

int
xlv_tab_subvol_to_irix5 (
	void			*from,
	int			count,
	register xlate_info_t	*info)
{
	ASSERT(ABI_IS_IRIX5_N32(info->abi) || ABI_IS_IRIX5(info->abi));
	if (ABI_IS_IRIX5_N32(info->abi))
		return xlv_tab_subvol_to_irix5_n32(from, count, info);
	else
		return xlv_tab_subvol_to_irix5_o32(from, count, info);
}

#elif (_MIPS_SIM == _ABIN32)
/*ARGSUSED*/
int
xlv_tab_subvol_n32_to_irix5_o32 (
	void			*from,
	int			count,
	register xlate_info_t	*info)
{
	int i;
	XLATE_COPYOUT_PROLOGUE (xlv_tab_subvol_s, irix5_o32_xlv_tab_subvol_s);

	ASSERT(count == 1);

	target->version = source->version;
	target->flags = source->flags;
	target->open_flag = source->open_flag;
	target->vol_p = (app32_ptr_t)source->vol_p;
	target->uuid = source->uuid;
	target->subvol_type = source->subvol_type;
	target->subvol_depth = source->subvol_depth;
	target->subvol_size = source->subvol_size;
	target->dev = source->dev;

	target->critical_region.excluded_rqst =
		(app32_ptr_t)source->critical_region.excluded_rqst;
	target->critical_region.start_blkno =
		source->critical_region.start_blkno;

	target->critical_region.end_blkno =
		source->critical_region.end_blkno;

	target->critical_region.pending_requests =
		(app32_ptr_t)source->critical_region.pending_requests;

	target->err_pending_rqst = (app32_ptr_t)source->err_pending_rqst;

	target->rd_wr_back_start = source->rd_wr_back_start;
	target->rd_wr_back_end = source->rd_wr_back_end;
	target->initial_read_slice = source->initial_read_slice;
	target->num_plexes = source->num_plexes;

	/*
	 * Translate the 32 bits plex pointers but the plexes
	 * themselves are not translated.
	 */
	for (i = 0 ; i < XLV_MAX_PLEXES; i++) {
		target->plex[i] = (app32_ptr_t)source->plex[i];
	}
	
	target->block_map = (app32_ptr_t)source->block_map;

	return (0);
}

int
xlv_tab_subvol_to_irix5 (
	void			*from,
	int			count,
	register xlate_info_t	*info)
{
	ASSERT(ABI_IS_IRIX5(info->abi));
	return xlv_tab_subvol_n32_to_irix5_o32(from, count, info);
}

#elif (_MIPS_SIM == _ABIO32)
/*ARGSUSED*/
int
xlv_tab_subvol_o32_to_irix5_n32 (
	void			*from,
	int			count,
	register xlate_info_t	*info)
{
	int i;
	XLATE_COPYOUT_PROLOGUE (xlv_tab_subvol_s, irix5_n32_xlv_tab_subvol_s);

	ASSERT(count == 1);

	target->version = source->version;
	target->flags = source->flags;
	target->open_flag = source->open_flag;
	target->vol_p = (app32_ptr_t)source->vol_p;
	target->uuid = source->uuid;
	target->subvol_type = source->subvol_type;
	target->subvol_depth = source->subvol_depth;
	target->subvol_size = source->subvol_size;
	target->dev = source->dev;

	target->critical_region.excluded_rqst =
		(app32_ptr_t)source->critical_region.excluded_rqst;
	target->critical_region.start_blkno =
		source->critical_region.start_blkno;

	target->critical_region.end_blkno =
		source->critical_region.end_blkno;

	target->critical_region.pending_requests =
		(app32_ptr_t)source->critical_region.pending_requests;

	target->err_pending_rqst = (app32_ptr_t)source->err_pending_rqst;

	target->rd_wr_back_start = source->rd_wr_back_start;
	target->rd_wr_back_end = source->rd_wr_back_end;
	target->initial_read_slice = source->initial_read_slice;
	target->num_plexes = source->num_plexes;

	/*
	 * Translate the 32 bits plex pointers but the plexes
	 * themselves are not translated.
	 */
	for (i = 0 ; i < XLV_MAX_PLEXES; i++) {
		target->plex[i] = (app32_ptr_t)source->plex[i];
	}
	
	target->block_map = (app32_ptr_t)source->block_map;

	return (0);
} /* end of xlv_tab_subvol_o32_to_irix5_n32() */

int
xlv_tab_subvol_to_irix5 (
	void			*from,
	int			count,
	register xlate_info_t	*info)
{
	ASSERT(ABI_IS_IRIX5_N32(info->abi));
	return xlv_tab_subvol_o32_to_irix5_n32(from, count, info);
}
#endif


#if (_MIPS_SIM == _ABI64)
int
xlv_tab_plex_to_irix5 (
	void			*from,
	int			count,
	register xlate_info_t	*info)
{
	int i;

	XLATE_COPYOUT_VARYING_PROLOGUE (
		xlv_tab_plex_s, irix5_xlv_tab_plex_s,
		sizeof(irix5_xlv_tab_plex_t) +
			((count-1)*sizeof(xlv_tab_vol_elmnt_t)));

        ASSERT(count > 0);
	ASSERT(count == 1 || count <= source->max_vol_elmnts);

	target->flags = source->flags;
	bcopy(source->name, target->name, sizeof(xlv_name_t));
	target->uuid = source->uuid;
	target->num_vol_elmnts = source->num_vol_elmnts;
	target->max_vol_elmnts = source->max_vol_elmnts;

	/*
	 * XXX Should this be count or source->num_vol_elmnts?
	 */
	for ( i = 0; i < count; i++) {
		target->vol_elmnts[i] = source->vol_elmnts[i];
	}

	return( 0 );
}


/*ARGSUSED1*/
int
xlv_tab_vol_entry_to_irix5 ( 
	void			*from,
	int			count,
	register xlate_info_t	*info)
{
	XLATE_COPYOUT_PROLOGUE(xlv_tab_vol_entry_s, irix5_xlv_tab_vol_entry_s);

	target->version = source->version;

	target->uuid = source->uuid;
	bcopy(source->name, target->name, sizeof(xlv_name_t));

	target->flags = source->flags;
	target->state = source->state;
	target->rsvd = source->rsvd;
	target->sector_size = source->sector_size;

	target->log_subvol =
		(__uint32_t)to32bit((__psint_t)source->log_subvol);
	target->data_subvol =
		(__uint32_t)to32bit((__psint_t)source->data_subvol);
	target->rt_subvol =
		(__uint32_t)to32bit((__psint_t)source->rt_subvol);

	target->nodename =
		(__uint32_t)to32bit((__psint_t)source->nodename);

	return(0);
}
#endif	/* _ABI64 */
