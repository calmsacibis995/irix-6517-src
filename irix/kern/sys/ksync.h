#ifndef __KSYNC_H__
#define __KSYNC_H__

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include "sys/hwgraph.h"
#include "sys/sema.h"
#include "sys/types.h"

#if _MIPS_SIM == _ABI64  /* xlate functions used in this file */
#include <sys/xlate.h>
#endif 


/*
 * module: kitchen sync ( ksync ).  
 *
 * purpose: control "kitchen sync" routing.
 *
 *
 */

/*
 * Ksync IOCTL values
 */

/*
 * ksync public ioctl's
 */
#define KSYIOC           ('k'<<24|'s'<<16)
#define KSYIOCTL(x)      (KSYIOC|x)
#define KSYIOCTLN(x)     (x & 0xffff)

#define KSY_CONTROL			KSYIOCTL(1) 
#define KSY_STATUS			KSYIOCTL(2)	


/*
 * Notification messages
 */

#define KSYNC_MSG_SHIFT	 24
#define KSYNC_BAD  	 (0 << KSYNC_MSG_SHIFT)
#define KSYNC_GOOD 	 (1 << KSYNC_MSG_SHIFT)
#define KSYNC_NEW_RATE	 (2 << KSYNC_MSG_SHIFT) /* new rate (Hz) in ls 8 bits */
#define KSYNC_NEW_MASTER (3 << KSYNC_MSG_SHIFT)
  
#define KSYNC_MSG(x)	 ((x)>>KSYNC_MSG_SHIFT)

/*
 * For drivers to use in ksync_attach() and ksync_detach()
 */
typedef enum ksync_role_e { KSYNC_CONSUMER=0, 
                            KSYNC_PRODUCER=1,
			    KSYNC_BOTH=2,
			    KSYNC_NEUTRAL=3 } ksync_role_t;

/*
 * Argument to KSY_CONTROL
 */
typedef enum ksync_enable_e {  KSYNCOFF=0, KSYNCON=1 } ksync_enable_t;


/*
 * ksync flag values
 */
#define KsyncIsProducer         0x1
#define KsyncProducerCapable    0x2
#define KsyncConsumerCapable    0x4
#define KsyncActive             0x8

#define KSTAT_NAME_LEN 64
typedef char kstat_name_t[KSTAT_NAME_LEN];

/*
 * Arguement to ksyncstatus and KSY_STATUS ioctl
 */
typedef struct kstat_s {
       kstat_name_t    	kName;
       int        	kFlags;
} kstat_t; 


#if _MIPS_SIM == _ABI64 && defined(_KERNEL)
/* 
 * For copyin/copyout xlate 
 */
typedef struct _irix5_kitchen_sync_s {
	kstat_name_t  	kName;
	int 		kFlags;	
} _irix5_kitchen_sync_t; 


extern int kstat_t_to_irix5(void *from, int count, 
			    xlate_info_t *info);
extern int irix5_to_kstat_t(enum xlate_mode mode, void *to, 
			    int count, xlate_info_t *info);
#endif /* _KENREL */


/*
 * Initializes lock for setting up tree.
 */
extern void kitchen_sync_init(void);

/*
 * purpose:
 *   attaches driver to ksync system
 *
 * effect:
 *   creates branch off hardware graph containing state data 
 *   and link back to driver vertex
 */
extern int ksync_attach( vertex_hdl_t driver_vtx, 
			 char *name, 
			 ksync_role_t procon,
			 void (*notify_func)(int),
			 int module_id );
/*
 * purpose:
 *   detaches driver from ksync system
 * 
 * effect:
 *   removes node from graph.  
 *
 */
extern int ksync_detach( vertex_hdl_t *driver_vtx, 
			 char* name, 
			 ksync_role_t procon, 
			 int module_id );
/*
 * purpose:
 *   driver -> ksync: call on driver disruption of sync 
 *
 * effect:
 *   consumers get ksync_notify callback
 */
extern int ksync_notify( vertex_hdl_t driver_vtx, 
			 int msg, 
			 int module_id );

 /*
  * Vertex info Ksync consumer parent vertex info
  */
typedef struct ksync_consumer_parent_vtx_info_s {
	int parent_placeholder;
} ksync_consumer_parent_vtx_info_t;

 /* 
  * Ksync consumer vertex info
  */
typedef struct ksync_vtx_info_s {
	char	*device_name;
	void	(*notify_func)(int);
} ksync_consumer_vtx_info_t;


 /*
  * Ksync  producer vertex info
  */
typedef struct ksync_producer_vtx_info_s {
	int	is_generating;
	char	*device_name;
	int	rate;
	void	(*notify_func)(int);
} ksync_producer_vtx_info_t;
 

#endif /* __KSYNC_H__ */
