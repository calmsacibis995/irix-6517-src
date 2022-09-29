/* 
   (C) Copyright Digital Instrumentation Technology, Inc. 1990 - 1993 
       All Rights Reserved
*/

/*
    TransferPro macCatalog.c  -  Functions for managing the Catalog Tree.
*/

#include <malloc.h>
#include "macPort.h"
#include "macSG.h"
#include "dm.h"

static char *Module = "macCatalog";
extern int macReadExtent();

static int getRecList(struct file_list **, struct leaf_node *, int *, int),
           AddToList(struct file_list **, void *);

struct file_list **getEndofList();

/*--------------------- macReadCatalog ------------------------------
    Reads the Catalog Tree.
---------------------------------------------------------------------*/

int macReadCatalog (volume)
    struct m_volume *volume;
    {
    int               retval = E_NONE;
    char             *funcname = "macReadCatalog";
    char             *buffer = (char *)0;
    int               i = 0; 
    unsigned int      offset = 0;
    unsigned int      xNodeNum;
    int               xRecNum;
    struct extent_record *Xextent = (struct extent_record *)0;
    struct record_key recKey;
    struct extent_descriptor tempExtent;
    struct BtreeNode tempNode;

    /* Allocate buffer for the catalog tree. */

    if( (buffer = malloc ((unsigned)(volume->vib->drCTFlSize*sizeof(char)))) 
         == (char *)0)
       {
        retval = set_error(E_MEMORY, Module, funcname, "");
       }
    else
       {
        for( i = 0; i < volume->vib->drCTFlSize; i++ )
           {
            *(buffer+i)= '\0';
           }
       }

    if ( retval == E_NONE )
       {
        /* read the header node, so we can get the bitmap and do
           selective reading */
        tempExtent.ed_start = volume->vib->drCTExtRec[0].ed_start;
        tempExtent.ed_length = 1;
        if ( (retval = macReadExtent(volume, &tempExtent, buffer)) 
            == E_NONE &&
            (retval = macUnPackHeaderNode(&tempNode.BtNodes.BtHeader,buffer))
            == E_NONE )
           {
            for ( i=0; i < sizeof (struct bitmap_record); i++)
               {
                volume->catBitmap.bitmap[i] = 0;
               }
           }

        if ( tempNode.BtNodes.BtHeader.hnDesc.ndFLink != UNASSIGNED_NODE )
           {
            retval = set_error( E_RANGE, Module, funcname,
                "Map node found" );
           }
       }

    i = 0;
    while (retval == E_NONE && volume->vib->drCTExtRec[i].ed_length && i < 3 )
       {
        retval = macReadExtent(volume, &(volume->vib->drCTExtRec[i]), 
                               buffer+offset);
        offset += volume->vib->drCTExtRec[i].ed_length * 
                                                   volume->vib->drAlBlkSiz;
        i++;
       }

    /* search extents tree for Catalog file records */
    while( retval == E_NONE && offset < volume->vib->drCTFlSize )
       { 
        buildXRecKey (&recKey, CATALOG_FILE_ID, DATA_FORK,
            (unsigned short)(offset/volume->vib->drAlBlkSiz) );

        if( (retval = searchTree(volume, &recKey, EXTENTS_TREE, 
            &xRecNum, &xNodeNum)) == E_NONE )
           {
            Xextent = (struct extent_record *) 
                volume->extents[xNodeNum]->BtNodes.BtLeaf.lnRecList[xRecNum];
           }
        
        i = 0;
        while (retval == E_NONE && Xextent->xkrExtents[i].ed_length && i < 3 )
           {
            retval = macReadExtent(volume, &Xextent->xkrExtents[i], 
                buffer+offset);
            offset += (Xextent->xkrExtents[i].ed_length * 
                                                  volume->vib->drAlBlkSiz);
            i++;
           }
       }
    /* Build a b*-tree from the buffer. */

    if (retval == E_NONE ) 
       {
        retval = macBuildTree(volume, CATALOG_TREE, buffer); 
       }
    if ( buffer )
       {
        free( buffer );
       }

    return (retval);
   }

/*-------------------------- macFindFile ------------------------------
  populates fileptr from current directory
------------------------------------------------------------------------*/
int macFindFile( volume, nodeNum, recNum, fileRec, filename, CWDid )
    struct m_volume    *volume;
    int                 *nodeNum;
    int                 *recNum;
    struct file_record **fileRec;
    char                *filename;
    int                 CWDid;
   {
    int                 retval = E_NONE;
    struct record_key   currRecKey;
    char               *funcname = "macFindFile";

    buildRecKey(&currRecKey, CWDid, filename);

    if( (retval=searchTree(volume, &currRecKey, CATALOG_TREE,
              recNum, (unsigned int *)nodeNum)) != E_NONE )
       {
        retval = set_error(E_NOTFOUND, Module, funcname, "file not found.");
       }
    else
       {
        *fileRec = (struct file_record *) 
          volume->catalog[*nodeNum]->BtNodes.BtLeaf.lnRecList[*recNum];
       }

    return(retval);
   }

/*-------------------------- macFindAlias ------------------------------
 Finds an alias_record in a B*-tree
------------------------------------------------------------------------*/
int macFindAlias( volume, nodeNum, recNum, aliasRec, filename, CWDid )
    struct m_volume    *volume;
    int                 *nodeNum;
    int                 *recNum;
    struct alias_record **aliasRec;
    char                *filename;
    int                 CWDid;
   {
    int                 retval = E_NONE;
    struct record_key   currRecKey;
    char               *funcname = "macFindAlias";

    buildRecKey(&currRecKey, CWDid, filename);

    if( (retval=searchTree(volume, &currRecKey, CATALOG_TREE,
              recNum, (unsigned int *)nodeNum)) != E_NONE )
       {
        retval = set_error(E_NOTFOUND, Module, funcname, 
                                       "alias record not found.");
       }
    else
       {
        *aliasRec = (struct alias_record *) 
          volume->catalog[*nodeNum]->BtNodes.BtLeaf.lnRecList[*recNum];
       }

    return(retval);
   }

/*-------------------------- macFindDir ------------------------------
   Finds a directory record in a B*-Tree
------------------------------------------------------------------------*/
int macFindDir( volume, nodeNum, recNum, dirRec, dirname, CWDid )
    struct m_volume     *volume;
    unsigned int        *nodeNum;
    int                 *recNum;
    struct dir_record  **dirRec;
    char                *dirname;
    int                 CWDid;
   {
    int                 retval = E_NONE;
    struct record_key   currRecKey;
    char               *funcname = "macFindDir";

    buildRecKey(&currRecKey, CWDid, dirname);

    if( (retval=searchTree(volume, &currRecKey, 
                          CATALOG_TREE, recNum, nodeNum)) !=E_NONE )
       {
        retval = set_error(E_NOTFOUND, Module, funcname, 
                           "directory not found.");
       }
    else
       {
        *dirRec = (struct dir_record *) 
            volume->catalog[*nodeNum]->BtNodes.BtLeaf.lnRecList[*recNum];
       }

    return(retval);
   }


/*------------------ macDirId ----------------------------------------------

   Finds the directory ID of the given Mac directory path name.

---------------------------------------------------------------------------*/
int macDirId ( volume, path, dirId )
    struct m_volume *volume;
    char *path;
    int *dirId;
   {
    int retval = E_NONE;

    retval = macAbsScandir ( volume, path, dirId, NO, (struct file_list **)0);

    return (retval);
}

/*-------------------------- macAbsScandir ------------------------------
   Finds directory's file list and populates macFileList using UNIX style
   absolute path
------------------------------------------------------------------------*/
int macAbsScandir(volume, pathname, CWDid, buildList, macFileList)
    struct m_volume     *volume;
    char                *pathname;
    int                 *CWDid;
    BOOL                 buildList;
    struct file_list   **macFileList;
   {
    int retval = E_NONE;
    char tempname[MAXMACNAME+2],
         *colonloc;

    if( (colonloc = strchr(pathname,':')) )
       {
        strncpy( tempname, pathname, (int)(colonloc-pathname) );
        *(tempname+(int)(colonloc-pathname)) = '\0';
        if( !*tempname )
           {
            strncpy(tempname, (char *)volume->vib->drVolName, 
                    (int)volume->vib->drVolNameLen);
            *(tempname + volume->vib->drVolNameLen) = '\0';
            *CWDid = ROOT_PAR_ID;
           }
       }
    else
       {
        strcpy( tempname, pathname );
       }

    retval = macScandir( volume, tempname, CWDid, buildList, macFileList );

    if( retval == E_NONE  && colonloc && *(colonloc+1) )
       {
        freeMacFileList( macFileList );
        retval = macAbsScandir( volume, colonloc+1, CWDid, buildList,
                                 macFileList);
       }

    return( retval );
   }


/*-------------------------- macScandir ------------------------------
   Finds directory's file list and populates macFileList
------------------------------------------------------------------------*/
int macScandir(volume, dirname, CWDid, buildList, macFileList)
    struct m_volume     *volume;
    char                *dirname;
    int                 *CWDid;
    BOOL                 buildList;
    struct file_list   **macFileList;
   {
    int                 retval = E_NONE,
                        thdNum = 0,
                        recsFound = 0,
                        totalRecsFound = 0;
    unsigned int        nodeNum = 0;
    int                 recNum;
    char               *funcname = "macScandir";
    struct dir_record  *dirptr = (struct dir_record *)0;
    struct record_key  recKey;


    /* look for subdirectory in current directory */

    retval = macFindDir(volume, &nodeNum, &recNum, &dirptr, dirname, *CWDid );

    /* look for thread record associated with that directory */
    if( retval == E_NONE )
       {
        buildRecKey(&recKey, (int)dirptr->dirDirID, "" );
        retval = searchTree(volume, &recKey, CATALOG_TREE, &thdNum, &nodeNum);
       }
    
    if ( buildList )
       {
       
        /* if thread rec last file in list, advance node number */
        if(  retval == E_NONE &&
        (unsigned short)(thdNum + 1) > volume->catalog[nodeNum]->BtNodes.BtLeaf.lnDesc.ndRecs )
           {
            nodeNum = volume->catalog[nodeNum]->BtNodes.BtLeaf.lnDesc.ndFLink;
           }
    
        totalRecsFound = 0;
    
        while( retval == E_NONE && (unsigned short)totalRecsFound < dirptr->dirVal )
           {
            retval = getRecList(macFileList, 
                                (struct leaf_node *)volume->catalog[nodeNum],
                                &recsFound, (int)dirptr->dirDirID);
            totalRecsFound += recsFound;
            if( (unsigned short)totalRecsFound < dirptr->dirVal )
               {
                if ( (nodeNum = 
                  volume->catalog[nodeNum]->BtNodes.BtLeaf.lnDesc.ndFLink) == 0)
                   {
                    retval = set_error( E_NOTFOUND, Module, funcname,
                        "Invalid catalog tree" );
                   }
               }
           }
       }

    if( retval == E_NONE )
       {
        *CWDid = dirptr->dirDirID;
       }
    return(retval);
   }

/*-------------------------- getRecList ------------------------------
   populates file list from catalog tree
------------------------------------------------------------------------*/
static int getRecList(macFileList,  leafNode,  recsFound, currID)
    struct file_list   **macFileList;
    struct leaf_node    *leafNode;
    int                 *recsFound,
                         currID;
   {
    int                 retval = E_NONE,
                        currRecNum = 0;

    *recsFound = 0;

    /* roam through record list, looking for matching records */ 

    while( retval == E_NONE && (unsigned short)currRecNum < leafNode->lnDesc.ndRecs
        && currID >=
        ((struct file_record *)leafNode->lnRecList[currRecNum])->
        filRecKey.key.ckr.ckrParID)
       {
        /* find first record in node with matching parent id */
        if( ((struct file_record *)leafNode->lnRecList[currRecNum])->
            filRecKey.key.ckr.ckrParID < currID )
           {
            currRecNum++;
           }
         
        /* deal with thread records at start and end of list */
        else if(  ((struct thread_record *)leafNode->lnRecList[currRecNum])->
                    thdType == THREAD_REC)
           {
            currRecNum++;   /* advance past first thread record */
           }

        /* check current record's ckrParID for validity */
        else
           {
            /* add valid file or directory record to file list */
            retval = AddToList(macFileList, leafNode->lnRecList[currRecNum]);
            (*recsFound)++;
            currRecNum++;
           }
       }
    return(retval);
   }

/*-------------------------- freeMacFileList ------------------------------
  frees file list for fresh list
------------------------------------------------------------------------*/
int freeMacFileList( macFileList )
    struct file_list **macFileList;
   {
    int    retval = E_NONE;

    if( macFileList && *macFileList )
       {
        if ( (*macFileList)->flNext )
           {
            retval = freeMacFileList( &(*macFileList)->flNext );
           }
        free( (char *)(*macFileList) );
        *macFileList = (struct file_list *)0;
       }

    return( retval );
   }

/*-------------------------- AddToList ------------------------------
   populates file list from catalog tree
------------------------------------------------------------------------*/
static int AddToList(macFileList, currRec)
    struct file_list   **macFileList;
    void                *currRec;
   {
    int                  retval = E_NONE,
                         i;
    static struct file_list    *ListEnd = 0;
    struct file_list    *newNode = (struct file_list *)0;
    char                *funcname = "AddToList";
 
    if( (newNode = (struct file_list *)malloc(sizeof(struct file_list))) 
        == (struct file_list *) 0 )
       {
        retval = set_error(E_MEMORY,Module,funcname,"");
       }

    /* if all is well, populate new node */
    if( retval == E_NONE )
       {
        /* zero out newNode structure */
        newNode->flNext = (struct file_list *)0;
        for( i = 0; i < sizeof(struct file_record); i++)
           {
            newNode->entry.flEntryBytes[i] = 0;
           }

        switch(  *( (char *)currRec + sizeof (struct record_key)) )
           {
            case DELETED_REC:
                break;
            case FILE_REC:
                memcpy((char *)&newNode->entry.flFilRec, 
                       (char *)currRec, 
                       sizeof(struct file_record));
                break;
            case DIRECTORY_REC:
                memcpy( (char *)&newNode->entry.flDirRec,  
                        (char *)currRec, 
                        sizeof(struct dir_record));
                break;
            case ALIAS_REC:
                memcpy( (char *)&newNode->entry.flAlsRec,  
                        (char *)currRec, 
                        sizeof(struct alias_record));
                break;
            default:
                retval = set_error(E_SYNTAX,Module,funcname,
                    "Invalid record in leaf node");
                break;
           }
       }

    /* add new node to list at appropriate spot */
    if( *macFileList == (struct file_list *) 0 )
       {
        *macFileList = newNode;
        ListEnd = *macFileList;
       }
    else
       {
        ListEnd->flNext = newNode;
        ListEnd = ListEnd->flNext;
       }

    return(retval);
   }


/*-------------------------- macWriteCatalog ------------------------------
  writes catalog tree file to disk
------------------------------------------------------------------------*/
int macWriteCatalog( volume ) 
    struct m_volume *volume;
  {
   int        retval = E_NONE,
              i = 0,
              xRecNum;
    unsigned int offset = 0;
    unsigned int  xNodeNum;
    char     *buffer = 0;
    char     *funcname = "macWriteCatalog";
    struct extent_record *Xextent = (struct extent_record *)0;
    struct record_key recKey;
    
    if( (buffer = malloc(volume->vib->drCTFlSize)) == (char *)0 )
       {
        retval = set_error(E_MEMORY, Module, funcname, "");
       }
    else
       {
        /* zero out catalog tree buffer */
        for( i = 0; i < volume->vib->drCTFlSize; i++ )
           {
            *(buffer+i) = 0;
           }
        /* pack catalog tree into buffer */
        retval = macPackTree (volume->catalog, CATALOG_TREE, 
                              volume->vib, buffer);
       }

    /* write extents tracked in vib */
    for( i=0,offset=0;
        retval == E_NONE && volume->vib->drCTExtRec[i].ed_length && i < 3;
        offset += volume->vib->drCTExtRec[i].ed_length * 
        volume->vib->drAlBlkSiz, i++ )
       {
        retval = macWriteTreeExtent(volume, &(volume->vib->drCTExtRec[i]), 
                                buffer, offset, CATALOG_TREE );
       }

    /* if necessary, search Extents tree */
    while( retval == E_NONE && offset < volume->vib->drCTFlSize )
       {
        /* when Mac builds 'em, it calls 'em DATA_FORK extents */
        /* even though by rights they ought to be RESOURCE_FORK */

        buildXRecKey (&recKey, CATALOG_FILE_ID, DATA_FORK,
                (unsigned short)(offset/volume->vib->drAlBlkSiz ));

        if( (retval = searchTree(volume, &recKey, EXTENTS_TREE, 
            &xRecNum, &xNodeNum)) == E_NONE )
            {
             Xextent = (struct extent_record *) 
                 volume->extents[xNodeNum]->BtNodes.BtLeaf.lnRecList[xRecNum];
            }

         i = 0;
         while (retval == E_NONE && Xextent->xkrExtents[i].ed_length && i < 3 )
            {
             retval = macWriteTreeExtent(volume, &Xextent->xkrExtents[i], 
                                     buffer, offset, CATALOG_TREE );
             offset += Xextent->xkrExtents[i].ed_length * 
                       volume->vib->drAlBlkSiz;
             i++;
            }
        }
    /* reset the catalog bitmap */
    if ( retval == E_NONE )
       {
        for ( i=0; i < sizeof (struct bitmap_record); i++)
           {
            volume->catBitmap.bitmap[i] = 0;
           }
       }
    if ( buffer )
       {
        free( buffer );
       }

    return(retval);
   }

/*-------------------------- buildRecKey ------------------------------
   builds a record key for target file 
------------------------------------------------------------------------*/
void buildRecKey(RecKeyPtr, parID, name)
    struct record_key *RecKeyPtr;
    int                parID;
    char              *name;
   {

    RecKeyPtr->key.ckr.ckrResrvl =   0;
    RecKeyPtr->key.ckr.ckrParID =  parID;
    RecKeyPtr->key.ckr.ckrNameLen =  strlen(name);
    strcpy( (char *)RecKeyPtr->key.ckr.ckrCName, name);
 
    RecKeyPtr->key.ckr.ckrKeyLen = (strlen(name)+2*sizeof(char)+sizeof(int));

   }

/*-------------------------- scanCPtrRecs ------------------------------
   searches index node for pointer record associated with targetKey
------------------------------------------------------------------------*/
int scanCPtrRecs(targetKey, currIndex, childNodeNum, targetRec)
    struct record_key   *targetKey;
    struct index_node   *currIndex;
    unsigned int        *childNodeNum;
    int                 *targetRec;
   {
    int    retval = E_NONE,
           done = 0,
           compare = 0,
           i = 0;
    char     *funcname = "scanCPtrRecs";

    while( retval == E_NONE && done == 0 && (unsigned short)i < currIndex->inDesc.ndRecs ) 
       {
        /* Whoa! beyond records with this ckrParId */
        if( targetKey->key.ckr.ckrParID < 
                 currIndex->inRecList[i]->ptrRecKey.key.ckr.ckrParID )
           {
            retval = set_error(E_NOTFOUND, Module, funcname, "File not found");
           }

        /* advance through record list */
        else if( targetKey->key.ckr.ckrParID > 
                 currIndex->inRecList[i]->ptrRecKey.key.ckr.ckrParID )
           {
            i++;
           }

        /* following test evaluated only if ckrParID's are equal */
        else
           {
            if ( targetKey->key.ckr.ckrNameLen != 
                currIndex->inRecList[i]->ptrRecKey.key.ckr.ckrNameLen )
               {
                /* don't ask */
                if ( (compare = strnaicmp(targetKey->key.ckr.ckrCName, 
                   currIndex->inRecList[i]->ptrRecKey.key.ckr.ckrCName,
                    (targetKey->key.ckr.ckrNameLen > 
                    currIndex->inRecList[i]->ptrRecKey.key.ckr.ckrNameLen?
                    currIndex->inRecList[i]->ptrRecKey.key.ckr.ckrNameLen :
                    targetKey->key.ckr.ckrNameLen))) < 0 ||
                    (compare == 0 &&
                    (currIndex->inRecList[i]->ptrRecKey.key.ckr.ckrNameLen >
                    targetKey->key.ckr.ckrNameLen ||
                    !(compare = *(targetKey->key.ckr.ckrCName
                    +currIndex->inRecList[i]->ptrRecKey.key.
                    ckr.ckrNameLen)))) )
                   {
                    retval = set_error(E_NOTFOUND, Module, funcname, 
                             "File not found");
                   }
               }
            else if ( (compare = strnaicmp(targetKey->key.ckr.ckrCName, 
                currIndex->inRecList[i]->ptrRecKey.key.ckr.ckrCName,
                targetKey->key.ckr.ckrNameLen)) < 0 )
               {
                retval = set_error(E_NOTFOUND, Module, funcname, 
                                   "File not found");
               }
            if ( retval == E_NONE )
               {
                if ( compare == 0 )
                   {
                    done = 1;
                   }
                else
                   {
                    i++;
                   }
               }
           }
       }

    *targetRec = i;

    if( (unsigned short)i >= currIndex->inDesc.ndRecs )
        {
          retval = set_error(E_NOTFOUND, Module, funcname, 
                                   "File not found");
         *childNodeNum = UNASSIGNED_NODE;
        }
    else
        {
         *childNodeNum = currIndex->inRecList[i]->ptrPointer;
        }

    return(retval);
   }

/*-------------------------- scanCLeafRecs ------------------------------
   searches leaf node for record associated with targetKey entity
------------------------------------------------------------------------*/
int scanCLeafRecs(targetKey, leafNode, targetRec)
    struct record_key   *targetKey;
    struct leaf_node    *leafNode;
    int                 *targetRec;
   {
    int retval = E_NONE,
        i = 0,
        compare = 1;
    char     *funcname = "scanCLeafRecs";
    void *rec;

    while( compare > 0 &&
        retval == E_NONE && 
        (unsigned short)i < leafNode->lnDesc.ndRecs )
       {
        rec = leafNode->lnRecList[i];
        if( (void *)leafNode->lnRecList[i] == (void *)0 )
           {
            retval = set_error(E_NOTFOUND, Module, funcname, "File not found");
           }
        else
           {
            switch(*((char *)rec + sizeof(struct record_key)))
               {
                case FILE_REC:
                     if( targetKey->key.ckr.ckrParID == 
                     ((struct file_record *)rec)->filRecKey.key.ckr.ckrParID )
                        {
                         if ( (compare = strnaicmp(targetKey->key.ckr.ckrCName,    
                         ((struct file_record *)rec)->filRecKey.key.ckr.ckrCName,     
                         ((targetKey->key.ckr.ckrNameLen <
                          ((struct file_record *)rec)->filRecKey
                          .key.ckr.ckrNameLen)?
                         targetKey->key.ckr.ckrNameLen :
                         ((struct file_record *)rec)->filRecKey
                          .key.ckr.ckrNameLen))) == 0 )
                            {
                             compare = targetKey->key.ckr.ckrNameLen -
                                 ((struct file_record *)rec)->filRecKey.
                                 key.ckr.ckrNameLen;
                            }
                        }
                     break;

                case DIRECTORY_REC:
                     if( targetKey->key.ckr.ckrParID == 
                     ((struct dir_record *)rec)->dirRecKey.key.ckr.ckrParID )
                        {
                         if ( (compare = strnaicmp(targetKey->key.ckr.ckrCName, 
                         ((struct dir_record *)rec)->dirRecKey.key.ckr.ckrCName,
                         ((targetKey->key.ckr.ckrNameLen <
                          ((struct dir_record *)rec)->dirRecKey
                          .key.ckr.ckrNameLen)?
                         targetKey->key.ckr.ckrNameLen :
                         ((struct dir_record *)rec)->dirRecKey
                          .key.ckr.ckrNameLen))) == 0 )
                            {
                             compare = targetKey->key.ckr.ckrNameLen -
                                 ((struct dir_record *)rec)->dirRecKey.
                                 key.ckr.ckrNameLen;
                            }
                        }
                     break;

                case THREAD_REC:
                     if( !Nstricmp(targetKey->key.ckr.ckrCName, "") )
                        { 
                         if( targetKey->key.ckr.ckrParID == 
                         ((struct thread_record *)rec)->thdRecKey.
                          key.ckr.ckrParID )
                            {
                              compare = 0;
                            }
                        }
                     break;

                case ALIAS_REC:
                   if( targetKey->key.ckr.ckrParID == 
                     ((struct alias_record *)rec)->alsRecKey.key.ckr.ckrParID )
                        {  
                         if ( (compare = strnaicmp(targetKey->key.ckr.ckrCName, 
                         ((struct alias_record *)rec)->alsName,
                         ((targetKey->key.ckr.ckrNameLen <
                          ((struct alias_record *)rec)->alsNameLen)?
                         targetKey->key.ckr.ckrNameLen :
                         ((struct alias_record *)rec)->alsNameLen))) == 0)
                            {
                             compare = targetKey->key.ckr.ckrNameLen -
                                 ((struct alias_record *)rec)->alsNameLen;
                            }
                        }   
                      break;
                default:
                     retval = set_error(E_NOTFOUND, Module, funcname, 
                         "Invalid record in leaf node.");
                     break;
               }
           }
        if ( retval == E_NONE )
           {
            if( compare < 0 || 
                targetKey->key.ckr.ckrParID < 
                ((struct file_record *)rec)->filRecKey.key.ckr.ckrParID )
               {
                retval = set_error(E_NOTFOUND, Module, funcname, "key not found");
               }
            else if( compare != 0 )
               {
                i++;
               }
           }
       }
    
    if (i ==  leafNode->lnDesc.ndRecs )
       {
        retval = set_error(E_NOTFOUND, Module, funcname, "key not found");
       }
    *targetRec = i;
    return(retval);
   }            
 

/*--------------- macAddRecToDir -------------------------------------------
 * backtrack through leaf node(s) to find thread rec for curr dir (to get
 * curr dir name), call macFindDir to find directory_record,
 * update dirMdDat and dirVal fields
 */ 

int macAddRecToDir( volume, currRec, currNode )
    struct m_volume *volume;
    int           currRec;
    unsigned int   currNode;
   {
    int        retval = E_NONE,
               found = 0,
               currParID = 
                   ((struct file_record *)volume->catalog[currNode]->
                   BtNodes.BtLeaf.lnRecList[currRec])->
                   filRecKey.key.ckr.ckrParID;
    unsigned int dirNodeNum = 0;
    int        dirRecNum = 0;
    char       dirName[MAXMACNAME+2];
    struct dir_record *dirPtr = (struct dir_record *)0;
    int        CWDid;

    /* spin through node looking for thread record */
    while( !found && 
           ((struct file_record *)volume->catalog[currNode]->BtNodes.BtLeaf.
           lnRecList[currRec])->filRecKey.key.ckr.ckrParID == currParID )
       {
        if( ((struct thread_record *)volume->catalog[currNode]->BtNodes.
              BtLeaf.lnRecList[currRec])->thdType == THREAD_REC )
           {
            found = 1;
           } 
        else if( currRec == 0 )
           {
            currNode = volume->catalog[currNode]->BtNodes.BtLeaf.lnDesc.ndBLink;
            currRec = (volume->catalog[currNode]->BtNodes.BtLeaf.lnDesc.ndRecs) - 1;
           }
        else
           {
            currRec--;
           }
       }

    /* if thread rec found, get name and look for directory record */
    if( found )
       {
        strncpy(dirName, (char *)((struct thread_record *)
        volume->catalog[currNode]->BtNodes.BtLeaf.lnRecList[currRec])->thdCName,
        (int)(((struct thread_record *)volume->catalog[currNode]->BtNodes. 
            BtLeaf.lnRecList[currRec])->thdCNameLen)+1);

        dirName[(((struct thread_record *)volume->catalog[currNode]->BtNodes.
                BtLeaf.lnRecList[currRec])->thdCNameLen)] = '\0';
       

        CWDid = ((struct thread_record *)volume->catalog[currNode]->BtNodes.
                BtLeaf.lnRecList[currRec])->thdParID;
        retval = macFindDir( volume, &dirNodeNum, &dirRecNum, &dirPtr, dirName,
                             CWDid);

        /* if directory record is now in hand, update it */
        if( retval == E_NONE )
           {
            dirPtr->dirVal++;
            dirPtr->dirMdDat = macDate();
            macSetBitmapRec ( &volume->catBitmap, dirNodeNum );
           }
       }

    return( retval );
   }

/*--------------- macDelRecFromDir -------------------------------------------
 * backtrack through leaf node(s) to find thread rec for curr dir (to get
 * curr dir name), call macFindDir to find directory_record,
 * update dirMdDat and dirVal fields
 */ 

int macDelRecFromDir( volume, currRec, currNode )
    struct m_volume *volume;
    int              currRec;
    unsigned int     currNode;
   {
    int        retval = E_NONE,
               found = 0,
               currParID = 
                   ((struct file_record *)volume->catalog[currNode]->BtNodes.
                   BtLeaf.lnRecList[currRec])->filRecKey.key.ckr.ckrParID;
    unsigned int  dirNodeNum = 0;
    int        dirRecNum = 0;
    char       dirName[MAXMACNAME+2];
    struct dir_record *dirPtr = (struct dir_record *)0;
    int CWDid;

    /* spin through node looking for thread record */
    while( !found && 
        ((struct file_record *)volume->catalog[currNode]->BtNodes.BtLeaf.
         lnRecList[currRec])->filRecKey.key.ckr.ckrParID == currParID )
       {
        if( ((struct thread_record *)volume->catalog[currNode]->BtNodes.
         BtLeaf.lnRecList[currRec])->thdType == THREAD_REC )
           {
            found = 1;
           } 
        else if( currRec == 0 )
           {
            currNode = volume->catalog[currNode]->BtNodes.BtLeaf.lnDesc.ndBLink;
            currRec = (volume->catalog[currNode]->BtNodes.BtLeaf.lnDesc.ndRecs) - 1;
           }
        else
           {
            currRec--;
           }
       }

    /* if thread rec found, get name and look for directory record */
    if( found )
       {
        strncpy(dirName, (char *)((struct thread_record *)
            volume->catalog[currNode]->BtNodes.BtLeaf.
            lnRecList[currRec])->thdCName, 
            (int)(((struct thread_record *)volume->catalog[currNode]->BtNodes. 
            BtLeaf.lnRecList[currRec])->thdCNameLen)+1);

        dirName[(((struct thread_record *)volume->catalog[currNode]->BtNodes.
                BtLeaf.lnRecList[currRec])->thdCNameLen)] = '\0';

        CWDid = ((struct thread_record *)volume->catalog[currNode]->BtNodes.
           BtLeaf.lnRecList[currRec])->thdParID;
        retval = macFindDir( volume, &dirNodeNum, &dirRecNum, &dirPtr, dirName,
                             CWDid);

        /* if directory record is now in hand, update it */
        if( retval == E_NONE )
           {
            dirPtr->dirVal--;
            dirPtr->dirMdDat = macDate();
            macSetBitmapRec (&volume->catBitmap, dirNodeNum);
           }
       }

    return( retval );
   }

