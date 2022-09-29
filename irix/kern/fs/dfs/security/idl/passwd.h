/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: passwd.h,v $
 * Revision 65.2  1999/02/03 22:44:20  mek
 * Convert from IDL to C style include format.
 *
 * Revision 65.1  1997/10/20 19:46:31  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.8.4  1996/11/13  18:01:49  arvind
 * 	Add/modify copyright info
 * 	[1996/09/16  20:28 UTC  aha  /main/DCE_1.2.2/5]
 *
 * 	CHFts19896: public key version support
 * 	[1996/09/13  23:42 UTC  aha  /main/DCE_1.2.2/aha_pk9_3/1]
 *
 * 	Merge Registry migration functionality: database reformat and enable sec_passwd_genprivkey
 * 	[1996/07/18  18:45 UTC  aha  /main/DCE_1.2.2/aha_pk7/1]
 *
 * 	Remove sec_passwd_c_max_key_size; no longer needed
 * 	[1996/07/13  20:49 UTC  aha  /main/DCE122_PK/aha_pk6/3]
 *
 * 	Add new password type for KDC pubkey key-pair update
 * 	[1996/04/12  20:31 UTC  aha  /main/aha_pk2/1]
 *
 * 	Add sec_passwd_genprivkey to sec_passwd_type_t, and a case
 * 	for it in sec_passwd_rec_t.  This type has no associated data.
 * 	It signals the registry to generate a new private key for the
 * 	targetted principal.  Only the krbtgt principal is allowed
 * 	to have this.
 * 	[1994/01/28  23:10:48  burati  1.1.4.1]
 *
 * Revision 1.1.8.3  1996/10/03  14:53:13  arvind
 * 	CHFts19896: public key version support
 * 	[1996/09/13  23:42 UTC  aha  /main/DCE_1.2.2/aha_pk9_3/1]
 * 
 * 	Change sec_passwd_privkey to sec_passwd_pubkey
 * 	[1996/09/10  21:07 UTC  aha  /main/DCE_1.2.2/aha_pk9_2/1]
 * 
 * 	Merge Registry migration functionality: database reformat and enable sec_passwd_genprivkey
 * 	[1996/07/18  18:45 UTC  aha  /main/DCE_1.2.2/aha_pk7/1]
 * 
 * 	Remove sec_passwd_c_max_key_size; no longer needed
 * 	[1996/07/13  20:49 UTC  aha  /main/DCE122_PK/aha_pk6/3]
 * 
 * 	Add new password type for KDC pubkey key-pair update
 * 	[1996/04/12  20:31 UTC  aha  /main/aha_pk2/1]
 * 
 * 	Add sec_passwd_genprivkey to sec_passwd_type_t, and a case
 * 	for it in sec_passwd_rec_t.  This type has no associated data.
 * 	It signals the registry to generate a new private key for the
 * 	targetted principal.  Only the krbtgt principal is allowed
 * 	to have this.
 * 	[1994/01/28  23:10:48  burati  1.1.4.1]
 * 
 * Revision 1.1.8.2  1996/08/09  12:06:19  arvind
 * 	Merge Registry migration functionality: database reformat and enable sec_passwd_genprivkey
 * 	[1996/07/18  18:45 UTC  aha  /main/DCE_1.2.2/aha_pk7/1]
 * 
 * 	Merge Registry support for KDC private key storage
 * 	[1996/07/18  17:33 UTC  aha  /main/DCE_1.2.2/3]
 * 
 * 	Remove sec_passwd_c_max_key_size; no longer needed
 * 	[1996/07/13  20:49 UTC  aha  /main/DCE122_PK/aha_pk6/3]
 * 
 * 	Merge-out from DCE122_PK to aha_pk6
 * 	[1996/06/18  20:55 UTC  aha  /main/DCE122_PK/aha_pk6/2]
 * 
 * 	Merge from cuti_pk_export to DCE_1.2.2
 * 	[1996/06/30  21:59 UTC  cuti  /main/DCE_1.2.2/2]
 * 
 * 	Merge PSM, sec_pvtkey, sec_pubkey, dced, base defs for Public Key Login
 * 	[1996/06/11  17:56 UTC  aha  /main/DCE_1.2.2/1]
 * 
 * 	Add new password type for KDC pubkey key-pair update
 * 	[1996/04/12  20:31 UTC  aha  /main/aha_pk2/1]
 * 
 * 	Add sec_passwd_genprivkey to sec_passwd_type_t, and a case
 * 	for it in sec_passwd_rec_t.  This type has no associated data.
 * 	It signals the registry to generate a new private key for the
 * 	targetted principal.  Only the krbtgt principal is allowed
 * 	to have this.
 * 	[1994/01/28  23:10:48  burati  1.1.4.1]
 * 
 * Revision 1.1.8.1  1996/07/08  18:33:58  arvind
 * 	Merge from cuti_pk_export to DCE_1.2.2
 * 	[1996/06/27  19:33 UTC  cuti  /main/DCE122_PK/cuti_pk_export/2]
 * 
 * 	Merge out from DCE122
 * 
 * 	Idl does not like const breaks into multi-line
 * 	[1996/06/27  14:02 UTC  cuti  /main/DCE122_PK/cuti_pk_export/1]
 * 
 * 	Changes for Public Key Login; support for Registry storage of KDC private keys
 * 	[1996/06/05  16:14 UTC  aha  /main/DCE12_PK/aha_pk5/1]
 * 
 * 	Merge PSM, sec_pvtkey, sec_pubkey, dced, base defs for Public Key Login
 * 	[1996/06/04  18:50 UTC  psn  /main/DCE122_PK/1]
 * 
 * 	Add new password type for KDC pubkey key-pair update
 * 	[1996/04/12  20:31 UTC  aha  /main/aha_pk2/1]
 * 
 * 	Add sec_passwd_genprivkey to sec_passwd_type_t, and a case
 * 	for it in sec_passwd_rec_t.  This type has no associated data.
 * 	It signals the registry to generate a new private key for the
 * 	targetted principal.  Only the krbtgt principal is allowed
 * 	to have this.
 * 	[1994/01/28  23:10:48  burati  1.1.4.1]
 * 
 * Revision 1.1.6.2  1996/03/09  23:28:00  marty
 * 	Update OSF copyright year
 * 	[1996/03/09  22:43:28  marty]
 * 
 * Revision 1.1.6.1  1995/12/08  17:31:59  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/08  16:55:02  root]
 * 
 * Revision 1.1.4.1  1994/01/28  23:10:48  burati
 * 	Import "dce/sec_base.idl", not "sec_base.idl"
 * 	[1994/01/21  17:02:24  burati]
 * 
 * 	Delegation/EPAC changes (dlg_bl1)
 * 	[1994/01/18  20:52:47  burati]
 * 
 * Revision 1.1.2.2  1992/12/29  13:29:39  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/28  20:16:48  zeliff]
 * 
 * Revision 1.1  1992/01/19  14:54:55  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/* Generated by IDL compiler version OSF DCE T1.2.0-09 */
#ifndef passwd_v0_0_included
#define passwd_v0_0_included
#ifndef IDLBASE_H
#include <dce/idlbase.h>
#endif

#ifdef __cplusplus
    extern "C" {
#endif

#ifndef nbase_v0_0_included
#include <dce/nbase.h>
#endif
#ifndef sec_base_v0_0_included
#include <dce/sec_base.h>
#endif
#ifndef sec_pk_base_v0_0_included
#include <dce/sec_pk_base.h>
#endif
#define sec_passwd_c_des_key_size (8)
typedef idl_byte sec_passwd_des_key_t[8];
#define sec_passwd_str_max_len (512)
#define sec_passwd_str_t_size (513)
typedef idl_char sec_passwd_str_t[513];
#define sec_passwd_c_max_pk_modulus (4096)
#define sec_passwd_c_max_pk_overhead (384)
#define sec_passwd_c_max_pk_key_size (1408)
typedef enum {sec_passwd_none=0,
sec_passwd_plain=1,
sec_passwd_des=2,
sec_passwd_pubkey=3,
sec_passwd_genprivkey=4} sec_passwd_type_t;
typedef struct  {
sec_passwd_version_t version_number;
idl_char *pepper;
struct  {
sec_passwd_type_t key_type;
union  {
/* case(s): 1 */
idl_char *plain;
/* case(s): 2 */
sec_passwd_des_key_t des_key;
/* case(s): 3 */
sec_pk_data_t pub_key;
/* case(s): 4 */
unsigned32 modulus_size;
} tagged_union;
} key;
} sec_passwd_rec_t;

#ifdef __cplusplus
    }

#endif
#endif
