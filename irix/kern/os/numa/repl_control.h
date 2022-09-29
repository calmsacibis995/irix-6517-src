/*
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

/* exported interface for replication controller */

#ident "$Revision: 1.2 $"

#ifndef __REPL_CONTROL_H__
#define __REPL_CONTROL_H__



/* nodepda data needed for page replication control */
typedef struct repl_control_data_s {
	uchar_t repl_control_enabled;  /* Enable/disable replication control */
	int repl_traffic_highmark;     /* Traffic high watermark             */
	int repl_mem_lowmark;          /* Memory low watermark               */
} repl_control_data_t;

extern void repl_control_init(cnodeid_t /*node*/, caddr_t* /*nextbyte*/);

/* 
 *  Return: id of the node where the page will be replicated if success
 *          CNODEID_NONE if fails to find a suitable node 
 */
extern cnodeid_t repl_control(cnodeid_t /*src_node*/, 
			      cnodeid_t /*dst_node*/, 
			      rqmode_t /*rqmode*/);



#endif /* __REPL_CONTROL_H__ */



