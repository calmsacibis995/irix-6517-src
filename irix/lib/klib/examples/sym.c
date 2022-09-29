#ident  "$Header: /proj/irix6.5.7m/isms/irix/lib/klib/examples/RCS/sym.c,v 1.1 1999/02/23 20:38:33 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#define LIBKERN_STUBS
#include <klib/kl_stubs.h>
#undef LIBKERN_STUBS

char namelist[256];
char *program;

int klp_hndl;                   /* handle of the current klib_s struct */

/*
 * main()
 */
main(int argc, char **argv)
{
	meminfo_t *mip;

    /* getopt() variables.
     */
    extern char *optarg;
    extern int optind;
    extern int opterr;

    /* Set opterr to zero to suppress getopt() error messages
     */
    opterr = 0;

    /* Initialize a few default variables.
     */
    sprintf(namelist, "/unix");

    /* Get the name of the program...
     */
    program = argv[0];

	/* Get the name of the namelist and corefile...
 	 */
	if (optind < argc) {
		strcpy(namelist, argv[optind]);
	}

	kl_setup_error(stderr, program, 0);

    /* Initialize the klib memory allocator. Note that if
	 * kl_init_mempool() is not called, malloc() and free()
	 * will be used when allocating or freeing blocks of
	 * memory. Or, another set of functions can be registered
	 * instead.
	 */
	kl_init_mempool(0, 0, 0);

	/* Set up the error handler right away, just in case there is
	 * a KLIB error during startup.
	 */
	kl_setup_error(stderr, program, NULL);

	if ((klp_hndl = init_klib()) == -1) {
		fprintf(stderr, "Could not initialize klib_s struct!\n");
		return(1);
	}

	if (kl_setup_symtab(namelist)) {
		fprintf(KL_ERRORFP, "\nCould not initialize symlib!\n");
		exit(1);
	}
	kl_free_klib(KLP);
	fprintf(stdout, "end!\n");
}


