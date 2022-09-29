#ifndef _CHKFP_H_
#define _CHKFP_H_


#include "fp.h"

#define  FPCK

/* the data structure uses as temparary space
   to record the data clusters chained information */
typedef struct clusterlist_t {
    int   status;
    int   clusterno;
    struct  clusterlist_t * nextcluster;
} CLUSTERLIST;


/* detailed file and subdirectory allocation informations 
   link list structure, used for files cross linked checking */
typedef struct fallocinfo_t {
    char * name;
    int    blknum;
    int  * blkv;         /* support up to 16 bit fat system */
    struct fallocinfo_t * parentdir;
    struct fallocinfo_t * nextfile;
} FALLOCINFO;

extern int           hfsCk(void *, int);
extern FALLOCINFO  * screenFallocinfo(FALLOCINFO *, int);
extern FALLOCINFO  * fAllocInfoCell(char *, CLUSTERLIST **, int,
                                FALLOCINFO **, FALLOCINFO *, CLUSTERLIST **);
extern void          linkClusterCells(CLUSTERLIST **, CLUSTERLIST *);
extern int           countClusterCell(CLUSTERLIST *);
extern CLUSTERLIST * lastClusterCell(CLUSTERLIST *);
extern char        * getUnixPath(FALLOCINFO *, char *);
extern int           isMemberCluster(FALLOCINFO *, int);
extern FALLOCINFO  * searchCrossLinked(FALLOCINFO *, CLUSTERLIST *);
extern int           selfLinked(CLUSTERLIST *, int);
extern void          releaseFalloc(FALLOCINFO **);
#endif /* _CHKFP_H_ */
