/* 
   (C) Copyright Digital Instrumentation Technology, Inc., 1990 - 1993 
       All Rights Reserved
*/

/* DIT TransferPro macPack.c  */
/* functions to populate structures from buffers read from disk */
/* and to populate buffers to be written to disk from structures */


#include <malloc.h>
#include "macSG.h" 
#include "dm.h" 

static char *Module = "macPack";

static void pack(void *, char **, int);
static void unpack(void *, char **, int);

/* -------------------------------------------------------------------------*/
/* Node UnPack functions: unpack contents of a node buffer into node pointer */
/* -------------------------------------------------------------------------*/

/*--- macUnPackHeaderNode ---------------------
 *
 */

int macUnPackHeaderNode( nptr, nodeBuffer )
    struct header_node  *nptr;
    char                *nodeBuffer;
   {
    int        retval = E_NONE,
               recNum = 0;
    short      recOffset = 0;
    char *funcname = "macUnPackHeaderNode";

    macUnpackNodeDescriptor(&nptr->hnDesc, nodeBuffer);

    if ( (recOffset = getRecordOffset(recNum++,(short *)nodeBuffer)) < 0 ||
        recOffset > NODE_SIZE )
       {
        retval = set_error( E_RANGE, Module, funcname, 
                           "Bad data in header node" );
       }
    else 
       {
         macUnpackHeaderRec (&nptr->hnHdrRec, nodeBuffer + recOffset);

        if ( (recOffset = getRecordOffset(recNum++,(short *)nodeBuffer)) < 0 ||
                recOffset > NODE_SIZE )
           {
            retval = set_error( E_RANGE, Module, funcname, 
                               "Bad data in header node" );
           }
        else 
            /* get reserved record (for now we assume it is always the same
               length, and do not attempt to analyze its structure */
           {
            memcpy( nptr->hnResRec, nodeBuffer+recOffset,
               TREE_RESREC_SIZE );
           }
       }
    if ( retval == E_NONE )
       {
        /* get offset for bitmap record */
        if ( (recOffset = getRecordOffset(recNum++,(short *)nodeBuffer)) < 0 ||
            recOffset > NODE_SIZE )
           {
            retval = set_error( E_RANGE, Module, funcname, 
                           "Bad data in header node" );
           }
        else
           {
            /* get bitmap record contents */
            memcpy((char *)&nptr->hnBitMap, nodeBuffer + recOffset, 
               sizeof(struct bitmap_record));
           }
       }

    return(retval);
   }

/*--- macUnPackIndexNode ---------------------
 *
 */
int macUnPackIndexNode( treeType, nptr, nodeBuffer )
    int treeType;
    struct index_node  *nptr;
    char               *nodeBuffer;
   {
    int        retval = E_NONE,
               done = 0,
               recNum = 0,
               keySize;
    short      recOffset = 0;
    char      *recPtr;
    int        i;

    for ( i = 0; i < MAXRECORDS; i++ )
       {
        nptr->inRecList[i] = 0;
       }

    /* get index node descriptor */
    macUnpackNodeDescriptor(&nptr->inDesc, nodeBuffer);

    while( !done &&  retval == E_NONE && (unsigned short)recNum < nptr->inDesc.ndRecs )
       {
        recOffset = getRecordOffset(recNum, (short *)nodeBuffer);

        if( *(nodeBuffer + recOffset) == 0)
           {
            done = 1;
           }
        else
           {
            recPtr = nodeBuffer+recOffset;
            keySize= (*recPtr % 2)? (int)(*recPtr+1) : (int)(*recPtr+2);

            if ( (nptr->inRecList[recNum] = (struct pointer_record *)
                malloc(sizeof (struct pointer_record)) ) == 
                (struct pointer_record *)0)
               {
                retval = set_error (E_MEMORY, Module, "macUnPackIndexNode",
                          "Insufficient memory."); 
               }
            else
               {
                if ( treeType == EXTENTS_TREE )
                   {
                    macUnpackExtRecordKey ((struct xkrKey *)
					    nptr->inRecList[recNum], recPtr);
                   }
                else
                   {
                    macUnpackRecordKey ((struct ckrKey *)
					nptr->inRecList[recNum], recPtr);
                   }
                memcpy((char *) &nptr->inRecList[recNum]->ptrPointer, 
                    recPtr+keySize, 4);
                recNum++;
               }
           }
       }

    return(retval);
   }  

/*--- macUnPackLeafNode ---------------------
 *
 */
int macUnPackLeafNode( treeType, nptr, nodeBuffer )
    int                treeType;
    struct leaf_node  *nptr;
    char              *nodeBuffer;
   {
    int        retval = E_NONE,
               done = 0,
               keySize = 0,
               recNum = 0;
    short      recOffset = 0;
    char      *bufptr = nodeBuffer,
              *recPtr;
    int        i;

    for ( i = 0; i < MAXRECORDS; i++ )
       {
        nptr->lnRecList[i] = 0;
       }

    /* get leaf node descriptor */
    macUnpackNodeDescriptor(&nptr->lnDesc, bufptr);

   /* unpack records from leaf node */
    bufptr += sizeof(struct node_descriptor);

    while( !done &&  retval == E_NONE && (unsigned short)recNum < nptr->lnDesc.ndRecs )
       {
        recOffset = getRecordOffset(recNum, (short *)nodeBuffer);

        /* check if record is null (no more records in node) */
        if( *(nodeBuffer + recOffset) == 0)
           {
            done = 1;
           }

        /* check if this node is extents leaf node */
        else if( treeType == EXTENT_NODE )
           {
            if ( (nptr->lnRecList[recNum] = (struct extent_record *)
                malloc(sizeof (struct extent_record)) ) == 
                (struct extent_record *)0)
               {
                retval = set_error (E_MEMORY, Module, "macUnPackLeafNode",
                          "Insufficient memory."); 
               }
            else
               {
                recPtr = nodeBuffer+recOffset;
                macUnpackExtRecordKey (nptr->lnRecList[recNum], recPtr);
                memcpy((char *) 
                ((struct extent_record *)nptr->lnRecList[recNum])->xkrExtents,
                 recPtr+X_KEY_SIZE, sizeof(struct extent_descriptor)*3);
                recNum++;
               }
           }
        else    /* deal with particular record lengths individually */
           {
            recPtr = nodeBuffer+recOffset;
            keySize= (*recPtr % 2)? (int)(*recPtr+1) : (int)(*recPtr+2);
            switch( *(recPtr+keySize) )
               {
                /* record is directory record: */
                case DIRECTORY_REC:
                    if ( (nptr->lnRecList[recNum] = (struct dir_record *)
                        malloc(sizeof (struct dir_record)) ) == 
                        (struct dir_record *)0)
                       {
                        retval = set_error (E_MEMORY, Module, 
                                 "macUnPackLeafNode", "Insufficient memory."); 
                       }
                    else
                       {
                        macUnpackRecordKey (nptr->lnRecList[recNum],recPtr);
                        macUnpackDirRecord ((struct dir_record *)
                                nptr->lnRecList[recNum], recPtr+keySize);
                       }
                    break;

                /* record is file record: */
                case FILE_REC:
                    if ( (nptr->lnRecList[recNum] = (struct file_record *)
                        malloc(sizeof (struct file_record)) ) == 
                        (struct file_record *)0)
                       {
                        retval = set_error (E_MEMORY, Module, 
                                 "macUnPackLeafNode", "Insufficient memory."); 
                       }
                    else
                       {
                        macUnpackRecordKey (nptr->lnRecList[recNum],recPtr);
                        macUnpackFileRec ((struct file_record *)
                                  nptr->lnRecList[recNum], recPtr+keySize);
                       }
                    break;

                /* record is thread record: */
                case THREAD_REC:
                    if ( (nptr->lnRecList[recNum] = (struct thread_record *)
                        malloc(sizeof (struct thread_record)) ) == 
                        (struct thread_record *)0)
                       {
                        retval = set_error (E_MEMORY, Module, 
                                 "macUnPackLeafNode", "Insufficient memory."); 
                       }
                    else
                       {
                        macUnpackRecordKey (nptr->lnRecList[recNum],recPtr);
                        macUnpackThreadRec ((struct thread_record *)
                                 nptr->lnRecList[recNum], recPtr+keySize);
                       }
                    break;
              
                /* record is alias record: */
                case ALIAS_REC:
                    if ( (nptr->lnRecList[recNum] = (struct alias_record *)
                        malloc(sizeof (struct alias_record)) ) == 
                        (struct alias_record *)0)
                       {
                        retval = set_error (E_MEMORY, Module, 
                                 "macUnPackLeafNode", "Insufficient memory."); 
                       }
                    else
                       {
                        macUnpackRecordKey (nptr->lnRecList[recNum],recPtr);
                        macUnpackAliasRec ((struct alias_record *)
                                 nptr->lnRecList[recNum], recPtr+keySize);
                       }
                    break;
                    
                /* unknown record type -- bail out! */
                default:
                    retval = set_error (E_SYNTAX, Module, 
                             "macUnpackLeafNode", "Unrecognized record type.");
                    break;
               }
            recNum++;
           }
       }

    return(retval);
   }  

/*--- macUnPackMapNode ---------------------
 *
 */
void macUnPackMapNode( nptr, nodeBuffer )
    struct map_node   *nptr;
    char              *nodeBuffer;
   {

    macUnpackNodeDescriptor ((struct node_descriptor *)nptr, nodeBuffer);
    memcpy((char *)&nptr->mnRecList, nodeBuffer + NODE_DESC_SIZE, 
           sizeof(struct bitmap_record));

   }  


/* ----------------------------------------------------------------0*/
/* Node Pack functions: pack a buffer with contents of node pointer */
/* ----------------------------------------------------------------0*/

/*--- macPackTree ---------------------
 *
 */
int macPackTree (tree, treeType, vib, buffer)
    struct BtreeNode **tree;
    int               treeType;
    struct m_VIB     *vib;
    char             *buffer;
   {
    int    retval = E_NONE,
           nodeCount,
           treeSize = ((treeType == CATALOG_TREE) ? 
               vib->drCTFlSize : vib->drXTFlSize)/NODE_SIZE;
    char  *funcname="macPackTree";

    for( nodeCount = 0; 
         retval == E_NONE && nodeCount < treeSize; 
         nodeCount++, buffer += NODE_SIZE )
       {
        if( !tree[nodeCount] )
           {
            ;     /* pass over unused nodes */
           } 
        else
           {
            switch(  tree[nodeCount]->BtNodes.nodeBytes[NODE_TYPE_OFFSET] )
               {
                case HEADER_NODE:
                   macPackHeaderNode(buffer, 
				     (struct header_node *)tree[nodeCount]);
                   break;
                case INDEX_NODE:
                   macPackIndexNode(treeType, buffer, 
				    (struct index_node *)tree[nodeCount]);
                   break;
                case LEAF_NODE:
                   retval = macPackLeafNode(treeType, buffer,
                                         (struct leaf_node *)tree[nodeCount]);
                   break;
                case BITMAP_NODE:
                   macPackMapNode(buffer, (struct map_node *)tree[nodeCount]);
                   break;
                default:
                   retval = set_error(E_RANGE,Module,funcname, 
                     "Unrecognized node type in catalog or extents tree" );
                   break;
               }
           }
       }
         
    return(retval);
   }

/*--- macPackHeaderNode ---------------------
 *
 */
void macPackHeaderNode( nodeBuffer, nptr )
    char                *nodeBuffer;
    struct header_node  *nptr;
   {
    int    recNum = 0,
           count,
           recLen = 0,
           recOffset = 0;

    for( count=0; count < NODE_SIZE; count++)
       {
        *(nodeBuffer+count) = '\0';
       }

    macPackNodeDescriptor(nodeBuffer, &nptr->hnDesc);

    recOffset = 0x0e;
    putRecordOffset((short *)nodeBuffer, recNum++, recOffset);	

    recLen =  macPackHeaderRec (&nptr->hnHdrRec, nodeBuffer+recOffset);

    recOffset += recLen;
    putRecordOffset((short *)nodeBuffer, recNum++, recOffset);

    /* no write necessary for Reserved Record */
    memcpy( nodeBuffer+recOffset, nptr->hnResRec, TREE_RESREC_SIZE );

    recOffset += TREE_RESREC_SIZE;
    putRecordOffset((short *)nodeBuffer, recNum++, recOffset);

    memcpy( nodeBuffer+recOffset, &nptr->hnBitMap,
        sizeof(struct bitmap_record));

    /* write freespace offset */
    recOffset += sizeof(struct bitmap_record);
    putRecordOffset((short *)nodeBuffer, recNum, recOffset);

   }

/*--- macPackIndexNode ---------------------
 *
 */
void macPackIndexNode( treeType, nodeBuffer, nptr )
    int treeType;
    char               *nodeBuffer;
    struct index_node  *nptr;
   {
    int    recNum = 0,
           count,
           recOffset = 0,
           keylen;

    for( count=0; count < NODE_SIZE; count++)
       {
        *(nodeBuffer+count) = '\0';
       }

    macPackNodeDescriptor(nodeBuffer, &nptr->inDesc);

    recOffset = 0x0e;
    putRecordOffset((short *)nodeBuffer, recNum, recOffset);	

    while( (unsigned short)recNum < nptr->inDesc.ndRecs )  
       {
        if ( treeType == EXTENTS_TREE )
           {
            keylen = macPackExtRecordKey(nodeBuffer+recOffset,
                                 (struct xkrKey *)nptr->inRecList[recNum]);
           }
        else
           {
            keylen = macPackRecordKey( nodeBuffer+recOffset,
                (struct ckrKey *)nptr->inRecList[recNum] );
           }
        memcpy( nodeBuffer+recOffset+keylen,
            &nptr->inRecList[recNum]->ptrPointer,
            sizeof(unsigned int) );
        recOffset += keylen + sizeof( unsigned int );
        recNum++;
        putRecordOffset((short *)nodeBuffer, recNum, recOffset);	
       }

    if ( nptr->inDesc.ndRecs == 0 )
       {
        /* write free space offset */
        putRecordOffset( (short *)nodeBuffer, recNum, recOffset );	
       }

   }  

/*--- macPackLeafNode ---------------------
 *
 */
int macPackLeafNode( treeType, nodeBuffer, nptr )
    int                treeType; 
    char              *nodeBuffer;
    struct leaf_node  *nptr;
   {
    int    retval = E_NONE,
           recNum = 0,
           count,
           recOffset = 0,
           keylen,
           datalen;
    char  *funcname="macPackLeafNode";

    for ( count=0; count < NODE_SIZE; count++)
       {
        *(nodeBuffer+count) = '\0';
       }

    macPackNodeDescriptor(nodeBuffer,&nptr->lnDesc);
    recOffset = 0x0e;
    putRecordOffset((short *)nodeBuffer, recNum, recOffset);	

    while( retval == E_NONE && (unsigned short)recNum < nptr->lnDesc.ndRecs )  
       {
        if( treeType == EXTENTS_TREE )
           {
            keylen = macPackExtRecordKey(nodeBuffer+recOffset,
                                   (struct xkrKey *)nptr->lnRecList[recNum]);
            datalen = 3*sizeof(struct extent_descriptor);

            memcpy( nodeBuffer+recOffset+keylen, 
                (char *)(((struct extent_record *)
                nptr->lnRecList[recNum])->xkrExtents), datalen );
           }
        else
           {
            keylen = macPackRecordKey( nodeBuffer+recOffset,
                nptr->lnRecList[recNum] );

            switch ( *((char *)nptr->lnRecList[recNum] +
                sizeof(struct record_key)) )
               {
                case FILE_REC:
                    datalen = macPackFileRec ((struct file_record *)
                      nptr->lnRecList[recNum], nodeBuffer + recOffset + keylen);
                    break;
                case DIRECTORY_REC:
                    datalen = macPackDirRecord ((struct dir_record *)
                      nptr->lnRecList[recNum], nodeBuffer + recOffset + keylen);
                    break;
                case THREAD_REC:
                    datalen = macPackThreadRec ((struct thread_record *)
                      nptr->lnRecList[recNum], nodeBuffer + recOffset + keylen);
                    break;
                case ALIAS_REC:
                    datalen = macPackAliasRec ((struct alias_record *)
                      nptr->lnRecList[recNum], nodeBuffer + recOffset + keylen);
                    break;
                default:
                    retval = set_error( E_RANGE, Module, funcname, 
                        "Unrecognized record in leaf node");
                    break;
               }
           }
        datalen = datalen % 2 ? datalen + 1 : datalen;
        recOffset += keylen + datalen;
        recNum++;
        putRecordOffset((short *)nodeBuffer, recNum, recOffset);	
       }
    if ( nptr->lnDesc.ndRecs == 0 )
       {
        /* write free space offset */
        putRecordOffset((short *)nodeBuffer, recNum, recOffset);	
       }

    return( retval );
   }  

/*--- macPackMapNode ---------------------
 *
 */
void macPackMapNode( nodeBuffer, nptr )
    char              *nodeBuffer;
    struct map_node   *nptr;
   {
    int    count,
           recOffset = 0;

    for ( count=0; count < NODE_SIZE; count++)
       {
        *(nodeBuffer+count) = '\0';
       }

    macPackNodeDescriptor(nodeBuffer,&nptr->mnDesc);
    recOffset = 0x0e;

    putRecordOffset((short *)nodeBuffer, 0, recOffset);	

    memcpy( nodeBuffer+recOffset, &nptr->mnRecList,
        sizeof(struct bitmap_record) );
    recOffset += sizeof(struct bitmap_record);

    /* write freespace offset */
    putRecordOffset((short *)nodeBuffer, 1, recOffset);	

   }  

/*-- macPackVIB -----------------------------------------*/

void macPackVIB (vib, buffer)
struct m_VIB *vib;
char *buffer;
   {
    
    char *bufptr = buffer;

    pack (&vib->drSigWord, &bufptr, 2); 
    pack (&vib->drCrDate, &bufptr, 4); 
    pack (&vib->drLsMod, &bufptr, 4); 
    pack (&vib->drAtrb, &bufptr, 2); 
    pack (&vib->drNmFls, &bufptr, 2); 
    pack (&vib->drVBMSt, &bufptr, 2); 
    pack (&vib->drAllocPtr, &bufptr, 2); 
    pack (&vib->drNmAlblks, &bufptr, 2); 
    pack (&vib->drAlBlkSiz, &bufptr, 4); 
    pack (&vib->drClpSiz, &bufptr, 4); 
    pack (&vib->drAlBlSt, &bufptr, 2); 
    pack (&vib->drNxtCNID, &bufptr, 4); 
    pack (&vib->drFreeBks, &bufptr, 2); 
    pack (&vib->drVolNameLen, &bufptr, 1); 
    pack (vib->drVolName, &bufptr, MAX_MAC_VOL_NAME); 
    pack (&vib->drVolBkUp, &bufptr, 4); 
    pack (&vib->drVSeqNum, &bufptr, 2); 
    pack (&vib->drWrCnt, &bufptr, 4); 
    pack (&vib->drXTClpSiz, &bufptr, 4); 
    pack (&vib->drCTClpSiz, &bufptr, 4); 
    pack (&vib->drNmRtDirs, &bufptr, 2); 
    pack (&vib->drFilCnt, &bufptr, 4); 
    pack (&vib->drDirCnt, &bufptr, 4); 
    pack (vib->drFndrInfo, &bufptr, MAX_FNDRINFO_SIZE); 
    pack (&vib->drVCSiz, &bufptr, 2); 
    pack (&vib->drVBMSiz, &bufptr, 2); 
    pack (&vib->drCtlCSiz, &bufptr, 2); 
    pack (&vib->drXTFlSize, &bufptr, 4); 
    pack (vib->drXTExtRec, &bufptr, sizeof (struct extent_descriptor) * 3); 
    pack (&vib->drCTFlSize, &bufptr, 4); 
    pack (vib->drCTExtRec, &bufptr, sizeof (struct extent_descriptor) * 3); 
    pack (vib->drUnused, &bufptr, 350); 
   }

/*-- macUnpackVIB -----------------------------------------*/

void macUnpackVIB (vib, buffer)
struct m_VIB *vib;
char *buffer;
   {
    
    char *bufptr = buffer;

    unpack (&vib->drSigWord, &bufptr, 2); 
    unpack (&vib->drCrDate, &bufptr, 4); 
    unpack (&vib->drLsMod, &bufptr, 4); 
    unpack (&vib->drAtrb, &bufptr, 2); 
    unpack (&vib->drNmFls, &bufptr, 2); 
    unpack (&vib->drVBMSt, &bufptr, 2); 
    unpack (&vib->drAllocPtr, &bufptr, 2); 
    unpack (&vib->drNmAlblks, &bufptr, 2); 
    unpack (&vib->drAlBlkSiz, &bufptr, 4); 
    unpack (&vib->drClpSiz, &bufptr, 4); 
    unpack (&vib->drAlBlSt, &bufptr, 2); 
    unpack (&vib->drNxtCNID, &bufptr, 4); 
    unpack (&vib->drFreeBks, &bufptr, 2); 
    unpack (&vib->drVolNameLen, &bufptr, 1); 
    unpack (vib->drVolName, &bufptr, MAX_MAC_VOL_NAME); 
    unpack (&vib->drVolBkUp, &bufptr, 4); 
    unpack (&vib->drVSeqNum, &bufptr, 2); 
    unpack (&vib->drWrCnt, &bufptr, 4); 
    unpack (&vib->drXTClpSiz, &bufptr, 4); 
    unpack (&vib->drCTClpSiz, &bufptr, 4); 
    unpack (&vib->drNmRtDirs, &bufptr, 2); 
    unpack (&vib->drFilCnt, &bufptr, 4); 
    unpack (&vib->drDirCnt, &bufptr, 4); 
    unpack (vib->drFndrInfo, &bufptr, MAX_FNDRINFO_SIZE); 
    unpack (&vib->drVCSiz, &bufptr, 2); 
    unpack (&vib->drVBMSiz, &bufptr, 2); 
    unpack (&vib->drCtlCSiz, &bufptr, 2); 
    unpack (&vib->drXTFlSize, &bufptr, 4); 
    unpack (vib->drXTExtRec, &bufptr, sizeof (struct extent_descriptor) * 3); 
    unpack (&vib->drCTFlSize, &bufptr, 4); 
    unpack (vib->drCTExtRec, &bufptr, sizeof (struct extent_descriptor) * 3); 
   }

/*-- UTILITY FUNCTIONS FOR USE BY MACPACK AND OTHERS ------
 *
 *---------------------------------------------------------*/

/*-- pack ----------------------*/

static void pack ( val, bufptr, size)
void *val;
char **bufptr;
int size;
   {
    memcpy (*bufptr, (char *)val, size);
    *bufptr += size;
   }

/*-- unpack ----------------------*/

static void unpack (val, bufptr, size)
void *val;
char **bufptr;
int size;
   {
    memcpy (val, *bufptr, size);
    *bufptr += size;
   }
 
/*-- macUnpackOldPartitionMap ---
 *
 */
void macUnpackOldPartitionMap( mapRec, buffer )
     struct m_OldPartitionMap *mapRec;
     char *buffer;
    {
     char *bufptr = buffer;

     unpack (&mapRec->pdSig, &bufptr, 2);
     unpack (&mapRec->pdPartition->pdStart, &bufptr, 4);
     unpack (&mapRec->pdPartition->pdSize, &bufptr, 4);
     unpack (mapRec->pdPartition->pdFSID, &bufptr, 4);
     unpack (mapRec->pdUnused, &bufptr, 6);

   }

/*-- macPackOldPartitionMap --
 *
 */
int macPackOldPartitionMap ( mapRec, buffer )
    struct m_OldPartitionMap *mapRec;
    char *buffer;
   {
    char *bufptr = buffer;
    int retval = E_NONE;

    pack (&mapRec->pdSig, &bufptr, 2);
    pack (&mapRec->pdPartition->pdStart, &bufptr, 4);
    pack (&mapRec->pdPartition->pdSize, &bufptr, 4);
    pack (mapRec->pdPartition->pdFSID, &bufptr, 4);
    pack (mapRec->pdUnused, &bufptr, 6);


    retval = bufptr - buffer;
    return( retval );
   }

/*-- macPackDriverMap --
 *
 */
int macPackDriverMap ( mapRec, buffer )
    struct m_DriverMap *mapRec;
    char *buffer;
   {
    char *bufptr = buffer;
    int retval = E_NONE;

    pack (&mapRec->sbSig, &bufptr, 2);
    pack (&mapRec->sbBlockSize, &bufptr, 2);
    pack (&mapRec->sbBlkCount, &bufptr, 4);
    pack (&mapRec->sbDevType, &bufptr, 2);
    pack (&mapRec->sbDevID, &bufptr, 2);
    pack (&mapRec->sbData, &bufptr, 4);
    pack (&mapRec->sbDrvrCount, &bufptr, 2);
    retval = bufptr - buffer;
    return (retval);
   }
   
/*-- macUnpackDriverMap --
 *
 */
void macUnpackDriverMap ( mapRec, buffer )
     struct m_DriverMap *mapRec;
     char *buffer;
    {
     char *bufptr = buffer;

     unpack (&mapRec->sbSig, &bufptr, 2);
     unpack (&mapRec->sbBlockSize, &bufptr, 2);
     unpack (&mapRec->sbBlkCount, &bufptr, 4);
     unpack (&mapRec->sbDevType, &bufptr, 2);
     unpack (&mapRec->sbDevID, &bufptr, 2);
     unpack (&mapRec->sbData, &bufptr, 4);
     unpack (&mapRec->sbDrvrCount, &bufptr, 2);
    }

/*-- macUnpackPartitionMap -- 
 *
 */
void macUnpackPartitionMap ( mapRec, buffer )
     struct m_PartitionMap *mapRec;
     char *buffer;
    {
     char *bufptr = buffer;
    
     unpack (&mapRec->pmSig, &bufptr, 2);
     unpack (&mapRec->pmSigPad, &bufptr, 2);
     unpack (&mapRec->pmMapBlkCnt, &bufptr, 4);
     unpack (&mapRec->pmPyPartStart, &bufptr, 4);
     unpack (&mapRec->pmPartBlkCnt, &bufptr, 4);
     unpack (mapRec->pmPartName, &bufptr, 32);
     unpack (mapRec->pmPartType, &bufptr, 32);
     unpack (&mapRec->pmLgDataStart, &bufptr, 4);
     unpack (&mapRec->pmDataCnt, &bufptr, 4);
     unpack (&mapRec->pmPartStatus, &bufptr, 4);
     unpack (&mapRec->pmLgBootStart, &bufptr, 4);
     unpack (&mapRec->pmBootSize, &bufptr, 4);
     unpack (&mapRec->pmBootLoad, &bufptr, 4);
     unpack (&mapRec->pmBootLoad2, &bufptr, 4);
     unpack (&mapRec->pmBootEntry, &bufptr, 4);
     unpack (&mapRec->pmBootEntry2, &bufptr, 4);
     unpack (&mapRec->pmBootCksum, &bufptr, 4);
     unpack (mapRec->pmProcessor, &bufptr, 16);
     unpack (mapRec->pmBootBytes, &bufptr, 128);
     unpack (mapRec->pmUnused, &bufptr, 248);
    }

/*-- macPackPartitionMap --------
 *
 */

int macPackPartitionMap( mapRec, buffer )
    struct m_PartitionMap *mapRec;
    char *buffer;
   {
    int retval = E_NONE;
    char *bufptr = buffer;

    pack (&mapRec->pmSig, &bufptr, 2);
    pack (&mapRec->pmSigPad, &bufptr, 2);
    pack (&mapRec->pmMapBlkCnt, &bufptr, 4);
    pack (&mapRec->pmPyPartStart, &bufptr, 4);
    pack (&mapRec->pmPartBlkCnt, &bufptr, 4);
    pack (mapRec->pmPartName, &bufptr, 32);
    pack (mapRec->pmPartType, &bufptr, 32);
    pack (&mapRec->pmLgDataStart, &bufptr, 4);
    pack (&mapRec->pmDataCnt, &bufptr, 4);
    pack (&mapRec->pmPartStatus, &bufptr, 4);
    pack (&mapRec->pmLgBootStart, &bufptr, 4);
    pack (&mapRec->pmBootSize, &bufptr, 4);
    pack (&mapRec->pmBootLoad, &bufptr, 4);
    pack (&mapRec->pmBootLoad2, &bufptr, 4);
    pack (&mapRec->pmBootEntry, &bufptr, 4);
    pack (&mapRec->pmBootEntry2, &bufptr, 4);
    pack (&mapRec->pmBootCksum, &bufptr, 4);
    pack (mapRec->pmProcessor, &bufptr, 16);
    pack (mapRec->pmBootBytes, &bufptr, 128);
    pack (mapRec->pmUnused, &bufptr, 248);
   
   
    retval = bufptr - buffer;
    return( retval );
   }

/*-- macPackDirRecord -----------
 *
 */

int macPackDirRecord( dirRec, buffer )
    struct dir_record *dirRec;
    char *buffer;
   {
    int retval = 0;
    char *bufptr = buffer;

    pack (&dirRec->dirType, &bufptr, 1);
    pack (&dirRec->dirReserv2, &bufptr, 1);
    pack (&dirRec->dirFlags, &bufptr, 2);
    pack (&dirRec->dirVal, &bufptr, 2);
    pack (&dirRec->dirDirID, &bufptr, 4);
    pack (&dirRec->dirCrDat, &bufptr, 4);
    pack (&dirRec->dirMdDat, &bufptr, 4);
    pack (&dirRec->dirBkDat, &bufptr, 4);
    pack (&dirRec->dirUsrInfo.frRect.top, &bufptr, 2);
    pack (&dirRec->dirUsrInfo.frRect.left, &bufptr, 2);
    pack (&dirRec->dirUsrInfo.frRect.bottom, &bufptr, 2);
    pack (&dirRec->dirUsrInfo.frRect.right, &bufptr, 2);
    pack (&dirRec->dirUsrInfo.frFlags, &bufptr, 2);
    pack (&dirRec->dirUsrInfo.frLocation.v, &bufptr, 2);
    pack (&dirRec->dirUsrInfo.frLocation.h, &bufptr, 2);
    pack (&dirRec->dirUsrInfo.frView, &bufptr, 2);
    pack (&dirRec->dirFndrInfo.frScroll.v, &bufptr, 2);
    pack (&dirRec->dirFndrInfo.frScroll.h, &bufptr, 2);
    pack (&dirRec->dirFndrInfo.frOpenChain, &bufptr, 4);
    pack (&dirRec->dirFndrInfo.frScript, &bufptr, 1);
    pack (&dirRec->dirFndrInfo.frXFlags, &bufptr, 1);
    pack (&dirRec->dirFndrInfo.frComment, &bufptr, 2);
    pack (&dirRec->dirFndrInfo.frPutAway, &bufptr, 4);
    pack (dirRec->dirReserv, &bufptr, TYP_FNDR_SIZE);

    retval = bufptr - buffer;
    return( retval );
   }

/*-- macUnpackDirRecord -----------
 *
 */

void macUnpackDirRecord( dirRec, buffer )
    struct dir_record *dirRec;
    char *buffer;
   {
    char *bufptr = buffer;

    unpack (&dirRec->dirType, &bufptr, 1);
    unpack (&dirRec->dirReserv2, &bufptr, 1);
    unpack (&dirRec->dirFlags, &bufptr, 2);
    unpack (&dirRec->dirVal, &bufptr, 2);
    unpack (&dirRec->dirDirID, &bufptr, 4);
    unpack (&dirRec->dirCrDat, &bufptr, 4);
    unpack (&dirRec->dirMdDat, &bufptr, 4);
    unpack (&dirRec->dirBkDat, &bufptr, 4);
    unpack (&dirRec->dirUsrInfo.frRect.top, &bufptr, 2);
    unpack (&dirRec->dirUsrInfo.frRect.left, &bufptr, 2);
    unpack (&dirRec->dirUsrInfo.frRect.bottom, &bufptr, 2);
    unpack (&dirRec->dirUsrInfo.frRect.right, &bufptr, 2);
    unpack (&dirRec->dirUsrInfo.frFlags, &bufptr, 2);
    unpack (&dirRec->dirUsrInfo.frLocation.v, &bufptr, 2);
    unpack (&dirRec->dirUsrInfo.frLocation.h, &bufptr, 2);
    unpack (&dirRec->dirUsrInfo.frView, &bufptr, 2);
    unpack (&dirRec->dirFndrInfo.frScroll.v, &bufptr, 2);
    unpack (&dirRec->dirFndrInfo.frScroll.h, &bufptr, 2);
    unpack (&dirRec->dirFndrInfo.frOpenChain, &bufptr, 4);
    unpack (&dirRec->dirFndrInfo.frScript, &bufptr, 1);
    unpack (&dirRec->dirFndrInfo.frXFlags, &bufptr, 1);
    unpack (&dirRec->dirFndrInfo.frComment, &bufptr, 2);
    unpack (&dirRec->dirFndrInfo.frPutAway, &bufptr, 4);
    unpack (dirRec->dirReserv, &bufptr, TYP_FNDR_SIZE);

   }

/*-- macPackFileRec -----------
 *
 */

int macPackFileRec( fileRec, buffer )
    struct file_record *fileRec;
    char *buffer;
   {
    int retval = E_NONE;
    char *bufptr = buffer;
 
    pack (&fileRec->cdrType, &bufptr, 1);
    pack (&fileRec->cdrReserv2, &bufptr, 1);
    pack (&fileRec->filFlags, &bufptr, 1);
    pack (&fileRec->filType, &bufptr, 1);
    pack (fileRec->filUsrWds.fdType, &bufptr, 4);
    pack (fileRec->filUsrWds.fdCreator, &bufptr, 4);
    pack (&fileRec->filUsrWds.fdFlags, &bufptr, 2);
    pack (&fileRec->filUsrWds.fdLocation.v, &bufptr, 2);
    pack (&fileRec->filUsrWds.fdLocation.h, &bufptr, 2);
    pack (&fileRec->filUsrWds.fdFldr, &bufptr, 2);
    pack (&fileRec->filFlNum, &bufptr, 4);
    pack (&fileRec->filStBlk, &bufptr, 2);
    pack (&fileRec->filLgLen, &bufptr, 4);
    pack (&fileRec->filPyLen, &bufptr, 4);
    pack (&fileRec->filRStBlk, &bufptr, 2);
    pack (&fileRec->filRLgLen, &bufptr, 4);
    pack (&fileRec->filRPyLen, &bufptr, 4);
    pack (&fileRec->filCrDat, &bufptr, 4);
    pack (&fileRec->filMdDat, &bufptr, 4);
    pack (&fileRec->filBkDat, &bufptr, 4);
    pack (&fileRec->filFndrInfo.fdIconID, &bufptr, 2);
    pack (fileRec->filFndrInfo.fdUnused, &bufptr, 6);
    pack (&fileRec->filFndrInfo.fdScript, &bufptr, 1);
    pack (&fileRec->filFndrInfo.fdXFlags, &bufptr, 1);
    pack (&fileRec->filFndrInfo.fdComment, &bufptr, 2);
    pack (&fileRec->filFndrInfo.fdPutAway, &bufptr, 4);
    pack (&fileRec->filClpSize, &bufptr, 2);
    pack (fileRec->filExtRec, &bufptr, sizeof(struct extent_descriptor)*3);
    pack (fileRec->filRExtRec, &bufptr, sizeof(struct extent_descriptor)*3);
    pack (&fileRec->filReserv, &bufptr, 4);

    retval = bufptr - buffer;
    return (retval);
   }

/*-- macUnpackFileRec -----------
 *
 */

void macUnpackFileRec( fileRec, buffer )
    struct file_record *fileRec;
    char *buffer;
   {
    char *bufptr = buffer;
 
    unpack (&fileRec->cdrType, &bufptr, 1);
    unpack (&fileRec->cdrReserv2, &bufptr, 1);
    unpack (&fileRec->filFlags, &bufptr, 1);
    unpack (&fileRec->filType, &bufptr, 1);
    unpack (fileRec->filUsrWds.fdType, &bufptr, 4);
    unpack (fileRec->filUsrWds.fdCreator, &bufptr, 4);
    unpack (&fileRec->filUsrWds.fdFlags, &bufptr, 2);
    unpack (&fileRec->filUsrWds.fdLocation.v, &bufptr, 2);
    unpack (&fileRec->filUsrWds.fdLocation.h, &bufptr, 2);
    unpack (&fileRec->filUsrWds.fdFldr, &bufptr, 2);
    unpack (&fileRec->filFlNum, &bufptr, 4);
    unpack (&fileRec->filStBlk, &bufptr, 2);
    unpack (&fileRec->filLgLen, &bufptr, 4);
    unpack (&fileRec->filPyLen, &bufptr, 4);
    unpack (&fileRec->filRStBlk, &bufptr, 2);
    unpack (&fileRec->filRLgLen, &bufptr, 4);
    unpack (&fileRec->filRPyLen, &bufptr, 4);
    unpack (&fileRec->filCrDat, &bufptr, 4);
    unpack (&fileRec->filMdDat, &bufptr, 4);
    unpack (&fileRec->filBkDat, &bufptr, 4);
    unpack (&fileRec->filFndrInfo.fdIconID, &bufptr, 2);
    unpack (fileRec->filFndrInfo.fdUnused, &bufptr, 6);
    unpack (&fileRec->filFndrInfo.fdScript, &bufptr, 1);
    unpack (&fileRec->filFndrInfo.fdXFlags, &bufptr, 1);
    unpack (&fileRec->filFndrInfo.fdComment, &bufptr, 2);
    unpack (&fileRec->filFndrInfo.fdPutAway, &bufptr, 4);
    unpack (&fileRec->filClpSize, &bufptr, 2);
    unpack (fileRec->filExtRec, &bufptr, sizeof(struct extent_descriptor)*3);
    unpack (fileRec->filRExtRec, &bufptr, sizeof(struct extent_descriptor)*3);
    unpack (&fileRec->filReserv, &bufptr, 4);

   }

/*-- macPackThreadRec -----------
 *
 */

int macPackThreadRec( thdRec, buffer )
    struct thread_record *thdRec;
    char *buffer;
   {
    int retval = E_NONE;
    char *bufptr = buffer;

    pack (&thdRec->thdType, &bufptr, 1);
    pack (&thdRec->thdReserv2, &bufptr, 1);
    pack (thdRec->thdReserv, &bufptr, 8);
    pack (&thdRec->thdParID, &bufptr, 4);
    pack (&thdRec->thdCNameLen, &bufptr, 1);
    pack (thdRec->thdCName, &bufptr, MAXMACNAME);

    retval = bufptr - buffer;
    return (retval);
   }
 
/*-- macUnpackThreadRec -----------
 *
 */

void macUnpackThreadRec( thdRec, buffer )
    struct thread_record *thdRec;
    char *buffer;
   {
    char *bufptr = buffer;

    unpack (&thdRec->thdType, &bufptr, 1);
    unpack (&thdRec->thdReserv2, &bufptr, 1);
    unpack (thdRec->thdReserv, &bufptr, 8);
    unpack (&thdRec->thdParID, &bufptr, 4);
    unpack (&thdRec->thdCNameLen, &bufptr, 1);
    unpack (thdRec->thdCName, &bufptr, MAXMACNAME);

   }
 
/*-- macPackAliasRec --------------
 *
 */

int macPackAliasRec (alsRec, buffer )
    struct alias_record *alsRec;
    char *buffer;
   {
    int retval = E_NONE;
    char *bufptr = buffer;
  
    pack (&alsRec->cdrType, &bufptr, 1);
    pack (&alsRec->cdrReserv2, &bufptr, 1);
    pack (alsRec->alsFlags, &bufptr, 12);
    pack (&alsRec->alsNameLen, &bufptr, 1);
    pack (alsRec->alsName, &bufptr, MAXMACNAME);

    retval = bufptr - buffer;
    return (retval);
   }

/*-- macUnpackAliasRec ---------------
 *
 */

void macUnpackAliasRec (alsRec, buffer )
    struct alias_record *alsRec;
    char *buffer;
   {
    char *bufptr = buffer;
  
    unpack (&alsRec->cdrType, &bufptr, 1);
    unpack (&alsRec->cdrReserv2, &bufptr, 1);
    unpack (alsRec->alsFlags, &bufptr, 12);
    unpack (&alsRec->alsNameLen, &bufptr, 1);
    unpack (alsRec->alsName, &bufptr, MAXMACNAME);
   }


/*-- macUnpackRecordKey -----------
 *
 */

void macUnpackRecordKey( keyptr, buffer )
    struct ckrKey *keyptr;
    char *buffer;
   {
    char *bufptr = buffer;

    unpack (&keyptr->ckrKeyLen, &bufptr, 1);
    unpack (&keyptr->ckrResrvl, &bufptr, 1);
    unpack (&keyptr->ckrParID, &bufptr, 4);
    unpack (&keyptr->ckrNameLen, &bufptr, 1);
    unpack (keyptr->ckrCName, &bufptr, keyptr->ckrNameLen);
    keyptr->ckrCName[keyptr->ckrNameLen] = '\0';

   }

/*-- macPackRecordKey -----------
 *
 * Returns length taken by key (including length of key and any padding)
 *
 */

int macPackRecordKey( buffer, keyptr )
    char *buffer;
    struct ckrKey *keyptr;
   {
    int retval;
    char *bufptr = buffer;

    keyptr->ckrKeyLen += (keyptr->ckrKeyLen % 2)? 0 : 1;
    retval = keyptr->ckrKeyLen + 1;

    pack (&keyptr->ckrKeyLen, &bufptr, 1);
    pack (&keyptr->ckrResrvl, &bufptr, 1);
    pack (&keyptr->ckrParID, &bufptr, 4);
    pack (&keyptr->ckrNameLen, &bufptr, 1);
    pack (keyptr->ckrCName, &bufptr, keyptr->ckrNameLen);

    return( retval );
   }

/*-- macPackExtRecordKey -----------
 *
 *
 */

int macPackExtRecordKey( buffer, keyptr )
    char *buffer;
    struct xkrKey *keyptr;
   {
    int retval;
    char *bufptr = buffer;

    pack (&keyptr->xkrKeyLen, &bufptr, 1);
    pack (&keyptr->xkrFkType, &bufptr, 1);
    pack (&keyptr->xkrFNum, &bufptr, 4);
    pack (&keyptr->xkrFABN, &bufptr, 2);

    retval = bufptr - buffer;
    return( retval );
   }

/*-- macUnpackExtRecordKey -----------
 *
 *
 */

void macUnpackExtRecordKey( keyptr, buffer )
    struct xkrKey *keyptr;
    char *buffer;
   {
    char *bufptr = buffer;

    unpack (&keyptr->xkrKeyLen, &bufptr, 1);
    unpack (&keyptr->xkrFkType, &bufptr, 1);
    unpack (&keyptr->xkrFNum, &bufptr, 4);
    unpack (&keyptr->xkrFABN, &bufptr, 2);

   }

/*--- macPackHeaderRec ---------------------
 *
 */

int macPackHeaderRec( hdrRec, buffer )
    struct header_record    *hdrRec;
    char                    *buffer;
   {
    char *bufptr = buffer;

    pack (&hdrRec->hrDepthCount, &bufptr, 2);
    pack (&hdrRec->hrRootNode, &bufptr, 4);
    pack (&hdrRec->hrTotalRecs, &bufptr, 4);
    pack (&hdrRec->hrFirstLeaf, &bufptr, 4);
    pack (&hdrRec->hrLastLeaf, &bufptr, 4);
    pack (&hdrRec->hrNodeSize, &bufptr, 2);
    pack (&hdrRec->hrKeyLen, &bufptr, 2);
    pack (&hdrRec->hrTotalNodes, &bufptr, 4);
    pack (&hdrRec->hrFreeNodes, &bufptr, 4);
    pack (hdrRec->hrUnused, &bufptr, 76);

    return(bufptr - buffer);
   }

/*--- macUnpackHeaderRec ---------------------
 *
 */

void macUnpackHeaderRec( hdrRec, buffer )
    struct header_record    *hdrRec;
    char                    *buffer;
   {
    char *bufptr = buffer;

    unpack (&hdrRec->hrDepthCount, &bufptr, 2);
    unpack (&hdrRec->hrRootNode, &bufptr, 4);
    unpack (&hdrRec->hrTotalRecs, &bufptr, 4);
    unpack (&hdrRec->hrFirstLeaf, &bufptr, 4);
    unpack (&hdrRec->hrLastLeaf, &bufptr, 4);
    unpack (&hdrRec->hrNodeSize, &bufptr, 2);
    unpack (&hdrRec->hrKeyLen, &bufptr, 2);
    unpack (&hdrRec->hrTotalNodes, &bufptr, 4);
    unpack (&hdrRec->hrFreeNodes, &bufptr, 4);
    unpack (hdrRec->hrUnused, &bufptr, 76);

   }

/*--- macUnpackNodeDescriptor ---------------------
 *
 */

void macUnpackNodeDescriptor( nodeDesc, buffer )
    struct node_descriptor    *nodeDesc;
    char                      *buffer;
   {
    char *bufptr = buffer;

    unpack (&nodeDesc->ndFLink, &bufptr, 4);
    unpack (&nodeDesc->ndBLink, &bufptr, 4);
    unpack (&nodeDesc->ndType, &bufptr, 1);
    unpack (&nodeDesc->ndLevel, &bufptr, 1);
    unpack (&nodeDesc->ndRecs, &bufptr, 2);
    unpack (&nodeDesc->ndReserved, &bufptr, 2);

   }

/*--- macPackNodeDescriptor ---------------------
 *
 */

int macPackNodeDescriptor(nodeBuffer, nodeDesc)
    char                      *nodeBuffer;
    struct node_descriptor    *nodeDesc;
   {
    char *bufptr = nodeBuffer;

    pack (&nodeDesc->ndFLink, &bufptr, 4);
    pack (&nodeDesc->ndBLink, &bufptr, 4);
    pack (&nodeDesc->ndType, &bufptr, 1);
    pack (&nodeDesc->ndLevel, &bufptr, 1);
    pack (&nodeDesc->ndRecs, &bufptr, 2);
    pack (&nodeDesc->ndReserved, &bufptr, 2);

    return( bufptr - nodeBuffer);
   }


/*--- getRecordOffset ---------------------
 *
 */

int getRecordOffset(targetRecNum, nodeBuffer)
    int       targetRecNum;
    short     *nodeBuffer;
   {
    return( (short)*(nodeBuffer+((NODE_SIZE/sizeof(short))-targetRecNum)-1) );
   }

/*-- putRecordOffset ---------
 *
 */

void putRecordOffset( nodeBuffer, recNum, offset )
    short *nodeBuffer;
    int   recNum;
    int   offset;
   {

    *(nodeBuffer+((NODE_SIZE/sizeof(short))-recNum)-1) = offset;

   }

