/* m68562.h - Motorola M68562 Serial I/O Chip header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01d,14nov95,myz   removed some element in M68562_CHAN structure.
01c,14nov95,myz   added one element int_set in M68562_CHAN structure
01b,15jun95,ms    updated for new driver.
01a,14apr95,myz   written (using m68562.h)
*/

#ifndef __INCm68562Sioh
#define __INCm68562Sioh

#ifdef __cplusplus
extern "C" {
#endif


#include "siolib.h"
#include "drv/multi/m68562.h"

#define N_CHANNELS      2       /* number of channels per chip */

typedef struct        
    {
    /* always goes first */

    SIO_DRV_FUNCS *     pDrvFuncs;      /* driver functions */

    /* callbacks */

    STATUS      (*getTxChar) ();
    STATUS      (*putRcvChar) ();
    void *      getTxArg;
    void *      putRcvArg;

    USHORT          channelMode; 
    USHORT          chan_num;
    struct M68562_QUSART *pQusart;  /* back pointer to the device */
    char * cmr1;
    char * cmr2;
    char * ss1r;           /* SYN1 /Secondary Adr Reg 1 */
    char * ss2r;           /* SYN2 /Secondary Adr Reg 2 */
    char * tpr;            /* transmitter parameter Reg */
    char * ttr;            /* transmitter timer register */
    char * rpr;            /* receiver parameter register */
    char * rtr;            /* receiver timer register */
    char * ctprh;          /* counter/timer preset register H */
    char * ctprl;          /* counter/timer preset register L */
    char * ctcr;           /* counter/timer control register  */
    char * omr;            /* output and miscellaneous register */
    char * cth;            /* counter/timer high */
    char * ctl;            /* counter/timer low  */
    char * pcr;            /* pin configuration register */
    char * ccr;            /* channel command register   */
    char * tx_data;        /* transmitter data port */
    char * rx_data;        /* receiver data port */
    char * rsr;            /* receiver status register */
    char * trsr;           /* transmitter/receiver state register */
    char * ictsr;          /* input + counter/timer stat register */
    char * ier;            /* interrupt enable register */

    char            rx_rdy;         /* receiver ready bit */
    char            tx_rdy;         /* transmitter ready bit */

    } M68562_CHAN;

typedef struct M68562_QUSART
    {
    M68562_CHAN  channel[N_CHANNELS];
    USHORT      numChannels;
    USHORT      int_vec; 
    volatile char * gsr;             /* general status register */
    volatile char * ivr;             /* interrupt vector register unmodified */
    volatile char * icr;             /* interrupt control register */
    volatile char * ivrm;            /* interrupt vector register modified */
    } M68562_QUSART;



/* port registers */

#define DSC_CMR1A	(0x00)	/* channel mode reg. 1    */
#define DSC_CMR2A	(0x01)	/* channel mode reg. 2    */
#define DSC_S1RA	(0x02)	/* secondary adrs reg1    */
#define DSC_S2RA	(0x03)	/* secondary adrs reg2    */
#define DSC_TPRA	(0x04)	/* tx parameter reg.      */
#define DSC_TTRA	(0x05)	/* tx timing reg.         */
#define DSC_RPRA	(0x06)	/* rx parameter reg.      */
#define DSC_RTRA	(0x07)	/* rx timing reg.         */
#define DSC_CTPRHA	(0x08)	/* counter preset high    */
#define DSC_CTPRLA	(0x09)	/* counter preset low     */
#define DSC_CTCRA	(0x0a)	/* counter control reg.   */
#define DSC_OMRA	(0x0b)	/* output/misc. reg.      */
#define DSC_CTHA	(0x0c)	/* counter/timer high     */
#define DSC_CTLA	(0x0d)	/* counter/timer low      */
#define DSC_PCRA	(0x0e)	/* pin configuration      */
#define DSC_CCRA	(0x0f)	/* channel command reg.   */
#define DSC_TXFIFOA	(0x10)	/* tx FIFO                */
#define DSC_RXFIFOA	(0x14)	/* rx FIFO                */
#define DSC_RSRA	(0x18)	/* rx status reg.         */
#define DSC_TRSRA	(0x19)	/* tx/rx status reg.      */
#define DSC_ICTSRA	(0x1a)	/* counter status reg.    */
#define DSC_GSR	(0x1b)	/* general status         */
#define DSC_IERA	(0x1c)	/* interrupt enable       */
#define DSC_IVR	(0x1e)	/* interrupt vector       */
#define DSC_ICR	(0x1f)	/* interrupt control      */
#define DSC_CMR1B	(0x20)	/* channel mode reg. 1    */
#define DSC_CMR2B	(0x21)	/* channel mode reg. 2    */
#define DSC_S1RB	(0x22)	/* secondary adrs reg 1   */
#define DSC_S2RB	(0x23)	/* secondary adrs reg 2   */
#define DSC_TPRB	(0x24)	/* tx parameter reg.      */
#define DSC_TTRB	(0x25)	/* tx timing reg.         */
#define DSC_RPRB	(0x26)	/* rx parameter reg.      */
#define DSC_RTRB	(0x27)	/* rx timing reg.         */
#define DSC_CTPRHB	(0x28)	/* counter preset high    */
#define DSC_CTPRLB	(0x29)	/* counter preset low     */
#define DSC_CTCRB	(0x2a)	/* counter control reg.   */
#define DSC_OMRB	(0x2b)	/* output/misc. reg.      */
#define DSC_CTHB	(0x2c)	/* counter/timer high     */
#define DSC_CTLB	(0x2d)	/* counter/timer low      */
#define DSC_PCRB	(0x2e)	/* pin configuration      */
#define DSC_CCRB	(0x2f)	/* channel command reg.   */
#define DSC_TXFIFOB	(0x30)	/* tx FIFO                */
#define DSC_RXFIFOB	(0x34)	/* rx FIFO                */
#define DSC_RSRB	(0x38)	/* rx status reg.         */
#define DSC_TRSRB	(0x39)	/* tx/rx status reg.      */
#define DSC_ICTSRB	(0x3a)	/* count/timer status     */
#define DSC_IERB	(0x3c)	/* interrupt enable       */
#define DSC_IVRM	(0x3e)	/* int. vec. modified     */


/* function declarations */

#ifndef _ASMLANGUAGE
#if defined(__STDC__) || defined(__cplusplus)

extern  void m68562TxInt (M68562_CHAN *pDev);
extern  void m68562RxInt (M68562_CHAN *pDev);
extern  void m68562RxTxErrInt (M68562_CHAN *pDev);
extern  void m68562HrdInit (M68562_QUSART *pQusart);

#else	/* __STDC__ */

extern  void m68562TxInt ();
extern  void m68562RxInt ();
extern  void m68562RxTxErrInt ();
extern  void m68562HrdInit ();

#endif	/* __STDC__ */
#endif	/* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCm68562Sioh */
