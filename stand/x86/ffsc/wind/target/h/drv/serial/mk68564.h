/* mk68564.h - MK68564 Serial I/O Chip header */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01e,22sep92,rrr  added support for c++
01d,26may92,rrr  the tree shuffle
01c,04oct91,rrr  passed through the ansification filter
		  -changed TINY and UTINY to INT8 and UINT8
		  -changed copyright notice
01b,05oct90,shl  added copyright notice.
01a,10sep87,dnw  written
*/

#ifndef __INCmk68564h
#define __INCmk68564h

#ifdef __cplusplus
extern "C" {
#endif


/* interrupt vector offsets in "status affects vector mode" */

#define MK564_IV_XMIT_B		0
#define MK564_IV_EXT_B		1
#define MK564_IV_RCV_B		2
#define MK564_IV_SPEC_B		3
#define MK564_IV_XMIT_A		4
#define MK564_IV_EXT_A		5
#define MK564_IV_RCV_A		6
#define MK564_IV_SPEC_A		7


/* registers */

#define MK564_COMMAND_REG(base)		((UINT8 *) (base) + 0)
#define MK564_MODE_CTRL_REG(base)	((UINT8 *) (base) + 1)
#define MK564_INT_CTRL_REG(base)	((UINT8 *) (base) + 2)
#define MK564_SYNC_WORD_1_REG(base)	((UINT8 *) (base) + 3)
#define MK564_SYNC_WORD_2_REG(base)	((UINT8 *) (base) + 4)
#define MK564_RCV_CTRL_REG(base)	((UINT8 *) (base) + 5)
#define MK564_XMIT_CTRL_REG(base)	((UINT8 *) (base) + 6)
#define MK564_STATUS_0_REG(base)	((UINT8 *) (base) + 7)
#define MK564_STATUS_1_REG(base)	((UINT8 *) (base) + 8)
#define MK564_DATA_REG(base)		((UINT8 *) (base) + 9)
#define MK564_TIME_CONST_REG(base)	((UINT8 *) (base) + 10)
#define MK564_BAUD_REG(base)		((UINT8 *) (base) + 11)
#define MK564_INT_VECTOR_REG(base)	((UINT8 *) (base) + 12)


/* command register */

#define MK564_CMD_RESET_RCV_CRC		0x40
#define MK564_CMD_RESET_XMIT_CRC	0x80
#define MK564_CMD_RESET_UNDERRUN	0xc0

#define MK564_CMD_SEND_ABORT		0x08
#define MK564_CMD_RESET_EXT_INT		0x10
#define MK564_CMD_CHANNEL_RESET		0x18
#define MK564_CMD_ENABLE_RCV_INT	0x20
#define MK564_CMD_RESET_XMIT_INT	0x28
#define MK564_CMD_ERROR_RESET		0x30


/* mode control register */

#define MK564_MODE_CLK_X1		0x00
#define MK564_MODE_CLK_X16		0x40
#define MK564_MODE_CLK_X32		0x80
#define MK564_MODE_CLK_X64		0xc0

#define MK564_MODE_SYNC_8BIT_PROG	0x00
#define MK564_MODE_SYNC_16BIT_PROG	0x10
#define MK564_MODE_SYNC_SDLC_MODE	0x20
#define MK564_MODE_SYNC_EXTERNAL	0x30

#define MK564_MODE_STOP_1		0x04
#define MK564_MODE_STOP_1_HALF		0x08
#define MK564_MODE_STOP_2		0x0c

#define MK564_MODE_PARITY_EVEN		0x02
#define MK564_MODE_PARITY_ENABLE	0x01


/* interrupt control register */

#define MK564_INT_CRC_16		0x80
#define MK564_INT_XMIT_RDY_ENABLE	0x40
#define MK564_INT_RCV_RDY_ENABLE	0x20

#define MK564_INT_ON_1ST_RCV_CHAR	0x08
#define MK564_INT_ON_EACH_RCV_CHAR_PARITY	0x10
#define MK564_INT_ON_EACH_RCV_CHAR	0x18

#define MK564_INT_STATUS_AFFECTS_VECTOR	0x04
#define MK564_INT_XMIT_INT_ENABLE	0x02
#define MK564_INT_EXT_INT_ENABLE	0x01


/* receiver control register */

#define MK564_RCV_5_BITS		0x00
#define MK564_RCV_6_BITS		0x40
#define MK564_RCV_7_BITS		0x80
#define MK564_RCV_8_BITS		0xc0

#define MK564_RCV_AUTO_ENABLES		0x20
#define MK564_RCV_HUNT_MODE		0x10
#define MK564_RCV_CRC_ENABLE		0x08
#define MK564_RCV_ADDRESS_SEARCH	0x04
#define MK564_RCV_STRIP_SYNC		0x02
#define MK564_RCV_ENABLE		0x01


/* transmitter control register */

#define MK564_XMIT_5_BITS		0x00
#define MK564_XMIT_6_BITS		0x40
#define MK564_XMIT_7_BITS		0x80
#define MK564_XMIT_8_BITS		0xc0

#define MK564_XMIT_AUTO_ENABLES		0x20
#define MK564_XMIT_SEND_BREAK		0x10
#define MK564_XMIT_CRC_ENABLE		0x08
#define MK564_XMIT_DTR			0x04
#define MK564_XMIT_RTS			0x02
#define MK564_XMIT_ENABLE		0x01


/* status register 0 */

#define MK564_ST0_BREAK			0x80
#define MK564_ST0_UNDERRUN		0x40
#define MK564_ST0_CTS			0x20
#define MK564_ST0_HUNT			0x10
#define MK564_ST0_DCD			0x08
#define MK564_ST0_XMIT_BUF_EMPTY	0x04
#define MK564_ST0_INT_PENDING		0x02
#define MK564_ST0_RCV_CHAR_AVAIL	0x01


/* status register 1 */

#define MK564_ST1_END_OF_FRAME		0x80
#define MK564_ST1_CRC_ERROR		0x40
#define MK564_ST1_RCV_OVERRUN		0x20
#define MK564_ST1_PARITY_ERROR		0x10
#define MK564_ST1_RESIDUE_MASK		0x0e
#define MK564_ST1_ALL_SENT		0x01


/* baud rate control register */

#define MK564_BAUD_RCV_CLK_INTERNAL	0x08	/* else external */
#define MK564_BAUD_XMIT_CLK_INTERNAL	0x04	/* else external */
#define MK564_BAUD_DIVIDE_BY_64		0x02	/* else 4 */
#define MK564_BAUD_GEN_ENABLE		0x01

#ifdef __cplusplus
}
#endif

#endif /* __INCmk68564h */
