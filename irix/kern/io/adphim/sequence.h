/* $Header: /proj/irix6.5.7m/isms/irix/kern/io/adphim/RCS/sequence.h,v 1.1 1995/10/04 22:02:11 mwang Exp $ */
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
*  Description:   Master sequencer code header, includes all files containing
*                 variations of AIC-7870 sequencer code. Sequencer code arrays
*                 are labeled seq_00 to seq_15, with corresponding constant
*                 definitions SEQ_00 to SEQ_15. Each array is contained in a
*                 separate file with names seq_00.h to seq_15.h.
*                 The array seq_exist[] is used to determine which sequencer
*                 codesets are available to the HIM.
*                 seq_00.h: STANDARD mode sequencer
*                 seq_01.h: OPTIMA mode sequencer
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

#if !defined( _EX_SEQ00_ )
#include "seq_00.h"
#define  SEQ_00
#endif

#if !defined( _EX_SEQ01_ )
#include "seq_01.h"
#define  SEQ_01
#endif

/* Array indicates existence of sequencer codesets */
typedef struct SeqStruct SeqStruct;
struct SeqStruct
{
   int seq_size;
   int seq_arraypart0;
   int seq_trglunmsk0;
};
SeqStruct SeqExist[16] =
{
   {
      #if defined (SEQ_00)
         sizeof(Seq_00),ARRAY_PARTITION0_0, TARG_LUN_MASK0_0
      #else
         0,0,0
      #endif
   },
   {
      #if defined (SEQ_01)
         sizeof(Seq_01),ARRAY_PARTITION0_1, TARG_LUN_MASK0_1
      #else
         0,0,0
      #endif
   },
   {
      #if defined (SEQ_02)
         sizeof(Seq_02),ARRAY_PARTITION0_2, TARG_LUN_MASK0_2
      #else
         0,0,0
      #endif
   },
   {
      #if defined (SEQ_03)
         sizeof(Seq_03),ARRAY_PARTITION0_3, TARG_LUN_MASK0_3
      #else
         0,0,0
      #endif
   },
   {
      #if defined (SEQ_04)
         sizeof(Seq_04),ARRAY_PARTITION0_4, TARG_LUN_MASK0_4
      #else
         0,0,0
      #endif
   },
   {
      #if defined (SEQ_05)
         sizeof(Seq_05),ARRAY_PARTITION0_5, TARG_LUN_MASK0_5
      #else
         0,0,0
      #endif
   },
   {
      #if defined (SEQ_06)
         sizeof(Seq_06),ARRAY_PARTITION0_6, TARG_LUN_MASK0_6
      #else
         0,0,0
      #endif
   },
   {
      #if defined (SEQ_07)
         sizeof(Seq_07),ARRAY_PARTITION0_7, TARG_LUN_MASK0_7
      #else
         0,0,0
      #endif
   },
   {
      #if defined (SEQ_08)
         sizeof(Seq_08),ARRAY_PARTITION0_8, TARG_LUN_MASK0_8
      #else
         0,0,0
      #endif
   },
   {
      #if defined (SEQ_09)
         sizeof(Seq_09),ARRAY_PARTITION0_9, TARG_LUN_MASK0_9
      #else
         0,0,0
      #endif
   },
   {
      #if defined (SEQ_10)
         sizeof(Seq_10),ARRAY_PARTITION0_10, TARG_LUN_MASK0_10
      #else
         0,0,0
      #endif
   },
   {
      #if defined (SEQ_11)
         sizeof(Seq_11),ARRAY_PARTITION0_11, TARG_LUN_MASK0_11
      #else
         0,0,0
      #endif
   },
   {
      #if defined (SEQ_12)
         sizeof(Seq_12),ARRAY_PARTITION0_12, TARG_LUN_MASK0_12
      #else
         0,0,0
      #endif
   },
   {
      #if defined (SEQ_13)
         sizeof(Seq_13),ARRAY_PARTITION0_13, TARG_LUN_MASK0_13
      #else
         0,0,0
      #endif
   },
   {
      #if defined (SEQ_14)
         sizeof(Seq_14),ARRAY_PARTITION0_14, TARG_LUN_MASK0_14
      #else
         0,0,0
      #endif
   },
   {
      #if defined (SEQ_15)
         sizeof(Seq_15),ARRAY_PARTITION0_15, TARG_LUN_MASK0_15
      #else
         0,0,0
      #endif
   }
};
