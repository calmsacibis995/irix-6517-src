/*  
 * regs.h : 
 *	Debug Tool Register/Memory read/write Tests
 *
 * Copyright 1996, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S GOVERNMENT RESTRICTED RIGHTS LEGEND:
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

#ident "ide/IP30/debugtool/regs.h:  $Revision: 1.4 $"

enum {EightBit, SixteenBit, ThirtyTwoBit, SixtyFourBit}; 
extern Heart_Regs	gz_heart_regs_ep[];
extern Bridge_Regs	gz_bridge_regs_ep[];
extern Xbow_Regs	gz_xbow_regs_ep[];
/*needs change
extern Ioc3_Regs	gz_heart_regs_ep[]; 
extern Rad_Regs		gz_heart_regs_ep[];
extern Scsi_Regs	gz_heart_regs_ep[];
*/
extern void heart_ind_peek(int offset, long long regAddr64, int loop);
extern void heart_ind_poke(int offset, long long regAddr64, int loop,long long data);
extern void bridge_ind_peek(int offset, long long regAddr64, int loop);
extern void bridge_ind_poke(int offset, long long regAddr64, int loop,long long data);
extern void xbow_ind_peek(int offset, long long regAddr64, int loop);
extern void xbow_ind_poke(int offset, long long regAddr64, int loop,long long data);
extern void ioc3_ind_peek(int offset, long long regAddr64, int loop);
extern void ioc3_ind_poke(int offset, long long regAddr64, int loop,long long data);
extern void duart_ind_peek(int offset, long long regAddr64, int loop);
extern void duart_ind_poke(int offset, long long regAddr64, int loop,long long data);
extern void scsi_ind_peek(int offset, long long regAddr64, int loop);
extern void scsi_ind_poke(int offset, long long regAddr64, int loop,long long data);
extern void rad_ind_peek(int offset, long long regAddr64, int loop);
extern void rad_ind_poke(int offset, long long regAddr64, int loop,long long data);
extern void phy_ind_poke(int offset, long long regAddr64, int loop,long long data);
extern void phy_ind_peek(int offset, long long regAddr64, int loop);
extern void initSCSI(void);
extern void restoreSCSI(void);
