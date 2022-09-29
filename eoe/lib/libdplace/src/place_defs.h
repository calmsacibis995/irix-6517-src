#define _SBRK __sbrk
#define SBRK  _sbrk

#if _MIPS_SZPTR == 32
#define TOPOFSTACK 0x7fff8000L
#endif

#if _MIPS_SZPTR == 64
#define TOPOFSTACK 0x10000000000
#endif

#define d __dplace_d
typedef struct
    { long long start,end; int node,pagesize,placed; } range_t;

typedef struct
    { int count; range_t *ranges; } node_mapping_t;

typedef struct
    { int valid,exists,pid,thread,node,cpu,cpu_link; } link_t;

enum topology_type { cube, cluster, physical, none };

enum affinity_type { None , Close };

enum policy_type { thread_local , fixed };

enum distribution { block, cyclic , dist_default };

struct list { char *name; struct list *next; };


