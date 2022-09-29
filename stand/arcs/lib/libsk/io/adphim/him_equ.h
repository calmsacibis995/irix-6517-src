/* $Header: /proj/irix6.5.7m/isms/stand/arcs/lib/libsk/io/adphim/RCS/him_equ.h,v 1.1 1997/06/05 23:38:29 philw Exp $ */
/***************************************************************************
*                                                                          *
* Copyright 1993 Adaptec, Inc.,  All Rights Reserved.                      *
*                                                                          *
* This software contains the valuable trade secrets of Adaptec.  The       *
* software is protected under copyright laws as an unpublished work of     *
* Adaptec.  Notice is for informational purposes only and does not imply   *
* publication.  The user of this software may make copies of the software  *
* for use with parts manufactured by Adaptec or under license from Adaptec *
* and for no other use.                                                    *
*                                                                          *
***************************************************************************/

/***************************************************************************
*
*  Module Name:   HIM_EQU.H
*
*  Description:
*						Equate and definitions for AIC-7870/AIC-7850 harwdare 
*						programming 
*
*  Programmers:   Paul von Stamwitz
*                 Chuck Fannin
*                 Jeff Mendiola
*                 Harry Yang
*    
*  Notes:         NONE
*
*  Revisions -
*
***************************************************************************/

/****************************************************************************

 Equates and definitions to be used exclusively by the Lance Driver modules

****************************************************************************/

#define  NARROW_OFFSET  15       /* maximum 8-bit sync transfer offset     */
#define  WIDE_OFFSET    8        /* maximum 16-bit sync transfer offset    */
#define  WIDE_WIDTH     1        /* maximum tarnsfer width = 16 bits       */
#define  FAST20_THRESHOLD 0x19   /* Smallest 10m transfer sync rate value  */

                                 /* scsi messages -                        */
#define  MSG00          0x00     /*   - command complete                   */
#define  MSG01          0x01     /*   - extended message                   */
#define  MSGSYNC        0x01     /*       - synchronous data transfer msg  */
#define  MSGWIDE        0x03     /*       - wide data transfer msg         */
#define  MSG02          0x02     /*   - save data pointers                 */
#define  MSG03          0x03     /*   - restore data pointers              */
#define  MSG04          0x04     /*   - disconnect                         */
#define  MSG05          0x05     /*   - initiator detected error           */
#define  MSG06          0x06     /*   - abort                              */
#define  MSG07          0x07     /*   - message reject                     */
#define  MSG08          0x08     /*   - nop                                */
#define  MSG09          0x09     /*   - message parity error               */
#define  MSG0A          0x0a     /*   - linked command complete            */
#define  MSG0B          0x0b     /*   - linked command complete            */
#define  MSG0C          0x0c     /*   - bus device reset                   */
#define  MSG0D          0x0d     /*   - abort tag                          */
#define  MSGTAG         0x20     /*   - tag queuing                        */
#define  MSGID          0x80     /* identify message, no disconnection     */
#define  MSGID_D        0xc0     /* identify message, disconnection        */

/****************************************************************************

 LANCE SCSI REGISTERS DEFINED AS A STRUCTURE

****************************************************************************/

#ifdef MIPS_BE
#define  SCSISEQ        0x03     /* scsi sequence control     (read/write) */
#define  SXFRCTL0       0x02     /* scsi transfer control 0   (read/write) */
#define  SXFRCTL1       0x01     /* scsi transfer control 1   (read/write) */
#define  SCSISIG        0x00     /* actual scsi bus signals   (write/read) */
#else
#define  SCSISEQ        0x00     /* scsi sequence control     (read/write) */
#define  SXFRCTL0       0x01     /* scsi transfer control 0   (read/write) */
#define  SXFRCTL1       0x02     /* scsi transfer control 1   (read/write) */
#define  SCSISIG        0x03     /* actual scsi bus signals   (write/read) */
#endif


/* SCSISEQ defs */
   #define  TEMODEO     0x80     /* target enable mode out                 */
   #define  ENSELO      0x40     /* enable selection out                   */
   #define  ENSELI      0x20     /* enable selection in                    */
   #define  ENRSELI     0x10     /* enable reselection in                  */
   #define  ENAUTOATNO  0x08     /* enable auto attention out              */
   #define  ENAUTOATNI  0x04     /* enable auto attention in               */
   #define  ENAUTOATNP  0x02     /* enable auto attention parity           */
   #define  SCSIRSTO    0x01     /* scsi reset out                         */

/* SXFRCTL0 defs */
   #define  DFON        0x80     /* Digital Filter On                      */
   #define  DFPEXP      0x40     /* Digital Filter Period                  */
   #define  FAST20      0x20     /* Fast20 hardware enabled                */
   #define  CLRSTCNT    0x10     /* clear Scsi Transfer Counter            */
   #define  SPIOEN      0x08     /* enable auto scsi pio                   */
   #define  SCAMEN      0x04     /* Enable SCAM protocol                   */
   #define  CLRCHN      0x02     /* clear Channel n                        */
              
/* SXFRCTL1 defs */                   
   #define  BITBUCKET   0x80     /* enable bit bucket mode                 */
   #define  SWRAPEN     0x40     /* enable wrap-around                     */
   #define  ENSPCHK     0x20     /* enable scsi parity checking            */
   #define  STIMESEL1   0x10     /* select selection timeout:  00 - 256 ms,*/
   #define  STIMESEL0   0x08     /* 01 - 128 ms, 10 - 64 ms, 11 - 32 ms    */
   #define  ENSTIMER    0x04     /* enable selection timer                 */
   #define  ACTNEGEN    0x02     /* enable active negation                 */
   #define  STPWEN      0x01     /* enable termination power               */

/* SCSISIG defs */
   #define  CDI         0x80     /* c/d                                    */
   #define  IOI         0x40     /* i/o                                    */
   #define  MSGI        0x20     /* msg                                    */
   #define  ATNI        0x10     /* atn                                    */
   #define  SELI        0x08     /* sel                                    */
   #define  BSYI        0x04     /* bsy                                    */
   #define  REQI        0x02     /* req                                    */
   #define  ACKI        0x01     /* ack                                    */

   #define  CDO         0x80     /* expected c/d  (initiator mode)         */
   #define  IOO         0x40     /* expected i/o  (initiator mode)         */
   #define  MSGO        0x20     /* expected msg  (initiator mode)         */
   #define  ATNO        0x10     /* set atn                                */
   #define  SELO        0x08     /* set sel                                */
   #define  BSYO        0x04     /* set busy                               */
   #define  REQO        0x02     /* not functional in initiator mode       */
   #define  ACKO        0x01     /* set ack                                */

   #define  BUSPHASE    CDO+IOO+MSGO   /* scsi bus phase -                 */
   #define  DOPHASE     0x00           /*  data out                        */
   #define  DIPHASE     IOO            /*  data in                         */
   #define  CMDPHASE    CDO            /*  command                         */
   #define  MIPHASE     CDO+IOO+MSGO   /*  message in                      */
   #define  MOPHASE     CDO+MSGO       /*  message out                     */
   #define  STPHASE     CDO+IOO        /*  status                          */


#ifdef MIPS_BE
#define  SCSIRATE       0x07     /* scsi rate control      (write)         */
#define  SCSIID         0x06     /* scsi id       (read/write)             */
#define  SCSIDATL       0x05     /* scsi latched data, lo     (read/write) */
#define  SCSIDATH       0x04     /* scsi latched data, hi     (read/write) */
#else
#define  SCSIRATE       0x04     /* scsi rate control      (write)         */
#define  SCSIID         0x05     /* scsi id       (read/write)             */
#define  SCSIDATL       0x06     /* scsi latched data, lo     (read/write) */
#define  SCSIDATH       0x07     /* scsi latched data, hi     (read/write) */
#endif

   #define  WIDEXFER    0x80     /* ch 0 wide scsi bus                     */
   #define  SXFR2       0x40     /* synchronous scsi transfer rate         */
   #define  SXFR1       0x20     /*                                        */
   #define  SXFR0       0x10     /*                                        */
   #define  SXFR        SXFR2+SXFR1+SXFR0       /*                         */
   #define  SOFS3       0x08     /* synchronous scsi offset                */
   #define  SOFS2       0x04     /*                                        */
   #define  SOFS1       0x02     /*                                        */
   #define  SOFS0       0x01     /*                                        */
   #define  SOFS        SOFS3+SOFS2+SOFS1+SOFS0 /*                         */

   #define  TID3        0x80     /* other scsi device id                   */
   #define  TID2        0x40     /*                                        */
   #define  TID1        0x20     /*                                        */
   #define  TID0        0x10     /*                                        */
   #define  OID3        0x08     /* my id                                  */
   #define  OID2        0x04     /*                                        */
   #define  OID1        0x02     /*                                        */
   #define  OID0        0x01     /*                                        */
   #define  TID         TID3+TID2+TID1+TID0  /* scsi device id mask        */
   #define  OID         OID3+OID2+OID1+OID0  /* our scsi device id mask    */



#ifdef MIPS_BE
#define  STCNT0         0x0b     /* scsi transfer count, lsb  (read/write) */
#define  STCNT1         0x0a     /*                    , mid  (read/write) */
#define  STCNT2         0x09     /*                    , msb  (read/write) */
#define  CLRSINT0       0x08     /* clear scsi interrupts 0   (write)      */
#define  SSTAT0         0x08     /* scsi status 0       (read)             */
#else
#define  STCNT0         0x08     /* scsi transfer count, lsb  (read/write) */
#define  STCNT1         0x09     /*                    , mid  (read/write) */
#define  STCNT2         0x0a     /*                    , msb  (read/write) */
#define  CLRSINT0       0x0b     /* clear scsi interrupts 0   (write)      */
#define  SSTAT0         0x0b     /* scsi status 0       (read)             */
#endif

/* CLRSINT0 defs */
   #define  CLRSELDO    0x40     /* clear seldo interrupt & status         */
   #define  CLRSELDI    0x20     /* clear seldi interrupt & status         */
   #define  CLRSELINGO  0x10     /* clear selingo interrupt & status       */
   #define  CLRSWRAP    0x08     /* clear swrap interrupt & status         */
   #define  CLRSDONE    0x04     /* clear sdone interrupt & status         */
   #define  CLRSPIORDY  0x02     /* clear spiordy interrupt & status       */
   #define  CLRDMADONE  0x01     /* clear dmadone interrupt & status       */

/* SSTAT0 defs */
   #define  TARGET      0x80     /* mode = target                          */
   #define  SELDO       0x40     /* selection out completed                */
   #define  SELDI       0x20     /* have been reselected                   */
   #define  SELINGO     0x10     /* arbitration won, selection started     */
   #define  SWRAP       0x08     /* transfer counter has wrapped around    */
   #define  SDONE       0x04     /* not used in mode = initiator           */
   #define  SPIORDY     0x02     /* auto pio enabled & ready to xfer data  */
   #define  DMADONE     0x01     /* transfer completely done               */

#ifdef MIPS_BE
#define  CLRSINT1       0x0f     /* clear scsi interrupts 1   (write)      */
#define  SSTAT1         0x0f     /* scsi status 1       (read)             */
#define  SSTAT2         0x0e     /* scsi status 2       (read)             */
#define  SSTAT3         0x0d     /* scsi status 3       (read)             */
#define  SCSITEST       0x0c     /* scsi test control   (read/write)       */
#else
#define  CLRSINT1       0x0c     /* clear scsi interrupts 1   (write)      */
#define  SSTAT1         0x0c     /* scsi status 1       (read)             */
#define  SSTAT2         0x0d     /* scsi status 2       (read)             */
#define  SSTAT3         0x0e     /* scsi status 3       (read)             */
#define  SCSITEST       0x0f     /* scsi test control   (read/write)       */
#endif

/* CLRSINT1 defs */
   #define  CLRSELTIMO  0x80     /* clear selto interrupt & status         */
   #define  CLRATNO     0x40     /* clear atno control signal              */
   #define  CLRSCSIRSTI 0x20     /* clear scsirsti interrupt & status      */
   #define  CLRBUSFREE  0x08     /* clear busfree interrupt & status       */
   #define  CLRSCSIPERR 0x04     /* clear scsiperr interrupt & status      */
   #define  CLRPHASECHG 0x02     /* clear phasechg interrupt & status      */
   #define  CLRREQINIT  0x01     /* clear reqinit interrupt & status       */

/* SSTAT1 defs */
   #define  SELTO       0x80     /* selection timeout                      */
   #define  ATNTARG     0x40     /* mode = target:  initiator set atn      */
   #define  SCSIRSTI    0x20     /* other device asserted scsi reset       */
   #define  PHASEMIS    0x10     /* actual scsi phase <> expected          */
   #define  BUSFREE     0x08     /* bus free occurred                      */
   #define  SCSIPERR    0x04     /* scsi parity error                      */
   #define  PHASECHG    0x02     /* scsi phase change                      */
   #define  REQINIT     0x01     /* latched req                            */

/* SSTAT 2 defs */
   #define  SFCNT4      0x10     /* scsi fifo count                        */
   #define  SFCNT3      0x08     /*                                        */
   #define  SFCNT2      0x04     /*                                        */
   #define  SFCNT1      0x02     /*                                        */
   #define  SFCNT0      0x01     /*                                        */

/* SSTAT3 defs */
   #define  SCSICNT3    0x80     /*                                        */
   #define  SCSICNT2    0x40     /*                                        */
   #define  SCSICNT1    0x20     /*                                        */
   #define  SCSICNT0    0x10     /*                                        */
   #define  OFFCNT3     0x08     /* current scsi offset count              */
   #define  OFFCNT2     0x04     /*                                        */
   #define  OFFCNT1     0x02     /*                                        */
   #define  OFFCNT0     0x01     /*                                        */

/* SCSITEST defs */
   #define  CNTRTEST    0x02     /*                                        */
   #define  CMODE       0x01     /*                                        */

#ifdef MIPS_BE
#define  SIMODE0        0x13     /* scsi interrupt mask 0     (read/write) */
#define  SIMODE1        0x12     /* scsi interrupt mask 1     (read/write) */
#define  SCSIBUSL       0x11     /* scsi data bus, lo  direct (read)       */
#define  SCSIBUSH       0x10     /* scsi data bus, hi  direct (read)       */
#else
#define  SIMODE0        0x10     /* scsi interrupt mask 0     (read/write) */
#define  SIMODE1        0x11     /* scsi interrupt mask 1     (read/write) */
#define  SCSIBUSL       0x12     /* scsi data bus, lo  direct (read)       */
#define  SCSIBUSH       0x13     /* scsi data bus, hi  direct (read)       */
#endif

/* SIMODE0 defs */

   #define  ENSELDO     0x40     /* enable seldo status to assert int      */
   #define  ENSELDI     0x20     /* enable seldi status to assert int      */
   #define  ENSELINGO   0x10     /* enable selingo status to assert int    */
   #define  ENSWRAP     0x08     /* enable swrap status to assert int      */
   #define  ENSDONE     0x04     /* enable sdone status to assert int      */
   #define  ENSPIORDY   0x02     /* enable spiordy status to assert int    */
   #define  ENDMADONE   0x01     /* enable dmadone status to assert int    */

/* SIMODE1 defs */

   #define  ENSELTIMO   0x80     /* enable selto status to assert int      */
   #define  ENATNTARG   0x40     /* enable atntarg status to assert int    */
   #define  ENSCSIRST   0x20     /* enable scsirst status to assert int    */
   #define  ENPHASEMIS  0x10     /* enable phasemis status to assert int   */
   #define  ENBUSFREE   0x08     /* enable busfree status to assert int    */
   #define  ENSCSIPERR  0x04     /* enable scsiperr status to assert int   */
   #define  ENPHASECHG  0x02     /* enable phasechg status to assert int   */
   #define  ENREQINIT   0x01     /* enable reqinit status to assert int    */


#ifdef MIPS_BE
#define  SHADDR0        0x17     /* scsi/host address      (read)          */
#define  SHADDR1        0x16     /*                                        */
#define  SHADDR2        0x15     /* host address incremented by scsi ack   */
#define  SHADDR3        0x14     /*                                        */
#else
#define  SHADDR0        0x14     /* scsi/host address      (read)          */
#define  SHADDR1        0x15     /*                                        */
#define  SHADDR2        0x16     /* host address incremented by scsi ack   */
#define  SHADDR3        0x17     /*                                        */
#endif

#ifdef MIPS_BE
#define  SELID          0x1a     /* selection/reselection id  (read)       */
#else
#define  SELID          0x19     /* selection/reselection id  (read)       */
#endif

   #define  SELID3      0x80     /* binary id of other device              */
   #define  SELID2      0x40     /*                                        */
   #define  SELID1      0x20     /*                                        */
   #define  SELID0      0x10     /*                                        */
   #define  ONEBIT      0x08     /* non-arbitrating selection detection    */


#ifdef MIPS_BE
#define  BRDCTL         0x1e
#define  SEEPROM        0x1d     /* Serial EEPROM (read/write)             */
#define  SBLKCTL        0x1c     /* scsi block control     (read/write)    */
#else
#define  BRDCTL         0x1d
#define  SEEPROM        0x1e     /* Serial EEPROM (read/write)             */
#define  SBLKCTL        0x1f     /* scsi block control     (read/write)    */
#endif

/* BRDCTL defs */
   #define  BRDDAT7	0x80     /* data bit 2                             */
   #define  BRDDAT6	0x40     /* data bit 1                             */
   #define  BRDDAT5	0x20     /* data bit 0                             */
   #define  BRDSTB	0x10     /* board strobe                           */
   #define  BRDCS	0x08     /* board chip select                      */
   #define  BRDRW	0x04     /* board read/write                       */

/* SEEPROM defs */

   #define  EXTARBACK   0x80     /* external arbitration acknowledge */
   #define  EXTARBREQ   0x40     /* external arbitration request */
   #define  SEEMS       0x20     /* serial EEPROM mode select */
   #define  SEERDY      0x10     /* serial EEPROM ready */
   #define  SEECS       0x08     /* serial EEPROM chip select */
   #define  SEECK       0x04     /* serial EEPROM clock */
   #define  SEEDO       0x02     /* serial EEPROM data out */
   #define  SEEDI       0x01     /* serial EEPROM data in */

/* SBLKCTL defs */

   #define  DIAGLEDEN      0x80  /* diagnostic led enable                  */
   #define  DIAGLEDON      0x40  /* diagnostic led on                      */
   #define  AUTOFLUSHDIS   0x20  /* disable automatic flush                */
   #define  SELBUS1        0x08  /* select scsi channel 1                  */
   #define  SELWIDE        0x02  /* scsi wide hardware configure           */


/****************************************************************************

 LANCE SCRATCH RAM AREA
 (Some scratch usage are defined in seq_off.h)
****************************************************************************/
#define SCRATCH                  0x20           /* scratch base address    */

   /* The Following SCRATCH Registers Modified for LANCE */

#ifdef MIPS_BE
#define SCRATCH_SCB_PTR_ARRAY    SCRATCH+0x1e   /* scb pointer array       */
#define SCRATCH_QIN_CNT          SCRATCH+0x22   /* queue in count          */
#define SCRATCH_QIN_PTR_ARRAY    SCRATCH+0x21   /* queue in pointer array  */
#define SCRATCH_NEXT_SCB_ARRAY   SCRATCH+0x25   /* next scb array          */
#define SCRATCH_QOUT_PTR_ARRAY   SCRATCH+0x35   /* queue out ptr array     */
#define SCRATCH_BUSY_PTR_ARRAY   SCRATCH+0x39   /* busy ptr array          */
#else
#define SCRATCH_SCB_PTR_ARRAY    SCRATCH+29     /* scb pointer array       */
#define SCRATCH_QIN_CNT          SCRATCH+33     /* queue in count          */
#define SCRATCH_QIN_PTR_ARRAY    SCRATCH+34     /* queue in pointer array  */
#define SCRATCH_NEXT_SCB_ARRAY   SCRATCH+38     /* next scb array          */
#define SCRATCH_QOUT_PTR_ARRAY   SCRATCH+54     /* queue out ptr array     */
#define SCRATCH_BUSY_PTR_ARRAY   SCRATCH+58     /* busy ptr array          */
#endif

/****************************************************************************

 LANCE SEQUENCER REGISTERS

****************************************************************************/

#ifdef MIPS_BE
#define  SEQCTL         0x63     /* sequencer control      (read/write)    */
#define  SEQRAM         0x62     /* sequencer ram data        (read/write) */
#define  SEQADDR0       0x61     /* sequencer address 0       (read/write) */
#define  SEQADDR1       0x60     /* sequencer address 1       (read/write) */
#else
#define  SEQCTL         0x60     /* sequencer control      (read/write)    */
#define  SEQRAM         0x61     /* sequencer ram data        (read/write) */
#define  SEQADDR0       0x62     /* sequencer address 0       (read/write) */
#define  SEQADDR1       0x63     /* sequencer address 1       (read/write) */
#endif

   #define  PERRORDIS   0x80     /* enable sequencer parity errors         */
   #define  PAUSEDIS    0x40     /* disable pause by driver                */
   #define  FAILDIS     0x20     /* disable illegal opcode/address int     */
   #define  FASTMODE    0x10     /* sequencer clock select                 */
   #define  BRKINTEN    0x08     /* break point interrupt enable           */
   #define  STEP        0x04     /* single step sequencer program          */
   #define  SEQRESET    0x02     /* clear sequencer program counter        */
   #define  LOADRAM     0x01     /* enable sequencer ram loading           */



#ifdef MIPS_BE
#define  ACCUM          0x67     /* accumulator               (read/write) */
#define  SINDEX         0x66     /* source index register     (read/write) */
#define  DINDEX         0x65     /* destination index register(read/write) */
#define  BRKADDR0       0x64     /* break address, lo         (read/write) */
#else
#define  ACCUM          0x64     /* accumulator               (read/write) */
#define  SINDEX         0x65     /* source index register     (read/write) */
#define  DINDEX         0x66     /* destination index register(read/write) */
#define  BRKADDR0       0x67     /* break address, lo         (read/write) */
#endif

#ifdef MIPS_BE
#define  BRKADDR1       0x6b     /* break address, hi         (read/write) */
#define  ALLONES        0x6a     /* all ones, src reg = 0ffh  (read)       */
#define  ALLZEROS       0x69     /* all zeros, src reg = 00h  (read)       */
#define  NONE           0x69     /* no destination, No reg altered (write) */
#define  FLAGS_R        0x68     /* flags            (read)                */
#else
#define  BRKADDR1       0x68     /* break address, hi         (read/write) */
#define  ALLONES        0x69     /* all ones, src reg = 0ffh  (read)       */
#define  ALLZEROS       0x6a     /* all zeros, src reg = 00h  (read)       */
#define  NONE           0x6a     /* no destination, No reg altered (write) */
#define  FLAGS_R        0x6b     /* flags            (read)                */
#endif

   #define  BRKDIS      0x80     /* disable breakpoint                     */
   #define  BRKADDR08   0x01     /* breakpoint addr, bit 8                 */

   #define  ZERO        0x02     /* sequencer 'zero' flag                  */
   #define  CARRY       0x01     /* sequencer 'carry' flag                 */


#ifdef MIPS_BE
#define  SINDIR         0x6f     /* source index reg, indirect(read)       */
#define  DINDIR         0x6e     /* destination index reg, indirect(read)  */
#define  FUNCTION1      0x6d     /* funct: bits 6-4 -> 1-of-8 (read/write) */
#define  STACK          0x6c     /* stack, for subroutine returns  (read)  */
#else
#define  SINDIR         0x6c     /* source index reg, indirect(read)       */
#define  DINDIR         0x6d     /* destination index reg, indirect(read)  */
#define  FUNCTION1      0x6e     /* funct: bits 6-4 -> 1-of-8 (read/write) */
#define  STACK          0x6f     /* stack, for subroutine returns  (read)  */
#endif

/****************************************************************************

 LANCE HOST REGISTERS

****************************************************************************/

#ifdef MIPS_BE
#define  VENDID0        0x83     /* PCI id                    (read/write) */
#define  VENDID1        0x82     /* PCI id                    (read/write) */
#define  DEVID0         0x81     /* PCI id                    (read/write) */
#define  DEVID1         0x80     /* PCI id                    (read/write) */
#else
#define  VENDID0        0x80     /* PCI id                    (read/write) */
#define  VENDID1        0x81     /* PCI id                    (read/write) */
#define  DEVID0         0x82     /* PCI id                    (read/write) */
#define  DEVID1         0x83     /* PCI id                    (read/write) */
#endif

   #define  HA_ID_HI    0x04
   #define  HA_ID_LO    0x90
   #define  HA_PROD_HI  0x78
   #define  HA_PROD_LO  0x70

#ifdef MIPS_BE
#define  COMMAND        0x87     /* PCI command               (read/write) */
#define  LATTIME        0x86     /* latency timer             (read/write) */
#define  PCISTATUS      0x85     /* PCI status                (read/write) */
#define  HCNTRL         0x84     /* host control              (read/write) */
#else
#define  COMMAND        0x84     /* PCI command               (read/write) */
#define  LATTIME        0x85     /* latency timer             (read/write) */
#define  PCISTATUS      0x86     /* PCI status                (read/write) */
#define  HCNTRL         0x87     /* host control              (read/write) */
#endif

/* command defs */
   #define  CACHETHEN   0x80     /*                                        */
   #define  DPARCKEN    0x40     /*                                        */
   #define  MPARCKEN    0x20     /*                                        */
   #define  EXTREQLCK   0x10     /*                                        */
   #define  DSSERRESPEN 0x08     /* (read only)                            */
   #define  DSPERRESPEN 0x04     /* (read only)                            */
   #define  DSMWRICEN   0x02     /* (read only)                            */
   #define  DSMASTEREN  0x01     /* (read only)                            */

/* lattime defs */
   #define  LATT7       0x80     /* latency timer time        (read)       */
   #define  LATT6       0x40     /*                                        */
   #define  LATT5       0x20     /*                                        */
   #define  LATT4       0x10     /*                                        */
   #define  LATT3       0x08     /*                                        */
   #define  LATT2       0x04     /*                                        */
   #define  HADDLDSEL1  0x02     /* host address load select               */
   #define  HADDLDSEL0  0x01     /*                                        */

/* PCI status defs */
   #define  DFTHRSH1    0x80     /* data fifo threshold control            */
   #define  DFTHRSH0    0x40     /*                                        */
   #define  DSDPR       0x20     /*                                        */
   #define  DSDPE       0x10     /*                                        */
   #define  DSSSE       0x08     /*                                        */
   #define  DSRMA       0x04     /*                                        */
   #define  DSRTA       0x02     /*                                        */
   #define  DSSTA       0x01     /*                                        */

/* host control defs */
   #define  POWRDN      0x40     /* power down                             */
   #define  BANKSEL     0x20     /* scratch bank select                    */
   #define  SWINT       0x10     /* force interrupt                        */
   #define  IRQMS       0x08     /* 0 = high true edge, 1 = low true level */
   #define  PAUSE       0x04     /* pause sequencer            (write)     */
   #define  PAUSEACK    0x04     /* sequencer is paused        (read)      */
   #define  INTEN       0x02     /* enable hardware interrupt              */
   #define  CHIPRESET   0x01     /* device hard reset                      */


#ifdef MIPS_BE
#define  HADDR0         0x8b     /* host address 0            (read/write) */
#define  HADDR1         0x8a     /* host address 1            (read/write) */
#define  HADDR2         0x89     /* host address 2            (read/write) */
#define  HADDR3         0x88     /* host address 3            (read/write) */
#else
#define  HADDR0         0x88     /* host address 0            (read/write) */
#define  HADDR1         0x89     /* host address 1            (read/write) */
#define  HADDR2         0x8a     /* host address 2            (read/write) */
#define  HADDR3         0x8b     /* host address 3            (read/write) */
#endif


#ifdef MIPS_BE
#define  HCNT0          0x8f     /* host count 0              (read/write) */
#define  HCNT1          0x8e     /* host count 1              (read/write) */
#define  HCNT2          0x8d     /* host count 2              (read/write) */
#else
#define  HCNT0          0x8c     /* host count 0              (read/write) */
#define  HCNT1          0x8d     /* host count 1              (read/write) */
#define  HCNT2          0x8e     /* host count 2              (read/write) */
#endif

#ifdef MIPS_BE
#define  SCBPTR         0x93     /* scb pointer               (read/write) */
#define  INTSTAT        0x92     /* interrupt status          (read)       */
#define  CLRINT         0x91     /* clear interrupt status    (write)      */
#define  ERROR          0x91     /* hard error                (read)       */
#define  DFCNTRL        0x90     /* data fifo control register(read/write) */
#else
#define  SCBPTR         0x90     /* scb pointer               (read/write) */
#define  INTSTAT        0x91     /* interrupt status          (read)       */
#define  CLRINT         0x92     /* clear interrupt status    (write)      */
#define  ERROR          0x92     /* hard error                (read)       */
#define  DFCNTRL        0x93     /* data fifo control register(read/write) */
#endif

/* SCBPTR defs */
   #define  SCBVAL2     0x04     /* label of element in scb array          */
   #define  SCBVAL1     0x02     /*                                        */
   #define  SCBVAL0     0x01     /*                                        */

/* INTSTAT defs */
   #define  INTCODE3    0x80     /* seqint interrupt code                  */
   #define  INTCODE2    0x40     /*                                        */
   #define  INTCODE1    0x20     /*                                        */
   #define  INTCODE0    0x10     /*                                        */
   #define  BRKINT      0x08     /* program count = breakpoint             */
   #define  SCSIINT     0x04     /* scsi event interrupt                   */
   #define  CMDCMPLT    0x02     /* scb command complete w/ no error       */
   #define  SEQINT      0x01     /* sequencer paused itself                */
   #define  INTCODE     INTCODE3+INTCODE2+INTCODE1+INTCODE0 /* intr code   */
   #define  ANYINT      BRKINT+SCSIINT+CMDCMPLT+SEQINT   /* any interrupt  */
   #define  ANYPAUSE    BRKINT+SCSIINT+SEQINT   /* any intr that pauses    */

/* ;  Bits 7-4 are written to '0' or '1' by sequencer.
   ;  Bits 3-0 can only be written to '1' by sequencer.  Previous '1's are
   ;  preserved by the write. */

/* CLRINT defs */
   #define  CLRBRKINT   0x08     /* clear breakpoint interrupt  (brkint)   */
   #define  CLRSCSINT   0x04     /* clear scsi interrupt  (scsiint)        */
   #define  CLRCMDINT   0x02     /* clear command complete interrupt       */
   #define  CLRSEQINT   0x01     /* clear sequencer interrupt  (seqint)    */

/* ERROR defs */
   #define  PARERR      0x08     /* status = sequencer ram parity error    */
   #define  ILLOPCODE   0x04     /* status = illegal command line          */
   #define  ILLSADDR    0x02     /* status = illegal sequencer address     */
   #define  ILLHADDR    0x01     /* status = illegal host address          */

/* DFCNTRL defs */
   #define  WIDEODD     0x40     /* prevent flush of odd last byte         */
   #define  SCSIEN      0x20     /* enable xfer: scsi <-> sfifo    (write) */
   #define  SCSIENACK   0x20     /* SCSIEN clear acknowledge       (read)  */
   #define  SDMAEN      0x10     /* enable xfer: sfifo <-> dfifo   (write) */
   #define  SDMAENACK   0x10     /* SDMAEN clear acknowledge       (read)  */
   #define  HDMAEN      0x08     /* enable xfer: dfifo <-> host    (write) */
   #define  HDMAENACK   0x08     /* HDMAEN clear acknowledge       (read)  */
   #define  DIRECTION   0x04     /* transfer direction = write             */
   #define  FIFOFLUSH   0x02     /* flush data fifo to host                */
   #define  FIFORESET   0x01     /* reset data fifo                        */


#ifdef MIPS_BE
#define  DFSTATUS       0x97     /* data fifo status          (read)       */
#define  DFWADDR0       0x96     /* data fifo write address   (read/write) */
#define  DFWADDR1       0x95     /* reserved         (read/write)          */
#define  DFRADDR0       0x94     /* data fifo read address    (read/write) */
#else
#define  DFSTATUS       0x94     /* data fifo status          (read)       */
#define  DFWADDR0       0x95     /* data fifo write address   (read/write) */
#define  DFWADDR1       0x96     /* reserved         (read/write)          */
#define  DFRADDR0       0x97     /* data fifo read address    (read/write) */
#endif

   #define  MREQPEND    0x10     /* master request pending                 */
   #define  HDONE       0x08     /* host transfer done:                    */
                                 /*  hcnt=0 & bus handshake done           */
   #define  DFTHRSH     0x04     /* threshold reached                      */
   #define  FIFOFULL    0x02     /* data fifo full                         */
   #define  FIFOEMP     0x01     /* data fifo empty                        */


#ifdef MIPS_BE
#define  DFRADDR1       0x9b     /* reserved         (read/write)          */
#define  DFDAT          0x9a     /* data fifo data register   (read/write) */
#define  SCBCNT         0x99     /* SCB count register        (read/write) */
#define  QINFIFO        0x98     /* queue in fifo             (read/write) */
#else
#define  DFRADDR1       0x98     /* reserved         (read/write)          */
#define  DFDAT          0x99     /* data fifo data register   (read/write) */
#define  SCBCNT         0x9a     /* SCB count register        (read/write) */
#define  QINFIFO        0x9b     /* queue in fifo             (read/write) */
#endif

   #define  SCBAUTO     0x80     /*                                        */


#ifdef MIPS_BE
#define  QINCNT         0x9f     /* queue in count            (read)       */
#define  QOUTFIFO       0x9e     /* queue out fifo            (read/write) */
#define  QOUTCNT        0x9d     /* queue out count           (read)       */
#define  SFUNCT         0x9c     /* test chip                 (read/write) */
#else
#define  QINCNT         0x9c     /* queue in count            (read)       */
#define  QOUTFIFO       0x9d     /* queue out fifo            (read/write) */
#define  QOUTCNT        0x9e     /* queue out count           (read)       */
#define  SFUNCT         0x9f     /* test chip                 (read/write) */
#endif

   #define  QINCNT2     0x04     /*                                        */
   #define  QINCNT1     0x02     /*                                        */
   #define  QINCNT0     0x01     /*                                        */

   #define  TESTHOST    0x08     /* select Host module for testing         */
   #define  TESTSEQ     0x04     /* select Sequencer module for testing    */
   #define  TESTFIFO    0x02     /* select Fifo module for testing         */
   #define  TESTSCSI    0x01     /* select Scsi module for testing         */


/****************************************************************************

 LANCE HOST REGISTERS

****************************************************************************/

#ifdef MIPS_BE
/* These can be used for single byte accesses only.  They cannot be used
 * as offsets! */

#define  SCB00          0xa3     /* scb array                 (read/write) */
#define  SCB01          0xa2
#define  SCB02          0xa1
#define  SCB03          0xa0
#define  SCB04          0xa7
#define  SCB05          0xa6
#define  SCB06          0xa5
#define  SCB07          0xa4
#define  SCB08          0xab
#define  SCB09          0xaa
#define  SCB10          0xa9
#define  SCB11          0xa8
#define  SCB12          0xaf
#define  SCB13          0xae
#define  SCB14          0xad
#define  SCB15          0xac
#define  SCB16          0xb3
#define  SCB17          0xb2
#define  SCB18          0xb1
#define  SCB19          0xb0
#define  SCB20          0xb7
#define  SCB21          0xb6
#define  SCB22          0xb5
#define  SCB23          0xb4
#define  SCB24          0xbb
#define  SCB25          0xba
#define  SCB26          0xb9
#define  SCB27          0xb8
#define  SCB28          0xbf
#define  SCB29          0xbe
#define  SCB30          0xbd
#define  SCB31          0xbc
#else /* MIPS_BE */
#define  SCB00          0xa0     /* scb array                 (read/write) */
#define  SCB01          0xa1
#define  SCB02          0xa2
#define  SCB03          0xa3
#define  SCB04          0xa4
#define  SCB05          0xa5
#define  SCB06          0xa6
#define  SCB07          0xa7
#define  SCB08          0xa8
#define  SCB09          0xa9
#define  SCB10          0xaa
#define  SCB11          0xab
#define  SCB12          0xac
#define  SCB13          0xad
#define  SCB14          0xae
#define  SCB15          0xaf
#define  SCB16          0xb0
#define  SCB17          0xb1
#define  SCB18          0xb2
#define  SCB19          0xb3
#define  SCB20          0xb4
#define  SCB21          0xb5
#define  SCB22          0xb6
#define  SCB23          0xb7
#define  SCB24          0xb8
#define  SCB25          0xb9
#define  SCB26          0xba
#define  SCB27          0xbb
#define  SCB28          0xbc
#define  SCB29          0xbd
#define  SCB30          0xbe
#define  SCB31          0xbf
#endif /* MIPS_BE */


/****************************************************************************

 LANCE PCI CONFIGURATION REGISTERS

****************************************************************************/

#define  ID_REG         0x0000

   #define  HA_ID_MASK  0x00789004  /* Mask for "ADP78xx"                  */
   #define  SABRE_ID    0x10789004  /* Mask for "ADP78xx"                  */
   #define  VIKING_SABRE   0x7810   /* Id for Sabre chip                */
   #define  VIKING_LANCE   0x7873   /* Id for Lance on Viking board     */
   #define  VLIGHT_LANCE   0x7872   /* Id for Sabre on Viking board     */
   #define  AHA2940	0x00780000  /* Id for 7870 on the 2940 board (BE) */


#define  STATUS_CMD_REG 0x0004      /* Status/Command registers            */

   #define  DPE         0x80000000  /* Detected parity error               */
   #define  DPE_MASK    0x8000FFFF  /*                                     */
   #define  SSE         0x40000000  /* Signal system error                 */
   #define  SSE_MASK    0x4000FFFF  /*                                     */
   #define  RMA         0x20000000  /* Received master abort               */
   #define  RMA_MASK    0x2000FFFF  /*                                     */
   #define  RTA         0x10000000  /* Received target abort               */
   #define  RTA_MASK    0x1000FFFF  /*                                     */
   #define  STA         0x08000000  /* Signal target abort                 */
   #define  STA_MASK    0x0800FFFF  /*                                     */
   #define  DPR         0x01000000  /* Data parity reported                */
   #define  DPR_MASK    0x0100FFFF  /*                                     */
   #define  MASTEREN    0x00000004  /* Master enable                       */
   #define  MSPACEEN    0x00000002  /* Memory space enable                 */
   #define  ISPACEEN    0x00000001  /* IO space enable                     */
   #define  ENABLE_MASK MASTEREN + ISPACEEN

#define  DEV_REV_ID     0x0008      /* Device revision id                  */

#define  BASE_IO_ADDR_REG  0x0010   /* Base Port Address for I/O space     */

#define  BASE_MEM_ADDR_REG  0x0014  /* Base Port Address for Memory space  */

#define  INTR_LEVEL_REG 0x003c      /* Interrupt Level (low 8 bits)        */

#define  BUS_NUMBER     0x0018      /* bus number etc                      */

#define  DEVCONFIG      0x0040      /* Device configuration register       */

   #define  DIFACTNEGEN 0x00000001  /* Differential active negation enable */
   #define  STPWLEVEL   0x00000002  /* SCSI termination power down mode sel*/
   #define  DACEN       0x00000004  /* Dual address cycle enable           */
   #define  BERREN      0x00000008  /* Byte parity error enable            */
   #define  EXTSCBPEN   0x00000010  /* External SCB parity enable          */
   #define  EXTSCBTIME  0x00000020  /* External SCB time                   */
   #define  MRDCEN      0x00000040 
   #define  SCBRAMSEL   0x00000080  /* External SCB RAM select             */
   #define  VOLSENSE    0x00000100  /* Voltage sense                       */
   #define  RAMPSM      0x00000200  /* RAM present                         */
   #define  MPORTMODE   0x00000400
   #define  TESTBITEN   0x00000800  /* Test Bit Enable                     */
   #define  REXTVALID   0x00001000  /* External Resister Valid - FAST20    */


/****************************************************************************

 Driver - Sequencer interface

****************************************************************************/

/* INTCODES  - */

/* Seqint Driver interrupt codes identify action to be taken by the Driver.*/

#define  SYNC_NEGO_NEEDED  0x00     /* initiate synchronous negotiation    */
#define  ATN_TIMEOUT       0x00     /* timeout in atn_tmr routine          */
#define  CDB_XFER_PROBLEM  0x10     /* possible parity error in cdb:  retry*/
#define  HANDLE_MSG_OUT    0x20     /* handle Message Out phase            */
#define  DATA_OVERRUN      0x30     /* data overrun detected               */
#define  UNKNOWN_MSG       0x40     /* handle the message in from target   */
#define  CHECK_CONDX       0x50     /* Check Condition from target         */
#define  PHASE_ERROR       0x60     /* unexpected scsi bus phase           */
#define  EXTENDED_MSG      0x70     /* handle Extended Message from target */
#define  ABORT_TARGET      0x80     /* abort connected target              */
#define  NO_ID_MSG         0x90     /* reselection with no id message      */


/****************************************************************************

EEPROM information definitions

****************************************************************************/
#define EEPROM_SIZE        64       /* EEPROM size */

/* offset for EEPROM information */

#define  EE_TARGET0        0        /* target 0 */
#define  EE_TARGET1        2        /* target 1 */
#define  EE_TARGET2        4        /* target 2 */
#define  EE_TARGET3        6        /* target 3 */
#define  EE_TARGET4        8        /* target 4 */
#define  EE_TARGET5        10       /* target 5 */
#define  EE_TARGET6        12       /* target 6 */
#define  EE_TARGET7        14       /* target 7 */
#define  EE_TARGET8        16       /* target 8 */
#define  EE_TARGET9        18       /* target 9 */
#define  EE_TARGET10       20       /* target 10 */
#define  EE_TARGET11       22       /* target 11 */
#define  EE_TARGET12       24       /* target 12 */
#define  EE_TARGET13       26       /* target 13 */
#define  EE_TARGET14       28       /* target 14 */
#define  EE_TARGET15       30       /* target 15 */
#define  EE_BIOSFLAG       32       /* bios flags */
#define  EE_CONTROL        34       /* scsi channel control */
#define  EE_ID             36       /* scsi id */
#define  EE_BUSRLS         37       /* bus release */
#define  EE_MAX_TARGET     38       /* max no of targets */ 

/* definitions for scsi channel control (EE_CONTROL) */

#define E2C_FAST20         0x80     /* enable/disable fast20 scsi */
#define E2C_RESET          0x40     /* enable/disable scsi reset */
#define E2C_PARITY         0x10     /* enable/disable parity */
#define E2C_TERM_LOW       0x04     /* enable/disable low byte termination */
#define E2C_TERM_HIGH      0x08     /* enable/disable high byte termination */
#define E2C_TERMINATION    E2C_TERM_LOW | E2C_TERM_HIGH

/* definitions for target control flags (EE_TARGET?) */

#define E2T_SYNCRATE       0x07     /* synchronous transfer rate */
#define E2T_SYNCMODE       0x08     /* synchronous enable */
#define E2T_DISCONNECT     0x10     /* disconnect enable */
#define E2T_WIDE           0x20     /* intiate wide negotiation */

/* SCB usage for External Access method */

#define HWSCB_BIOS      0           /* BIOS uses SCB 0 */
#define HWSCB_DRIVER    1           /* Driver uses SCB 1 */
#define HWSCB_STORAGE   2           /* SCB 2 used for parameter storage */
