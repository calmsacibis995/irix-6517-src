#include "rcspriv.h"
#include <stdlib.h>

/* Memory management - support multiple heaps.
 *
 * On server side, heaps are used to maintain opts values, which are
 * per-command.  So, each command has its own heap, with which it may
 * maintain the user's last set of "defaults".
 *
 * On client side, memory may be needed to return some values.  This
 * memory is freed as soon as the next API is called.  Thus, a single
 * heap suffices for return values.
 */

	void *
rcsHeapAlloc(heap, size)
	struct rcsHeap *heap;	/* existing heap */
	size_t size;		/* size of next block to allocate */
{
	/* Allocate a block of memory, and add it to a list of blocks
	 * belonging to the heap.  This allows the entire heap to
	 * be deallocated (freed) at once.
	 *
	 * The heap list is never freed, since it can be reused.
	 *
	 * The heap itself is a linked list of pointers to blocks of memory.
	 */

	/*
	  ----------------	-----------------
	 | block|free|next|     | block|free|next|
heap---->|   |  |  | |   \----->|   |  |    | \0 |
	 ----v-----v------      ----v------------
	     |     |              memory      ^        
	     v     |                          |
	  memory   \--------------------------/


	A heap with two blocks of memory allocated, and no more list entries
	available to keep track of next block of memory.

	 */


	struct rcsHeap **heapfree;	/* ptr to ptr to heap block to use for
					 * next alloc of memory
					 */
	struct rcsHeap *hp;		/* ptr to heap block to use */


	/* If no blocks in heap, use head of heap to keep track of block.
	 * Otherwise, use heap->free (which may point to a NULL next ptr).
	 */
	if (!heap->block) {
	    heapfree = &heap;
	}
	else heapfree = heap->free;

	/* If there is no heap list entry already available, malloc one */
	if (!(hp = *heapfree)) {
	    if (!(hp = *heapfree =  malloc(sizeof(struct rcsHeap))))
		return NULL;
	    hp->next = NULL;
	}

	/* Malloc the requested space */
	if (!(hp->block = malloc(size))) return NULL;

	/* The next free list entry is now what "next" points to */
	heap->free = &(hp->next);

	return hp->block;
}

	void
rcsHeapFree(heap)
	struct rcsHeap *heap;
{
	/* just free the blocks associated with heap; don't free list
	 * entries themselves.
	 */
	register struct rcsHeap *hp = heap;
	for(;hp && hp->block; hp = hp->next) {
	    free(hp->block);
	    hp->block = NULL;
	}
}
	
#ifdef TESTMAIN
main()
{
	struct rcsHeap heap;
	int i;
	char *ptr;

	heap.block = NULL;	/* initialize */

    	for(i = 1; i < 10; i++) {
	    ptr = rcsHeapAlloc(&heap, i);
	    printf("got %#x\n", ptr);
	}

	rcsHeapFree(&heap);

	for(i = 10; i < 1000; i *= 3) {
	    ptr = rcsHeapAlloc(&heap, i);
	    printf("got %#x\n", ptr);
	}

	rcsHeapFree(&heap);

	for(i = 1; i < 11; i++) {
	    ptr = rcsHeapAlloc(&heap, i);
	    printf("got %#x\n", ptr);
	}
}
#endif /* TESTMAIN */
