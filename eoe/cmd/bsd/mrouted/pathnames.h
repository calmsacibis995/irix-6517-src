/*
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE".  Use of the mrouted program represents acceptance of
 * the terms and conditions listed in that file.
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 *
 * $Id: pathnames.h,v 1.1 1995/06/20 00:42:11 nowicki Exp $
 */

#define _PATH_MROUTED_CONF	"/etc/mrouted.conf"

#if (defined(BSD) && (BSD >= 199103))
#define _PATH_MROUTED_PID	"/var/run/mrouted.pid"
#define _PATH_MROUTED_GENID	"/var/run/mrouted.genid"
#define _PATH_MROUTED_DUMP	"/var/tmp/mrouted.dump"
#define _PATH_MROUTED_CACHE	"/var/tmp/mrouted.cache"
#else
#define _PATH_MROUTED_PID	"/etc/mrouted.pid"
#define _PATH_MROUTED_GENID	"/etc/mrouted.genid"
#define _PATH_MROUTED_DUMP	"/usr/tmp/mrouted.dump"
#define _PATH_MROUTED_CACHE	"/usr/tmp/mrouted.cache"
#endif
