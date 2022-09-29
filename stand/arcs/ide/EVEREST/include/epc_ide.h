/*******************************************************************
 * 
 * epc_ide.h 
 *
 *
 ********************************************************************/

typedef struct ee_info_ide {
        struct arpcom   ei_ac;

        int             ei_rbufs;       /* # receive bufs (RLIMIT) */
        caddr_t         ei_rbase;       /* base of rbufs */
        int             ei_orindex;     /* prev RINDEX */
        caddr_t         ei_epcbase;

        int             ei_tbufs;       /* # transmit bufs (TLIMIT) */
        caddr_t         ei_tbase;       /* base of tbufs */
        int             ei_tfree;       /* 1st open tbuf */
        int             ei_tdmawait;    /* dirty tbuf not queued for tmit */
} ee_info_ide;

