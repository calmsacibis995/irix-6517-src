/*
 * Write gatherer header file.  Contains the addresses of where to
 * write things so that stuff happens...
 *
 */



#if     defined(IP19)

#define SETUP_GE	volatile union venice_pipe *GE = \
                            (volatile union venice_pipe *)SBUS_TO_KVU(0x1830000c)

#endif



#if     (defined(IP21) || defined(IP25)) && _MIPS_SIM == _ABI64

#define SETUP_GE	volatile union venice_pipe *GE = \
                            (volatile union venice_pipe *)TO_WGSPACE(0x18300000)

#endif

#if !defined(IP19) && !defined(IP21) && !defined(IP25)
#define WGFLUSH
#endif

#define SETUP_REGS	/* Assume pipe 0 */                       \
                        volatile struct fcg *fcg = fcg_base_va
               

#if defined(IP21) || defined(IP25)
extern void fifocmd(int cmd, int val);
extern void fifodata(int cmd, int val);
extern void wg_flush(void);


#define gestore		fifodata
#define gecmdstore	fifocmd
#define wg_flush	wg_flush
#endif
#ifdef IP19
#define gecmdstore(cmd, val) { SETUP_GE; GE[cmd].i = val; } 
#define gestore(cmd, val)    { SETUP_GE; GE[cmd].i = val; } 
#define wg_flush()           { SETUP_GE; GE[CP_WG_FLUSH_0].i = 0; }
#endif
