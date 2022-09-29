/* selects cache(s) for alter.  Note that
 * since the icache tests are separated, the BOTH option refers to
 * the primary data and (combined) secondary. */
#define PRIMARYD	0
#define PRIMARYI	1
#define SECONDARY	2
#define BOTH		3
#define DO_MEMORY	4

#define	PHYS_CHECK_LO	(PHYS_RAMBASE + 0x01400000)

#define	SR_FORCE_ON	(SR_CU0 | SR_CU1 | SR_CE | SR_IEC)
#define	SR_FORCE_OFF	(SR_CU0 | SR_CU1 | SR_IEC)
#define	SR_FORCE_NOERR	(SR_CU0 | SR_CU1 | SR_CE | SR_DE )
#define	SR_NOERR	(SR_CU0 | SR_CU1 | SR_DE )


#ifndef	LOCORE

#include <sys/types.h>

/* indicates which cache state to set.
 * Some are specific to primary or secondary.  Note that DIRTY_EXCL in
 * primary sets the writeback bit therefore acting like the secondary
 * does in that state.  The DIRTY_EXCL_NWB state applies only to the
 * primary and is the special case where a line is read from a dirty
 * secondary but not modified in the primary. */
enum c_states {
	INVALIDL,	/* P or S: cache line is invalid */
	CLEAN_EXCL,	/* P or S: clean exclusive */
	DIRTY_EXCL,	/* P or S: dirty exclusive */
	DIRTY_EXCL_NWB	/* P: Dirty Excl. w/ writeback bit clear */
};

enum mem_space {
	k0seg,		/* cached, unmapped access */
	k1seg,		/* uncached, unmapped access */
	k2seg		/* mapped; specified caching strategy */
};

/* get_tag rolls phys addrs from the taglo register to their correct
 * 31-bit positions before placing them in the tag_info.physaddr
 * field for easy comparision.  The below masks return the valid portion
 * of the address (depending upon which cache the tag is from).
 */
#define	PINFOADDRMASK	0xFFFFF000
#define	SINFOADDRMASK	0xFFFE0000
/* top bit if phys addrs in taglo is 35: we want it to be 31 so roll left 4 */
#define	TAGADDRLROLL	4

/* roll the 3 vindex (2ndary cache) bits from their taglo spot (9..7)
 * to their real positions (14..12) for easy comparision by get_tag_info */
#define	SVINDEXLROLL	5
#define	INFOVINDMASK	0x7000

/* define bit-rolls to get cache-state portion to bottom of tag_lo word.  */
#define	PSTATE_RROLL	6	/* tag_lo bits 7..6 indicate pcache state */
#define	SSTATE_RROLL	10	/* tag_lo bits 12..10 indicate scache state */

/* several primitives return MISSED_2NDARY when a cache instruction on
 * an address doesn't hit the secondary cache */
#define	MISSED_2NDARY	(-2)

/* sbd.h contains the masks for accessing the primary and secondary
 * fields in the TagLo register.  The below #defines manipulate
 * those values into positions in the tag_info struct and then mask
 * the valid parts of those fields for use by get_tag_info.
 *
 * The tag_info struct holds the state (in the low 2 or 3 bits depending
 * upon which cache is being queried), the 3 Vindex bits shifted to
 * their proper position for use (if the tag is from secondary cache),
 * and the upper bits of the physaddr, also shifted to their useable
 * positions.  If the tag is from a primary line the tag reported bits
 * 35..12, so physaddr contains bits 31..12 of the physical address.
 * If it is a secondary line, the tag contained bits 35..17, so physaddr
 * will contain bits 31..17.
 */
typedef struct tag_info {
	ushort state;
	ushort vindex;
	uint physaddr;
} tag_info_t;

/* tag_regs structs hold the contents of the TagHi and TagLo registers,
 * which cache tag instructions use to read and write the primary and
 * secondary caches.
 */
typedef struct tag_regs {
	unsigned int tag_lo;
	unsigned int tag_hi;
} tag_regs_t;


extern int _sidcache_size;
extern int _scache_linesize;
extern int _scache_linemask;
extern int _icache_size;
extern int _icache_linesize;
extern int _icache_linemask;
extern int _dcache_size;
extern int _dcache_linesize;
extern int _dcache_linemask;
int pd_HWBINV(uint *);

#define	PD_SIZE		_dcache_size
#define	PDL_SIZE	_dcache_linesize
#define	PI_SIZE		_icache_size
#define	PIL_SIZE	_icache_linesize
#define	SID_SIZE	_sidcache_size
#define	SIDL_SIZE	_scache_linesize

#endif	/* LOCORE */
