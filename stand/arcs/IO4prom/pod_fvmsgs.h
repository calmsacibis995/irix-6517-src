/***********************************************************************\
*           File : pod_fvmsgs.h                                         *
*                                                                       *
*    This file contains the diagnostic messages emitted			*
*    by the IAID, F and VMECC diagnostic routines. This 		*
*    should be merged with the system diagnostic messages		*
*                                                                       *
\***********************************************************************/

#ifndef __POD_FVMSGS_H__
#define __POD_FVMSGS_H__

#ident "arcs/IO4prom/pod_fvmsgs.h $Revision: 1.8 $"

/* Various error messages displayed as part of the diagnostics. */
#ifndef	NEWTSIM

#define	SL_IOA		IO4_slot, Ioa_num

#define	HW_STATE	"HARDWARE STATE: \n"

#define	REGR_RDWR_ERR	"ERROR: Reg bits Rd/Wr. Reg 0x%x, Wrote 0x%x, Read 0x%x\n"
#define	REGHI_RDWR_ERR	"Reg bits (32-63) Rd/Wr Error. Reg 0x%x, Wrote 0x%8x, Read 0x%8x\n"

#define	FCHIP_TEST	"Checking Fchip on IO4 slot %d Adap %d\n", SL_IOA
#define	FCHIP_TESTP	"Passed Checking Fchip on IO4 slot %d Adap %d\n",SL_IOA
#define	FCHIP_TESTF	"FAILED Checking Fchip on IO4 slot %d Adap %d\n",SL_IOA

#define	FCHIP_NEXIST	"FAILED accessing Fchip Version Register\n"

#define	FCHK_REG	"Checking Register bits in Fchip <%d %d>\n", SL_IOA
#define	FCHK_REGP	"Passed Checking Regs bits in Fchip <%d %d>\n",SL_IOA
#define	FCHK_REGF	"FAILED Checking Regs bits in Fchip <%d %d>\n",SL_IOA

#define	FCHK_PONF	"FAILED Fchip during Power-on value checking\n"

#define	FCHK_LWIN	"Testing lwin Regs access in Fchip <%d %d>\n",SL_IOA
#define	FCHK_LWINP	"Passed checking lwin regs access in Fchip <%d %d>\n",SL_IOA
#define	FCHK_LWINF	"FAILED checking lwin regs access in Fchip <%d %d>\n", SL_IOA

#define	FCHK_IMASK	"Checking Intr masks in Fchip <%d %d> \n",SL_IOA
#define	FCHK_IMASKP	"Passed Checking Intr masks in Fchip <%d %d> \n",SL_IOA
#define	FCHK_IMASKF	"FAILED Checking Intr masks in Fchip <%d %d> \n",SL_IOA

#define	FCHK_ADDR	"Checking Register Addrs in Fchip <%d %d>\n", SL_IOA
#define	FCHK_ADDRP	"Passed Checking Regs Addrs in Fchip <%d %d>\n",SL_IOA
#define	FCHK_ADDRF	"FAILED Checking Regs Addrs in Fchip <%d %d>\n",SL_IOA

#define	FCHK_EREG	"Checking Error Register in Fchip <%d %d>\n", SL_IOA
#define	FCHK_EREGP	"Passed Checking Error Reg in Fchip <%d %d>\n",SL_IOA
#define	FCHK_EREGF	"FAILED Checking Error Reg in Fchip <%d %d>\n",SL_IOA

#define	FCHK_TLB	"Checking TLB functions in Fchip <%d %d>\n", SL_IOA
#define	FCHK_TLBP	"Passed Checking TLB funcs in Fchip <%d %d>\n",SL_IOA
#define	FCHK_TLBF	"FAILED Checking TLB funcs in Fchip <%d %d>\n",SL_IOA

#define	FCHK_RSTFCI	"Checking Reset FCI in Fchip <%d %d>\n", SL_IOA
#define	FCHK_RSTFCIP	"Passed Checking Reset FCI in Fchip <%d %d>\n",SL_IOA
#define	FCHK_RSTFCIF	"FAILED Checking Reset FCI in Fchip <%d %d>\n",SL_IOA

#define	FREG_UNKN_TEST	"Fchip <%d %d> Unknown error\n", SL_IOA

#define	FERR_INVAL_VER	"WARNING: Expected version no %d or %d actual %d\n"
#define	FERR_INVAL_MAS  "ERROR: Got invalid master ID %d\n"
#define	FERR_LWIN_VAL	"ERROR: checking Register 0x%x Expected 0x%x Got 0x%x\n"
#define	FERR_IMASK_SET	"ERROR: setting Interrupt Mask \n"
#define	FERR_IMASK_CLR	"ERROR: clearing Interrupt Mask\n"
#define	FERR_NO_ECLR    "ERROR: reading Error clear Reg. Exp:0x%x, Act:0x%x\n"
#define	FERR_ERR_REG    "ERROR: reading Error Reg. Exp:0x%x, Act:0x%x\n"
#define	FERR_REG_ADDR	"ERROR: Reg No 0x%x, Wrote 0x%x  Read 0x%x \n"
#define	FERR_TLB_NOFL   "ERROR: Writing value of 0 to Mask flushed TLB??\n"
#define	FERR_TLB_1FL	"ERROR: F TLB Flushed two entries instead of one\n"
#define	FERR_TLB_FLUSH	"ERROR: Could not flush all F TLB entries \n"
#define	FERR_FCI_FAIL	"ERROR: Could not trigger reset FCI bit Exp:%x Act:%x\n"
#define FERR_ERRB_TEST	"ERROR: F sets wrong error bits. Exp:0x%x Act:0x%x\n"


#define	IAID_NOINT_ERR	"ERROR: IAID didnot send intr on level : %d \n"
#define	IAID_INTR_ERR	"ERROR: Invalid Intr. Exp level %d Got on %d \n"

#define	IAID_INV_BITSET	"ERROR: Wrong Erro Reg bits set: Exp 0x%x, Act 0x%x\n"

#define	IAID_TEST	"Checking IAID in IO4 slot %d\n", IO4_slot
#define	IAID_TESTP	"Passed checking IAID in IO4 slot %d \n", IO4_slot
#define	IAID_TESTF	"FAILED checking IAID in IO4 slot %d \n", IO4_slot

#define	IAID_REGTEST	"Checking Regs of IAID in IO4 slot %d \n", IO4_slot
#define	IAID_REGTESTP	"Passed Checking Regs of IAID in IO4 slot %d\n",IO4_slot
#define	IAID_REGTESTF	"FAILED Checking Regs of IAID in IO4 slot %d\n",IO4_slot

#define	IAID_EREGTEST	"Checking Err Regs of IAID in IO4 slot %d \n", IO4_slot
#define	IAID_EREGTESTP	"Passed Checking Err Regs of IAID in IO4 slot %d\n",IO4_slot
#define	IAID_EREGTESTF	"FAILED Checking Err Regs of IAID in IO4 slot %d\n",IO4_slot

#define	IAID_UNKN_TEST	"IAID <%d> Unknown error\n", IO4_slot

#define	IARAM_TEST	"Checking MAPRAM in IO4 slot %d \n", IO4_slot
#define	IARAM_TESTP	"Passed Checking MAPRAM in IO4 slot %d \n", IO4_slot
#define	IARAM_TESTF	"FAILED Checking MAPRAM in IO4 slot %d \n", IO4_slot

#define	IRAM_RWTEST	"Doing RdWr Test on MAPRAM in IO4 slot %d\n", IO4_slot
#define	IRAM_RWTESTP	"Passed  RdWr Test on MAPRAM in IO4 slot %d\n", IO4_slot
#define	IRAM_RWTESTF	"FAILED  RdWr Test on MAPRAM in IO4 slot %d\n", IO4_slot

#define	IRAM_ADTEST	"Doing Addr Test on MAPRAM in IO4 slot %d\n", IO4_slot
#define	IRAM_ADTESTP	"Passed Addr Test on MAPRAM in IO4 slot %d\n", IO4_slot
#define	IRAM_ADTESTF	"FAILED Addr Test on MAPRAM in IO4 slot %d\n", IO4_slot

#define	IRAM_W1TEST	"Walking 1s Test on MAPRAM in IO4 slot %d\n",IO4_slot
#define	IRAM_W1TESTP	"Passed Walk_1 Test on MAPRAM in IO4 slot %d\n",IO4_slot
#define	IRAM_W1TESTF	"FAILED Walk_1 Test on MAPRAM in IO4 slot %d\n",IO4_slot


#define	IARAM_UNKN_ERR	"MAPRam <%d> Unknown Error\n", IO4_slot

#define	IRAM_RDWR_ERR 	"ERROR: in MAPRAM Rd/Wr. At 0x%x, Wrote %x, Read %x\n"
#define	IRAM_ADDR_ERR	"ERROR: in Addressing. At 0x%x, Wrote %x, Read %x\n"
#define	IRAM_WALK1_ERR	"ERROR: in Walking 1s. At 0x%x Wrote %x, Read %x\n"

#define	VME_TEST	"Checking VMECC on IO4 slot %d Adap %d\n",SL_IOA
#define	VME_TESTP	"Passed Checking VMECC on IO4 slot %d Adap %d\n",SL_IOA
#define	VME_TESTF	"FAILED Checking VMECC on IO4 slot %d Adap %d\n",SL_IOA

#define	VCHK_REGR	"Checking Register in VMECC <%d %d>\n", SL_IOA
#define	VCHK_REGRP	"Passed Checking Registers in VMECC <%d %d>\n", SL_IOA
#define	VCHK_REGRF	"FAILED Checking Registers in VMECC <%d %d>\n", SL_IOA

#define	VCHK_ADDR	"Checking Register Addrs in VMECC <%d %d>\n", SL_IOA
#define	VCHK_ADDRP	"Passed Checking Regs Addrs in VMECC <%d %d>\n", SL_IOA
#define	VCHK_ADDRF	"FAILED Checking Regs Addrs in VMECC <%d %d>\n", SL_IOA

#define	VCHK_EREG	"Checking Error Register in VMECC <%d %d>\n", SL_IOA
#define	VCHK_EREGP	"Passed Checking Error Regs in VMECC <%d %d>\n", SL_IOA
#define	VCHK_EREGF	"FAILED Checking Error Regs in VMECC <%d %d>\n", SL_IOA

#define	VCHK_IMASK	"Checking Intr Masks in VMECC <%d %d>\n", SL_IOA
#define	VCHK_IMASKP	"Passed Checking Intr Masks in VMECC <%d %d>\n", SL_IOA
#define	VCHK_IMASKF	"FAILED Checking Intr Masks in VMECC <%d %d>\n", SL_IOA

#define	VCHK_INTR	"Checking Interrupts in VMECC <%d %d>\n", SL_IOA
#define	VCHK_INTRP	"Passed Checking Interrupts in VMECC <%d %d>\n", SL_IOA
#define	VCHK_INTRF	"FAILED Checking Interrupts in VMECC <%d %d>\n", SL_IOA

#define	VCHK_RMWL	"Checking RMW lpbk in VMECC <%d %d>\n", SL_IOA
#define	VCHK_RMWLP	"Passed Checking RMW lpbk in VMECC <%d %d>\n", SL_IOA
#define	VCHK_RMWLF	"FAILED Checking RMW lpbk in VMECC <%d %d>\n", SL_IOA

#define	VCHK_DMAR	"Checking DMA Rd Time out in VMECC <%d %d>\n", SL_IOA
#define	VCHK_DMARP	"Passed Checking DMA Rd Time out in VMECC <%d %d>\n",SL_IOA
#define	VCHK_DMARF	"FAILED Checking DMA Rd Time out in VMECC <%d %d>\n",SL_IOA

#define	VCHK_DMAW	"Checking DMA Wr Time out in VMECC <%d %d>\n", SL_IOA
#define	VCHK_DMAWP	"Passed Checking DMA Wr Time out in VMECC <%d %d>\n",SL_IOA
#define	VCHK_DMAWF	"FAILED Checking DMA Wr Time out in VMECC <%d %d>\n",SL_IOA

#define	VCHK_RDPRB	"Checking Read Probe err in VMECC <%d %d>\n", SL_IOA
#define	VCHK_RDPRBP	"Passed Checking Rd Probe err in VMECC <%d %d>\n",SL_IOA
#define	VCHK_RDPRBF	"FAILED Checking Rd Probe err in VMECC <%d %d>\n",SL_IOA

#define	VCHK_LPBK	"Checking Loopback path via VMECC <%d %d>\n",SL_IOA
#define	VCHK_LPBKP	"Passed Checking lpbk path via VMECC <%d %d>\n",SL_IOA
#define	VCHK_LPBKF	"FAILED Checking lpbk path via VMECC <%d %d>\n",SL_IOA

#define	VCHK_LPSET	"Setting Loopback path via VMECC <%d %d>\n",SL_IOA
#define	VCHK_LPSETP	"Passed setting lpbk path via VMECC <%d %d>\n",SL_IOA
#define	VCHK_LPSETF	"FAILED setting lpbk path via VMECC <%d %d>\n",SL_IOA

#define	VCHK_LPIO	"Checking PIO Loopback path via VMECC <%d %d> \n",SL_IOA
#define	VCHK_LPIOP	"Passed Checking PIO lpbk path via VMECC <%d %d>\n",SL_IOA
#define	VCHK_LPIOF	"FAILED Checking PIO lpbk path via VMECC <%d %d>\n",SL_IOA

#define	VCHK_FTLB	"Checking F-TLB in VMECC <%d %d> lpbk mode \n", SL_IOA
#define VCHK_FTLBP	"Passed Checking F-TLB in VMECC <%d %d> lpbk mode\n",SL_IOA
#define	VCHK_FTLBF	"FAILED Checking F-TLB in VMECC <%d %d> lpbk mode\n",SL_IOA

#define	VCHK_LPA32	"Checking Loopback in VME A32 mode via VMECC <%d %d>\n", SL_IOA
#define	VCHK_LPA32P	"Passed Checking lpbk in VME A32 mode via VMECC <%d %d>\n", SL_IOA
#define	VCHK_LPA32F	"FAILED Checking lpbk in VME A32 mode via VMECC <%d %d>\n", SL_IOA

#define	VCHK_LPA24	"Checking Loopback in VME A24 mode via VMECC <%d %d>\n", SL_IOA
#define	VCHK_LPA24P	"Passed Checking lpbk in VME A24 mode via VMECC <%d %d>\n", SL_IOA
#define	VCHK_LPA24F	"FAILED Checking lpbk in VME A24 mode via VMECC <%d %d>\n", SL_IOA

#define	VCHK_LPL2A32	"Checking Loopback in VME A32, 2 level map mode via VMECC <%d %d>\n", SL_IOA
#define	VCHK_LPL2A32P	"Passed Checking lpbk in VME A32, 2 level map  mode via VMECC <%d %d>\n", SL_IOA
#define	VCHK_LPL2A32F	"FAILED Checking lpbk in VME A32, 2 level map  mode via VMECC <%d %d>\n", SL_IOA

#define	VCHK_LPL2A24	"Checking Loopback in VME A24, 2 level map mode via VMECC <%d %d>\n", SL_IOA
#define	VCHK_LPL2A24P	"Passed Checking lpbk in VME A24, 2 level map  mode via VMECC <%d %d>\n", SL_IOA
#define	VCHK_LPL2A24F	"FAILED Checking lpbk in VME A24, 2 level map  mode via VMECC <%d %d>\n", SL_IOA

#define	VCHK_LPUNKN	"Unknown Exception occured during VMECC <%d %d> loopback operation\n", SL_IOA

#define	VME_ADDR_ERR	"ERROR: At 0x%x Wrote 0x%8x, Read 0x%8x\n"
#define	VME_CLREG_ERR	"ERROR: reading Error Clear Reg. Exp: 0x%x Got: 0x%x\n"
#define	VME_CAREG_ERR	"ERROR: reading Error Cause Reg. Exp: 0x%x Got: 0x%x\n"
#define	VME_NOINTR_ERR	"ERROR: Failed to receive intr from VMECC level %d\n"
#define	VME_IRESP_ERR	"ERROR: got bad intr vector from VMECC. Expected: %d Got: %d \n"
#define	VME_IMSET_ERR	"ERROR: setting  Intr Mask bit. Exp: %x Act: %x\n"
#define	VME_IMCLR_ERR	"ERROR: clearing Intr Mask bit. Exp: %x Act: %x\n"
 

#define	VMELP_RMW_ERR	"ERROR: in read part of RMW Loopback. Exp: %x Act: %x\n"
#define	VMELP_RMWD_ERR	"ERROR: in writ part of RMW Loopback. Exp: %x Act: %x\n"

#define	VMELP_A32_ERR	"VMECC <%d %d> FAILED in VME A32 loopback, One level Mapping\n",SL_IOA
#define	VMELP_A24_ERR	"VMECC <%d %d> FAILED in VME A24 loopback, One level Mapping\n",SL_IOA
#define	VMELP_A32L2_ERR	"VMECC <%d %d> FAILED in VME A32 loopback, Two level Mapping\n",SL_IOA
#define	VMELP_A24L2_ERR	"VMECC <%d %d> FAILED in VME A24 loopback, Two level Mapping\n",SL_IOA

#define	VMELP_MEMBY_ERR	"ERROR: Byte access Mem:0x%x, Loopback: 0x%x, Exp: 0x%x, Got: 0x%x\n"

#define	VMELP_MEMWD_ERR	"ERROR: half word access Mem:0x%x, Loopback: 0x%x, Exp: 0x%x, Got: 0x%x \n"

#define	VMELP_MEMLO_ERR	"ERROR: word access Mem:0x%x, Loopback: 0x%x, Exp: 0x%x, Got: 0x%x \n"

#define	FTLB_VAL_ERR	"Fchip has invalid TLB value, Exp : %x Act :%x\n"

#define	TLB_ENTRS_FULL "TLB Array used internally in diagnostics is full\n. This may require a change of IO4prom."

#else

#define	HW_STATE		1

#define	FCHIP_TEST		1
#define	FCHIP_TESTP		1
#define	FCHIP_TESTF		1

#define	FCHK_REG		1
#define	FCHK_REGP		1
#define	FCHK_REGF		1

#define	FCHK_IMASK		1
#define	FCHK_IMASKP		1
#define	FCHK_IMASKF		1

#define	FCHK_ADDR		1
#define	FCHK_ADDRP		1
#define	FCHK_ADDRF		1

#define	FCHK_EREG		1
#define	FCHK_EREGP		1
#define	FCHK_EREGF		1

#define	FCHK_TLB		1
#define	FCHK_TLBP		1
#define	FCHK_TLBF		1

#define	FCHK_RSTFCI		1
#define	FCHK_RSTFCIP		1
#define	FCHK_RSTFCIF		1

#define	FERR_IMASK_SET		1
#define	FERR_IMASK_CLR		1
#define	FERR_INVAL_MAS		1
#define	FERR_REG_RDWR		1
#define	FERR_NO_ECLR    	1
#define	FERR_ERR_REG    	1
#define	FERR_REG_ADDR		1
#define	FERR_TLB_FLUSH		1
#define	FERR_TLB_NOFL   	1
#define	FERR_TLB_1FL		1
#define	FERR_INVAL_VER		1
#define	FERR_ERRB_TEST		1

#define	FREG_UNKN_TEST		1

#define	FTLB_VAL_ERR		1
#define	REGR_RDWR_ERR		1 
#define	REGHI_RDWR_ERR		1
#define	TLB_ENTRS_FULL		1

#define IAID_REG_RDWR    	1
#define	IAID_NOINT_ERR		1
#define	IAID_INTR_ERR		1
#define	IAID_INV_BITSET		1

#define	IAID_TEST		1
#define	IAID_TESTP		1
#define	IAID_TESTF		1

#define	IAID_REGTEST		1
#define	IAID_REGTESTP		1
#define	IAID_REGTESTF		1

#define	IAID_EREGTEST		1
#define	IAID_EREGTESTP		1
#define	IAID_EREGTESTF		1

#define	IAID_UNKN_TEST		1

#define	IARAM_TEST		1
#define	IARAM_TESTP		1
#define	IARAM_TESTF		1

#define	IRAM_RWTEST		1
#define	IRAM_RWTESTP		1
#define	IRAM_RWTESTF		1

#define	IRAM_ADTEST		1
#define	IRAM_ADTESTP		1
#define	IRAM_ADTESTF		1

#define	IRAM_W1TEST		1
#define	IRAM_W1TESTP		1
#define	IRAM_W1TESTF		1

#define	IARAM_UNKN_ERR		1

#define	IRAM_RDWR_ERR		1
#define	IRAM_ADDR_ERR		1
#define	IRAM_WALK1_ERR		1


#define	VME_REG_RDWR		1
#define	VME_ADDR_ERR		1
#define	VME_CLREG_ERR		1
#define	VME_CAREG_ERR		1
#define	VME_NOINTR_ERR		1
#define	VME_IRESP_ERR		1
#define	VME_IMSET_ERR		1
#define	VME_IMCLR_ERR		1


#define	VME_TEST		1
#define	VME_TESTP		1
#define	VME_TESTF		1

#define	VCHK_REGR		1
#define	VCHK_REGRP		1
#define	VCHK_REGRF		1

#define	VCHK_ADDR		1
#define	VCHK_ADDRP		1
#define	VCHK_ADDRF		1

#define	VCHK_EREG		1
#define	VCHK_EREGP		1
#define	VCHK_EREGF		1

#define	VCHK_IMASK		1
#define	VCHK_IMASKP		1
#define	VCHK_IMASKF		1

#define	VCHK_INTR		1
#define	VCHK_INTRP		1
#define	VCHK_INTRF		1

#define	VCHK_RMWL		1
#define	VCHK_RMWLP		1
#define	VCHK_RMWLF		1

#define	VCHK_DMAR		1
#define	VCHK_DMARP		1
#define	VCHK_DMARF		1

#define	VCHK_DMAW		1
#define	VCHK_DMAWP		1
#define	VCHK_DMAWF		1

#define	VCHK_RDPRB		1
#define	VCHK_RDPRBP		1
#define	VCHK_RDPRBF		1


#define	VCHK_LPBK		1
#define	VCHK_LPBKP		1
#define	VCHK_LPBKF		1

#define	VCHK_LPSET		1
#define	VCHK_LPSETP		1
#define	VCHK_LPSETF		1

#define	VCHK_LPIO		1
#define	VCHK_LPIOP		1
#define	VCHK_LPIOF		1

#define	VCHK_FTLB		1
#define VCHK_FTLBP		1
#define	VCHK_FTLBF		1

#define	VCHK_LPA32		1
#define	VCHK_LPA32P		1
#define	VCHK_LPA32F		1

#define	VCHK_LPA24		1
#define	VCHK_LPA24P		1
#define	VCHK_LPA24F		1

#define	VCHK_LPL2A32		1
#define	VCHK_LPL2A32P		1
#define	VCHK_LPL2A32F		1

#define	VCHK_LPL2A24		1
#define	VCHK_LPL2A24P		1
#define	VCHK_LPL2A24F		1

#define	VCHK_LPUNKN		1

#define	VMELP_SETUP_ERR		1
#define	VMELP_PIOLP_ERR		1
#define	VMELP_FTLB_ERR		1

#define	VMELP_RMW_ERR		1
#define	VMELP_RMWD_ERR		1
#define	VMELP_A32_ERR		1
#define	VMELP_A24_ERR		1
#define	VMELP_A32L2_ERR		1
#define	VMELP_A24L2_ERR		1
#define	VMELP_UNKN_ERR		1

#define	VMELP_MEMBY_ERR		1
#define	VMELP_MEMWD_ERR		1
#define	VMELP_MEMLO_ERR		1

#endif

#endif
