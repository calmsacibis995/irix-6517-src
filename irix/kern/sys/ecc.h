#ifndef	__SYS_ECC_H__
#define	__SYS_ECC_H__

/*
 * ECC syndromes are an index into an array, the entries of which indicate
 * the severity and--if is a correctable error--the location of the ECC
 * error.  'OK' is a syndrome of zero and indicates that the codes detected
 * no error.  'DB' and 'CB' indicate single-bit errors in a data-bit and
 * check-bit, resp., and are correctable because they identify the bit
 * position that is in error.  'B2' and 'B3' designate 2- and 3-bit
 * errors in a nibble, and 'UN' means an unfixable-error of another type
 * occurred.
 */
enum error_type {OK, DB, CB, B2, B3, B4, UN};

/*
 * in the eccdesc struct, the 'error_type' field indicates the type of 
 * error, and 'value' indicates the bit-position of the error if it is
 * correctable.  There are separate eccdesc tables for the data ECC 
 * (8 checkbits per 64 bit double-word) and the 7-bit secondary-cache 
 * tags (7 checkbits per 25-bits of the STagLo register).
 */
typedef struct eccdesc {
	enum error_type type;
	char value;
} eccdesc_t;

#define	ECCSYN_TABSIZE(esyntab)		(sizeof(esyntab) / sizeof(eccdesc_t))

extern char *etstrings[];
extern eccdesc_t real_data_eccsyns[];
extern eccdesc_t real_tag_eccsyns[];

#endif	/* __SYS_ECC_H__ */
