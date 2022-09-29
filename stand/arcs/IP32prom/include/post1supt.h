/*
 * -------------------------------------------------------------
 *  $Revision: 1.1 $
 *  $Date: 1997/08/18 20:46:18 $
 * -------------------------------------------------------------
 */

#if !defined(KSEG0)
#define KSEG0   0x80000000
#endif

#if !defined(KSEG1)
#define KSEG1   0x80000000
#endif

/*
 *  Following defines are used by the secondary cache tests
 */

#define     SCACHESIZE          16*1024*1024
#define     PCACHESIZE          16*1024*1024
#define     TESTADDR            0x1000
#define     ALIASADDR           TESTADDR + SCACHESIZE
#define     PSET                0x2000
#define     PTESTKEY1           0xa5a5a5a5
#define     PTESTKEY2           0x5a5a5a5a
#define     STESTKEY1           0xaaaa5555
#define     STESTKEY2           0x5555aaaa
#define     STESTKEY3           0xdeaddead
#define     MTESTKEY1           0xdeadbeef
#define     MTESTKEY2           0xbeefdead
#define     MTESTKEY3           0xcacaacac
#define     MTESTKEY4           0xacaccaca
#define     MTESTKEY5           0xbeeffeeb
#define     TESTKEY             0xbeef5aa5

/*
 *  CRIME REG defines.
 */
#define CrimeBASE           0xb4000000          /* Crime base address.  */
#define MEMcntlBASE         0xb4000200          /* MEM base address.    */

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


/*
 * common to both C and assembly.
 */
#define SE                  0x00001000          /* The secondary enable */
#define SC                  0x00020000          /* The secondary present*/
#define SS                  0x00300000          /* The secondary size   */

#if defined(_LANGUAGE_ASSEMBLY)

/*
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
#define K0                         0x07         /* The kseg0 coherent   */

/*
 *  Define Cause register ExeCode, from IDT79R4600 ORION Processor.
 */
#define	FPE     0x3c                    /* FloatingPoint exception */
#define	Tr  	0x34                    /* Trap exception.         */
#define	Ov 	0x30                    /* Overflow exception.     */
#define	CpU	0x2c                    /* Cop unuseable exception.*/
#define	RI 	0x28                    /* Reserved instruction.   */
#define	Bp 	0x24                    /* Breakpoint instruction. */
#define	Sys	0x20                    /* Syscall    instruction. */
#define	DBE	0x1c                    /* Bus error (data r/w).   */
#define	IBE	0x18                    /* Bus error (I fetch).    */
#define	AdES	0x14                    /* Addr error (store).     */
#define	AdEL	0x10                    /* Addr error (load).      */
#define	TLBS	0x0c                    /* TLB exception (store).  */
#define	TLBL	0x08                    /* TLB exception (load).   */
#define	Mod 	0x04                    /* TLB modification.       */

/*
 * Cause register other fields.
 */
#define	IP      0xff00                  /* Interrupt pending field */
#define	CE      0x03000000              /* Cop unit number.  	   */
#define	BD      0x80000000              /* Delay slot indicator.   */

/*
 * PRid bit fields.
 */
#define IMP     0xff00                  /* Processor implementation#*/
#define REV     0x00ff                  /* Processor revision ID    */
#define TRITON  0x23                    /* Triton impl #            */

/*
 *  Primary/Secondary cache Instruction/Data
 */
#define PI                  0x00000000 /* Primary Instruction  */
#define PD                  0x00000001 /* Primary Data.        */
#define SI                  0x00000002 /* Secondary Instruc.   */
#define SD                  0x00000003 /* Secondary Data.      */

/*
 *  Cache ops.
 */
#define SDC                 0x00000000 /* Secondary cache clear sequence*/
#define IDXI                0x00000000 /* I-index invalidate            */
#define IWI                 0x00000000 /* Index Writeback Invalidate    */
#define ILT                 0x00000004 /* Index Load Tags               */
#define IST                 0x00000008 /* Index Store Tags              */
#define CDE                 0x0000000C /* Create Dirty Exclusive.       */
#define HitI                0x00000010 /* Hit Invalidate.               */
#define HWI                 0x00000014 /* Hit WriteBack Invalidate.     */
#define FILL                0x00000014 /* I-FILL                        */
#define SCPI                0x00000014 /* Secondary cache page invalid  */
#define HWB                 0x00000018 /* Hit WriteBack.                */
#define HSV                 0x0000001C /* Hit Set Virtual.              */

/*
 * R4600 primary cache state.
 */
#define INVALID             0x0
#define SHARED              0x1
#define CLEAN_EC            0x2
#define DIRTY_EC            0x3

#endif
		
#if defined(_LANGUAGE_C)

#if !defined(__inttypes_INCLUDED)
#include <inttypes.h>
#endif

/*
 * C function prototypes from post1asmsupt.s
 */
void        CPUinterruptON          (uint32_t, uint32_t, uint32_t)  ;
void        CPUinterruptOFF         (uint32_t, uint32_t, uint32_t)  ;
int         TLBentries              (uint32_t)  ;
uint32_t    pcachesize              (uint32_t)  ;
uint32_t    icachesize              (uint32_t)  ;
uint32_t    icache_idx_invalidate   (uint32_t)  ;
uint32_t    mfc0_epc                (void)      ;
void        mtc0_epc                (uint32_t)  ;
uint32_t    mfc0_config             (void)      ;
void        mtc0_config             (uint32_t)  ;
uint32_t    mfc0_CacheErr           (void)      ;
uint32_t    mfc0_status             (void)      ;
uint32_t    mfc0_cause              (void)      ;
void        mtc0_index              (uint32_t)  ;
uint32_t    mfc0_index              (void)      ;
void        mtc0_entrylo0           (uint32_t)  ;
void        mtc0_entrylo1           (uint32_t)  ;
void        mtc0_entryhi            (uint32_t)  ;
void        mtc0_pagemask           (uint32_t)  ;
void        tlb_probe               (void)      ;
uint32_t    tlb_read                (void)      ;
void        tlb_write               (void)      ;
void        mtc0_taglo              (uint32_t)  ;
uint32_t    mfc0_taglo              (void)      ;
void        pcache_hit_invalid      (uint32_t)  ;
void        pcache_hit_wb_invalid   (uint32_t)  ;
void        pcache_hit_wb           (uint32_t)  ;
void        pcache_hit_set_virtual  (uint32_t)  ;
void        pcache_index_wb_invalid (uint32_t)  ;
void        pcache_index_load_tag   (uint32_t)  ;
void        pcache_index_store_tag  (uint32_t)  ;
void        pcache_i_fill           (uint32_t)  ;
void        scache_clear_sequence   (uint32_t)  ;
void        scache_page_invalid     (void)      ;
void        scache_index_load_tag   (uint32_t)  ;
void        scache_index_store_tag  (uint32_t)  ;
void        scache_index_store_tag  (uint32_t)  ;
void        pcache_create_dirty_exclusive (uint32_t)    ;


/*
 * C function prototypes from post1tstsupt.c
 */
void        diag_error_exit         (void)                          ;
void        diag_normal_exit        (void)                          ;
void        pcache_init             (uint32_t, int32_t, uint32_t)   ;
void        scache_init             (uint32_t, int32_t, uint32_t)   ;
void        mem_init                (uint32_t, int32_t, uint32_t)   ;
void        pcache_verify           (uint32_t, int32_t, uint32_t)   ;
void        scache_verify           (uint32_t, int32_t, uint32_t)   ;
void        mem_verify              (uint32_t, int32_t, uint32_t)   ;

/*
 * C function prototypes from post1diags.c
 */
int         mem_scache1             (void)  ;
int         mem_scache2             (void)  ;
int         mem_scache3             (void)  ;
int         mem_scache4             (void)  ;
int         mem_scache5             (void)  ;
int         mem_scache6             (void)  ;
int         mem_scache7             (void)  ;
int         mem_scache8             (void)  ;
int         mem_scache9             (void)  ;
int         mem_scache91            (void)  ;
int         mem_scache10            (void)  ;
int	mace_serial_test	(void)	;

#endif

/*
 * -------------------------------------------------------------
 *
 * $Log: post1supt.h,v $
 * Revision 1.1  1997/08/18 20:46:18  philw
 * updated file from bonsai/patch2039 tree
 *
 * Revision 1.3  1996/10/21  19:17:34  kuang
 * Merged in changes from Pulpwood IP32prom to bring bonsai IP32prom to Pulpwood IP32 PROM v2.4
 *
 * Revision 1.3  1996/07/24  18:07:46  philw
 * add prototype for serial port test
 *
 * Revision 1.2  1995/11/28  02:19:10  kuang
 * Added Triton secondary cache SS SE and SC bits in Config register
 *
 * Revision 1.1  1995/11/16  18:55:35  kuang
 * initial checkin
 *
 * -------------------------------------------------------------
 */

/* END OF FILE */
