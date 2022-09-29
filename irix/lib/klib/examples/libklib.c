#ident  "$Header: /proj/irix6.5.7m/isms/irix/lib/klib/examples/RCS/libklib.c,v 1.1 1999/02/23 20:38:33 tjm Exp $"

#include <stdio.h>
#include <sys/fcntl.h>
#include <klib.h>

/*
 * main()
 */
main(int argc, char **argv)
{
	k_ptr_t *p;

	kl_setup_error(stderr, "klib", NULL);	

	p = kl_alloc_block(128, K_TEMP);
	return(0);
}
