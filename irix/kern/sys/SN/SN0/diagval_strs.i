/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * This file contains the definition of all of the diagnostic strings
 * in the system.  We moved it into an include file to avoid problems
 * with keeping multiple copies of the file up-to-date.
 *
 * This file may only be included in only one file per application.
 */

#include <sys/SN/kldiag.h>

char generic_enet_fail_str[] =     "Failed ethernet hardware test.";
char generic_scsimem_fail_str[] =  "Failed SCSI memory test.";
char generic_scsidma_fail_str[] =  "Failed SCSI DMA test.";
char generic_scsiself_fail_str[] = "Failed SCSI selftest.";
char generic_bridge_fail_str[] =   "Failed bridge hardware test.";
char generic_io6cfg_fail_str[] =   "Failed IO6 config. space test.";
char generic_pci_fail_str[] =      "Failed PCI bus hardware test.";
char generic_serpio_fail_str[] =   "Failed serial PIO test.";
char generic_serdma_fail_str[] =   "Failed serial DMA test.";
char generic_xbow_fail_str[] =     "Failed XBow hardware test.";


diagval_t diagval_map[] = {
    /* success */
    {KLDIAG_PASSED,		0, 0,	"Device passed diagnostics."},

    /* cache tests */
    {KLDIAG_DCACHE_DATA_WAY0,	0, 0,	"Failed dcache data test (way 0)."},
    {KLDIAG_DCACHE_DATA_WAY1,	0, 0,	"Failed dcache data test (way 1)."},
    {KLDIAG_DCACHE_ADDR_WAY0,	0, 0,	"Failed dcache addr test (way 0)."},
    {KLDIAG_DCACHE_ADDR_WAY1,	0, 0,	"Failed dcache addr test (way 1)."},
    {KLDIAG_DCACHE_TAG,		0, 0,	"Failed dcache tag test."},

    {KLDIAG_SCACHE_DATA_WAY0,	0, 0,	"Failed scache data test (way 0)."},
    {KLDIAG_SCACHE_DATA_WAY1,	0, 0,	"Failed scache data test (way 1)."},
    {KLDIAG_SCACHE_ADDR_WAY0,	0, 0,	"Failed scache addr test (way 0)."},
    {KLDIAG_SCACHE_ADDR_WAY1,	0, 0,	"Failed scache addr test (way 1)."},
    {KLDIAG_SCACHE_TAG_WAY0,	0, 0,	"Failed scache tag test (way 0)."},
    {KLDIAG_SCACHE_TAG_WAY1,	0, 0,	"Failed scache tag test (way 1)."},
    {KLDIAG_SCACHE_FTAG,	0, 0,	"Failed FPU scache tag ram test."},

    {KLDIAG_ICACHE_DATA_WAY0,	0, 0,	"Failed icache data test (way 0)."},
    {KLDIAG_ICACHE_DATA_WAY1,	0, 0,	"Failed icache data test (way 1)."},
    {KLDIAG_ICACHE_ADDR_WAY0,	0, 0,	"Failed icache addr test (way 0)."},
    {KLDIAG_ICACHE_ADDR_WAY1,	0, 0,	"Failed icache addr test (way 1)."},

    {KLDIAG_DCACHE_HANG,	0, 0,	"Dcache test hung."},
    {KLDIAG_SCACHE_HANG,	0, 0,	"Scache test hung."},
    {KLDIAG_ICACHE_HANG,	0, 0,	"Icache test hung."},
    {KLDIAG_CACHE_INIT_HANG,	0, 0,	"I & D cache init hung."},

    {KLDIAG_TESTING_DCACHE,	0, 0,	"CPU testing dcache."},
    {KLDIAG_TESTING_ICACHE,	0, 0,	"CPU testing icache."},
    {KLDIAG_TESTING_SCACHE,	0, 0,	"CPU testing scache."},
    {KLDIAG_INITING_CACHES,	0, 0,	"CPU initializing caches."},
    {KLDIAG_INITING_SCACHE,	0, 0,	"CPU initializing scache."},
    {KLDIAG_DCACHE_FAIL,	0, 0,	"Failed D-cache test."},
    {KLDIAG_ICACHE_FAIL,	0, 0,	"Failed I-cache test."},

    {KLDIAG_OTHER_CPU_DEAD,	0, 0,	"Other CPU on this node died."},
    {KLDIAG_CPU_DISABLED,	0, 0,	"CPU disabled from POD."},
    {KLDIAG_EARLYINIT_FAILED,	0, 0,	"CPU failed early init."},
    {KLDIAG_UNEXPEC_EXCP,	0, 0,	"One or both CPU(s) took unexpected exception."},
    {KLDIAG_DEAD_COP1,		0, 0,	"Unuseable Coprocessor 1."},
    {KLDIAG_LOCAL_ARB,		0, 0,	"Died in/before local arbitration."},
    {KLDIAG_MODEBIT,		0, 0,	"CPU and PROM modebits mismatch."},

    {KLDIAG_PROM_CHKSUM_BAD,	0, 0,	"Checksum in PROM is bad."},
    {KLDIAG_PROM_MEMCPY_BAD,	0, 0,	"PROM copied to memory (bank 0) is bad."},
    {KLDIAG_PROM_CPY_FAILED,	0, 0,	"PROM copy to memory failed."},
    {KLDIAG_MEMTEST_FAILED,	0, 0,	"Some DIMMs failed mem test."},
    {KLDIAG_DIRTEST_FAILED,	0, 0,	"Some DIMMs failed dir test."},
    {KLDIAG_MEMORY_DISABLED,	0, 0,	"Memory disabled from POD."},
    {KLDIAG_PROM_BAD_BANK0,	0, 0,	"Unuseable bank 0."},
    {KLDIAG_IP27_256MB_BANK,	0, 0,	"256 MB dimm bank not supported on IP27 node boards."},

    {KLDIAG_ENET_EXCEPTION,     0, 0,   generic_enet_fail_str },
    {KLDIAG_ENET_SSRAM_FAIL,    0, 0,   generic_enet_fail_str },
    {KLDIAG_ENET_PHYREG_FAIL,   0, 0,   generic_enet_fail_str },
    {KLDIAG_ENET_PHY_R_BUSY_TO, 0, 0,   generic_enet_fail_str },
    {KLDIAG_ENET_PHY_W_BUSY_TO, 0, 0,   generic_enet_fail_str },
    {KLDIAG_ENET_PHY_RESET_TO,  0, 0,   generic_enet_fail_str },
    {KLDIAG_ENET_NO_TXCLK,      0, 0,   generic_enet_fail_str },
    {KLDIAG_ENET_LOOP_ILL_PARM, 0, 0,   generic_enet_fail_str },
    {KLDIAG_ENET_LOOP_MALLOC,   0, 0,   generic_enet_fail_str },
    {KLDIAG_ENET_EMCR_RST_TO,   0, 0,   generic_enet_fail_str },
    {KLDIAG_ENET_EMCR_RXEN_TO,  0, 0,   generic_enet_fail_str },
    {KLDIAG_ENET_LOOP_AN_TO,    0, 0,   generic_enet_fail_str },
    {KLDIAG_ENET_LOOP_AN_ABLE,  0, 0,   generic_enet_fail_str },
    {KLDIAG_ENET_LOOP_AN_MODE,  0, 0,   generic_enet_fail_str },
    {KLDIAG_ENET_LOOP_DMA_TO1,  0, 0,   generic_enet_fail_str },
    {KLDIAG_ENET_LOOP_DMA_TO2,  0, 0,   generic_enet_fail_str },
    {KLDIAG_ENET_LOOP_DMA_TO3,  0, 0,   generic_enet_fail_str },
    {KLDIAG_ENET_LOOP_BAD_RUPT1,0, 0,   generic_enet_fail_str },
    {KLDIAG_ENET_LOOP_BAD_RUPT2,0, 0,   generic_enet_fail_str },
    {KLDIAG_ENET_LOOP_BAD_RUPT3,0, 0,   generic_enet_fail_str },
    {KLDIAG_ENET_LOOP_BAD_RUPT4,0, 0,   generic_enet_fail_str },
    {KLDIAG_ENET_LOOP_NO_RUPT,  0, 0,   generic_enet_fail_str },
    {KLDIAG_ENET_LOOP_BAD_W1,   0, 0,   generic_enet_fail_str },
    {KLDIAG_ENET_LOOP_BAD_STAT, 0, 0,   generic_enet_fail_str },
    {KLDIAG_ENET_LOOP_BAD_DATA1,0, 0,   generic_enet_fail_str },
    {KLDIAG_ENET_LOOP_BAD_DATA2,0, 0,   generic_enet_fail_str },
    {KLDIAG_ENET_NIC_FAIL,      0, 0,   "Failed ethernet NIC test." },

    {KLDIAG_SCSIRAM_EXCEPTION,  0, 0,   generic_scsimem_fail_str},
    {KLDIAG_SCSI_MBOX_FAIL,     0, 0,   generic_scsimem_fail_str},
    {KLDIAG_SCSI_SSRAM_FAIL1,   0, 0,   generic_scsimem_fail_str},
    {KLDIAG_SCSI_SSRAM_FAIL2,   0, 0,   generic_scsimem_fail_str},
    {KLDIAG_SCSI_SSRAM_FAIL,    0, 0,   generic_scsimem_fail_str},
    {KLDIAG_SCSIDMA_EXCEPTION,  0, 0,   generic_scsidma_fail_str},
    {KLDIAG_SCSI_DMA_FAIL1,     0, 0,   generic_scsidma_fail_str},
    {KLDIAG_SCSI_DMA_FAIL2,     0, 0,   generic_scsidma_fail_str},
    {KLDIAG_SCSI_DMA_FAIL,      0, 0,   generic_scsidma_fail_str},
    {KLDIAG_SCSICNTRL_EXCEPTION,0, 0,   generic_scsiself_fail_str},
    {KLDIAG_SCSI_STEST_FAIL1,   0, 0,   generic_scsiself_fail_str},
    {KLDIAG_SCSI_STEST_FAIL2,   0, 0,   generic_scsiself_fail_str},
    {KLDIAG_SCSI_STEST_FAIL,    0, 0,   generic_scsiself_fail_str},

    {KLDIAG_BRIDGE_EXCEPTION,   0, 0,   generic_bridge_fail_str},
    {KLDIAG_BRIDGE_FAIL,        0, 0,   generic_bridge_fail_str},

    {KLDIAG_IO6CONFIG_EXCEPTION,0, 0,   generic_io6cfg_fail_str},
    {KLDIAG_PCICONFIG_FAIL,     0, 0,   generic_io6cfg_fail_str},

    {KLDIAG_IOC3_BASE_FAIL,     0, 0,
     "Failed to determine IOC3 base Address."},

    {KLDIAG_PCIBUS_EXCEPTION,   0, 0,   generic_pci_fail_str},
    {KLDIAG_PCIBUS_FAIL,        0, 0,   generic_pci_fail_str},

    {KLDIAG_SERPIO_EXCEPTION,   0, 0,   generic_serpio_fail_str},
    {KLDIAG_SER_PIO_FAIL1A,     0, 0,   generic_serpio_fail_str},
    {KLDIAG_SER_PIO_FAIL1B,     0, 0,   generic_serpio_fail_str},
    {KLDIAG_SER_PIO_FAILA,      0, 0,   generic_serpio_fail_str},
    {KLDIAG_SER_PIO_FAILB,      0, 0,   generic_serpio_fail_str},

    {KLDIAG_SERDMA_EXCEPTION,   0, 0,   generic_serdma_fail_str},
    {KLDIAG_SER_DMA_FAILA,      0, 0,   generic_serdma_fail_str},
    {KLDIAG_SER_DMA_FAILB,      0, 0,   generic_serdma_fail_str},

    {KLDIAG_XBOW_EXCEPTION,     0, 0,   generic_xbow_fail_str},
    {KLDIAG_XBOW_FAIL,          0, 0,   generic_xbow_fail_str},

    {KLDIAG_NI_RCVD_LNK_ERR,	0, 0,	"Hub NI saw too many link errors."},
    {KLDIAG_NI_RCVD_MISC_ERR,	0, 0,	"Hub NI got a misc err from Rtr."},
    {KLDIAG_NI_FAIL_ERR_DET,	0, 0,	"Hub NI did not detect forced errs."},
    {KLDIAG_HUB_FAIL_LBIST,	0, 0,	"Hub chip failed lbist test."},
    {KLDIAG_HUB_FAIL_ABIST,	0, 0,	"Hub chip failed abist test."},
    {KLDIAG_HUB_LLP_DOWN,	0, 0,	"Hub chip's llp interface is down."},
    {KLDIAG_DISCOVERY_FAILED,	0, 0,	"Local hub could not do discovery."},
    {KLDIAG_HUB_INTS_FAILED,	0, 0,	"Hub chip failed interrupt test."},
    {KLDIAG_NI_DIED,		0, 0,	"Kernel detected a hub ni error."},
    {KLDIAG_HUB_BTE_FAILED,	0, 0,	"Hub BTE test failed: miscompare."},
    {KLDIAG_HUB_BTE_HUNG,	0, 0,	"Hub BTE test failed: hang."},
    {KLDIAG_RTC_ERROR,		0, 0,	"Hub real time clock error."},
    {KLDIAG_RESET_WAIT,		0, 0,	"Reset propagation error."},

    {KLDIAG_NI_RTR_TIMEOUT,	0, 0,   "Rtr is not replying--may be dead."},
    {KLDIAG_NI_RTR_BAD_RPLY,	0, 0,	"Hub got overrun/mismatch rply."},
    {KLDIAG_NI_RTR_HW_ERROR,	0, 0,	"Hub got HW err accessing rtr."},
    {KLDIAG_NI_RTR_PROT_ERR,	0, 0,	"Hub got protection err from rtr."},
    {KLDIAG_NI_RTR_INV_VEC,	0, 0,	"Hub got invalid vector err from rtr."},
    {KLDIAG_RTR_FAIL_ERR_DET,	0, 0,	"Rtr did not detect forced errors."},
    {KLDIAG_RTR_RCVD_LNK_ERR,	0, 0,	"Rtr saw too many link errors."},
    {KLDIAG_RTR_FAIL_LBIST,	0, 0,	"Rtr failed lbist test."},
    {KLDIAG_RTR_FAIL_ABIST,	0, 0,	"Rtr failed abist test."},

    {KLDIAG_RTC_DIST,		0, 0,	"Failed RTC distribution due to CrayLink problems."},


    {KLDIAG_EXC_GENERAL,	0, 0,	"Unexpected General Exception."},
    {KLDIAG_EXC_ECC,		0, 0,	"Unexpected ECC Exception."},
    {KLDIAG_EXC_TLB,		0, 0,	"Unexpected TLB Refill Exception."},
    {KLDIAG_EXC_XTLB,		0, 0,	"Unexpected XTLB Refill Exception."},
    {KLDIAG_EXC_UNIMPL,		0, 0,	"Unexpected Unimplemented Exception."},
    {KLDIAG_EXC_CACHE,		0, 0,	"Unexpected Cache Error Exception."},
    {KLDIAG_NMI,		0, 0,	"A nonmaskable interrupt occurred."},
    {KLDIAG_NMIBADMEM,		0, 0,	"NMI while memory was inaccessible."},
    {KLDIAG_NMICORRUPT,		0, 0,	"NMI with corrupted vector."},
    {KLDIAG_NMIPROM,		0, 0,	"NMI while executing in PROM."},
    {KLDIAG_DEBUG,		0, 0,	"POD mode switch set or POD key pressed."},
    {KLDIAG_TBD,		0, 0,	"Unspecified diagnostic failure."},
    {KLDIAG_NOT_SET,		0, 0,	"Diagnostic value unset."},
    {KLDIAG_NOT_PRESENT,	0, 0,	"Device not present."}
};
