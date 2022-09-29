/*
 * main diags file.
 *
 * $Id: pcidiags.c,v 1.1 1997/08/18 20:41:00 philw Exp $
 *
 * modified to do more complete testing by bongers@mfg
 *
 */

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/mace.h>
#include <sys/sbd.h>
#include "../../../include/libsc.h"
#include <DBCsim.h>
#include "../../../include/pci/PCI_defs.h"

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

	printf("Initializing MACE PCI\n");
	printf("PCI rev. info 0x%x\n", *pci_rev_info);

	/*
	 * Enable interrupts for scsi 0 and scsi 1 only.
	 * Enable all error conditions.
	 */
	printf("Set PCI CONTROL REG and read back\n");
	*pci_config_reg = 0xff000053;

	/* read back to check */
	readback = *pci_config_reg;
	if (readback != (uint)0xff000053)
		printf("Readback failed, got 0x%x, expected 0xff000053\n",
			readback);
	else
		printf("Readback OK\n");

	return 0;
}

/*
 * Read from the configuration space of the 2 motherboard scsi devices.
 * This code is based on lib/libsk/io/pci_intf.c pci_get_cfg() - any
 * changes/bug fixes here should be reflected back to that function.
 *
 * Changes made to reflect the fact that the Adaptec chip only reads
 * 0's on the lower order byte
 */

#define PCI_CONFIG_ADDR_PTR     ((volatile u_int32_t *) (PHYS_TO_K1(PCI_CONFIG_ADDR)))
#define PCI_CONFIG_DATA_PTR     ((volatile u_int32_t *) (PHYS_TO_K1(PCI_CONFIG_DATA)))

void test_pci_cfg_space()
{

	int i, test_data, save_value;
	int vendid, cfgaddr, data_in, failed;

	setup_mace_pci();

	for (i=1; i < 3; i++) {
		printf("Read Vendor and Device ID from PCI slot %d\n",
			i);

		cfgaddr = (1 << 31) | (i << CFG1_DEVICE_SHIFT); 
		*(PCI_CONFIG_ADDR_PTR) = cfgaddr;

		vendid = *(PCI_CONFIG_DATA_PTR);
		printf("Device/Vendor ID is 0x%x\n", vendid);
	}
	/*
	 * use scsi PCI dev1 config space base address 0
	 * to verify data bus integrity
	 */
	failed = 0;
	cfgaddr = (1 << 31) | (1 << CFG1_DEVICE_SHIFT);
	*(PCI_CONFIG_ADDR_PTR) = cfgaddr;
	vendid = *(PCI_CONFIG_DATA_PTR);
	if ( vendid != 0xFFFFFFFF ) {
		/*
		 * test the upper 3 bytes of the config data using i/o base addr register on scsi 0
		*/
		cfgaddr = (1 << 31) | (1 << CFG1_DEVICE_SHIFT) | PCI_CFG_BASE_ADDR_0;
		*(PCI_CONFIG_ADDR_PTR) = cfgaddr;
		save_value = *(PCI_CONFIG_DATA_PTR);
		for (i=8; i < 32; i++) {
			*(PCI_CONFIG_DATA_PTR) = (0x1 << i); /* walking one */
			data_in = *(PCI_CONFIG_DATA_PTR) & 0xffffff00;
			if ( data_in != (1 << i) ) {
				printf("PCI addr/data bus error: data is:: 0x%x  data sb:: 0x%x\n", data_in, (1 << i));
				failed += 1;
			}
		}
		i = 0;
		while ( i <= 3 ) { 
			test_data = (i * 0x55555555) & 0xffffff00; /* 0's, 5's, a's and f's */
			*(PCI_CONFIG_DATA_PTR) = test_data;
			data_in = *(PCI_CONFIG_DATA_PTR) & 0xffffff00;
			if ( data_in != test_data ) {
				printf("PCI addr/data bus error: data is:: 0x%x  data sb:: 0x%x\n", data_in, test_data);
				failed += 1;
			}
			i += 1;
		}
		*(PCI_CONFIG_DATA_PTR) = save_value;
		/*
		 * test the lower byte of the config data using interrupt line select register on scsi 0
		*/
		cfgaddr = (1 << 31) | (1 << CFG1_DEVICE_SHIFT) | PCI_INTR_LINE;
		*(PCI_CONFIG_ADDR_PTR) = cfgaddr;
		save_value = *(PCI_CONFIG_DATA_PTR);
		i = 0;
		while ( i <= 3 ) { 
			test_data = (i * 0x00000055) & 0x000000ff; /* 0's, 5's, a's and f's */
			*(PCI_CONFIG_DATA_PTR) = test_data;
			data_in = *(PCI_CONFIG_DATA_PTR) & 0x000000ff;
			if ( data_in != test_data ) {
				printf("PCI addr/data bus error: data is:: 0x%x  data sb:: 0x%x\n", data_in, test_data);
				failed += 1;
			}
			i += 1;
		}
		for (i=0; i < 8; i++) {
			*(PCI_CONFIG_DATA_PTR) = (0x1 << i); /* walking one */
			data_in = *(PCI_CONFIG_DATA_PTR) & 0x000000ff;
			if ( data_in != (1 << i) ) {
				printf("PCI addr/data bus error: data is:: 0x%x  data sb:: 0x%x\n", data_in, (1 << i));
				failed += 1;
			}
		}
		*(PCI_CONFIG_DATA_PTR) = save_value;
	}
	else {
		printf("PCI dev#1 not responding: Device/Vendor ID is 0x%x\n", vendid);
		failed += 1;
	}
	if (failed != 0) {
		printf("PCI address/data bus test FAILED :-(((\n");
	}
	else {
		printf("PCI address/data bus test PASSED :-)))\n");
	}
/*	return (failed); */
}
