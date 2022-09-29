#ifndef _EVENT_H_
#define _EVENT_H_

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/* Global names
 */
#define evt_prompt	PFX_NAME(evt_prompt)
#define evt_fork_child	PFX_NAME(evt_fork_child)

extern void evt_prompt(void);
extern void evt_fork_child(void);

#endif /* !_EVENT_H_ */
