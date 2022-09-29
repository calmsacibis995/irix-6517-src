#if !defined(__PIODMA_H__)
#define  __PIODMA_H__


#define PAGES_TO_ALLOCATE     1

#define PLP_DMA_ADDRESS           0xBF314000
#define PLP_DMA_CNTXT_A_ADDR      ((volatile long long*) 0xBF314000)
#define PLP_DMA_CNTXT_B_ADDR      ((volatile long long*) 0xBF314008)
#define PLP_DMA_CONTROL_ADDR      ((volatile long long*) 0xBF314010)
#define PLP_DMA_DIAG_ADDR         ((volatile long long*) 0xBF314018)
#define PLP_DMA_CNTXT_VALID_BITS  (long long)0x80000FFFFFFFFFFF

#ifdef notdef
#define PLP_DMA_CNTXT_A           (*(PLP_DMA_CNTXT_A_ADDR))
#define PLP_DMA_CNTXT_B           (*(PLP_DMA_CNTXT_B_ADDR))
#define PLP_DMA_CONTROL           (*(PLP_DMA_CONTROL_ADDR))
#define PLP_DMA_DIAG              (*(PLP_DMA_DIAG_ADDR))
#else /* notdef */
#define PLP_DMA_CNTXT_A           READ_REG64(PLP_DMA_CNTXT_A_ADDR, long long)
#define PLP_DMA_CNTXT_B           READ_REG64(PLP_DMA_CNTXT_B_ADDR, long long)
#define PLP_DMA_CONTROL           READ_REG64(PLP_DMA_CONTROL_ADDR, long long)
#define PLP_DMA_DIAG              READ_REG64(PLP_DMA_DIAG_ADDR, long long)
#endif /* notdef */

#define PLP_DMA_CONTROL_MASK      0x1f
#define PLP_DMA_CONTEXT_A_VALID   0x10
#define PLP_DMA_CONTEXT_B_VALID   0x08
#define PLP_DMA_RESET             0x04   
#define PLP_DMA_ENABLE            0x02
#define PLP_DMA_DIRECTION         0x01

#ifdef notdef
#define SET_PLP_DMA_CONTROL(mask)                            \
     PLP_DMA_CONTROL = PLP_DMA_CONTROL | (mask)

#define CLEAR_PLP_DMA_CONTROL(mask)                          \
     PLP_DMA_CONTROL = PLP_DMA_CONTROL & ~(mask)             \

#else /* notdef */
#define RESET_PLP_DMA_CONTROL                           \
     WRITE_REG64(PLP_DMA_RESET, PLP_DMA_CONTROL_ADDR, \
			long long)

#define SET_PLP_DMA_CONTROL(mask)                            \
     WRITE_REG64((PLP_DMA_CONTROL | (mask)), PLP_DMA_CONTROL_ADDR, \
			long long)

#define CLEAR_PLP_DMA_CONTROL(mask)                          \
     WRITE_REG64((PLP_DMA_CONTROL & (~mask)), PLP_DMA_CONTROL_ADDR, \
			(long long))

#endif /* notdef */
/* The following tests, the LAST BIT, BYTE LENGTH and ADDR  */
/* fields of the DMA context registers.                     */

#define PLP_DMA_CONTEXT_MASK      0x80000FFFFFFFFFFF
#define PLP_DMA_LAST_MASK         0x8000000000000000
#define PLP_DMA_LENGTH_MASK       0x00000FFF00000000

/* Define masks for testing bits in the DMA diagnostic reg. */

#define PLP_DMA_ACTIVE            0x2
#define PLP_DMA_CONTEXT_IN_USE    0x1

#ifdef notdef
#define INIT_PLP_DMA_CONTEXT(cntxt,base,bytes,lastXfer)      \
    *cntxt = (((long long) base |                                \
	(((long long) bytes-1) << 32) & PLP_DMA_LENGTH_MASK) |   \
        (((long long) lastXfer) << 63) & PLP_DMA_CONTEXT_MASK)   \
#else /* notdef */
#define PLP_DMA_PHYS_ADDR(base,bytes,lastXfer)                   \
	(((long long)(K1_TO_PHYS(base)) |                                     \
        (((long long) bytes-1) << 32) & PLP_DMA_LENGTH_MASK) |   \
        (((long long) lastXfer) << 63) & PLP_DMA_CONTEXT_MASK)
#define INIT_PLP_DMA_CONTEXT(cntxt,base,bytes,lastXfer)      \
	WRITE_REG64(PLP_DMA_PHYS_ADDR(base,bytes,lastXfer), cntxt, long long)
#endif /* notdef */

typedef struct {
    long long contextA;
    long long contextB;
    long long dmaControl;
    long long diagnostic;
} ParallelPortDma;

#endif
