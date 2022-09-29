/*
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE".  Use of the mrouted program represents acceptance of
 * the terms and conditions listed in that file.
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 *
 * $Id: igmpv2.h,v 1.2 1997/10/03 17:33:41 nowicki Exp $
 */

/*
 * Constants for IGMP Version 2.  Several of these, especially the
 * robustness variable, should be variables and not constants.
 */
#define	IGMP_ROBUSTNESS_VARIABLE		2
#define	IGMP_QUERY_INTERVAL			125
#define	IGMP_QUERY_RESPONSE_INTERVAL		10
#define	IGMP_GROUP_MEMBERSHIP_INTERVAL		(IGMP_ROBUSTNESS_VARIABLE * \
					IGMP_QUERY_INTERVAL + \
					IGMP_QUERY_RESPONSE_INTERVAL)
#define	IGMP_OTHER_QUERIER_PRESENT_INTERVAL	(IGMP_ROBUSTNESS_VARIABLE * \
					IGMP_QUERY_INTERVAL + \
					IGMP_QUERY_RESPONSE_INTERVAL / 2)
#define	IGMP_STARTUP_QUERY_INTERVAL		30
#define	IGMP_STARTUP_QUERY_COUNT		IGMP_ROBUSTNESS_VARIABLE
#define	IGMP_LAST_MEMBER_QUERY_INTERVAL		1
#define	IGMP_LAST_MEMBER_QUERY_COUNT		IGMP_ROBUSTNESS_VARIABLE
