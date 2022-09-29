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

/*
 * This the is the kernel header for datapipes.  Nothing in here will
 * be visible to the end user.  Some of this stuff will have to be
 * visible to libraries involved with datapipe.  (GL, Datapipe LIB)
 *
 */


#ifndef _DPIPE_SYS_H
#define _DPIPE_SYS_H

#ident "$Revision: 1.20 $" 

#include <sys/types.h>


typedef __int64_t  dpipe_transfer_id_t;
typedef __uint64_t dpipe_end_trans_ctx_hdl_t;

/* 
 * flags for dpipe_end_role_flags
 */
#define DPIPE_SINK_CAPABLE              0x0001
#define DPIPE_SOURCE_CAPABLE            0x0002

#ifdef _KERNEL

/* 
 * flags for dpipe_end_bus_type_flags 
 */
#define DPIPE_MASTER            0x0001
#define DPIPE_SLAVE             0x0002
#define DPIPE_MASTER_PREFERRED  0x0004
#define DPIPE_SLAVE_PREFERRED   0x0008

/*
 *  Flag(s) for and dpipe_end_conn_dma_flags dpipe_conn_dma_flags
 */
#define DPIPE_TIMEDOMAIN      0x0001  /* Some latency guarantee? */
#define DPIPE_PARTIAL_BLOCK   0x0002  /* A block device master that deals with
                                         partial blocks */
#define DPIPE_TRAILING_DATA   0x0004  /* The slave can accept extra data at the 
                                         end of a transfer */
#define DPIPE_PRECEEDING_DATA 0x0008  /* The slave can accept extra data at the
                                         beginning of a transfer (unlikely) */
#define DPIPE_SEQUENTIAL      0x0010  /* Data must arrive at the slave "in 
                                         order" */

/*
 *  Flag(s) for the protocol word in both the end_attr and conn_attr struct.
 * (Maybe one bit per protocol is sufficient?)
 */

#define DPIPE_SIMPLE_PROTOCOL 0x0001

/*
 * The dpipe_end_quantum is the preferred transfer length.
 */

/* Most of the types really should be explict types */
typedef struct dpipe_end_attr_s {
    __uint32_t                    dpipe_end_bus_type_flags;
    __uint32_t                    dpipe_end_role_flags;   /* src or sink*/

    int                          dpipe_end_quantum;
    __uint32_t                    dpipe_end_max_len_per_dest;
    int                          dpipe_end_max_num_of_dest;
    int                          dpipe_end_block_size;
    int                          dpipe_end_alignment;
    int                          dpipe_end_dma_flags;
    int                          dpipe_end_protocol_flags;
   
} dpipe_end_attr_t;

/* 
 * Flags for dpipe_conn_status_flags
 */
#define DPIPE_UNCONNECTED 0x0001
#define DPIPE_CONNECTED   0x0010

typedef struct dpipe_conn_attr_s {
    int                           dpipe_id; /*Unique int per connection*/
    /* The set of attributes that the pipe driver has dictated for the
       connection btw the 2 pipe ends */
    int                           dpipe_conn_quantum; 
    int                           dpipe_conn_max_len_per_dest;
    int                           dpipe_conn_max_num_of_dest;
    int                           dpipe_conn_block_size;
    int                           dpipe_conn_alignment;
    int                           dpipe_conn_dma_flags;
    int                           dpipe_conn_protocol_flags;
                           
    int                           dpipe_conn_status_flags;
} dpipe_conn_attr_t;

#include <sys/uio.h>

typedef struct dpipe_dest_elem_s {
    __uint64_t       dpipe_addr;
    __uint32_t       dpipe_len;
} dpipe_dest_elem_t;

typedef struct dpipe_slave_dest_s {
    uio_seg_t                 dpipe_segflg;       /* What kind of addresses are these */
    dpipe_dest_elem_t        *dpipe_dest_array;   /* Array of destinations */
    int                       dpipe_dest_count;   /* Number of iovecs */
} dpipe_slave_dest_t;

/* This is a structure that a block-dev master uses to notify a slave
   of whether starting or trailing data needs be ignored*/

/* Non-block devices such as gfx can ignore references to this
   struct*/

typedef struct dpipe_pad_info_s {
    __uint32_t      dpipe_start_partial_len;
    __uint32_t      dpipe_end_partial_len;
} dpipe_pad_info_t;


/*
 * Operations supported by pipe_ends
 */

/* Return values for simple protocol ops */
#define DPIPE_SP_OK        0
#define DPIPE_SP_ERROR     -1
#define DPIPE_SP_BROKEN    -2

/* Functions for the SIMPLE protocol */
typedef struct dpipe_end_simple_protocol_ops {
    int         (*dpipe_slave_prepare)(dpipe_transfer_id_t,
				       dpipe_end_trans_ctx_hdl_t,
				       dpipe_conn_attr_t *,
				       dpipe_pad_info_t *);
    int         (*dpipe_master_prepare)(dpipe_transfer_id_t,
					dpipe_end_trans_ctx_hdl_t,
					dpipe_conn_attr_t *,
					dpipe_pad_info_t *);
    int         (*dpipe_slave_get_dest)(dpipe_transfer_id_t,
					dpipe_conn_attr_t *,
					dpipe_slave_dest_t *);
    int         (*dpipe_slave_done)(dpipe_transfer_id_t, 
                                    dpipe_conn_attr_t *,
                                    dpipe_slave_dest_t *);
    int         (*dpipe_master_start)(dpipe_transfer_id_t,
				      dpipe_conn_attr_t *,
				      dpipe_slave_dest_t *);
    int         (*dpipe_slave_transfer_done)(dpipe_transfer_id_t,
					     dpipe_conn_attr_t *,
                                             int /*error code*/);
    int         (*dpipe_master_transfer_done)(dpipe_transfer_id_t,
					      dpipe_conn_attr_t *,
	                                      int /*error code*/);

} dpipe_end_simple_protocol_ops_t;

/* A union of all of the different types of protocol fcn tables */
typedef union dpipe_end_protocol_ops {
	dpipe_end_simple_protocol_ops_t simple; /* only one so far */
} dpipe_end_protocol_ops_t;

/* Functions for setting up and tearing down pipe connections.
 * THIS is the function table that should be returned to the user-level
 * datapipe library.
 */
typedef struct dpipe_end_control_ops {
    int         (*dpipe_init)(int);
    int         (*dpipe_get_slave_attr)(int, dpipe_end_attr_t *);
    int         (*dpipe_get_master_attr)(int, /* fd */
					 dpipe_end_attr_t *);
    int         (*dpipe_free)(int);
    int         (*dpipe_connect)(int, /* fd */
				 int, /* which role (source/sink) */
	                         int, /* which bus type (master/slave) */
				 dpipe_conn_attr_t *,
				 dpipe_end_protocol_ops_t **);
    int         (*dpipe_disconnect)(int, /*fd*/
	                            dpipe_conn_attr_t *);
    int         (*dpipe_prio_vertices)(int /* fd */,
				       dev_t *, /* vertices */
				       int * /* number of vertices */);
    int         (*dpipe_prio_set)(int);
    int         (*dpipe_prio_clear)(int);
} dpipe_end_control_ops_t;

#endif /* _KERNEL */

/* Flags for DPIPE_STATUS, it's a per data pipe per transfer property */
#define DPIPE_TRANS_COMPLETE             0x0000
#define DPIPE_TRANS_PENDING              0x0001
#define DPIPE_TRANS_CANCELLED            0x0002
#define DPIPE_TRANS_ERROR                0x0004
#define DPIPE_TRANS_UNKNOWN              0x0008

/* This is the ioctl that the user lib used to pass in the src, sink ptrs
 * in the create call
 */

/* Ioctl structures for the pipe */

#define DPIPE_CREATE     1   /* Create a dpipe*/
#define DPIPE_GETATTR    2   /* Get a particular attribute of a dpipe
				tags to specify which attr TBD*/
#define DPIPE_PRETRANS   3   /* Return some parameters to bind */
#define DPIPE_TRANSFER   4   /* Returns a transfer id*/
#define DPIPE_STATUS     5   /* Get status on a transfer id*/
#define DPIPE_CANCEL     6   /* Cancel a transfer id*/
#define DPIPE_STOP       7
#define DPIPE_RESET      8
#define DPIPE_FLUSH      9

typedef struct dpipe_create_ioctl_s {
	int             src_fd;
	__uint64_t      dpipe_src_ops;
	int             sink_fd;
	__uint64_t      dpipe_sink_ops;
} dpipe_create_ioctl_t;

typedef struct dpipe_pretrans_ioctl_s {
    __int32_t       src_pipe_id;
    __int32_t       sink_pipe_id;
    __int64_t       transfer_id;
} dpipe_pretrans_ioctl_t;

typedef struct dpipe_transfer_ioctl_s {
    __int64_t                     transfer_id;
    dpipe_end_trans_ctx_hdl_t     src_cookie;
    dpipe_end_trans_ctx_hdl_t     sink_cookie;
} dpipe_transfer_ioctl_t;


/* Ioctl structures for a pipe end */

typedef union dpipe_get_ops_ioctl_u {
	int       fd;
	void      *dpipe_end_ops;
} dpipe_get_ops_ioctl_t;

struct dpipe_fspe_bind_list {
	__int64_t     offset;     /* offset in the file */
	__int64_t size;       /* transfer size from the offset */
};

/* Syssgi SGI_DPIPE_FSPE_BIND structure */
struct sgi_dpipe_fspe_bind {
	int                 pipe_id;
	dpipe_transfer_id_t transfer_id;
	int                 role;
	int                 iovcnt;
	__uint64_t          sglist;
};

#endif  /* _DPIPE_SYS_H */





