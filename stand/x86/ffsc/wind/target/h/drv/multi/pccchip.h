/* pccchip.h - Peripheral Channel Controller header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01b,22sep92,rrr  added support for c++
01a,16jun92,ccc  created.
*/

/*
DESCRIPTION
This file contains I/O addresses and related constants for the PCC
chip.  PCC_BASE_ADRS and PRINTER must be defined.
*/

#ifndef __INCpccchiph
#define __INCpccchiph

#ifdef __cplusplus
extern "C" {
#endif

/* PCC addresses */

#define SCSI_DMA_TABLE_ADRS     ((long *) (PCC_BASE_ADRS + 0x00))
#define SCSI_PCC_DMA_ADRS       ((long *) (PCC_BASE_ADRS + 0x04))
#define SCSI_DMA_BYTE_COUNT     ((long *) (PCC_BASE_ADRS + 0x08))

#define TIC_1_PRELOAD           ((short *)(PCC_BASE_ADRS + 0x10))
#define TIC_2_PRELOAD           ((short *)(PCC_BASE_ADRS + 0x14))

#define TIC_1_INT_CTL           ((char *) (PCC_BASE_ADRS + 0x18))
#define TIC_1_CSR               ((char *) (PCC_BASE_ADRS + 0x19))
#define TIC_2_INT_CTL           ((char *) (PCC_BASE_ADRS + 0x1a))
#define TIC_2_CSR               ((char *) (PCC_BASE_ADRS + 0x1b))

#define ACFAIL_INT_CTL          ((char *) (PCC_BASE_ADRS + 0x1c))
#define WDOG_CSR                ((char *) (PCC_BASE_ADRS + 0x1d))
#define BERR_INT_CTL            ((char *) (PCC_BASE_ADRS + 0x22))
#define ABORT_INT_CTL           ((char *) (PCC_BASE_ADRS + 0x24))

#define PRINTER_INT_CTL         ((char *) (PCC_BASE_ADRS + 0x1e))
#define PRINTER_CSR             ((char *) (PCC_BASE_ADRS + 0x1f))
#define PRINTER_DATA            ((char *) (PRINTER + 0x00))
#define PRINTER_STATUS          ((char *) (PRINTER + 0x00))

#define SCSI_DMA_INT_CTL        ((char *) (PCC_BASE_ADRS + 0x20))
#define SCSI_DMA_CSR            ((char *) (PCC_BASE_ADRS + 0x21))
#define SCSI_DMA_TABLE_FC       ((char *) (PCC_BASE_ADRS + 0x25))
#define SCSI_INT_CTL            ((char *) (PCC_BASE_ADRS + 0x2a))

#define SERIAL_INT_CTL          ((char *) (PCC_BASE_ADRS + 0x26))

#define LAN_INT_CTL             ((char *) (PCC_BASE_ADRS + 0x28))

#define SOFT_1_INT_CTL          ((char *) (PCC_BASE_ADRS + 0x2c))
#define SOFT_2_INT_CTL          ((char *) (PCC_BASE_ADRS + 0x2e))

#define GP_CTL                  ((char *) (PCC_BASE_ADRS + 0x27))
#define GP_STATUS               ((char *) (PCC_BASE_ADRS + 0x29))
#define INT_BASE_CTL            ((char *) (PCC_BASE_ADRS + 0x2d))
#define SLAVE_BASE_CTL          ((char *) (PCC_BASE_ADRS + 0x2b))

#define PCC_REVISION            ((char *) (PCC_BASE_ADRS + 0x2f))

/* PCC register bit definitions */

#define TIC_1_INT_CTL_CLEAR     0x80    /* Clear tic 1 interrupt        */
#define TIC_1_INT_CTL_ENABLE    0x08    /* Enable tic 1 interrupt       */
#define TIC_1_INT_CTL_DISABLE   0x00    /* Disable tic 1 interrupt      */

#define TIC_1_CSR_CLR_OVF       0x04    /* Clear overflow counter       */
#define TIC_1_CSR_ENABLE        0x03    /* Enable counter               */
#define TIC_1_CSR_DISABLE       0x01    /* Disable tic 1 counter        */
#define TIC_1_CSR_STOP          0x00    /* Load preload                 */

#define TIC_2_INT_CTL_CLEAR     0x80    /* Clear tic 2 interrupt        */
#define TIC_2_INT_CTL_ENABLE    0x08    /* Enable tic 2 interrupt       */
#define TIC_2_INT_CTL_DISABLE   0x00    /* Disable tic 2 interrupt      */

#define TIC_2_CSR_CLR_OVF       0x04    /* Clear overflow counter       */
#define TIC_2_CSR_ENABLE        0x03    /* Enable counter               */
#define TIC_2_CSR_DISABLE       0x01    /* Disable tic 2 counter        */
#define TIC_2_CSR_STOP          0x00    /* Load preload                 */

#define ACFAIL_INT_CTL_CLEAR    0x80    /* Clear AC fail interrupt      */
#define ACFAIL_INT_CTL_ENABLE   0x08    /* Enable AC fail interrupt     */
#define ACFAIL_INT_CTL_DISABLE  0x00    /* Disable AC fail interrupt    */

#define WDOG_CSR_TO_CLEAR       0x08    /* Clear wDog time-out          */
#define WDOG_CSR_WD_RESET       0x04    /* wDog activates reset         */
#define WDOG_CSR_WD_CLEAR       0x02    /* Clear wDog                   */
#define WDOG_CSR_ENABLE         0x01    /* Enable wDog                  */
#define WDOG_CSR_DISABLE        0x00    /* Disable wDog                 */

#define BERR_INT_CTL_CLEAR      0x80    /* Clear bus error interrupt    */
#define BERR_INT_CTL_ENABLE     0x08    /* Enable bus error interrupt   */
#define BERR_INT_CTL_DISABLE    0x00    /* Disable bus error interrupt  */

#define ABORT_INT_CTL_CLEAR     0x80    /* Clear abort interrupt        */
#define ABORT_INT_CTL_ENABLE    0x08    /* Enable abort interrupt       */
#define ABORT_INT_CTL_DISABLE   0x00    /* Disable abort interrupt      */

#define SERIAL_INT_CTL_VECT_IN  0x10    /* Interrupt vector from PCC    */
#define SERIAL_INT_CTL_VCHIP    0x00    /* Interrupt vector from chip   */
#define SERIAL_INT_CTL_ENABLE   0x08    /* Enable serial interrupt      */
#define SERIAL_INT_CTL_DISABLE  0x00    /* Disable serial interrupt     */

#define GP_CTL_RST_SW_DISABLE   0xa0    /* Disable front panel reset    */
#define GP_CTL_RST_SW_ENABLE    0x00    /* Enable front panel reset     */
#define GP_CTL_MASTER_INT_EN    0x10    /* Master interrupt enable      */
#define GP_CTL_MASTER_INT_DIS   0x00    /* Master interrupt disable     */
#define GP_CTL_LOCAL_TO_EN      0x08    /* Enable local bus timeout     */
#define GP_CTL_LOCAL_TO_DIS     0x00    /* Disable local bus timeout    */
#define GP_CTL_PARITY_DISABLE   0x00    /* Disable parity checking      */
#define GP_CTL_PARITY_ENABLE    0x01    /* Enable parity checking       */

#define GP_STATUS_PU_CLEAR      0x02    /* Clear power-up reset         */
#define GP_STATUS_PARITY_CLEAR  0x01    /* Clear parity error           */

#define LAN_INT_CTL_ENABLE      0x08    /* Enable LAN interrupt         */
#define LAN_INT_CTL_DISABLE     0x00    /* Disable LAN interrupt        */

#define SOFT_1_INT_CTL_ENABLE   0x08    /* Enable software 1 interrupt  */
#define SOFT_1_INT_CTL_DISABLE  0x00    /* Disable software 1 interrupt */

#define SOFT_2_INT_CTL_ENABLE   0x08    /* Enable software 2 interrupt  */
#define SOFT_2_INT_CTL_DISABLE  0x00    /* Disable software 2 interrupt */
#define PRNT_INT_CTL_CLR_FAULT  0x40    /* Clear fault interrupt        */
#define PRNT_INT_CTL_CLR_ACK    0x20    /* Clear ACK interrupt          */
#define PRNT_INT_CTL_ACK_FALL   0x10    /* ACK on falling edge          */
#define PRNT_INT_CTL_ACK_RISE   0x00    /* ACK on rising edge           */
#define PRNT_INT_CTL_ENABLE     0x08    /* Enable printer interrupt     */
#define PRNT_INT_CTL_DISABLE    0x00    /* Disable printer interrupt    */

#define DMA_INT_CTL_PENDING     0x80    /* Interrupt pending            */
#define DMA_INT_CTL_CLEAR       0x80    /* Clear pending interrupt      */
#define DMA_INT_CTL_ENABLE      0x08    /* Enable DMA interrupt         */
#define DMA_INT_CTL_DISABLE     0x00    /* Disable DMA interrupt        */

#define DMA_CSR_ENABLE          0x01    /* Enable DMA controller        */
#define DMA_CSR_DISABLE         0x00    /* Disable DMA controller       */
#define DMA_CSR_TW              0x02    /* Use table address register   */
#define DMA_CSR_READ            0x00    /* Transfer from SCSI bus       */
#define DMA_CSR_WRITE           0x04    /* Transfer to SCSI bus         */

#define SCSI_INT_CTL_RESET      0x40    /* Reset SCSI interrupt         */
#define SCSI_INT_CTL_BUS_RESET  0x10    /* Reset SCSI bus               */
#define SCSI_INT_CTL_ENABLE     0x08    /* Enable SCSI interrupt        */
#define SCSI_INT_CTL_DISABLE    0x00    /* Disable SCSI interrupt       */

/* function declarations */

#ifndef	_ASMLANGUAGE
#if defined(__STDC__) || defined(__cplusplus)

#if	CPU_FAMILY == I960
IMPORT	STATUS	sysIntDisable ();
IMPORT	STATUS	sysIntEnable ();
IMPORT	int	sysBusIntAck ();
IMPORT	STATUS	sysBusIntGen ();
IMPORT	STATUS	sysMailboxEnable ();
#else	/* CPU_FAMILY == I960 */
IMPORT	STATUS	sysIntDisable (int intLevel);
IMPORT	STATUS	sysIntEnable (int intLevel);
IMPORT	int	sysBusIntAck (int intLevel);
IMPORT	STATUS	sysBusIntGen (int level, int vector);
IMPORT	STATUS	sysMailboxEnable (char *mailboxAdrs);
#endif	/* CPU_FAMILY == I960 */

IMPORT	STATUS	sysMailboxConnect (FUNCPTR routine, int arg);

#else	/* __STDC__ */

IMPORT	STATUS	sysIntDisable ();
IMPORT	STATUS	sysIntEnable ();
IMPORT	int	sysBusIntAck ();
IMPORT	STATUS	sysBusIntGen ();
IMPORT	STATUS	sysMailboxEnable ();
IMPORT	STATUS	sysMailboxConnect ();

#endif	/* __STDC__ */

#endif	/* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCpccchiph */
