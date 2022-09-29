

#define PROM    0xbfc00000
#define PC_BASE_REGS	0xbf310000
#define ISA_CNTRL       0xbf310004
#define FP_CNTRL        0xbf31000c
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

#define PC_INT_STATUS   (PHYS_TO_K1(ISA_INT_STS_REG))
#define PC_INT_MASK     (PHYS_TO_K1(ISA_INT_MSK_REG))
#define COUNTERS        (PHYS_TO_K1(MACE_UST))
#define CMP1_ALIAS      (PHYS_TO_K1(MACE_COMPARE1))
#define CMP2_ALIAS      (PHYS_TO_K1(MACE_COMPARE2))
#define CMP3_ALIAS      (PHYS_TO_K1(MACE_COMPARE3))
#define CMP1_INT_BIT    0x2000
#define CMP2_INT_BIT    0x4000
#define CMP3_INT_BIT    0x8000

struct pc_base_regs{
	unsigned long long	ring_base;
	unsigned long long	flash_prom_cntrl;
	unsigned long long	pc_int_status;
	unsigned long long	pc_int_mask;
};

struct counters{
        unsigned long long   ust;
        unsigned long long   cmp1;
        unsigned long long   cmp2;
        unsigned long long   cmp3;
        unsigned long   aud_in_msc;
        unsigned long   aud_in_ust;
        unsigned long   aud_out1_msc;
        unsigned long   aud_out1_ust;
        unsigned long   aud_out2_msc;
        unsigned long   aud_out2_ust;
        unsigned long   vid_in1_msc;
        unsigned long   vid_in1_ust;
        unsigned long   vid_in2_msc;
        unsigned long   vid_in2_ust;
        unsigned long   vid_out_msc;
        unsigned long   vid_out_ust;
};

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
}isa_ace_regs;
