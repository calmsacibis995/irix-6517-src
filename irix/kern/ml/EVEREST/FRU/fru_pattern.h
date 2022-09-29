/*
 * fru_pattern.h -
 *
 * This file contains all the symbols that can be used in FRU analyzer
 * patterns (or error signatures) as well as the necessary data structures.
 *
 */

/* Register and pseudo-register names */
typedef enum entry_type_e {
	BEGINPATTERNS,		/* Start of patterns */
	BEGINCASE,		/* Start of a case description. */
	BEGINBOARD,		/* Start of a set of board registers */
	BEGINSUBUNIT,		/* Start of a subunit (bank, slice, ioa) */
	ENDSUBUNIT,		/* End of a subunit */
	ENDBOARD,		/* End of a board */
	ENDCASE,		/* End of this particular case */
	ENDPATTERNS,		/* End of _all_ of the patterns */
	FRU_POINTER,		/* The FRU is _here_  Each pattern must have
				 * exactly one */

	/* IP board */
	A_ERROR,

	/* CPU slice */
	CPU_ERTOIP,
	CPU_EXT_INTR,

	/* MC3 board */
	MA_EBUSERR,
	
	/* MC3 leaf */
	LEAF_ERR,
	LEAF_ERADDRHI,
	LEAF_ERRADDRLO,
	LEAF_SYNDROME0,
	LEAF_SYNDROME1,
	LEAF_SYNDROME2,
	LEAF_SYNDROME3,

	/* IO4 board */
	IO4_IBUS_ERR,
	IO4_EBUS_ERR,
	IO4_EBUS_ERR1,
	IO4_EBUS_ERR2,

	/* EPC chip */
	EPC_IBUS_ERR,
	EPC_IA_NP_ERR1,
	EPC_IA_NP_ERR2,

	/* VMECC */
	VMECC_ERROR,
	VMECC_ADDREBUS,
	VMECC_ADDRVME,
	VMECC_XTRAVME,

	/* HIPPI */
	HIPPI_ERROR,
	HIPPI_ADDREBUS,
	HIPPI_ADDRVME,
	HIPPI_XTRAVME,
	
	/* F chip */
	F_ERROR,
	F_IBUS_ADDR,
	F_FCI,

	/* FCG */
	FCG_ERROR,

	/* DANG */
	DANG_P_ERR,
	DANG_MD_ERR,
	DANG_SD_ERR,
	DANG_MD_CMPLT,

	/* S1 */
	S1_IBUS_ERR,
	S1_CSR,
	S1_IERR_LOGLO,
	S1_IERR_LOGHI,

	/* HIP */
	HIP_ERROR

} entry_type_t;

/* Here we have an entry into the big FRU pattern array. */
typedef struct fru_entry_s {
	entry_type_t entry_type;
	unsigned value;
	unsigned mask;
} fru_entry_t;

/* These "subunit" numbers tell what a subunit is. */
#define SUBUNIT_LEAF	1
#define SUBUNIT_SLICE	2
/* All other subunits are defined by IOA_ADAP_* values */

/* Match return values */
#define FRU_MATCH_NO	0
#define FRU_MATCH_YES	1
#define FRU_MATCH_FRU	2

/*
 * Each case we can match against must have an entry in the case table.
 * See fru_pattern.c.
 */
typedef struct fru_case_s {
	int case_id;
	int confidence;
	fru_element_t fru;
	char *string;
} fru_case_t;

#define CONFIDENCE(_x)	(_x)->confidence
#define CASE_ID(_x)	(_x)->case_id

extern void update_pattern(whatswrong_t *, fru_case_t *, short, short);

extern fru_entry_t *next_token(fru_entry_t *);
extern fru_entry_t *prev_token(fru_entry_t *);
extern fru_entry_t *find_case_end(fru_entry_t *);
extern fru_entry_t *find_board_end(fru_entry_t *);
extern char *decode_control_token(fru_entry_t *token);

extern int is_control_token(fru_entry_t *);
extern int is_fru_token(fru_entry_t *);

extern int match_ip19board(fru_entry_t **, everror_t *, evcfginfo_t *,
			   whatswrong_t *, fru_case_t *);
extern int match_ip21board(fru_entry_t **, everror_t *, evcfginfo_t *,
                           whatswrong_t *, fru_case_t *);
extern int match_io4board(fru_entry_t **, everror_t *, evcfginfo_t *,
                           whatswrong_t *, fru_case_t *);
extern int match_mc3board(fru_entry_t **, everror_t *, evcfginfo_t *,
                           whatswrong_t *, fru_case_t *);

/* Global Variables */
extern fru_entry_t fru_patterns[];
extern fru_case_t fru_cases[];

extern int num_fru_patterns;
extern int num_fru_cases;

