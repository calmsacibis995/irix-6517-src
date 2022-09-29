/*
 * -------------------------------------------------------------
 * $Revision: 1.1 $
 * $Date: 1997/08/18 20:41:18 $
 * -------------------------------------------------------------
 *  R4000 CP0 register address.
 */

#define Index                      $0           /* Index   register.    */
#define Random                     $1           /* Random pointer       */
#define EntryLo0                   $2           /* TLB entry lo0 (VPN)  */
#define EntryLo1                   $3           /* TLB entry lo1 (VPN)  */
#define Context                    $4           /* Context register.    */
#define PageMask                   $5           /* TLB Page Mask reg.   */
#define Wired                      $6           /* TLB wired register.  */
#define BadVAddr                   $8           /* Bad Virtual address. */
#define Count                      $9           /* Count register.      */
#define EntryHi                    $10          /* High half of TLB     */
#define Compare                    $11          /* Compare register.    */
#define Status                     $12          /* Status  register.    */
#define Cause                      $13          /* Cause   register.    */
#define EPC                        $14          /* Exception PC.        */
#define PRid                       $15          /* Processor ID.        */
#define Config                     $16          /* Configurtation reg.  */
#define LLAddr                     $17          /* Load Linked Address. */
#define WatchLo                    $18          /* Trap addr low.       */
#define WatchHi                    $19          /* Trap addr high.      */
#define XContext                   $20          /* Extended context reg */
#define ECC                        $26          /* ECC register.        */
#define CacheErr                   $27          /* Cache error & status */
#define TagLo                      $28          /* Cache tag regsister. */
#define TagHi                      $29          /* Cache tag regsister. */
#define ErrorEPC                   $30          /* Error Exception PC.  */

/*
 *  Mini definition for configure register.
 */
#define SE                         0x1000       /* The secondary enable */
#define K0                         0x07         /* The kseg0 coherent   */
#define DB                         0x10         /* The pd line size.    */
#define IB                         0x20         /* The pi line size.    */
#define DC                         0x1c0        /* The pd cache size    */
#define IC                         0xe00        /* The pi cache size    */

/*
 *	Define Cause register ExeCode, from IDT79R4600 ORION Processor.
 */
#define	FPE                         0x3c        /* FloatingPoint exception */
#define	Tr                          0x34        /* Trap exception.         */
#define	Ov                          0x30        /* Overflow exception.     */
#define	CpU                         0x2c        /* Cop unuseable exception.*/
#define	RI                          0x28        /* Reserved instruction.   */
#define	Bp                          0x24        /* Breakpoint instruction. */
#define	Sys                         0x20        /* Syscall    instruction. */
#define	DBE                         0x1c        /* Bus error (data r/w).   */
#define	IBE                         0x18        /* Bus error (I fetch).    */
#define	AdES                        0x14        /* Addr error (store).     */
#define	AdEL                        0x10        /* Addr error (load).      */
#define	TLBS                        0x0c        /* TLB exception (store).  */
#define	TLBL                        0x08        /* TLB exception (load).   */
#define	Mod                         0x04        /* TLB modification.       */

/*
 * Cause register other fields.
 */
#define	IP                          0xff00      /* Interrupt pending field */
#define	CE                          0x03000000	/* Cop unit number.  	   */
#define	BD                          0x80000000  /* Delay slot indicator.   */

/*
 * Cache Error register bit fields.
 */
#define CacheErr_ER                 0x80000000  /* Type of reference_      */
#define CacheErr_EC                 0x40000000  /* Cache level of error    */
#define CacheErr_ED                 0x20000000  /* Indicate data field err */
#define CacheErr_ET                 0x10000000  /* Indicate tags field err */
#define CacheErr_ES                 0x08000000  /* External request error  */
#define CacheErr_EE                 0x04000000  /* Error on SysAD_         */
#define CacheErr_EB                 0x02000000  /* Data+Instruction Error  */
#define CacheErr_EI                 0x01000000  /* 2nd error on refill_    */
#define CacheErr_EW                 0x00800000  /* multiple cache error    */
#define CacheErr_ERES               0x00400000  /* reserved_               */
#define CacheErr_SIdx               0x003FFFF8  /* paddr<21:3>             */
#define CacheErr_PIdx               0x00000007  /* vaddr<14:12>            */

		
/*
 * -------------------------------------------------------------
 *
 * $Log: cp0regdef.h,v $
 * Revision 1.1  1997/08/18 20:41:18  philw
 * updated file from bonsai/patch2039 tree
 *
 * Revision 1.3  1996/10/31  21:51:09  kuang
 * Bring Bonsai IP32 debugcard up to the level of IP32 debugcard v2.4 on Pulpwood
 *
 * Revision 1.2  1996/04/04  23:11:00  kuang
 * Added more diagnostic supports
 *
 * Revision 1.1  1995/11/15  00:41:21  kuang
 * initial checkin
 *
 * Revision 1.1  1995/11/14  22:50:38  kuang
 * Initial revision
 *
 * -------------------------------------------------------------
 */

/* END OF FILE */
