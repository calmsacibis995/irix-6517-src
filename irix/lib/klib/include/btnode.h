#ident "$Header: "

/*
 * Node header struct for use in binary search tree routines
 */
typedef struct btnode_s {
	struct btnode_s     *bt_left;
	struct btnode_s     *bt_right;
	struct btnode_s		*bt_parent;
	char                *bt_key;
	int                  bt_height;
} btnode_t;

#define DUPLICATES_OK   1

/**
 ** btnode operation function prototypes
 **/
int max(
	int			/* value 1 */, 
	int			/* value 2 */);

/* Allocate a btnode_s struct. Make a copy of the key passed in and
 * add it to the bt_key field. In the event of an error (which would
 * only be caused if a block of memory could not be allocated for 
 * the btnode_s struct), a NULL btnode_s pointer will be returned.
 */
btnode_t *alloc_btnode(
	char*		/* key */);

/* Free a btnode_s struct including the key string it contains.
 */
void free_btnode(
	btnode_t*	/* pointer to btnode_s struct to free */);

/* Return the hight of a given btnode_s struct in a tree. In the
 * event of an error (a NULL btnode_s pointer was passed in), a
 * value of -1 will be returned.
 */
int btnode_height(
	btnode_t*	/* pointer to btnode_s struct */);

/* Insert a btnode_s struct into a tree. After the insertion, the
 * tree will be left in a reasonibly ballanced state. Note that, if 
 * the DUPLICATES_OK flag is set, duplicate keys will be inserted 
 * into the tree (otherwise return an error). In the event of an
 * error, a value of -1 will be returned.
 */
int insert_btnode(
	btnode_t**	/* pointer to root of tree */, 
	btnode_t*	/* pointer to btnode_s struct to insert */, 
	int			/* flags (DUPLICATES_OK) */);

/* Finds a btnode in a tree and removes it, making sure to keep 
 * the tree in a reasonably balanced state. As part of the 
 * delete_btnode() operation, a call will be made to the free 
 * function (passed in as a parameter) to free any application 
 * specific data.
 */
int delete_btnode(
	btnode_t**	/* pointer to the root of the btree */, 
	btnode_t*	/* pointer to btnode_s struct to delete */, 
	void(*)()	/* pointer to function to actually free the node */, 
	int			/* flags */);

/* Traverse a tree looking for a particular key. In the event that
 * duplicate keys are allowed in the tree, returns the first occurance
 * of the search key found. A pointer to an int should be passed in
 * to hold the maximum depth reached in the search. Upon success,
 * returns a pointer to a btnode_s struct. Otherwise, a NULL pointer
 * will be returned.
 */
btnode_t *find_btnode(
	btnode_t*	/* pointer to btnode_s struct to start search with */, 
	char*		/* key we are looking for */, 
	int*		/* pointer to where max depth vlaue will be placed */);

btnode_t *first_btnode(
	btnode_t *	/* pointer to any btnode in a btree */);

btnode_t *next_btnode(
	btnode_t *	/* pointer to current btnode */);

