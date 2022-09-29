
#include        "sys/types.h"
#include        "sys/sema.h"
#include        "sys/rrm.h"
#include        "sys/gfx.h"

#ifdef LG1

#include        "sys/lg1.h"
#include        "sys/lg1hw.h"

#define 	PIXDMA	LG1_PIXELDMA
#define 	WRITE	LG1_WRITE
#define 	STRIDE	LG1_STRIDE
#define 	DMA_MAXSIZE LG1_DMA_MAXSIZE 
#define 	DMA_MAXXLEN LG1_DMA_MAXXLEN
#define 	DMA_MAXYLEN LG1_DMA_MAXYLEN


#define GFXCHIP Rexchip
#define rex_dma_addr	(void *)&REX->go.rwaux1
typedef struct lg1_pixeldma_args pixdma_t;
#define REXWAIT(rex)    while ((rex)->p1.set.configmode & CHIPBUSY);

#else 
#define 	NG1

#include        "sys/ng1.h"
#include        "sys/ng1hw.h"

#define 	PIXDMA	NG1_PIXELDMA
#define 	WRITE	NG1_WRITE
#define 	STRIDE	NG1_STRIDE
#define 	DMA_MAXSIZE NG1_DMA_MAXSIZE 
#define 	DMA_MAXXLEN NG1_DMA_MAXXLEN
#define 	DMA_MAXYLEN NG1_DMA_MAXYLEN

#define GFXCHIP Rex3chip
#define rex_dma_addr	(void *)&REX->go.hostrw0
typedef struct ng1_pixeldma_args pixdma_t;


#endif


extern GFXCHIP *REX;
