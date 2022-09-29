#ident "$Revision: 1.1 $"
#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <net/if.h>
struct mac_label;

/*
 * Return values for ip security options.
 */
typedef enum ip_security_retvals {
	IP_SECOPT_PASSED,
	IP_SECOPT_FAILED,
	IP_SECOPT_INVALID
} ip_security_retvals_t;

/* 
 * CIPSO Tag Types
 */
typedef enum cipso_tag_types {
	CIPSO_TAG_TYPE_1 = 1,
	CIPSO_TAG_TYPE_2,
	CIPSO_TAG_TYPE_3,	/* Not supported by SGI */
	CIPSO_TAG_TYPE_4,	/* Not supported by SGI */
	CIPSO_TAG_TYPE_5,	/* Not supported by SGI */
	CIPSO_TAG_TYPE_6	/* Not supported by SGI */
	} cipso_tag_types_t;

/*
 * SGI IP Security Extensions to CIPSO
 *
 * The following table is used to map DOI-defined tag-type numbers into
 * SGI-defined tag type numbers.
 */
typedef enum sgipso_ext {
	SGIPSO_BITMAP	= 128,	/* mint flavored bitmamp tag value */
	SGIPSO_ENUM,		/* mint flavored enumerated tag value */
	SGIPSO_SPECIAL,		/* Special label type(high,low,equal) */
	SGIPSO_DAC		/* UID tag value */
} sgipso_ext_t;

/*
 * Defines for RFC 1108 - DOD Security Option For IP (SOFTIP)
 * (See: RIPSO-BSO)
 */
#define RIPSOPT_LVL	2	/* offset to level field	*/
#define RIPSOPT_AUTH	3	/* offset to authority field	*/
#define BSO_TYPE	0	/* from RFC 1108 */
#define BSO_LGH		1	/* from RFC 1108 */
#define BSO_LVL		2	/* from RFC 1108 */
#define BSO_FLAG	3	/* from RFC 1108 */
#define BSO_MIN_LEN	4
#define level_rtoi(n) level_rtoi_tab[ (n) ]
#define level_itor(n) level_itor_tab[ (n) ]

/*
 * RIPSO-BSO RFC 1108
 */
typedef struct	ripso_bso {	/* option containing RIPSO BSO		*/
	u_char	opttype;	/* option type RIPSO BSO = 130		*/
	u_char	optlength;	/* option length			*/
	u_char	lvl;		/* Classification level		 	*/
	u_char	flags;		/* Protection Authority Flags	 	*/
} ripso_bso_t;

/*
 * Defines for Commercial IP Security Option (CIPSO) Version 2.3
 */
#define CIPSO_MIN_LEN	  6	/* from draft RFC */
#define CIPSO_MAX_LEN	  40	/* from draft RFC */
#define CIPSO_OPT	  0	/* offset to beginning of option field  */
#define CIPSO_LGH	  1	/* offset to beginning of length field  */
#define CIPSO_DOI	  2	/* offset to beginning of DOI field	*/
#define CIPSO_TAG  	  6	/* offset of first tag type		*/ 

/*
 * Defines for Commercial IP Security Option (CIPSO) Version 2.3
 * Tags Types Suboption Header.
 */
#define CIPSOTAG_VAL      0	/* offset of tag value within tag	*/
#define CIPSOTAG_LEN      1	/* offset of tag length within tag      */
#define CIPSOTAG_MIN_LGH  4	/* minimum total length tag type record */
#define CIPSOTAG_MAX_LGH  34    /* maximum total length tag type record */
#define CIPSOTAG_ALIGN    2	/* offset of alignment octet		*/
#define CIPSOTAG_LVL      3	/* offset of tag sensitivite level	*/

/*
 * Defines for Commercial IP Security Option (CIPSO) Version 2.3
 * and SGI IP Securiy Options (SGIPSO) Tags Types 1 
 * Suboptions Header (Bit Category)
 */
#define CIPSOTT1_BITMAP   4	/* offset of tag type 1 bitmap of cats	*/
#define CIPSOTT1_MIN_LVL  0	/* min sen level in tag types 1 and 129 */
#define CIPSOTT1_MAX_LVL  255	/* max sen level in tag types 1 and 129 */
#define CIPSOTT1_MIN_VAL  0	/* minimum bits  in tag types 1 and 129  */
#define CIPSOTT1_MAX_VAL  239	/* maximum bits  in tag types 1 and 129  */
#define CIPSOTT1_MIN_BITS 0	/* minimum bits  in tag types 1 and 129 */
#define CIPSOTT1_MAX_BITS 30	/* maximum bits  in tag types 1 and 129 */

/*
 * Defines for Commercial IP Security Option (CIPSO) Version 2.3
 * and SGI IP Securiy Options (SGIPSO) Tags Types 2 
 * Suboptions Header (Enumerated Category)
 */
#define CIPSOTT2_ENUMS    4	/* offset of tag type 2 enumerated cats */
#define CIPSOTT2_MIN_LVL  0	/* min sen level in tag types 2 and 130 */
#define CIPSOTT2_MAX_LVL  255   /* max sen level in tag types 2 and 130 */
#define CIPSOTT2_MIN_VAL  0     /* minimum value in tag types 2 and 130 */
#define CIPSOTT2_MAX_VAL  65534 /* maximum value in tag types 2 and 130 */
#define CIPSOTT2_MIN_ENUM 0     /* minimum enums in tag types 2 and 130 */
#define CIPSOTT2_MAX_ENUM 15    /* maximum enums in tag types 2 and 130 */

/*
 * SGIPSO DAC (uid) extended tag type
 */
#define SGIPSO_DAC_LEN   4	/* Length of SGIPSO Dac tag (uid)	*/
#define SGIPSO_UID       2	/* offset of (ushort) uid in DAC tag    */

/*
 * SGIPSO Special MSEN/MINT extended tag type
 */
#define SGIPSO_SPEC_LEN  4	/* Length of SGIPSO Special tag		*/
#define SGIPSO_MSEN      2	/* offset of msen_type in SPECIAL tag   */
#define SGIPSO_MINT      3	/* offset of mint_type in SPECIAL tag   */

/*
 * CIPSO header Version 2.3
 */
typedef struct	cipso_h	{	/* option containing CIPSO2 tag type 1	*/
	u_char	opttype;	/* option type				*/
	u_char	optlength;	/* option length			*/
	u_short	doih;		/* Most signficant 16 bits of DOI 	*/
	u_short	doil;		/* Least signficant 16 bits of DOI 	*/
} cipso_h_t;

/*
 * CIPSO tag type 1 header Version 2.3
 */
typedef struct	cipso_tt1 {	/* option containing CIPSO2 tag type 1	*/
	u_char	tagtype;	/* tag type == 1			*/
	u_char	taglength;	/* length of tag, in bytes		*/
	u_char	align;		/* alignment octet - new in CIPSO2      */
	u_char	level;		/* sensitivity level - external rep.	*/
	u_char	bits[CIPSOTT1_MAX_BITS]; /* big_endian category bit map */
} cipso_tt1_t;

/*
 * CIPSO tag type 2 header Version 2.3
 */
typedef struct	cipso_tt2 {	/* option containing CIPSO2 tag type 2	*/
	u_char	tagtype;	/* tag type == 2			*/
	u_char	taglength;	/* length of tag, in bytes		*/
	u_char	align;		/* alignment octet - new in CIPSO2     	*/
	u_char	level;		/* sensitivity level - external rep.	*/
	u_short	cats[CIPSOTT2_MAX_ENUM]; /* enumerated list of categorie*/
} cipso_tt2_t;

/* 
 * SGIPSO DAC tag type
 */
typedef struct	sgipso_dac {
	u_char	tagtype;	/* type = external_tag_type[SGIPSO_DAC] */
	u_char	taglength;	/* length = SGIPSO_DAC_LEN		*/
	ushort	uid;		/* uid  		                */
} sgipso_dac_t;

/* 
 * SGIPSO Special tag type
 */
typedef struct  sgipso_special {
        u_char  tagtype;        /* type = external_tag_type[SGIPSO_DAC] */
        u_char  taglength;      /* length = SGIPSO_DAC_LEN              */
        u_char  msen;           /* msen value                           */
        u_char  mint;           /* mint value                           */
} sgipso_special_t;

/* 
 * These pseudo functions translate category numbers from external to
 * internal values, according to the Domain Of Interpretation (DOI).
 * This version reflects the fact that we only support one DOI.
 */
#define category_xtoi(n,doi) (n)
#define division_xtoi(n,doi) (n)
#define level_xtoi(n,doi)    (n)

/* 
 * These pseudo functions translate category numbers from internal to
 * external values, according to the Domain Of Interpretation (DOI).
 * This version reflects the fact that we only support one DOI.
 */
#define category_itox(n,doi) (n)
#define division_itox(n,doi) (n)
#define level_itox(n,doi)    (n)

/*
 * Macro modified from mtod. Used to locate the end of
 * data in an mbuf.
 */
#define mpassd(x,t)       ((t)((__psint_t)(x) + (x)->m_off + (x)->m_len))

#ifdef NOTYET
extern int           if_security_invalid( ifsec_t * );
#endif
extern struct mbuf * ip_strip_security( struct mbuf * );
extern void          sort_shorts( u_short *, int );

#ifdef DEBUG
#define STATIC_PRF
#else
#define STATIC_PRF static
#endif
#ifdef __cplusplus
}
#endif
