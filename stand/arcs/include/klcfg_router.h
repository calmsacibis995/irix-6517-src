/*
 * klcfg_router.h
 *
 * 	Companion file to klcfg_router.c.
 * 	Defines macros for PCFG struct
 *	manipulation and for handling 
 *	multiple router ports.
 */

#define ForEachRouterPort(i)                                    \
                for ((i) = 1; (i) <= MAX_ROUTER_PORTS; (i)++)   \

#define ForEachValidRouterPort(_pr,_i)                           \
                ForEachRouterPort((_i))                          \
                        if ((_pr)->port[(_i)].index != PCFG_INDEX_INVALID)

#define ForAllPcfg(_p,_i)	for ((_i) = 0; (_i) < (_p)->count; (_i)++)

#define ForAllPcfgHub(_p,_i)					\
		ForAllPcfg((_p),(_i))				\
			if ((_p)->array[(_i)].any.type == PCFG_TYPE_HUB)

#define ForAllPcfgRouter(_p,_i)					\
		ForAllPcfg((_p),(_i))				\
			if ((_p)->array[(_i)].any.type == PCFG_TYPE_ROUTER)

#define pcfgGetType(_p,_i)	(_p->array[_i].any.type)
#define pcfgGetHub(_p,_i)	(&_p->array[_i].hub)
#define pcfgGetRouter(_p,_i)	(&_p->array[_i].router)

#define pcfgIsHub(_p,_i)	(pcfgGetType(_p,_i) == PCFG_TYPE_HUB)
#define pcfgIsRouter(_p,_i)	(pcfgGetType(_p,_i) == PCFG_TYPE_ROUTER)

