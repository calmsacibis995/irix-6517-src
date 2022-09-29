#ident "$Header: "
#include <sys/types.h>
#include <klib/hwreg.h>

char hwreg_xbow_stab[] =
	"x\000"
	"XB_ID\000"
	"PART_NUM\000"
	"MFG_ID\000"
	"ALWAYS_1\000"
	"XB_STAT\000"
	"LINK_F_INT_REQ\000"
	"LINK_E_INT_REQ\000"
	"LINK_D_INT_REQ\000"
	"LINK_C_INT_REQ\000"
	"LINK_B_INT_REQ\000"
	"LINK_A_INT_REQ\000"
	"LINK_9_INT_REQ\000"
	"LINK_8_INT_REQ\000"
	"WGT_0_INT_REQ\000"
	"REG_ACCESS_ERR\000"
	"XTALK_ERR\000"
	"MULTI_ERR\000"
	"XB_ERR_UPPER\000"
	"ERR_ADDR<47:32>\000"
	"XB_ERR_LOWER\000"
	"ERR_ADDR<31:0>\000"
	"XB_CTRL\000"
	"ENINT_ACCESS_ERR\000"
	"ENINT_XTALK_ERR\000"
	"XB_PKT_TO\000"
	"PKT_TO_INTERVAL\000"
	"XB_INT_UPPER\000"
	"INT_VECTOR\000"
	"INT_TARGET_WID\000"
	"INT_TARGET_ADDR<47:32>\000"
	"XB_INT_LOWER\000"
	"INT_TARGET_ADDR<31:0>\000"
	"XB_ERR_CMDWORD\000"
	"ERR_CMDWORD\000"
	"Write clears XB_ERR_CMDWORD, XB_ERR_UPPER and XB_ERR_LOWER\000"
	"XB_LLP_CTRL\000"
	"MAX_UPKT_RETRY\000"
	"NULL_TO\000"
	"MAX_BURST\000"
	"XB_STAT_CLR\000"
	"XB_ARB_RELOAD\000"
	"GBR_RELOAD_INTERVAL\000"
	"XB_PERF_CTR_A\000"
	"PORT_SELECT\000"
	"PERF_CTR_VAL\000"
	"XB_PERF_CTR_B\000"
	"XB_NIC\000"
	"NIC_BMP\000"
	"NIC_OFFSET\000"
	"NIC_DATA_VLD\000"
	"NIC_DATA\000"
	"XB_LINK_IBUF_FLUSH_8\000"
	"INPUT_FLUSH_OK_N\000"
	"XB_LINK_CTRL_8\000"
	"ENINT_LINK_ALIVE\000"
	"PERF_MODE_SEL\000"
	"INPKT_BUF_LVL\000"
	"SEND_BMOD8\000"
	"EN_FORCE_BAD_UPKT\000"
	"CREDITS_TO_WIDGET\000"
	"ENINT_ILLEGAL_DEST\000"
	"ENINT_BUF_OFLOW\000"
	"ENINT_BAND_ALLOC_ERR\000"
	"ENINT_LLP_RX_ERRCTR_OFLOW\000"
	"ENINT_LLP_TX_RTRYCTR_OFLOW\000"
	"ENINT_LLP_TX_MAX_RTRY\000"
	"ENINT_LLP_RX_ERR\000"
	"ENINT_LLP_TX_RTRY\000"
	"UNUSED\000"
	"ENINT_MAXREQ_TO\000"
	"ENINT_SRC_TO\000"
	"XB_LINK_STAT_8\000"
	"LINK_ALIVE\000"
	"ILLEGAL_DEST\000"
	"BUF_OFLOW\000"
	"BAND_ALLOC_ERR_F\000"
	"BAND_ALLOC_ERR_E\000"
	"BAND_ALLOC_ERR_D\000"
	"BAND_ALLOC_ERR_C\000"
	"BAND_ALLOC_ERR_B\000"
	"BAND_ALLOC_ERR_A\000"
	"BAND_ALLOC_ERR_9\000"
	"BAND_ALLOC_ERR_8\000"
	"LLP_RX_ERR_CTR_OFLOW\000"
	"LLP_TX_RTRY_CTR_OFLOW\000"
	"LLP_TX_MAX_RTRY\000"
	"LLP_RX_ERR\000"
	"LLP_TX_RTRY\000"
	"MAXREQ_TO\000"
	"SRC_TO\000"
	"XB_LINK_ARB_UPPER_8\000"
	"RWV_B\000"
	"GBR_B\000"
	"RWV_A\000"
	"GBR_A\000"
	"RWV_9\000"
	"GBR_9\000"
	"RWV_8\000"
	"GBR_8\000"
	"XB_LINK_ARB_LOWER_8\000"
	"RWV_F\000"
	"GBR_F\000"
	"RWV_E\000"
	"GBR_E\000"
	"RWV_D\000"
	"GBR_D\000"
	"RWV_C\000"
	"GBR_C\000"
	"XB_LINK_STAT_CLR_8\000"
	"Also clears XB_LINK_AUX_STAT\000"
	"XB_LINK_RESET_8\000"
	"IGNORED\000"
	"Write resets port and sends link-reset to widget\000"
	"XB_LINK_AUX_STAT_8\000"
	"LLP_RX_ERR_CTR\000"
	"LLP_TX_RTRY_CTR\000"
	"TO_PKT_FROM_LINK_F\000"
	"TO_PKT_FROM_LINK_E\000"
	"TO_PKT_FROM_LINK_D\000"
	"TO_PKT_FROM_LINK_C\000"
	"TO_PKT_FROM_LINK_B\000"
	"TO_PKT_FROM_LINK_A\000"
	"TO_PKT_FROM_LINK_9\000"
	"TO_PKT_FROM_LINK_8\000"
	"LINK_FAILURE\000"
	"WIDGET_PRESENT\000"
	"BITMODE8\000"
	"Cleared by read of XB_LINK_STAT_CLR\000"
	"XB_LINK_IBUF_FLUSH_9\000"
	"XB_LINK_CTRL_9\000"
	"XB_LINK_STAT_9\000"
	"XB_LINK_ARB_UPPER_9\000"
	"XB_LINK_ARB_LOWER_9\000"
	"XB_LINK_STAT_CLR_9\000"
	"XB_LINK_RESET_9\000"
	"XB_LINK_AUX_STAT_9\000"
	"XB_LINK_IBUF_FLUSH_A\000"
	"XB_LINK_CTRL_A\000"
	"XB_LINK_STAT_A\000"
	"XB_LINK_ARB_UPPER_A\000"
	"XB_LINK_ARB_LOWER_A\000"
	"XB_LINK_STAT_CLR_A\000"
	"XB_LINK_RESET_A\000"
	"XB_LINK_AUX_STAT_A\000"
	"XB_LINK_IBUF_FLUSH_B\000"
	"XB_LINK_CTRL_B\000"
	"XB_LINK_STAT_B\000"
	"XB_LINK_ARB_UPPER_B\000"
	"XB_LINK_ARB_LOWER_B\000"
	"XB_LINK_STAT_CLR_B\000"
	"XB_LINK_RESET_B\000"
	"XB_LINK_AUX_STAT_B\000"
	"XB_LINK_IBUF_FLUSH_C\000"
	"XB_LINK_CTRL_C\000"
	"XB_LINK_STAT_C\000"
	"XB_LINK_ARB_UPPER_C\000"
	"XB_LINK_ARB_LOWER_C\000"
	"XB_LINK_STAT_CLR_C\000"
	"XB_LINK_RESET_C\000"
	"XB_LINK_AUX_STAT_C\000"
	"XB_LINK_IBUF_FLUSH_D\000"
	"XB_LINK_CTRL_D\000"
	"XB_LINK_STAT_D\000"
	"XB_LINK_ARB_UPPER_D\000"
	"XB_LINK_ARB_LOWER_D\000"
	"XB_LINK_STAT_CLR_D\000"
	"XB_LINK_RESET_D\000"
	"XB_LINK_AUX_STAT_D\000"
	"XB_LINK_IBUF_FLUSH_E\000"
	"XB_LINK_CTRL_E\000"
	"XB_LINK_STAT_E\000"
	"XB_LINK_ARB_UPPER_E\000"
	"XB_LINK_ARB_LOWER_E\000"
	"XB_LINK_STAT_CLR_E\000"
	"XB_LINK_RESET_E\000"
	"XB_LINK_AUX_STAT_E\000"
	"XB_LINK_IBUF_FLUSH_F\000"
	"XB_LINK_CTRL_F\000"
	"XB_LINK_STAT_F\000"
	"XB_LINK_ARB_UPPER_F\000"
	"XB_LINK_ARB_LOWER_F\000"
	"XB_LINK_STAT_CLR_F\000"
	"XB_LINK_RESET_F\000"
	"XB_LINK_AUX_STAT_F\000"
;

hwreg_field_t hwreg_xbow_fields[] = {
	{ 8, 27, 12, 0, 1, 0x0LL },
	{ 17, 11, 1, 0, 1, 0x0LL },
	{ 24, 0, 0, 11, 1, 0x1LL },
	{ 41, 31, 31, 0, 1, 0x0LL },
	{ 56, 30, 30, 0, 1, 0x0LL },
	{ 71, 29, 29, 0, 1, 0x0LL },
	{ 86, 28, 28, 0, 1, 0x0LL },
	{ 101, 27, 27, 0, 1, 0x0LL },
	{ 116, 26, 26, 0, 1, 0x0LL },
	{ 131, 25, 25, 0, 1, 0x0LL },
	{ 146, 24, 24, 0, 1, 0x0LL },
	{ 161, 23, 23, 0, 1, 0x0LL },
	{ 175, 5, 5, 0, 1, 0x0LL },
	{ 190, 2, 2, 0, 1, 0x0LL },
	{ 200, 0, 0, 0, 1, 0x0LL },
	{ 223, 15, 0, 0, 1, 0x0LL },
	{ 252, 31, 0, 0, 1, 0x0LL },
	{ 275, 5, 5, 6, 1, 0x0LL },
	{ 292, 2, 2, 6, 1, 0x0LL },
	{ 318, 19, 0, 6, 1, 0xfffffLL },
	{ 347, 31, 24, 6, 1, 0x0LL },
	{ 358, 19, 16, 6, 1, 0x0LL },
	{ 373, 15, 0, 6, 1, 0x0LL },
	{ 409, 31, 0, 6, 1, 0x0LL },
	{ 446, 31, 0, 9, 1, 0x0LL },
	{ 529, 25, 16, 6, 1, 0x3ffLL },
	{ 544, 15, 10, 6, 1, 0x6LL },
	{ 552, 9, 0, 6, 1, 0x10LL },
	{ 41, 31, 31, 1, 1, 0x0LL },
	{ 56, 30, 30, 1, 1, 0x0LL },
	{ 71, 29, 29, 1, 1, 0x0LL },
	{ 86, 28, 28, 1, 1, 0x0LL },
	{ 101, 27, 27, 1, 1, 0x0LL },
	{ 116, 26, 26, 1, 1, 0x0LL },
	{ 131, 25, 25, 1, 1, 0x0LL },
	{ 146, 24, 24, 1, 1, 0x0LL },
	{ 161, 23, 23, 1, 1, 0x0LL },
	{ 175, 5, 5, 1, 1, 0x0LL },
	{ 190, 2, 2, 1, 1, 0x0LL },
	{ 200, 0, 0, 1, 1, 0x0LL },
	{ 588, 5, 0, 6, 1, 0x2LL },
	{ 622, 23, 20, 6, 1, 0x8LL },
	{ 634, 19, 0, 0, 1, 0x0LL },
	{ 622, 23, 20, 6, 1, 0x8LL },
	{ 634, 19, 0, 0, 1, 0x0LL },
	{ 668, 19, 10, 6, 1, 0x0LL },
	{ 676, 9, 2, 6, 1, 0x0LL },
	{ 687, 1, 1, 6, 1, 0x1LL },
	{ 700, 0, 0, 6, 1, 0x0LL },
	{ 730, 0, 0, 2, 1, 0x0LL },
	{ 762, 31, 31, 6, 1, 0x0LL },
	{ 779, 29, 28, 6, 1, 0x0LL },
	{ 793, 27, 25, 6, 1, 0x0LL },
	{ 807, 24, 24, 6, 1, 0x0LL },
	{ 818, 23, 23, 6, 1, 0x0LL },
	{ 836, 22, 18, 6, 1, 0x2LL },
	{ 854, 17, 17, 6, 1, 0x0LL },
	{ 873, 16, 16, 6, 1, 0x0LL },
	{ 889, 8, 8, 6, 1, 0x0LL },
	{ 910, 7, 7, 6, 1, 0x0LL },
	{ 936, 6, 6, 6, 1, 0x0LL },
	{ 963, 5, 5, 6, 1, 0x0LL },
	{ 985, 4, 4, 6, 1, 0x0LL },
	{ 1002, 3, 3, 6, 1, 0x0LL },
	{ 1020, 2, 2, 6, 1, 0x0LL },
	{ 1027, 1, 1, 6, 1, 0x0LL },
	{ 1043, 0, 0, 6, 1, 0x0LL },
	{ 1071, 31, 31, 0, 1, 0x0LL },
	{ 200, 18, 18, 0, 1, 0x0LL },
	{ 1082, 17, 17, 0, 1, 0x0LL },
	{ 1095, 16, 16, 0, 1, 0x0LL },
	{ 1105, 15, 15, 0, 1, 0x0LL },
	{ 1122, 14, 14, 0, 1, 0x0LL },
	{ 1139, 13, 13, 0, 1, 0x0LL },
	{ 1156, 12, 12, 0, 1, 0x0LL },
	{ 1173, 11, 11, 0, 1, 0x0LL },
	{ 1190, 10, 10, 0, 1, 0x0LL },
	{ 1207, 9, 9, 0, 1, 0x0LL },
	{ 1224, 8, 8, 0, 1, 0x0LL },
	{ 1241, 7, 7, 0, 1, 0x0LL },
	{ 1262, 6, 6, 0, 1, 0x0LL },
	{ 1284, 5, 5, 0, 1, 0x0LL },
	{ 1300, 4, 4, 0, 1, 0x0LL },
	{ 1311, 3, 3, 0, 1, 0x0LL },
	{ 1323, 1, 1, 0, 1, 0x0LL },
	{ 1333, 0, 0, 0, 1, 0x0LL },
	{ 1360, 31, 29, 6, 1, 0x0LL },
	{ 1366, 28, 24, 6, 1, 0x0LL },
	{ 1372, 23, 21, 6, 1, 0x0LL },
	{ 1378, 20, 16, 6, 1, 0x0LL },
	{ 1384, 15, 13, 6, 1, 0x0LL },
	{ 1390, 12, 8, 6, 1, 0x0LL },
	{ 1396, 7, 5, 6, 1, 0x0LL },
	{ 1402, 4, 0, 6, 1, 0x0LL },
	{ 1428, 31, 29, 6, 1, 0x0LL },
	{ 1434, 28, 24, 6, 1, 0x0LL },
	{ 1440, 23, 21, 6, 1, 0x0LL },
	{ 1446, 20, 16, 6, 1, 0x0LL },
	{ 1452, 15, 13, 6, 1, 0x0LL },
	{ 1458, 12, 8, 6, 1, 0x0LL },
	{ 1464, 7, 5, 6, 1, 0x0LL },
	{ 1470, 4, 0, 6, 1, 0x0LL },
	{ 1071, 31, 31, 1, 1, 0x0LL },
	{ 200, 18, 18, 1, 1, 0x0LL },
	{ 1082, 17, 17, 1, 1, 0x0LL },
	{ 1095, 16, 16, 1, 1, 0x0LL },
	{ 1105, 15, 15, 1, 1, 0x0LL },
	{ 1122, 14, 14, 1, 1, 0x0LL },
	{ 1139, 13, 13, 1, 1, 0x0LL },
	{ 1156, 12, 12, 1, 1, 0x0LL },
	{ 1173, 11, 11, 1, 1, 0x0LL },
	{ 1190, 10, 10, 1, 1, 0x0LL },
	{ 1207, 9, 9, 1, 1, 0x0LL },
	{ 1224, 8, 8, 1, 1, 0x0LL },
	{ 1241, 7, 7, 1, 1, 0x0LL },
	{ 1262, 6, 6, 1, 1, 0x0LL },
	{ 1284, 5, 5, 1, 1, 0x0LL },
	{ 1300, 4, 4, 1, 1, 0x0LL },
	{ 1311, 3, 3, 1, 1, 0x0LL },
	{ 1323, 1, 1, 1, 1, 0x0LL },
	{ 1333, 0, 0, 1, 1, 0x0LL },
	{ 1540, 31, 0, 5, 1, 0x0LL },
	{ 1616, 31, 24, 0, 1, 0x0LL },
	{ 1631, 23, 16, 0, 1, 0x0LL },
	{ 1647, 15, 15, 0, 1, 0x0LL },
	{ 1666, 14, 14, 0, 1, 0x0LL },
	{ 1685, 13, 13, 0, 1, 0x0LL },
	{ 1704, 12, 12, 0, 1, 0x0LL },
	{ 1723, 11, 11, 0, 1, 0x0LL },
	{ 1742, 10, 10, 0, 1, 0x0LL },
	{ 1761, 9, 9, 0, 1, 0x0LL },
	{ 1780, 8, 8, 0, 1, 0x0LL },
	{ 1799, 6, 6, 0, 1, 0x0LL },
	{ 1812, 5, 5, 0, 1, 0x0LL },
	{ 1827, 4, 4, 0, 1, 0x0LL },
};

hwreg_t hwreg_xbow_regs[] = {
	{ 2, 0, 0x4LL, 0, 3 },
	{ 33, 0, 0xcLL, 3, 12 },
	{ 210, 0, 0x14LL, 15, 1 },
	{ 239, 0, 0x1cLL, 16, 1 },
	{ 267, 0, 0x24LL, 17, 2 },
	{ 308, 0, 0x2cLL, 19, 1 },
	{ 334, 0, 0x34LL, 20, 3 },
	{ 396, 0, 0x3cLL, 23, 1 },
	{ 431, 458, 0x44LL, 24, 1 },
	{ 517, 0, 0x4cLL, 25, 3 },
	{ 562, 0, 0x54LL, 28, 12 },
	{ 574, 0, 0x5cLL, 40, 1 },
	{ 608, 0, 0x64LL, 41, 2 },
	{ 647, 0, 0x6cLL, 43, 2 },
	{ 661, 0, 0x74LL, 45, 4 },
	{ 709, 0, 0x104LL, 49, 1 },
	{ 747, 0, 0x10cLL, 50, 17 },
	{ 1056, 0, 0x114LL, 67, 19 },
	{ 1340, 0, 0x11cLL, 86, 8 },
	{ 1408, 0, 0x124LL, 94, 8 },
	{ 1476, 1495, 0x12cLL, 102, 19 },
	{ 1524, 1548, 0x134LL, 121, 1 },
	{ 1597, 1836, 0x13cLL, 122, 13 },
	{ 1872, 0, 0x144LL, 49, 1 },
	{ 1893, 0, 0x14cLL, 50, 17 },
	{ 1908, 0, 0x154LL, 67, 19 },
	{ 1923, 0, 0x15cLL, 86, 8 },
	{ 1943, 0, 0x164LL, 94, 8 },
	{ 1963, 0, 0x16cLL, 102, 19 },
	{ 1982, 0, 0x174LL, 121, 1 },
	{ 1998, 0, 0x17cLL, 122, 13 },
	{ 2017, 0, 0x184LL, 49, 1 },
	{ 2038, 0, 0x18cLL, 50, 17 },
	{ 2053, 0, 0x194LL, 67, 19 },
	{ 2068, 0, 0x19cLL, 86, 8 },
	{ 2088, 0, 0x1a4LL, 94, 8 },
	{ 2108, 0, 0x1acLL, 102, 19 },
	{ 2127, 0, 0x1b4LL, 121, 1 },
	{ 2143, 0, 0x1bcLL, 122, 13 },
	{ 2162, 0, 0x1c4LL, 49, 1 },
	{ 2183, 0, 0x1ccLL, 50, 17 },
	{ 2198, 0, 0x1d4LL, 67, 19 },
	{ 2213, 0, 0x1dcLL, 86, 8 },
	{ 2233, 0, 0x1e4LL, 94, 8 },
	{ 2253, 0, 0x1ecLL, 102, 19 },
	{ 2272, 0, 0x1f4LL, 121, 1 },
	{ 2288, 0, 0x1fcLL, 122, 13 },
	{ 2307, 0, 0x204LL, 49, 1 },
	{ 2328, 0, 0x20cLL, 50, 17 },
	{ 2343, 0, 0x214LL, 67, 19 },
	{ 2358, 0, 0x21cLL, 86, 8 },
	{ 2378, 0, 0x224LL, 94, 8 },
	{ 2398, 0, 0x22cLL, 102, 19 },
	{ 2417, 0, 0x234LL, 121, 1 },
	{ 2433, 0, 0x23cLL, 122, 13 },
	{ 2452, 0, 0x244LL, 49, 1 },
	{ 2473, 0, 0x24cLL, 50, 17 },
	{ 2488, 0, 0x254LL, 67, 19 },
	{ 2503, 0, 0x25cLL, 86, 8 },
	{ 2523, 0, 0x264LL, 94, 8 },
	{ 2543, 0, 0x26cLL, 102, 19 },
	{ 2562, 0, 0x274LL, 121, 1 },
	{ 2578, 0, 0x27cLL, 122, 13 },
	{ 2597, 0, 0x284LL, 49, 1 },
	{ 2618, 0, 0x28cLL, 50, 17 },
	{ 2633, 0, 0x294LL, 67, 19 },
	{ 2648, 0, 0x29cLL, 86, 8 },
	{ 2668, 0, 0x2a4LL, 94, 8 },
	{ 2688, 0, 0x2acLL, 102, 19 },
	{ 2707, 0, 0x2b4LL, 121, 1 },
	{ 2723, 0, 0x2bcLL, 122, 13 },
	{ 2742, 0, 0x2c4LL, 49, 1 },
	{ 2763, 0, 0x2ccLL, 50, 17 },
	{ 2778, 0, 0x2d4LL, 67, 19 },
	{ 2793, 0, 0x2dcLL, 86, 8 },
	{ 2813, 0, 0x2e4LL, 94, 8 },
	{ 2833, 0, 0x2ecLL, 102, 19 },
	{ 2852, 0, 0x2f4LL, 121, 1 },
	{ 2868, 0, 0x2fcLL, 122, 13 },
	{ 0, 0, 0, 0 }
};

hwreg_set_t hwreg_xbow = {
	hwreg_xbow_regs,
	hwreg_xbow_fields,
	hwreg_xbow_stab,
	79,
};
