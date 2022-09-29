/*
 * dest.h
 *	Declarations and prototypes about FFSC destinations
 */

#ifndef _DEST_H_
#define _DEST_H_


/* The standard default destination */
#ifdef PRODUCTION
#define DEST_DEFAULT	"m *"		/* All modules */
#else
#define DEST_DEFAULT	"r * b *"	/* All racks/bays */
#endif  /* !PRODUCTION */


/* Bitmaps describing selected racks and bays. The #define's associated */
/* with these fields must be kept synchronized with BAYID_* & RACKID_*.	*/
typedef unsigned char	baymap_t;	/* Selected bays within a rack */
#define BAYMAP_ALL	0x80		/* Bay "ALL" was specified */

typedef unsigned int	rackmap_t;	/* Selected racks in a system */


/* This describes an MMSC command destination */
#define DEST_STRLEN	80		/* Max length of dest string */
typedef struct dest {
	unsigned int	Flags;			/* Information flags */
	baymap_t	Map[MAX_RACKS];		/* Map of selected rack+bays */
	char		String[DEST_STRLEN];	/* Human-readable form */
} dest_t;

#define DESTF_NONE	0x00000001	/* No destination specified */
#define DESTF_DEFAULT	0x00000002	/* This is the default destination */


/* Public functions */
#define destAllBaysSelected(D,R)    ((D)->Map[R] & BAYMAP_ALL)
#define destBayIsSelected(D,R,B)    (((D)->Map[R]) & (1 << B))
#define destLocalBayIsSelected(D,B) destBayIsSelected((D),ffscLocal.rackid,(B))
#define destRackIsSelected(D,R)	    ((D)->Map[R] != 0)

void destClear(dest_t *);
int  destNumBays(const dest_t *, rackid_t *, bayid_t *);
int  destNumRacks(const dest_t *, rackid_t *);
const char *destParse(const char *, dest_t *, int);

#endif  /* _DEST_H_ */
