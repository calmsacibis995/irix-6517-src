/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __DEFS_H__
#define __DEFS_H__

/*
 * Max number of slaves in a system
 */
#define MAX_NUMBER_OF_SLAVES 36

/*
 * Get the cpu from an frs
 * Shouldn't ever do this really, but it's
 * a very simple to obtain a serializable handle.
 */

#define frs_getcpu(frs)  ((frs)->frs_info.cpu)


/*
 * pdesc structure for process control
 */

typedef struct pdesc {
	frs_t*    frs;
	uint      messages;
	int       (*action_function)(void);
} pdesc_t;

#ifdef MESSAGES_ON
#define  PDESC_MSG_JOIN   0x0001
#define  PDESC_MSG_YIELD  0x0002
#else
#define  PDESC_MSG_JOIN   0x0000
#define  PDESC_MSG_YIELD  0x0000
#endif

typedef struct enqdesc {
        int minor;
        uint disc;
} enqdesc_t;
        
        
extern void enqueue_server(frs_t* frs, int number_of_processes);
extern frs_t* enqueue_client(int cpu,
                             int seq,
                             int enqdesc_list_len,
                             enqdesc_t* enqdesc_list);
extern pdesc_t* pdesc_create(frs_t* frs, uint messages, int (*action_function)());

#endif /* __DEFS_H__ */
