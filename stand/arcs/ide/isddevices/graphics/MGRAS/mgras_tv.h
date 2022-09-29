
#include "mgras_hq3.h"

#undef 	HQ3_PIO_WR_RE
#undef 	HQ3_PIO_RD_RE

#ifdef TESTING
#define HQ3_PIO_WR_RE(reg, val, mask) 								\
	*(__uint32_t *)((__uint32_t) (mgbase + RSS_BASE + (HQ_RSS_SPACE(reg)))) = val & mask;

#define	HQ3_PIO_RD_RE(reg, mask, actual) 							\
	actual = (*(__uint32_t *)((__uint32_t) (mgbase + RSS_BASE + (HQ_RSS_SPACE(reg))))) & mask;

#define HQ3_PIO_WR(offset, val, mask) 								\
	*(__uint32_t *)((__uint32_t) (mgbase + offset) = val & mask;

#define	HQ3_PIO_RD(offset, mask, actual) 							\
	actual = (*(__uint32_t *)((__uint32_t) (mgbase + offset) & mask;
#else

#define HQ3_PIO_WR_RE(reg, val, mask) {								\
	printf("HQ3_PIO_WR_RE..... reg = 0x%x; val = 0x%x; mask = 0x%x\n", reg, val, mask);	\
}

#define	HQ3_PIO_RD_RE(reg, mask, actual) {							\
	printf("HQ3_PIO_RD_RE..... reg = 0x%x; mask = 0x%x;\n", reg, mask);			\
}

#define HQ3_PIO_WR(offset, val, mask) {								\
	printf("HQ3_PIO_WR..... offset = 0x%x; val = 0x%x; mask = 0x%x\n", offset, val, mask);	\
}
#define	HQ3_PIO_RD(offset, mask, actual) { 							\
	printf("HQ3_PIO_RD..... offset = 0x%x; mask = 0x%x\n", offset, mask);			\
}
#endif

