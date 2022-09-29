#ifndef _TIMER_
#define	_TIMER_

#include <sys/types.h>
#include <stddef.h>

typedef uint64_t iotimer64_t;
typedef uint32_t iotimer32_t;

struct cyclecounter {
    volatile iotimer64_t *iotimer_addr64;
    volatile iotimer32_t *iotimer_addr32;
    unsigned int cycleval;
    ptrdiff_t iotimer_size;
    int fd;
};

extern	struct cyclecounter cc;			/* memory-mapped area */
extern	uint64_t readcc(void);			/* handles 32-64 bit coercion */
extern	void map_timer(void);
#endif /* _TIMER_ */
