/*
 * =========================================================
 * $Revision: 1.1 $
 * $Date: 1997/08/18 20:41:24 $    
 * $Author: philw $   
 * =========================================================
 *  Crime register address map and more....
 */

/*
 * The base address for Crime
 */
#define CrimeBASE           0xb4000000          /* Crime base address.  */
#define MEMcntlBASE         0xb4000200          /* MEM base address.    */
#define MEMoffset           0x00000200          /* MEM base offset.     */

/*
 *  ID and Revision Register.
 */
#define CrimeID             0x00000000          /* ID register.         */
#define     IDMASK          0x000000f0          /* Crime ID mask.       */
#define     REVMASK         0x0000000f          /* Crime revision mask. */

/*
 *  Crime control register.
 */
#define CrimeCNTL           0x00000008          /* Control register.    */
#define     SysADcTriton    0x00002000          /* Triton check SysADC  */
#define     SysADcCrime     0x00001000          /* Crime check SysADC   */
#define     Softreset       0x00000800          /* Softreset            */
#define     WARMRESET       0x00000400          /* Triton warm reset.   */
#define     WATCHDOG        0x00000200          /* Watchdog timer enable*/
#define     ENDIAN          0x00000100          /* big/little 1/0       */
#define     WBMARK          0x000000f0          /* high water mark mask.*/
#define     CMDQMARK        0x0000000f          /* command Q high wmask.*/

/*
 *  Interrupt status register, interrupt enable register,    
 *  Software interrupt register, and Hardware interrupt register.
 */
#define CrimeIntrStatus     0x00000010          /* Interrupt Status.    */
#define CrimeIntrEnable     0x00000018          /* Interrupt Enable.    */
#define CrimeSoftIntr       0x00000020          /* Software Interrupt.  */
#define CrimeHardIntr       0x00000028          /* Hardware Interrupt.  */
#define     VICE_INT        0x80000000          /* VICE interrupt.      */
#define     SOFT_INT1       0x20000000          /* Software interrupt1  */
#define     SOFT_INT0       0x10000000          /* Software interrupt0  */
#define     RE_INT1         0x00800000          /* RE interrupt1        */
#define     RE_INT0         0x00400000          /* RE interrupt0        */
#define     MEMerr_INT      0x00200000          /* Memory Err interrupt.*/
#define     CRIMEerr_INT    0x00100000          /* Crime  Err interrupt.*/
#define     GBEerr_INT      0x000f0000          /* GBE interrupt mask.  */
#define     MACEerr_INT     0x0000ffff          /* MACE interrupt mask. */
#define     NORM_INT_STAT   0x0B400000

/*
 *  WatchDog Timer register.
 */
#define CrimeWatchDog       0x00000030          /* Watch Dog timer.     */
#define     WATCHDOG_ALARM  0x00100000          /* WatchDog time out.   */
#define     WATCHDOG_MASK   0x000fffff          /* WatchDog value mask. */

/*
 *  Crime timer register.
 */
#define CrimeTimer          0x00000038          /* Crime Timer.         */

/*
 *  CPU Error Address register, and CPU Error status register.
 */
#define CPUErrADDR          0x00000040          /* CPU Error Address.   */
#define CPUErrSTATUS        0x00000048          /* CPU Error Status.    */
#define     SYSCMD_INVALID  0x00000010          /* SysCMD Invalid cmd.  */
#define     SYSAD_PARITY    0x00000008          /* SysAD parity error.  */
#define     SYSCMD_PARITY   0x00000004          /* SysCMD parity error. */
#define     CPUinvalidADDR  0x00000002          /* CPU  invalid address.*/
#define     CPUinvalidREG   0x00000001          /* Triton invalid reg.  */
#define CPUErrENABLE        0x00000050          /* CPU Error ENABLE.    */
#define     CPUinvalidADDr  0x00000200          /* CPU invalid read.    */
#define     CPUillegalINST  0x00000010          /* CPU Illegal inst.    */
#define     CPUSysADC       0x00000008          /* CPU SysAD parity.    */
#define     CPUSysCMDC      0x00000004          /* CPU SysCMD parity    */
#define     CPUinvalidADDP  0x00000002          /* CPU Illegal addr.    */
#define     CPUinvalidREGP  0x00000001          /* CPU Illegal reg.     */
#define     CPUallIllegal   0x0000001F          /* everything except CMDC */
#define VICEerrADDR         0x00000058          /* VICE Error Addr.     */

/*
 *  Memory controller Status/control register.
 */
#define     MEMCntl         0x00000200          /* Memory control reg.  */
#define     ECCREPL         0x00000002          /* Use ECC replacement  */
                                                /* check bits.          */
#define     ECCENABLE       0x00000001          /* Enable ECC checking. */

/*
 *  Bank 0 - 7 control register.
 */
#define MEMBank0CNTL        0x00000208          /* Bank0 control reg.   */
#define MEMBank1CNTL        0x00000210          /* Bank1 control reg.   */
#define MEMBank2CNTL        0x00000218          /* Bank2 control reg.   */
#define MEMBank3CNTL        0x00000220          /* Bank3 control reg.   */
#define MEMBank4CNTL        0x00000228          /* Bank4 control reg.   */
#define MEMBank5CNTL        0x00000230          /* Bank5 control reg.   */
#define MEMBank6CNTL        0x00000238          /* Bank6 control reg.   */
#define MEMBank7CNTL        0x00000240          /* Bank7 control reg.   */

/*
 *  Refresh Counter Register.
 */
#define MEMRefshCNTR        0x00000248          /* Refresh counter.     */
#define     REFRESH_MASK    0x000007ff          /* Refresh counter mask */

/*
 *  Memory Error status register and memory error address register.
 */
#define MEMErrSTATUS        0x00000250          /* Memory error status. */
#define MEMErrADDR          0x00000258          /* Memory error address.*/
#define     INVDADDR        0x02000000          /* Invalid mem address. */
#define     ECCWRERR        0x01000000          /* Ecc read error.      */
#define     ECCRDERR        0x00800000          /* Ecc read error.      */
#define     MHARDERR        0x00400000          /* Multiple hard error. */
#define     HARDERR         0x00200000          /* First hard error.    */
#define     SOFTERR         0x00100000          /* First soft error.    */
#define     CPUECC          0x00040000          /* CPU cause ecc error. */
#define     VICEECC         0x00020000          /* VICE cause ecc err.  */
#define     GBEECC          0x00010000          /* GBE cause ecc error. */
#define     REECC           0x00008000          /* RE  cause ecc error. */
#define     REIDMASK        0x00007f00          /* RE source id mask.   */
#define     MACEECC         0x00000080          /* MACE cause ecc err.  */
#define     MACEMASK        0x0000007f          /* MACE source id mask. */

/*
 *  ECC Syndrome register, ECC Check bits register, and
 *  ECC replacement check bits register.
 */
#define EccSyndrome         0x00000260          /* ECC syndrome bits.   */
#define EccCheckbits        0x00000268          /* ECC check bits.      */
#define EccReplchkbits      0x00000270          /* ECC cehck bit replace*/

#if defined(_LANGUAGE_C)

#ifndef uint64_t
#include <inttypes.h>
#endif

struct CRIMEREG {
    uint64_t    id              ;   /* Crime id register.         00*/
    uint64_t    cntl            ;   /* Crime control register.    08*/
    uint64_t    intrpt_status   ;   /* Crime interrupt status.    10*/
    uint64_t    intrpt_enable   ;   /* Crime interrupt enable.    18*/
    uint64_t    soft_intrpt     ;   /* Crime software interrupt.  20*/
    uint64_t    hard_intrpt     ;   /* Crime hardware interrupt.  28*/
    uint64_t    watch_dog       ;   /* Crime watchdog timer.      30*/
    uint64_t    crime_timer     ;   /* Crime timer.               38*/
    uint64_t    cpu_error_addr  ;   /* Crime cpu error address.   40*/
    uint64_t    cpu_error_status;   /* Crime cpu error status.    48*/
    uint64_t    cpu_error_enable;   /* Crime cpu error enable.    50*/
    uint64_t    vice_err_address;   /* Crime vice err address.    58*/
} ;


struct MEMREG {    
    uint64_t    status          ;   /* MEMcntl status register.   00*/
    uint64_t    bank0_cntl      ;   /* MEMcntl bank0 control reg. 08*/
    uint64_t    bank1_cntl      ;   /* MEMcntl bank1 control reg. 10*/
    uint64_t    bank2_cntl      ;   /* MEMcntl bank2 control reg. 18*/
    uint64_t    bank3_cntl      ;   /* MEMcntl bank3 control reg. 20*/
    uint64_t    bank4_cntl      ;   /* MEMcntl bank4 control reg. 28*/
    uint64_t    bank5_cntl      ;   /* MEMcntl bank5 control reg. 30*/
    uint64_t    bank6_cntl      ;   /* MEMcntl bank6 control reg. 38*/
    uint64_t    bank7_cntl      ;   /* MEMcntl bank7 control reg. 40*/
    uint64_t    refresh_cntr    ;   /* MEMcntl refresh counter.   48*/
    uint64_t    mem_error_status;   /* MEMcntl error status.      50*/
    uint64_t    mem_error_addr  ;   /* MEMcntl error  address.    58*/
    uint64_t    ecc_syndrome    ;   /* MEMcntl ecc syndrome.      60*/
    uint64_t    ecc_check_bits  ;   /* MEMcntl generated ecc.     68*/
    uint64_t    ecc_repl        ;   /* MEMcntl ecc replacement    70*/
} ;

typedef struct CRIMEREG *crime_reg  ;
typedef struct MEMREG   *mem_reg    ;

#endif

/*
 * ========================================
 *
 * $Log: crimereg.h,v $
 * Revision 1.1  1997/08/18 20:41:24  philw
 * updated file from bonsai/patch2039 tree
 *
 * Revision 1.3  1996/10/31  21:51:12  kuang
 * Bring Bonsai IP32 debugcard up to the level of IP32 debugcard v2.4 on Pulpwood
 *
 * Revision 1.3  1996/10/04  20:42:10  kuang
 * Added NORM_INT_STAT define
 *
 * Revision 1.2  1996/04/04  23:11:01  kuang
 * Added more diagnostic supports
 *
 * Revision 1.1  1996/03/25  22:17:24  kuang
 * These should fix the PHYS_TO_K1 problem
 *
 * Revision 1.9  1995/08/07  19:55:02  kuang
 * Added cpu read invalid address (enable) bit in CPU/VICE error status
 * register.
 *
 * Revision 1.10  1995/08/07  19:50:11  kuang
 * Added CPU invalid read address
 * bit.
 *
 * Revision 1.9  1995/07/11  18:03:55  kuang
 * *** empty log message ***
 *
 * Revision 1.8  1995/07/05  20:12:54  gardner
 * removed mem row register hit/miss status bits
 *
 * Revision 1.7  1995/06/29  02:46:41  kuang
 * Defined kseg1 address for memory controller registers.
 *
 * Revision 1.8  1995/06/29  02:44:53  kuang
 * Added memory controller base address for kseg1 address space.
 *
 * Revision 1.7  1995/06/08  01:38:06  kuang
 * fixed the memcontroll register.
 *
 * Revision 1.6  1995/05/19  01:53:50  kuang
 * Fixed the CrimeBASE problem for triton model
 *
 * Revision 1.5  1995/05/18  01:22:20  kuang
 * Fixed the missing Softreset problem.
 *
 * Revision 1.4  1995/05/18  01:19:11  kuang
 * Changed several crime register defines to reflect
 * the crime register changes.
 *
 * Revision 1.3  1995/04/21  22:08:07  kuang
 * Changed to the new CRIME register map.
 *
 *
 * ========================================
 */

/* END OF FILE */
