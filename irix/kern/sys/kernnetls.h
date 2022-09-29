#ifndef kernnetls_included
#define kernnetls_included
/*
 * ========================================================================== 
 * Confidential and Proprietary.  Copyright 1987 by Apollo Computer Inc.,
 * Chelmsford, Massachusetts.  Unpublished -- All Rights Reserved Under
 * Copyright Laws Of The United States.
 *
 * Apollo Computer Inc. reserves all rights, title and interest with respect 
 * to copying, modification or the distribution of such software programs and
 * associated documentation, except those rights specifically granted by Apollo
 * in a Product Software Program License, Source Code License or Commercial
 * License Agreement (APOLLO NETWORK COMPUTING SYSTEM) between Apollo and 
 * Licensee.  Without such license agreements, such software programs may not
 * be used, copied, modified or distributed in source or object code form.
 * Further, the copyright notice must appear on the media, the supporting
 * documentation and packaging as set forth in such agreements.  Such License
 * Agreements do not grant any rights to use Apollo Computer's name or trademarks
 * in advertising or publicity, with respect to the distribution of the software
 * programs without the specific prior written permission of Apollo.  Trademark 
 * agreements may be obtained in a separate Trademark License Agreement.
 * ========================================================================== 
 */

/*
 * ========================================================================== 
 * Used by ShareII optional product to validate license.
 * Extracted from netls product.
 *
 * ========================================================================== 
 */

#define	longtype	int	/* netls always deals with 32-bit objects */

#ifndef true
   typedef unsigned char boolean;
#  define true  ((unsigned char) 0xff)
#  define false ((unsigned char) 0x00)
#endif                                    

#define ndr_boolean        unsigned char
#define ndr_false          ((unsigned char) 0x00)
#define ndr_true           ((unsigned char) 0xff)
#define ndr_byte           unsigned char
#define ndr_char           unsigned char
#define ndr_small_int      char
#define ndr_usmall_int     unsigned char
#define ndr_short_int      short int
#define ndr_ushort_int     unsigned short int
#define ndr_long_int       int
#define ndr_ulong_int      unsigned int

typedef struct status_t status_t;
struct status_t {
ndr_long_int all;
};
#define status_ok 0

typedef ndr_long_int status_all_t;
typedef ndr_ulong_int ls_time_t;
typedef ndr_ushort_int ls_date_t;
typedef ndr_ulong_int ls_rt_t;
typedef ndr_char nls_la_t[82];
typedef struct NIDL_tag_ab1 prod_info_t;
struct NIDL_tag_ab1 {
ndr_char appl_name[32];
ndr_char version[12];
nls_la_t lic_annot;
ndr_short_int lic_annot_len;
ndr_long_int prod_id;
ls_time_t time_stamp;
ls_time_t start_date;
ls_time_t exp_date;
ls_rt_t recordtype;
ndr_long_int prodno;
ndr_long_int total_lics;
ndr_long_int aggregate_duration;
ls_time_t derived_start;
ls_time_t derived_end;
ls_rt_t destination_type;
ndr_short_int in_use;
ndr_char multi_use_flag;
ndr_char buddiable;
};
typedef ndr_long_int ls_target_t;

/*
 * error codes for the license server 
 */ 

#define netls_server               0x1D010000   /* server module             */
#define netls_tools                0x1D030000   /* tools libraries (xxx_subr)*/
#define netls_bad_lic_annot        netls_server + 0x29

#define netls_bad_password         netls_tools  + 0x2
#define netls_wrong_target         netls_tools  + 0x3   
#define netls_bad_pword_ver        netls_tools  + 0x4
#define netls_not_bundled          netls_tools  + 0x6


   
extern boolean	nls_check_version( char *, char);
extern void	nls_decode_product( char *, int, nls_la_t, prod_info_t *,
			char *, ls_target_t *, char *, int *);
extern void	nls_decode_vendor( char *, int *, int *);
extern void	nls_set_key( int );

#ifdef KERNNETLS_EXAMPLE
/*
 * Example of decode routine which uses kernnetls
 */
#include <sys/kernnetls.h>
#include <sys/cmn_err.h>

/*
 *	decode_license
 *
 *	Decodes a NetLS license to retrieve encoded information.
 *
 *	Inputs:
 *		vpassword	Vendor Password
 *		ppassword	Product Password
 *		version		Version string (eg "4.0")
 *		lic_annot	Annotation string
 *		target		sysinfo number of the license host
 *
 *	Outputs:
 *		ttype		Platform type of the license 
 *		prod_id		Product id for the product
 *		time_stamp	Time when license was generated	(seconds)
 *		start_date	State date of license (seconds)
 *		exp_date	Expiration date of license (seconds)
 *		recordtype	Type of license 
 *		totallic	No of licenses
 *		
 *	Returns:
 *		1	success
 *		0	bad vendor password
 *	       -1	bad product password
 */

int 
decode_license(vpassword, ppassword, version, lic_annot, target,	
	       ttype, prod_id, time_stamp, start_date, exp_date, 
	       recordtype, totallic)
char		*vpassword;
char		*ppassword;
char		*version;
nls_la_t	lic_annot;
longtype		target;
char		*ttype;
longtype		*prod_id;
longtype		*time_stamp;
longtype		*start_date;
longtype		*exp_date;
longtype		*recordtype;
longtype		*totallic;
{
    prod_info_t		prec;
    longtype		ttarget;	/* place holder for target */
    longtype		key;
    int			status;
    char		pversion;

    nls_set_key(target);

    nls_decode_vendor(vpassword, &key, &status);

    if (status != 0)	/* Bad vendor password */
	return(0);

    nls_decode_product(ppassword, key, lic_annot, &prec, ttype, &ttarget,
		       &pversion, &status);

    if (status != 0)	/* Bad product password */
	return(-1);

    if (nls_check_version(version ,pversion) == false) {
        printf("Bad version\n");
	return(0);
    }

    *prod_id = prec.prod_id;
    *time_stamp = prec.time_stamp;
    *start_date = prec.start_date;
    *exp_date = prec.exp_date;
    *recordtype = prec.recordtype;
    *totallic = prec.total_lics;

    return(1);
}
#endif /* KERNNETLS_EXAMPLE */

#endif	/* kernnetls_included */
