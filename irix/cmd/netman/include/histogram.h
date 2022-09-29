#ifndef HISTOGRAM_H
#define HISTOGRAM_H
/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Histogram include file
 *
 *	$Revision: 1.4 $
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#include <sys/time.h>

/*
 * Histogram service
 */

#define HIST_MAXBINS		2048

struct counts {
	float		c_packets;
	float		c_bytes;
};

struct histogram {
	unsigned int	h_bins;			/* Number of filled bins */
	struct timeval	h_timestamp;		/* Ending time */
	struct counts	h_count[HIST_MAXBINS];	/* Bins */
};

#endif /* !HISTOGRAM_H */
