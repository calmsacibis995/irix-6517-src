/**************************************************************************
 *                                                                        *
 *                  Copyright (C) 1995 Silicon Graphics, Inc.             *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 *    Ksync: Kitchen Sync system
 *
 *    This module encapsulates access by drivers to the ksync portion
 *    of the hw graph.  
 *
 *    There are two types of ksync devices: producers and consumers.
 *    Both types need to register their presence during driver attachment.
 *    They must un-register themselves during driver detachment.
 *    
 *    The hwgraph created by kysync_xxx will be used by applications
 *    via the libksync library, which will control switching of the
 *    ksync master, and notification of consumers when such an event
 *    occurs.
 *    
 */

#ident "$Revision: 1.3 $"

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/sema.h>
#include <sys/debug.h>
#include <sys/driver.h>
#include <sys/hwgraph.h>
#include <sys/iobus.h>
#include <sys/iograph.h>
#include <sys/ioerror.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/atomic_ops.h>
#include <sys/cdl.h>
#include <sys/errno.h>
#include <sys/ksync.h>
#if defined (SN0)
#include <sys/SN/klconfig.h>
#endif
int                     kitchen_sync_devflag = D_MP;

/*
 * technically only true on IP30 right now.  Other
 * machines may have different base layouts.
 * 
 */
#define KSY_PRODUCER_NAME "/producer"
#define KSY_CONSUMER_NAME "/consumer"

mutex_t ksync_graph_lock;  /* Ksync graph lock */

/* 
 * Public Interface 
 */
int ksync_attach( vertex_hdl_t   driver_vtx, 
		  char          *name, 
		  ksync_role_t   procon,
		  void         (*notify_func)(int), 
		  int            module_id );

int ksync_detach( vertex_hdl_t *driver_vtx, 
		  char         *name, 
		  ksync_role_t  procon, 
		  int           module_id );

int ksync_notify( vertex_hdl_t  driver_vtx, 
		  int           msg, 
		  int           module_id );

/*
 * Private methods
 */

static int _ksync_add( ksync_role_t procon, 
		       vertex_hdl_t driver_vtx,
                       char        *name,
		       void       (*notify_func)(int),
		       int         module_id );

static int _ksync_remove( ksync_role_t  procon, 
			  vertex_hdl_t *driver_vtx, 
			  char         *name,
			  int 	        module_id );

/*
 * Private Data
 */

#ifndef MAX_MODULES
#define MAX_MODULES 32
#endif

static vertex_hdl_t ksync_root_vertex[MAX_MODULES];
static vertex_hdl_t ksync_consumer_vertex[MAX_MODULES];
static vertex_hdl_t ksync_producer_vertex[MAX_MODULES];
static int state[MAX_MODULES];

void
kitchen_sync_init()
{
    mutex_init(&ksync_graph_lock,MUTEX_DEFAULT, "ksync graph lock");
}

void
ksync_root_name( char *buf, int module_id)
{
    char module_string[8];

#ifdef DEBUG
#if IP30
    if ( module_id >= 0 )
	cmn_err(CE_DEBUG,"KSync: For IP30, module_id must be -1!\n");
#endif
#if IP27
    if ( (module_id < 0) || (module_id > MAX_MODULE_ID) )
	cmn_err(CE_DEBUG,"KSync: Use a positive module id!\n");
#endif
#endif /* DEBUG */
    if ( module_id < 0 ) {
	strcpy(buf, "ksync" );
    }
    else {
	sprintf(module_string,"%d",module_id);
	strcpy(buf, "module/");
	strcat(buf, module_string);
	strcat(buf, "/ksync");
    }
}


/*
 * Name: _ksync_add
 *
 * Args:
 *   procon     - KSYNC_CONSUMER,KSYNC_PRODUCER,KSYNC_BOTH, or KSYNC_NEUTRAL
 *   driver_vtx - vertex handle of actal driver
 *   name       - name required for this node.  64 chars max.
 *  notify_func - func to call with notify message, or NULL
 *
 * Purpose: 
 *   Adds edge to ksync graph.  edge is an alias for device
 * whose node is driver_vtx.  Edge shows up under /hw/{producer|consumer}
 * as "name". 
 *
 * Returns
 *      0: success
 *  EBADF: driver_vtx is not a vertex
 *  ENXIO: if any of the graph operations fail
 *
 */
static int 
_ksync_add( ksync_role_t procon, 
	    vertex_hdl_t driver_vtx, 
	    char *name,
	    void (*notify_func)(int),
	    int module_id )
{
    graph_error_t             rc = GRAPH_SUCCESS;
    vertex_hdl_t              child_vtx;
    ksync_consumer_vtx_info_t *cons_ptr;
    ksync_producer_vtx_info_t *prod_ptr;
    vertex_hdl_t              parent_hdl;
    int 	  module_index = module_id;

#if IP30
    module_index = 0;
#endif


    if (!dev_is_vertex(driver_vtx) )
	return EBADF;

    /* set up entry 
     *  - get producer vertex
     *  - add child vertex
     *  - link child vertex to driver
     *  - put fastinfo on child vertex
     */

    /* producer entry */
    if ( (procon == KSYNC_PRODUCER) || (procon == KSYNC_BOTH) ) {
	parent_hdl = ksync_producer_vertex[module_index];

	/*
	 * If edge already exists, just exit.
	 * This may happen for a loadable driver which
	 * creates its entry in its attach routine. 
	 * It may also happen as an error if a 
	 * driver with multiple instances passess the
	 * same "name" string on each attach.
	 */
	if((rc = hwgraph_edge_get( parent_hdl, name, &child_vtx))
						    != GRAPH_SUCCESS )  {
	    rc = hwgraph_path_add(parent_hdl, name, &child_vtx);
	    hwgraph_chmod(child_vtx,0755);
	}
	else {
	    rc = ~GRAPH_SUCCESS;
	    return rc;
	}

	if(rc == GRAPH_SUCCESS) {
	    rc = hwgraph_edge_add(child_vtx,driver_vtx, name);
	    hwgraph_chmod(driver_vtx,0755);
	}

	if(rc == GRAPH_SUCCESS) {
	    prod_ptr = (ksync_producer_vtx_info_t *) 
			kern_malloc(sizeof(ksync_producer_vtx_info_t));
	    prod_ptr->device_name = (char *)kern_malloc(KSTAT_NAME_LEN); 
	    prod_ptr->is_generating = 0;                                 
	    prod_ptr->rate = 0;                                          
	    prod_ptr->notify_func = notify_func;                         
	    strncpy(prod_ptr->device_name,name,KSTAT_NAME_LEN);          
	    hwgraph_fastinfo_set( child_vtx, (arbitrary_info_t)prod_ptr );
	}
    }

    /* consumer entry */
    if ( rc == GRAPH_SUCCESS ) {
	if ( (procon == KSYNC_CONSUMER) || (procon == KSYNC_BOTH) ) {
	    parent_hdl = ksync_consumer_vertex[module_index];

	    /* see comment above */
	    if((rc = hwgraph_edge_get( parent_hdl, name, &child_vtx))
							!= GRAPH_SUCCESS ) {
		rc = hwgraph_path_add(parent_hdl, name, &child_vtx);
	    }
	    else {
		rc = ~GRAPH_SUCCESS;
		return rc;
	    }

	    if(rc == GRAPH_SUCCESS) {
		rc = hwgraph_edge_add(child_vtx,driver_vtx, name);
	    }

	    if(rc == GRAPH_SUCCESS) {
		cons_ptr = (ksync_consumer_vtx_info_t *) 
			    kern_malloc(sizeof(ksync_consumer_vtx_info_t));
		cons_ptr->device_name = (char *)kern_malloc(KSTAT_NAME_LEN); 
		cons_ptr->notify_func = notify_func;                         
		strncpy(cons_ptr->device_name,name,KSTAT_NAME_LEN);          
		hwgraph_fastinfo_set( child_vtx, (arbitrary_info_t)cons_ptr );
	    }
	}
    }

    if ( rc != GRAPH_SUCCESS )
	return ENXIO;
    else
	return 0;
}

/*
 * Name:
 *   _ksync_remove
 *
 * Args:
 *   procon     - KSYNC_CONSUMER,KSYNC_PRODUCER,KSYNC_BOTH, or KSYNC_NEUTRAL
 *   driver_vtx - pointer to vertex handle of actal driver
 *   name       - name of edge to remove
 *
 * Purpose:
 *   Removes an edge from the ksync graph.
 *
 * Returns:
 *      0: success
 *  EBADF: *driver_vtx is not a vertex
 *  ENXIO: if any of the graph operations fail
 */

static int 
_ksync_remove( ksync_role_t  procon, 
	       vertex_hdl_t *driver_vtx, 
	       char         *name, 
	       int           module_id )
{
    vertex_hdl_t parent_vhdl;
    vertex_hdl_t child_vhdl;
    graph_error_t prod_rc, cons_rc;
    char buf[128];              /* I hope this is big enough */
    graph_edge_place_t placeptr = EDGE_PLACE_WANT_REAL_EDGES;
    ksync_consumer_vtx_info_t *cons_ptr;
    ksync_producer_vtx_info_t *prod_ptr;


    if (!dev_is_vertex(*driver_vtx) )
        return EBADF;

    if (procon == KSYNC_PRODUCER || procon == KSYNC_BOTH ) {
	ksync_root_name(buf, module_id);
	strcat(buf,KSY_PRODUCER_NAME);
	parent_vhdl = hwgraph_path_to_vertex(buf);

	while ( (prod_rc = hwgraph_edge_get_next( parent_vhdl,
                           buf,
                           &child_vhdl,
                           &placeptr)) == GRAPH_SUCCESS ) {
	    if ( strstr( buf, name )  != NULL ) {
		prod_ptr = 
	      (ksync_producer_vtx_info_t *)hwgraph_fastinfo_get( child_vhdl );
		if ( prod_ptr ) {
		    kern_free(prod_ptr->device_name);
		    kern_free(prod_ptr);
		}
		hwgraph_fastinfo_set( child_vhdl, (arbitrary_info_t)NULL);
		hwgraph_edge_remove( parent_vhdl, name, 0 );
		hwgraph_edge_remove( child_vhdl, name, 0 ); 
	    }
	}
    }

    if (procon == KSYNC_CONSUMER || procon == KSYNC_BOTH ) {
	ksync_root_name(buf, module_id);
	strcat(buf,KSY_CONSUMER_NAME);
	parent_vhdl = hwgraph_path_to_vertex(buf);

	while ( (cons_rc = hwgraph_edge_get_next( parent_vhdl,
                           buf,
                           &child_vhdl,
                           &placeptr)) == GRAPH_SUCCESS ) {
	    if ( strstr( name, buf )  != NULL ) {
		cons_ptr = 
	      (ksync_consumer_vtx_info_t *)hwgraph_fastinfo_get( child_vhdl);
		hwgraph_edge_remove( child_vhdl, name, 0 );
		hwgraph_edge_remove( parent_vhdl, name, 0 );
		hwgraph_vertex_destroy(child_vhdl);
		if ( cons_ptr ) {
		    kern_free(cons_ptr->device_name);
		    kern_free(cons_ptr);
		}
	    }
	}
    }

    if ((prod_rc == GRAPH_SUCCESS)  && (cons_rc == GRAPH_SUCCESS))
	return 0;
    else
	return ENXIO;
}


/*
 * Name:
 *   ksync_attach
 *
 * Args:
 *   driver_vtx   - vertex handle of actual driver
 *   name         - name of edge to add
 *   procon       - KSYNC_CONSUMER,KSYNC_PRODUCER,KSYNC_BOTH, or KSYNC_NEUTRAL
 *   notify_func  - function to call on ksync_notify or NULL if not desired
 *
 * Purpose:
 *    Called by drivers to attach to the ksync graph.  On return, driver
 * will be in ksync inventory, and may send and receive ksync_notify
 * events if requested.
 *
 * Returns:
 *   0     - success
 *   EBADF - driver_vtx invalid  
 *   ENXIO - graph operations failed
 */
int
ksync_attach( vertex_hdl_t driver_vtx, 
	      char        *name, 
	      ksync_role_t procon,
	      void       (*notify_func)(int),
	      int          module_id )
{
    graph_error_t rc = GRAPH_SUCCESS;
    int           err;
    char	  ksync_root_buf[64];
    int 	  module_index;

#if IP30
    module_index = 0;
#else /* SN0 */
    module_index = module_id;
#endif

    if ( showconfig )
	cmn_err(CE_DEBUG,"ksync_attach: device %v(%s)\n",driver_vtx,name);

    if (!dev_is_vertex(driver_vtx) )
	return EBADF;

    /* First one in builds the root */
    mutex_enter(&ksync_graph_lock);

    if ( !state[module_index]) {
	state[module_index]++;
	ksync_root_name(ksync_root_buf, module_id);
	rc = hwgraph_path_add(hwgraph_root, ksync_root_buf, 
				&(ksync_root_vertex[module_index]));
	ASSERT(rc == GRAPH_SUCCESS);

	rc = hwgraph_path_add(ksync_root_vertex[module_index], "consumer", 
		&(ksync_consumer_vertex[module_index]));
	ASSERT(rc == GRAPH_SUCCESS);

	rc = hwgraph_path_add(ksync_root_vertex[module_index], "producer", 
		&(ksync_producer_vertex[module_index]));
	ASSERT(rc == GRAPH_SUCCESS);
    }
    mutex_exit(&ksync_graph_lock);

    if ( rc == GRAPH_SUCCESS )
	err = _ksync_add( procon, driver_vtx, name, notify_func, module_id );
    else
	err = ENXIO;

    return err;
}

/*
 * Name:
 *  ksync_detach
 *
 * Args:
 *   driver_vtx - pointer vertex handle of actual driver
 *   name       - name of edge to remove
 *   procon     - KSYNC_CONSUMER,KSYNC_PRODUCER,KSYNC_BOTH, or KSYNC_NEUTRAL
 *
 * Purpose:
 *    To remove driver from ksync inventory.
 *
 * Notes:
 *    Got to find ksync vertex by name and destroy it, 
 *    not the one it points to.
 *
 * Returns:
 *   0     - success
 *   EBADF - driver_vtx invalid  
 *   ENXIO - graph operations failed
 *
 */
int 
ksync_detach( vertex_hdl_t *driver_vtx, 
	      char         *name, 
	      ksync_role_t procon,
	      int          module_id )
{
    int err;

    if (!dev_is_vertex(*driver_vtx) )
	return EBADF;

    err = _ksync_remove( procon, driver_vtx, name, module_id );

    return err;
}

/*
 * Name:
 *   ksync_notify
 *
 * Args:
 *   driver_vtx - vertex handle of master driver
 *   msg        - integer messge 
 *
 * Purpose:
 *   to notify ksync consumers of change in ksync.  Each
 *   consumer who registered a callback at attach time
 *   will get called back with msg as arg.
 *
 * Returns:
 *   0     - success
 *   EBADF - driver_vtx invalid  
 *   ENXIO - graph operations failed
 */
int 
ksync_notify( vertex_hdl_t driver_vtx, int msg, int module_id )
{
    vertex_hdl_t parent_vhdl;
    vertex_hdl_t child_vhdl;
    char buf[128];		/* I hope this is big enough */
    graph_edge_place_t placeptr = EDGE_PLACE_WANT_REAL_EDGES;
    int err = 0;
    ksync_consumer_vtx_info_t *cons_ptr;
    ksync_producer_vtx_info_t *prod_ptr;


    if (!dev_is_vertex(driver_vtx) )
	return EBADF;

    ksync_root_name(buf, module_id);
    strcat(buf,KSY_CONSUMER_NAME);
    parent_vhdl = hwgraph_path_to_vertex(buf);

    while ( hwgraph_edge_get_next( parent_vhdl,
			   buf,
			   &child_vhdl,
			   &placeptr) == GRAPH_SUCCESS ) {
	cons_ptr = 
	    ( ksync_consumer_vtx_info_t *)hwgraph_fastinfo_get( child_vhdl );
	if ( cons_ptr && cons_ptr->notify_func )
	    (*(cons_ptr->notify_func))(msg);

    }

    ksync_root_name(buf, module_id);
    strcat(buf,KSY_PRODUCER_NAME);
    parent_vhdl = hwgraph_path_to_vertex(buf);

    while ( hwgraph_edge_get_next( parent_vhdl,
                           buf,
                           &child_vhdl,
                           &placeptr) == GRAPH_SUCCESS ) {
        prod_ptr =
            ( ksync_producer_vtx_info_t *)hwgraph_fastinfo_get( child_vhdl );
        if ( prod_ptr && prod_ptr->notify_func )
            (*(prod_ptr->notify_func))(msg);

    }

    return err;
}


#if _MIPS_SIM == _ABI64  /* xlate functions used in this file */

/*
 * Name:
 *    irix5_to_kstat_t
 *
 * Args:
 *   enum xlate_mode mode  - abi mode
 *   void *to, 		   - target structure ( kernel kstat_t )
 *   int count             - size of kernel structure
 *   xlate_info_t *info    - translation struct
 *
 * Purpose:
 *   To convert 32-bit kstat_t structs to 64-bit kstat_t structs.
 *   The name string is copied in.
 *
 * Returns:
 *     0
 *
 */

/* ARGSUSED */
int
irix5_to_kstat_t(
    enum xlate_mode mode, 
    void *to, 
    int count, 
    xlate_info_t *info)
{
        COPYIN_XLATE_PROLOGUE(_irix5_kitchen_sync_s, kstat_s);

        bcopy(target->kName,source->kName,KSTAT_NAME_LEN);
        target->kFlags = source->kFlags;

        return 0;
}

/*
 * Name:
 *    kstat_t_to_irix5
 *
 * Args:
 *   void *from         -   address of struct being translated
 *   int count          -   size of target struct
 *   xlate_info_t *info -   xlation info struct
 *
 * Purpose:
 *   To convert 64-bit kstat_t structs to 32-bit kstat_t structs.
 *   The name string is copied out.
 *
 * Returns:
 *     0
 */

/* ARGSUSED */
int
kstat_t_to_irix5(void *from, 
		 int count, 
		 xlate_info_t *info)
{
        XLATE_COPYOUT_PROLOGUE(kstat_s,_irix5_kitchen_sync_s);

        bcopy(source->kName,target->kName,KSTAT_NAME_LEN);

        target->kFlags = source->kFlags;

        return 0;
}

#endif /* _MIPS_SIM */
