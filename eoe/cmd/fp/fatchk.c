/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1993 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.6 $"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <bstring.h>
#include <time.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>

#include "fpck.h"
#include "dosfs.h"
#include "macLibrary.h"  /* adopt the error code */

/* utilities functions of fat file system */
static void          clearfatbitmap(dfs_t *, u_short);
static u_short       readfatbitmap(dfs_t *, u_short);
static int           readRootCabinet(dfs_t *);
static void          travFileTree(dfs_t *, dfile_t *, int, FALLOCINFO *, int);
static int           sanitychk(dfs_t *, u_short);
static void          verifyFile(dfs_t *, dfile_t *, FALLOCINFO *, int);
static int           scanbitmap(dfs_t *);
static void          concludeCheck(dfs_t *, int);
static int           chainLostCluster(dfs_t *, u_short, int);
static int           createEntry(dfs_t *, u_short, int, int);
static int           loopClustersChain(dfs_t *, int, CLUSTERLIST **, int);
static CLUSTERLIST * fatClusterCell(dfs_t * dfs, int);
static FALLOCINFO  * readSubdirCabinet(dfs_t *, dfile_t *, dfile_t **,
                                            int *, FALLOCINFO *, int);
static void          reportEntryError(FALLOCINFO *, dfile_t *, int);
static void          fatfpckexit(dfs_t *);

extern	int	debug_flag;

#ifdef DEBUG
#include "fatchk.debug"
#endif /* DEBUG */


/*-------- BeginScanLoop -------------------------------------

    the main routine for DOS FAT file system checking.
    this function traverse the entire file system and
    diagnose the structure of each found file or
    subdirectory.

  ------------------------------------------------------------*/

void
BeginScanLoop(dfs_t * dfs, int mode)
{
    /* traverse file system for individual file diagnostic */
    travFileTree(dfs, dfs->dfs_root, dfs->dfs_rootsize, 
                             (FALLOCINFO *)NULL, mode);

    if (debug_flag)
	fpMessage("fatchk.c:BeginScanLoop: travFileTree Completed.");

#ifdef DEBUG
    travTreeprint(dfs->fblkheadp);
    pscanbitmap(dfs);
#else /* DEBUG */
    concludeCheck(dfs, mode);

    if (debug_flag)
	fpMessage("fatchk.c:BeginScanLoop: concludeCheck Completed.");
#endif /* DEBUG */
}


/*-------- concludeCheck -------------------------------------

    this function analyze both collected corss lined
    cluster list and FAT bitmap (the being used clusters
    map, used to compare against FAT) to make final
    correction, if the used specified.

  ------------------------------------------------------------*/

static void
concludeCheck(dfs_t * dfs, int needcorrect)
{
    CLUSTERLIST * nclusterp;
    FALLOCINFO  * curfilep;
    u_short cluster;
    int     i, j, entrychanged = 0;
    int     error;
    int     lostclusters;
    int     lostchain = 0;
    int     fclusters;     /* the number of clusters which
                              has been chained into a file */

    /* handle the files globally cross linked */
    while (dfs->crosslinklist) {
        nclusterp = dfs->crosslinklist->nextcluster;

        /* search the target cluster against the file cluster
           information list to find the files which own this cluster */
        curfilep = dfs->fblkheadp;
        while (curfilep = searchCrossLinked(curfilep, dfs->crosslinklist)) {
            fpWarn("file %s is cross linked on allocation unit %d\n",
                                           getUnixPath(curfilep, NULL),
                                         dfs->crosslinklist->clusterno);

            curfilep = curfilep->nextfile;
        }
        free((char *) dfs->crosslinklist);
        dfs->crosslinklist = nclusterp;
    }

    if (lostclusters = scanbitmap(dfs)) {

        for (i = 0; i < dfs->fatbitmapsz; i++) {
            for (j = 0; j < 8; j++) {
                if ((dfs->fatbitmapp)[i] & (0x01 << j)) {
                    cluster = i*BITMAPUNITSZ+j;
                    fclusters = chainLostCluster(dfs, cluster, needcorrect);
                    lostchain++;
                    if (needcorrect &&
                        createEntry(dfs, cluster, lostchain, fclusters))
                            entrychanged++;

                }     
            }

        }
        if (entrychanged)
            dfs->dirtyrootdir = TRUE;

        fpWarn("there are(is) %d lost cluster found in %d data chain(s)",
                                                  lostclusters, lostchain);
    }

    if (needcorrect) {
        if (dfs->dirtyrootdir) {
            lseek(dfs->dfs_fd, dfs->dfs_rootaddr, 0);
            if ((error = rfpblockwrite(dfs->dfs_fd, (u_char *) dfs->dfs_root,
                                      dfs->dfs_rootsize)) != E_NONE) {
		if (error == E_PROTECTION)
		    fpError("Medium is write-protected\n");
		else
		    fpError("can't write file system information, the floppy is not safe to use!");
                exit(1);
            }
        }

        if (dfs->dirtyfat) {
            lseek(dfs->dfs_fd, dfs->dfs_fataddr, 0);
            for (i = 0; i < dfs->dfs_bp.bp_fatcount; i++) {
                if ((error = rfpblockwrite(dfs->dfs_fd, (u_char *) dfs->dfs_fat, 
                                           dfs->dfs_fatsize)) != E_NONE) {
		    if (error == E_PROTECTION)
			fpError("Medium is write-protected\n");
		    else
			fpError("can't write file system information, the floppy is not safe to use!");
                    exit(1);
                }
                lseek(dfs->dfs_fd,
                dfs->dfs_fataddr + dfs->dfs_fatsize * (i + 1), 0);
            }
        }
    }

    fatfpckexit(dfs);
}


/*-------- createEntry -------------------------

      create the entries in the root
      directory for each found data chain.

  ------------------------------------------------*/

static int
createEntry(dfs_t * dfs, u_short startcluster, int chainth, int chainsz)
{
    time_t    mtime;
    dfile_t   dfe;
    dfile_t * dfp;
    int       i, size;

    bzero(&dfe, sizeof(dfile_t));
    sprintf(dfe.df_name, "FILE%.4d", chainth);
    strcpy(dfe.df_ext, "CHK");
    dfe.df_attribute = ATTRIB_ARCHIVE;
    mtime = time(NULL);
    to_dostime(dfe.df_date, dfe.df_time, &mtime);
    dfe.df_start[0] = startcluster & 0xff;
    dfe.df_start[1] = startcluster >> 8;
    size = dfs->dfs_clustersize*chainsz; 
    dfe.df_size[0]  = size & 0xff;
    dfe.df_size[1]  = (size >> 8) & 0xff;
    dfe.df_size[2]  = (size >> 16) & 0xff;
    dfe.df_size[3]  = (size >> 24) & 0xff;

    for (i = dfs->dfs_rootsize / DIR_ENTRY_SIZE, dfp = dfs->dfs_root;
                                    i > 0; i--, dfp++) {
        if (dfp->df_name[0] == LAST_ENTRY ||
            dfp->df_name[0] == REUSABLE_ENTRY) {

            * dfp = dfe;

            return(E_NONE);
        }
    }
    return(E_SPACE);
}



static int
scanbitmap(dfs_t * dfs)
{
    int i, j, count =0;

    for (i = 0; i < dfs->fatbitmapsz; i++) 
        for (j = 0; j < 8; j++) 
            if ((dfs->fatbitmapp)[i] & (0x01 << j)) 
                count++;

    return count;
}

static void
fatfpckexit(dfs_t * dfs)
{
    FALLOCINFO  * curp;
    CLUSTERLIST * curclusterp;

    if (dfs->dfs_fat)
        free (dfs->dfs_fat);

    if (dfs->dfs_root)
        free (dfs->dfs_root);

    if (dfs->fatbitmapp)
        free(dfs->fatbitmapp);

    releaseFalloc(&(dfs->fblkheadp));

    while (dfs->crosslinklist) {
        curclusterp = dfs->crosslinklist;
        dfs->crosslinklist = (dfs->crosslinklist)->nextcluster;
        free((char *) curclusterp);
    }
}



static u_short
readfatbitmap(dfs_t * dfs, u_short cluster)
{
    int offset, index;

    if (!(dfs->fatbitmapp) || cluster > dfs->fatbitmapsz*BITMAPUNITSZ) {
        fpError("The program encount a bizarre situation");
        exit(1);
    }
    index  = cluster / BITMAPUNITSZ;
    offset = cluster % BITMAPUNITSZ;
    return((dfs->fatbitmapp)[index] & (0x01 << offset));
}


void
setfatbitmap(dfs_t * dfs, u_short cluster)
{
    int offset, index;
    BITMAPUNIT tmp;

    if (!(dfs->fatbitmapp) && cluster > dfs->fatbitmapsz*BITMAPUNITSZ) {
        fpError("The program encount a bizarre situation");
        exit(1);
    }
    index  = cluster / BITMAPUNITSZ;
    offset = cluster % BITMAPUNITSZ;
    /* (dfs->fatbitmapp)[index] |= (0x01 << offset); */
    tmp = (0x01 <<  offset);
    (dfs->fatbitmapp)[index] |= tmp;
}


static void
clearfatbitmap(dfs_t * dfs, u_short cluster)
{
    int offset, index;

    if (!(dfs->fatbitmapp) && cluster > dfs->fatbitmapsz*BITMAPUNITSZ) {
        fpError("The program encount a bizarre situation");
        exit(1);
    }
    index  = cluster / BITMAPUNITSZ;
    offset = cluster % BITMAPUNITSZ;
    (dfs->fatbitmapp)[index] &= ~(0x1 << offset);
}

/*-------- readRootCabinet -------------------

    read the root directory  from floppy

  --------------------------------------------*/

static int
readRootCabinet(dfs_t * dfs)
{
    int retval = E_NONE;

    dfs->dfs_root = (dfile_t *) safemalloc(dfs->dfs_rootsize);
    lseek(dfs->dfs_fd, dfs->dfs_rootaddr, 0);
    return(rfpblockread(dfs->dfs_fd, (u_char *) dfs->dfs_root,
                                            dfs->dfs_rootsize));
}


/*-------- travFileTree -----------------------------------------

    a recursive routine to traverse the entire  file
    system, and perform the individual file stucture diagnostic.

  ---------------------------------------------------------------*/

static void
travFileTree(dfs_t * dfs, dfile_t * findex, int dirsz, 
                      FALLOCINFO * parentdirp, int mode)
{
    char       * tmpname, * dirpath;
    int          dirpathlen;
    int          i, retsz = 0;
    int          count, filenum;
    dfile_t    * sdf = 0;
    dfile_t    * df = findex;
    FALLOCINFO * suballocinfop;

    filenum = (dirsz+DIR_ENTRY_SIZE-1) / DIR_ENTRY_SIZE;
    for (i = 0; i < filenum; i++, df++) {
        if (df->df_name[0] == LAST_ENTRY) 
            break;      /* end of directory */

        if (df->df_name[0] == REUSABLE_ENTRY) 
            continue;   /* reusable entry */

        if (df->df_attribute & ATTRIB_LABEL)
            continue;   /* volume label */

        if (df->df_attribute & ATTRIB_DIR) {
            if (df->df_name[0] != '.')  {
#ifdef DEBUG1
        showfile(dfs, df, parentdirp);
#endif /* DEBUG */
                suballocinfop = readSubdirCabinet(dfs, df, &sdf,
                                         &retsz, parentdirp, mode);
                if (suballocinfop) {
                    travFileTree(dfs, sdf, retsz, suballocinfop, mode);
            if (sdf)
                        free(sdf);
                }
            } 
        } else 
#ifndef DEBUG1
            verifyFile(dfs, df, parentdirp, mode);
#else /*DEBUG */
        showfile(dfs, df, parentdirp);
#endif
    }
}



static int
sanitychk(dfs_t * dfs, u_short cluster)
{
    int  retval = E_NONE;

    /* cluster 0 and 1 are not legal */
    if (cluster == 1 || cluster == 0)
        retval = E_UNKNOWN;

    else if (LAST_CLUSTER(dfs, cluster)) 
        retval = E_USERSTOP;

    /* need to consider the bad sector condition */

    return(retval);
}


/*-------- readSubdirCabinet ----------------------------------

    In DOS, subdirectory structure is same with file.
    read in the subdirectory clusters according to FAT table.

  --------------------------------------------------------------*/

static FALLOCINFO * 
readSubdirCabinet(dfs_t * dfs, dfile_t * df, dfile_t ** fpp, 
                 int * cabinetsz, FALLOCINFO * subdirinfop, int needcorrect)
{
    int          retval = E_NONE;
    u_char     * bp;
    char       * fullpath = NULL;
    dfile_t    * findex;
    u_short      cluster;
    int          subdircnt = 0;
    int          index = 0;
    CLUSTERLIST * subdirheadp = NULL;
    CLUSTERLIST * subdircurp;
    FALLOCINFO  * curfbinfop = NULL;

    /* set 0 before the file cabinet being initialized */
    * cabinetsz = 0;
    cluster  = getstartcluster(df);

    retval = loopClustersChain(dfs, cluster, &subdirheadp, needcorrect);

    /* the subdirectory is empty */
    if (subdirheadp == (CLUSTERLIST *) NULL) {
        reportEntryError(subdirinfop, df, ATTRIB_ARCHIVE);
        return(curfbinfop);
    } 

    /* create the file/subdirectory allocation
       info block, and link it to the list    */
    curfbinfop = fAllocInfoCell(getdosname(df), &subdirheadp,
                                countClusterCell(subdirheadp),
                                &(dfs->fblkheadp), subdirinfop,
                                &dfs->crosslinklist);

    /* allocate the space to store the subdir file entries */
    findex = (dfile_t *)
    safemalloc(dfs->dfs_clustersize*curfbinfop->blknum);
    * fpp = findex;  /* return as the first file entry */
    bp = (u_char *) findex;
    while (index < curfbinfop->blknum) {
        lseek(dfs->dfs_fd, DISK_ADDR(dfs, curfbinfop->blkv[index]), 0);
        fpblockread(dfs->dfs_fd, bp, dfs->dfs_clustersize);
        bp += dfs->dfs_clustersize;
        * cabinetsz += dfs->dfs_clustersize;
        index++;
    }

    if (retval == E_UNKNOWN)
        fpWarn("subdirectory %s has invalid allocation unit", 
                         fullpath = getUnixPath(curfbinfop, NULL));
    else if (retval == E_SELFLINK)    /* self linked */
        fpWarn("subdirectory %s is delf linked", 
               fullpath = getUnixPath(curfbinfop, NULL));

    if (fullpath)
        free(fullpath);

    return(curfbinfop) ;
}


/*-------- verifyFile ----------------------------------

    check the file integrity by scanning the data
    chain consecutive described in the FAT table.

  --------------------------------------------------------------*/

static void
verifyFile(dfs_t * dfs, dfile_t * df, FALLOCINFO * subinfop, int needcorrect)
{
    char       * fullpath;
    char       * filename;
    int          retval = E_NONE;
    FALLOCINFO * curfbinfop;
    FALLOCINFO * croslnkfinfop = NULL; 
    int          cluster;
    int          fdocsz;
    CLUSTERLIST * curfclusterp;
    CLUSTERLIST * fheaderp = NULL;

    fdocsz  = (getfilesz(df)+dfs->dfs_clustersize-1) / dfs->dfs_clustersize;

    cluster = getstartcluster(df);

    retval  = loopClustersChain(dfs, cluster, &fheaderp, needcorrect);

    /* the file is empty */
    if (fheaderp == (CLUSTERLIST *) NULL) {
        reportEntryError(subinfop, df, ATTRIB_ARCHIVE);
        return;
    }

    /* create the file/subdirectory allocation
       info block, and link it to the list    */
    curfbinfop = fAllocInfoCell(getdosname(df), &fheaderp,
                                countClusterCell(fheaderp),
                                &(dfs->fblkheadp), subinfop,
                                &dfs->crosslinklist);

    fullpath = getUnixPath(curfbinfop, NULL);

    /* try to bring up all the possible errors */
    if (retval == E_UNKNOWN) {

        /* if this file contain the invalid allocation unit */
        if (fdocsz < curfbinfop->blknum)
            fpWarn("file %s has invalid cluster unit, file expanded",
                                                           fullpath);
        else if (fdocsz > curfbinfop->blknum)
            fpWarn("file %s has invalid cluster unit, file truncated",
                                                             fullpath);
        else
            fpWarn("file %s has invalid cluster unit");

    } else if (retval == E_SELFLINK) {

        fpWarn("file %s data chain is self linkd", fullpath);

    } else {

        if (fdocsz < curfbinfop->blknum) 
            fpWarn("file %s has been expaned", fullpath);

        else if (fdocsz > curfbinfop->blknum)
            fpWarn("file %s has been truncated.\n", fullpath);
    }

    free(fullpath);
    return;
}



/*-------- chainLostCluster -------------------------------------

   chain the lost cluters of the FAT table
   into the individual file

  ---------------------------------------------------------------*/

static int
chainLostCluster(dfs_t * dfs, u_short firstcluster, int needcorrect)
{
    int   retval = E_NONE;
    int   count = 0;  /* default is one */
    int   cluster, oldcluster = 0;

    cluster =  firstcluster;

    /* only one cluster in this chain */
    if (LAST_CLUSTER(dfs, cluster)) {
        clearfatbitmap(dfs, cluster);
        return ++count;
    }

    while (1) {

        /* do the cluster chain sanity checking */
        if ((retval = sanitychk(dfs, cluster)) != E_NONE) 
        break;

    count++;
        clearfatbitmap(dfs, cluster);
        oldcluster = cluster;  /* save the previous cluster pointer */
        cluster = (*dfs->dfs_readfat)(dfs, cluster);
    }

    if (retval != E_USERSTOP && needcorrect) {
        if (oldcluster != 0) { 
            (*dfs->dfs_writefat)(dfs, cluster, dfs->dfs_endclustermark);
            dfs->dirtyfat = TRUE;
    }
    } else
    count++;

    return count;
}



CLUSTERLIST *
fatClusterCell(dfs_t * dfs, int cluster)
{
    CLUSTERLIST * curcellp;

    curcellp = safemalloc(sizeof(CLUSTERLIST));
    if (readfatbitmap(dfs, cluster)) {

        clearfatbitmap(dfs, cluster);
        curcellp->status = E_NONE;
    } else
        curcellp->status = E_CROSSLINK;

    curcellp->clusterno = cluster;
    curcellp->nextcluster = NULL;

    return(curcellp);
}


/*-------- loopClustersChain ----------------------------------------

   the function will take the start cluster number from file
   entry, and step through the cluster chain which described
   int the FAT table. 
   return cases: E_USERSTOP, which means no error is found.
         E_SELFLINK, the file is self linked.
         E_UNKNOW, the invalis allocation unit is encounted.

  -------------------------------------------------------------------*/

static int
loopClustersChain(dfs_t * dfs, int startc, CLUSTERLIST ** fheaderpp,
                             int needcorrect)
{
    char * fullpath;    
    int retval = E_NONE;
    int cluster = startc;
    CLUSTERLIST * curfclusterp;

    while (1) {

        /* do the cluster chain sanity checking */
        if ((retval = sanitychk(dfs, cluster)) != E_NONE) 
            break;

        else {
            curfclusterp = fatClusterCell(dfs, cluster);
            if (curfclusterp->status == E_CROSSLINK)
                /* detect the condition of the data
                   was self linked and became a loop */
                if (selfLinked(* fheaderpp, cluster) != E_NONE) {
                    free((char *) curfclusterp);
                    retval = E_SELFLINK;
                    break;
                }
            linkClusterCells (fheaderpp, curfclusterp);
            cluster = (*dfs->dfs_readfat)(dfs, cluster);
        } 
    }

    /* if it's not a smooth stop, and correction flag is on */
    if (retval != E_USERSTOP && needcorrect) {

        /* the last cell of the list points to this cluster */
        curfclusterp = lastClusterCell(* fheaderpp);

        /* it's a empty list, file entry must
           contain invalid allocation unit    */
        if (curfclusterp) {
            (* dfs->dfs_writefat) (dfs, curfclusterp->clusterno,
                                        dfs->dfs_endclustermark);
            dfs->dirtyfat = TRUE;
        }
    } 
    return(retval);
}


static void
reportEntryError(FALLOCINFO * subinfop, dfile_t * df, int attrib)
{
    char * filename;
    char * pathname;

    /* entry is wrong, report the error */
    filename = getdosname(df);
    pathname = getUnixPath(subinfop, filename);

    fpWarn("%s %s has invalid allocation unit in entry",
              (attrib == ATTRIB_DIR) ? "subdirectory" : "file",
                             pathname);

    free(filename);
    free(pathname);
    return;
}
