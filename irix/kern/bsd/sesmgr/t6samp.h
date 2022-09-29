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

#ifndef __T6SAMP_H__
#define __T6SAMP_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident  "$Revision: 1.1 $"

/*
 * T6 Token Index values
 */
#define T6SAMP_SL          0    /* 0001: Sensitivity Label      */
#define T6SAMP_NAT_CAVEAT  1    /* 0002: NCAV                   */
#define T6SAMP_INTEG_LABEL 2    /* 0004: Integrity Label        */
#define T6SAMP_SID         3    /* 0008: Session Id             */
#define T6SAMP_CLEARANCE   4    /* 0010: Clearance              */
#define T6SAMP_UNUSED      5    /* 0020: Undefined              */
#define T6SAMP_IL          6    /* 0040: Information Label      */
#define T6SAMP_PRIVS       7    /* 0080: Privileges             */
#define T6SAMP_LUID        8    /* 0100: Login user id          */
#define T6SAMP_PID         9    /* 0200: Process Id             */
#define T6SAMP_IDS        10    /* 0400: Descretionary ID's     */
#define T6SAMP_AUDIT_INFO 11    /* 0800: Additional Audit Info  */
#define T6SAMP_PROC_ATTR  12    /* 1000: Process Attributes     */
#define T6SAMP_RESERVED_1 13    /* 2000: Reserved for future use*/
#define T6SAMP_RESERVED_2 14    /* 4000: Reserved for future use*/
#define T6SAMP_RESERVED_3 15    /* 8000: Reserved for future use*/

#define T6SAMP_MAX_ATTR		12    /* Maximum token array size*/
#define T6SAMP_MAX_TOKENS	13    /* Maximum token array size*/

/* Macro to convert Token Index to mask value */
#define T6SAMP_MASK(index)	((u_short)(1<<(index)))


#define T6SAMP_HEADER_LEN	8		/* Size of the protocol */
#define T6SAMP_NULL_ATTR_LEN	6
#define T6SAMP_ATTR_HEADER_LEN	12
#define T6SAMP_MIN_HEADER	14		/* 8 header + 6 bytes null
							attribute */
typedef struct t6samp_header {
	/* Protocol Header */
	u_int16_t samp_type;		/* Session management protocol id */
	u_int16_t samp_version;		/* ASCII protocol version number */
	u_int32_t samp_length;		/* Total length including this header */
	/* Samp Attributes */
	u_char	  attr_type;		/* CIPSO format option type = 134 */
	u_char	  attr_length;		/* Length of this specification */
	u_char 	  reserved[4];		/* Not used (Must be zero) */
	u_char	  token_type;
	u_char	  token_generation[3];
	u_int16_t token_mask;		/* Attribute mask specification */
	u_int32_t tokens[T6SAMP_MAX_TOKENS];
} t6samp_header_t;

/*
 *  Session Management Protocol Id Definitions
 */
#define CIPSO_FORMAT_OPTION	134
#define T6SAMP_ID		2		/* TSIX(re) 1.1 protocol id */
#define T6SAMP_VERSION		0x3031		/* Protocol version 1       */
#define T6SAMP_DOMAIN		0		/* Domain is not used       */
#define T6SAMP_TOKEN_TYPE	3		/* T6 CIPSO token type      */
#define T6SAMP_PRIV_SID		1		/* privileged session id    */


#ifdef __cplusplus
}
#endif

#endif  /* __T6SAMP_H__ */
