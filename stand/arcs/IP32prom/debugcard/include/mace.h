#ifndef _SYS_MACE_H__
#define _SYS_MACE_H__

/*
 * $Id: mace.h,v 1.1 1997/08/18 20:41:34 philw Exp $
 */

/******************
 * MACE Address Map
 */
#define MACE_BASE    (            0x1F000000)
#define MACE_PCI     (MACE_BASE + 0x00080000)
#define MACE_VIN1    (MACE_BASE + 0x00100000)
#define MACE_VIN2    (MACE_BASE + 0x00180000)
#define MACE_VOUT    (MACE_BASE + 0x00200000)
#define MACE_ENET    (MACE_BASE + 0x00280000)
#define MACE_PERIF   (MACE_BASE + 0x00300000)
#define MACE_ISA_EXT (MACE_BASE + 0x00380000)

#define MACE_AUDIO   (MACE_PERIF + 0x00000) 
#define MACE_ISA     (MACE_PERIF + 0x10000)
#define MACE_KBDMS   (MACE_PERIF + 0x20000)
#define MACE_I2C     (MACE_PERIF + 0x30000)
#define MACE_UST_MSC (MACE_PERIF + 0X40000)


/*******************************************************
 * MACE PCI Interface Address Map and IO Address Maps
 */
#define PCI_ERROR_ADDR		(MACE_PCI+0x0)
#define PCI_ERROR_FLAGS		(MACE_PCI+0x4)
#define PCI_CONTROL		(MACE_PCI+0x8)
#define PCI_REV_INFO_R		(MACE_PCI+0xC)
#define PCI_FLUSH_W		(MACE_PCI+0xC)
#define PCI_CONFIG_ADDR		(MACE_PCI+0xCF8)
#define PCI_CONFIG_DATA		(MACE_PCI+0xCFC)
#define PCI_LOW_MEMORY		0x1A000000
#define PCI_LOW_IO		0x18000000
#define PCI_NATIVE_VIEW		0x40000000
#define PCI_IO			0x80000000

/***********************
 * PCI_ERROR_FLAGS Bits
 */
#define PERR_MASTER_ABORT		0x80000000
#define PERR_TARGET_ABORT		0x40000000
#define PERR_DATA_PARITY_ERR		0x20000000
#define PERR_RETRY_ERR			0x10000000
#define PERR_ILLEGAL_CMD		0x08000000
#define PERR_SYSTEM_ERR			0x04000000
#define PERR_INTERRUPT_TEST		0x02000000
#define PERR_PARITY_ERR			0x01000000
#define PERR_OVERRUN			0x00800000
#define PERR_RSVD			0x00400000
#define PERR_MEMORY_ADDR		0x00200000
#define PERR_CONFIG_ADDR		0x00100000
#define PERR_MASTER_ABORT_ADDR_VALID	0x00080000
#define PERR_TARGET_ABORT_ADDR_VALID	0x00040000
#define PERR_DATA_PARITY_ADDR_VALID	0x00020000
#define PERR_RETRY_ADDR_VALID		0x00010000


/*******************************
 * MACE ISA External Address Map
 */
#define ISA_EPP_BASE   (MACE_ISA_EXT+0x00000)
#define ISA_ECP_BASE   (MACE_ISA_EXT+0x08000)
#define ISA_SER1_BASE  (MACE_ISA_EXT+0x10000)
#define ISA_SER2_BASE  (MACE_ISA_EXT+0x18000)
#define ISA_RTC_BASE   (MACE_ISA_EXT+0x20000)
#define ISA_GAME_BASE  (MACE_ISA_EXT_0x30000)


/*************************
 * ISA Interface Registers
 */

/* ISA Ringbase Address and Reset Register */

#define ISA_RINGBASE       (MACE_ISA+0x0000)

/* Flash-ROM/LED/DP-RAM/NIC Controller Register */

#define ISA_FLASH_NIC_REG  (MACE_ISA+0x0008)
#define   ISA_FLASH_WE       (0x01) /* 1=> Enable FLASH writes */
#define   ISA_PWD_CLEAR      (0x02) /* 1=> PWD CLEAR jumper detected */
#define   ISA_NIC_DEASSERT   (0x04) 
#define   ISA_NIC_DATA       (0x08)
#define   ISA_LED_RED        (0x10) /* 1=> Illuminate RED LED */
#define   ISA_LED_GREEN      (0x20) /* 1=> Illuminate GREEN LED */
#define   ISA_DP_RAM_ENABLE  (0x40)

/* Interrupt Status and Mask Registers */

#define ISA_INT_STS_REG    (MACE_ISA+0x0010)
#define ISA_INT_MSK_REG    (MACE_ISA+0x0018)


/********************************
 * MACE Timer Interface Registers
 *
 * Note: MSC_UST<31:0> is MSC, MSC_UST<63:32> is UST. 
 */
#define MACE_UST	   (MACE_UST_MSC + 0x00) /* Universial system time */
#define MACE_COMPARE1      (MACE_UST_MSC + 0x08) /* Interrupt compare reg 1 */
#define MACE_COMPARE2      (MACE_UST_MSC + 0x10) /* Interrupt compare reg 2 */
#define MACE_COMPARE3      (MACE_UST_MSC + 0x18) /* Interrupt compare reg 3 */
#define MACE_UST_PERIOD    960	                 /* UST Period in ns  */

#define MACE_AIN_MSC_UST   (MACE_UST_MSC + 0x20) /* Audio in MSC/UST pair */
#define MACE_AOUT1_MSC_UST (MACE_UST_MSC + 0x28) /* Audio out 1 MSC/UST pair */
#define MACE_AOUT2_MSC_UST (MACE_UST_MSC + 0x30) /* Audio out 2 MSC/UST pair */
#define MACE_VIN1_MSC_UST  (MACE_UST_MSC + 0x38) /* Video In 1 MSC/UST pair */
#define MACE_VIN2_MSC_UST  (MACE_UST_MSC + 0x40) /* Video In 2 MSC/UST pair */
#define MACE_VOUT_MSC_UST  (MACE_UST_MSC + 0x48) /* Video out MSC/UST pair */

#endif /* _SYS_MACE_H__ */

