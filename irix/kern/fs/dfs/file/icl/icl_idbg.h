/*
 * icl_idbg.h
 *
 *      Declaration of idbg entry points defined in icl_idbg.c
 *
 * Copyright  1998 Silicon Graphics, Inc.  ALL RIGHTS RESERVED
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND
 * 
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the 
 * Rights in Technical Data and Computer Software clause at DFARS 252.227-7013 
 * and/or in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement. Unpublished -- rights reserved under the Copyright Laws 
 * of the United States. Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd., Mountain View, CA 94039-7311.
 * 
 * THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SGI
 * 
 * The copyright notice above does not evidence any actual or intended 
 * publication or disclosure of this source code, which includes information 
 * that is the confidential and/or proprietary, and is a trade secret,
 * of Silicon Graphics, Inc. Any use, duplication or disclosure not 
 * specifically authorized in writing by Silicon Graphics is strictly 
 * prohibited. ANY DUPLICATION, MODIFICATION, DISTRIBUTION,PUBLIC PERFORMANCE,
 * OR PUBLIC DISPLAY OF THIS SOURCE CODE WITHOUT THE EXPRESS WRITTEN CONSENT 
 * OF SILICON GRAPHICS, INC. IS STRICTLY PROHIBITED.  THE RECEIPT OR POSSESSION
 * OF THIS SOURCE CODE AND/OR INFORMATION DOES NOT CONVEY ANY RIGHTS 
 * TO REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE, USE, 
 * OR SELL ANYTHING THAT IT MAY DESCRIBE, IN WHOLE OR IN PART.
 *
 * $Log: icl_idbg.h,v $
 * Revision 65.1  1998/09/22 20:31:17  gwehrman
 * Declarations of functions defined in icl_idbg.c.
 *
 */

#ident "$Revision: 65.1 $"

#ifndef __ICL_IDBG_H__
#define __ICL_IDBG_H__

/* Entrypoints defined in icl_idbg.c */
extern void idbg_dfslogs (void);
extern void idbg_dfstrace (int log);

#endif /* !__ICL_IDBG_H__ */
