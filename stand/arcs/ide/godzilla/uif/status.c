#ident "stand/arcs/ide/godzilla/uif/status.c: $Revision 1.1"
/*
 * IDE universal status messages and notifier functions.
 */

#ident "stand/arcs/ide/godzilla/uif/status.c: $Revision: 1.23 $"

#include <saioctl.h>
#include <sys/cpu.h>
#include <libsk.h>
#include <libsc.h>
#include <uif.h>
#include <sys/nic.h>
#include "sys/mgrashw.h"
#include "sys/mgras.h"
#define DEFINE_ERROR_DATA	1 /* must remain before include/d_godzilla.h> */
#include <godzilla/include/d_godzilla.h> 
#include <godzilla/include/d_bridge.h> 
#include <sys/mips_addrspace.h>
#define DEFINE_FRU_DATA	1	/* must remain before include/d_frus.h> */
#include <godzilla/include/d_frus.h> 

#ifndef PROM

/*
 * Following code is not needed by libpide.a so its under
 * !PROM flag to avoid linking problems and unecssary code in prom
 */

#define USAGE		"Usage: led n (0=off, 1=green, 2=amber)\n"
#define MAXVALUE	2
#define SETLED(X)	ip30_set_cpuled(X)
#define	FORMAT_DUMP	0	

/*
 * Command to turn LED various colors (on/off on IP20).
 */
int
setled(int argc,char *argv[])
{
	extern int failflag;
	int value = -1;
	char *usage = USAGE;

	if ( argc != 2 ) {
		printf(usage);
		return (1);
	}

	atob(argv[1],&value);
	if (value < 0 || value > MAXVALUE) {
		printf(usage);
		return (1);
	}

        if (value <= 1) failflag = 0;

	SETLED(value);

	return (0);
}


void
gfx_exit(void)
{
	if (console_is_gfx()) {
		/*
		 *  Now we need to reintialize the graphics console, but first
		 *  we need do some voodoo to convince the ioctl that graphics
		 *  really need initializing.
		 */
		ioctl(1, TIOCREOPEN, 0);
    	}
	buffoff();
}


/*
 *  emfail - "electronics module failure"
 *
 *  Report diagnostic-detected failure in terms of the appropriate field
 *  replaceable unit. Exit from IDE restoring the state of the console
 *  if necessary.
 */
int
emfail(int argc, char *argv[])
{
	char	*fru_name;
	int	fru_num;
	int	slot = -1;

	if (argc == 2) {
		atob(argv[1], &fru_num);
	}
	else if (argc == 3) {
		atob(argv[1], &fru_num);
		atob(argv[2], &slot);
	}
	else {
		msg_printf(SUM, "usage: emfail fru_number [slot]\n");
		return (1);
	}

	if ((fru_num >= 0) && (fru_num < GODZILLA_FRUS) ) {
		printf("Failure detected on the %s", 
			godzilla_fru_names[fru_num]);
		if (slot != -1)
			printf(" in XIO slot %d",slot);
		printf("\n");
		print_nic_info(fru_num,slot);
		return (0);
	}
	else {	printf("UNKNOWN FRU!\n");
        	return (1);
	}

}

/* 
 *	prints the NIC info (similar to stand/arcs/IP30prom/IP30.c)
 *	NOTE: does not call msg_printf
 */
static void
ide_dump_nic_info(char *buf, int dumpflag)
{
	int n = 0, display = 0;

	puts("Part number info:");

	/* unformatted dump */
	if (dumpflag) {
		printf("%s\n",buf);
		return;
	}
	/* formatted dump */
	while (*buf != '\0') {
		switch (*buf) {
		case ':':
			if (display==1) printf(": ");
			buf++;
			while (*buf != '\0' && *buf != ';') {
				if (display==1) putchar(*buf);
				buf++;
			}
			if (*buf== ';') display = 0;
			printf("  ");
			n++; 
			break;
		default:
			if (strncmp(buf, "Part:", 5) == 0 ||
			    strncmp(buf, "Name:", 5) == 0 ||
			    strncmp(buf, "Serial:", 7) == 0 ||
			    strncmp(buf, "Revision:", 9) == 0) {
				display = 1;
			}
			if (strncmp(buf, "Part:", 5) == 0 ||
			    strncmp(buf, "NIC:", 4) == 0) {
				if (display==1) puts("\n      ");
				n = 0;
			}
			else if (n == 4) {
				if (display==1) puts("\n         ");
				n = 0;
			}
			if (display==1) putchar(*buf);
			break;
		}
		buf++;
	}
	putchar('\n');
}
/*
 * print_nic_info: prints the nic info for each fru
 * NOTE:
 *	CPU module's nic is connected to the heart
 *	IP30 baseboard's nic is connected to the bridge
 *	Front plane's and power supply's nics are connected to the ioc3
 */
void
print_nic_info(int fru_number, int slot)
{
	nic_data_t	mcr = 0;
	char		*buf;
	__uint32_t 	port;
	mgras_hw *mgbase  = (mgras_hw *)PHYS_TO_K1(MGRAS_BASE0_PHYS);


	buf = malloc(1024); 

	if (buf == 0) {
		printf("print_nic_info: malloc failed!\n");
		return;
	}

	switch (fru_number) {
	case D_FRU_PMXX:
		/* CPU module's nic is connected to the heart */
		mcr = (nic_data_t)(PHYS_TO_K1(HEART_PIU_BASE)
				+ HEART_PIU_MLAN_CTL+ 0x4);
		cfg_get_nic_info(mcr, buf);
		ide_dump_nic_info(buf, FORMAT_DUMP);
		break;
	case D_FRU_IP30:
		/* IP30 baseboard's nic is connected to the bridge */
		mcr = (nic_data_t)(K1_MAIN_WIDGET(BRIDGE_ID) + BRIDGE_NIC);
		cfg_get_nic_info(mcr, buf);
		ide_dump_nic_info(buf, FORMAT_DUMP);
		break;
	case D_FRU_FRONT_PLANE:
		/* Front plane's nic is connected to the ioc3 */
		mcr = (nic_data_t)(IOC3_PCI_DEVIO_K1PTR+IOC3_MCR);
		cfg_get_nic_info(mcr, buf);
		ide_dump_nic_info(buf, FORMAT_DUMP);
			break;
	case D_FRU_PCI_CARD:
		/* no NICs on PCI cards in general */
	case D_FRU_MEM:
		/* no part number reported as they depend on DIMM size*/
		/*	and there could be 3rd party vendors */
		break;
	case D_FRU_GFX:
		/* NIC on gamera is on HQ4, use the passed in XIO slot # */
		if (slot >= XBOW_PORT_9 && slot <= XBOW_PORT_D) {
			mgbase  = (mgras_hw *) K1_MAIN_WIDGET(slot);
			mcr = (nic_data_t)&mgbase->microlan_access;
			cfg_get_nic_info(mcr, buf);
			ide_dump_nic_info(buf, FORMAT_DUMP);
		}
		break;
	default:
		printf("ERROR: UNKNOWN FRU!\n");
	}
	free(buf);
}

int
ismp(void)
{
	return cpu_probe() != 1;
}
int
slavecpu(void)
{
	return !cpuid();		/* only works 2 way */
}
#endif /* ! PROM */

/*
 * Following fcts are used by libpide.a (PROM) as well as
 * rest of ide. If you adding new fcts put it under the
 * !PROM ifdef unless its used by libpide.a
 */

/*
 * Print out error summary of diagnostics test
 */
void
sum_error (char *test_name)
{
#ifdef PROM
	msg_printf (SUM, "\r\t\t\t\t\t\t\t\t- ERROR -\n");
#endif /* PROM */
	msg_printf (SUM, "\rHardware failure detected in %s test\n", test_name);
}

/*
 *  Print out commonly used successful completion message
 */
void
okydoky(void)
{
	msg_printf(VRB, "Test completed with no errors.\n");
}
