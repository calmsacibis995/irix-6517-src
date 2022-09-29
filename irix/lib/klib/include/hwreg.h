#ident "$Header: "

#ifndef _HWREG_H_
#define _HWREG_H_

/*
 * Access mode
 */

typedef struct hwreg_accmode_s hwreg_accmode_t;

struct hwreg_accmode_s {
	char	       *name;
	char	       *desc;
};

/*
 * Register field
 */

typedef struct hwreg_field_s {
	__uint32_t		nameoff;	/* Field name			*/
	uchar_t		topbit;		/* Field's msbit position 	*/
	uchar_t		botbit;		/* Field's lsbit position 	*/
	uchar_t		accmode;	/* Access mode RO, WO, or RW 	*/
	uchar_t		reset;		/* Flag whether set upon reset	*/
	__uint64_t		resetval;	/* System reset initial value 	*/
} hwreg_field_t;

/*
 * Register
 */

typedef struct hwreg_s {
#ifdef HWREG_COMPILE
	char	       *name;		/* Register name		*/
#else
	__uint32_t		nameoff;	/* Register name		*/
#endif
	__uint32_t		noteoff;	/* Informational note		*/
	__uint64_t		address;	/* Address of register 		*/
	ushort_t		sfield;		/* Start field index		*/
	uchar_t		nfield;		/* Number of fields		*/
} hwreg_t;

typedef struct hwreg_set_s {
	hwreg_t	       *regs;
	hwreg_field_t      *fields;
	char	       *strtab;
	int			regcount;
} hwreg_set_t;

/*
 * Externs
 */

extern	char	       *hwreg_upcase(char *dst, char *src);
extern	void		hwreg_name2c(char *dst, char *src);
extern	void		hwreg_getbits(char *s,
					  uchar_t *topbit, uchar_t *botbit);
extern	__int64_t	hwreg_ctoi(char *s, char **end_s);

extern	hwreg_t	       *hwreg_lookup_name(hwreg_set_t *regset,
					  char *name, int partial);
extern	hwreg_t	       *hwreg_lookup_addr(hwreg_set_t *regset,
					  __uint64_t addr);

extern	hwreg_field_t  *hwreg_lookup_field(hwreg_set_t *regset, hwreg_t *r,
					   char *name, int partial);

extern	__uint64_t	hwreg_reset_default(hwreg_set_t *regset, hwreg_t *r);

extern	__uint64_t	hwreg_encode_field(hwreg_set_t *regset, hwreg_t *r,
					   char *asst,
					   int (*prf)(const char *fmt, ...));

extern	__uint64_t	hwreg_encode(hwreg_set_t *regset, hwreg_t *r,
					 char *assts,
					 int (*prf)(const char *fmt, ...));

extern	void		hwreg_decode(hwreg_set_t *regset, hwreg_t *r,
					 int shortform, int showreset,
					 int base, __uint64_t value,
					 int (*prf)(const char *fmt, ...));

extern	hwreg_accmode_t	hwreg_accmodes[];

#endif /* _HWREG_H_ */
