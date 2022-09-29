#ident "$Header: /proj/irix6.5.7m/isms/stand/arcs/dmon/include/RCS/scc_cons.h,v 1.1 1994/07/20 22:55:09 davidl Exp $"
/*	%Q%	%I%	%M%	*/
/* $Copyright$ */

/* SCC write registers */
#define         SCCW_CMD_R                      0 /* command register */
# define        SCCW_CMD_RSEL_F                 0
# define        SCCW_CMD_RSEL_M                 (0x07 << SCCW_CMD_RSEL_F)
# define        SCCW_CMD_CMD_F                  3
# define        SCCW_CMD_CMD_M                  (0x07 << SCCW_CMD_CMD_F)
#  define       SCCW_CMD_CMD_Null               (0    << SCCW_CMD_CMD_F)
#  define       SCCW_CMD_CMD_PtHi               (1    << SCCW_CMD_CMD_F)
#  define       SCCW_CMD_CMD_ResetExtIntr       (2    << SCCW_CMD_CMD_F)
#  define       SCCW_CMD_CMD_SendAbort          (3    << SCCW_CMD_CMD_F)
#  define       SCCW_CMD_CMD_ArmRxIntr          (4    << SCCW_CMD_CMD_F)
#  define       SCCW_CMD_CMD_ResetTxIntr        (5    << SCCW_CMD_CMD_F)
#  define       SCCW_CMD_CMD_ResetError         (6    << SCCW_CMD_CMD_F)
#  define       SCCW_CMD_CMD_ResetIUS           (7    << SCCW_CMD_CMD_F)
# define        SCCW_CMD_RES_F                  6
# define        SCCW_CMD_RES_M                  (0x03 << SCCW_CMD_RES_F)
#  define       SCCW_CMD_RES_Null               (0    << SCCW_CMD_RES_F)
#  define       SCCW_CMD_RES_CrcChecker         (0    << SCCW_CMD_RES_F)
#  define       SCCW_CMD_RES_CrcGenerator       (0    << SCCW_CMD_RES_F)
#  define       SCCW_CMD_RES_Underrun           (0    << SCCW_CMD_RES_F)

#define         SCCW_ENAB_R                     1 /* Interrupt enables */
# define        SCCW_ENAB_ExtIntr               0x01
# define        SCCW_ENAB_TxIntr                0x02
# define        SCCW_ENAB_ParitySpecial         0x04
# define        SCCW_ENAB_RxIntr1stChar         0x08
# define        SCCW_ENAB_RxIntrAllChars        0x10
# define        SCCW_ENAB_RxIntrSpecial         0x18
# define	SCCW_ENAB_RxMask		0x18

#define         SCCW_VECTOR_R                   2 /* Interrupt vector */

#define         SCCW_RCV_R                      3 /* Receiver Control */
# define        SCCW_RCV_RxEn                   0x01
# define        SCCW_RCV_SyncLoadInhibit        0x02
# define        SCCW_RCV_AddressSearch          0x04
# define        SCCW_RCV_RxCrcEnable            0x08
# define        SCCW_RCV_Hunt                   0x10
# define        SCCW_RCV_AutoEnables            0x20
# define        SCCW_RCV_5Bits                  0x00
# define        SCCW_RCV_6Bits                  0x80
# define        SCCW_RCV_7Bits                  0x40
# define        SCCW_RCV_8Bits                  0xC0

#define         SCCW_MISC_R                     4
# define        SCCW_MISC_PAR_F                 0
# define        SCCW_MISC_PAR_M                 (0x03 << SCCW_MISC_PAR_F)
#  define       SCCW_MISC_PAR_ParityOff         (0    << SCCW_MISC_PAR_F)
#  define       SCCW_MISC_PAR_ParityOdd         (1    << SCCW_MISC_PAR_F)
#  define       SCCW_MISC_PAR_ParityEven        (3    << SCCW_MISC_PAR_F)
# define        SCCW_MISC_SB_F                  2
# define        SCCW_MISC_SB_M                  (0x03 << SCCW_MISC_SB_F)
#  define       SCCW_MISC_SB_1                  (1    << SCCW_MISC_SB_F)
#  define       SCCW_MISC_SB_15                 (2    << SCCW_MISC_SB_F)
#  define       SCCW_MISC_SB_2                  (3    << SCCW_MISC_SB_F)
# define        SCCW_MISC_CLK_F                 6
# define        SCCW_MISC_CLK_M                 (0x03 << SCCW_MISC_CLK_F)
#  define       SCCW_MISC_CLK_1                 (0    << SCCW_MISC_CLK_F)
#  define       SCCW_MISC_CLK_16                (1    << SCCW_MISC_CLK_F)
#  define       SCCW_MISC_CLK_32                (2    << SCCW_MISC_CLK_F)
#  define       SCCW_MISC_CLK_64                (3    << SCCW_MISC_CLK_F)

#define         SCCW_TXMDM_R                    5
# define        SCCW_TXMDM_TxCrcEnable          0x01
# define        SCCW_TXMDM_LabeledRTS           0x02
# define        SCCW_TXMDM_NotSDLC	        0x04
# define        SCCW_TXMDM_TxEnable             0x08
# define        SCCW_TXMDM_SendBreak            0x10
# define        SCCW_TXMDM_5Bits                0x00
# define        SCCW_TXMDM_6Bits                0x40
# define        SCCW_TXMDM_7Bits                0x20
# define        SCCW_TXMDM_8Bits                0x60
# define        SCCW_TXMDM_LabeledDTR           0x80

#define         SCCW_SYNC_1st_R                 6 /* Syn/Addr */
# define        SCCW_SYNC_Addr_R                SCCW_SYNC_1st_R
#define         SCCW_SYNC_2nd_R                 7 /* Syn/Flag */
# define        SCCW_SYNC_Flag_R                SCCW_SYNC_2nd_R

#define         SCCW_MIC_R                      9 /* Master Intr Control */
# define        SCCW_MIC_VecIncludesStatus      0x01
# define        SCCW_MIC_NoVector               0x02
# define        SCCW_MIC_MIE                    0x08
# define        SCCW_MIC_VecMod123              0x00
# define        SCCW_MIC_VecMod456              0x10
#define         SCCW_RES_R                      SCCW_MIC_R
# define        SCCW_RES_F                      6
# define        SCCW_RES_M                      (0x03 << SCCW_RES_F)
#  define       SCCW_RES_A                      (0x02 << SCCW_RES_F)
#  define       SCCW_RES_B                      (0x01 << SCCW_RES_F)
#  define       SCCW_RES_All                    (0x03 << SCCW_RES_F)

#define         SCCW_ENCODE_R                   10 /* Data encoding */

#define         SCCW_CLK_R                      11
# define        SCCW_CLK_DriveTRxC              0x04
# define        SCCW_CLK_XTAL                   0x80
# define        SCCW_CLK_TRxC_F                 0
# define        SCCW_CLK_TRxC_M                 (0x03 << SCCW_CLK_TRxC_F)
#  define       SCCW_CLK_TRxC_Xtal              (0    << SCCW_CLK_TRxC_F)
#  define       SCCW_CLK_TRxC_TxC               (1    << SCCW_CLK_TRxC_F)
#  define       SCCW_CLK_TRxC_BRGen             (2    << SCCW_CLK_TRxC_F)
#  define       SCCW_CLK_TRxC_DPLL              (3    << SCCW_CLK_TRxC_F)
# define        SCCW_CLK_TxC_F                  3
# define        SCCW_CLK_TxC_M                  (0x03 << SCCW_CLK_TxC_F)
#  define       SCCW_CLK_TxC_RTxC               (0    << SCCW_CLK_TxC_F)
#  define       SCCW_CLK_TxC_TRxC               (1    << SCCW_CLK_TxC_F)
#  define       SCCW_CLK_TxC_BRGen              (2    << SCCW_CLK_TxC_F)
#  define       SCCW_CLK_TxC_DPLL               (3    << SCCW_CLK_TxC_F)
# define        SCCW_CLK_RxC_F                  5
# define        SCCW_CLK_RxC_M                  (0x03 << SCCW_CLK_RxC_F)
#  define       SCCW_CLK_RxC_RTxC               (0    << SCCW_CLK_RxC_F)
#  define       SCCW_CLK_RxC_TRxC               (1    << SCCW_CLK_RxC_F)
#  define       SCCW_CLK_RxC_BRGen              (2    << SCCW_CLK_RxC_F)
#  define       SCCW_CLK_RxC_DPLL               (3    << SCCW_CLK_RxC_F)

#define         SCCW_BRLO_R                     12
#define         SCCW_BRHI_R                     13

#define         SCCW_AUX_R                      14
# define        SCCW_AUX_BRGenEnable            0x01
# define        SCCW_AUX_BRfromPClock           0x02
# define        SCCW_AUX_BRfromTxC              0x00
# define		SCCW_AUX_Echo					0x08
# define		SCCW_AUX_Loopback				0x10
# define        SCCW_AUX_CMD_F                  5
# define        SCCW_AUX_CMD_M                  (0x07 << SCCW_AUX_CMD_F)
#  define       SCCW_AUX_CMD_Null               (0    << SCCW_AUX_CMD_F)
#  define       SCCW_AUX_CMD_Search             (1    << SCCW_AUX_CMD_F)
#  define       SCCW_AUX_CMD_ResetMissingClock  (2    << SCCW_AUX_CMD_F)
#  define       SCCW_AUX_CMD_DisableDPLL        (3    << SCCW_AUX_CMD_F)
#  define       SCCW_AUX_CMD_BRtoDPLL           (4    << SCCW_AUX_CMD_F)
#  define       SCCW_AUX_CMD_RTxCtoDPLL         (5    << SCCW_AUX_CMD_F)
#  define       SCCW_AUX_CMD_FM                 (6    << SCCW_AUX_CMD_F)
#  define       SCCW_AUX_CMD_NRZI               (7    << SCCW_AUX_CMD_F)

#define         SCCW_EXTINTR_R                  15
# define        SCCW_EXTINTR_ZeroCount          0x01
# define        SCCW_EXTINTR_LabeledDCD         0x08
# define        SCCW_EXTINTR_LabeledSYNC        0x10
# define        SCCW_EXTINTR_LabeledCTS         0x20
# define        SCCW_EXTINTR_Eom                0x40
# define        SCCW_EXTINTR_Break              0x80




/* SCC read registers */
#define         SCCR_STATUS_R                   0
# define        SCCR_STATUS_RxDataAvailable     0x01
# define        SCCR_STATUS_ZeroCount           0x02
# define        SCCR_STATUS_TxReady             0x04
# define        SCCR_STATUS_LabeledDCD          0x08
# define        SCCR_STATUS_LabeledSYNC         0x10
# define        SCCR_STATUS_LabeledCTS          0x20
# define        SCCR_STATUS_Eom                 0x40
# define        SCCR_STATUS_Break               0x80

#define         SCCR_SPECIAL_R                  1
# define        SCCR_SPECIAL_TxEmpty            0x01
# define        SCCR_SPECIAL_ParityError        0x10
# define        SCCR_SPECIAL_OverrunError       0x20
# define        SCCR_SPECIAL_FramingError       0x40

#define         SCCRA_IP_R                      3
#define         SCCRA_IP_M                      0x3F
# define        SCCRA_IP_ExtB                   (1 << 0)
# define        SCCRA_IP_TxB                    (1 << 1)
# define        SCCRA_IP_RxB                    (1 << 2)
# define        SCCRA_IP_ExtA                   (1 << 3)
# define        SCCRA_IP_TxA                    (1 << 4)
# define        SCCRA_IP_RxA                    (1 << 5)

/* __EOF__ */
