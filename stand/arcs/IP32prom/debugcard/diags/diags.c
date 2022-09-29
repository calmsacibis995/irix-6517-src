/*
 * main diags file.
 *
 * $Id: diags.c,v 1.1 1997/08/18 20:39:55 philw Exp $
 */

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/mace.h>
#include <DBCsim.h>
/* #include <pci/PCI_defs.h> CFG1_DEVICE_SHIFT comes from here*/


some_func()
{
	int i;

}

another_func()
{
	char cc;
	k_machreg_t reg;
	caddr_t a;

	a = (caddr_t ) PCI_IO;

}

/*
 * Setup the MACE PCI registers.
 * This is based on lib/libsk/ml/IP32.c init_pci() - any changes/bug fixes
 * here should be reflected back to that function.
 */
setup_mace_pci(void)
{
	volatile uint *pci_config_reg = (uint *) PHYS_TO_K1(PCI_CONTROL);
	volatile uint *pci_rev_info = (uint *) PHYS_TO_K1(PCI_REV_INFO_R);
	uint readback;

	DPRINTF("Initializing MACE PCI\n");
	DPRINTF("PCI rev. info 0x%x\n", *pci_rev_info);

	/*
	 * Enable interrupts for scsi 0 and scsi 1 only.
	 * Enable all error conditions.
	 */
	DPRINTF("Set PCI CONTROL REG and read back\n");
	*pci_config_reg = 0xff000053;

	/* read back to check */
	readback = *pci_config_reg;
	if (readback != 0xff000053)
		DPRINTF("Readback failed, got 0x%x, expected 0xff000053\n",
			readback);
	else
		DPRINTF("Readback OK\n");

	return 0;
}

/*
 * Read from the configuration space of the 2 motherboard scsi devices.
 * This code is based on lib/libsk/io/pci_intf.c pci_get_cfg() - any
 * changes/bug fixes here should be reflected back to that function.
 */

#define PCI_CONFIG_ADDR_PTR	((volatile u_int32_t *) (PHYS_TO_K1(PCI_CONFIG_ADDR)))
#define PCI_CONFIG_DATA_PTR	((volatile u_int32_t *) (PHYS_TO_K1(PCI_CONFIG_DATA)))
#define CFG1_DEVICE_SHIFT	11

test_pci_cfg_space()
{
	int i, vendid, cfgaddr;

	setup_mace_pci();

	for (i=1; i < 3; i++) {
		DPRINTF("Read Vendor and Device ID from PCI slot %d\n",
			i);

		cfgaddr = (1 << 31) | (1 << CFG1_DEVICE_SHIFT);
		*(PCI_CONFIG_ADDR_PTR) = cfgaddr;

		vendid = *(PCI_CONFIG_DATA_PTR);
		DPRINTF("Device/Vendor ID is 0x%x\n", vendid);
	}
}
