#ident "$Header: "

#include <klib/kl_cmp.h>

#define LSEEK(C, X, Y, Z) \
	(((C)->ptrsz == 64) ? lseek64(X,(off64_t)Y, Z) : lseek(X, (off_t)Y, Z))

/* CORE_TYPE indicates type of corefile
 */
typedef enum {
	dev_kmem,   /* image of /dev/kmem, a running kernel */
	reg_core,   /* Regular (uncompressed) core file */
	cmp_core    /* compressed core file */
} CORE_TYPE;

/* Information about vmcore file (core dump or live system memory)
 */
typedef struct meminfo_s {
	CORE_TYPE       core_type;   /* type of core file                        */
	char           *corefile;    /* pathname for corefile                    */
	char           *namelist;    /* pathname for namelist (unix)             */
	int32           core_fd;     /* file descriptor for dump file            */
	dump_hdr_t     *dump_hdr;    /* dump header information                  */
	int32           ptrsz;       /* Number of bytes in a pointer type        */
	int32           rw_flag;  /* O_RDONLY/O_RDWR (valid only for dev_kmem)   */
} meminfo_t;

/* Macros for easy access to meminfo_s struct data
 */
#define LIBMEM_DATA				((meminfo_t*)(KLP)->k_libmem.k_data)

#define K_CORE_TYPE     		(LIBMEM_DATA->core_type)
#define K_COREFILE              (LIBMEM_DATA->corefile)
#define K_NAMELIST              (LIBMEM_DATA->namelist)
#define K_CORE_FD               (LIBMEM_DATA->core_fd)
#define K_DUMP_HDR              (LIBMEM_DATA->dump_hdr)
#define K_PTRSZ                 (LIBMEM_DATA->ptrsz)
#define K_RW_FLAG               (LIBMEM_DATA->rw_flag)

/**
 ** Macros that use values stored in the meminfo_s struct.
 **/
#define ACTIVE                  (K_CORE_TYPE == dev_kmem)
#define COMPRESSED              (K_CORE_TYPE == cmp_core)
#define UNCOMPRESSED            (K_CORE_TYPE == reg_core)

/**
 ** Function prototypes
 **/
int kl_setup_meminfo(
	char*       /* namelist */,
	char*       /* corefile */,
	int         /* rw flag */);

meminfo_t *kl_open_core(
	char*       /* namelist */,
	char*       /* corefile */,
	int         /* rw flag */);

