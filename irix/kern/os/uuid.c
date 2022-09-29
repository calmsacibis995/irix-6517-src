/**************************************************************************
 *									  *
 *		 Copyright (C) 1994, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs contain	  *
 *  unpublished proprietary information of Silicon Graphics, Inc., and	  *
 *  are protected by Federal copyright law.  They may not be disclosed	  *
 *  to third parties or copied or duplicated in any form, in whole or	  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.25 $"

#ifndef _KERNEL
#ifdef __STDC__
	#pragma weak uuid_create	= _uuid_create
	#pragma weak uuid_create_nil	= _uuid_create_nil
	#pragma weak uuid_is_nil	= _uuid_is_nil
	#pragma weak uuid_equal		= _uuid_equal
	#pragma weak uuid_compare	= _uuid_compare
	#pragma weak uuid_to_string	= _uuid_to_string
	#pragma weak uuid_from_string	= _uuid_from_string
	#pragma weak uuid_hash		= _uuid_hash
	#pragma weak uuid_hash64	= _uuid_hash64
#endif
#include "synonyms.h"
#endif	/* notdef _KERNEL */

#include <sys/types.h>

#ifdef _KERNEL
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/sema.h>
#include <sys/timers.h>
#else	/* ! _KERNEL */
#include <stdio.h>
#include <string.h>
#include <bstring.h>
#include <malloc.h>
#include <errno.h>
#include <sys/syssgi.h>
#endif	/* _KERNEL */

#include <sys/uuid.h>


/*
 * This file is shared between the kernel and libc.
 */

/* NODE_SIZE is the number of bytes used for the node identifier portion. */
#define	NODE_SIZE	6

/*
 * Total size must be 128 bits.  N.B. definition of uuid_t in uuid.h!
 */
typedef struct {
	u_int32_t	uu_timelow;	/* time "low" */
	u_int16_t	uu_timemid;	/* time "mid" */
	u_int16_t	uu_timehi;	/* time "hi" and version */
	u_int16_t	uu_clockseq;	/* "reserved" and clock sequence */
	u_int16_t	uu_node[NODE_SIZE / 2]; /* ethernet hardware address */
} uu_t;

/*
 * The Time Base Correction is the amount to add on to a UNIX-based
 * time value (i.e. seconds since 1 Jan. 1970) to convert it to the
 * time base for UUIDs (15 Oct. 1582).
 */
#define	UUID_TBC	0x01B21DD2138140LL

/*
 * there are 3 uuid variants --
 *	variant 0 - defined 1989, used in Apollo/HP NCS rpc 1.0 and DECrpc v1
 *	variant 1 - used in DCE RPC, NCS 2.0, DECrpc v2
 *	variant 2 - Microsoft defined
 *
 * the OSF DCE uuid implementation sanity checks UUIDs to ensure that
 * they are one of the three legal known variants.  Since IRIX DCE uses
 * the IRIX uuid code, we have to do the same.
 */

#define VALID_UUID_VARIANT(uuid_p) \
( \
((((uu_t *)uuid_p)->uu_clockseq & 0x8000) == 0x0000) ||	/* variant 0 */ \
((((uu_t *)uuid_p)->uu_clockseq & 0xc000) == 0x8000) ||	/* variant 1 */ \
((((uu_t *)uuid_p)->uu_clockseq & 0xe000) == 0xc000)	/* variant 2 */ \
)

#ifdef _KERNEL

static	char		uuid_is_init;	/* time/clockseq are initialized */
static	char		uuid_have_eaddr;	/* was ethernet addr found? */
static	short		uuid_eaddr[NODE_SIZE / 2];	/* ethernet address */
static	__int64_t	uuid_time;	/* last time basis used */
static	mutex_t		uuid_lock;	/* protects uuid_time */
static	u_int16_t 	uuid_clockseq;	/* boot-time randomizer */

void	uuid_init(void);
int	get_eaddr(char []);


/*
 * uuid_init - called from out of init_tbl[]
 */
void
uuid_init(void)
{
	init_mutex(&uuid_lock, MUTEX_DEFAULT, "uuid", 0);
}


/*
 * uuid_create - kernel version, does the actual work
 */
void
uuid_create(uuid_t *uuid, uint_t *status)
{
	int		i;
	uu_t		*uu = (uu_t *)uuid;

	mutex_lock(&uuid_lock, PZERO);
	if (!uuid_is_init) {
		timespec_t	ts;

		nanotime(&ts);
		/*
		 * The clock sequence must be initialized randomly.
		 */
		uuid_clockseq = (get_timestamp() & 0xfff) | 0x8000;
		/*
		 * Initialize the uuid time, it's in 100 nanosecond
		 * units since a time base in 1582.
		 */
		uuid_time = ts.tv_sec * 10000000LL +
			    ts.tv_nsec / 100LL +
			    UUID_TBC;
		uuid_is_init = 1;
	}
	if (!uuid_have_eaddr) {
		uuid_have_eaddr = get_eaddr((char *)uuid_eaddr);
		if (!uuid_have_eaddr) {
			*status = uuid_s_no_address;
			mutex_unlock(&uuid_lock);
			return;
		}
	}
	uuid_time++;
	uu->uu_timelow = (u_int32_t)(uuid_time & 0x00000000ffffffffLL);
	uu->uu_timemid = (u_int16_t)((uuid_time >> 32) & 0x0000ffff);
	uu->uu_timehi = (u_int16_t)((uuid_time >> 48) & 0x00000fff) | 0x1000;
	mutex_unlock(&uuid_lock);
	uu->uu_clockseq = uuid_clockseq;
	for (i = 0; i < (NODE_SIZE / 2); i++)
		uu->uu_node [i] = uuid_eaddr [i];
	*status = uuid_s_ok;
}

/*
 * uuid_getnodeuniq - obtain the node unique fields of a UUID.
 *
 * This is not in any way a standard or condoned UUID function;
 * it just something that's needed for user-level file handles.
 */
void
uuid_getnodeuniq(uuid_t *uuid, int fsid [2])
{
	uu_t	*uup = (uu_t *)uuid;

	fsid[0] = (uup->uu_clockseq << 16) | uup->uu_timemid;
	fsid[1] = uup->uu_timelow;
}

#endif	/* _KERNEL */

/*
 * To share UUID functions between kernel and user code, put them here.
 */

void
uuid_create_nil(uuid_t *uuid, uint_t *status)
{
	*status = uuid_s_ok;
	bzero(uuid, sizeof *uuid);
}

boolean_t
uuid_is_nil(uuid_t *uuid, uint_t *status)
{
	int	i;
	char	*cp = (char *)uuid;

	*status = uuid_s_ok;
	if (uuid == NULL)
		return B_TRUE;
	if (!VALID_UUID_VARIANT(uuid)) {
		*status = uuid_s_bad_version;
		return B_FALSE;
	}
	/* implied check of version number here... */
	for (i = 0; i < sizeof *uuid; i++)
		if (*cp++) return B_FALSE;	/* not nil */
	return B_TRUE;	/* is nil */
}


int
uuid_compare(uuid_t *uuid1, uuid_t *uuid2, uint_t *status)
{
	int	i;
	char	*cp1 = (char *) uuid1;
	char	*cp2 = (char *) uuid2;

	*status = uuid_s_ok;
	if (uuid1 == NULL) {
		if (uuid2 == NULL)
			return 0;	/* equal because both are nil */
		else  {
			if (!VALID_UUID_VARIANT(uuid2))
				*status = uuid_s_bad_version;
			return -1;	/* uuid1 nil, so precedes uuid2 */
		}
	} else if (uuid2 == NULL) {
		if (!VALID_UUID_VARIANT(uuid1)) {
			*status = uuid_s_bad_version;
			return -1;	/* uuid2 nil, uuid1 bad */
		} else
			return 1;
	}
	if (!VALID_UUID_VARIANT(uuid1)) {
		*status = uuid_s_bad_version;
		return -1;
	}
	if (!VALID_UUID_VARIANT(uuid2)) {
		*status = uuid_s_bad_version;
		return -1;
	}

	/* implied check of version number here... */
	for (i = 0; i < sizeof(uuid_t); i++) {
		if (*cp1 < *cp2)
			return -1;
		if (*cp1++ > *cp2++)
			return 1;
	}
	return 0;	/* they're equal */
}


boolean_t
uuid_equal(uuid_t *uuid1, uuid_t *uuid2, uint_t *status)
{
	*status = uuid_s_ok;
	if (!VALID_UUID_VARIANT(uuid1)) {
		*status = uuid_s_bad_version;
		return B_FALSE;
	}
	if (!VALID_UUID_VARIANT(uuid2)) {
		*status = uuid_s_bad_version;
		return B_FALSE;
	}
	return bcmp(uuid1, uuid2, sizeof(uuid_t)) ? B_FALSE : B_TRUE;
}


ushort_t
uuid_hash(uuid_t *uuid, uint_t *status)
{
	int		i;
	ushort_t	*sp = (ushort_t *)uuid;
	ushort_t	hash = 0;

	*status = uuid_s_ok;
	if (!VALID_UUID_VARIANT(uuid)) {
		*status = uuid_s_bad_version;
		return 0;
	}
	for (i = 0; i < sizeof(*uuid) / 2; i++)
		hash += *sp++;
	return hash;
}


/*
 * Given a 128-bit uuid, return a 64-bit value by adding the top and bottom
 * 64-bit words.  NOTE: This function can not be changed EVER.  Although
 * brain-dead, some applications depend on this 64-bit value remaining
 * persistent.  Specifically, DMI vendors store the value as a persistent
 * filehandle.
 */
__uint64_t
uuid_hash64(uuid_t *uuid, uint_t *status)
{
	__uint64_t	*sp = (__uint64_t *)uuid;

	*status = uuid_s_ok;
	if (!VALID_UUID_VARIANT(uuid)) {
		*status = uuid_s_bad_version;
		return 0;
	}
	return sp[0] + sp[1];
}	/* uuid_hash64 */


#ifndef _KERNEL

/*
 * uuid_create - user version, does syscall to kernel version
 */
void
uuid_create(uuid_t *uuid, uint_t *status)
{
	int	rc;

	rc = (int)syssgi(SGI_CREATE_UUID, uuid, status);
	if (rc == -1 && errno == EFAULT) {
		/*
		 * Caller deserves no less than what they're due:
		 * Try to make 'em fault.
		 */
		bzero(uuid, sizeof(*uuid));
		/*
		 * Fault may have been on status; covered below.
		 */
	}
	if (rc == -1) {
		/*
		 * No idea what went wrong; return the closest thing to
		 * a "system error" we've got.
		 */
		*status = uuid_s_socket_failure;
	}
}


/*
 * A string uuid looks like: 7e0c8f5f-113e-101d-8850-010203040506
 */

#define UUID_STR_LEN	36	/* does not include trailing \0 */
#define UUID_FORMAT_STR "%08x-%04hx-%04hx-%04hx-%04x%04x%04x"

static const char uuid_format_str[] = { UUID_FORMAT_STR };

void
uuid_to_string(uuid_t *uuid, char **string_uuid, uint_t *status)
{
	uu_t	*uu = (uu_t *)uuid;

	*status = uuid_s_ok;
	if (string_uuid == NULL)
		return;
	if (!VALID_UUID_VARIANT(uuid)) {
		*status = uuid_s_bad_version;
		return;
	}
	*string_uuid = (char *)malloc(UUID_STR_LEN + 1);
	if (uuid == NULL) {
		sprintf(*string_uuid, uuid_format_str, 0, 0, 0, 0, 0, 0, 0);
		return;
	}
	sprintf(*string_uuid, uuid_format_str, uu->uu_timelow, uu->uu_timemid,
		uu->uu_timehi, uu->uu_clockseq,
		uu->uu_node[0], uu->uu_node[1], uu->uu_node[2]);
}


void
uuid_from_string(char *string_uuid, uuid_t *uuid, uint_t *status)
{
	int	i;
	uu_t	*uu = (uu_t *)uuid;
	int	tempnode[NODE_SIZE / 2];

	if (string_uuid == NULL || *string_uuid == NULL) {
		uuid_create_nil(uuid, status);
		return;
	}
	if (strlen(string_uuid) != UUID_STR_LEN ||
	    sscanf(string_uuid, uuid_format_str,
		   &uu->uu_timelow, &uu->uu_timemid, &uu->uu_timehi,
		   &uu->uu_clockseq,
		   &tempnode[0], &tempnode[1], &tempnode[2]) != 7) {
		*status = uuid_s_invalid_string_uuid;
		return;
	}
	if (!VALID_UUID_VARIANT(uuid)) {
		*status = uuid_s_bad_version;
		return;
	}
	for (i = 0; i < NODE_SIZE / 2; i++)
		uu->uu_node[i] = (u_int16_t)tempnode[i];

	*status = uuid_s_ok;
}

#endif	/* notdef _KERNEL */
