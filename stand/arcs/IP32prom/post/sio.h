

#define DP_RAM_BASE     0xbf312000

/* flash rom and misc control register */
#define FPROM_WR_EN     0x01
#define PSWD_CLR_EN     0x02
#define NIC_HOLD_LO     0x04
#define NIC_DATA        0x08
#define RED_DIS         0x10
#define GREEN_DIS       0x20
#define DP_RAM_WR_EN    0x40
#define RTC     0xbf3a0007
#define RTC2    0xbf3a0107

#define PC_INT_STATUS   0xbf310014
#define PC_INT_MASK     0xbf31001c
#define COUNTERS        0xbf340000
#define CMP1_ALIAS      0xbf350004
#define CMP2_ALIAS      0xbf360004
#define CMP3_ALIAS      0xbf370004
#define CMP1_INT_BIT    0x2000
#define CMP2_INT_BIT    0x4000
#define CMP3_INT_BIT    0x8000

typedef struct pc_base_regs{
	unsigned long long	ring_base;
	unsigned long long	flash_prom_cntrl;
	unsigned long long	mace_int_status;
	unsigned long long	mace_int_mask;
} pc_base_regs_t;

typedef struct counters{
        unsigned long long   ust;
        unsigned long long   cmp1;
        unsigned long long   cmp2;
        unsigned long long   cmp3;
        unsigned long long  aud_in_msc_ust;
        unsigned long long  aud_out1_msc_ust;
        unsigned long long  aud_out2_msc_ust;
        unsigned long long  vid_in1_msc_ust;
        unsigned long long  vid_in2_msc_ust;
        unsigned long long  vid_out_msc_ust;
}counters_t;

#define KEYBOARD        0xbf320000
#define MOUSE           0xbf320040

#define EOTX_CLK_INHIBIT        0x01
#define TX_ENABLE       0x02
#define TX_INT_ENABLE   0x04
#define RX_INT_ENABLE   0x08
#define CLK_INHIBIT     0x10

#define CLK_SIGNAL      0x01
#define CLK_INHBT_SIG   0x02
#define TX_IN_PROG      0x04
#define TX_BUF_EMPTY    0x08
#define RX_BUF_FULL     0x10
#define RX_IN_PROG      0x20
#define PARITY_ERR      0x40
#define FRAME_ERR       0x80

#define CLK_SIGNAL_RX   0x010
#define CLK_INHBT_SIG_RX        0x020
#define TX_IN_PROG_RX   0x040
#define TX_BUF_EMPTY_RX 0x080
#define RX_BUF_FULL_RX  0x100
#define RX_IN_PROG_RX   0x200
#define PARITY_ERR_RX   0x400
#define FRAME_ERR_RX    0x800

struct sync_serial{
        long long tx_buf;
        long long rx_buf;
        long long control;
        long long status;
};


/*
 * ISA bus definitions
 */

#define K1_BASE                 0xA0000000
#define PERPH_CONT_OFFSET       0x300000
#define MACE_BASE_ADDRS         0x1F000000
#define ISA_BUS_OFFSET          0x380000
#define SERIAL_A_OFFSET         0x10000
#define SERIAL_B_OFFSET         0x18000

#define ISA_DMA_REG_OFFSET      0x10000

#define ISA_BUS_ADDRS     (K1_BASE+MACE_BASE_ADDRS+ISA_BUS_OFFSET)

#define SERIAL_A_ADDRS       ISA_BUS_ADDRS + SERIAL_A_OFFSET
#define SERIAL_B_ADDRS       ISA_BUS_ADDRS + SERIAL_B_OFFSET


typedef struct {
	char pad1[7];
	unsigned char rbr_thr;
	char pad2[255];
	unsigned char ier;
	char pad3[255];
	unsigned char iir_fcr;
	char pad4[255];
	unsigned char lcr;
	char pad5[255];
	unsigned char mcr;
	char pad6[255];
	unsigned char lsr;
	char pad7[255];
	unsigned char msr;
	char pad8[255];
	unsigned char scr;
}isa_ace_regs_t;
