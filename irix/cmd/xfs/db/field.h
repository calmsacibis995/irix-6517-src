#ident	"$Revision: 1.15 $"

typedef enum fldt	{
	FLDT_AEXTNUM,
	FLDT_AGBLOCK,
	FLDT_AGBLOCKNZ,
	FLDT_AGF,
	FLDT_AGFL,
	FLDT_AGI,
	FLDT_AGINO,
	FLDT_AGINONN,
	FLDT_AGNUMBER,
#if VERS >= V_62
	FLDT_ATTR,
	FLDT_ATTR_BLKINFO,
	FLDT_ATTR_LEAF_ENTRY,
	FLDT_ATTR_LEAF_HDR,
	FLDT_ATTR_LEAF_MAP,
	FLDT_ATTR_LEAF_NAME,
	FLDT_ATTR_NODE_ENTRY,
	FLDT_ATTR_NODE_HDR,
	FLDT_ATTR_SF_ENTRY,
	FLDT_ATTR_SF_HDR,
	FLDT_ATTRBLOCK,
	FLDT_ATTRSHORT,
	FLDT_BMAPBTA,
	FLDT_BMAPBTAKEY,
	FLDT_BMAPBTAPTR,
	FLDT_BMAPBTAREC,
#endif
	FLDT_BMAPBTD,
	FLDT_BMAPBTDKEY,
	FLDT_BMAPBTDPTR,
	FLDT_BMAPBTDREC,
#if VERS >= V_62
	FLDT_BMROOTA,
	FLDT_BMROOTAKEY,
	FLDT_BMROOTAPTR,
#endif
	FLDT_BMROOTD,
	FLDT_BMROOTDKEY,
	FLDT_BMROOTDPTR,
	FLDT_BNOBT,
	FLDT_BNOBTKEY,
	FLDT_BNOBTPTR,
	FLDT_BNOBTREC,
	FLDT_CEXTFLG,
	FLDT_CEXTLEN,
#if VERS >= V_62
	FLDT_CFILEOFFA,
#endif
	FLDT_CFILEOFFD,
	FLDT_CFSBLOCK,
	FLDT_CHARNS,
	FLDT_CHARS,
	FLDT_CNTBT,
	FLDT_CNTBTKEY,
	FLDT_CNTBTPTR,
	FLDT_CNTBTREC,
	FLDT_DEV,
#if VERS >= V_62
	FLDT_DFILOFFA,
#endif
	FLDT_DFILOFFD,
	FLDT_DFSBNO,
#if VERS >= V_62
	FLDT_DINODE_A,
#endif
	FLDT_DINODE_CORE,
	FLDT_DINODE_FMT,
	FLDT_DINODE_U,
	FLDT_DIR,
#if VERS >= V_654
	FLDT_DIR2,
	FLDT_DIR2_BLOCK_TAIL,
	FLDT_DIR2_DATA_FREE,
	FLDT_DIR2_DATA_HDR,
	FLDT_DIR2_DATA_OFF,
	FLDT_DIR2_DATA_OFFNZ,
	FLDT_DIR2_DATA_UNION,
	FLDT_DIR2_FREE_HDR,
	FLDT_DIR2_INO4,
	FLDT_DIR2_INO8,
	FLDT_DIR2_INOU,
	FLDT_DIR2_LEAF_ENTRY,
	FLDT_DIR2_LEAF_HDR,
	FLDT_DIR2_LEAF_TAIL,
	FLDT_DIR2_SF_ENTRY,
	FLDT_DIR2_SF_HDR,
	FLDT_DIR2_SF_OFF,
	FLDT_DIR2SF,
#endif
	FLDT_DIR_BLKINFO,
	FLDT_DIR_INO,
	FLDT_DIR_LEAF_ENTRY,
	FLDT_DIR_LEAF_HDR,
	FLDT_DIR_LEAF_MAP,
	FLDT_DIR_LEAF_NAME,
	FLDT_DIR_NODE_ENTRY,
	FLDT_DIR_NODE_HDR,
	FLDT_DIR_SF_ENTRY,
	FLDT_DIR_SF_HDR,
	FLDT_DIRBLOCK,
	FLDT_DIRSHORT,
#if VERS >= V_62
	FLDT_DISK_DQUOT,
	FLDT_DQBLK,
	FLDT_DQID,
#endif
	FLDT_DRFSBNO,
	FLDT_DRTBNO,
	FLDT_EXTLEN,
	FLDT_EXTNUM,
	FLDT_FSIZE,
	FLDT_INO,
	FLDT_INOBT,
	FLDT_INOBTKEY,
	FLDT_INOBTPTR,
	FLDT_INOBTREC,
	FLDT_INODE,
	FLDT_INOFREE,
	FLDT_INT16D,
	FLDT_INT32D,
	FLDT_INT64D,
	FLDT_INT8D,
	FLDT_NSEC,
#if VERS >= V_62
	FLDT_QCNT,
	FLDT_QWARNCNT,
#endif
	FLDT_SB,
	FLDT_TIME,
	FLDT_TIMESTAMP,
	FLDT_UINT1,
	FLDT_UINT16D,
	FLDT_UINT16O,
	FLDT_UINT16X,
	FLDT_UINT32D,
	FLDT_UINT32O,
	FLDT_UINT32X,
	FLDT_UINT64D,
	FLDT_UINT64O,
	FLDT_UINT64X,
	FLDT_UINT8D,
	FLDT_UINT8O,
	FLDT_UINT8X,
	FLDT_UUID,
	FLDT_ZZZ			/* mark last entry */
} fldt_t;

typedef int (*offset_fnc_t)(void *obj, int startoff, int idx);
#define	OI(o)	((offset_fnc_t)(__psint_t)(o))

typedef int (*count_fnc_t)(void *obj, int startoff);
#define	CI(c)	((count_fnc_t)(__psint_t)(c))
#define	C1	CI(1)

typedef struct field
{
	char		*name;
	fldt_t		ftyp;
	offset_fnc_t	offset;
	count_fnc_t	count;
	int		flags;
	typnm_t		next;
} field_t;

/*
 * flag values
 */
#define	FLD_ABASE1	1	/* field array base is 1 not 0 */
#define	FLD_SKIPALL	2	/* skip this field in an all-fields print */
#define	FLD_ARRAY	4	/* this field is an array */
#define	FLD_OFFSET	8	/* offset value is a function pointer */
#define	FLD_COUNT	16	/* count value is a function pointer */

typedef int (*size_fnc_t)(void *obj, int startoff, int idx);
#define	SI(s)	((size_fnc_t)(__psint_t)(s))

typedef struct ftattr
{
	fldt_t		ftyp;
	char		*name;
	prfnc_t		prfunc;
	char		*fmtstr;
	size_fnc_t	size;
	int		arg;
	adfnc_t		adfunc;
	const field_t	*subfld;
} ftattr_t;
extern const ftattr_t	ftattrtab[];

/*
 * arg values
 */
#define	FTARG_SKIPZERO	1	/* skip 0 words */
#define	FTARG_DONULL	2	/* make -1 words be "null" */
#define	FTARG_SKIPNULL	4	/* skip -1 words */
#define	FTARG_SIGNED	8	/* field value is signed */
#define	FTARG_SIZE	16	/* size field is a function */
#define	FTARG_SKIPNMS	32	/* skip printing names this time */
#define	FTARG_OKEMPTY	64	/* ok if this (union type) is empty */

extern int		bitoffset(const field_t *f, void *obj, int startoff,
				  int idx);
extern int		fcount(const field_t *f, void *obj, int startoff);
extern const field_t	*findfield(char *name, const field_t *fields);
extern int		fsize(const field_t *f, void *obj, int startoff,
			      int idx);
