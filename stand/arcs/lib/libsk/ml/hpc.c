#if IP20 || IP22 || IP26 || IP28			/* whole file */
/*
 * hpc.c - Support for IP20 HPC 1.5 IP22/IP24/IP26/IP28 HPC 3.0
 *
 * $Revision: 1.21 $
 */
#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <libsk.h>

/* Imports */
extern int needs_dma_swap;

/* Exports */
extern int little_endian;

/*
 * Set the DMA hardware for the correct endian
 */
void
hpc_set_endianness(void)
{
    volatile unsigned int testaddr;
    char *cp = (char *)&testaddr;

    testaddr = 0x11223344;
    if (*cp == 0x44)
	little_endian = 1;

#if IP20						/* HPC 1.X */
    /* Probe for the endian configuration register on the HPC
     *
     * Note: wbadaddr works here but badaddr does not.  wbadaddr
     * has the side effect of clearing the register.
     */
    if (wbadaddr ((void *)PHYS_TO_K1(HPC_ENDIAN), 1)) {
	/* old HPC1 */
	if (little_endian)
	    needs_dma_swap = 1;
    } else {
	/* new HPC1.5 */
	if (little_endian)
	    *(volatile char *)PHYS_TO_K1(HPC_ENDIAN) |= HPC_ALL_LITTLE;
	else
	    *(volatile char *)PHYS_TO_K1(HPC_ENDIAN) &= ~HPC_ALL_LITTLE;
    }
#endif
#if IP22 || IP26
    /* XXX -- set HPC 3 endian flags */
#endif
}

#ifdef	_NT_PROM
#include <arcs/hinv.h>

/*----------------------------HPC Configuration table---------------------*/
static COMPONENT hpctmpl = {
	AdapterClass,			/* Class */
	MultiFunctionAdapter,		/* Type */
	0,				/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	7,				/* IdentifierLength */
#if IP20
	"HPC1.5"			/* Identifier */
#endif	/* IP20 */
#if IP22 || IP26 || IP28
	"HPC3.0"			/* Identifier */
#endif
};

void
hpc_install(COMPONENT *root)
{
	COMPONENT *tmp;

	root = AddChild(root,&hpctmpl,(void *)NULL);
	if (root == (COMPONENT *)NULL) cpu_acpanic("HPC");

	return;
}
#endif	/* _NT_PROM */
#endif /* IPXX */
