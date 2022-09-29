#include "sys/types.h"
#include "sys/sbd.h"
#include "sys/sema.h"
/*
#include "sys/gfx.h"
#include "sys/gr2.h"
#include "sys/gr2hw.h"
#include "sys/gr2_if.h"
#include "sys/ng1hw.h"
#include "libsc.h"
*/
#include "ide_msg.h"
#include "sys/param.h"
#define HQ3     /*must be defined here for HQ3 operations*/
#include "uif.h"
#include "sys/mgrashw.h"
#include "sys/mgras.h"
#include "mgv1_regio.h"

/*extern struct gr2_hw  *base ;*/
extern mgras_hw *mgbase;
/*extern struct mgras_hw *base;*/


extern int	GVType;

#ifndef HQ3
static int	dcb_setup;
static int	dcb_hold;
static int	dcb_width;
#endif
static int	dcb_inited;

#define AB_OFFSET 0x62800
#define CC_OFFSET 0x63000


/******************************************************************************
*
*  addressprint() -- Enable/Disable the printing of the address which is 
*                    being written to when reading/writing the AB1/CC1
*
*  inputs: (function toggles printing function)
*  returns: 
*  side effects:
*  notes:
*  jfg 5/95
******************************************************************************/

int addprnt = 0;

void addressprint()
{
 if (addprnt){
        addprnt = 0;
	msg_printf(VRB, "Printing cc1 ab1 gio addresses DISABLED \n");}
else 
       { addprnt = 1;
	msg_printf(VRB, "Printing cc1 ab1 gio addresses ENABLED \n");}
}

void gvio_init (mgras_hw *base)
{
#ifdef HQ3
	if (GVType == GVType_unknown) {
			GVType = GVType_ng1;
		      }

#else
	if (GVType == GVType_unknown) {
		if (badaddr(&base->hq.mystery, 4) ||
		    base->hq.mystery != 0xdeadbeef) {
			GVType = GVType_ng1;
		} else {
			GVType = GVType_gr2;
		      }
	      }
#endif	
	      
 
/*      MAX*/
#ifndef HQ3
       dcb_setup = 4;
        dcb_hold = 4;
        dcb_width = 4;
#endif
        dcb_inited  = 1;

/*	Mark's suggestion
        dcb_setup = 3; 
	dcb_hold = 2 ; 
	dcb_width = 2; 
	dcb_inited = 1;*/

/*     original
	dcb_setup = 2;
	dcb_hold = 1;
	dcb_width = 2; 
	dcb_inited = 1;*/
      }

/******************************************************************************
*
*  gv_r1, gv_r4, gv_w1, gv_w4 - The actual functions called when
*                               reading/writing  1/4 bytes
*                               to/from the AB1 and CC1
*  inputs: (mgras_hw *mgbase, int reg_set, int sync, int reg_offset)
*  returns: 
*  side effects:
*  notes:
*  jfg 5/95
******************************************************************************/

int gv_r1 (mgras_hw *mgbase, int reg_set, int sync, int reg_offset)
{
	int		 mode;
	unsigned int     val;

	if (dcb_inited == 0) gvio_init (mgbase);
#ifdef HQ3
 { 		/*volatile __psunsigned_t  *regbase;*/
		volatile caddr_t regbase;
                /*volatile char *regbase;*/
		regbase = (reg_set == DCB_VCC1_PXHACK ?
	(caddr_t)((__psint_t)(mgbase) + CC_OFFSET + (reg_offset << 7) + (0x1 << 3)) : 
			(caddr_t)((__psint_t)(mgbase) + AB_OFFSET+ (reg_offset << 7) + (0x1 << 3)));  



	val = *regbase;

if (addprnt)
	msg_printf(DBG, "gv_r1  regbase 0x%x val 0x%x \n", regbase,val );		

	      }
#else 
	if (GVType & GVType_ng1) {

		volatile struct rex3chip *rex = (volatile struct rex3chip *)
			((unsigned long) base + 0x000f0000);

		mode = reg_set |
			(dcb_setup << DCB_CSSETUP_SHIFT) |
			(dcb_hold << DCB_CSHOLD_SHIFT) |
			(dcb_width << DCB_CSWIDTH_SHIFT) |
			sync |
			(reg_offset << DCB_CRS_SHIFT) |
		        DCB_DATAWIDTH_1;   /* 1 */
		        rex->set.dcbmode = mode;
		        val = rex->set.dcbdata0.bybyte.b3;

	} else {
		volatile char *regbase;
		regbase = (reg_set == DCB_VCC1 ?
			(int *) &base->cc1 :
			(int *) &base->ab1);
		val = regbase [reg_offset];

	}
#endif
	return (val);
}

int gv_r4 (mgras_hw *mgbase, int reg_set, int sync, int reg_offset)
{
	int		 mode;
	unsigned int     val;

	if (dcb_inited == 0) gvio_init (mgbase);


#ifdef HQ3

      { 	/*	volatile caddr_t regbase;*/
       		volatile int *regbase;
		regbase = (reg_set == DCB_VCC1_PXHACK ?
	          (int *)((__psint_t)(mgbase) + CC_OFFSET + (reg_offset << 7) + (0x0 << 3)) : 
		(int *)((__psint_t)(mgbase) + AB_OFFSET+ (reg_offset << 7) + (0x0 << 3)));  

/*	          (caddr_t)(( int  )(mgbase) + CC_OFFSET + (reg_offset << 7) + (0x1 << 3) + (1 << 6)) : 
		(caddr_t)(( int  )(mgbase) + AB_OFFSET+ (reg_offset << 7) + (0x1 << 3)+ (1 << 6))) ;  */

		val = *regbase;
if (addprnt)
 msg_printf(DBG, "gv_r4  regbase 0x%x val 0x%x \n", regbase,val );

	}		 

#else
if (GVType & GVType_ng1) {
		volatile struct rex3chip *rex = (volatile struct rex3chip *)
			((unsigned long) base + 0x000f0000);

		mode = reg_set |
			(dcb_setup << DCB_CSSETUP_SHIFT) |
			(dcb_hold << DCB_CSHOLD_SHIFT) |
			(dcb_width << DCB_CSWIDTH_SHIFT) |
			sync |
			(reg_offset << DCB_CRS_SHIFT) |
			DCB_DATAWIDTH_4;
		rex->set.dcbmode = mode;
		val = rex->set.dcbdata0.byword;

	} else {
		volatile int *regbase;

		regbase = (reg_set == DCB_VCC1 ?
			(int *) &base->cc1 :
			(int *) &base->ab1);
		val = regbase [reg_offset];
	}
#endif
	return (val);
}

int gv_w1 (mgras_hw *mgbase, int reg_set, int sync, int reg_offset, int val)
{
  
	if (dcb_inited == 0) gvio_init (mgbase);

#ifdef HQ3




{		/*volatile caddr_t regbase;*/
		volatile char *regbase;
		regbase = (reg_set == DCB_VCC1_PXHACK ?
			(caddr_t)((__psint_t)(mgbase) + CC_OFFSET + (reg_offset << 7) + (0x1 << 3)) : 
			(caddr_t)((__psint_t)(mgbase) + AB_OFFSET+ (reg_offset << 7) + (0x1 << 3)));  

if (addprnt)
	msg_printf(DBG, "gv_w1  regbase 0x%x val 0x%x\n", regbase, val );
	
		*regbase  = val;

	      }
#else
	if (GVType & GVType_ng1) {
		volatile struct rex3chip *rex = (volatile struct rex3chip *)
			((unsigned long) base + 0x000f0000);
		rex->set.dcbmode = 
			reg_set |
			(dcb_setup << DCB_CSSETUP_SHIFT) |
			(dcb_hold << DCB_CSHOLD_SHIFT) |
			(dcb_width << DCB_CSWIDTH_SHIFT) |
			sync |
			(reg_offset << DCB_CRS_SHIFT) |
			DCB_DATAWIDTH_1;
		rex->set.dcbdata0.bybyte.b3 = val;

	} else {
		volatile char *regbase;

		regbase = (reg_set == DCB_VCC1 ?
			(int *) &base->cc1 :
			(int *) &base->ab1);
		regbase [reg_offset << 2] = val;
	}
#endif
	return 0;
}

int gv_w4 (mgras_hw *mgbase, int reg_set, int sync, int reg_offset, unsigned int val)
{
	if (dcb_inited == 0) gvio_init (mgbase);


#ifdef HQ3
	{	/*volatile caddr_t regbase;	*/
		volatile int *regbase;
		regbase = (reg_set == DCB_VCC1_PXHACK ?
	          (int *)((__psint_t)(mgbase) + CC_OFFSET + (reg_offset << 7) + (0x0 << 3)) : 
		(int *)((__psint_t)(mgbase) + AB_OFFSET+ (reg_offset << 7) + (0x0 << 3)));  

/*          (caddr_t)((_psint_t)(mgbase) + CC_OFFSET + (reg_offset << 7) + (0x1 << 3) + (1 << 6)) : 
		(caddr_t)((__psint_t)(mgbase) + AB_OFFSET+ (reg_offset << 7) + (0x1 << 3)+ (1 << 6))) ;  */

if (addprnt)

	msg_printf(DBG, "gv_w4  regbase 0x%x val 0x%x \n", regbase,val );

		*regbase = val;

			 }
#else
	if (GVType & GVType_ng1) {
		volatile struct rex3chip *rex = (volatile struct rex3chip *)
			((unsigned long) base + 0x000f0000);

		rex->set.dcbmode = 
			reg_set |
			(dcb_setup << DCB_CSSETUP_SHIFT) |
			(dcb_hold << DCB_CSHOLD_SHIFT) |
			(dcb_width << DCB_CSWIDTH_SHIFT) |
			sync |
			(reg_offset << DCB_CRS_SHIFT) |
			DCB_DATAWIDTH_4;
		rex->set.dcbdata0.byword = val;

	} else {
		volatile int *regbase;

		regbase = (reg_set == DCB_VCC1 ?
			(int *) &base->cc1 :
			(int *) &base->ab1);
		regbase [reg_offset] = val;
	}
#endif

	return 0;
}
