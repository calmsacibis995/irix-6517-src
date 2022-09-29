/*
 * crmDefs.h - IP32 Macros for Graphics Diagnostics
 *
 *
 *
 */

/*	
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ifndef __CRMDEFS_H__
#define __CRMDEFS_H__

#ident "$Revision: 1.1 $"

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

/*******************************/
/* Section xx: Temporary Space */
/*******************************/
#define	GBE_CMAP_WAIT_LEVEL	16

/*************************************/
/* Section xx: Macros for GBE Access */
/*************************************/

#define gbeSetReg(hwpage, reg, val ) \
    *((volatile unsigned int *) (&((hwpage)->reg))) = (val)

#define gbeSetTable(hwpage, reg, index, val) \
    *((volatile unsigned int *) (&((hwpage)->reg[index]))) = (val)

#define gbeGetReg(hwpage, reg, val) \
   (val) = *((volatile unsigned int *) &((hwpage)->reg))

#define gbeGetTable(hwpage, reg, index, val) \
   (val) = *((volatile unsigned int *) &((hwpage)->reg[index]))

#if 1
extern void crmGBEWaitCmapFifo(void);
#define gbeWaitCmapFifo(hwpage, val)	crmGBEWaitCmapFifo()
#else
#define gbeWaitCmapFifo(hwpage, val)			\
{							\
   int tempReadVal;					\
							\
   while(1)						\
   {							\
      gbeGetReg( hwpage, cm_fifo, tempReadVal);		\
      tempReadVal &= GBE_CMFIFO_MASK;			\
							\
      if ((tempReadVal == 0x00000000) ||		\
	  (tempReadVal > val))				\
	 break;						\
   }							\
}
#endif

#if 1
extern void crmGBEWaitCmapFifoEmpty(void);
#define gbeWaitCmapFifoEmpty(hwpage)	crmGBEWaitCmapFifoEmpty()
#else
#define gbeWaitCmapFifoEmpty(hwpage)			\
{							\
   int tempReadVal;					\
							\
   while(1)						\
   {							\
      gbeGetReg( hwpage, cm_fifo, tempReadVal);		\
      tempReadVal &= GBE_CMFIFO_MASK;			\
							\
      if (tempReadVal == 0x00000000)			\
	 break;						\
   }							\
}
#endif

#if 1
extern void crmGBEWaitForBlanking(crime_timing_info_t *vtp);
#define gbeWaitForBlanking(hwpage, timing)	crmGBEWaitForBlanking(timing)
#else
#define gbeWaitForBlanking(hwpage, timing)		\
{							\
   int maxX, maxY;					\
   int tempReadVal, tempX, tempY;			\
							\
   maxX = ((timing)->hblank_end) - ((timing)->hblank_start); \
   maxX /= 2;						\
   maxX += ((timing)->hblank_start);			\
							\
   maxY = ((timing)->vblank_end) - ((timing)->vblank_start); \
   maxY /= 2;						\
   maxY += ((timing)->vblank_start);			\
							\
   while(1)						\
   {							\
      gbeGetReg( hwpage, vt_xy, tempReadVal);		\
      tempX = tempReadVal & 0x00000fff;			\
      tempY = (tempReadVal & 0x00fff000) >> 12;		\
							\
      if ((tempX >= ((timing)->hblank_start)) &&	\
          (tempX <= maxX) &&				\
          (tempY >= ((timing)->vblank_start)) &&	\
          (tempY <= maxY))				\
      {							\
	 break;						\
      }							\
   }							\
}
#endif


/************************************/
/* Section xx: Macros for RE Access */
/************************************/

#define crmSet(_hwpage, _reg, _val ) \
   *((volatile unsigned int *) (&((_hwpage)->_reg))) = (_val);

#define crmSetAndGo(_hwpage, _reg, _val ) \
   *((volatile unsigned int *) (((unsigned int) (&((_hwpage)->_reg))) | 0x0800)) = _val;

#define crmSet64(_hwpage, _reg, _val ) \
{									  \
   unsigned long long temp;						  \
   temp = (_val);							  \
   (*((volatile double *) (&((_hwpage)->_reg)))) = *((double *) (&temp)); \
}

#define crmSet64AndGo(_hwpage, _reg, _val ) \
   (*((volatile double *) ((((unsigned int) &((_hwpage)->_reg))) | 0x800))) = (_val);

#define crmGet(_hwpage, _reg, _val ) \
   (*((volatile unsigned int *) &(_val))) = (*((volatile unsigned int *) (&((_hwpage)->_reg))))

#define crmGet64(_hwpage, _reg, _val ) \
   (*((volatile double *) &(_val))) = (*((volatile double *) (&((_hwpage)->_reg))))

#define crmFlush(_hwpage) \
   *((volatile unsigned int *) &((_hwpage)->PixPipeFlush)) = 0;

#define MTEFlush(_hwpage) \
   *((volatile unsigned int *) &((_hwpage)->mte.Flush)) = 0;

#define crmGetType(_val) \
   (_val) = *((volatile unsigned int *) (PHYS_TO_K1(0x14000004)));

#define crmWaitReIdle(_hwpage)				 \
{							 \
   int tempReStatusReadVal;				 \
							 \
   while(1)						 \
   {							 \
      crmGet(_hwpage, Status, tempReStatusReadVal);	 \
							 \
      if (tempReStatusReadVal & ((1 << 27) | (1 << 25))) \
	 break;						 \
   }							 \
}

#define crmWaitReFifo(_hwpage, _max)				\
{								\
   int tempReStatusReadVal;					\
								\
   while(1)							\
   {								\
      crmGet(_hwpage, Status, tempReStatusReadVal);		\
								\
      if ((CRMSTAT_IB_LEVEL(tempReStatusReadVal)) < (_max))	\
	 break;							\
   }								\
}

#define crmGetRev(_val) \
   (_val) = *((volatile unsigned int *) (PHYS_TO_K1(0x14000004)));

#define crmGetRERev(_val) \
   (_val) = ((*((volatile unsigned int *) (PHYS_TO_K1(0x15000400)))) & 0x00000001);

#define CRM_TP_DRAWING_MODE	(BM_FB_A | BM_BUF_DEPTH_8 | BM_PIX_DEPTH_8 | BM_COLOR_INDEX)

/**************************************************/
/* Section xx: Macros for General Register Access */
/**************************************************/

#define reg32Read(_reg)							\
   ((unsigned int) *((volatile unsigned int *) (&(_reg))));

#define reg32Write(_reg, _data)						\
{									\
   unsigned int temp;							\
   temp = (_val);							\
   (*((volatile int *) (&((_hwpage)->_reg)))) = *((int *) (&temp)); \
}
   
#define reg64Read(_reg)							\
   ((unsigned long long) *((volatile double *) (&(_reg))));

#define reg64Write(_hwpage, _reg, _val )				  \
{									  \
   unsigned long long temp;						  \
   temp = (_val);							  \
   (*((volatile double *) (&((_hwpage)->_reg)))) = *((double *) (&temp)); \
}

#endif /* C || C++ */

#endif /* !__CRMDEFS_H__ */
