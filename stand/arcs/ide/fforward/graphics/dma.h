

#define DATA_WADDR      0x8401000	/* physical addr. for writing */
#define DATA_RADDR      0x8402000	/* physical addr. for reading */

extern unsigned int vdma_wphys;	/* physical address for writing (one page) */
extern unsigned int vdma_rphys;	/* physical address for reading (one page) */

#define DMA_READ        0		/* DMA from memory to gfx */
#define DMA_WRITE       0x2		/* DMA from gfx to memory */
#define VDMA_HTOG	0
#define VDMA_GTOH	0x2

/*
 * DMA_MODE register bits
 */
#define VDMA_SYNC	0x4		/* delay transfer until sync */
#define VDMA_FILL	0x8		/* write constant data to memory */
#define VDMA_INCA	0x10		/* memory address increments */
#define VDMA_DECA	0x0		/* memory address decrements */
#define VDMA_SNOOP	0x20		/* enable cache snoop (short burst) */
#define VDMA_LBURST	0x40		/* Long burst transfers */
#define VDMA_SBURST	0x0		/* Short burst transfers */

/* VDMA interrupt */
#define VDMA_COMPLETE	0
#define VDMA_PAGEFAULT	1
#define VDMA_UTLBMISS	2
#define VDMA_CLEAN	3
#define VDMA_BMEM	4
#define VDMA_BPTE	5
#define VDMA_UNKNOWN	-1

extern int vdma_wait(void);

extern int dma_error_handler(void);
extern void basic_DMA_setup(int);
extern void dma_go(caddr_t, uint, int, int, int, int, int);
extern void vc2_off(void);

