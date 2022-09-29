/**************************************************
 * System Coprocessor Registers                   *
 **************************************************/
#define C0_TLBSET   $0
#define C0_INX      $0
#define C0_ENTRYLO  $2
#define C0_UBASE    $4
#define C0_SHIFTAMT $5
#define C0_TRAPBASE $6
#define C0_BADPADDR $7
#define C0_VADDR    $8
#define C0_BADVADDR C0_VADDR
#define C0_COUNTS   $9
#define C0_ENTRYHI  $10
#define C0_ENHI     C0_ENTRYHI
#define C0_SYSMON   $11
#define C0_SR       $12
#define C0_CAUSE    $13
#define C0_EPC      $14
#define C0_PRID     $15
#define C0_CONFIG   $16
#define C0_LLADDR   $17
#define C0_WORK0    $18
#define C0_WORK1    $19
#define C0_PBASE    $20
#define C0_GBASE    $21
#define C0_VP       $22
#define C0_WIRED    $24
#define C0_DCACHE   $28
#define C0_ICACHE   $29

/******************************************************
 * config  register
 ******************************************************/

#define CONFIG_DB       0x0000000000000010
#define CONFIG_IB       0x0000000000000020
#define CONFIG_DCMASK   0x00000000000001c0
#define CONFIG_ICMASK   0x0000000000000e00
#define CONFIG_BE       0x0000000000008000
#define CONFIG_PM       0x0000000100000000
#define CONFIG_ICE      0x0000000200000000
#define CONFIG_SMM      0x0000000400000000


/********************************************************
 * STATUS register			                     		*
 ********************************************************/
#define SR_DM                  0x10000000000
#define SR_KPSMASK             0xf000000000
#define SR_UPSMASK             0xf00000000
#define SR_CUMASK              0x30000000
#define SR_CU0                 0x10000000
#define SR_CU1                 0x20000000
#define SR_FR                  0x4000000
#define SR_RE                  0x2000000
#define SR_SR                  0x100000
#define SR_IBIT11              0x80000
#define SR_IBIT10              0x40000
#define SR_IBIT9               0x20000
#define SR_IBIT8               0x10000
#define SR_IBIT7               0x8000
#define SR_IBIT6               0x4000
#define SR_IBIT5               0x2000
#define SR_IBIT4               0x1000
#define SR_IBIT3               0x0800
#define SR_IBIT2               0x0400
#define SR_IBIT1               0x0200
#define SR_IBIT0               0x0100
#define SR_XX                  0x0040
#define SR_UX                  0x0020
#define SR_KU                  0x0010
#define SR_EXL                 0x0002
#define SR_IE                  0x0001

#define SR_USER                SR_KU

#define	SR_IMASK	           0x7ff00	/* Interrupt mask */
#define SR_IMASK11             0x00000
#define SR_IMASK10             0x40000
#define SR_IMASK9              0x60000
#define	SR_IMASK8	           0x70000	/* mask level 8 */
#define	SR_IMASK7	           0x78000	/* mask level 7 */
#define	SR_IMASK6	           0x7c000	/* mask level 6 */
#define	SR_IMASK5	           0x7e000	/* mask level 5 */
#define	SR_IMASK4	           0x7f000	/* mask level 4 */
#define	SR_IMASK3	           0x7f800	/* mask level 3 */
#define	SR_IMASK2	           0x7fc00	/* mask level 2 */
#define	SR_IMASK1	           0x7fe00	/* mask level 1 */
#define	SR_IMASK0	           0x7ff00	/* mask level 0 */

#define SR_KPS4K               0x0000000000
#define SR_KPS8K               0x1000000000
#define SR_KPS16K              0x2000000000
#define SR_KPS64K              0x3000000000
#define SR_KPS1M               0x4000000000
#define SR_KPS4M               0x5000000000
#define SR_KPS16M              0x6000000000

#define SR_UPS4K               0x000000000
#define SR_UPS8K               0x100000000
#define SR_UPS16K              0x200000000
#define SR_UPS64K              0x300000000
#define SR_UPS1M               0x400000000
#define SR_UPS4M               0x500000000
#define SR_UPS16M              0x600000000

/***************************************************
 * CAUSE Register
 **************************************************/
#define CAUSE_BD               0x8000000000000000	/* Branch delay slot */
#define	CAUSE_IPMASK           0x0fff00	/* Pending interrupt mask */
#define	CAUSE_EXCMASK          0x00f8	/* Cause code bits */
#define CAUSE_CE	       0x10000000
#define CAUSE_NMI	       0x08000000
#define CAUSE_BE	       0x04000000
#define CAUSE_VCI	       0x02000000
#define CAUSE_FPI	       0x01000000
#define CAUSE_IP11             0x80000  
#define CAUSE_IP10             0x40000
#define CAUSE_IP9              0x20000
#define	CAUSE_IP8              0x10000	/* External level 8 pending */
#define	CAUSE_IP7              0x8000	/* External level 7 pending */
#define	CAUSE_IP6              0x4000	/* External level 6 pending */
#define	CAUSE_IP5              0x2000	/* External level 5 pending */
#define	CAUSE_IP4              0x1000	/* External level 4 pending */
#define	CAUSE_IP3              0x0800	/* External level 3 pending */
#define CAUSE_IP2              0x0400
#define	CAUSE_IP1              0x0200	/* Software level 2 pending */
#define	CAUSE_IP0              0x0100	/* Software level 1 pending */
#define	CAUSE_SW2              0x0200	/* Software level 2 pending */
#define	CAUSE_SW1              0x0100	/* Software level 1 pending */

#define	CAUSE_IPSHIFT            8
#define	CAUSE_EXCSHIFT           3


/* Cause register exception codes */
#define	EXC_CODE(x)	((x)<<3)

#define	EXC_INT		EXC_CODE(0)	/* interrupt */
#define	EXC_MOD		EXC_CODE(1)	/* TLB mod */
#define	EXC_TLBL	EXC_CODE(2)	/* Mnemonic compatible */
#define	EXC_TLBS	EXC_CODE(3)	/* Mnemonic compatible */
#define	EXC_ADEL	EXC_CODE(4)	/* Mnemonic compatible */
#define	EXC_ADES	EXC_CODE(5)	/* Mnemonic compatible */
#define	EXC_SYSCALL	EXC_CODE(8)	/* SYSCALL */
#define	EXC_SYS		EXC_CODE(8)	/* Mnemonic compatible */
#define	EXC_BREAK	EXC_CODE(9)	/* BREAKpoint */
#define	EXC_BP		EXC_CODE(9)	/* Mnemonic compatible */
#define	EXC_RI		EXC_CODE(10)	/* Mnemonic compatible */
#define	EXC_CPU		EXC_CODE(11)	/* CoProcessor Unusable */
#define	EXC_OV		EXC_CODE(12)	/* OVerflow */
#define	EXC_TRAP	EXC_CODE(13)	/* trap instruction */

/********************************************************
 * ENTRYHI register			                     		*
 ********************************************************/
#define ENTRYHI_ASIDMASK       0x0000000000000ff0
#define ENTRYHI_VPNMASK        0xf800fffffff80000
#define ENTRYHI_VPNSHIFT       19
#define ENTRYHI_ASIDSHIFT      4
#define ENTRYHI_RMASK          0xc000000000000000
#define ENTRYHI_CMASK          0x3800000000000000
#define ENTRYHI_RCMASK         0xf800000000000000
#define ENTRYHI_KV0            0x4000000000000000
#define ENTRYHI_KV1            0xc000000000000000
#define ENTRYHI_KV1_NEG        0xffff000000000000
#define ENTRYHI_KP             0x8000000000000000
#define ENTRYHI_UV             0x0000000000000000

/********************************************************
 * ENTRYLO register			                     		*
 ********************************************************/
#define ENTRYLO_VMASK          0x0000000000000080
#define ENTRYLO_DMASK          0x0000000000000100
#define ENTRYLO_CMASK          0x0000000000000e00
#define ENTRYLO_PFNMASK        0x000000fffffff000
#define ENTRYLO_VSHIFT         7
#define ENTRYLO_DSHIFT         8
#define ENTRYLO_CSHIFT         9
#define ENTRYLO_PFNSHIFT       12

/********************************************************
 * TLBSET register			                     		*
 ********************************************************/
#define TLBSET_PMASK           0x8000000000000000
#define TLBSET_SETMASK         0x0000000000000007

/********************************************************
 * COUNTS register			                     		*
 ********************************************************/
#define COUNTS_CYCLESMASK      0xffffffff00000000
#define COUNTS_OPERATIONSMASK  0x00000000ffffffff

/********************************************************
 * ICACHE register			                     		*
 ********************************************************/
#define ICACHE_ASIDMASK        0x0000ff0000000000
#define ICACHE_ASIDSHIFT       40
#define ICACHE_ASIDSHIFT1      31
#define ICACHE_ASIDSHIFT2       9
/* 
 * These constants describe the way the virtual and physical 
 * addresses are broken up into page number and offset.  Although
 * this information can be determined from fields in the ENTRYHI
 * and ENTRYLO registers, these constants give us another level
 * of indirection in case the bit positions of those registers
 * change.
 */
#define PAGE_SIZE_4K
#ifdef PAGE_SIZE_4K
#define VADDR_OFFSETMASK       0x0000000000000fff
#define VADDR_VPNMASK          0xf800fffffffff000
#define VADDR_VADDRMASK        0xf800ffffffffffff
#define VADDR_VPNSHIFT         12
#define PADDR_OFFSETMASK       0x0000000000000fff
#define PADDR_PFNMASK          0x000000fffffff000
#define PADDR_PFNSHIFT         12
#define VADDR_ICACHEINDEXMASK  0xf800ffffffffffe0
#endif


/********************************************************
 * The following constants are provided for             *
 * compatability with niblet                            *
 ********************************************************/
#define PFNSHIFT               ENTRYLO_PFNSHIFT
#define TLBLO_PFNSHIFT         ENTRYLO_PFNSHIFT
#define TLBLO_CSHIFT           ENTRYLO_CSHIFT
#define TLBLO_DSHIFT           ENTRYLO_DSHIFT
#define TLBLO_VSHIFT           ENTRYLO_VSHIFT
#define	TLBLO_PFNMASK	       ENTRYLO_PFNMASK
#define	TLBLO_DMASK	           ENTRYLO_DMASK
#define	TLBLO_VMASK	           ENTRYLO_VMASK

#define	NTLBENTRIES	128

#define TLBHI_VPNSHIFT         ENTRYHI_VPNSHIFT
#define TLBHI_VPNMASK          ENTRYHI_VPN
#define TLBHI_ASIDMASK         ENTRYHI_ASID
#define TLBHI_PID              ENTRYHI_ASID

/***********************************************************
 * fix the opcodes for tlb probe/read instructions for tfp
 */

#define tlbp	tlbp1
#define tlbr	tlbr1
