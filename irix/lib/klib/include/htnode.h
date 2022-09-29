#ident "$Header: "

/* Node structure for use in hierarchical trees (htrees). 
 */
typedef struct htnode_s {
	struct htnode_s	*next;
	struct htnode_s	*prev;
	struct htnode_s	*parent;
	struct htnode_s	*children;
	int				 seq;
	int				 level;
	int				 key;
} htnode_t;

/** Flag values
 **/
#define HT_BEFORE	0x1
#define HT_AFTER	0x2
#define HT_CHILD	0x4
#define HT_PEER		0x8

/** Function prototypes
 **/
htnode_t *next_htnode(
	htnode_t *		/* htnode pointer */);

htnode_t *prev_htnode(
	htnode_t *		/* htnode pointer */);

void ht_insert_peer(
	htnode_t *		/* htnode pointer */, 
	htnode_t *		/* new htnode pointer*/, 
	int 			/* flags */);

void ht_insert_child(
	htnode_t *		/* htnode pointer */, 
	htnode_t *		/* new htnode pointer*/, 
	int 			/* flags */);

int ht_insert(
	htnode_t *		/* htnode pointer */, 
	htnode_t *		/* new htnode pointer*/, 
	int 			/* flags */);

void ht_insert_next_htnode(
	htnode_t *      /* htnode pointer */,
	htnode_t *      /* new htnode pointer*/);
