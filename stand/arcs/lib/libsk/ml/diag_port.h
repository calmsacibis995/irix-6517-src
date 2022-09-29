#if IP30

#define reset_enet()
#define PRE_PIO
#define POST_PIO
#define PASS(_name_)		if(diag_get_mode()) ==DIAG_MODE_MFG) result_pass(_name_, DIAG_MODE_MFG, "")
#define result_fail(s,e,ss)	printf("%s:[%d] %s\n", s, e, ss)
/*---------------------------------------------------------*/
/* libkl/diags/diag_lib stubs */
#define diag_ioc3_base(br_base,devno)	(br_base+BRIDGE_DEVIO(devno))

int diag_check_pci_scr(int diag_mode,__psunsigned_t bridge_baseAddr,int npci);
void *diag_malloc(size_t nbytes);
void diag_free(void *mem);
void diag_delay(uint usecs);

#define DIAG_SIZE		(8 * 1024 * 1024)
#define DIAG_MEM_START		(4 * 1024 * 1024)

void bridge_dma_config(int, int, int);
__psunsigned_t kv_to_bridge32_dirmap(int, __psunsigned_t);

/* this is for devices that do not support byte reads of their
   config space  like ioc3
*/

typedef struct pcicfg32_s {
        __uint32_t      pci_id ;
        __uint32_t      pci_scr ;
        __uint32_t      pci_rev ;
        __uint32_t      pci_lat ;
        __uint32_t      pci_addr ;
        __uint32_t      pad_0[11] ;
        __uint32_t      pci_erraddrl ;
        __uint32_t      pci_erraddrh ;
} pcicfg32_t ;

#include <sys/SN/kldiag.h>

#define SETUP_TIME      static heartreg_t ht_t0
#define START_TIME      ht_t0 = HEART_PIU_K1PTR->h_count
#define DELTA_TICKS	(HEART_PIU_K1PTR->h_count - ht_t0)
#define SAMPLE_TICKS(ticks)					\
{								\
    (ticks) = HEART_PIU_K1PTR->h_count - ht_t0;			\
}

#define STOP_TIME(str)						\
{								\
    heartreg_t ht_ticks;					\
    heartreg_t ns;						\
    SAMPLE_TICKS(ht_ticks);					\
    ns = ht_ticks * HEART_COUNT_NSECS;				\
    printf("%s, elapsed time %d,%03d,%03d nsecs\n",		\
        (str),ns/1000000, (ns%1000000)/1000, (ns%1000000)%1000);\
}
#define USEC_TO_TICKS(us)	(((__uint64_t)(us)*1000L)/(__uint64_t)HEART_COUNT_NSECS)
#elif KLEGO

#define SETUP_TIME	static __uint64_t t0;
#define START_TIME	t0 = LOCAL_HUB_L(PI_RT_COUNT);
#define SAMPLE_TICKS(ticks) (ticks) = LOCAL_HUB_L(PI_RT_COUNT) - t0
#define DELTA_TICKS	(LOCAL_HUB_L(PI_RT_COUNT) - t0)
#define STOP_TIME(str)
{								\
    __uint64_t us;						\
    SAMPLE_TICKS(us);						\
    printf("%s, elapsed time %d,%03d,%03d usecs\n",		\
       (str), us/1000000, (us%1000000)/1000, (us%1000000)%1000);\
}
#define USEC_TO_TICKS(us)	(us)
#endif
