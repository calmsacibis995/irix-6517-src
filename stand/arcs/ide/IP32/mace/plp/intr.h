#if  !defined( __TIME_H__)
#define __TIME_H__
#include "unistd.h"

/* The following address are unmapped and  uncached address */

#ifdef notdef
#define UST                      (*(volatile long long*) 0xBF340000)
#else /* notdef */
#define UST_ADDR		((volatile long long *) 0xBF340000)
#define UST			(READ_REG64(UST_ADDR, long long))
#endif /* notdef */
#define NS_PER_TICK              960

#ifdef notdef
#define ISA_BASE_AND_RESET       (*(volatile long long*) 0xBF310000)
#else /* notdef */
#define ISA_BASE_AND_RESET_ADDR	((volatile long long *) 0xBF310000)
#define ISA_BASE_AND_RESET	(READ_REG64(ISA_BASE_AND_RESET_ADDR,long long))
#define SET_ISA_BASE_AND_RESET(data) \
	(WRITE_REG64(data, ISA_BASE_AND_RESET_ADDR, long long))
#endif /* notdef */
#define ISA_RESET                0x01

#ifdef notdef
#define ISA_INTR_STATUS          (*(volatile long long*) 0xBF310010)
#define ISA_INTR_ENABLE          (*(volatile long long*) 0xBF310018)
#else /* notdef */
#define ISA_INTR_STATUS_ADDR	((volatile long long *) 0xBF310010)
#define ISA_INTR_ENABLE_ADDR	((volatile long long *) 0xBF310018)
#define ISA_INTR_STATUS		(READ_REG64(ISA_INTR_STATUS_ADDR, long long))
#define ISA_INTR_ENABLE		(READ_REG64(ISA_INTR_ENABLE_ADDR, long long))
#define SET_ISA_INTR_ENABLE(data) \
	(WRITE_REG64(data, ISA_INTR_ENABLE_ADDR, long long))
#endif /* notdef */

#define PLP_INTR_MASK            0x10

#define PLP_PORT_INTR            0x10000
#define PLP_A_DMA_DONE           0x20000
#define PLP_B_DMA_DONE           0x40000
#define PLP_MEMORY_ERROR         0x80000

#define ANY_PLP_INTR             0xF0000

extern unsigned long long zzCurrentTime;
extern unsigned long long zzTicks;
extern unsigned long long zzElapsedTime;
extern unsigned long long zztim;
extern unsigned long long zzcond;
#define Await(cond,timeout,errmsg,infmsg)                 \
     {                                                    \
          zzCurrentTime = UST;                            \
          zzTicks = (timeout * 1000) / NS_PER_TICK;       \
	msg_printf(DBG, "Await:zzCurrentTime==0x%llx,zzTicks==0x%llx\n",  \
			zzCurrentTime, zzTicks);		      \
          do {                                            \
	       delay(30000);                               \
               zzElapsedTime = UST - zzCurrentTime;       \
               if (zzcond = (cond)) {                    \
		msg_printf(DBG,"Await: Condition Hit\n"); \
		   zzElapsedTime = (zzTicks);             \
		} else {                                  \
		msg_printf(DBG,"Await: Condition Not Hit (0x%llx)\n", \
				zzcond);  \
		}                                         \
          } while (zzElapsedTime < (zzTicks));            \
          if (!zzcond) {                                  \
               ErrorMsg(1,errmsg);                        \
          } else                                          \
               InfoMsg(infmsg);                           \
     }

#ifdef notdef
#define Delay(timeout)                                    \
     {                                                    \
	  zzCurrentTime = UST;                            \
          zzTicks = (timeout * 1000) / NS_PER_TICK;       \
          do {                                            \
	      sleep(30);                                  \
	      zztim = UST;                                \
	      printf("UST = %0.8llx\n",zztim);              \
	      zzElapsedTime = UST - zzCurrentTime;        \
	  } while (zzElapsedTime < zzTicks);              \
     }
#endif /* notdef */
	 
long long  GetInterruptStatus();

void EnableInterrupt(unsigned long mask);

void DisableInterrupt(unsigned long mask);

boolean_t InterruptOccurred(unsigned long mask, ulong uSecs);


#endif

