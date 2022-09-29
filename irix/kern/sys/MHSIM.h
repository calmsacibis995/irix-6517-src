#ifndef __SYS_MHSIM_H_
#define __SYS_MHSIM_H_ 1

#define MHSIM_DUART1B_DATA		PHYS_TO_K1(0x1fbd0000)
#define MHSIM_DUART_INTR_ENBL_ADDR	PHYS_TO_K1(0x1fbd0010)
#define MHSIM_DUART_INTR_STATE_ADDR	PHYS_TO_K1(0x1fbd0020)
#define MHSIM_DUART_SYNC_WR_ADDR	PHYS_TO_K1(0x1fbd0030)

#define MHSIM_DUART_RX_INTR_STATE	0x1
#define MHSIM_DUART_TX_INTR_STATE	0x2

/*
 * Support for MHSIM's version of Sabledsk
 */
#define MHSIM_DISK_MAX_TRANSFER_SIZE	(1000 * 1024)
#define MHSIM_DISK_DATA			PHYS_TO_K1(0x1f100000)
#define MHSIM_DISK_DISKNUM		PHYS_TO_K1(0x1fbf0000)
#define MHSIM_DISK_SECTORNUM		PHYS_TO_K1(0x1fbf0010)
#define MHSIM_DISK_SECTORCOUNT		PHYS_TO_K1(0x1fbf0020)
#define MHSIM_DISK_STATUS		PHYS_TO_K1(0x1fbf0030)
#define MHSIM_DISK_BYTES_TRANSFERRED	PHYS_TO_K1(0x1fbf0040)
#define MHSIM_DISK_PROBE_UNIT		PHYS_TO_K1(0x1fbf0050)
#define MHSIM_DISK_SIZE			PHYS_TO_K1(0x1fbf0060)
#define MHSIM_DISK_OPERATION		PHYS_TO_K1(0x1fbf0070)

#define MHSIM_DISK_NOP			0x0
#define MHSIM_DISK_READ			0x1
#define MHSIM_DISK_WRITE		0x2
#define MHSIM_DISK_PROBE		0x3

#define SABLE_NVRAM_BASE 0x1d000000

#define	NVRAM_SECONDS		0
#define	NVRAM_MINUTES		2
#define NVRAM_HOURS		4
#define NVRAM_DAY_OF_WEEK	6
#define NVRAM_DAY_OF_MONTH	7
#define NVRAM_MONTH		8
#define NVRAM_YEAR		9
#define NVRAM_REGD		13

typedef struct {
	uint sec;
	uint res0;
	uint min;
	uint res1;
	uint hour;
	uint res2;
	uint day;
	uint date;
	uint month;
	uint year;
	uint res3[3];
	uint regd;
} sable_clk_t;

#endif /* __SYS_MHSIM_H_ */
