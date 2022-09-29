/* 
** prom_callout.h -- version $Revision: 1.8 $
*/
#ifndef PROM_CALLOUT_H
#define PROM_CALLOUT_H

#include "sys/types.h"
#include "arcs/folder.h"

typedef struct {
    unsigned (*get_pgmask)(void);
    void (*set_pgmask)(unsigned);
    void (*tlbwired)(int, int, caddr_t, unsigned int, unsigned int);
    char *(*strcpy)(char *, const char *);
    char *(*strcat)(char *, const char *);
    int (*strlen)(const char *);
    COMPONENT *(*AddChild)(COMPONENT *, COMPONENT *, void *);
    long (*RegisterDriverStrategy)(COMPONENT *, STATUS (*)(COMPONENT *self, IOBLOCK *));
    int (*_gfx_strat)(COMPONENT *, IOBLOCK *);
    int (*cpuid)(void);
    void (*us_delay)(int);
    long long (*load_double)(long long *);
    void (*store_double)(long long *, long long);
    long long (*__ll_lshift)(long long, long long);
    void (*bzero)(void *, long);
    void (*bcopy)(const void *, void *, long);
    k_machreg_t (*get_SR)(void);
    void (*set_SR)(k_machreg_t);
    int (*printf)(const char *, ...);
    unsigned char *clogo_data_addr;
    int *clogo_w_addr;
    int *clogo_h_addr;
#ifdef EVEREST
    /*
     * For everest systems only.  Initialised by the IO4 prom, read
     * by the graphics prom code.  The IO4 prom knows of the existence
     * of all graphics pipes within the system, it picks one of the
     * graphics pipes as the prom textport, and pass that information
     * to the graphics prom so that the graphics prom code knows where
     * to bring up the prom textport.
     */
    int tp_io4slot;	/* Slot number of the IO4 connected to textport */
    int tp_window;	/* Window number of the textport's graphics pipe */
    int tp_adapter;	/* adapter number of the textport's graphics pipe */
    void *tp_vaddr;	/* base virtual address used to access graphics */
#if defined(IP21) || defined(IP25)
    void (*fifocmd)(int, int);	/* Routine to write the fifo through the write-gatherer */
    void (*fifodata)(int, int);	/* Routine to write the fifo through the write-gatherer */
    void (*wg_flush)(void);	/* Routine to flush the write gatherer */
#endif /* IP21 || IP25 */
#endif /* EVEREST */
} prom_callout_t;

#endif /* PROM_CALLOUT_H */

