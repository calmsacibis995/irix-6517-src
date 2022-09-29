/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __T6SATMP_H__
#define __T6SATMP_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident  "$Revision: 1.3 $"

/*
 * Reference header file for the TSIX(re) 1.1 Security Attribute
 * Token Mapping Protocol. Refer to satmp(7) for details.
 */

/* SATMP Message Codes */
#define SATMP_FLUSH_REPLY		162
#define SATMP_GET_ATTR_REQUEST		163
#define SATMP_GET_ATTR_REPLY		164
#define SATMP_INIT_REQUEST		165
#define SATMP_INIT_REPLY		166
#define SATMP_FLUSH_REMOTE_CACHE	167
#define SATMP_GET_TOK_REQUEST		173
#define SATMP_GET_TOK_REPLY		174

/* SGI Message Codes */
#define SATMP_SEND_INIT_REQUEST		200
#define SATMP_SEND_GET_ATTR_REQUEST	201
#define SATMP_SEND_GET_TOK_REQUEST	202
#define SATMP_SEND_FLUSH_REMOTE_CACHE	203
#define SATMP_EXIT			204
#define SATMP_GET_LRTOK_REQUEST		210
#define SATMP_GET_LRTOK_REPLY		211

/***********************************************************************
 * TSIX(RE) SATMP Attribute Identifiers
 *
 */
#define SATMP_ATTR_SEN_LABEL		0
#define SATMP_ATTR_NATIONAL_CAVEATS	1
#define SATMP_ATTR_INTEGRITY_LABEL	2
#define SATMP_ATTR_INFO_LABEL		3
#define SATMP_ATTR_PRIVILEGES		4
#define SATMP_ATTR_AUDIT_ID		5
#define SATMP_ATTR_IDS			6
#define SATMP_ATTR_CLEARANCE		7
#define SATMP_ATTR_AUDIT_INFO	        8
#define SATMP_ATTR_UNASSIGNED_9	        9
#define SATMP_ATTR_ACL		       10
#define SATMP_ATTR_FILE_PRIVILEGES     11
#define SATMP_ATTR_LAST		       11
#define SATMP_ATTR_UNDEFINED	       -1

/* Message status constants */ 
#define SATMP_REPLY_OK			0	
#define SATMP_INVALID_VERSION		1
#define SATMP_INVALID_GENERATION	2
#define SATMP_UNSUPPORTED_MESSAGE	3
#define SATMP_FORMAT_ERROR 		4
#define SATMP_TRANSLATION_ERROR		5
#define SATMP_NO_DOT 			6
#define SATMP_NO_MAPPING		7
#define SATMP_INTERNAL_ERROR		255

/* DOT flags */
#define DOT_BINARY_ATTR_REP 		1
#define DOT_ASCII_ATTR_REP 		2

/* DOT weight constants */
#define DOT_NULL_WEIGHT 		0
#define DOT_NATIVE_WEIGHT 		255

/* Version */
#define T6_VERSION_NUMBER		0x301

/* Server State */
#define T6_IS_TOKEN_SERVER		0
#define T6_USING_PRIMARY_SERVER		1
#define T6_USING_BACKUP_SERVER		2

/* INIT_REPLY status codes */
#define IRQ_FLAG_PENDING		0
#define IRQ_FLAG_OK			1
#define IRQ_FLAG_FAILED			2

#ifndef __SATMP_ESI_T_DEFINED__
#define __SATMP_ESI_T_DEFINED__
typedef __uint64_t satmp_esi_t;
#endif /* !__SATMP_ESI_T_DEFINED__ */

/*
 * SATMP HEADER
 */
typedef struct _satmp_header {
	u_char	message_number;
	u_char	message_status;
	u_short	message_length;
	satmp_esi_t message_esi;
} satmp_header;

/*
 * SATMP_INIT_REQUEST DATA STRUCTURES
 */
typedef struct _init_request_dot_rep {
	u_char  weight;
	u_char  flags;
	u_short length;
	char   *dot_name;
} init_request_dot_rep;

typedef struct _init_request_attr_format {
	u_short attribute;
	u_short dot_count;
	init_request_dot_rep *dot;
} init_request_attr_format;

typedef struct _init_request {
	u_int   generation;
	u_short version;
	u_char  server_state;
	u_int   hostid;
	u_int   token_server;
	u_int   backup_server;
	u_short format_count;
	init_request_attr_format *format;
} init_request;

/*
 * SATMP_INIT_REPLY DATA STRUCTURES
 */
typedef struct _init_request init_reply;

/*
 * SATMP_GET_TOK_REQUEST DATA STRUCTURES
 */
typedef struct _get_tok_request_attr_spec {
	u_short attribute;
	u_short nr_length;
	u_int   mid;
	char   *domain;
	char   *nr;
} get_tok_request_attr_spec;

typedef struct _get_tok_request {
	u_int   hostid;
	u_int   generation;
	u_short attr_spec_count;
	get_tok_request_attr_spec *attr_spec;
} get_tok_request;

/*
 * SATMP_GET_TOK_REPLY DATA STRUCTURES
 */
typedef struct _get_tok_reply_token_spec {
	u_short attribute;
	u_int   mid;
	u_int   token;
} get_tok_reply_token_spec;

typedef struct _get_tok_reply {
	u_int	hostid;
	u_int   generation;
	u_short token_spec_count;
	get_tok_reply_token_spec *token_spec;
} get_tok_reply;

/*
 * SATMP_GET_ATTR_REQUEST DATA STRUCTURES
 */
typedef struct _get_attr_request_token_spec {
	u_short attribute;
	char   *domain;
	u_int   token;
} get_attr_request_token_spec;

typedef struct _get_attr_request {
	u_int   hostid;
	u_int   generation;
	u_short token_spec_count;
	get_attr_request_token_spec *token_spec;
} get_attr_request;

/*
 * SATMP_GET_ATTR_REPLY DATA STRUCTURES
 */
typedef struct _get_attr_reply_attr_spec {
	u_short attribute;
	u_short nr_length;
	u_int   token;
	u_char  status;
	char   *nr;
} get_attr_reply_attr_spec;

typedef struct _get_attr_reply {
	u_int   hostid;
	u_int   generation;
	u_short attr_spec_count;
	get_attr_reply_attr_spec *attr_spec;
} get_attr_reply;

/*
 * SATMP_GET_LRTOK_REQUEST DATA STRUCTURES
 */
typedef struct _get_lrtok_request_attr_spec {
	u_short attribute;
	u_short lr_length;
	u_int   mid;
	char   *domain;
	void   *lr;
} get_lrtok_request_attr_spec;

typedef struct _get_lrtok_request {
	u_int   hostid;
	u_int   generation;
	u_short attr_spec_count;
	get_lrtok_request_attr_spec *attr_spec;
} get_lrtok_request;

/*
 * SATMP_GET_LRTOK_REPLY DATA STRUCTURES
 */
typedef struct _get_lrtok_reply_token_spec {
	u_short attribute;
	u_int   mid;
	u_int   token;
	u_char  status;
} get_lrtok_reply_token_spec;

typedef struct _get_lrtok_reply {
	u_int	hostid;
	u_int   generation;
	u_short token_spec_count;
	get_lrtok_reply_token_spec *token_spec;
} get_lrtok_reply;

/*
 * SATMP_TOKEN_LIST
 */
typedef struct _satmp_lrtok {
	u_int mid;
	u_int token;
} satmp_lrtok;

typedef struct _satmp_token_list {
	u_int   hostid;
	u_int   generation;
	u_short mask;
	satmp_lrtok lrtok_list[SATMP_ATTR_LAST + 1];
} satmp_token_list;

/*
 * SATMP_LREP_LIST
 */
typedef struct _satmp_lrep {
	u_int   mid;
	void   *lr;
	u_short lr_len;
} satmp_lrep;

typedef struct _satmp_lrep_list {
	u_int hostid;
	u_int generation;
	u_short mask;
	satmp_lrep lr_list[SATMP_ATTR_LAST + 1];
} satmp_lrep_list;

/*
 * SEND_INIT_REQUEST and SEND_FLUSH_REMOTE_CACHE command structure
 */
typedef struct _host_cmd {
	u_int   hostid;
	u_short port;
	u_char  kernel;
} host_cmd;

/*
 * SATMP interface system calls
 */
#ifdef _KERNEL
struct socket;
struct syssesmgra;
union rval;

extern int sesmgr_enabled;

int  sesmgr_get_attr_request (satmp_token_list *);
int  sesmgr_get_lrtok_request (satmp_lrep_list *);
int  sesmgr_init_request (u_int, u_int *);
int  sesmgr_satmp_syscall (struct syssesmgra *, union rval *);
void sesmgr_satmp_init (void);
void sesmgr_satmp_soclear (struct socket *);

#define _SESMGR_SATMP_SOCLEAR(a)	((sesmgr_enabled) ? \
					 sesmgr_satmp_soclear(a) : (void) 0)
#endif /* _KERNEL */

int satmp_init (int, u_int);
int satmp_done (void);
int satmp_get_attr_reply (int, const void *, size_t);
int satmp_get_lrtok_reply (int, const void *, size_t);
int satmp_init_reply (int, satmp_esi_t, int, u_int);
int sesmgr_satmpd_started (void);

#ifdef __cplusplus
}
#endif

#endif  /* __T6SATMP_H__ */
