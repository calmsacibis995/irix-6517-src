#ident "$Header: "

/* List element header
 */
typedef struct element_s {
	struct element_s    *next;
	struct element_s    *prev;
} element_t;

/* Some useful macros
 */
#define ENQUEUE(list, elem) enqueue((element_t **)list, (element_t *)elem)
#define DEQUEUE(list) dequeue((element_t **)list)
#define FINDQUEUE(list, elem) findqueue((element_t **)list, (element_t *)elem)
#define REMQUEUE(list, elem) remqueue((element_t **)list, (element_t *)elem)

typedef struct list_of_ptrs {
	element_t			elem;
	unsigned long long 	val64;
} list_of_ptrs_t;

#define FINDLIST_QUEUE(list, elem, compare) \
	findlist_queue((list_of_ptrs_t **)list, (list_of_ptrs_t *)elem, compare)

/** 
 ** Function prototypes
 **/

/* Add a new element to the tail of a doubly linked list.
 */
void kl_enqueue(
	element_t**		/* pointer to head of list */, 
	element_t*		/* pointer to element to add to the list */);

/* Remove an element from the head of a doubly linked list. A pointer 
 * to the element will be returned. In the event that the list is 
 * empty, a NULL pointer will be returned.
 */
element_t *kl_dequeue(
	element_t**		/* pointer to head of list (first item will be removed) */);

/* Checks to see if a particular element is in a list. If it is, a 
 * value of one (1) will be returned. Otherwise, a value of zero (0) 
 * will be returned.
 */
int kl_findqueue(
	element_t**		/* pointer to head of list */, 
	element_t*		/* pointer to element to find on list */);

/* Walks through a list of pointers to queues and looks for a 
 * particular list.
 */
int findlist_queue(
	list_of_ptrs_t** /* pointer to list of lists */,  
	list_of_ptrs_t* /* pointer to list to look for */,
	int(*)()		/* pointer to compare function */);

/* Remove specified element from doubly linked list.
 */
void kl_remqueue(
	element_t**		/*pointer to head of list */, 
	element_t*		/* pointer to element to remove from list */);
