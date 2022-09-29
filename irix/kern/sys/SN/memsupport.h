/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1997, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __SYS_SN_MEMSUPPORT_H__
#define __SYS_SN_MEMSUPPORT_H__


extern void poison_state_clear(pfn_t pfn);
extern void page_allow_access(pfn_t pfn);

extern int page_read_accessible(pfn_t pfn);
extern int page_write_accessible(pfn_t pfn);
extern int page_local_calias(pfn_t pfn);
extern int page_iscalias(pfn_t pfn);

extern int page_dumpable(cnodeid_t node,pfn_t pfn);
#endif /* __SYS_SN_MEMSUPPORT_H__ */
