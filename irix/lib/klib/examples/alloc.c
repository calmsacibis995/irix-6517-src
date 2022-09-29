#ident  "$Header: /proj/irix6.5.7m/isms/irix/lib/klib/examples/RCS/alloc.c,v 1.1 1999/02/23 20:38:33 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>

#define ALLOC_DEBUG
#include <klib/alloc.h>

/*
 * main()
 */
void
main(int argc, char **argv)
{
	void *p1, *p2, *p3, *p4;
	void *t1, *t2, *t3, *t4;
	void *s1;

    /* Initialize the alloc memory allocator. 
	 */
	init_mempool(0, 0, 0);

	/* allocate some permanent blocks
	 */
	p1 = alloc_block(8, B_PERM);
	p2 = alloc_block(32, B_PERM);
	p3 = alloc_block(65, B_PERM);
	p4 = alloc_block(129, B_PERM);

	/* allocate some temporary blocks
	 */
	t1 = alloc_block(8, B_TEMP);
	t2 = alloc_block(32, B_TEMP);
	t3 = alloc_block(65, B_TEMP);
	t4 = alloc_block(129, B_TEMP);

	free_block(t1);
	t1 = realloc_block(t2, 30, B_TEMP);
	t2 = dup_block(t2, B_TEMP);

	s1 = str_to_block("this is a string", B_TEMP);
	printf("%s\n", s1);

	/* Free all temporary blocks
	 */
	free_temp_blocks();

	fprintf(stdout, "end!\n");
}
