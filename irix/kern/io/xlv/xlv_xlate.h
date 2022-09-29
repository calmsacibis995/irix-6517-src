#ifndef __XLV_XLATE_H__
#define __XLV_XLATE_H__

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
#ident "$Revision: 1.2 $"

/*
 * Declarations for ABI copyin/copyout translation functions.
 */

/* don't do any explicit 32->64 or 64->32 bit conversions */
#undef to32bit
#define to32bit(p) ((__psint_t)p)
#undef to64bit
#define to64bit(p) ((__psint_t)p)

struct xlate_info_s;
extern int xlv_geom_to_irix5 (
	void *from, int count, struct xlate_info_s *info);

extern int xlv_tab_vol_entry_to_irix5 (
	void *from, int count, struct xlate_info_s *info);
extern int xlv_tab_subvol_to_irix5 (
	void *from, int count, struct xlate_info_s *info);
extern int xlv_tab_plex_to_irix5 (
	void *from, int count, struct xlate_info_s *info);

extern int irix5_to_xlv_tab_vol_entry (
	enum xlate_mode mode, void *to, int count, struct xlate_info_s *info);
extern int irix5_to_xlv_tab_subvol (
	enum xlate_mode mode, void *to, int count, struct xlate_info_s *info);
extern int irix5_to_xlv_tab_plex (
	enum xlate_mode mode, void *to, int count, struct xlate_info_s *info);

extern int irix5_to_xlv_plex_copy_param (
	enum xlate_mode mode, void *to, int count, struct xlate_info_s *info);

#endif /* __XLV_XLATE_H__ */
