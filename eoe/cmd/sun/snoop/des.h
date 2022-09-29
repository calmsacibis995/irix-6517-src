/*
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/sun/snoop/RCS/des.h,v $
 * $Author: nn $
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/sun/snoop/RCS/des.h,v 1.2 1996/07/06 17:42:26 nn Exp $
 *
 * Copyright 1987, 1988 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 *
 * Include file for the Data Encryption Standard library.
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sun/snoop/RCS/des.h,v 1.2 1996/07/06 17:42:26 nn Exp $"

#ifndef	_KERBEROS_DES_H
#define	_KERBEROS_DES_H


#ifdef	__cplusplus
extern "C" {
#endif

typedef unsigned char des_cblock[8];	/* crypto-block size */
/* Key schedule */
typedef struct des_ks_struct { des_cblock _; } des_key_schedule[16];

#define	DES_KEY_SZ 	(sizeof (des_cblock))
#define	KRBDES_ENCRYPT	1
#define	KRBDES_DECRYPT	0

#ifndef NCOMPAT
#define	C_Block des_cblock
#define	Key_schedule des_key_schedule
#define	ENCRYPT KRBDES_ENCRYPT
#define	DECRYPT KRBDES_DECRYPT
#define	KEY_SZ DES_KEY_SZ
#define	string_to_key des_string_to_key
#define	read_pw_string des_read_pw_string
#define	random_key des_random_key
#define	pcbc_encrypt des_pcbc_encrypt
#define	key_sched des_key_sched
#define	cbc_encrypt des_cbc_encrypt
#define	cbc_cksum des_cbc_cksum
#define	C_Block_print des_cblock_print
#define	quad_cksum des_quad_cksum
typedef struct des_ks_struct bit_64;
#endif

#define	des_cblock_print(x) des_cblock_print_file(x, stdout)

#ifdef	__cplusplus
}
#endif

#endif	/* _KERBEROS_DES_H */
