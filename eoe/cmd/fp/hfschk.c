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

#ident "$Revision: 1.11 $"

#include <stdio.h>
#include <bstring.h>
#include <string.h>
#include "dm.h"
#include "fpck.h"

static char *Module = "fhsck";

#define FPCKDEBUG
#undef  FPCKDEBUG 

#define NOT_PARTITION_BLOCK 1
#define HFS_PARTITION       2
#define NOT_HFS_PARTITION   3

#define PTNSIG              0x504d
#define PTNSIG0             (PTNSIG>>8)
#define PTNSIG1             (PTNSIG&0xff)
#define MDBSIGNATURE        0x4244

#define MAXPATH             256

/* the struct carry all the allocated blocks informations
    within the disk volume, for the files data chains diagnostic */
typedef struct fallocinfov_t {
    FALLOCINFO   * fallocheadp;
    FALLOCINFO   * xtreefbinfop;
    FALLOCINFO   * ctreefbinfop;
} FALLOCINFOV;

static int sparebadflag;     /* set it, if there is bad block sparing */
static struct m_VBM * lvbm;   /* duplicate blocks bitmap */
static CLUSTERLIST  * crosslinkp;  /* cross link blocks list */

static void initHfsVolume(void *, struct m_volume *, int, int);
static int  getHfsVolume(int, char *, int *, int *, int *);
static int  hfsVolumeChk(void *, int, int, int);
static int  verifyVib(struct m_volume *);
static int  verifyVbm(struct m_volume *);
static int  verifyXBtree(struct m_volume *, FALLOCINFO **);
static int  verifyCBtree(struct m_volume *, FALLOCINFO **);
static void verifyFile(struct m_volume *, struct file_record *, int,
                                    char *, CLUSTERLIST **, char *);
static int  loopHFS(struct m_volume *, FALLOCINFO **);
static int  scanDir(struct m_volume *, char *, struct file_list *,
                                      FALLOCINFO *, FALLOCINFO **);
static int  queryBitmaps(struct m_VIB *, int, int, CLUSTERLIST **);
static void crossLinkSearch(CLUSTERLIST **, FALLOCINFOV *);
static void hfsExit(struct m_volume *, FALLOCINFOV *);
static int  extentSanityCheck(struct m_volume *, struct extent_descriptor *,
                                                                 int *, int);

#ifdef FPCKDEBUG
#include "hfschk.debug"
#endif /* FPCKDEBUG */


/*-------- hfsCk -------------------------------------

    the main routine which start with hfs volume
    searching in the partitioned file system or
    assume it's not partitioned floppy, if there
    is no partition signature.

  ----------------------------------------------------*/

int
hfsCk(void * aDevice, int needcorrect)
{
    int retval = E_NOTSUPPORTED;
    char bkbuf[SCSI_BLOCK_SIZE];
    int startbk, bksize;
    int partition = 0;  /* search from the partition 0 */
    int n_partitions = 0;
    
    /* in case that the floppy was partitioned in multiple volumes */
    do {
        /* assume the floppy is partitioned and do the hfs volume searching */
        retval = macReadBlockDevice(aDevice, bkbuf, PTNBLK+partition,
                                                           PTNBLKCNT);
        if (retval != E_NONE) {
            /* a bad sector, try next partition */
            partition++;
            continue;
        }

        retval = getHfsVolume(partition, bkbuf, &startbk, &bksize, &n_partitions);
        if (retval == NOT_PARTITION_BLOCK) {

            /* it's non partitioned hfs file system */
            if (partition == 0) {
                startbk = 0;
                /* number count from 0 */
                bksize = macLastLogicalBlockDevice(aDevice)+1;
                hfsVolumeChk(aDevice, startbk, bksize, needcorrect);
            }
            /* exit, if it's not partition map sector */
            break;
        } else if (retval == HFS_PARTITION)
            hfsVolumeChk(aDevice, startbk, bksize, needcorrect);

        partition++;
    } while (partition < n_partitions || !n_partitions);

    /* floppy is intact */
    if (retval == E_NOTSUPPORTED)
        fpError("can't find HFS volume"); 

	return 0;
}


/*-------- initHfsVolume ----------------------------------------

    initialize the volume data structure for further usage

  ------------------------------------------------------------*/

static void
initHfsVolume(void * volumeDevice, struct m_volume * macVolume, 
                      int start, int size)
{
    macVolume->device = volumeDevice;
    macVolume->vib = (struct m_VIB *)0;
    macVolume->vbm = (struct m_VBM *)0;
    macVolume->catalog = (struct BtreeNode **)0;
    macVolume->extents = (struct BtreeNode **)0;
    macVolume->CWDid = 0;
    macVolume->maxNodes = 0;
    strcpy (macVolume->curDir, ":");
    macVolume->vstartblk = start;
    macVolume->vblksize  = size;
}


/*-------- getHfsVolume --------------------------------------------------

    search mac hfs volume around the disk partitions.
    input : partition number
    output : the start block number and the size of blocks for the volume

  ------------------------------------------------------------------------*/

static int
getHfsVolume(int partition, char * bbufp, int * startbkp, int * sizebkp,
	     int *n_parts)
{

    if (bbufp[0] == PTNSIG0 && bbufp[1] == PTNSIG1) {
        struct m_PartitionMap mapRec;

        macUnpackPartitionMap(&mapRec, bbufp);
        if (mapRec.pmSig != PTNSIG)
            return(NOT_PARTITION_BLOCK);
	if (*n_parts == 0)
	    *n_parts = mapRec.pmMapBlkCnt;

#ifdef DEBUGVOLUME
printf("pmMapBlkCnt=%d pmPyPartStart=%d pmPartBlkCnt=%d pmPartType=%s\n",
                                mapRec.pmMapBlkCnt, mapRec.pmPyPartStart, 
                                 mapRec.pmPartBlkCnt, mapRec.pmPartType);
printf("pmLgDataStart=%d pmDataCnt=%d pmPartStatus=%d\n",
         mapRec.pmLgDataStart, mapRec.pmDataCnt, mapRec.pmPartStatus);
#endif /* DEBUGVOLUME */

        if (strncmp(mapRec.pmPartType, PT_PARTTYPEHFS, 
                       strlen(mapRec.pmPartType)) == 0) {

            * startbkp = mapRec.pmPyPartStart;
            * sizebkp  = mapRec.pmPartBlkCnt;

            return(HFS_PARTITION);
        } else
            return(NOT_HFS_PARTITION);

    } else if (bbufp[0] == OLDPTNSIG0 && bbufp[1] == OLDPTNSIG1) {
        struct m_OldPartitionMap omapRec;

        /* macUnpackOldPartitionMap only unpack the first 
           set of partition descriptor, it's a bug, fix it later */      
        macUnpackOldPartitionMap(&omapRec, bbufp);

        if (omapRec.pdPartition[0].pdStart == 0 &&
            omapRec.pdPartition[0].pdSize  == 0 &&
            omapRec.pdPartition[0].pdFSID  == 0)

            return(NOT_HFS_PARTITION);

        if (!strcmp(omapRec.pdPartition[0].pdFSID, OLD_PARTTYPEHFS)) {
            * startbkp = omapRec.pdPartition[0].pdStart;
            * sizebkp  = omapRec.pdPartition[0].pdSize;

            return(HFS_PARTITION);
        } else
            return(NOT_PARTITION_BLOCK);

    } else
        return(NOT_PARTITION_BLOCK);
}


/*-------- hfsVolumeChk -------------------------------------

    The procedure to perform hfs file system checking in the
    input HFS volume (the volume start block number and 
    the size of blocks is specified in the argument).

  ------------------------------------------------------------*/

static int
hfsVolumeChk(void * aDevice, int startbk, int bksize, int needcorrect)
{
    int dirty = 0;
    int notOwnedSectors = 0;
    int vbmsz, i;
    int retval = E_NONE;
    struct m_volume hfsVolume;
    struct m_volume *macVolume = &hfsVolume;
    FALLOCINFOV fallocinfov;

    bzero(&fallocinfov, sizeof(FALLOCINFOV));
    /* initialize the global variables */
    initHfsVolume(aDevice, &hfsVolume, startbk, bksize);
    fallocinfov.fallocheadp  = (FALLOCINFO *)NULL;
    fallocinfov.xtreefbinfop = (FALLOCINFO *)NULL;
    fallocinfov.ctreefbinfop = (FALLOCINFO *)NULL;
    lvbm = NULL;  
    crosslinkp = NULL;
    sparebadflag = 0;

    /* read and check the validate of the Master directory block */
    if ((retval = verifyVib(macVolume)) != E_NONE) {
        if (retval == E_READ) {
            fpError("can't read Master directory block, the floppy is not safe to use!");
            goto leave;
        } else if (retval == E_NOOBJECT) {  /* no signature */
            fpError("the disk is not HFS format");
            goto leave;
        }
    }

    /* Verify the volume attribute */
    if (!(macVolume->vib->drAtrb & Atrb_UnMounted)) {
        fpWarn("the HFS volume was not properly dismounted");
        if (needcorrect) {
            macVolume->vib->drAtrb |= Atrb_UnMounted;
            dirty = 1;
        }
    }
    if (macVolume->vib->drAtrb & Atrb_HW_Locked) {
        fpWarn("the HFS volume is locked by hardware");
        if (needcorrect) {
            macVolume->vib->drAtrb &= ~Atrb_HW_Locked;
            dirty = 1;
        }
    }
    if (macVolume->vib->drAtrb & Atrb_SW_Locked) {
        fpWarn("the HFS volume is locked by software");
        if (needcorrect) {
            macVolume->vib->drAtrb &= ~Atrb_SW_Locked;
            dirty = 1;
        }
    }
    if (macVolume->vib->drAtrb & Atrb_SparedBad)
        sparebadflag = 1;

    /* read and duplicate the block allocation bitmap table */
    if (verifyVbm(macVolume) != E_NONE) {
        fpError("can't read the volume bitmap, the floppy is not safe to use!");
        goto leave;
    } else {  /* duplicate the bitmap */
        vbmsz = (macVolume->vib->drAlBlSt - macVolume->vib->drVBMSt)*NODE_SIZE;
        lvbm = (struct m_VBM *) safecalloc(vbmsz);
        memcpy(lvbm, macVolume->vbm, vbmsz);
    }

    macVolume->maxNodes = macVolume->vib->drNmAlblks/4;

    if ((retval = verifyXBtree(macVolume, &fallocinfov.xtreefbinfop))
                                                           != E_NONE) {
        if (retval == E_MEMORY) 
            fpError("out of memory");
        else if (retval == E_READ || retval == E_OPEN)
            fpError("Extended Btree: bad sector");
        else if (retval == E_UNRECOVER)
            fpError("Extented Btree: tree extent starting point out of range");
        else if (retval == E_RANGE)
            fpError("Extended Btree: tree extent out of allocatable boundary");
        else if (retval == E_MEDIUM)
            fpError("Extended Btree: contains unknown node type");
        else if (retval == E_BTREEBMPNODE)
            fpError("Extended Btree: header node was corrupted");
        else if (retval == E_BTREENODESZ)
            fpError("Extended Btree: header node was corrupted");

        goto leave;
    }
#ifdef FPCKDEBUG
    printfalloc(fallocinfov.xtreefbinfop);
    pntcount=1;
#endif /* FPCKDEBUG */
          
    if ((retval = verifyCBtree(macVolume, &fallocinfov.ctreefbinfop))
                                                          != E_NONE) {
        if (retval == E_MEMORY)
            fpError("out of memory");
        else if (retval == E_READ || retval == E_OPEN)
            fpError("Catalog Btree: bad sector");
        else if (retval == E_UNRECOVER)
            fpError("Catalog Btree: tree extent starting point out of range");
        else if (retval == E_RANGE)
            fpError("Catalog Btree: tree extent out of allocatable boundary");
        else if (retval == E_MEDIUM)
            fpError("Catalog Btree: contains unknown node type");
        else if (retval == E_NOTFOUND)
            fpError("Catalog Btree: header node is not found");
        else if (retval == E_BTREEBMPNODE)
            fpError("Catalog Btree: header node was corrupted");
        else if (retval == E_BTREENODESZ)
            fpError("Catalog Btree: header node was corrupted");

        goto leave;
    }

#ifdef FPCKDEBUG
    printfalloc(fallocinfov.ctreefbinfop);
#endif /* FPCKDEBUG */
    
    /* update the Master Directory block, if it's dirty  and need correct */
    if (needcorrect && dirty && macWriteVIB(macVolume) != E_NONE) {
            fpError("can't write the Master directory block, floppy is not safe to use!");
            goto leave;
    }

    /* traverse the file system and do individual file diagnostic */
    loopHFS(macVolume, &fallocinfov.fallocheadp);

    if (crosslinkp)
        crossLinkSearch(&crosslinkp, &fallocinfov);

    for (i = 0; i < macVolume->vib->drNmAlblks; i++) {
        if (macGetBit((char *)lvbm, i)) {
            if (needcorrect)
                macClearBit((char *) (macVolume->vbm), i);

            notOwnedSectors++;
        }
    }
    /* the volume don't have its bad block spared */ 
    if (notOwnedSectors && !sparebadflag) {
        if (needcorrect)
            macWriteBitmap(macVolume);

        fpError("found %d lost track sectors", notOwnedSectors);
    }
leave:
    hfsExit(macVolume, &fallocinfov);

    return (retval);
}


/*-------- hfsExit -----------------------------------

   release all the allocated resource before leaving

  ----------------------------------------------------*/

static void
hfsExit(struct m_volume * macvolume, FALLOCINFOV * fallocinfovp)
{
    int i;
    struct BtreeNode **tree;

    if (lvbm)
        free((char *)lvbm);
    if (macvolume->vib)
        free((char *)macvolume->vib);
    if (macvolume->vbm)
        free((char *)macvolume->vbm);
   
    if (macvolume->extents){
        tree = macvolume->extents;
        for (i = 0; i < macvolume->maxNodes; i++)
            if (tree[i]) 
                free((char *)tree[i]);

        free((char *) tree);
    }

    if (macvolume->catalog){
        tree = macvolume->catalog;
        for (i = 0; i < macvolume->maxNodes; i++)
            if (tree[i]) 
                free((char *)tree[i]);

        free((char *) tree);
    }

    if (fallocinfovp->fallocheadp)
        releaseFalloc(&(fallocinfovp->fallocheadp));
    if (fallocinfovp->xtreefbinfop)
        releaseFalloc(&(fallocinfovp->xtreefbinfop));
    if (fallocinfovp->ctreefbinfop)
        releaseFalloc(&(fallocinfovp->ctreefbinfop));
}


/*-------- crossLinkSearch ---------------------------------

    search the cross link list against the file allocation
    block list to match the cross lined cases.

  ----------------------------------------------------------*/

static void
crossLinkSearch(CLUSTERLIST ** crosslinkpp, FALLOCINFOV * fallocinfovp)
{
    char        * filename = 0;
    FALLOCINFO  * curfilep;
    CLUSTERLIST * curclusterp ;
    CLUSTERLIST * tmpclusterp ;

    if (* crosslinkpp == (CLUSTERLIST *)NULL)
        return;
    else
        curclusterp = * crosslinkpp;

    /* looking for the cross linked cases 
       and prompt the message to the users */
    while (curclusterp) {
        tmpclusterp = curclusterp->nextcluster;
   
        curfilep = fallocinfovp->ctreefbinfop;
        if (curfilep = searchCrossLinked(curfilep, curclusterp))
            fpWarn("the Catalog tree corss linked on %d", 
                                  curclusterp->clusterno);

        curfilep = fallocinfovp->xtreefbinfop;
        if (curfilep = searchCrossLinked(curfilep, curclusterp))
            fpWarn("the Extend tree corss linked on %d", 
                                 curclusterp->clusterno);

        curfilep = fallocinfovp->fallocheadp;
        while (curfilep = searchCrossLinked(curfilep, curclusterp)) {

            filename = getUnixPath(curfilep, NULL);
            fpWarn("the file %s cross linked on %d", curclusterp->clusterno);
            curfilep = curfilep->nextfile;
        }

        if (filename)
            free((char *) filename);

        free((char *) curclusterp);
        curclusterp = tmpclusterp;
    }
    * crosslinkpp = (CLUSTERLIST *) NULL; 
}


/*-------- verifyVib -------------------------------------

        verify vib (volume information block)
     Reads, unpacks and vib, then verify the signature
     and mount status of the floppy disk.

  --------------------------------------------------------*/

static int
verifyVib(struct m_volume * macvolume)
{
    int retval = E_NONE;
  
    /* Read the on-disk vib into a buffer. */
    macvolume->vib = (struct m_VIB *)safemalloc((unsigned)NODE_SIZE);

    if ((retval = macReadVIB(macvolume)) != E_NONE)
        return(retval);

    /* Verify the signature */
    if (macvolume->vib->drSigWord != MDBSIGNATURE)
        return(E_NOOBJECT); 

    return(retval);
}


/*-------- verifyVbm -----------------------------

        verify vbm  (volume bitmap block)  
        Read VBM into memory from disk.

  ------------------------------------------------*/

static int
verifyVbm(struct m_volume * macvolume)
{
    int retval = E_NONE;
    int vbmblk = macvolume->vib->drAlBlSt - macvolume->vib->drVBMSt;
    int vbmsize = BBTOB(vbmblk);

     /* Read the on-disk vmb into a buffer. */
     macvolume->vbm = (struct m_VBM *)safemalloc(vbmsize);
     retval = macReadBitmap(macvolume, vbmblk);

     return(retval);
}


/*-------- verifyXBtree -----------------------------

         verify XBtree (Extent BTree)
      Reads, unpacks the extent btree node
      into a in-core extent node table.

  ------------------------------------------------*/

static int
verifyXBtree(struct m_volume * macvolume, FALLOCINFO ** xtreefbinfopp)
{
    int i = 0, offset = 0;
    int retval = E_NONE;
    char * buffer = NULL;
    CLUSTERLIST * xtreeheadp = NULL;
    CLUSTERLIST * xtreecurp;

    macvolume->extents = (struct BtreeNode **) safecalloc((unsigned)
                 ((macvolume->maxNodes)*sizeof(struct BtreeNode *)));

    buffer = (char *) safemalloc (macvolume->vib->drXTFlSize);
    bzero((void *)&(macvolume->extBitmap.bitmap[0]),
                      sizeof(struct bitmap_record));

    while (macvolume->vib->drXTExtRec[i].ed_length) {

        if ((retval = extentSanityCheck(macvolume, 
                      &(macvolume->vib->drXTExtRec[i]), NULL, 0)) != E_NONE)
            goto lxbtree;

        /* E_RANGE, E_READ, E_OPEN */
        if ((retval = macReadExtent(macvolume,
                      &(macvolume->vib->drXTExtRec[i]),
                                        buffer+offset)) != E_NONE)
             
            goto lxbtree;

        offset += macvolume->vib->drXTExtRec[i].ed_length *
                                  macvolume->vib->drAlBlkSiz;

        /* E_RANGE */
        if ((retval = queryBitmaps(macvolume->vib,
                          macvolume->vib->drXTExtRec[i].ed_start,
                          macvolume->vib->drXTExtRec[i].ed_length,
                                          &xtreeheadp)) != E_NONE)

            goto lxbtree;

#ifdef FPCKDEBUG
    printf("\n");
#endif
        i++;
    }

    /* Build a b*-tree from the buffer. */
    /* E_MEMORY, E_RANGE(bad tree header), E_MEDIUM(bad node) */
    retval = macBuildTree(macvolume, EXTENTS_TREE, buffer);

    if (retval == E_NONE)
        fAllocInfoCell((char *)NULL, &xtreeheadp, countClusterCell(xtreeheadp),
                               xtreefbinfopp, (FALLOCINFO *)NULL, &crosslinkp);

lxbtree:
    if (buffer) 
        free (buffer);

    return (retval);
}


/*-------- verifyCBtree ---------------------------

         verify CBtree (Catalog Tree)
      Reads, unpacks the Catalog btree node
      into a in-core extent node table.

  -------------------------------------------------*/

static int
verifyCBtree(struct m_volume * macvolume, FALLOCINFO ** ctreefbinfopp)
{
    char    *funcname = "verifyCBtree";
    int i, offset = 0;
    int retval = E_NONE;
    char * buffer = NULL;
    unsigned int      xNodeNum;
    int               xRecNum;
    struct extent_record *Xextent = (struct extent_record *)0;
    struct record_key recKey;
    struct extent_descriptor tempExtent;
    struct BtreeNode tempNode;
    CLUSTERLIST * ctreeheadp = NULL;
    CLUSTERLIST * ctreecurp;

    macvolume->catalog = (struct BtreeNode **) safecalloc((unsigned)
                 ((macvolume->maxNodes)*sizeof(struct BtreeNode *)));

    buffer = (char *) safecalloc (macvolume->vib->drCTFlSize);

    /* read the header node, so we can get
       the bitmap and do selective reading */
    tempExtent.ed_start = macvolume->vib->drCTExtRec[0].ed_start;
    tempExtent.ed_length = 1;
    if ((retval = macReadExtent(macvolume, &tempExtent, buffer)) == E_NONE &&
        (retval = macUnPackHeaderNode(&tempNode.BtNodes.BtHeader,buffer))
                                                                == E_NONE) 
        bzero((void *)&(macvolume->catBitmap.bitmap[0]), 
                           sizeof(struct bitmap_record));

    if (tempNode.BtNodes.BtHeader.hnDesc.ndFLink != UNASSIGNED_NODE) {
        retval = set_error(E_RANGE, Module, funcname, "Map node found");
        goto lcbtree;
    }

    i = 0;
    while (retval == E_NONE && macvolume->vib->drCTExtRec[i].ed_length &&
                                                                  i < 3) {

        if ((retval = extentSanityCheck(macvolume,
                          &(macvolume->vib->drCTExtRec[i]), NULL, 0)) != E_NONE)
            goto lcbtree;

        /* E_RANGE, E_READ, E_OPEN */
        if ((retval = macReadExtent(macvolume,
                                    &(macvolume->vib->drCTExtRec[i]),
                                                      buffer+offset)) != E_NONE)
            goto lcbtree;

        offset += macvolume->vib->drCTExtRec[i].ed_length *
                                  macvolume->vib->drAlBlkSiz;

        if ((retval = queryBitmaps(macvolume->vib,
                              macvolume->vib->drCTExtRec[i].ed_start,
                              macvolume->vib->drCTExtRec[i].ed_length,
                                              &ctreeheadp)) != E_NONE)
            goto lcbtree;

#ifdef FPCKDEBUG
    printf("\n");
#endif

        i++;
    }

    /* search extents tree for Catalog file records */
    while (retval == E_NONE && offset < macvolume->vib->drCTFlSize) { 

        buildXRecKey (&recKey, CATALOG_FILE_ID, DATA_FORK,
            (unsigned short)(offset/macvolume->vib->drAlBlkSiz));

        if ((retval = searchTree(macvolume, &recKey, EXTENTS_TREE, 
                                 &xRecNum, &xNodeNum)) == E_NONE) {

            Xextent = (struct extent_record *) 
            macvolume->extents[xNodeNum]->BtNodes.BtLeaf.lnRecList[xRecNum];
        }
        
        i = 0;
        while (retval == E_NONE && Xextent->xkrExtents[i].ed_length && i < 3) {

            retval = macReadExtent(macvolume, &Xextent->xkrExtents[i], 
                                                        buffer+offset);
            if (retval != E_NONE)
                return(retval);

            offset += (Xextent->xkrExtents[i].ed_length * 
                                            macvolume->vib->drAlBlkSiz);

            retval = queryBitmaps(macvolume->vib,
                                  Xextent->xkrExtents[i].ed_start,
                                  Xextent->xkrExtents[i].ed_length,
                                                       &ctreeheadp);
            if (retval != E_NONE)
                return(retval);
#ifdef FPCKDEBUG
    printf("\n");
#endif

            i++;
        }
    }

    /* Build a b*-tree from the buffer. */
    /* E_MEMORY, E_RANGE(bad tree header), E_MEDIUM(bad node) */
    retval = macBuildTree(macvolume, CATALOG_TREE, buffer);

    if (retval == E_NONE)
        fAllocInfoCell((char *)NULL, &ctreeheadp, countClusterCell(ctreeheadp),
                               ctreefbinfopp, (FALLOCINFO *)NULL, &crosslinkp);

lcbtree:
    if (buffer) 
        free (buffer);

    return (retval);
}


/*-------- loopHFS ---------------------------

     Loop the HFS file sytem tree

  -------------------------------------------------*/

static int
loopHFS(struct m_volume * macvolume, FALLOCINFO ** fallocheadpp)
{
    int retval = E_NONE;

/*
    if ((retval = scanDir(macvolume, ":", NULL, (FALLOCINFO *)NULL, 
                      fallocheadpp)) != E_NONE)
        fpWarn("");
 */

    scanDir(macvolume, ":", NULL, (FALLOCINFO *)NULL, fallocheadpp);

    return(retval);
}


/*-------- scanDir ---------------------------

     scan the designate directory

  -------------------------------------------------*/

static int
scanDir(struct m_volume * macvolume, char * dirp, struct file_list * dirRec,
                    FALLOCINFO * parentpath, FALLOCINFO ** fallocheadpp)
{
    int    CWDid;
    int    retval = E_NONE;
    char   pathName[MAXPATH],
           recName[MAXMACNAME+2],
         * funcname = "scanDir",
         * fname,
         * pname;
    struct file_list * macRec = (struct file_list *)0;
    struct file_list * currMacFileList = (struct file_list *)0;
    FALLOCINFO  * curfbinfop = NULL;
    CLUSTERLIST * fdataheadp = NULL;
    CLUSTERLIST * fresoheadp = NULL;

    if (dirRec)  /* not root */
        CWDid = dirRec->entry.flDirRec.dirDirID;

    retval = macAbsScandir(macvolume, dirp, &CWDid, YES,  &currMacFileList);

    /* no subdir or file information was found */
    if (currMacFileList == (struct file_list *)0)
        return(retval); 

    for (macRec = currMacFileList; macRec && retval == E_NONE;
                                       macRec = macRec->flNext) {

        strncpy(recName,
                (char *)macRec->entry.flFilRec.filRecKey.key.ckr.ckrCName,
                (int)macRec->entry.flFilRec.filRecKey.key.ckr.ckrNameLen);
        recName[macRec->entry.flFilRec.filRecKey.key.ckr.ckrNameLen] = '\0';

        /* should check the pathname length */
        if (dirRec)   /* not root dir */
            sprintf(pathName, "%s:%s", dirp, recName);
        else          /* root dir */
            sprintf(pathName, "%s%s", dirp, recName);

        if (macRec->entry.flDirRec.dirType == DIRECTORY_REC) {
 
            curfbinfop = (FALLOCINFO *) safemalloc(sizeof(FALLOCINFO));   
            bzero(curfbinfop, sizeof(FALLOCINFO));
            curfbinfop->name = safecalloc(strlen(recName)+1);
            strcpy(curfbinfop->name, recName);
            curfbinfop->parentdir = parentpath;
            if (* fallocheadpp == NULL)
                * fallocheadpp = curfbinfop;
            else {
                curfbinfop->nextfile = * fallocheadpp;
                * fallocheadpp = curfbinfop;
            }
            retval = scanDir(macvolume, pathName, macRec, curfbinfop,
                            fallocheadpp);
            if (retval != E_NONE)
                fpWarn("the subdirectory %s is in danger", pathName);

        } else {
#ifdef FPCKDEBUG
            printf("\n(M)File %s sectors in the list :\n", pathName);
        pntcount = 1;
#endif /* FPCKDEBUG */
            /* fileType : 0=data fork, 1=resource fork, 2=file record */
            fdataheadp = NULL;
            fresoheadp = NULL;
            verifyFile(macvolume, &macRec->entry.flFilRec, DO_DATA_FORK,
                                           recName, &fdataheadp, pathName);
            verifyFile(macvolume, &macRec->entry.flFilRec, DO_RESOURCE_FORK,
                                           recName, &fresoheadp, pathName);
            linkClusterCells(&fdataheadp, fresoheadp);
            fname = safecalloc(strlen(recName)+1);
            strcpy(fname, recName);
            curfbinfop = fAllocInfoCell(fname, &fdataheadp,
                                      countClusterCell(fdataheadp),
                                      fallocheadpp, parentpath, &crosslinkp);
#ifdef FPCKDEBUG
            printf("\n");    

            pname = getUnixPath(curfbinfop, NULL);
            printf("(U)File %s sectors in the list :\n", pname);
            printfalloc(curfbinfop);
            free(pname);
#endif /* FPCKDEBUG */
        }
    }
    return(retval);
}


/*-------- verifyFile ---------------------------

     verify the file structure.

  -------------------------------------------------*/

static void
verifyFile(struct m_volume * macvolume, struct file_record * fileRec,
                                       int fileType, char * filename,
                   CLUSTERLIST ** clusterheadpp, char * fullpath)
{
    int fd = -1;
    int i = 0; 
    int retval = E_NONE;
    int filesz, lfilesz, pfilesz; 
    unsigned int byteRead = 0;
    unsigned int byteToCopy = 0;
    char *funcname = "verifyFile";
    struct m_fd * openFd = (struct m_fd *) 0;
    struct extent_descriptor *filerecExtents;
    unsigned int xRecNum = UNASSIGNED_REC;
    unsigned int xNodeNum = UNASSIGNED_NODE;
    unsigned int curBlk, tmpBlk;
    CLUSTERLIST * filecurp;
    FALLOCINFO  * filecurfbinfop = NULL;

    /* looking for the data fork of the file */
    retval = macOpenFile(macvolume, fileRec, DT_RDONLY, fileType, &fd);

    if (fd == -1 && retval != E_NONE) {
        fpWarn("can't open file %s", filename); 
        return;
    } else if (fd == -1 && retval == E_NONE) {
        fpError("out of resources"); 
        return;
    }

    /* logical end of file of data fork */
    openFd = macGetFd(fd);
    lfilesz  = (fileType == DO_DATA_FORK) ? 
                fileRec->filLgLen : fileRec->filRLgLen; 

    pfilesz  = (fileType == DO_DATA_FORK) ? 
                fileRec->filPyLen : fileRec->filRPyLen; 

    /* use physical size which is sector size boundary) to check
       against the real size that was reading from extend descriptors */
    filesz = pfilesz;

    filerecExtents = (fileType == DO_DATA_FORK) ?
          fileRec->filExtRec : fileRec->filRExtRec;

    /* calculate the file size based on  
       what the extend descriptors described */
    for (i = 0; i < 3 && filerecExtents[i].ed_length; i++) {
        byteToCopy = macvolume->vib->drAlBlkSiz*
                     filerecExtents[i].ed_length;

        byteRead         += byteToCopy;
        openFd->fdOffset += byteToCopy;

        retval = queryBitmaps(macvolume->vib, filerecExtents[i].ed_start,
                              filerecExtents[i].ed_length, clusterheadpp);
    }

    /* search the file extend records for the specified file */
    while (     /* byteRead < filesz &&  */
       (retval = macFindExtRec(openFd, &xNodeNum, &xRecNum)) == E_NONE) {

        curBlk = (unsigned int) 
                 (openFd->fdOffset/openFd->fdVolume->vib->drAlBlkSiz);
        openFd->fdExtentRec = (struct extent_record *)
                               openFd->fdVolume->extents[xNodeNum]->
                               BtNodes.BtLeaf.lnRecList[xRecNum];
        tmpBlk = ((struct extent_record *)
        openFd->fdVolume->extents[xNodeNum]->
        BtNodes.BtLeaf.lnRecList[xRecNum])->xkrRecKey.key.xkr.xkrFABN;

        for (i = 0; i < 3 && byteRead < filesz; i++) {
            if (curBlk >= tmpBlk+
                openFd->fdExtentRec->xkrExtents[i].ed_length) {

                tmpBlk += openFd->fdExtentRec->xkrExtents[i].ed_length;
            } else {
                byteToCopy = macvolume->vib->drAlBlkSiz*
                           openFd->fdExtentRec->xkrExtents[i].ed_length;

                byteRead         += byteToCopy;
                openFd->fdOffset += byteToCopy;

                retval = queryBitmaps(macvolume->vib,
                             openFd->fdExtentRec->xkrExtents[i].ed_start,
                             openFd->fdExtentRec->xkrExtents[i].ed_length,
                                                           clusterheadpp);
 
            }
        }
    }

    if (byteRead < filesz)
        fpWarn("file %s %s is truncated\n", fullpath,
                (fileType == DO_DATA_FORK) ? "Data Fork":"Resource Fork");
    else if (byteRead > filesz)
        fpWarn("file %s %s is expanded\n", fullpath,
                (fileType == DO_DATA_FORK) ? "Data Fork":"Resource Fork");
#ifdef FPCKDEBUG1
    else
        printf("File %s %s is OK,size=%d (%dk)\n", fullpath,
                (fileType == DO_DATA_FORK) ? "Data Fork":"Resource Fork",
                         filesz, (filesz+1023)/1024); 
#endif /* FPCKDEBUG1 */

    macCloseFile(fd);
}


/*-------- queryBitmaps -----------------------------------------

     the function do the sanity checking of the cluster number,
     also validate the cluster status against the volume bitmap.
     Meanwhile setup the cluster information block and link to
     the cluster list.

  ----------------------------------------------------------------*/

static int
queryBitmaps(struct m_VIB * vibptr, int startblk, int blkcnt,
                                 CLUSTERLIST ** clusterheadpp)
{
    int    allocblks = vibptr->drNmAlblks;
    char * funcname  = "queryBitmaps";
    int    retval    = E_NONE;
    int    count, blkindex;
    CLUSTERLIST * curcellp;

    for (count=0, blkindex=startblk; count < blkcnt; count++, blkindex++) {

        if (blkindex >= allocblks)
            return(E_RANGE);  

        curcellp = safemalloc(sizeof(CLUSTERLIST));
        if (macGetBit((char *)lvbm, blkindex)) {
            macClearBit((char *)lvbm, blkindex);
            curcellp->status = E_NONE;
        } else 
            curcellp->status = E_CROSSLINK;

        curcellp->clusterno = blkindex;
        curcellp->nextcluster = NULL;
        linkClusterCells(clusterheadpp, curcellp);
#ifdef FPCKDEBUG
        printf("%4d ", blkindex);
            if (!(pntcount++ % 15))
                printf("\n");    
#endif /* FPCKDEBUG */
    }
    return(E_NONE);
}


/*-------- extentSanityCheck -------------------

    do the sanity checks for the input extents.

  ----------------------------------------------*/

static int
extentSanityCheck(struct m_volume * macvolume, struct extent_descriptor * ed,
                                                  int * dirtyp, int correctit)
{
    int blockFactor = macvolume->vib->drAlBlkSiz/SCSI_BLOCK_SIZE;

    /* if the starting block is out of allocation ranges,
        then it's in an unrecoverable condition */
    if (ed->ed_start >= macvolume->vib->drNmAlblks*blockFactor)
        return(E_UNRECOVER);

    if ((ed->ed_start+(ed->ed_length*blockFactor)) >= 
	    macvolume->vib->drNmAlblks*blockFactor) {
        if (correctit) {
            ed->ed_length = ((macvolume->vib->drNmAlblks-1)*
				    blockFactor-ed->ed_start) / blockFactor;
            * dirtyp = 1;
        }
        return(E_RANGE); 
    }
    return(E_NONE);
}
