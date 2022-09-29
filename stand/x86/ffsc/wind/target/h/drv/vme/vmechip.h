/* vmechip.h - Motorola MVME6000 VMEbus Interface Controller */

/* Copyright 1991-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01e,22sep92,rrr  added support for c++
01d,03jun92,ccc  added a few more constants.
01c,26may92,rrr  the tree shuffle
01b,02jan92,caf  fixed GLOBAL_0 bits. changed copyright notice.
01a,19dec91,caf  written.
*/

#ifdef	DOC
#define INCvmechiph
#endif	/* DOC */

#ifndef __INCvmechiph
#define __INCvmechiph

#ifdef __cplusplus
extern "C" {
#endif

/*
This file contains constants for the Motorola MVME6000 VMEbus Interface.
The macro VMECHIP_BASE_ADRS must be defined when using local bus addresses
defined in this header.  The macro GCSR_BUS_BASE_ADRS must be defined when
using VMEbus addresses defined in this header.

The following standards have been adopted in the creation of this file.

Registers in the LCSR have the prefix "LCSR".

Registers in the GCSR have the prefix "GCSR" when addressed locally and the
prefix "GCSR_BUS" when addressed from the VMEbus.  The bit definitions are
shared.

The registers are listed in ascending (numerical) order; the definitions
for each register start with a header, e.g.:

 Register                Register Register Register
 Mnemonic                Number   Address  Name
    |                        |       |       |
    v                        v       v       v
 ************************************************************************
* SYS_CONF                  0x00    0x01    system controller config    *
*************************************************************************

In cases where a number of registers have the same definitions, the
header looks like this:

 ************************************************************************
* GP_CSR_0                  0x13    0x27    general purpose CSR 0       *
* to                                                                    *
* GP_CSR_4                  0x17    0x2f    general purpose CSR 4       *
*************************************************************************

The format of the definitions is as follows.  The define name always
starts with the register mnemonic it is associated with.  The [7-0]
number at the end of the comment indicates the bit position to which the
the define applies.  In the definition of RQST_CONF_DWB, for example,
the 7 indicates bit 7:
                                                                      |
                                                                      v
 define RQST_CONF_DWB       0x80     * device wants bus               7 *

If the define applies to more than one bit, then the applicable bit
range is specified by two digits.  In the following example,
TMO_CONF_VBTO_2048 applies to the two bit field, bits 5-4:
                                                                      |
                                                                      v
 define TMO_CONF_VBTO_2048  0x10     * VMEbus time-out 2048 * T      54 *

If no bit field is given, then the define applies to the whole register.
*/

#ifdef	_ASMLANGUAGE
#define	CAST
#else
#define	CAST (char *)
#endif	/* _ASMLANGUAGE */

#define	VMECHIP_REG_INTERVAL            2

#ifndef	VMECHIP_ADRS    /* to permit alternative board addressing */
#define	VMECHIP_ADRS(reg) \
                (CAST (VMECHIP_BASE_ADRS + 1 + (reg * VMECHIP_REG_INTERVAL)))
#endif	/* VMECHIP_ADRS */

/*
 * Local Control and Status Register (LCSR) definitions.
 *
 * Only the local CPU can access the LCSR.
 */

#define LCSR_SYS_CONF       VMECHIP_ADRS(0x00)  /* system controller config  */
#define LCSR_RQST_CONF      VMECHIP_ADRS(0x01)  /* requester configuration   */
#define LCSR_MASTER_CONF    VMECHIP_ADRS(0x02)  /* master configuration      */
#define LCSR_SLAVE_CONF     VMECHIP_ADRS(0x03)  /* slave configuration       */
#define LCSR_TMO_CONF       VMECHIP_ADRS(0x04)  /* timer configuration       */
#define LCSR_SLAVE_AM       VMECHIP_ADRS(0x05)  /* slave address modifier    */
#define LCSR_MASTER_AM      VMECHIP_ADRS(0x06)  /* master address modifier   */
#define LCSR_INT_MASK       VMECHIP_ADRS(0x07)  /* interrupt handler mask    */
#define LCSR_UTIL_INT_MASK  VMECHIP_ADRS(0x08)  /* utility interrupt mask    */
#define LCSR_UTIL_VEC_BASE  VMECHIP_ADRS(0x09)  /* utility interrupt vector  */
#define LCSR_INT_REQ        VMECHIP_ADRS(0x0a)  /* interrupt request         */
#define LCSR_INT_STATUS_ID  VMECHIP_ADRS(0x0b)  /* status/ID                 */
#define LCSR_BERR_STATUS    VMECHIP_ADRS(0x0c)  /* bus error status          */
#define LCSR_GCSR_BASE_ADRS VMECHIP_ADRS(0x0d)  /* GCSR base address         */


/*
 * Global Control and Status Register (GCSR) definitions.
 *
 * All registers of the GCSR are accessible to both the local processor
 * and to other VMEbus masters.
 */

/* local addresses for the GCSR */

#define GCSR_GLOBAL_0       VMECHIP_ADRS(0x10)  /* global 0                  */
#define GCSR_GLOBAL_1       VMECHIP_ADRS(0x11)  /* global 1                  */
#define GCSR_BOARD_ID       VMECHIP_ADRS(0x12)  /* board ID                  */
#define GCSR_GP_CSR_0       VMECHIP_ADRS(0x13)  /* general purpose CSR 0     */
#define GCSR_GP_CSR_1       VMECHIP_ADRS(0x14)  /* general purpose CSR 1     */
#define GCSR_GP_CSR_2       VMECHIP_ADRS(0x15)  /* general purpose CSR 2     */
#define GCSR_GP_CSR_3       VMECHIP_ADRS(0x16)  /* general purpose CSR 3     */
#define GCSR_GP_CSR_4       VMECHIP_ADRS(0x17)  /* general purpose CSR 4     */

/* VMEbus addresses for the GCSR */

#define GCSR_BUS_REG_INTERVAL           2

#ifndef GCSR_BUS_ADRS   /* to permit alternative off-board addressing */
#define GCSR_BUS_ADRS(reg) \
                (CAST (GCSR_BUS_BASE_ADRS + 1 + (reg * GCSR_BUS_REG_INTERVAL)))
#endif  /* GCSR_BUS_ADRS */

#define GCSR_BUS_GLOBAL_0   GCSR_BUS_ADRS(0x00) /* global 0 on VMEbus        */
#define GCSR_BUS_GLOBAL_1   GCSR_BUS_ADRS(0x01) /* global 1 on VMEbus        */
#define GCSR_BUS_BOARD_ID   GCSR_BUS_ADRS(0x02) /* board ID on VMEbus        */
#define GCSR_BUS_GP_CSR_0   GCSR_BUS_ADRS(0x03) /* general purp. 0 on VMEbus */
#define GCSR_BUS_GP_CSR_1   GCSR_BUS_ADRS(0x04) /* general purp. 1 on VMEbus */
#define GCSR_BUS_GP_CSR_2   GCSR_BUS_ADRS(0x05) /* general purp. 2 on VMEbus */
#define GCSR_BUS_GP_CSR_3   GCSR_BUS_ADRS(0x06) /* general purp. 3 on VMEbus */
#define GCSR_BUS_GP_CSR_4   GCSR_BUS_ADRS(0x07) /* general purp. 4 on VMEbus */


/************************************************************************
* SYS_CONF                  0x00    0x01    system controller config    *
*************************************************************************/

#define SYS_CONF_SCON       0x01    /* system controller on (status)  0 */
#define SYS_CONF_SRESET     0x02    /* system reset                   1 */
#define SYS_CONF_BRDFAIL    0x04    /* board fail                     2 */
#define SYS_CONF_ROBIN      0x08    /* round robin select             3 */
#define SYS_CONF_PRIORITY   0x00    /* priority mode select           3 */

/************************************************************************
* RQST_CONF                 0x01    0x03    requester configuration     *
*************************************************************************/

#define RQST_CONF_RQLEV     0x03    /* VMEbus request level (mask)   10 */
#define RQST_CONF_RQLEV_0   0x00    /* VMEbus request level 0        10 */
#define RQST_CONF_RQLEV_1   0x01    /* VMEbus request level 1        10 */
#define RQST_CONF_RQLEV_2   0x02    /* VMEbus request level 2        10 */
#define RQST_CONF_RQLEV_3   0x03    /* VMEbus request level 3        10 */

#define RQST_CONF_RNEVER    0x08    /* release never                  3 */
#define RQST_CONF_RWD       0x10    /* release when done              4 */
#define RQST_CONF_ROR       0x00    /* release on request             4 */
#define RQST_CONF_RONR      0x20    /* request on no request          5 */
#define RQST_CONF_DHB       0x40    /* device has bus (status)        6 */
#define RQST_CONF_DWB       0x80    /* device wants bus               7 */

/************************************************************************
* MASTER_CONF               0x02    0x05    master configuration        *
*************************************************************************/

#define MASTER_CONF_MASD16  0x01    /* master D16                     0 */
#define	MASTER_CONF_MASD32  0x00    /* master D32, D16, D8            0 */
#define	MASTER_CONF_MASA32  0x00    /* master A32                     1 */
#define MASTER_CONF_MASA24  0x02    /* master A24                     1 */
#define MASTER_CONF_MASA16  0x04    /* master A16                     2 */
#define MASTER_CONF_MASUAT  0x08    /* master unaligned transfers     3 */
#define MASTER_CONF_CFILL   0x10    /* cache fill                     4 */
#define MASTER_CONF_MASWP   0x20    /* master write-posting (CAUTION) 5 */
#define MASTER_CONF_020     0x40    /* local CPU is a 68020           6 */
#define MASTER_CONF_DDTACK  0x80    /* extra delay for 33MHz 020/030  7 */

/************************************************************************
* SLAVE_CONF                0x03    0x07    slave configuration         *
*************************************************************************/

#define SLAVE_CONF_SLVD16   0x01    /* slave D16                      0 */
#define SLAVE_CONF_SLVWP    0x20    /* slave write-posting (CAUTION)  5 */
#define SLAVE_CONF_SLVEN    0x80    /* slave enable                   7 */
#define	SLAVE_CONF_SLVDIS   0x00    /* slave disable                  7 */

/************************************************************************
* TMO_CONF                  0x04    0x09    timer configuration         *
*************************************************************************/

#define TMO_CONF_LBTO       0x03    /* local bus time-out (mask)     10 */
#define TMO_CONF_LBTO_1024  0x00    /* local bus time-out 1024 * T   10 */
#define TMO_CONF_LBTO_2048  0x01    /* local bus time-out 2048 * T   10 */
#define TMO_CONF_LBTO_4096  0x02    /* local bus time-out 4096 * T   10 */
#define TMO_CONF_LBTO_DIS   0x03    /* local bus time-out disabled   10 */

#define TMO_CONF_ACTO       0x0c    /* access time-out (mask)        32 */
#define TMO_CONF_ACTO_1024  0x00    /* access time-out 1024 * T      32 */
#define TMO_CONF_ACTO_16384 0x04    /* access time-out 16384 * T     32 */
#define TMO_CONF_ACTO_524   0x08    /* access time-out 524290 * T    32 */
#define TMO_CONF_ACTO_DIS   0x0c    /* access time-out disabled      32 */

#define TMO_CONF_VBTO       0x30    /* VMEbus time-out (mask)        54 */
#define TMO_CONF_VBTO_1024  0x00    /* VMEbus time-out 1024 * T      54 */
#define TMO_CONF_VBTO_2048  0x10    /* VMEbus time-out 2048 * T      54 */
#define TMO_CONF_VBTO_4096  0x20    /* VMEbus time-out 4096 * T      54 */
#define TMO_CONF_VBTO_DIS   0x30    /* VMEbus time-out disabled      54 */

#define TMO_CONF_ARBTO      0x40    /* arbitration timer              6 */

/************************************************************************
* SLAVE_AM                  0x05    0x0b    slave address modifier      *
*************************************************************************/

#define SLAVE_AM_DATA       0x01    /* respond to data                0 */
#define SLAVE_AM_PROGRAM    0x02    /* respond to program             1 */
#define SLAVE_AM_BLOCK      0x04    /* respond to block (never set)   2 */

#define SLAVE_AM_SHORT      0x08    /* respond to short               3 */
#define SLAVE_AM_STND       0x10    /* respond to standard            4 */
#define SLAVE_AM_EXTED      0x20    /* respond to extended            5 */

#define SLAVE_AM_USER       0x40    /* respond to user                6 */
#define SLAVE_AM_SUPER      0x80    /* respond to supervisor          7 */

/************************************************************************
* MASTER_AM                 0x06    0x0d    master address modifier     *
*************************************************************************/

#define MASTER_AM_AM        0x3f    /* address modifier code (mask)  50 */

#define MASTER_AM_AMSEL     0x80    /* address modifier select        7 */
#define MASTER_AM_DYNAMIC   0x00    /* AM code dynamically determined 7 */

/************************************************************************
* INT_MASK                  0x07    0x0f    interrupt handler mask      *
*************************************************************************/

#define INT_MASK_IEN1       0x02    /* VMEbus IRQ1 interrupt enable   1 */
#define INT_MASK_IEN2       0x04    /* VMEbus IRQ2 interrupt enable   2 */
#define INT_MASK_IEN3       0x08    /* VMEbus IRQ3 interrupt enable   3 */
#define INT_MASK_IEN4       0x10    /* VMEbus IRQ4 interrupt enable   4 */
#define INT_MASK_IEN5       0x20    /* VMEbus IRQ5 interrupt enable   5 */
#define INT_MASK_IEN6       0x40    /* VMEbus IRQ6 interrupt enable   6 */
#define INT_MASK_IEN7       0x80    /* VMEbus IRQ7 interrupt enable   7 */
#define	INT_MASK_DISABLE    0x00    /* Disable VMEbus interrupts        */

/************************************************************************
* UTIL_INT_MASK               0x08  0x11    utility interrupt mask      *
*************************************************************************/

#define UTIL_INT_MASK_SIGLEN  0x02  /* signal low interrupt enable    1 */
#define UTIL_INT_MASK_LM0EN   0x04  /* loc. monitor 0 intrpt. enable  2 */
#define UTIL_INT_MASK_IACKEN  0x08  /* IACK interrupt enable          3 */
#define UTIL_INT_MASK_LM1EN   0x10  /* loc. monitor 1 intrpt. enable  4 */
#define UTIL_INT_MASK_SIGHEN  0x20  /* signal high interrupt enable   5 */
#define UTIL_INT_MASK_SFIEN   0x40  /* SYSFAIL interrupt enable       6 */
#define UTIL_INT_MASK_WPERREN 0x80  /* write posting bus error enable 7 */
#define	UTIL_INT_MASK_DISABLE 0x00  /* Disable utility interrupts       */

/************************************************************************
* UTIL_VEC_BASE               0x09  0x13    utility interrupt vector    *
*************************************************************************/

#define UTIL_VEC_BASE_UID     0x07  /* utility interrupt ID (mask)   20 */
#define UTIL_VEC_BASE_SIGLP   0x01  /* signal low                    20 */
#define UTIL_VEC_BASE_LM0     0x02  /* location monitor 0            20 */
#define UTIL_VEC_BASE_IACK    0x03  /* IACK                          20 */
#define UTIL_VEC_BASE_LM1     0x04  /* location monitor 1            20 */
#define UTIL_VEC_BASE_SIGHP   0x05  /* signal high                   20 */
#define UTIL_VEC_BASE_SYSFAIL 0x06  /* SYSFAIL                       20 */
#define UTIL_VEC_BASE_WPBERR  0x07  /* write post bus error          20 */

#define UTIL_VEC_BASE_UVB     0xf8  /* utility vector base (mask)    73 */

/************************************************************************
* INT_REQ                   0x0a    0x15    interrupt request           *
*************************************************************************/

#define INT_REQ_UID         0x07    /* interrupt request (mask)      20 */
#define INT_REQ_NONE        0x00    /* none                          20 */
#define INT_REQ_IRQ1        0x01    /* IRQ1                          20 */
#define INT_REQ_IRQ2        0x02    /* IRQ2                          20 */
#define INT_REQ_IRQ3        0x03    /* IRQ3                          20 */
#define INT_REQ_IRQ4        0x04    /* IRQ4                          20 */
#define INT_REQ_IRQ5        0x05    /* IRQ5                          20 */
#define INT_REQ_IRQ6        0x06    /* IRQ6                          20 */
#define INT_REQ_IRQ7        0x07    /* IRQ7                          20 */

/************************************************************************
* INT_STATUS_ID             0x0b    0x17    status/ID                   *
*************************************************************************/

/* none */

/************************************************************************
* BERR_STATUS               0x0c    0x19    bus error status            *
*************************************************************************/

#define BERR_STATUS_LBTO    0x01    /* local bus time-out             0 */
#define BERR_STATUS_ACTO    0x02    /* access time-out                1 */
#define BERR_STATUS_VBERR   0x04    /* VMEbus bus error               2 */
#define BERR_STATUS_RMCERR  0x08    /* RMC lock error                 3 */

/************************************************************************
* GCSR_BASE_ADRS            0x0d    0x1b    GCSR base address           *
*************************************************************************/

#define GCSR_BASE_ADRS      0x0f    /* GCSR base address (mask)      30 */
#define GCSR_BASE_ADRS_NONE 0x0f    /* GCSR not mapped on VMEbus     30 */

/************************************************************************
* GLOBAL_0                  0x10    0x21    global 0                    *
*************************************************************************/

#define GLOBAL_0_CHIP_ID    0x0f    /* chip ID (mask)                30 */

#define GLOBAL_0_LM0        0x10    /* location monitor 0             4 */
#define GLOBAL_0_LM1        0x20    /* location monitor 1             5 */
#define GLOBAL_0_LM2        0x40    /* location monitor 2             6 */
#define GLOBAL_0_LM3        0x80    /* location monitor 3             7 */

/************************************************************************
* GLOBAL_1                  0x11    0x23    global 1                    *
*************************************************************************/

#define GLOBAL_1_SIGLP      0x01    /* signal low priority            0 */
#define GLOBAL_1_SIGHP      0x02    /* signal high priority           1 */
#define GLOBAL_1_BRDFAIL    0x10    /* board fail                     4 */
#define GLOBAL_1_ISF        0x20    /* inhibit SYSFAIL                5 */
#define GLOBAL_1_SCON       0x40    /* system controller on (status)  6 */
#define GLOBAL_1_R_AND_H    0x80    /* reset and hold                 7 */

/************************************************************************
* BOARD_ID                  0x12    0x25    board ID                    *
*************************************************************************/

/* none */

/************************************************************************
* GP_CSR_0                  0x13    0x27    general purpose CSR 0       *
* to                                                                    *
* GP_CSR_4                  0x17    0x2f    general purpose CSR 4       *
*************************************************************************/

/* none */

#ifdef __cplusplus
}
#endif

#endif /* __INCvmechiph */
