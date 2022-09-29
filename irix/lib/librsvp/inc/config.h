/*
 *  $Id: config.h,v 1.9 1998/11/25 08:43:36 eddiem Exp $
 */

/*
 * This file is a placeholder for an automatically generated config file
 * (by GNU autoconf or similar). It is intended to group together the
 * definition of all OS-dependent configuration options.
 *
 * In the program, code which is dependent on the processor type is
 * conditionalized by that type. Code which is dependent on a single
 * OS is conditionalized by OS. Code which is dependent on a particular
 * OS -feature- is conditionalized by a definition in this file. Until
 * such time as this file is automatically generated, please add new
 * conditionals here as required.
 */

/*
 *	Configuration options for some specific operating systems:
 */
#ifdef __sgi
#define STANDARD_C_LIBRARY
#endif
#ifndef __sgi
#define INCLUDE_SYSTEM_H
#endif

#ifdef SOLARIS
#define STANDARD_C_LIBRARY
#endif

#ifdef freebsd
#define HAVE_MACHINE_ENDIAN_H
#define STANDARD_C_LIBRARY
#define SOCKADDR_LEN
#define NET2_STYLE_SIOCGIFCONF
#define SYS_ERRLIST_DECLARED
#endif

/* Define if you have an ANSI standard C library and your system
 * includes <stddef.h> and <stdlib.h>. (Currently undefined means
 * SunOS 4.1, more or less..)
 */
/*#define STANDARD_C_LIBRARY*/

/* Define if your system uses BSD4.4-style sockaddr structures which
 * include a length field
 */
/*#define SOCKADDR_LEN*/

/* Define if your system has an IP_TTL setsockopt call
 * As of IRIX6.5, you cannot set unicast TTL on a raw socket - mwang.
 */
/*#define SET_IP_TTL*/

/* Define if your system uses the BSD Net2-style SIOCGIFCONF data
 * structures Undefined means the older 4.2/SunOS 4.1 style.
 */
/*#define NET2_STYLE_SIOCGIFCONF*/

/* Define if your system include files include declarations for the
 * external datastructures sys_errlist[] and sys_nerr. If undefined
 * they will be declared for you. If your system does not -have- these
 * datastructures you will need to do some work.
 */
#define SYS_ERRLIST_DECLARED

/* Define to include the local file "system.h" at appropriate points
 * in the compilation.  This is a last resort, as all of these
 * functions should have been declared by system include files.
 */
/*#define INCLUDE_SYSTEM_H */

/*
 * don't compile for ipv6 on SGI for now...mwang 
 *
 * #if	defined(AF_INET6) && defined(IPPROTO_IPV6)
 * #define	USE_IPV6
 * #endif
 */

#ifdef INCLUDE_SYSTEM_H
/*
 * Define this if bzero() etc. are macros, so we don't try to declare
 * them in system.h.
 */
#define BZERO_IS_A_MACRO
#endif

/*
 * Pathnames. Configure appropriately for your system
 */
#include <sys/param.h>

/* Current and previous log files */
#if BSD >= 199103
#define  RSVP_LOG         "/var/tmp/rsvpd.log"
#define  RSVP_LOG_PREV    "/var/tmp/rsvpd.log.prev"
#else
#define  RSVP_LOG         "/usr/tmp/.rsvpd.log"
#define  RSVP_LOG_PREV    "/usr/tmp/.rsvpd.log.prev"
#endif

/* File containing PID of running daemon */
#if BSD >= 199103
#define  RSVP_PID_FILE	  "/var/run/rsvpd.pid"
#else
#define  RSVP_PID_FILE    "/usr/tmp/.rsvpd.pid"
#endif

/* Name of AF_UNIX socket used by API library to communicate with daemon */
#if BSD >= 199103
#  define SNAME "/var/run/RSVP.daemon"
#else
#  define SNAME "/usr/tmp/.RSVP.daemon"
#endif

/* Common portion of local socket name for API clients */
#define LNAME "/tmp/.RSVP.client"

