#ident	"lib/libsk/lib/mem.c:  $Revision: 1.17 $"

/*
 * memory.c - list manipulation for arcs GetMemoryDescriptor function
 */

/*
 * N.B. These functions maintain physical memory addresses.
 *	All page counts are for 4K pages.
 */


#include <sys/types.h>
#include <arcs/spb.h>
#include <arcs/hinv.h>
#include <arcs/spb.h>
#include <sys/param.h>
#include <sys/sbd.h>
#include <saio.h>
#include <libsc.h>
#include <libsk.h>

#define MEMMAGIC	0x4b45565e

/*#define	DEBUG*/
#ifdef DEBUG
void mem_list_dump(char *string);
#endif

/* singly linked list structure for descriptors
 */
typedef struct dl {
    MEMORYDESCRIPTOR d;
    int magic;
    struct dl *n;
} dl_t;

static dl_t *mem_root;

MEMORYDESCRIPTOR *
GetMemoryDescriptor (MEMORYDESCRIPTOR *Current)
{
    dl_t *dl = (dl_t *)Current;

#if LIBSL
    static md_init_called = 0;
    if(!md_init_called) {
	md_init();
	md_init_called = 1;
    }
#endif

    if (Current == NULL)
	return &mem_root->d;

    /* check magic number 
     */
    if (dl->magic != MEMMAGIC)
	return NULL;

    return &(dl->n->d);
}

/*
 * mem_init - initialize memory descriptor list
 */
void
md_init(void)
{
	unsigned i;
	dl_t *p;
	MEMORYDESCRIPTOR *d;
#ifndef LIBSL
	extern int _prom;
	if(_prom) {
		mem_root = 0;

		/* Create the first descriptor node consisting of all of memory
		*/
		cpu_mem_init ();

		/* An arcs system should always have the exception vectors
		* at 0x0 and the spb at 0x1000.
		*/
#ifdef SN0
		md_alloc ((unsigned long) TO_NODE(get_nasid(), SPBADDR), (unsigned long)1, SPBPage);
		md_alloc ((unsigned long) TO_NODE(get_nasid(), KDM_TO_PHYS(K0BASE)), (unsigned long)1, ExceptionBlock);
#else
		md_alloc ((unsigned long)SPBADDR, (unsigned long)1, SPBPage);
		md_alloc ((unsigned long)KDM_TO_PHYS(K0BASE), (unsigned long)1, ExceptionBlock);
#endif /* SN0 */
	}
	else {
#endif
		extern MEMORYDESCRIPTOR *arcsGetMemDesc(MEMORYDESCRIPTOR *);
		/* count number of memory descriptors kept by prom */
		for(i = 0, d = 0; d = arcsGetMemDesc(d); i++) {
		}

		/* malloc guaranteed to succeed this early in the
		 * execution of a standalone program.
		 */
		p = (dl_t *)malloc(i * sizeof(dl_t));
		mem_root = p;

		for(d = 0; d = arcsGetMemDesc(d); p++) {
#if _K64PROM32
			if(_isK64PROM32()) {
				typedef struct memorydescriptor32 {
					MEMORYTYPE	Type;
					__uint32_t	BasePage;
					__uint32_t	PageCount;
				} *m32_t;
				p->d.Type = ((m32_t)d)->Type;
				p->d.BasePage = ((m32_t)d)->BasePage;
				p->d.PageCount = ((m32_t)d)->PageCount;
			}
			else
#endif
				p->d = *d;
			p->magic = MEMMAGIC;
			if(--i == 0) {
				p->n = 0;
			}
			else {
				p->n = p + 1;
			}
		}
#ifndef LIBSL
	}
#endif
}

/*
 * md_alloc - allocate a chunk of memory from the memory
 * 	pool of a specified type
 * 
 * returns the new memory descriptor if successful, NULL if unsuccessful
 */
MEMORYDESCRIPTOR *
md_alloc(unsigned long base, unsigned long pages, MEMORYTYPE type)
{
    dl_t *dl;
    MEMORYDESCRIPTOR *m;

    /* Search the list for this base address
     */
    dl = mem_root;
    while (dl) {
#ifdef	DEBUG
	printf ("base=0x%x, BasePage=0x%x, BasePage+PageCount=0x%x, type=%d\n",
		base, arcs_ptob(dl->d.BasePage),
		arcs_ptob(dl->d.BasePage+dl->d.PageCount), dl->d.Type);
#endif	/*DEBUG*/
	if ((base >= arcs_ptob(dl->d.BasePage)) &&
		(base < arcs_ptob(dl->d.BasePage+dl->d.PageCount)))
	    break;
	dl = dl->n;
    }

    if (!dl) {
	printf ("0x%x does not fall within a valid memory descriptor\n", base);
	return NULL;
    }

    if (base == arcs_ptob(dl->d.BasePage)) {
	/* add part after allocated chunk
	 */
	md_add (base+arcs_ptob(pages), dl->d.PageCount-pages, dl->d.Type);

	dl->d.PageCount = pages;
	dl->d.Type = type;

	return &dl->d;
    } else {
	/* add part after allocated chunk
	 */
	md_add (base+arcs_ptob(pages), 
	    dl->d.PageCount-(pages+arcs_btop(base-arcs_ptob(dl->d.BasePage))),
	    dl->d.Type);

	/* add allocated chunk
	 */
	m = md_add (base, pages, type);

	dl->d.PageCount = arcs_btop(base - arcs_ptob(dl->d.BasePage));

	return m;
    }
}

/*
 * md_dealloc - deallocate a memory descriptor
 */
/*ARGSUSED*/
LONG
md_dealloc(unsigned long base, unsigned long pages)
{
    dl_t *d1, *d2;

    /* Search the list for this base address
     */
    d1 = mem_root;
    while (d1) {
	if ((arcs_ptob(base) >= arcs_ptob(d1->d.BasePage)) &&
		(arcs_ptob(base) < arcs_ptob(d1->d.BasePage+d1->d.PageCount)))
	    break;
	d1 = d1->n;
    }
    if (!d1) {
	printf ("0x%x does not fall within a valid memory descriptor\n", 
		arcs_ptob(base));
	return 1;
    }

    d1->d.Type = FreeMemory;
    d1 = mem_root;

    while (d1) {
	d2 = d1->n;
	/* Join adjacent Free memory descriptors */
	while (d2 && (d1->d.Type == FreeMemory) && 
		(d1->d.Type == d2->d.Type))  {
	    d1->d.PageCount += d2->d.PageCount;
	    d1->n = d2->n;
	    free (d2);
	    d2 = d1->n;
	}
	d1 = d2;
    }
    return NULL;
}

#if 0		/* not currently used */
/* find next free block of memory
 * protocol is:
 * opaqueptr = mem_free_chunk((void *)0, &ptr, &count)
 * 	the first time, first param is zero.  That means start looking
 *	at the beginning of memory for a first free chunk.  Return the
 *	ptr and #bytes and chunk handle if a free chunk is found.
 *
 * subsequently, call
 * opaqueptr = mem_free_chunk(opaqueptr, &ptr, &count);
 *	and if opaqueptr is non-null, then ptr and count are set to
 *	the beginning and size of the next free chunk of memory.
 *	but if opaqueptr is returned 0 == NULL, then end of free list has
 *	been hit.
 */
void *
mem_free_chunk(void *p, __psint_t *pbase, unsigned int *pbytes)
{
	MEMORYDESCRIPTOR *dp = (MEMORYDESCRIPTOR *)p;
	/* first, get the pointer to the next free block after 'p' */
	dp = GetMemoryDescriptor(dp);
	/* now look for the next free block */
	while (dp && dp->Type != FreeMemory) {
		dp = GetMemoryDescriptor(dp);
	}
	if(dp) {
		*pbytes = arcs_ptob(dp->PageCount);
		*pbase = PHYS_TO_K1(arcs_ptob(dp->BasePage));
	}
	return (void *)dp;
}

/* find biggest free block of memory */
void
mem_biggest_free(__psint_t *pbase, unsigned int *pbytes)
{
	unsigned int bytes = 0;
	__psint_t base;
	void *d;

	*pbytes = 0;
	d = 0;
	while (d = mem_free_chunk(d, &base, &bytes)) {
		if(bytes > *pbytes) {
			*pbytes = bytes;
			*pbase = base;
		}
	}
}
#endif /* 0 */

/*
 * md_add - add a chunk of memory to the descriptor list
 * 
 * returns the new memory descriptor if successful, NULL if unsuccessful
 */
MEMORYDESCRIPTOR *
md_add(unsigned long base, unsigned long pages, MEMORYTYPE type)
{
    dl_t *dl, *dp;
    
    if (pages == 0)
	return NULL;

    if (mem_root == 0) {
	mem_root = (dl_t *)malloc(sizeof(dl_t));
	if (mem_root == 0)
	    return NULL;
	dl = mem_root;
	dl->n = NULL;
    } else {

	dl = mem_root;
	dp = dl->n;
    	if ((arcs_btop(base)) < dl->d.BasePage) { /* insert at beginning */
	    dp = (dl_t *)malloc(sizeof(dl_t));
	    if (dp == 0)
	    	return NULL;
	    dp->n = dl; 
	    dl = dp;
	    mem_root = dp;
	} else {				/* find spot in list */
	    while (dl->n) {
	    	if ((arcs_btop(base) > dl->d.BasePage) &&
		    (arcs_btop(base) < dp->d.BasePage))
		    break;
	    	dl = dp;
	    	dp = dp->n;
	    }

	    if (!dp) {		/* insert at end of list */
	    	dl->n = (dl_t *)malloc(sizeof(dl_t));
	    	if (dl->n == 0)
	    	    return NULL;
    	    	dl = dl->n;
    	    	dl->n = NULL;
	    } else {		/* insert in middle of list */
	    	dl->n = (dl_t *)malloc(sizeof(dl_t));
	    	if (dl->n == 0)
	    	    return NULL;
    	    	dl = dl->n;
	    	dl->n = dp;	    
	    }
    	}
    }

    dl->magic = MEMMAGIC;
    dl->d.Type = type;
    dl->d.BasePage = arcs_btop(base);
    dl->d.PageCount = pages;
    return &dl->d;
}

#ifdef	DEBUG
void
mem_list_dump(char *string)
{
    dl_t *dl;

    printf ("List dump (%s):\n", string);
    dl = mem_root;

    if (dl == 0) {
	printf ("\tNo elements in list\n");
	return;
    }

    while (dl) {
	printf ("0x%x, 0x%x, 0x%x\n", dl->d.BasePage, dl->d.PageCount, dl->d.Type);
	dl = dl->n;
    }

}
#endif	/* DEBUG */
#undef DEBUG
