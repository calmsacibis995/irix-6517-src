/* z8536.h - zilog CIO (counter,timer,parallel i/o chip) */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01g,22sep92,rrr  added support for c++
01f,26may92,rrr  the tree shuffle
01e,04oct91,rrr  passed through the ansification filter
		  -changed TINY and UTINY to INT8 and UINT8
		  -changed copyright notice
01d,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01c,10jan90,shl  merged with z8036.h.
01b,11sep89,dab  changed U8 to UINT8.
01a,10aug88,rml  written
*/

/* Note: in the following, the value '0x00' has been replaced by 'ZERO'.
   This has been done to provide a way to avoid the 'clr.b' instruction
   on the 68000/10 cpu. 'ZERO' should be defined to be a memory location
   which holds the value '0x00', this will cause compilers to generate
   memory to memory transfers rather than clear instructions. */

#ifndef __INCz8536h
#define __INCz8536h

#ifdef __cplusplus
extern "C" {
#endif

/* internal register addresses */

#define  ZCIO_MIC          ((UINT8) ZERO)  /* Master Interrupt Control */
#define  ZCIO_MCC          ((UINT8) 0x01)  /* Master Configuration Control */
#define  ZCIO_PAIV         ((UINT8) 0x02)  /* Port A Interrupt Vector */
#define  ZCIO_PBIV         ((UINT8) 0x03)  /* Port B Interrupt Vector */
#define  ZCIO_CTIV         ((UINT8) 0x04)  /* Counter/Timer Interrupt Vector */

#define  ZCIO_PCDPP        ((UINT8) 0x05)  /* Port C Data Path Polarity */
#define  ZCIO_PCDD         ((UINT8) 0x06)  /* Port C Data Direction */
#define  ZCIO_PCSIOC       ((UINT8) 0x07)  /* Port C Special I/O Control */
#define  ZCIO_PACS         ((UINT8) 0x08)  /* Port A Command and Status */
#define  ZCIO_PBCS         ((UINT8) 0x09)  /* Port B Command and Status */

#define  ZCIO_CT1CS        ((UINT8) 0x0a)  /* C/T 1 Command and Status */
#define  ZCIO_CT2CS        ((UINT8) 0x0b)  /* C/T 2 Command and Status */
#define  ZCIO_CT3CS        ((UINT8) 0x0c)  /* C/T 3 Command and Status */

#define  ZCIO_PAD          ((UINT8) 0x0d)  /* Port A Data */
#define  ZCIO_PBD          ((UINT8) 0x0e)  /* Port B Data */
#define  ZCIO_PCD          ((UINT8) 0x0f)  /* Port C Data */

#define  ZCIO_CT1CCMSB     ((UINT8) 0x10)  /* C/T 1 Current Count (MS Byte) */
#define  ZCIO_CT1CCLSB     ((UINT8) 0x11)  /* C/T 1 Current Count (LS Byte) */
#define  ZCIO_CT2CCMSB     ((UINT8) 0x12)  /* C/T 2 Current Count (MS Byte) */
#define  ZCIO_CT2CCLSB     ((UINT8) 0x13)  /* C/T 2 Current Count (LS Byte) */
#define  ZCIO_CT3CCMSB     ((UINT8) 0x14)  /* C/T 3 Current Count (MS Byte) */
#define  ZCIO_CT3CCLSB     ((UINT8) 0x15)  /* C/T 3 Current Count (LS Byte) */

#define  ZCIO_CT1TCMSB     ((UINT8) 0x16)  /* C/T 1 Time Constant (MS Byte) */
#define  ZCIO_CT1TCLSB     ((UINT8) 0x17)  /* C/T 1 Time Constant (LS Byte) */
#define  ZCIO_CT2TCMSB     ((UINT8) 0x18)  /* C/T 2 Time Constant (MS Byte) */
#define  ZCIO_CT2TCLSB     ((UINT8) 0x19)  /* C/T 2 Time Constant (LS Byte) */
#define  ZCIO_CT3TCMSB     ((UINT8) 0x1a)  /* C/T 3 Time Constant (MS Byte) */
#define  ZCIO_CT3TCLSB     ((UINT8) 0x1b)  /* C/T 3 Time Constant (LS Byte) */

#define  ZCIO_CT1MS        ((UINT8) 0x1c)  /* C/T 1 Mode Specification */
#define  ZCIO_CT2MS        ((UINT8) 0x1d)  /* C/T 2 Mode Specification */
#define  ZCIO_CT3MS        ((UINT8) 0x1e)  /* C/T 3 Mode Specification */

#define  ZCIO_CV           ((UINT8) 0x1f)  /* Current Vector */

#define  ZCIO_PAMS         ((UINT8) 0x20)  /* Port A Mode Specification */
#define  ZCIO_PAHS         ((UINT8) 0x21)  /* Port A Handshake Specification */
#define  ZCIO_PADPP        ((UINT8) 0x22)  /* Port A Data Path Polarity */
#define  ZCIO_PADD         ((UINT8) 0x23)  /* Port A Data Direction */
#define  ZCIO_PASIOC       ((UINT8) 0x24)  /* Port A Special I/O Control */
#define  ZCIO_PAPP         ((UINT8) 0x25)  /* Port A Pattern Polarity */
#define  ZCIO_PAPT         ((UINT8) 0x26)  /* Port A Pattern Transition */
#define  ZCIO_PAPM         ((UINT8) 0x27)  /* Port A Pattern Mask */

#define  ZCIO_PBMS         ((UINT8) 0x28)  /* Port B Mode Specification */
#define  ZCIO_PBHS         ((UINT8) 0x29)  /* Port B Handshake Specification */
#define  ZCIO_PBDPP        ((UINT8) 0x2a)  /* Port B Data Path Polarity */
#define  ZCIO_PBDD         ((UINT8) 0x2b)  /* Port B Data Direction */
#define  ZCIO_PBSIOC       ((UINT8) 0x2c)  /* Port B Special I/O Control */
#define  ZCIO_PBPP         ((UINT8) 0x2d)  /* Port B Pattern Polarity */
#define  ZCIO_PBPT         ((UINT8) 0x2e)  /* Port B Pattern Transition */
#define  ZCIO_PBPM         ((UINT8) 0x2f)  /* Port B Pattern Mask */


/* If a particular CPU board uses Zilog 8036 counter/timer/parallel I/O chip,
 * Z8036 should be defined in {target}/sysLib.h. */

#ifdef Z8036

#define  Z8036_MIC(base)          ((char *) ((base) + ZCIO_MIC))
#define  Z8036_MCC(base)          ((char *) ((base) + ZCIO_MCC))
#define  Z8036_PAIV(base)         ((char *) ((base) + ZCIO_PAIV))
#define  Z8036_PBIV(base)         ((char *) ((base) + ZCIO_PBIV))
#define  Z8036_CTIV(base)         ((char *) ((base) + ZCIO_CTIV))
#define  Z8036_PCDPP(base)        ((char *) ((base) + ZCIO_PCDPP))
#define  Z8036_PCDD(base)         ((char *) ((base) + ZCIO_PCDD))
#define  Z8036_PCSIOC(base)       ((char *) ((base) + ZCIO_PCSIOC))
#define  Z8036_PACS(base)         ((char *) ((base) + ZCIO_PACS))
#define  Z8036_PBCS(base)         ((char *) ((base) + ZCIO_PBCS))
#define  Z8036_CT1CS(base)        ((char *) ((base) + ZCIO_CT1CS))
#define  Z8036_CT2CS(base)        ((char *) ((base) + ZCIO_CT2CS))
#define  Z8036_CT3CS(base)        ((char *) ((base) + ZCIO_CT3CS))
#define  Z8036_PAD(base)          ((char *) ((base) + ZCIO_PAD))
#define  Z8036_PBD(base)          ((char *) ((base) + ZCIO_PBD))
#define  Z8036_PCD(base)          ((char *) ((base) + ZCIO_PCD))
#define  Z8036_CT1CCMSB(base)     ((char *) ((base) + ZCIO_CT1CCMSB))
#define  Z8036_CT1CCLSB(base)     ((char *) ((base) + ZCIO_CT1CCLSB))
#define  Z8036_CT2CCMSB(base)     ((char *) ((base) + ZCIO_CT2CCMSB))
#define  Z8036_CT2CCLSB(base)     ((char *) ((base) + ZCIO_CT2CCLSB))
#define  Z8036_CT3CCMSB(base)     ((char *) ((base) + ZCIO_CT3CCMSB))
#define  Z8036_CT3CCLSB(base)     ((char *) ((base) + ZCIO_CT3CCLSB))
#define  Z8036_CT1TCMSB(base)     ((char *) ((base) + ZCIO_CT1TCMSB))
#define  Z8036_CT1TCLSB(base)     ((char *) ((base) + ZCIO_CT1TCLSB))
#define  Z8036_CT2TCMSB(base)     ((char *) ((base) + ZCIO_CT2TCMSB))
#define  Z8036_CT2TCLSB(base)     ((char *) ((base) + ZCIO_CT2TCLSB))
#define  Z8036_CT3TCMSB(base)     ((char *) ((base) + ZCIO_CT3TCMSB))
#define  Z8036_CT3TCLSB(base)     ((char *) ((base) + ZCIO_CT3TCLSB))
#define  Z8036_CT1MS(base)        ((char *) ((base) + ZCIO_CT1MS))
#define  Z8036_CT2MS(base)        ((char *) ((base) + ZCIO_CT2MS))
#define  Z8036_CT3MS(base)        ((char *) ((base) + ZCIO_CT3MS))
#define  Z8036_CV(base)           ((char *) ((base) + ZCIO_CV))
#define  Z8036_PAMS(base)         ((char *) ((base) + ZCIO_PAMS))
#define  Z8036_PAHS(base)         ((char *) ((base) + ZCIO_PAHS))
#define  Z8036_PADPP(base)        ((char *) ((base) + ZCIO_PADPP))
#define  Z8036_PADD(base)         ((char *) ((base) + ZCIO_PADD))
#define  Z8036_PASIOC(base)       ((char *) ((base) + ZCIO_PASIOC))
#define  Z8036_PAPP(base)         ((char *) ((base) + ZCIO_PAPP))
#define  Z8036_PAPT(base)         ((char *) ((base) + ZCIO_PAPT))
#define  Z8036_PAPM(base)         ((char *) ((base) + ZCIO_PAPM))
#define  Z8036_PBMS(base)         ((char *) ((base) + ZCIO_PBMS))
#define  Z8036_PBHS(base)         ((char *) ((base) + ZCIO_PBHS))
#define  Z8036_PBDPP(base)        ((char *) ((base) + ZCIO_PBDPP))
#define  Z8036_PBDD(base)         ((char *) ((base) + ZCIO_PBDD))
#define  Z8036_PBSIOC(base)       ((char *) ((base) + ZCIO_PBSIOC))
#define  Z8036_PBPP(base)         ((char *) ((base) + ZCIO_PBPP))
#define  Z8036_PBPT(base)         ((char *) ((base) + ZCIO_PBPT))
#define  Z8036_PBPM(base)         ((char *) ((base) + ZCIO_PBPM))

#endif	/* Z8036 */


/*  Counter/Timer Mode Specification register values (C/T 1, 2 and 3) */

#define  ZCIO_CTMS_DCS_PO  ((UINT8) ZERO)  /* Pulse Output */
#define  ZCIO_CTMS_DCS_OSO ((UINT8) 0x01)  /* One-Shot Output */
#define  ZCIO_CTMS_DCS_SWO ((UINT8) 0x02)  /* Square-Wave Output */
#define  ZCIO_CTMS_DCS     ((UINT8) 0x03)  /* output Duty Cycle Selects */
#define  ZCIO_CTMS_REB     ((UINT8) 0x04)  /* Retrigger Enable Bit */
#define  ZCIO_CTMS_EGE     ((UINT8) 0x08)  /* External Gate Enable */
#define  ZCIO_CTMS_ETE     ((UINT8) 0x10)  /* External Trigger Enable */
#define  ZCIO_CTMS_ECE     ((UINT8) 0x20)  /* External Count Enable */
#define  ZCIO_CTMS_EOE     ((UINT8) 0x40)  /* External Output Enable */
#define  ZCIO_CTMS_CSC     ((UINT8) 0x80)  /* Continuous Single Cycle */


/* Master Configuration Control register values */

#define  ZCIO_MCC_PLC_IND  ((UINT8) ZERO)  /* port A and B INDependent */
#define  ZCIO_MCC_LC_IND   ((UINT8) ZERO)  /* counter/timers INDependent */
#define  ZCIO_MCC_LC_GATE  ((UINT8) 0x01)  /* ct1 output GATEs ct2 */
#define  ZCIO_MCC_LC_TRIG  ((UINT8) 0x02)  /* ct1 output TRIGgers ct2 */
#define  ZCIO_MCC_LC_CNT   ((UINT8) 0x03)  /* ct1 output is ct2 CouNT input */
#define  ZCIO_MCC_LC       ((UINT8) 0x03)  /* counter/timer Link Controls */
#define  ZCIO_MCC_PAE      ((UINT8) 0x04)  /* Port A Enable */
#define  ZCIO_MCC_PLC      ((UINT8) 0x08)  /* Port Link Control */
#define  ZCIO_MCC_PLC_LNK  ((UINT8) 0x08)  /* port A and B are LiNKed */
#define  ZCIO_MCC_CT3PCE   ((UINT8) 0x10)  /* C/T 3 and Port C Enable */
#define  ZCIO_MCC_CT2E     ((UINT8) 0x20)  /* Counter/Timer 2 Enable */
#define  ZCIO_MCC_CT1E     ((UINT8) 0x40)  /* Counter/Timer 1 Enable */
#define  ZCIO_MCC_PBE      ((UINT8) 0x80)  /* Port B Enable */


/* Master Interrupt Control register values */

#define  ZCIO_MIC_RESET    ((UINT8) 0x01)  /* device RESET */
#define  ZCIO_MIC_RJ       ((UINT8) 0x02)  /* Right Just */
#define  ZCIO_MIC_CTVIS    ((UINT8) 0x04)  /* C/T Vector Includes Status */
#define  ZCIO_MIC_PBVIS    ((UINT8) 0x08)  /* Port B Vector Includes Status */
#define  ZCIO_MIC_PAVIS    ((UINT8) 0x10)  /* Port A Vector Includes Status */
#define  ZCIO_MIC_NV       ((UINT8) 0x20)  /* No Vector */
#define  ZCIO_MIC_DLC      ((UINT8) 0x40)  /* Disable Lower Chain */
#define  ZCIO_MIC_MIE      ((UINT8) 0x80)  /* Master Interrupt Enable */


/* Command and Status resigter values (port A and B and C/T 1, 2 and 3) */

#define  ZCIO_CS_NC        ((UINT8) ZERO)  /* Null Code */
#define  ZCIO_CS_CIP       ((UINT8) 0x01)  /* Count In Progress */
#define  ZCIO_CS_IOE       ((UINT8) 0x01)  /* Interrupt On Error */
#define  ZCIO_CS_TCB       ((UINT8) 0x02)  /* Trigger Command Bit */
#define  ZCIO_CS_PMF       ((UINT8) 0x02)  /* Pattern Match Flag */
#define  ZCIO_CS_IRF       ((UINT8) 0x04)  /* Input Register Full */
#define  ZCIO_CS_GCB       ((UINT8) 0x04)  /* Gate Command Bit */
#define  ZCIO_CS_RCC       ((UINT8) 0x08)  /* Read Counter Control */
#define  ZCIO_CS_ORE       ((UINT8) 0x08)  /* Output Register Empty */
#define  ZCIO_CS_ERR       ((UINT8) 0x10)  /* interrupt ERRor */
#define  ZCIO_CS_IP        ((UINT8) 0x20)  /* Interrupt Pending */
#define  ZCIO_CS_CLIPIUS   ((UINT8) 0x20)  /* CLear IP and IUS */
#define  ZCIO_CS_IE        ((UINT8) 0x40)  /* Interrupt Enable */
#define  ZCIO_CS_SIUS      ((UINT8) 0x40)  /* Set Interrupt Under Service */
#define  ZCIO_CS_CLIUS     ((UINT8) 0x60)  /* CLear Interrupt Under Service */
#define  ZCIO_CS_IUS       ((UINT8) 0x80)  /* Interrupt Under Service */
#define  ZCIO_CS_SIP       ((UINT8) 0x80)  /* Set Interrupt Pending */
#define  ZCIO_CS_CLIP      ((UINT8) 0xa0)  /* CLear Interrupt Pending */
#define  ZCIO_CS_SIE       ((UINT8) 0xc0)  /* Set Interrupt Enable */
#define  ZCIO_CS_CLIE      ((UINT8) 0xe0)  /* CLear Interrupt Enable */


/* Data Direction register values (port A, B and C) */

#define  ZCIO_DD_OUT       ((UINT8) ZERO)  /* OUTput bit */
#define  ZCIO_DD_IN        ((UINT8) 0xff)  /* INput bit */


/* Data Path Polarity register values (port A, B and C) */

#define  ZCIO_DPP_NONINV   ((UINT8) ZERO)  /* NON-INVerting */
#define  ZCIO_DPP_INVERT   ((UINT8) 0xff)  /* INVerting */


/* Port Handshake Specification register values (port A and B) */

#define  ZCIO_PHS_RWS_RWD  ((UINT8) ZERO)  /* Request/Wait Disabled */
#define  ZCIO_PHS_DTS_2    ((UINT8) ZERO)  /* 2 PCLK cycles */
#define  ZCIO_PHS_HTS_IH   ((UINT8) ZERO)  /* Interlocked Handshake */
#define  ZCIO_PHS_DTS_4    ((UINT8) 0x01)  /* 4 PCLK cycles */
#define  ZCIO_PHS_DTS_6    ((UINT8) 0x02)  /* 6 PCLK cycles */
#define  ZCIO_PHS_DTS_8    ((UINT8) 0x03)  /* 8 PCLK cycles */
#define  ZCIO_PHS_DTS_10   ((UINT8) 0x04)  /* 10 PCLK cycles */
#define  ZCIO_PHS_DTS_12   ((UINT8) 0x05)  /* 12 PCLK cycles */
#define  ZCIO_PHS_DTS_14   ((UINT8) 0x06)  /* 14 PCLK cycles */
#define  ZCIO_PHS_DTS      ((UINT8) 0x07)  /* Deskew Time Specification */
#define  ZCIO_PHS_DTS_16   ((UINT8) 0x07)  /* 16 PCLK cycles */
#define  ZCIO_PHS_RWS_OW   ((UINT8) 0x08)  /* Output Wait */
#define  ZCIO_PHS_RWS_IW   ((UINT8) 0x18)  /* Input Wait */
#define  ZCIO_PHS_RWS_SR   ((UINT8) 0x20)  /* Special Request */
#define  ZCIO_PHS_RWS_OR   ((UINT8) 0x28)  /* Output Request */
#define  ZCIO_PHS_RWS_IR   ((UINT8) 0x38)  /* Input Request */
#define  ZCIO_PHS_RWS      ((UINT8) 0x38)  /* Request/Wait Specification */
#define  ZCIO_PHS_HTS_SH   ((UINT8) 0x40)  /* Strobed Handshake */
#define  ZCIO_PHS_HTS_PH   ((UINT8) 0x80)  /* Pulsed Handshake */
#define  ZCIO_PHS_HTS_3WH  ((UINT8) 0xc0)  /* 3-Wire Handshake */
#define  ZCIO_PHS_HTS      ((UINT8) 0xc0)  /* Handshake Type Specification */


/* Interrupt Vector register values (port A and B and C/Ts) */

#define  ZCIO_IV_ERR       ((UINT8) ZERO)  /* Error */
#define  ZCIO_IV_CT3       ((UINT8) ZERO)  /* Counter/Timer 3 */
#define  ZCIO_IV_CT2       ((UINT8) 0x02)  /* Counter/Timer 2 */
#define  ZCIO_IV_PMF       ((UINT8) 0x02)  /* Pattern Match Flag */
#define  ZCIO_IV_IRF       ((UINT8) 0x04)  /* Input Register Full */
#define  ZCIO_IV_CT1       ((UINT8) 0x04)  /* Counter/Timer 1 */
#define  ZCIO_IV_CTE       ((UINT8) 0x06)  /* Counter/Timer Error */
#define  ZCIO_IV_ORE       ((UINT8) 0x08)  /* Output Register Empty */


/* Port Mode Specification register values (port A and B) */

#define  ZCIO_PMS_PTS_BIT  ((UINT8) ZERO)  /* BIT port */
#define  ZCIO_PMS_PMS_DPM  ((UINT8) ZERO)  /* Disable Pattern Match */
#define  ZCIO_PMS_LPM      ((UINT8) 0x01)  /* Latch on Pattern Match */
#define  ZCIO_PMS_DTE      ((UINT8) 0x01)  /* Deskew Timer Enable */
#define  ZCIO_PMS_PMS_AND  ((UINT8) 0x02)  /* "AND" mode */
#define  ZCIO_PMS_PMS_OR   ((UINT8) 0x04)  /* "OR" mode */
#define  ZCIO_PMS_PMS      ((UINT8) 0x06)  /* Pattern Mode Specification */
#define  ZCIO_PMS_PMS_PEV  ((UINT8) 0x06)  /* "Priority Encoded Vector" mode */
#define  ZCIO_PMS_IMO      ((UINT8) 0x08)  /* Interrupt on Match Only */
#define  ZCIO_PMS_SB       ((UINT8) 0x10)  /* Single Buffered mode */
#define  ZCIO_PMS_ITB      ((UINT8) 0x20)  /* Interrupt on Two Bytes */
#define  ZCIO_PMS_PTS_IN   ((UINT8) 0x40)  /* INput port */
#define  ZCIO_PMS_PTS_OUT  ((UINT8) 0x80)  /* OUTput port */
#define  ZCIO_PMS_PTS_BI   ((UINT8) 0xc0)  /* BIdirectal port */
#define  ZCIO_PMS_PTS      ((UINT8) 0xc0)  /* Port Type Select */


/* Pattern Mask resigter values (port A and B) */

#define  ZCIO_PM_OFF       ((UINT8) ZERO)  /* Bit Masked Off (X) */
#define  ZCIO_PM_ANY       ((UINT8) ZERO)  /* Any Transition (^ or v) */
#define  ZCIO_PM_ZERO      ((UINT8) 0xff)  /* Zero (0) */
#define  ZCIO_PM_ONE       ((UINT8) 0xff)  /* One (1) */
#define  ZCIO_PM_0TO1      ((UINT8) 0xff)  /* Zero to One Transition (^) */
#define  ZCIO_PM_1TO0      ((UINT8) 0xff)  /* One to Zero Transition (v) */

/* Pattern Polarity register values (port A and B) */

#define  ZCIO_PP_OFF       ((UINT8) ZERO)  /* Bit Masked Off (X) */
#define  ZCIO_PP_ANY       ((UINT8) ZERO)  /* Any Transition (^ or v) */
#define  ZCIO_PP_ZERO      ((UINT8) ZERO)  /* Zero (0) */
#define  ZCIO_PP_1TO0      ((UINT8) ZERO)  /* One to Zero Transition (v) */
#define  ZCIO_PP_ONE       ((UINT8) 0xff)  /* One (1) */
#define  ZCIO_PP_0TO1      ((UINT8) 0xff)  /* Zero to One Transition (^) */

/* Pattern Transition register values (port A and B) */

#define  ZCIO_PT_OFF       ((UINT8) ZERO)  /* Bit Masked Off (X) */
#define  ZCIO_PT_ZERO      ((UINT8) ZERO)  /* Zero (0) */
#define  ZCIO_PT_ONE       ((UINT8) ZERO)  /* One (1) */
#define  ZCIO_PT_ANY       ((UINT8) 0xff)  /* Any Transition (^ or v) */
#define  ZCIO_PT_1TO0      ((UINT8) 0xff)  /* One to Zero Transition (v) */
#define  ZCIO_PT_0TO1      ((UINT8) 0xff)  /* Zero to One Transition (^) */

/* Special I/O Control register values (port A B and C) */

#define  ZCIO_SIO_NORMAL   ((UINT8) ZERO)  /* NORMAL input or output */
#define  ZCIO_SIO_ODRAIN   ((UINT8) 0xff)  /* output with Open DRAIN */
#define  ZCIO_SIO_1CATCH   ((UINT8) 0xff)  /* input with 1's CATCHer */

#ifdef __cplusplus
}
#endif

#endif /* __INCz8536h */
