/* I don't really care what the system pagesize is -- 4K good enough */

#undef NBPC
#undef btoc
#undef ctob

#define NBPC 4096
#define btoc(x) (((__uint64_t)(x)+(NBPC-1))/NBPC)
#define ctob(x) ((__uint64_t)(x)*NBPC)
