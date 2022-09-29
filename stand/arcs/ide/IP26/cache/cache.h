#include <sys/types.h>
#include "libsk.h"

#define SCACHE_TESTADDR      PHYS_TO_K0(0x8c00000)

extern int  scache_data(void);
extern int  scache_data2(void);
extern int  scache_tag(void);
extern int  scache_tag2(void);
extern int  scache_tag3(void);
extern int  dcache_test(unsigned long, unsigned long, unsigned long, 
		        unsigned long);
void inval_gcache_tags(void);

/* From lib/libsk/ml/tfpcache.s  */
extern void clean_icache(void *addr, unsigned int len);
extern void clean_dcache(void *addr, unsigned int len);
extern void clear_cache(void *addr, unsigned int len);
extern void flush_cache(void);
extern long get_dcachesize(void);

extern void run_cached(void);
extern void run_chandra(void);
extern void *fp_ls(long, volatile void *, long);

/*	From lib/libsk/ml/IP26asm.s	*/
extern void __dcache_inval(void *address, int length);
extern void __dcache_wb(void *address, int length);
extern void __dcache_wb_inval(void *address, int length);
extern void __dcache_wb_inval_all(void);
extern void __dcache_wb_all(void);
