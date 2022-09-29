/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: base-defs.h,v $
 * Revision 65.1  1997/10/20 19:47:44  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.10.1  1996/10/03  14:55:27  arvind
 * 	Placebos for beta6 code.
 * 	[1996/09/16  21:25 UTC  sommerfeld  /main/sommerfeld_pk_kdc_1/1]
 *
 * Revision 1.1.8.3  1996/02/18  23:02:08  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:20:34  marty]
 * 
 * Revision 1.1.8.2  1995/12/08  17:41:15  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/1  1995/04/15  00:00 UTC  rps
 * 	copyright
 * 	[1995/04/05  17:04 UTC  rps  /main/MOTHRA_panic/2]
 * 
 * 	Fix krb5_princ_name macro
 * 	[1995/01/29  21:44 UTC  rps  /main/rps_hc/1]
 * 
 * Revision 1.1.4.2  1994/08/26  16:01:17  sommerfeld
 * 	Fix krb5_princ_type macro.
 * 	Add more KRB5 beta1 stuff
 * 	[1994/08/26  15:51:04  sommerfeld]
 * 
 * Revision 1.1.4.1  1994/05/11  19:15:21  ahop
 * 	hp_sec_to_osf_2 drop
 * 	[1994/04/29  21:11:55  ahop]
 * 
 * Revision 1.1.2.2  1992/12/29  13:57:57  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/28  20:51:44  zeliff]
 * 
 * Revision 1.1  1992/01/19  14:48:28  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */

/*
**
** Copyright (c) Hewlett-Packard Company 1991, 1994, 1995
** Unpublished work. All Rights Reserved.
**
*/
/*
 *
 * Copyright 1989,1990 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America is assumed
 *   to require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 * 
 *
 * Basic definitions for Kerberos V5 library
 */


#ifndef KRB5_BASE_DEFS__
#define KRB5_BASE_DEFS__

#ifdef OSF_DCE
#define OSF_DCE_ARCHAISM
#define OSF_DCE_FEATURE
#endif

/* For Kerberos V5 Beta 1 compatibility */
#ifndef KRB5_BETA1
#define KRB5_BETA1
#endif

#ifdef OSF_DCE_ARCHAISM
typedef void *krb5_context;
#define krb5_klog_syslog syslog
#define krb5_const const

#endif

#include <krb5/wordsize.h>

#ifndef FALSE
#define	FALSE	0
#endif
#ifndef TRUE
#define	TRUE	1
#endif

typedef krb5_octet	krb5_boolean;
typedef	krb5_octet	krb5_msgtype;
typedef	krb5_octet	krb5_kvno;

typedef	krb5_ui_2	krb5_addrtype;
typedef krb5_ui_2	krb5_keytype;
typedef krb5_ui_2	krb5_enctype;
typedef krb5_ui_2	krb5_cksumtype;
typedef krb5_ui_2	krb5_authdatatype;

typedef krb5_int32	krb5_preauthtype; /* This may change, later on */
typedef	krb5_int32	krb5_flags;
typedef krb5_int32	krb5_timestamp;
typedef	krb5_int32	krb5_error_code;
typedef krb5_int32	krb5_deltat;

typedef struct _krb5_data {
    int length;
    char *data;
} krb5_data;

/* make const & volatile available without effect */

#if !defined(__STDC__) && !defined(HAS_ANSI_CONST)
#define const
#endif
#if !defined(__STDC__) && !defined(HAS_ANSI_VOLATILE)
#define volatile
#endif

#if defined(__STDC__) || defined(HAS_VOID_TYPE)
typedef	void * krb5_pointer;
typedef void const * krb5_const_pointer;
#else
typedef char * krb5_pointer;
typedef char const * krb5_const_pointer;
#endif

#if defined(__STDC__) || defined(KRB5_PROVIDE_PROTOTYPES)
#define PROTOTYPE(x) x
#if defined(__STDC__) || defined(STDARG_PROTOTYPES)
#define	STDARG_P(x) x
#else
#define STDARG_P(x) ()
#endif /* defined(__STDC__) || defined(STDARG_PROTOTYPES) */
#ifdef NARROW_PROTOTYPES
#define DECLARG(type, val) type val
#define OLDDECLARG(type, val)
#else
#define DECLARG(type, val) val
#define OLDDECLARG(type, val) type val;
#endif /* NARROW_PROTOTYPES */
#else
#define PROTOTYPE(x) ()
#define STDARG_P(x) ()
#define DECLARG(type, val) val
#define OLDDECLARG(type, val) type val;
#endif /* STDC or PROTOTYPES */

#ifdef NO_NESTED_PROTOTYPES
#define	NPROTOTYPE(x) ()
#else
#define	NPROTOTYPE(x) PROTOTYPE(x)
#endif

#ifdef KRB5_BETA1
typedef krb5_data **    krb5_principal; /* array of strings */
                                        /* CONVENTION: realm is first elem. */
#else
typedef struct krb5_principal_data {
    krb5_data realm;
    krb5_data *data;
    krb5_int32 length;
    krb5_int32 type;
} krb5_principal_data;

typedef	krb5_principal_data *krb5_principal;	/* array of strings */
#endif /* KRB5_BETA1 */

/*
 * Per V5 spec on definition of principal types
 */

/* Name type not known */
#define KRB5_NT_UNKNOWN		0
/* Just the name of the principal as in DCE, or for users */
#define KRB5_NT_PRINCIPAL	1
/* Service and other unique instance (krbtgt) */
#define KRB5_NT_SRV_INST	2
/* Service with host name as instance (telnet, rcommands) */
#define KRB5_NT_SRV_HST		3
/* Service with host as remaining components */
#define KRB5_NT_SRV_XHST	4
/* Unique ID */
#define KRB5_NT_UID		5

/* constant version thereof: */
#ifdef KRB5_BETA1
typedef krb5_data * const *  krb5_const_principal;
typedef krb5_data krb5_principal_data;
#else
typedef const krb5_principal_data *krb5_const_principal;
#endif /* KRB5_BETA1 */

#ifdef KRB5_BETA1
#define krb5_princ_realm(princ) ((princ)[0])
#define krb5_princ_set_realm(princ,value) ((princ)[0] = (value))
#define krb5_princ_set_realm_length(princ,value) (princ)[0]->length = (value)
#define krb5_princ_set_realm_data(princ,value) (princ)[0]->data = (value)
extern int krb5_princ_size(krb5_const_principal p);

#define krb5_princ_type(princ) KRB5_NT_UNKNOWN
#define krb5_princ_name(princ) ((princ)[1])
#define krb5_princ_component(princ,i) (princ[i+1])
#else
#define krb5_princ_realm(princ) (&(princ)->realm)
#define krb5_princ_set_realm(princ,value) ((princ)->realm = *(value))
#define krb5_princ_set_realm_length(princ,value) (princ)->realm.length = (value)
#define krb5_princ_set_realm_data(princ,value) (princ)->realm.data = (value)
#define	krb5_princ_size(princ) (princ)->length
#define	krb5_princ_type(princ) (princ)->type
#define	krb5_princ_name(princ) (princ)->data
#define	krb5_princ_component(princ,i) ((princ)->data + i)
#endif /* KRB5_BETA1 */
#endif /* KRB5_BASE_DEFS__ */
