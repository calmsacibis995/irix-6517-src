

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

#ifndef _KSYS_CELL_HB_H
#define _KSYS_CELL_HB_H

#include "ksys/cell/cell_set.h"

typedef enum { UP, DOWN } hb_state_t;

/* Min heart beat period in clock ticks*/
#define DEFAULT_MIN_POLL_PERIOD         HZ

/*
 * Flags to hb_get_cell_state.
 */
#define	HB_WAIT_POLL_PERIOD	1 	/* Wait for a poll period before  */
					/* getting cell state. 		  */

#define	hb_set_min_poll_period(x) \
		(hb_min_poll_period = (x))
/*
 * Exported kernel heart beat interfaces. Used by the driver or a kernel level
 * membership service.
 */
extern void     hb_init(void);
extern void     hb_update_local_heart_beat(void);
extern void 	hb_get_connectivity_set(cell_set_t *, cell_set_t *);

/*
 * Exported interfaces from the kernel heart beat subsystem.
 */
extern int      hb_add_cell(cell_t); /* Add a cell to be polling set */
extern int      hb_remove_cell(cell_t); /* Remove a cell to be polling set */
				/* Set the poll period for a given cell */
extern int      hb_set_cell_poll_period(cell_t, int /* poll period */); 
				/* Set heart beat global parameters */
extern int      hb_set_params(int /* Min polling period */); 

/* 
 * Inform heart beat if cell failure is detected.
 */
extern void 	hb_detected_cell_failure(cell_t);	


#endif /* _KSYS_CELL_HB_H */
