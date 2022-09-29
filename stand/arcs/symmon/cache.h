/*
 * Cache data structures for cache dump and coherency checker
 */

#ident "symmon/cache.h: $Revision: 1.3 $"

#ifndef __CACHE_H__
#define __CACHE_H__

/*
 * These values shouldn't really be hardwired in this file. XXX
 * DO NOT USE THESE VALUES -- they're only temporary!!!!!!
 */
#define PCacheLineSize          16
#define PCacheSize        	0x4000
#define SCacheLineSize        	128
#define SCacheSize        	0x100000
#define NumberOfSCacheLines	(SCacheSize / SCacheLineSize)
#define EverestMaxCPUs		56
#define BusTagDataMask 		((0x20000000/(SCacheSize>>10))-1)
#define BusTagBaseAddr		EV_BUSTAG_BASE	/* Defined in IP19.h */

#define Invalid	 	0
#define CleanExclusive	4
#define DirtyExclusive	5
#define Shared		6
#define DirtyShared	7	
#define BusInvalid	0
#define BusExclusive	2
#define BusShared	3

#define Addr2BusTag(addr)	\
	((((__psunsigned_t)((addr) & (SCacheSize-1))>>7)<<3)+BusTagBaseAddr)
#define PhysicalTag(tag) ((int) ((tag)>>13)&0x7FFFF)
#define CacheState(tag) ((int) ((tag)>>10)&7)
#define BusTagState(busTag) ((int) ((busTag)>>22)&3)
#define BusPhysTag(bTag) ((int) ((bTag)/(SCacheSize/0x40000))&BusTagDataMask)

#define TagsMatch(STagLo,bustag) (((STagLo)>>3) == (bustag))

typedef struct _Tags
{
  unsigned int  sTag;
  unsigned int  busTag;
} Tags;

typedef struct _CallStruct
{ /* what we call the dump routine with */
  u_numinp_t readAddr;
  Tags *tags;
  int done; /* we set this to show we're done */
} CallStruct;

void memDiffDisplay(char *a,char *b,int length,char *titleA,char *titleB);
char *cacheStateStr(int cacheState);
char *busTagStateStr(int busTagState);
int cacheStateCheck(Tags cpuTags[],
                    int cpuThere[],
                    unsigned int cacheLineOffset);


#endif /* __CACHE_H__ */
