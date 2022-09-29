/*
 * Copyright 1996,  Silicon Graphics, Inc.
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


#ident "$Revision: 1.4 $"

#ifndef __NUMA_TRAFFIC_H__
#define __NUMA_TRAFFIC_H__

/* 
 * help for floating point operation 
 */
#define MAGNIFICATION 16
#define FULLY_UTILIZED 1 << MAGNIFICATION 
#define PERCENTAGE_BASE 100

/*
 * The returned load values are in the range
 * [0, 2^MAGNIFICATION]. We need to convert
 * these values into percentages.
 */

#define TRAFFIC_ABS_TO_REL(abs) (((abs) * 100) / (1 << MAGNIFICATION))

int local_neighborhood_traffic_overall(cnodeid_t /*my_node_id*/);
int local_neighborhood_traffic_byroute(cnodeid_t /*my_node_id*/,
				       cnodeid_t /*dest_node_id*/);
int remote_neighborhood_traffic_byroute(cnodeid_t /*dest_node_id*/,
					cnodeid_t /*my_node_id*/);


#endif /* __NUMA_TRAFFIC_H__ */







