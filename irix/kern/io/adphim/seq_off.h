/* $Header: /proj/irix6.5.7m/isms/irix/kern/io/adphim/RCS/seq_off.h,v 1.1 1995/10/04 22:02:10 mwang Exp $ */
/***************************************************************************
*                                                                          *
* Copyright 1993 Adaptec, Inc.,  All Rights Reserved.                      *
*                                                                          *
* This software contains the valuable trade secrets of Adaptec.  The       *
* software is protected under copyright laws as an unpublished work of     *
* Adaptec.  Notice is for informational purposes only and does not imply   *
* publication.  The user of this software may make copies of the software  *
* for use with parts manufactured by Adaptec or under license from Adaptec *
* and for no other use.                                                    *
*                                                                          *
***************************************************************************/

/***************************************************************************
*
*  Module Name:   SEQUENCE.H
*
*  Description:   Sequencer code address definitions.
*
*  Programmers:   Paul von Stamwitz
*                 Chuck Fannin
*                 Jeff Mendiola
*                 Harry Yang
*    
*  Notes:         NONE
*
*  Revisions -
*
***************************************************************************/

#ifdef MIPS_BE

/* register offsets - must be converted */
#define DISCON_OPTION 		0x0031
#define PASS_TO_DRIVER 		0x003f
#define ACTIVE_SCB 		0x0038
#define WAITING_SCB 		0x0039
#define XFER_OPTION 		0x0023
#define FAST20_LOW		0x0033
#define FAST20_HIGH		0x0032

/* byte defs - no conversion */
#define SIOSTR3_ENTRY 		0x0008
#define ATN_TMR_ENTRY 		0x0010
#define START_LINK_CMD_ENTRY	0x0004
#define IDLE_LOOP_ENTRY 	0x0000
#define SEQUENCER_PROGRAM 	0x0000
#define SIO204_ENTRY 		0x000c

#else /* MIPS_BE */

#define DISCON_OPTION 		0x0032
#define SIOSTR3_ENTRY 		0x0008
#define ATN_TMR_ENTRY 		0x0010
#define PASS_TO_DRIVER 		0x003c
#define START_LINK_CMD_ENTRY 		0x0004
#define FAST20_LOW		0x0030
#define FAST20_HIGH		0x0031
#define IDLE_LOOP_ENTRY 		0x0000
#define ACTIVE_SCB 		0x003b
#define WAITING_SCB 		0x003a
#define SEQUENCER_PROGRAM 		0x0000
#define XFER_OPTION 		0x0020
#define SIO204_ENTRY 		0x000c

#endif /* MIPS_BE */

#if !defined( _EX_SEQ00_ )
#define TARG_LUN_MASK0_0 		0x05a0
#define ARRAY_PARTITION0_0 		0x05a8
#endif

#if !defined( _EX_SEQ01_ )

#define TARG_LUN_MASK0_1 		0x0000       /*NOT DEFINED IN SEQ_01 */
#define ARRAY_PARTITION0_1 		0x0000    /*NOT DEFINED IN SEQ_01 */

#endif
