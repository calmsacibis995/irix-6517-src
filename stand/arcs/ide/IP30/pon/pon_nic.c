#ident  "ide/IP30/pon/pon_nic.c: $Revision: 1.13 $"

#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/nic.h>
#include <uif.h>
#include <libsk.h>
#include <libsc.h>
#include <pon.h>

extern int      nic_get_eaddr(__uint32_t *, __uint32_t *, char *);
#ifdef MFG_USED
extern void	ide_dump_nic_info(char *, int);
#endif

/* make sure none of the NICs in the system are missing */
int
pon_nic(void)
{
	static char	*testname = "Number-In-a-Can (NIC)";
	char		buf[480];	/* malloc() not set up set */
	char		eaddr[6];
	int		error = 0;
#ifdef MFG_USED
	int		dumpflag = 0;
#endif
	__psunsigned_t	gpcr_s;
	__psunsigned_t	mcr;
	char		*str_pm10;
	char		*str_pm20;

	msg_printf(VRB, "%s test.\n",testname);

	/* cpu board */
	mcr = (__psunsigned_t)&HEART_PIU_K1PTR->h_mlan_ctl + 0x4;
	cfg_get_nic_info((nic_data_t)mcr, buf);
	str_pm10 = strstr(buf, "Name:PM10");
	str_pm20 = strstr(buf, "Name:PM20");
	if (str_pm10 == 0x0 && str_pm20 == 0x0) {
	    error |= FAIL_NIC_CPUBOARD;
	    msg_printf(ERR, "Bad/missing CPU Module NIC: %s\n", buf);
#ifdef MFG_USED
	    msg_printf(SUM, "\nThe Name field in the NIC was not PM10 or PM20. Here is what was read from the NIC: \n\n");
	    ide_dump_nic_info(buf,dumpflag);
	    msg_printf(SUM, "\n\n");
#endif
	} else {
		heartreg_t	active_cpus;

		active_cpus = HEART_PIU_K1PTR->h_status &
			HEART_STAT_PROC_ACTIVE_MSK;
		active_cpus >>= HEART_STAT_PROC_ACTIVE_SHFT;

		if ((str_pm10 && active_cpus != 0x1) ||
		    (str_pm20 && active_cpus != 0x3))
			error |= FAIL_NIC_CPUBOARD;
#ifdef MFG_USED
		if (*Reportlevel >= VRB) {
			msg_printf(VRB, "\nCPU Module NIC:\n");
			ide_dump_nic_info(buf,dumpflag);
			msg_printf(VRB, "\n\n");
		}
#endif
	}

	/* base board */
	mcr = (__psunsigned_t)&BRIDGE_K1PTR->b_nic;
	cfg_get_nic_info((nic_data_t)mcr, buf);
	if (strstr(buf, "Name:IP30") == 0x0) {
	    error |= FAIL_NIC_BASEBOARD;
	    msg_printf(ERR, "Bad/missing System Board NIC: %s\n", buf);
#ifdef MFG_USED
	    msg_printf(SUM, "\nThe Name field in the NIC was not IP30. Here is what was read from the NIC: \n\n");
	    ide_dump_nic_info(buf,dumpflag);
	    msg_printf(SUM, "\n\n");
#endif
	}
#ifdef MFG_USED
	else {
		if (*Reportlevel >= VRB) {
			msg_printf(VRB, "\nSystem Board NIC:\n");
			ide_dump_nic_info(buf,dumpflag);
			msg_printf(VRB, "\n\n");
		}
	}
#endif

	/* front plane and power supply */
	mcr = IOC3_PCI_DEVIO_K1PTR + IOC3_MCR;
	cfg_get_nic_info((nic_data_t)mcr, buf);
	if (strstr(buf, "Name:FP") == 0x0) {
	    error |= FAIL_NIC_FRONTPLANE;
	    msg_printf(ERR, "Bad/missing Front Plane NIC: %s\n", buf);
#ifdef MFG_USED
	    msg_printf(SUM, "\nThe Name field in the NIC was not FP*. Here is what was read from the NIC: \n\n");
	    ide_dump_nic_info(buf,dumpflag);
	    msg_printf(SUM, "\n\n");
#endif
	}
#ifdef MFG_USED
	else {
		if (*Reportlevel >= VRB) {
			msg_printf(VRB, "\nFront Plane NIC:\n");
			ide_dump_nic_info(buf,dumpflag);
			msg_printf(VRB, "\n\n");
		}
	}
#endif

	/* All P1 systems with rev B or greater hears have new supplies */
	if (heart_rev() >= HEART_REV_B) {
#ifdef MFG_USED
		if (strstr(buf, "Name:PWR.SP") == 0x0) {
#else
		if ((strstr(buf, "Name:PWR.SP") == 0x0) && (strstr(buf, "Name:PWR_SP") == 0x0)) {
#endif
	    		error |= FAIL_NIC_PWR_SUPPLY;
	    		msg_printf(ERR,
				"Bad/missing Power Supply NIC: %s\n",
				buf);
#ifdef MFG_USED
	    		msg_printf(SUM, "\nThe Name field in the NIC was not PWR.SP* . Here is what was read from the NIC: \n\n");
	    		ide_dump_nic_info(buf,dumpflag);
	    		msg_printf(SUM, "\n\n");
#endif
		}
#ifdef MFG_USED
		else {
			if (*Reportlevel >= VRB) {
				msg_printf(VRB, "\nPower Supply NIC:\n");
				ide_dump_nic_info(buf,dumpflag);
				msg_printf(VRB, "\n\n");
			}
		}
#endif
	}

	/* ethernet address */
	mcr = IOC3_PCI_DEVIO_K1PTR + IOC3_MCR;
	gpcr_s = IOC3_PCI_DEVIO_K1PTR + IOC3_GPCR_S;
	if (nic_get_eaddr((__uint32_t *)mcr, (__uint32_t *)gpcr_s, eaddr) !=
	    NIC_OK) {
		error |= FAIL_NIC_EADDR;
		msg_printf(ERR, "Bad/missing eaddr NIC\n");
#ifdef MFG_USED
	    	msg_printf(SUM, "\nCheck the ethernet NIC on the frontplane.\n");
#endif
	}
#ifdef MFG_USED
	else {
		if (*Reportlevel >= VRB) {
	    		msg_printf(VRB, 
				"\nThe ethernet NIC was read correctly\n\n");
		}
	}
#endif

	if (error)
		sum_error(testname);
	else
		okydoky();

	return error;
}
