/*
 * main diags file.
 *
 * $Id: pcidiags.c,v 1.1 1997/05/15 16:11:26 philw Exp $
 *
 * modified to do more complete testing by bongers@mfg
 *
 */

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/mace.h>
#include <sys/sbd.h>
#include <uif.h>
#include <IP32_status.h>
#include "libsc.h"
#include "../../../include/pci/PCI_defs.h"

code_t	pci_ecode;
#define	ZERO_FIVE_A_F	0x01;
#define	WALKING_1	0x02;
#define PCI_NO_RESPONSE	0x03;

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

	msg_printf(VRB, "Initializing MACE PCI rev. info 0x%x\n", *pci_rev_info);

	/*
	 * Enable interrupts for scsi 0 and scsi 1 only.
	 * Enable all error conditions.
	 */
	*pci_config_reg = (PCI_CONFIG_BITS | 0x03);

	/* read back to check */
	readback = *pci_config_reg;
	if (readback != (uint)(PCI_CONFIG_BITS | 0x03))
		msg_printf(ERR,"Readback failed, got 0x%x, expected 0x%x\n",
			readback,(PCI_CONFIG_BITS | 0x03));
	else
		msg_printf(VRB,"Readback OK\n");
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

int test_pci_cfg_space()
{

	int i, test_data, save_value;
	int vendid, cfgaddr, data_in, failed;
	volatile uint *pci_config_reg = (uint *) PHYS_TO_K1(PCI_CONTROL);
	volatile uint *pci_eflags_reg = (uint *) PHYS_TO_K1(PCI_ERROR_FLAGS);

	pci_ecode.sw   = SW_IDE;
	pci_ecode.func = FUNC_PCI_CORE;
	pci_ecode.test = 0x01;
	pci_ecode.code = 0;
	pci_ecode.w1   = 0;
	pci_ecode.w2   = 0;

	setup_mace_pci();

	for (i=1; i <= 3; i++) {
		cfgaddr = (1 << 31) | (i << CFG1_DEVICE_SHIFT); 
		*(PCI_CONFIG_ADDR_PTR) = cfgaddr;
		vendid = *(PCI_CONFIG_DATA_PTR);
		/* report at 'VRB' level */
		msg_printf (VRB, "Read Vendor and Device ID from PCI slot %d: 0x%x\n", i, vendid);
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
		pci_ecode.code = WALKING_1;
		pci_ecode.test = 0x02;
		for (i=8; i < 32; i++) {
			*(PCI_CONFIG_DATA_PTR) = (0x1 << i); /* walking one */
			data_in = *(PCI_CONFIG_DATA_PTR) & 0xffffff00;
			if ( data_in != (1 << i) ) {
				pci_ecode.w1 = (1 << i);
				pci_ecode.w2 = data_in;
				report_error(&pci_ecode);
				msg_printf (ERR, "PCI addr/data bus error: data is:: 0x%x  data sb:: 0x%x\n", data_in, (1 << i));
				failed += 1;
			}
		}
		pci_ecode.code = ZERO_FIVE_A_F;
		pci_ecode.test = 0x03;
		i = 0;
		while ( i <= 3 ) { 
			test_data = (i * 0x55555555) & 0xffffff00; /* 0's, 5's, a's and f's */
			*(PCI_CONFIG_DATA_PTR) = test_data;
			data_in = *(PCI_CONFIG_DATA_PTR) & 0xffffff00;
			if ( data_in != test_data ) {
				pci_ecode.w1 = test_data;
				pci_ecode.w2 = data_in;
				report_error(&pci_ecode);
				msg_printf (ERR, "PCI addr/data bus error: data is:: 0x%x  data sb:: 0x%x\n", data_in, test_data);
				failed += 1;
			}
			i += 1;
		}
		*(PCI_CONFIG_DATA_PTR) = save_value;
		/*
		 * test the lower byte of the config data using interrupt line select register on scsi 0
		*/
		pci_ecode.test = 0x04;
		cfgaddr = (1 << 31) | (1 << CFG1_DEVICE_SHIFT) | PCI_INTR_LINE;
		*(PCI_CONFIG_ADDR_PTR) = cfgaddr;
		save_value = *(PCI_CONFIG_DATA_PTR);
		i = 0;
		while ( i <= 3 ) { 
			test_data = (i * 0x00000055) & 0x000000ff; /* 0's, 5's, a's and f's */
			*(PCI_CONFIG_DATA_PTR) = test_data;
			data_in = *(PCI_CONFIG_DATA_PTR) & 0x000000ff;
			if ( data_in != test_data ) {
				pci_ecode.w1 = test_data;
				pci_ecode.w2 = data_in;
				report_error(&pci_ecode);
				msg_printf (ERR, "PCI addr/data bus error: data is:: 0x%x  data sb:: 0x%x\n", data_in, test_data);
				failed += 1;
			}
			i += 1;
		}
		pci_ecode.code = WALKING_1;
		pci_ecode.test = 0x05;
		for (i=0; i < 8; i++) {
			*(PCI_CONFIG_DATA_PTR) = (0x1 << i); /* walking one */
			data_in = *(PCI_CONFIG_DATA_PTR) & 0x000000ff;
			if ( data_in != (1 << i) ) {
				pci_ecode.w1 = (1 << i);
				pci_ecode.w2 = data_in;
				report_error(&pci_ecode);
				msg_printf (ERR, "PCI addr/data bus error: data is:: 0x%x  data sb:: 0x%x\n", data_in, (1 << i));
				failed += 1;
			}
		}
		*(PCI_CONFIG_DATA_PTR) = save_value;
	}
	else {
		pci_ecode.code = PCI_NO_RESPONSE;
		report_error(&pci_ecode);
		msg_printf (ERR, "PCI dev#1 not responding: Device/Vendor ID is 0x%x\n", vendid);
		failed += 1;
	}
	*pci_config_reg = PCI_CONFIG_BITS;
	*pci_eflags_reg = 0x0;
	if (failed != 0) {
		sum_error("PCI address/data bus test FAILED :-(((");
	}
	else {
		okydoky();
	}
	return (failed); 
}
