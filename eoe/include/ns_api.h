#ifndef __NS_API_H__
#define __NS_API_H__

#include <mdbm.h>
#include <limits.h>
#include <sys/time.h>
#include <abi_mutex.h>

#define NS_DEFAULT_PORT	7770

/*
** Request result types.
*/
#define NS_SUCCESS	0	/* Everything is cool. */
#define NS_NOTFOUND	1	/* Key does not exist in named services. */
#define	NS_UNAVAIL	2	/* Could not talk to lamed. */
#define NS_TRYAGAIN	3	/* Ran out of some resource. */
#define NS_BADREQ	4	/* The request was unparsable. */
#define NS_FATAL	5	/* Unrecoverable error. */
#define NS_NOPERM       6	/* Permission denied */

#define NS_RESULTS	7	/* Total number of result types. */

/*
** Limits.
*/
#define NS_MAX_REQUEST	1024
#define LOCK_FILE_SIZE	65536

/*
** File locations.
*/
#define NS_HOME_DIR	"/var/ns/"
#define NS_DOMAINS_DIR	"/var/ns/domains/"
#define NS_LIB_DIR	"/var/ns/lib/"
#define NS_CACHE_DIR	"/var/ns/cache"
#define NS_LOCK_FILE	"/var/ns/cache/locks"
#define NS_MOUNT_DIR	"/ns"
#define NS_DOMAIN_LOCAL	".local"
#define NS_ALL_FILE	".all"

/* nsswitch.conf is located in domains/domainname  */
#define NS_SWITCH_FILE	"nsswitch.conf"

#ifndef TRUE
#	define TRUE	(1)
#endif
#ifndef FALSE
#	define FALSE	(0)
#endif

/* Lock file is a list of these records.  Length is padded to 4 byte line. */
typedef struct {
	u_int16_t		l_len;		/* Length of entire record. */
	u_int32_t		l_version;
	abilock_t		l_lock;
	char			l_name[1];	/* Really a variable length. */
} ns_lock_t;

typedef struct {
	time_t			c_timeout;
	time_t			c_mtime;
	char			c_status;
	char			c_data[1];	/* Really a variable length. */
} ns_cache_t;

/*
** The map_elem structure is used inside of libc to hold information about
** mapped files, etc.
*/
typedef struct {
	time_t			m_version;
	int			m_flags;
	MDBM			*m_map;
	int			m_stayopen;
} ns_map_t;

/*
** ns_map_t flags to be set in the m_flags field
*/
#define	NS_MAP_CACHE_EACCES		1<<0
#define	NS_MAP_FILE_EACCES		1<<1
#define	NS_MAP_NO_HOSTALIAS		1<<2
#define NS_MAP_DYNAMIC                  1<<3

/*
 * Inline versions of get/put short/long.  Pointer is advanced.
 *
 * These macros demonstrate the property of C whereby it can be
 * portable or it can be elegant but rarely both.
 *
 * These are stolen from nameser.h.
 */
#ifndef INT16SZ
#define INT16SZ	2
#endif
#ifndef INT32SZ
#define	INT32SZ	4
#endif

#ifndef GETSHORT
#define GETSHORT(s, cp) { \
	register u_char *t_cp = (u_char *)(cp); \
	(s) = ((u_int16_t)t_cp[0] << 8) \
	    | ((u_int16_t)t_cp[1]) \
	    ; \
	(cp) += INT16SZ; \
}
#endif

#ifndef GETLONG
#define GETLONG(l, cp) { \
	register u_char *t_cp = (u_char *)(cp); \
	(l) = ((u_int32_t)t_cp[0] << 24) \
	    | ((u_int32_t)t_cp[1] << 16) \
	    | ((u_int32_t)t_cp[2] << 8) \
	    | ((u_int32_t)t_cp[3]) \
	    ; \
	(cp) += INT32SZ; \
}
#endif

#ifndef PUTSHORT
#define PUTSHORT(s, cp) { \
	register u_int16_t t_s = (u_int16_t)(s); \
	register u_char *t_cp = (u_char *)(cp); \
	*t_cp++ = t_s >> 8; \
	*t_cp   = t_s; \
	(cp) += INT16SZ; \
}
#endif

#ifndef PUTLONG
#define PUTLONG(l, cp) { \
	register u_int32_t t_l = (u_int32_t)(l); \
	register u_char *t_cp = (u_char *)(cp); \
	*t_cp++ = t_l >> 24; \
	*t_cp++ = t_l >> 16; \
	*t_cp++ = t_l >> 8; \
	*t_cp   = t_l; \
	(cp) += INT32SZ; \
}
#endif

int ns_lookup(ns_map_t *, char *, char *, const char *, char *, char *, size_t);
FILE *ns_list(ns_map_t *, char *, char *, char *);

#endif /* not __NS_API_H__ */
