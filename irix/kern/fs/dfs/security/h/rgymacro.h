/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: rgymacro.h,v $
 * Revision 65.1  1997/10/20 19:46:11  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.15.3  1996/10/03  14:50:35  arvind
 * 	CHFts19896: public key version support
 * 	[1996/09/13  23:42 UTC  aha  /main/DCE_1.2.2/aha_pk9_3/1]
 *
 * 	Change priv_key to pub_key
 * 	[1996/09/10  21:07 UTC  aha  /main/DCE_1.2.2/aha_pk9_2/1]
 *
 * 	CHFts19728: support salvage of krbtgt private key values
 * 	[1996/08/27  16:02 UTC  aha  /main/DCE_1.2.2/aha_dbpk1_1/1]
 *
 * 	CHFts19728: support salvaging of krbtgt private key
 * 	[1996/08/27  15:32 UTC  aha  /main/DCE_1.2.2/aha_dbpk1/1]
 *
 * 	Define PRIVKEY_PASSWD_LEN and PRIVKEY_PASSWD_DATA macros
 * 	[1996/07/18  17:32 UTC  aha  /main/DCE_1.2.2/2]
 *
 * 	Merge Registry support for KDC private key storage
 * 	[1996/06/18  20:55 UTC  aha  /main/DCE_1.2.2/aha_pk6/2]
 *
 * 	Changes for Public Key Login; support Registry storage of KDC private key.
 * 	[1996/06/07  19:05 UTC  aha  /main/DCE_1.2.2/aha_pk6/1]
 *
 * 	Changes for Public Key Login; support Registry storage of KDC private key.
 * 	[1996/03/27  17:45 UTC  hanfei  /main/DCE_1.2/1]
 *
 * Revision 1.1.15.2  1996/08/09  12:04:26  arvind
 * 	Merge Registry support for KDC private key storage
 * 	[1996/06/18  20:55 UTC  aha  /main/DCE_1.2.2/aha_pk6/2]
 * 
 * 	Changes for Public Key Login; support Registry storage of KDC private key.
 * 	[1996/06/07  19:05 UTC  aha  /main/DCE_1.2.2/aha_pk6/1]
 * 
 * 	merge in global group work for DCE 1.2.2
 * 	[1996/03/27  17:45 UTC  hanfei  /main/DCE_1.2/1]
 * 
 * 	multi-cell group work: add macro check_domain_expand.
 * 	[1996/02/02  18:37 UTC  hanfei  /main/hanfei_dce1.2_b1/1]
 * 
 * 	Update OSF copyright years
 * 	[1996/02/18  22:17:04  marty]
 * 
 * 	*
 * 
 * 	Submit
 * 	[1995/12/11  15:14:25  root]
 * 
 * 	Changes for Public Key Login; support Registry storage of KDC private key.
 * 	[1996/03/27  17:45 UTC  hanfei  /main/DCE_1.2/1]
 * 
 * Revision 1.1.15.1  1996/05/10  13:15:13  arvind
 * 	Drop 1 of DCE 1.2.2 to OSF
 * 
 * 	HP revision /main/DCE_1.2/1  1996/03/27  17:45 UTC  hanfei
 * 	merge in global group work for DCE 1.2.2
 * 
 * 	HP revision /main/hanfei_dce1.2_b1/1  1996/02/02  18:37 UTC  hanfei
 * 	multi-cell group work: add macro check_domain_expand.
 * 	[1996/05/09  20:49:04  arvind]
 * 
 * Revision 1.1.9.1  1994/10/17  18:18:07  cuti
 * 	deleted unused LEGAL_ATTR_NAME
 * 	[1994/10/14  16:14:56  cuti]
 * 
 * Revision 1.1.6.2  1994/05/11  19:05:06  ahop
 * 	hp_sec_to_osf_2 drop
 * 	[1994/04/29  20:48:17  ahop]
 * 
 * 	hp_sec_to_osf_2 drop
 * 
 * 	hp_sec_to_osf_2 drop
 * 
 * Revision 1.1.4.2  1993/12/14  11:32:52  mdf
 * 	Fix for CHFts09919
 * 	[1993/12/13  15:41:13  mdf]
 * 
 * 	Added check to ensure that a LEGAL_PGO_NAME is not of zero length.
 * 	The only function that uses the LEGAL_PGO_NAME macro is rs_pgo_add, which
 * 	is appropriate, becuase you can't create a pgo node with a null name.
 * 	[1993/12/10  15:00:44  mdf]
 * 
 * Revision 1.1.2.3  1993/09/15  15:38:58  root
 * 	    Loading drop DCE1_0_3b03
 * 
 * Revision 1.1.4.3  1992/12/29  13:07:06  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/28  20:43:29  zeliff]
 * 
 * Revision 1.1.4.2  1992/08/31  18:16:51  sekhar
 * 	First security replication code drop
 * 	[1992/08/31  14:56:41  sekhar]
 * 
 * Revision 1.1.2.2  1992/05/22  20:06:55  ahop
 * 	 CR3427: add LEGAL_PGO_NAME macro to check for leading, trailing '/'
 * 	[1992/05/22  19:53:29  ahop]
 * 
 * Revision 1.1  1992/01/19  14:43:27  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
*/

/*
 *
 * Copyright (c) Hewlett-Packard Company 1991, 1994
 * Unpublished work. All Rights Reserved.
 *
 */
/*
 *      Registry Client/Server - misc useful macros
 *
 */

#ifndef __rgy_macro_h_included__
#define __rgy_macro_h_included__

#include <macros.h> 

#ifdef OSF_DCE
#   include <krb5/osf_dce.h>
#endif

#define check_domain(D) (((D) >= sec_rgy_domain_person) && ((D) < sec_rgy_domain_last))
#define check_domain_expand(D) (((D) >= sec_rgy_domain_person) && ((D) < sec_rgy_domain_invalid))
#define check_go_domain(D) (((D) == sec_rgy_domain_group) || ((D)== sec_rgy_domain_org))

#define NOT_ZERO_LENGTH(str)  (strlen((char *) (str)) != 0)

#define IS_AN_ALIAS(P) ((P)->flags & sec_rgy_pgo_is_an_alias)

#define CHECK_RGY_PNAME(str) (strlen( (char *) (str)) < \
			      sizeof(sec_rgy_pname_t))
#define CHECK_RGY_NAME(str) (strlen( (char *) (str)) < \
			      sizeof(sec_rgy_name_t))

#define CHECK_PGO_NAME(name) (CHECK_RGY_NAME((name)))

#define LEGAL_PGO_NAME(name) (NOT_ZERO_LENGTH(name) && ( \
		*((char *)(name)) != '/'  && \
		*((char *)(name) + strlen((char *)(name)) - 1) != '/' ))

#define CHECK_PGO_FULLNAME(fname) (CHECK_RGY_PNAME((fname)))

#define CHECK_LOGIN_NAME(lname) ( CHECK_PGO_NAME((lname)->pname) && \
		CHECK_PGO_NAME((lname)->gname) && \
		CHECK_PGO_NAME((lname)->oname) )

#define NULL_PLAIN_PASSWD(p) ((p) == NULL ? true : (*p) == '\0')

#define CHECK_ACCT_KEY_PARTS(kp_ptr) (*(kp_ptr) == sec_rgy_acct_key_person)
#define CHECK_ACCT_GECOS(str) CHECK_RGY_PNAME((str))
#define CHECK_ACCT_HOMEDIR(str) CHECK_RGY_PNAME((str))
#define CHECK_ACCT_SHELL(str)  CHECK_RGY_PNAME((str))
#define CHECK_ACCT_PLAIN_PASSWD(p) (NULL_PLAIN_PASSWD(p) ? true : \
				    strlen( (char *) (p)) <= \
			            sec_passwd_str_max_len)

#define CHECK_ACCT_USER_DATA(user_part_p) \
                ( CHECK_ACCT_GECOS((user_part_p)->gecos) \
                  && CHECK_ACCT_HOMEDIR((user_part_p)->homedir) \
                  && CHECK_ACCT_SHELL((user_part_p)->shell) )


#define RGY_POLICY(policy_key) \
                ( ((policy_key) == NULL) || (*(policy_key) == '\0') )

/* returns true if the auth policy query key components are all zero-length
 * indicating that the caller has specified registry-wide authentication policy
 * as opposed to policy for a particular account.
 */
#define RGY_AUTH_POLICY(lname) \
               ( (((lname)->pname == NULL) || (*((lname)->pname) == '\0')) \
               && (((lname)->gname == NULL) || (*((lname)->gname) == '\0')) \
	       && (((lname)->oname == NULL) || (*((lname)->oname) == '\0')) )

#define IS_SET(FIELD, MASK) ((FIELD) & (MASK)) == (MASK)

/* true when a login name component is a wildcard */
#define IS_WILD(component) ((component) == NULL || *(component) == '\0')

/* make a login name component a wildcard */
#define MAKE_WILD(component) (*(component) = '\0')

/* construct a wildcard login_name */
#define CLEAR_LOGIN_NAME(login_name_p) \
                MAKE_WILD((login_name_p)->pname); \
                MAKE_WILD((login_name_p)->gname); \
                MAKE_WILD((login_name_p)->oname)

#define COPY_LOGIN_NAME(target, source) \
		strcpy( (char *) (target)->pname, (char *) (source)->pname); \
		strcpy( (char *) (target)->gname, (char *) (source)->gname); \
		strcpy( (char *) (target)->oname, (char *) (source)->oname);

#define STRING_TO_UUID(uuid_str,uuid_p,stp) \
                (uuid_from_string((uuid_str), (uuid_p), (stp)));

#define UUID_TO_STRING(uuid_p,uuid_str) \
                (uuid_to_string((uuid_p), (uuid_str)));

#define UUID_EQUAL(uuid_p1,uuid_p2,stp) \
                (uuid_equal((uuid_p1),(uuid_p2),(stp)))

/* macros for ease of reference to passwd rec fields */
#define PASSWD_TYPE(p) (p)->key.key_type
#define PLAIN_PASSWD(p) (p)->key.tagged_union.plain
#define DES_PASSWD(p) (p)->key.tagged_union.des_key
#define MOD_SIZE_PASSWD(p) (p)->key.tagged_union.modulus_size
#define PUBKEY_PASSWD(p) (p)->key.tagged_union.pub_key
#define PUBKEY_PASSWD_LEN(p) (p)->key.tagged_union.pub_key.len
#define PUBKEY_PASSWD_DATA(p) (p)->key.tagged_union.pub_key.data

#endif /* #ifndef __rgy_macro_h_included__ */
