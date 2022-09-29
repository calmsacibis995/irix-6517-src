#ident "$Header: "

/**
 **	Main header file for the klib library.
 **
 **/
#include <sys/dump.h>  /* To pick up the definition for dump_hdr_t */

#ifdef KLIB_DEBUG
#define ALLOC_DEBUG 1
#endif

#include <klib/alloc.h>
#include <klib/kl_types.h>

/* Macros that eliminate the offset paramaters to the kl_uint() and kl_int()
 * functions (just makes things cleaner looking)
 */
#define KL_UINT(p, s, m) kl_uint(p, s, m, 0)
#define KL_INT(p, s, m) kl_int(p, s, m, 0)

typedef struct kstruct_s {
	k_ptr_t     ptr;
	kaddr_t     addr;
	void       *type;
} kstruct_t;

typedef struct banks_s {
	uint    b_size;         /* Size of memory in MB */
	kaddr_t b_paddr;        /* First physical address */
} banks_t;

typedef struct node_memory_s {
	int     	n_module;
	char       *n_slot;
	int     	n_nodeid;
	int     	n_nasid;
	int     	n_memsize;
	int     	n_numbanks;
	int     	n_flag;
	banks_t 	n_bank[1];
} node_memory_t;

/* Iclude all relevant header files
 */
#include <klib/btnode.h>
#include <klib/htnode.h>
#include <klib/queue.h>

#include <klib/kl_debug.h>
#include <klib/kl_error.h>
#include <klib/kl_stringtab.h>
#include <klib/kl_alloc.h>
#include <klib/kl_print.h>

#include <klib/kl_libmem.h>
#include <klib/kl_libkern.h>
#include <klib/kl_libsym.h>
#include <klib/kl_libtrace.h>
#include <klib/kl_hwconfig.h>
#include <klib/kl_libhwconfig.h>
#include <klib/kl_swconfig.h>
#include <klib/kl_libswconfig.h>
#include <klib/kl_sysconfig.h>

/* Struct libinfo_s contains a pointer to the library control structure
 * and a pointer to library free function (to free all memory allocated
 * on behalf of the library).
 */ 
typedef void (*klib_freeinfo_func) (
	void			*	/* void pointer */);

typedef struct libinfo_s {
	void 				*k_data;
	klib_freeinfo_func 	 k_free;
} libinfo_t;

/* Struct klib_s, contains all the information necessary for accessing
 * information in the kernel. A pointer to a klib_t struct will be
 * returned from libkern_init() if core dump analysis (or live system
 * analysis) is possible. 
 *
 */
typedef struct klib_s {
	int				 k_flags;		/* Flags pertaining to klib_s struct */
	char            *k_icrashdef;   /* pathname for icrashdef file           */
	libinfo_t		 k_libmem;
	libinfo_t        k_libkern;
	libinfo_t        k_libsym;
} klib_t;

#define K_ICRASHDEF             ((KLP)->k_icrashdef)

extern klib_t *klib_table[];
extern int cur_klib;
extern int klib_trap_msg;
extern int generate_core;

#define MAX_KLIBS   1
#define KLP			klib_table[cur_klib]
#define STP 		((syminfo_t*)KLP->k_libsym.k_data)
#define K_FLAGS		KLP->k_flags

/* Some flag values
 */
#define K_IGNORE_FLAG       	0x0001   /* Global ignore flag           */
#define K_IGNORE_MEMCHECK   	0x0002   /* Ignore memory validity check */
#define K_ICRASHDEF_FLAG    	0x0004   /* Use alternate icrashdef from file */

#define K_MEMINFO_ALLOCED		0x0100
#define K_SYSINFO_ALLOCED		0x0200
#define K_KERNINFO_ALLOCED		0x0400
#define K_KERNDATA_ALLOCED		0x0800
#define K_PDEINFO_ALLOCED		0x1000
#define K_SYMTAB_ALLOCED		0x2000
#define K_GLOBAL_ALLOCED		0x4000
#define K_STRUCT_SIZES_ALLOCED	0x8000

#define FALSE   0
#define TRUE    1

/* Flags that can be passed to various functions. Applications can
 * make use of these flags when calling kl_readmem(), kl_get_struct(),
 * print routines, etc.
 */
#define K_TEMP		0x00000001
#define K_PERM  	0x00000002
#define K_ALL		0x00000004	
#define K_INDENT	0x00000008
#define K_FULL  	0x00000010
#define K_SUPRSERR 	0x00000020
#define K_DECIMAL   0x00000040  /* print decimal value                    */
#define K_OCTAL     0x00000080  /* print octal value                      */
#define K_HEX       0x00000100  /* print hex value                        */

/* Macros for translating strings into long numeric values depending on the
 * base of 's'.
 */
#define GET_VALUE(s, value) kl_get_value(s, NULL, NULL, value)
#define GET_HEX_VALUE(s) (kaddr_t)strtoull(s, (char**)NULL, 16)
#define GET_DEC_VALUE(s) (unsigned)strtoull(s, (char**)NULL, 10)
#define GET_OCT_VALUE(s) (unsigned)strtoull(s, (char**)NULL, 8)

/**
 ** klib_s struct operation function prototypes
 **/

/* Initialize the next klib_s struct in the klib_table[].
 */
int init_klib();

/* Set any KLIB specific flags.
 */
void set_klib_flags(
	int 	/* flag value(s) */);

/* Switch to a different klib 
 */
int switch_klib(
	int 	/* klib index */);

/* Free all memory associated with a klib_s struct
 */
void kl_free_klib(
	klib_t *		/* pointer to klib_s struct to free */);

