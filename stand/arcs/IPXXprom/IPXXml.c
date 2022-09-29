#ident	"$Revision: 1.10 $"

#include <sys/types.h>
#include <sys/z8530.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/immu.h>
#include <libsc.h>
#include <libsk.h>
#include <sys/types.h>
#include <sys/param.h>
#include <arcs/folder.h>
#include <arcs/errno.h>
#include <arcs/hinv.h>
#include <arcs/io.h>
#include <arcs/cfgtree.h>

/* Exports */
void		cpu_install(COMPONENT *);
unsigned int	cpu_get_memsize(void);
void		cpu_acpanic(char *str);
int		cpuid(void);
int		ip12_board_rev();

/* Private */
static unsigned	cpu_get_membase(void);

#define	SGI_DISPLAY	0
#define	ASCII_DISPLAY	1

/* If we still must support the HPC1.0, define _OLDHPC so 
 * byte swapping will be done for dma.
 */
#define _OLDHPC
int needs_dma_swap;

/*
 * IP12 board registers saved during exceptions
 *
 * These are referenced by the assembly language fault handler
 *
 */
char	_parerr_save,
	_liointr0_save,
	_liointr1_save;
int	_cpu_parerr_save,
	_dma_parerr_save;

void cpu_reset() {}
void ip12_set_sickled(value) {}
void ip12_set_cpuled(int color) {}
unsigned char ip12_duart_rr0(int channel) {}

int is_hp1 () { return(1); }
ip12_board_rev () { return(0); }
int cpu_fpu_present(void) { return 1; }

#define SYSBOARDIDLEN	9
#define SYSBOARDID	"SGI-IPXX"

static COMPONENT IPXXtmpl = {
	SystemClass,		/* Class */
	ARC,			/* Type */
	0,			/* Flags */
	SGI_ARCS_VERS,		/* Version */
	SGI_ARCS_REV,		/* Revision */
	0,			/* Key */
	0,			/* Affinity */
	0,			/* ConfigurationDataSize */
	SYSBOARDIDLEN,		/* IdentifierLength */
	SYSBOARDID		/* Identifier */
};
	
cfgnode_t *
cpu_makecfgroot(void)
{
	COMPONENT *root;

	root = AddChild(NULL,&IPXXtmpl,(void *)NULL);
	if (root == (COMPONENT *)NULL) cpu_acpanic("root");
	return((cfgnode_t *)root);
}

static COMPONENT cputmpl = {
	ProcessorClass,			/* Class */
	CPU,				/* Type */
	0,				/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	11,				/* IdentifierLength */
	"MIPS-RX000"			/* Identifier */
};
static COMPONENT memtmpl = {
	MemoryClass,			/* Class */
	Memory,				/* Type */
	0,				/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	0,				/* Identifier Length */
	NULL				/* Identifier */
};

void
cpu_install(COMPONENT *root)
{
	extern unsigned _icache_size, _dcache_size;
	COMPONENT *c, *tmp;
	union key_u key;
	unsigned int reg;

	c = AddChild(root,&cputmpl,(void *)NULL);
	if (c == (COMPONENT *)NULL) cpu_acpanic("cpu");
	c->Key = cpuid();
}

void cpu_hardreset(void) {}
void cpu_show_fault(unsigned long saved_cause) {}
void cpu_soft_powerdown(void) {}
int cpuid(void) { return 0; }

unsigned int memsize;

unsigned int
cpu_get_memsize(void)
{
	return(0x8000000);
}

static unsigned int
cpu_get_membase(void)
{
#undef PHYS_RAMBASE
#define PHYS_RAMBASE 0
    return (unsigned int)PHYS_RAMBASE;
}

char *
cpu_get_disp_str(void)
{
	return("video()");
}
char *
cpu_get_kbd_str(void)
{
	return ("keyboard()");
}
char *
cpu_get_serial(void)
{
	return("serial(0)");
}

void
cpu_errputc(char c)
{
    void tty_errputc(char);
    tty_errputc(c);
}

void
cpu_mem_init(void)
{
    /* add all of main memory with the exception of two pages that
     * will be also mapped to physical 0
     */
#define NPHYS0_PAGES	2
    md_add (cpu_get_membase()+arcs_ptob(NPHYS0_PAGES),
	    arcs_btop(cpu_get_memsize()) - NPHYS0_PAGES, FreeMemory);
    md_add (0, NPHYS0_PAGES, FreeMemory);
}

void
cpu_acpanic(char *str)
{
	printf("Cannot malloc %s component of config tree\n", str);
	panic("Incomplete config tree -- cannot continue.\n");
}

/* IP12 cannot save configuration to nvram */
LONG
cpu_savecfg(void)
{
	return(ENOSPC);
}

char *
cpu_get_mouse(void)
{
	return("pointer()");
}

/*ARGSUSED*/
void
cpu_nv_lock(int on)
{
	return;
}

void
cpu_scandevs() {}
