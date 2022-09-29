#ifndef _SYS_UUID_H
#define _SYS_UUID_H

/**************************************************************************
 *                                                                        *
 *                Copyright (C) 1994, Silicon Graphics, Inc.              *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.8 $"

/* Universally Unique Identifiers */

typedef struct {
        unsigned char	__u_bits[16];
} uuid_t, *uuid_p_t;

void	uuid_create	(uuid_t *uuid,			uint_t *status);
void	uuid_create_nil	(uuid_t *uuid,			uint_t *status);
boolean_t uuid_is_nil	(uuid_t *uuid,			uint_t *status);
boolean_t uuid_equal	(uuid_t *uuid1, uuid_t *uuid2,	uint_t *status); 
int	uuid_compare	(uuid_t *uuid1, uuid_t *uuid2,	uint_t *status);
void	uuid_to_string	(uuid_t *uuid, char **uuid_str,	uint_t *status);
void	uuid_from_string (char *uuid_str, uuid_t *uuid,	uint_t *status);
ushort_t uuid_hash	(uuid_t *uuid,			uint_t *status);
__uint64_t uuid_hash64	(uuid_t *uuid,			uint_t *status);

#ifdef _KERNEL
void	uuid_getnodeuniq (uuid_t *uuid, int fsid [2]);
#endif	/* _KERNEL */

/*
 *  Values to be returned in the "status" parameter.
 */
#define	uuid_s_ok			0
#define uuid_s_bad_version		382312584
#define	uuid_s_socket_failure		382312585
#define	uuid_s_getconf_failure		382312586
#define	uuid_s_no_address		382312587
#define	uuid_s_overrun			382312588
#define	uuid_s_internal_error		382312589
#define	uuid_s_coding_error		382312590
#define	uuid_s_invalid_string_uuid	382312591
#define uuid_s_no_memory		382312592

#endif /* _SYS_UUID_H */
