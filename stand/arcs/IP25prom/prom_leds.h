/**************************************************************************
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

#ident "$Revision: 1.3 $"
/***********************************************************************\
*	File:		prom_leds.h					*
*									*
*	Contains the led values displayed at various phases in the	*
*	PROM startup sequence. 						*
*									*
\***********************************************************************/

#ifndef _IP25_PROM_LEDS_H_
#define _IP25_PROM_LEDS_H_

#define PLED_STARTUP                    1	/* PROM started */ 
#define PLED_TESTCP1                    2	/* test FPU registers */ 
#define PLED_TESTICACHE                 3	/* test icache */ 
#define PLED_TESTDCACHE                 4	/* test dcache */ 
#define PLED_TESTSCACHE                 5	/* test secondary cache */ 
#define PLED_CCTAGS                     6	/* Testing CC dup tags */ 
#define PLED_CKCCLOCAL                  7	/* Testing CC local regs */ 
#define PLED_CKCCCONFIG                 8	/* */ 
#define PLED_INVICACHE                  9	/* inval primary i caches */ 
#define PLED_INVDCACHE                  10	/* inval primary d cache */ 
#define PLED_INVSCACHE                  11	/* invalidate 2ndary cache */ 
#define PLED_INVCCTAGS                  12	/* Invalidate CC tags */ 
#define PLED_INITDCACHE                 13	/* Init stack in DCACHE */ 
#define PLED_INITICACHE                 14	/* Init code in ICACHE */ 
#define PLED_INITCOP0                   15	/* */ 
#define PLED_FLUSHTLB                   16	/* flush tlb */ 
#define PLED_CLEARTAGS                  17	/* Clearing the pd tags */ 
#define PLED_CCLFAILED_INITUART         18	/* UART init failed */ 
#define PLED_CCINIT1                    19	
#define PLED_CCCFAILED_INITUART         20	/* */ 
#define PLED_NOCLOCK_INITUART           21	/* */ 
#define PLED_CCINIT2                    22	/* */ 
#define PLED_UARTINIT                   23	/* */ 
#define PLED_CCUARTDONE                 24	/* Finished init CC-UART */ 
#define PLED_CKACHIP                    25	/* Testing A-chip regs */ 
#define PLED_AINIT                      26	/* Initializing the A ahip */ 
#define PLED_CKEBUS1                    27	/* Checking EBus ints. */ 
#define PLED_SCINIT                     28	/* Init system controller */ 
#define PLED_BMARB                      29	/* Arbitrating bootmaster */ 
#define PLED_BMASTER                    30	/* Processor is bootmaster */ 
#define PLED_CCJOIN                     31	/* Test CC join register */ 
#define PLED_POD                        32	/* Setting up POD mode */ 
#define PLED_PODLOOP                    33	/* Entering POD */ 
#define PLED_CKPDCACHE1                 34	/* Checking pd cache */ 
#define PLED_MAKESTACK                  35	/* Initing PD Stack */ 
#define PLED_MAIN                       36	/* Calling main() */ 
#define PLED_LOADPROM                   37	/* Loading IO4 prom */ 
#define PLED_CKSCACHE1                  38	/* Test SCACHE RAM */ 
#define PLED_CKBT                       39	/* TEST SCACHE TAGS */ 
#define PLED_INSLAVE                    40	/* Entering slave mode */ 
#define PLED_PROMJUMP                   41	/* Jumping to IO4prom */ 
#define PLED_NMI                        42	/* Jumping to NMIVEC */ 
#define PLED_INV_IDCACHES               43	/* Slave inval I/D cache */ 
#define PLED_INV_SCACHE                 44	/* Slave inval S cache */ 
#define PLED_WRCONFIG                   45	/* Write CONDIF entry */ 

/*
 * Failure leds ...
 */

#define FLED_CP1                        0x20+1	/* CP1 failed */ 
#define FLED_RESTART                    0x20+2	/* */ 
#define FLED_ICACHE                     0x20+3	/* icache failed*/ 
#define FLED_DCACHE                     0x20+4	/* dcache failed*/ 
#define FLED_SCACHE                     0x20+5	/* scache failed */ 
#define FLED_CCTAGS                     0x20+6	/* scache tags failed (CC) */ 
#define FLED_BADGDA                     0x20+7	/* */ 
#define FLED_ECC                        0x20+8	/* */ 
#define FLED_XTLBMISS                   0x20+9	/* XTLB miss exception */ 
#define FLED_UTLBMISS                   0x20+10	/* UTLB miss exception */ 
#define FLED_KTLBMISS                   0x20+11	/* KTLB miss exception */ 
#define FLED_GENERAL                    0x20+12	/* General Exception */ 
#define FLED_NOTIMPL                    0x20+13	/* unimplemented exception */ 
#define FLED_CACHE                      0x20+14	/* Cache error exception */ 
#define FLED_OS                         0x20+15	/* OS requested LEDS */ 
#define FLED_BROKEWB                    0x20+16	/* Broke qait barrier */ 
#define FLED_CCLOCAL                    0x20+17	/* CC local failed */ 
#define FLED_CCCONFIG                   0x20+18	/* CC Config failed */ 
#define FLED_CCCLOCK                    0x20+19	/* CC clock failed */ 
#define FLED_ACHIP                      0x20+20	/* Achip failed diags */ 

#endif
