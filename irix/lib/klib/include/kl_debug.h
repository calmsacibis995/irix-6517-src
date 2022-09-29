#ident "$Header: /proj/irix6.5.7m/isms/irix/lib/klib/include/RCS/kl_debug.h,v 1.1 1999/02/23 20:38:33 tjm Exp $"

extern k_uint_t klib_debug;

/* DEBUG macros
 *
 * The contents klib_debug determines if a particular debug 
 * message will or will not be displayed. Bits 32-63 are
 * reserved for KLIB debug class flags (defined below); Bits 16-31 
 * are reserved for application level classes (defined by the 
 * application). Bits 0-15 are reserved for the debug level. In 
 * order for a debug message to be displayed, the flag for the 
 * appropriate class must be set AND the the specified debug level 
 * must be <= the current level.
 */

/* KLIB level debug classes
 */
#define KLDC_GLOBAL         0x0000000000000000
#define KLDC_FUNCTRACE      0x0000001000000000
#define KLDC_CMP            0x0000002000000000
#define KLDC_ERROR          0x0000004000000000
#define KLDC_HWGRAPH        0x0000008000000000
#define KLDC_INIT           0x0000010000000000
#define KLDC_KTHREAD        0x0000020000000000
#define KLDC_MEM            0x0000040000000000
#define KLDC_PAGE           0x0000080000000000
#define KLDC_PROC           0x0000100000000000
#define KLDC_STRUCT         0x0000200000000000
#define KLDC_UTIL           0x0000400000000000
#define KLDC_SYM            0x0001000000000000
#define KLDC_DWARF          0x0002000000000000
#define KLDC_ALLOC          0x0004000000000000
#define KLDC_TRACE          0x0008000000000000

#define DEBUG(class, level) \
	((!class || (class & klib_debug)) && (level <= (klib_debug & 0xf)))
