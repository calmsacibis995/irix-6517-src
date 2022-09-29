#ident  "$Header: /proj/irix6.5.7m/isms/irix/lib/klib/examples/RCS/klib.c,v 1.1 1999/02/23 20:38:33 tjm Exp $"

#include <stdio.h>
#include <sys/fcntl.h>
#include <klib/klib.h>

/* Global variables
 */
char namelist[256];
char corefile[256];
char *program;
FILE *ofp = stdout;
int klp_hndl;               /* handle of the current klib_s struct */

/*
 * main()
 */
main(int argc, char **argv)
{
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

    /* Initialize the klib_s struct that will provide a single
	 * point of contact for various bits of KLIB data.
	 */
	if ((klp_hndl = init_klib()) == -1) {
		fprintf(stderr, "Could not initialize klib_s struct!\n");
		exit(1);
	}

	/* Set any KLIB global behavior flags (e.g., K_IGNORE_FLAG).
	 */
	set_klib_flags(0);

    /* Setup info necessary for reading the vmcore image (vmcore file
	 * or live system memory).
	 */
	if (kl_setup_meminfo(namelist, corefile, O_RDWR)) {
		fprintf(stderr, "Could not setup coreinfo!\n");
		exit(1);
	}

    /* Setup info necessary for reading from the kernel symbol table.
	 */
    if (kl_setup_symtab(namelist)) {
        fprintf(KL_ERRORFP, "\nCould not initialize symlib!\n");
        exit(1);
    }

    /* Initialize the libkern library. Specifically, gather up information
	 * about the system, its configuration, state of the kernel, etc.
	 */
    if (libkern_init()) {
        fprintf(KL_ERRORFP, "\nCould not initialize libkern!\n");
        return(1);
    }

    doit();
	exit(0);
}

/*
 * doit()
 */
int
doit()
{
	int i, cpuid, PDAINDR_S_SIZE;
	kaddr_t pdaindr, pda;
	k_ptr_t pdap;

	/* Sizes of many commonly referenced kernel structures are captured 
	 * during KLIB initialization. The pdaindr_s structure, however, is 
	 * not one of the ones captured. Consequently, we have to determine its 
	 * size here. It's best to capture the size in a local variable if it 
	 * is going to be used more than once. That is because quite a bit of 
	 * work (accessing Dwarf data in the kernel symbol table) is necessary
	 * to determine the size.
	 */
	PDAINDR_S_SIZE = kl_struct_len("pdaindr_s");

	/* Allocate a block of memory large enough to hold a pdaindr_s struct.
	 */
	pdap = kl_alloc_block(PDAINDR_S_SIZE, K_TEMP);

	/* Cycle through the elements of the pdaindr array; read in the 
	 * pdaindr_s struct; capture the cpu ID plus the pointer to the 
	 * pda_s struct and print the values out. Note that the KLIB macro 
	 * K_MAXCPUS references the value of the kernel variable maxcpus, 
	 * which was captured at KLIB initialization.
	 */
	for (i = 0; i < K_MAXCPUS; i++) {

		/* Get the pointer to the location in kernel memory where the
		 * pdaindr_s pointer is stored. We use the base address of the
		 * pdaindr array which was captured during KLIB initialization,
		 * plus the offset to the current element (index * the size of
		 * the pdaindr_s struct).
		 */
		pdaindr = K_PDAINDR + (i * PDAINDR_S_SIZE);

		/* Read in the pdaindr_s struct from kernel memory into the
		 * buffer we just allocated.
		 */
		kl_get_struct(pdaindr, PDAINDR_S_SIZE, pdap, "pdaindr");

		/* Check to see if there were any errors
		 */
		if (KL_ERROR) {
			/* If there was an error, free the block of memory and
			 * return.
			 */
			kl_free_block(pdap);
			return(1);
		}

		/* Get the cpu ID from the pdaindr_s struct.
		 */
		cpuid = kl_int(pdap, "pdaindr_s", "CpuId", 0);

		/* Get the address of the pda_s struct from the pdaindr_s struct.
		 */
		pda = kl_kaddr(pdap, "pdaindr_s", "pda");

		/* Print out the values we just obtained.
		 */
		printf("cpu %d,  pda=0x%llx\n", cpuid, pda);
	}

	/* Free the block of memory we previously allocated.
	 */
	kl_free_block(pdap);

	return(0);
}
