#ident  "$Header: /proj/irix6.5.7m/isms/irix/lib/klib/examples/RCS/mem.c,v 1.1 1999/02/23 20:38:33 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>

char namelist[256];
char corefile[256];
char *program;

int klp_hndl;

/*
 * main()
 */
main(int argc, char **argv)
{
	kaddr_t taddr;
	kaddr_t hexval;

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
    sprintf(corefile, "/dev/mem");
    sprintf(namelist, "/unix");

    /* Get the name of the program...
     */
    program = argv[0];

	/* Get the name of the namelist and corefile...
 	 */
	if (optind < argc) {
		if ((argc - optind) > 1) {
			strcpy(namelist, argv[optind]);
			strcpy(corefile, argv[optind+1]);
		}
		else {
			fprintf(stderr, "Need to specify both namelist and corefile!\n");
			exit(1);
		}
	}

	kl_setup_error(stderr, program, 0);

    /* Initialize the klib_s struct that will provide a single
     * point of contact for various bits of KLIB data.
     */
    if ((klp_hndl = init_klib()) == -1) {
        fprintf(stderr, "Could not initialize klib_s struct!\n");
        return(1);
    }

	if (kl_setup_meminfo(namelist, corefile, 0) || KL_ERROR) {
		return(1);
	}

	/* Address has to be PHYSICAL address unless the address is mapped.
	 */
	taddr = 0x387548;
	kl_readmem(taddr, 8, &hexval);

	fprintf(stdout, "hexval: 0x%llx!\n", hexval);
}
