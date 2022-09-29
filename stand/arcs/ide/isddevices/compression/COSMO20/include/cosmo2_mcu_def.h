

typedef struct name_tbl {
	char *name ;
	__uint32_t offset;
	UWORD size;
	UWORD mask;
} name_tbl_t ;

#define         HARDWARE_REVISION       1

#define		FBC_XILINX_CODE		1

#define		MODE1	1
#define		MODE2	2
#define		MODE3	3
#define		MODE4	4
#define		MODE5	5
#define		MODE6	6
#define		MODE7	7
#define		MODE8	8
#define		MODE9	9

/*  MCU PORTS/REGISTER */
#define  	DDRGP			0xFFF906
#define         SYNCR                   0xfffa04
#define		CFORC                	0xfff924
#define         PWMC                    0xfff925
#define         PORTE                   0xfffa11
#define  PORTE0               		0xfffa11
#define  PORTE1               		0xfffa13
#define  DDRE       			0xfffa15
#define  PEPAR      			0xfffa17


#define		SIXTEENMHZ		0x1000

#define			QPDR					0xfffc15
#define			PQS7    	0x80
#define			PQS6		0x40
#define			PORTF0		0xfffa19
#define			PBWMA		0x02

#define		GVS_LD_PAR		0x02
#define		GVS_AD_SEL		0x01

#define		RESET_LOW_1_UPC_SCALER			0x40
#define		RESET_LOW_1_CPC_050_016_FBC		0x80


#define		DOWN_LOAD_XILINX	0x0002
#define		MCU_FIFO_LB			0x0005
#define		CC1_FB_WRITE_CMD	0x0006
#define		CC1_FB_READ_CMD		0x0007
#define		SRAM_CODE			0x0008

#define	 ofc1_write_slaveaddress 	((OFC1_WRITE_SLAVEADDRESS) << 16)
#define	 vip_read_slaveaddress  	((VIP_READ_SLAVEADDRESS) << 16)
#define	 vip_write_slaveaddress 	((VIP_WRITE_SLAVEADDRESS) << 16)
#define	 dveg_write_slaveaddress 	((DVEG_WRITE_SLAVEADDRESS) << 16)

