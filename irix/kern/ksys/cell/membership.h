
/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995-1996 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef	_KSYS_MEMBERSHIP_H_
#define	_KSYS_MEMBERSHIP_H_
#ident "$Revision: 1.4 $"

#include <ksys/cell/cell_set.h>
extern	cell_set_t cell_membership;

#define cell_in_membership(cell) \
		((cms_ignore_membership_check) ? 1 : \
		set_is_member(&cell_membership, cell))

extern	void cms_wait_for_initial_membership(void);
extern	void cms_leave_membership(void);
extern	int cms_ignore_membership_check;
extern	void cms_notify_cell(cell_t, boolean_t);
extern  void cms_detected_cell_failure(cell_t);
extern	void cms_mark_recovery_complete(void);

extern void cms_init(void);
#endif /* _KSYS_MEMBERSHIP_H_ */

