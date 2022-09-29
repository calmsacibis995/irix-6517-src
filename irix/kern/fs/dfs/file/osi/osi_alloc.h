/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: osi_alloc.h,v $
 * Revision 65.3  1999/07/21 19:01:14  gwehrman
 * Defined SGIMIPS for this development header.  Fix for bug 657352.
 *
 * Revision 65.2  1997/12/16 17:05:39  lmc
 *  1.2.2 initial merge of file directory
 *
 * Revision 65.1  1997/10/20  19:17:42  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.8.1  1996/10/02  18:10:20  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:44:46  damon]
 *
 * Revision 1.1.3.1  1994/07/13  21:31:20  devsrc
 * 	Moved prototype of osi_caller to machine specific
 * 	header file osi_port_mach.h
 * 	[1994/04/07  18:21:53  mbs]
 * 
 * 	Created this file so typedefs needed by other files (in
 * 	particular tom.c) are available.
 * 	[1994/04/04  15:08:00  mbs]
 * 
 * $EndLog$
 */

#ifndef OSI_ALLOC_H
#define OSI_ALLOC_H
#ifndef SGIMIPS
#define SGIMIPS
#endif /* !SGIMIPS */

/*
 * Memory allocation stuff is placed in a separate file so user
 * level memory diagnostic commands can include only what they
 * need.
 */

#ifdef DEBUG
#define AFSDEBMEM 1       /* THIS SHOULD BE OFF IN ANY REAL SYSTEM */
#endif
#define NEW_MALLOC 1


/*
 * Overall usage of dynamically allocated memory.  Regular buffers
 * are included in the alloc counts and byte counts.  Pinned
 * buffers are not.  These statistics are always collected, regardless
 * of AFSDEBMEM.
 */
struct memUsage {
    /* 
     * The following fields are protected by the osi_alloc_mutex.
     */
    u_long mu_curr_bytes;               /* tot bytes currently held */
    u_long mu_hiwat_bytes;              /* max bytes ever held */
    u_long mu_curr_allocs;              /* tot allocations held */
    u_long mu_hiwat_allocs;             /* max allocations ever held */

    /*
     * The following fields are protected by the osi_bufferLock.
     */
    u_long mu_alloc_buffers;            /* num of buffers allocated */
    u_long mu_used_buffers;             /* num of buffers in use */

    /*
     * The following fields are protected by the osi_buflock.
     */
    u_long mu_alloc_IO_buffers;        /* num of IO buffers allocated */
    u_long mu_used_IO_buffers;         /* num of IO buffers in use */
};

#ifdef AFSDEBMEM

#define	OSI_MEMHASHSIZE	997		/* prime */

/*
 * Per-block information.  This structure is prepended to the
 * front of each allocation.
 */
struct osimem {
    struct osimem *next;
    long size;
    caddr_t caller;
};

/*
 * Per-caller information.  There is one of these for each
 * program counter value from which an allocation is made.
 * The list is maintained in MRU (most recently used) order.
 * That is, whenever an alloc/dealloc is done, the corresponding
 * osi_allocrec is moved to the front of the list.
 */
struct osi_allocrec {
    struct osi_allocrec *next, *prev;
    caddr_t caller;			/* Caller for this record */
    u_long total;			/* Total bytes allocated */
    u_long held;			/* No. bytes currently held */
    u_long allocs;			/* Total no. of allocations */
    u_long blocks;			/* No. blocks currently held */
    u_long last;			/* Size of most recent alloc */
    u_long allocs_hiwat;                /* Hiwater mark for no. allocs */
    u_long bytes_hiwat;                 /* Hiwater mark for bytes held */
    int type;                           /* Memory usage type */
};

#endif /* AFSDEBMEM */

/*
 * Memory Component Types.  Be sure the update the
 * strings table below when you update these definitions.
 */
#define GENERAL                 0x00010000
#define CM                      0x00020000
#define NUM_DFS_COMPONENTS      2
#define MAX_TYPES_IN_COMPONENT  18

/*
 * General types.
 */
#define M_UNKNOWN       	(GENERAL|0)
#define M_STRING        	(GENERAL|1)
#define M_BUFFER        	(GENERAL|2)
#define M_IO_BUFFER     	(GENERAL|3)
#define M_FS_BLOCK      	(GENERAL|4)
#define M_AFS_FID       	(GENERAL|5)
#define M_GNODE         	(GENERAL|6)
#define M_NH_CACHE      	(GENERAL|7)
#define M_SOCKADDR_IN   	(GENERAL|8)
#define M_VLCONF_CELL   	(GENERAL|9)
#define M_XVFS_VNODEOPS 	(GENERAL|10)
#define M_GENERAL_NUM_TYPES	11
	
/*
 * CM types.
 */
#define M_CM_ACL        	(CM|0)
#define M_CM_CELL       	(CM|1)
#define M_CM_CONN       	(CM|2)
#define M_CM_COOKIE 	        (CM|3)
#define M_CM_DCACHE     	(CM|4)
#define M_CM_DLINFO     	(CM|5)
#define M_CM_INDEXFLAGS 	(CM|6)
#define M_CM_INDEXTIMES 	(CM|7)
#define M_CM_MEMCACHE   	(CM|8)
#define M_CM_MEMCACHE_DATA      (CM|9)
#define M_CM_PARAMS             (CM|10)
#define M_CM_RACINGITEM         (CM|11)
#define M_CM_SCACHE             (CM|12)
#define M_CM_SERVER             (CM|13)
#define M_CM_TGT                (CM|14)
#define M_CM_TOKENLIST          (CM|15)
#define M_CM_VDIRENT            (CM|16)
#define M_CM_VOLUME             (CM|17)
#define M_CM_NUM_TYPES          18
                                
#ifdef MEM_TYPE_STRINGS
/*
 * This table is used to print memory usage statics.
 */
char * mem_type_strings[NUM_DFS_COMPONENTS][MAX_TYPES_IN_COMPONENT] = {
    {
        "unknown",               
        "string",                
        "buffer",                
        "io_buffer",             
        "fs_block",              
        "afs_fid",               
        "gnode",                 
        "nh_cache",              
        "sockaddr_in",           
        "vlconf_cell",           
        "xvfs_vnodeops"         
    },
    {
        "cm_acl",                
        "cm_cell",               
        "cm_conn",               
        "cm_cookielist",         
        "cm_dcache",             
        "cm_dlinfo",             
        "cm_indexflags",         
        "cm_indextimes",         
        "cm_memcache",           
        "cm_memcache_data",
        "cm_params",             
        "cm_racingitem",         
        "cm_scache",             
        "cm_server",             
        "cm_tgt",                
        "cm_tokenlist",          
        "cm_vdirent",            
        "cm_volume"             
   }            
};

#define M_COMPONENT(t) ((t) >> 16)
#define M_TYPE(t) ((t) & 0xFFFF)
#define M_NAME(t) (mem_type_strings[M_COMPONENT(t) - 1][M_TYPE(t)])

#endif /* MEM_TYPE_STRINGS */

/*
 * Commands for the "plumber" function. 
 */
#define PLUMBER_GET_MEM_USAGE          0
#define PLUMBER_GET_ALLOC_RECS         1
#define PLUMBER_GET_MEM_RECS           2           

int
dfs_plumber(
    int cmd,
    int *cookie,
    int size,
    int *countp,
    void *bufferp);

#ifdef AFSDEBMEM
#ifndef SGIMIPS
int foobar;
#endif /* SGIMIPS */
#define OSI_MALLOC(space, cast, size, type, flags) \
     (space) = (cast) osi_Malloc((size), (type))
#else
#define OSI_MALLOC(space, cast, size, type, flags) \
    (space) = (cast) osi_Alloc(size)
#endif

#define OSI_FREE(space) osi_Free(space)

#endif /* OSI_ALLOC_H */
