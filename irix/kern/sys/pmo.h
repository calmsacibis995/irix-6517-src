/*
 * sys/pmo.h
 *
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ifndef __SYS_PMO_H__
#define __SYS_PMO_H__

#ifdef	__cplusplus
extern "C" {
#endif
        
#include <sys/types.h>

/***********************************************************
 *                  Version control                        *
 ***********************************************************/
        
#define PMOCTL_VERSION          (1)
#define PMOCTL_VERSION_HEADER   uint pmoctl_version
#define PMOCTL_SETVERSION(p)    ((p)->pmoctl_version = PMOCTL_VERSION)

/***********************************************************
 *                  Operation OPCODES                      *
 ***********************************************************/

#define PMO_MLDCREATE          1
#define PMO_MLDDESTROY         2
#define PMO_MLDCREATEX         3

#define PMO_MLDSETCREATE       10
#define PMO_MLDSETDESTROY      11
#define PMO_MLDSETPLACE        12
#define PMO_MLDSETOP           13       
#define PMO_MLDSETCREATEX      14

#define PMO_PMCREATE           20
#define PMO_PMDESTROY          21
#define PMO_PMOP               22
#define PMO_PMATTACH           23
#define PMO_PMGETDEFAULT       24
#define PMO_PMGETALL           25  
#define PMO_PMSTAT             26
#define PMO_PMSETPAGESIZE      27
#define PMO_PMSETDEFAULT       28
#define PMO_PMCREATEX          29

        
#define PMO_PROCMLDLINK        30

#define PMO_USERMIGRATE        40        


#define PMO_PMGET_PAGE_INFO   50
#define PMO_MLDGETNODE         51

#define PMO_GETNODEMASK		60
#define PMO_SETNODEMASK		61

#ifdef SN0XXL
/*
 * The GET/SET NODEMASK functions above don't
 * support more than 64 nodes.  Add new functions
 * that support more than 64 nodes.
 *
 * These should be considered temporary and should
 * eventually be replaced.
 */
#define PMO_GETNODEMASK_BYTESIZE  100
#define PMO_GETNODEMASK_UINT64    101
#define PMO_SETNODEMASK_UINT64    102
#endif /* SN0XXL */


/***********************************************************
 *                      GLOBAL DEFS                        *
 ***********************************************************/

/*
 * Handle type for any kind
 * of Policy Management Object.
 */
typedef int pmo_handle_t;

/*
 * Kinds of PMO objects
 */
typedef enum {
        __PMO_ANY,      /* any pmo type */
        __PMO_MLD,      /* an MLD (Memory Locality Domain) */
        __PMO_RAFF,     /* a Resource Affinity spec */
        __PMO_MLDSET,   /* a group of MLDs */
        __PMO_PM,       /* a policy module */
	__PMO_PMO_NS	/* PMO NS object */
} pmo_type_t;

#ifndef _KERNEL
/*
 * pmoctl user call
 */
#define pmoctl(x, y, z) syssgi(SGI_PMOCTL, (x), (y), (z))
#endif

/*
 * policy flags definitions. 
 */
#define	POLICY_CACHE_COLOR_FIRST	1  /* Give priority to cache color */
#define	POLICY_PAGE_ALLOC_WAIT		2  /* Wait for large pages to be allocated */
/*
 *	The following flags are set by the kernel for information only
 */
#define	POLICY_DEFAULT_MEM_STACK	0x010	/* Default Stack Policy */
#define	POLICY_DEFAULT_MEM_TEXT		0x020	/* Default Text Policy */
#define	POLICY_DEFAULT_MEM_DATA		0x040	/* Default Data Policy */

/***********************************************************
 *                         MLD API                         *
 ***********************************************************/

#define MLD_DEFAULTSIZE ((size_t)(-1))

typedef struct mld_info {
        int    radius;      /* radius of 0 indicates local only */
        size_t size;        /* this is the expected size in bytes */
        int    reserved[2];
} mld_info_t;

#if defined(_KERNEL) && (_MIPS_SIM == _ABI64)
typedef struct irix5_mld_info {
        app32_int_t radius;
        app32_int_t size;
        app32_int_t reserved[2];
} irix5_mld_info_t;
#endif

#ifndef _KERNEL
extern pmo_handle_t mld_create(int radius, long size);
extern pmo_handle_t mld_create_special(int radius, long size, pmo_handle_t required);
extern int mld_destroy(pmo_handle_t mld_handle);
#endif

/***********************************************************
 *                        MLDSET API                       *
 ***********************************************************/

/*
 * Basic mldset specification
 */

typedef struct mldset_info {
        pmo_handle_t*    mldlist;           /* list of mlds */
        int              mldlist_len;       /* number of mlds */
        void*            reserved[2];
} mldset_info_t;

#if defined(_KERNEL) && (_MIPS_SIM == _ABI64)
typedef struct irix5_mldset_info {
        app32_ptr_t  mldlist;           /* list of mlds */
        app32_int_t  mldlist_len;       /* number of mlds */
        app32_ptr_t  reserved[2];
} irix5_mldset_info_t;
#endif

/*
 * Topology types for mldsets
 */
typedef enum {
        TOPOLOGY_FREE,
        TOPOLOGY_CUBE,
        TOPOLOGY_CUBE_FIXED,
        TOPOLOGY_PHYSNODES,
        TOPOLOGY_CPUCLUSTER,
        TOPOLOGY_LAST
} topology_type_t;

/*
 * Specification of resource affinity.
 */

/* resource identification type */
#define RAFFIDT_NAME 10
#define RAFFIDT_FD   11

/* rafflist attributes */
#define RAFFATTR_ATTRACTION 0x0001
#define RAFFATTR_REPULSION  0x0002
#define RAFFATTR_VALID      (RAFFATTR_ATTRACTION | RAFFATTR_REPULSION)

typedef struct raff_info {
        void*  resource;
        ushort reslen;
        ushort restype;
        ushort radius;
        ushort attr;
        void*  reserved[2];
} raff_info_t;

#if defined(_KERNEL) && (_MIPS_SIM == _ABI64)
typedef struct irix5_raff_info {
        app32_ptr_t  resource;
        ushort       reslen;
        ushort       restype;
        ushort       radius;
        ushort       attr;
        app32_ptr_t  reserved[2];
} irix5_raff_info_t;
#endif


/*
 * Request types
 */
typedef enum {
        RQMODE_ADVISORY,
        RQMODE_MANDATORY
} rqmode_t;

/*
 * Specification of an MLD SET.
 */

typedef struct mldset_placement_info {
        pmo_handle_t     mldset_handle;     /* set of mlds */
        topology_type_t  topology_type;     /* topology */
        raff_info_t*     rafflist;          /* resource affinity info */
        int              rafflist_len;      /* number of resources in list */
        rqmode_t         rqmode;            /* mandatory or advisory? */
        void*            reserved[2];
} mldset_placement_info_t;

#if defined(_KERNEL) && (_MIPS_SIM == _ABI64)
typedef struct irix5_mldset_placement_info {
        app32_int_t  mldset_handle;     /* set of mlds */
        app32_int_t  topology_type;     /* topology */
        app32_ptr_t  rafflist;          /* resource affinity info */
        app32_int_t  rafflist_len;      /* number of resources in list */
        app32_int_t  rqmode;            /* mandatory or advisory? */
        app32_ptr_t  reserved[2];
} irix5_mldset_placement_info_t;
#endif

#ifndef _KERNEL
extern pmo_handle_t mldset_create(pmo_handle_t* mldlist, int mldlist_len);
extern pmo_handle_t mldset_create_special(pmo_handle_t* mldlist,
					  int mldlist_len,
					  pmo_handle_t requested);
extern int mldset_place(pmo_handle_t mldset_handle,
                        topology_type_t topology_type,
                        raff_info_t* rafflist,
                        int rafflist_len,
                        rqmode_t rqmode);
extern int mldset_destroy(pmo_handle_t mldset_handle);
#endif

/***********************************************************
 *                          PM API                         *
 ***********************************************************/

#define PM_NAME_SIZE        63
#define PMFACTORY_NS_NENT   32
#define PM_PAGESZ_DEFAULT   0
#define ARENA_SIZE_DEFAULT  0  /* Flag indicating use of default arena size */
#define PAGE_SIZE_DEFAULT   0  /* Flag indicating use of default page size */
#define NUMA_ARENA_SIZE_DEFAULT (64*1024*1024)  /* Default arena size -- 64M */

/*
 * Mem types for pm_setdefault.
 */

typedef enum {
	MEM_STACK,
	MEM_TEXT,
	MEM_DATA,
	NUM_MEM_TYPES
} mem_type_t;

/*
 * Policy module info
 */

typedef struct policy_set {
        char*  placement_policy_name;
        void*  placement_policy_args;
        char*  fallback_policy_name;
        void*  fallback_policy_args;
        char*  replication_policy_name;
        void*  replication_policy_args;
        char*  migration_policy_name;
        void*  migration_policy_args;
        char*  paging_policy_name;
        void*  paging_policy_args;
        size_t page_size;
	short  page_wait_timeout;
	short  policy_flags;
        void*  reserved[6];
} policy_set_t;

typedef struct pm_info {
        char          placement_policy_name[PM_NAME_SIZE + 1];
        void*         placement_policy_args;
        char          fallback_policy_name[PM_NAME_SIZE + 1];
        void*         fallback_policy_args;        
        char          replication_policy_name[PM_NAME_SIZE + 1];
        void*         replication_policy_args;
        char          migration_policy_name[PM_NAME_SIZE + 1];
        void*         migration_policy_args;
        char          paging_policy_name[PM_NAME_SIZE + 1];
        void*         paging_policy_args;
        size_t        page_size;
	short	      page_wait_timeout;
	short	      policy_flags;
        char          reserved_policy_name[2][PM_NAME_SIZE + 1];
        void*         reserved_policy_args[2];
        int           reserved[2];
} pm_info_t;


#if defined(_KERNEL) && (_MIPS_SIM == _ABI64)
typedef struct irix5_pm_info {
        char          placement_policy_name[PM_NAME_SIZE + 1];
        app32_ptr_t   placement_policy_args;
        char          fallback_policy_name[PM_NAME_SIZE + 1];
        app32_ptr_t   fallback_policy_args;        
        char          replication_policy_name[PM_NAME_SIZE + 1];
        app32_ptr_t   replication_policy_args;
        char          migration_policy_name[PM_NAME_SIZE + 1];
        app32_ptr_t   migration_policy_args;
        char          paging_policy_name[PM_NAME_SIZE + 1];
        app32_ptr_t   paging_policy_args;
        app32_uint_t  page_size;
        short	      page_wait_timeout;
        short	      policy_flags;
        char          reserved_policy_name[2][PM_NAME_SIZE + 1];
        app32_ptr_t   reserved_policy_args[2];
        app32_uint_t  reserved[2];
} irix5_pm_info_t;
#endif

/*
 * Virtual address space range
 */
typedef struct vrange {
        caddr_t base_addr;
        size_t  length;
} vrange_t;

#if defined(_KERNEL) && (_MIPS_SIM == _ABI64)
typedef struct irix5_vrange {
        app32_ptr_t   base_addr;
        app32_uint_t  length;
} irix5_vrange_t;
#endif

/*
 * List of pmo handles
 */
typedef struct pmo_handle_list {
        pmo_handle_t* handles;
        uint          length;
} pmo_handle_list_t;

#if defined(_KERNEL) && (_MIPS_SIM == _ABI64)
typedef struct irix5_pmo_handle_list {
        app32_ptr_t   handles;
        app32_uint_t  length;
} irix5_pmo_handle_list_t;
#endif

#if defined(_KERNEL) && (_MIPS_SIM == _ABI64)
typedef struct irix5_pm_pginfo_s {
        app32_ptr_t	vaddr;
        dev_t		node_dev;
        app32_uint_t	page_size;
	app32_int_t	pm_handle;
        app32_ptr_t     reserved[2];
} irix5_pm_pginfo_t;
#endif

typedef	struct pm_pginfo_s {
	caddr_t vaddr;
	dev_t	node_dev;
	uint	page_size;
	pmo_handle_t pm_handle;
        void*   reserved[2];
} pm_pginfo_t;

/*
 * List of nodes passed to the user as devts.
 */
typedef struct pm_pginfo_list {
        pm_pginfo_t* 	pm_pginfo;
        uint    length;
} pm_pginfo_list_t;

#if defined(_KERNEL) && (_MIPS_SIM == _ABI64)
typedef struct irix5_pm_pginfo_list {
        app32_ptr_t   pm_pginfo;
        app32_uint_t  length;
} irix5_pm_pginfo_list_t;
#endif

/*
 * Policy Module Stat Structure
 */
typedef struct pm_stat {
        char         placement_policy_name[PM_NAME_SIZE + 1];
        char         fallback_policy_name[PM_NAME_SIZE + 1];
        char         replication_policy_name[PM_NAME_SIZE + 1];
        char         migration_policy_name[PM_NAME_SIZE + 1];
        char         paging_policy_name[PM_NAME_SIZE + 1];
        size_t       page_size;
        int          policy_flags;
        pmo_handle_t pmo_handle;
        char         reserved_policy_name[2][PM_NAME_SIZE + 1];
        int          reserved[2];
} pm_stat_t;

#if defined(_KERNEL) && (_MIPS_SIM == _ABI64)
typedef struct irix5_pm_stat {
        char         placement_policy_name[PM_NAME_SIZE + 1];
        char         fallback_policy_name[PM_NAME_SIZE + 1];
        char         replication_policy_name[PM_NAME_SIZE + 1];
        char         migration_policy_name[PM_NAME_SIZE + 1];
        char         paging_policy_name[PM_NAME_SIZE + 1];
        app32_uint_t page_size;
        app32_uint_t policy_flags;
        pmo_handle_t pmo_handle;
        char         reserved_policy_name[2][PM_NAME_SIZE + 1];
        app32_uint_t reserved[2];        
} irix5_pm_stat_t;
#endif

/*
 * mldlink info
 */
typedef struct mldlink_info {
        pid_t pid;
        pmo_handle_t mld_handle;
        int lcpuid;
        rqmode_t rqmode;
        int  reserved[2];
} mldlink_info_t;

#define MLDLINK_ANYCPU (-1)
        
#ifndef _KERNEL
extern pmo_handle_t pm_create(policy_set_t* policy_set);
extern pmo_handle_t pm_create_special(policy_set_t* policy_set, pmo_handle_t *);
extern pmo_handle_t pm_create_simple(char* plac_name,
                                     void* plac_args,
                                     char* repl_name,
                                     void* repl_args,
                                     size_t page_size);
extern void pm_filldefault(policy_set_t* policy_set);
extern int pm_destroy(pmo_handle_t pm_handle);
extern int pm_attach(pmo_handle_t pm_handle, void* base_addr, size_t length);
extern pmo_handle_t pm_getdefault(mem_type_t);
extern pmo_handle_t pm_setdefault(pmo_handle_t pm_handle, mem_type_t);
extern int pm_getall(void* base_addr,
                     size_t length,
                     pmo_handle_list_t* pmo_handle_list);
extern int pm_getstat(pmo_handle_t pm_handle, pm_stat_t* pm_stat);
extern int pm_setpagesize(pmo_handle_t pm_handle, size_t page_size);

extern void *numa_acreate(pmo_handle_t mld, 
			  size_t arena_size, 
			  size_t page_size);
#define numa_amalloc amalloc
#define numa_afree afree
#endif 


/*
 * Link thread to MLD
 */
extern int process_mldlink(pid_t pid, pmo_handle_t mld_handle, rqmode_t rqmode);
extern int process_cpulink(pid_t pid,
                           pmo_handle_t mld_handle,
                           cpuid_t lcpuid,
                           rqmode_t rqmode);

/*
 * User initiated migration
 */
int
migr_range_migrate(void* base_addr, size_t length, pmo_handle_t pmo_handle);

/*
 * Specification of arguments for
 * the "MigrationControl" policy
 */


typedef struct migr_policy_uparms {
        PMOCTL_VERSION_HEADER;
        __uint64_t  migr_base_enabled         :1,
                    migr_base_threshold       :8,
                    migr_freeze_enabled       :1,
                    migr_freeze_threshold     :8,
                    migr_melt_enabled         :1,
                    migr_melt_threshold       :8,
                    migr_enqonfail_enabled    :1,
                    migr_dampening_enabled    :1,
                    migr_dampening_factor     :8,
                    migr_refcnt_enabled       :1,
                    migr_resv_bits            :4;
        __uint64_t  migr_reserved[2];
} migr_policy_uparms_t;

#define MIGRATION_OFF               0x0
#define MIGRATION_ON                0x1

#define REFCNT_OFF                  0x0
#define REFCNT_ON                   0x1

#define SETOFF                      0x0
#define SETON                       0x1


/*
 * syssgi call to change parameters related to page migration control 
 */

/* command to access/manipulate migration control parameters */

#define MIGR_PARMS_GET         100
#define MIGR_PARMS_SET         101

/* Test commands */
#define NUMNODES_GET           200
#define PFNHIGH_GET            201
#define PFNLOW_GET             202
#define REFCOUNTER_GET         203
#define MLD_GETNODE            204

/*
 * Default Migr modes (tunables)
 */
#define MIGR_DEFMODE_DISABLED 0  /* disabled, overrides user settings */
#define MIGR_DEFMODE_ENABLED  1  /* enabled, overrides user settings */
#define MIGR_DEFMODE_NORMOFF  2  /* use default migr policy (OFF), users can override */
#define MIGR_DEFMODE_NORMON   3  /* use default migr policy (ON), users can override */
#define MIGR_DEFMODE_LIMITED  4  /* NORMOFF for machines with maxradius < numa_migr_min_maxradius
                                    NORMON for machines with maxradius >= numa_migr_min_maxradius */

/*
 * Default Refcnt modes (tunables)
 */
#define REFCNT_DEFMODE_DISABLED 0  /* disabled, overrides user settings */
#define REFCNT_DEFMODE_ENABLED  1  /* enabled, overrides user settings */
#define REFCNT_DEFMODE_NORMOFF  2  /* use default migr policy (OFF), users can override */
#define REFCNT_DEFMODE_NORMON   3  /* use default migr policy (ON), users can override */

/*
 * Migr Mechs
 */
#define MIGR_MECH_ENQUEUE   1  /* enqueue to migrate */
#define MIGR_MECH_IMMEDIATE 0  /* migrate immediately */



#ifdef	__cplusplus
}
#endif

#endif /* __SYS_PMO_H__ */
