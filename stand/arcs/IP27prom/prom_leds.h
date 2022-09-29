/**************************************************************************
 *									  *
 *  File:		prom_leds.h					  *
 *									  *
 *  Contains the led values displayed at various phases in the	  	  *
 *  PROM startup sequence, and on boot failures.			  *
 *									  *
 * 		 Copyright (C) 1994 Silicon Graphics, Inc.	  	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.41 $"

#ifndef _IP27_PROM_LEDS_H_
#define _IP27_PROM_LEDS_H_

/*
 * Progress LED (PLED) values
 *
 *   Warning: if you change these, you will want to also update the
 *   document IP27prom/doc/ip27prom.html.  Avoid big changes if possible!
 */

#define PLED_RESET			0x00
#define PLED_INITCPU			0x01
#define PLED_TESTCP1			0x02
#define PLED_RUNTLB			0x03
#define PLED_TESTICACHE			0x04
#define PLED_TESTDCACHE			0x05
#define PLED_TESTSCACHE			0x06
#define PLED_FLUSHCACHES		0x07
#define PLED_CKHUBLOCAL			0x08
#define PLED_CKHUBCONFIG		0x09
#define PLED_INVICACHE			0x0a
#define PLED_INVDCACHE			0x0b
#define PLED_INVSCACHE			0x0c
#define PLED_INMAIN			0x0d
#define PLED_SPEEDUP			0x0e
#define PLED_SPEEDUPOK			0x0f
#define PLED_INITDCACHE			0x10
#define PLED_INITICACHE			0x11
#define PLED_INITCOP0			0x12
#define PLED_FLUSHTLB			0x13
#define PLED_CLEARTAGS			0x14
#define PLED_CCLFAILED_INITUART		0x15
#define PLED_HUBINIT			0x16
#define PLED_HUBCFAILED_INITUART	0x17
#define PLED_NOCLOCK_INITUART		0x18
#define PLED_HUBINITDONE		0x19
#define PLED_ELSCPROBE			0x1a
#define PLED_JUNKPROBE			0x1b
#define PLED_DONEPROBE			0x1c
#define PLED_UARTINIT			0x1d
#define PLED_UARTINITDONE		0x1e
#define PLED_CKHUBCHIP			0x1f
#define PLED_PODMAIN			0x20
#define PLED_PODLOOP			0x21
#define PLED_PODPROMPT			0x22
#define PLED_PODMODE			0x23
#define PLED_LOCALARB			0x24
#define PLED_SCINIT			0x25
#define PLED_BMARB			0x26
#define PLED_BMASTER			0x27
#define PLED_BARRIER			0x28
#define PLED_CKPDCACHE1			0x29
#define PLED_MAKESTACK			0x2a
#define PLED_MAIN			0x2b
#define PLED_LOADPROM			0x2c
#define PLED_CKSCACHE1			0x2d
#define PLED_CKBT			0x2e
#define PLED_INSLAVE			0x2f
#define PLED_PROMJUMP			0x30
#define PLED_NMI			0x31
#define PLED_INV_IDCACHES		0x32
#define PLED_INV_SCACHE			0x33
#define PLED_WRCONFIG			0x34
#define PLED_RTCINIT			0x35
#define PLED_RTCINITDONE		0x36
#define PLED_LOCK			0x37
#define PLED_BARRIEROK			0x38
#define PLED_LOCKOK			0x39
#define PLED_FPROMINIT			0x3a
#define PLED_FPROMINITDONE		0x3b
#define PLED_JUMPRAMU			0x3c
#define PLED_JUMPRAMUOK			0x3d
#define PLED_JUMPRAMC			0x3e
#define PLED_JUMPRAMCOK			0x3f
#define PLED_STACKRAM			0x40
#define PLED_STACKRAMOK			0x41
#define PLED_SLAVEINT			0x42
#define PLED_SLAVECALL			0x43
#define PLED_SLAVEREND			0x44
#define PLED_LAUNCHLOOP			0x45
#define PLED_LAUNCHINTR			0x46
#define PLED_LAUNCHCALL			0x47
#define PLED_LAUNCHDONE			0x48

#define PLED_UARTBASE			0x49
#define PLED_MDIRINIT			0x4a
#define PLED_MDIRCONFIG			0x4b
#define PLED_I2CINIT			0x4c
#define PLED_I2CDONE			0x4d

#define PLED_CONFIG_INIT		0x4e
#define PLED_IODISCOVER			0x4f
#define PLED_HUB_CONFIG			0x50
#define PLED_ROUTER_CONFIG		0x51
#define PLED_INITIO			0x52
#define PLED_CONSOLE_GET		0x53
#define PLED_CONSOLE_GET_OK		0x54
#define PLED_NOT_USED_55		0x55	/* Avoid using this pattern  */
#define PLED_INITIODONE			0x56
#define PLED_STASH2			0x57
#define PLED_STASH3			0x58
#define PLED_STASH4			0x59

#define PLED_IODISCOVER_DONE		0x5a
#define PLED_NMI_INIT			0x5b
#define PLED_TEST_INTS			0x5c
#define PLED_IORESET			0x5d

#define	PLED_RESET_OK			0x5e	/* CPU came out of reset */
#define	PLED_PRE_I2C_RESET		0x5f	/* Going to reset I2C */
#define	PLED_POST_I2C_RESET		0x60	/* Finished resetting I2C */

/*
 * Failure LED (FLED) values
 *
 *   Warning: if you change these, you will want to also update the
 *   document IP27prom/doc/ip27prom.html.  Avoid big changes if possible!
 */

#define FLED_CP1			0x81	/* CP1 failed 		     */
#define FLED_RESTART			0x82	/* 			     */
#define FLED_ICACHE			0x83	/* icache failed 	     */
#define FLED_DCACHE			0x84	/* dcache failed 	     */
#define FLED_SCACHE			0x85	/* scache failed 	     */
#define FLED_KILLED			0x86	/* CPU disabled by another   */
#define FLED_RTC			0x87	/* Real-time counter broken  */
#define FLED_ECC			0x88	/* 			     */
#define FLED_XTLBMISS			0x89	/* XTLB miss exception 	     */
#define FLED_UTLBMISS			0x8a	/* UTLB miss exception 	     */
#define FLED_KTLBMISS			0x8b	/* KTLB miss exception 	     */
#define FLED_GENERAL			0x8c	/* General Exception 	     */
#define FLED_NOTIMPL			0x8d	/* unimplemented exception   */
#define FLED_CACHE			0x8e	/* Cache error exception     */
#define FLED_OS				0x8f	/* OS requested LEDS 	     */
#define FLED_HUBINTS			0x90	/* Hub ints failed 	     */
#define FLED_HUBLOCAL			0x91	/* Hub local failed 	     */
#define FLED_HUBCONFIG			0x92	/* Hub config failed 	     */
#define FLED_PREM_DIR_REQ		0x93	/* Some node not premium     */
#define FLED_UNUSED1			0x94	/* NMI re-POD requested      */
#define FLED_HUBUART			0x95	/* Hub UART init failed      */
#define FLED_HUBCCS			0x96	/* Hub cross-cpu ints failed */
#define FLED_MAINRET			0x97	/* main() returned 	     */
#define FLED_NOMEM			0x98	/* No local memory 	     */
#define FLED_I2CFATAL			0x99	/* I2C can't happen error    */
#define FLED_DISABLED			0x9a	/* CPU is disabled by envvar */
#define FLED_DOWNLOAD			0x9b	/* Memory download failed    */
#define FLED_COREDEBUG			0x9c	/* Can't set core debug reg  */
#define FLED_IODISCOVER			0x9d	/* Iodiscovery failed	     */
#define FLED_HUB_CONFIG			0x9e	/* Hub klconfig failed       */
#define FLED_ROUTER_CONFIG		0x9f	/* Router klconfig failed    */
#define FLED_HUBIO_INIT			0xa0	/* hub io init failed	     */
#define FLED_CONFIG_INIT		0xa1	/* node klconfig failed	     */
#define FLED_RTRCHIP			0xa2	/* Rtr chip failed diags     */
#define FLED_LINKDEAD			0xa3	/* LLP Link failed diags     */
#define FLED_HUBBIST			0xa4	/* Hub chip failed l/abist   */
#define FLED_RTRBIST			0xa5	/* RTR chip failed l/abist   */
#define FLED_RESETWAIT			0xa6	/* Waiting for reset to go   */
#define FLED_LLP_FAIL			0xa7	/* LLP failed after reset    */
#define FLED_LLP_NORESET		0xa8	/* LLP never up after reset  */
#define FLED_BADMEM			0xa9	/* No good local mem         */
#define FLED_NOT_USED_AA		0xaa	/* Avoid using this pattern  */
#define FLED_NET_DISCOVER		0xab	/* Network discovery failed  */
#define FLED_NASID_CALC			0xac	/* NASID calculation failed  */
#define FLED_ROUTE_CALC			0xad	/* Route calculation failed  */
#define FLED_ROUTE_DIST			0xae	/* Route distribution failed */
#define FLED_NASID_DIST			0xaf	/* NASID distribution failed */
#define FLED_NO_NASID			0xb0	/* Master assigned no NASID  */
#define FLED_NO_MODULEID		0xb1	/* Moduleid arb failed       */
#define	FLED_MIXED_SN00			0xb2	/* SN0 mixed with SN00	     */
#define	FLED_ERRPART			0xb3	/* Error partition config.   */
#define	FLED_MODEBIT			0xb4	/* Error copying modebits    */
#define	FLED_BACK_CALC			0xb5	/* Error calculating back-plane freq    */

#endif /* _IP27_PROM_LEDS_H_ */
