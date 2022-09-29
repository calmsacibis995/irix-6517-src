/* 
   (C) Copyright Digital Instrumentation Technology, Inc. 1990 - 1993  
       All Rights Reserved
*/

/*
    TransferPro macExtents.c  -  Functions for managing the Extents Tree.
*/

#include <malloc.h>
#include "macSG.h"
#include "dm.h"

static char *Module = "macExtents";
/*--------------------- macReadExtentsTree ------------------------------
    Reads the Extents Tree.
---------------------------------------------------------------------*/

int macReadExtentsTree (volume)
    struct m_volume *volume;
   {
    int retval = E_NONE;
    char *funcname = "macExtents.macReadExtentsTree";
    char *buffer = (char *)0;
    int i = 0;
    int offset = 0;
    struct extent_descriptor tempExtent;
    struct BtreeNode tempNode;

    /* Allocate buffer for the extents tree. */

    buffer = malloc (volume->vib->drXTFlSize);
    if (buffer == (char *)0)
       {
        retval = set_error(E_MEMORY, Module, funcname, "");
       }
     
    if ( retval == E_NONE )
       {
        /* read the header node, so we can get the bitmap and do
           selective reading */
        tempExtent.ed_start = volume->vib->drCTExtRec[0].ed_start;
        tempExtent.ed_length = 1;
        if ( (retval = macReadExtent(volume,&tempExtent,buffer)) == E_NONE &&
            (retval = macUnPackHeaderNode(&tempNode.BtNodes.BtHeader,buffer))
            == E_NONE )
           {
            for (i = 0; i < sizeof (struct bitmap_record); i++)
               {
                volume->extBitmap.bitmap[i] = 0;
               }
           }
       }

    i = 0;
    while (retval == E_NONE && volume->vib->drXTExtRec[i].ed_length)
       {
        retval = macReadExtent(volume, &(volume->vib->drXTExtRec[i]), 
                               buffer+offset);
        offset += volume->vib->drXTExtRec[i].ed_length * 
                                                volume->vib->drAlBlkSiz;
        i++;
       }

    /* Build a b*-tree from the buffer. */

    if (retval == E_NONE)
       {
        retval = macBuildTree(volume, EXTENTS_TREE, buffer);
       }

    if( buffer )
       {
        free ( buffer );
       }

    return (retval);
}

             
/*-------------------------- macWriteExtentsTree -------------------------
  writes extents tree file to disk
------------------------------------------------------------------------*/

int macWriteExtentsTree( volume ) 
    struct m_volume *volume;
   {
   int        retval = E_NONE,
              i = 0,
              offset = 0;
    char     *buffer = 0;
    char     *funcname = "macWriteExtentsTree";
    
    if( (buffer = malloc(volume->vib->drXTFlSize)) == (char *)0 )
       {
        retval = set_error(E_MEMORY, Module, funcname, "");
       }
    else
       {
        /* zero out extents tree buffer */
        for( i = 0; i < volume->vib->drXTFlSize; i++ )
           {
            *(buffer+i) = 0;
           }
        /* pack extents tree into buffer */
        retval = macPackTree (volume->extents, EXTENTS_TREE, 
                              volume->vib, buffer);
       }


    /* write extents tracked in vib */
    for( i=0,offset=0;
        retval == E_NONE && volume->vib->drXTExtRec[i].ed_length && i < 3;
        offset += volume->vib->drXTExtRec[i].ed_length * 
                  volume->vib->drAlBlkSiz, i++ )
       {
        retval = macWriteTreeExtent(volume, &(volume->vib->drXTExtRec[i]), 
                                buffer, offset, EXTENTS_TREE);
       }
    /* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
    /* Note: no provision is made for Extents tree extents */
    /*       if this should be implemented, macBuildTree   */
    /*       had better know about it                      */
    /* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */

    /* reset the extents bitmap */
    if ( retval == E_NONE )
       {
        for ( i=0; i < sizeof (struct bitmap_record); i++)
           {
            volume->extBitmap.bitmap[i] = 0;
           }
       }
    if ( buffer )
       {
        free( buffer );
       }

    return(retval);
   }

/*-------------------------- buildXRecKey ------------------------------
   builds a record key for target extent 
------------------------------------------------------------------------*/
void buildXRecKey(struct record_key *RecKeyPtr,
		  unsigned int TREEID,
		  unsigned char fkType,
		  unsigned short block)
{

    RecKeyPtr->key.xkr.xkrKeyLen =  EXTENT_KEYLENGTH;
    RecKeyPtr->key.xkr.xkrFkType =  fkType;
    RecKeyPtr->key.xkr.xkrFNum   =  TREEID;
    RecKeyPtr->key.xkr.xkrFABN   =  block;

}

/*-------------------------- scanXPtrRecs ------------------------------
   searches extents index node for pointer record associated with targetKey
------------------------------------------------------------------------*/
int scanXPtrRecs(targetKey, currIndex, childNodeNum, targetRec)
    struct record_key   *targetKey;
    struct index_node   *currIndex;
    unsigned int        *childNodeNum;
    int                 *targetRec;
   {
    int    retval = E_NONE,
           done = 0,
           i = 0;
    char     *funcname = "scanXPtrRecs";
    while( retval == E_NONE && done == 0 && (unsigned short)i < currIndex->inDesc.ndRecs ) 
       {
        /* Whoa! beyond records with this ckrParId */
        if( targetKey->key.xkr.xkrFNum < 
                 currIndex->inRecList[i]->ptrRecKey.key.xkr.xkrFNum )
           {
            retval = set_error(E_NOTFOUND, Module, funcname, 
                      "Cannot find extent record");
           }

        /* advance through record list */
        else if( targetKey->key.xkr.xkrFNum > 
                 currIndex->inRecList[i]->ptrRecKey.key.xkr.xkrFNum )
           {
            i++;
           }

        /* following test evaluated only if xkrFNum's are equal */
        else
           {
            if ( targetKey->key.xkr.xkrFABN < 
                currIndex->inRecList[i]->ptrRecKey.key.xkr.xkrFABN )
               {
                retval = set_error(E_NOTFOUND, Module, funcname, 
                                   "Cannot find extent record");
               }
            else
               {
                i++;
                done = 1;  /* added by H.C. */
               }
           }
       }

    *targetRec = i;
    if( (unsigned short)i >= currIndex->inDesc.ndRecs )
        {
          retval = set_error(E_NOTFOUND, Module, funcname, 
                                   "Cannot find extent record");
         *childNodeNum = UNASSIGNED_NODE;
        }
    else
        {
         *childNodeNum = currIndex->inRecList[i]->ptrPointer;
        }

    return(retval);
   }

/*-------------------------- scanXLeafRecs ------------------------------
   searches extents leaf node for record associated with targetKey entity
------------------------------------------------------------------------*/
int scanXLeafRecs(targetKey, leafNode, targetRec)
    struct record_key   *targetKey;
    struct leaf_node    *leafNode;
    int                 *targetRec;
   {
    int retval = E_NONE,
        done = 0,
        count = 0;
    char     *funcname = "scanXLeafRecs";

    while( !done  && retval == E_NONE && (unsigned short)count < leafNode->lnDesc.ndRecs )
       {
        if( targetKey->key.xkr.xkrFNum < 
            ((struct extent_record *)leafNode->lnRecList[count])->
             xkrRecKey.key.xkr.xkrFNum )
           {
            done = 1;
	    /* added by H.C. */
            retval = set_error( E_NOTFOUND, Module, funcname,
                                "Can't find extent record." );
           }
        else if( targetKey->key.xkr.xkrFNum == 
            ((struct extent_record *)leafNode->lnRecList[count])->
             xkrRecKey.key.xkr.xkrFNum )
           {
            if (targetKey->key.xkr.xkrFABN ==     
               ((struct extent_record *)leafNode->lnRecList[count])->
               xkrRecKey.key.xkr.xkrFABN)
               {
                done = 1;
               }
            else if ( targetKey->key.xkr.xkrFABN < 
                ((struct extent_record *)leafNode->lnRecList[count])->
                xkrRecKey.key.xkr.xkrFABN)
               {
                retval = set_error( E_NOTFOUND, Module, funcname, 
                          "Can't find extent record." );
               }
           }
        if( retval == E_NONE && !done )
           {
            count++;
           }
       }

    if( count ==  leafNode->lnDesc.ndRecs )
       {
        retval = set_error(E_NOTFOUND, Module, funcname, 
                        "Can't find extent record.");
       }
    *targetRec = count;

    return(retval);
   }            



/*------------------------- macFindExtRec ----------------------------------
    Finds extent tree record for to current offset file descriptor.
 ----------------------------------------------------------------------------*/

int macFindExtRec (fd, fndNodeNum, fndRecNum)
    struct m_fd *fd;
    unsigned int  *fndNodeNum,
           *fndRecNum;
   {
    int retval = E_NONE;
    unsigned int  xRecNum = 0;
    unsigned int  xNodeNum = 0;
    struct record_key recKey;
    char *funcname="macFindExtRec";

    buildXRecKey( &recKey, (unsigned int)fd->fdFileRec->filFlNum, 
                           (unsigned char)fd->fdFork, 
       (unsigned short)(fd->fdOffset/fd->fdVolume->vib->drAlBlkSiz) );

/*
    if ( (retval = searchTree(fd->fdVolume, &recKey, (unsigned int)EXTENTS_TREE,
                   (int *)&xRecNum,&xNodeNum)) == E_NOTFOUND )
       {
        retval = clear_error();
       }
 */  /* modified by H.C. */

    retval = searchTree(fd->fdVolume, &recKey, (unsigned int)EXTENTS_TREE,                   (int *)&xRecNum,&xNodeNum);

    /* if not a direct hit or if out of current file's extent records, */
    /* move back one record */

    if ( retval != E_NONE )
       {
        ;
       }
    else if( xNodeNum <= 0 )
       {
        retval = set_error( E_NOTFOUND, Module, funcname, "" );
       }
    else if( xRecNum >=
       fd->fdVolume->extents[xNodeNum]->BtNodes.BtLeaf.lnDesc.ndRecs )
       {
        if ( xRecNum > 0 &&
            fd->fdOffset/fd->fdVolume->vib->drAlBlkSiz >=
            ((struct extent_record *)fd->fdVolume->extents[xNodeNum]->BtNodes.
            BtLeaf.lnRecList[xRecNum-1])->xkrRecKey.key.xkr.xkrFABN )
           {
            xRecNum--;
           }
        else
           {
            retval = set_error( E_NOTFOUND, Module, funcname, "" );
           }
       }
    else if (
        ((struct extent_record *)fd->fdVolume->extents[xNodeNum]->BtNodes.
        BtLeaf.lnRecList[xRecNum])->xkrRecKey.key.xkr.xkrFNum == 
        fd->fdFileRec->filFlNum &&
        fd->fdOffset/fd->fdVolume->vib->drAlBlkSiz < 
        ((struct extent_record *)fd->fdVolume->extents[xNodeNum]->BtNodes.
        BtLeaf.lnRecList[xRecNum])->xkrRecKey.key.xkr.xkrFABN )
       {
        if( xRecNum == 0 )
           {
            if( fd->fdVolume->extents[xNodeNum]->BtNodes.BtLeaf.lnDesc.ndBLink 
                == UNASSIGNED_NODE )
               {
                retval = set_error(E_NOTFOUND,Module,funcname,
                                    "Cannot find extent record");
               }
            else
               {
                xNodeNum = 
                 fd->fdVolume->extents[xNodeNum]->BtNodes.BtLeaf.lnDesc.ndBLink;
                xRecNum = 
                 (fd->fdVolume->extents[xNodeNum]->BtNodes.BtLeaf.lnDesc.ndRecs) - 1;
               }
           }
        else
           {
            xRecNum--;
           }
       }
    *fndNodeNum = xNodeNum;
    *fndRecNum = xRecNum;
      
    return (retval);
   }

/*-- macAddExtentsList ------
 *
 * Add new extent to extents tree for a file
 *
 */

int macAddExtentsList( fd )
    struct m_fd *fd;
   {
    int retval = E_NONE;
    struct extent_record tempXRec;
    int  xNodeNum, xRecNum;
    int addARec = 0;
    int a;

    /* if there are no records in extents tree, prep the tree */
    if ( fd->fdVolume->extents[0]->BtNodes.BtHeader.hnHdrRec.hrTotalRecs == 0 &&
        (retval = macPlantTree(fd->fdVolume, EXTENTS_TREE)) != E_NONE )
       {
        ;
       }  

    else
       {
        /* zero out tempXRec */
        for ( a = 0; a < sizeof(struct extent_record); a++ )
           {
            *((char *)(&tempXRec)+a) = '\0';
           }

        /* find locus for first extent record for this file */
        buildXRecKey((struct record_key *)&tempXRec, 
       	             (unsigned int)fd->fdFileRec->filFlNum,
	             (unsigned char)fd->fdFork,
	             (unsigned short)((fd->fdExtentRec)?
            fd->fdOffset/fd->fdVolume->vib->drAlBlkSiz : 0));

        if( (retval = searchTree(fd->fdVolume,(struct record_key *)(&tempXRec), 
            (unsigned int)EXTENTS_TREE, &xRecNum, (unsigned int *)&xNodeNum)) 
            == E_NOTFOUND )
           {
            retval = clear_error();
           }

        /* if no records exist for this file, add this record to tree  */
        if ( retval == E_NONE &&
            (unsigned short)xRecNum >= fd->fdVolume->extents[xNodeNum]->BtNodes.BtLeaf.lnDesc.ndRecs ) 
           {
            addARec = 1;
           }
            
        else
           {
            fd->fdExtentRec = fd->fdVolume->extents[xNodeNum]->BtNodes.BtLeaf.
                lnRecList[xRecNum];

            if ( fd->fdExtentRec->xkrExtents[2].ed_start > 0 )
               {
                /* set xRecNum so new rec will be inserted at right place */
                xRecNum++;
                addARec = 1;
               }
           }

        if ( addARec )
           {
            /* correct extent_record and add to tree */
            tempXRec.xkrRecKey.key.xkr.xkrFABN = 
                (short)((fd->fdOffset-fd->fdExtOffset)/
                 fd->fdVolume->vib->drAlBlkSiz);

            if ( (retval = macAddRecToTree(fd->fdVolume, &xNodeNum,
                 (void *)&tempXRec, &xRecNum,EXTENTS_TREE)) == E_NONE )
               {
                fd->fdExtentRec = fd->fdVolume->extents[xNodeNum]->BtNodes.
                    BtLeaf.lnRecList[xRecNum];
                fd->fdExtents = fd->fdExtentRec->xkrExtents;
                fd->fdExtentNode = xNodeNum;
               }
           }
       }

    return( retval );
   }
