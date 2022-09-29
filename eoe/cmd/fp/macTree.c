/* 
   (C) Copyright Digital Instrumentation Technology, Inc., 1990 - 1993 
       All Rights Reserved
*/

/*
     DIT TransferPro macTree.c - Generic B*-Tree data structure 
         management functions, i.e. that work with both Catalog and Extents
*/

#include <string.h>
#include <malloc.h>
#include "macSG.h"     /* BOOL */
#include "macPort.h"   /* MAX_TRANSFER */
#include "dm.h"

static char *Module = "macTree";
static unsigned int bitmapNodes(struct m_volume *, 
				struct bitmap_record *, unsigned int); 

/*-------------------------- macBuildTree ------------------------------
    Builds a B*-Tree from a buffer containing nodes of the tree in 
    Macintosh format.
------------------------------------------------------------------------*/

int macBuildTree ( volume, treeType, buffer )
    struct m_volume *volume;
    unsigned int treeType;
    char *buffer;
   {
    int  retval = E_NONE;
    struct BtreeNode **tree;
    char    *bufptr = buffer;
    unsigned int  numnodes, bmpnodes;
    int  i, count;
    int  volmtreesize;
    int  headtreesize;

    char *funcname = "MacBuildTree";

    if (treeType == CATALOG_TREE )
       {
        tree = volume->catalog;
        volmtreesize = volume->vib->drCTFlSize*sizeof(char);
       }
    else 
       {
        tree = volume->extents;
        volmtreesize = volume->vib->drXTFlSize*sizeof(char);
       }

    /* Unpack header node */
    if ( (tree[0] = (struct BtreeNode *)malloc (sizeof (struct BtreeNode)) ) == 
       (struct BtreeNode *)0)
       {
        retval = set_error( E_MEMORY, Module, funcname, "");
       }
    else
       {
        for( i = 0; i < NODE_SIZE; i++ )
           {
            tree[0]->BtNodes.nodeBytes[i] = 0;
           }

        if( (retval = macUnPackHeaderNode(&tree[0]->BtNodes.BtHeader, bufptr))
             == E_NONE )
            {
             bufptr += tree[0]->BtNodes.BtHeader.hnHdrRec.hrNodeSize;
             numnodes = tree[0]->BtNodes.BtHeader.hnHdrRec.hrTotalNodes - 
                 tree[0]->BtNodes.BtHeader.hnHdrRec.hrFreeNodes;
            }
       }

    if ( tree[0]->BtNodes.BtHeader.hnHdrRec.hrNodeSize <= 0 ||
        numnodes > volume->maxNodes  )
       {
        retval = set_error( E_RANGE, Module, funcname, 
            "Bad vib or %s tree header.", (treeType == CATALOG_TREE)?
            "catalog" : "extents" );
       }

    bmpnodes = bitmapNodes(volume, (struct bitmap_record *)
                   tree[0]->BtNodes.BtHeader.hnBitMap.bitmap,
                   tree[0]->BtNodes.BtHeader.hnHdrRec.hrTotalNodes);
    if ( bmpnodes != numnodes )
        return (E_BTREEBMPNODE);
 
    headtreesize = bmpnodes*tree[0]->BtNodes.BtHeader.hnHdrRec.hrNodeSize;
    if ( headtreesize > volmtreesize)
        return (E_BTREENODESZ);

    /* Loop through the buffer, processing each node */
    for( count = 1; retval == E_NONE && 
        count < tree[0]->BtNodes.BtHeader.hnHdrRec.hrTotalNodes; count++ )
       {
        /* if node does not appear in the bitmap */
        if ( !macInBitmapRec((struct bitmap_record *)
	    (tree[0]->BtNodes.BtHeader.hnBitMap.bitmap),count) )
           {
            /* zero this one out, it's not needed */
            tree[count] = 0;
           }
        else if ( (tree[count] = (struct BtreeNode *)malloc(
             sizeof(struct BtreeNode))) == (struct BtreeNode *)0 )
           {
            retval = set_error( E_MEMORY, Module, funcname, "malloc failed: %s", 
                "tree" );
           }
        else
           {
            for( i = 0; (unsigned short)i < tree[0]->BtNodes.BtHeader.hnHdrRec.hrNodeSize; i++ )
               {
                tree[count]->BtNodes.nodeBytes[i] = 0;
               }

            switch( (unsigned char) *(bufptr + NODE_TYPE_OFFSET) )
               {
                case INDEX_NODE: 
                   retval = macUnPackIndexNode(treeType,&tree[count]->BtNodes.BtIndex,
                       bufptr );
                    break;                    
                case BITMAP_NODE: 
                    macUnPackMapNode(&tree[count]->BtNodes.BtMap, 
                       bufptr );
                    break;                    
                case LEAF_NODE: 
                    retval = macUnPackLeafNode( treeType, 
                       &tree[count]->BtNodes.BtLeaf, bufptr );
                    break;                    
                default:
                    retval = set_error( E_MEDIUM, Module, funcname,
                        "bad node type at node %d: %x", count,
                        tree[count]->BtNodes.nodeBytes[NODE_TYPE_OFFSET] ); 
                    break;   
               }
           }
        bufptr += tree[0]->BtNodes.BtHeader.hnHdrRec.hrNodeSize;
       }

    return (retval);
}

/*-------------------------- searchTree ------------------------------
   Finds a file or directory record in a B*-Tree
------------------------------------------------------------------------*/
int searchTree(volume, targetKey, treeType, targetRecNum, nodeNum )
    struct m_volume    *volume;
    struct record_key  *targetKey;
    unsigned int        treeType;  
    int                *targetRecNum;
    unsigned int       *nodeNum;
   {
    int      retval = E_NONE,
             i = 0;
    struct BtreeNode   **tree;
    char     *funcname = "searchTree";
    unsigned int rootNum = 0;
    unsigned int treeNodes;

    if (treeType == CATALOG_TREE )
       {
        tree = volume->catalog;
        treeNodes = volume->vib->drCTFlSize/NODE_SIZE;
       }
    else 
       {
        tree = volume->extents;
        treeNodes = volume->vib->drXTFlSize/NODE_SIZE;
       }

    while( i < treeNodes &&
        (!tree[i] || tree[i]->BtNodes.BtHeader.hnDesc.ndType !=
        (unsigned char) HEADER_NODE) )
       {
        i++;
       }
    if( i == treeNodes )
       {
        retval = set_error(E_NOTFOUND, Module, funcname,
                          "Can't find header node in tree");
       }  
    else if( (rootNum = tree[i]->BtNodes.BtHeader.hnHdrRec.hrRootNode) == 0 )
       {
        retval = set_error(E_NOTFOUND, Module, funcname,"");
       }  

    if( retval == E_NONE )
       {
        retval=travTree(volume, targetKey, treeType, rootNum, targetRecNum, 
                        nodeNum);
       }

    return(retval);
   }

/*-------------------------- travTree ------------------------------
   recurses through B*-Tree looking for location of leaf rec
------------------------------------------------------------------------*/
int travTree(volume, targetKey, treeType, curNode, targetRec, nodeNum)
    struct m_volume    *volume;
    struct record_key  *targetKey;
    unsigned int        treeType;
    unsigned int        curNode;
    int                *targetRec;
    unsigned int       *nodeNum;
   {
    int retval = E_NONE;
    unsigned int nextNode;
    struct BtreeNode   **tree;
    char     *funcname = "travTree";

    if (treeType == CATALOG_TREE )
       {
        tree = volume->catalog;
       }
    else 
       {
        tree = volume->extents;
       }

    /* if arrived at appropriate leaf node, process records */

    if (  curNode > volume->maxNodes || !tree[curNode] )
       {
        retval = set_error( E_RANGE, Module, funcname, "Node not found" );
       }
    else if( tree[curNode]->BtNodes.BtLeaf.lnDesc.ndType == LEAF_NODE )
       {
        retval = scanLeafRecs(targetKey, &tree[curNode]->BtNodes.BtLeaf,
                              targetRec, treeType);
        *nodeNum = curNode;
       }
    else 
       {
        retval = scanPtrRecs(targetKey, &tree[curNode]->BtNodes.BtIndex, 
               &nextNode, targetRec, treeType);

        /* if not an exact hit, decrement numbers for next travTree call */

        if( retval == E_NOTFOUND ) 
           {
            if ( *targetRec > 0 )
               {
                (*targetRec)--;
               }
            nextNode = tree[curNode]->BtNodes.BtIndex.
                    inRecList[*targetRec]->ptrPointer;
            retval = clear_error();
           }

        if ( retval == E_NONE )
           {
            retval = travTree(volume, targetKey, treeType, nextNode, targetRec,
                nodeNum);
           }
       }
    return(retval);
   }

/*--- macDelNodeFromTree ----------------------------------
 */
int macDelNodeFromTree( volume, nodeNum, tree, treeType )
    struct m_volume *volume;
    unsigned int nodeNum;
    struct BtreeNode **tree;
    int treeType;
   {
    int        retval = E_NONE;
    char      *funcname = "macDelNodeFromTree";
    int       fNode = -1;
    int       bNode = -1;

    if( nodeNum == 0 )
       {
        retval = set_error(E_PERMISSION, Module, funcname,
                  "Can't delete node zero");
       }
    else
       {
        /* if nodeNum'th node was last in a linked list */
        if( tree[nodeNum]->BtNodes.BtLeaf.lnDesc.ndFLink == UNASSIGNED_NODE &&
            tree[nodeNum]->BtNodes.BtLeaf.lnDesc.ndBLink != UNASSIGNED_NODE )
           {
            tree[0]->BtNodes.BtHeader.hnHdrRec.hrLastLeaf = 
                tree[nodeNum]->BtNodes.BtLeaf.lnDesc.ndBLink;
            bNode = tree[nodeNum]->BtNodes.BtLeaf.lnDesc.ndBLink;
            tree[bNode]->BtNodes.BtLeaf.lnDesc.ndFLink = UNASSIGNED_NODE;
           }

        /* if nodeNum'th node was first in a linked list */
        else if( tree[nodeNum]->BtNodes.BtLeaf.lnDesc.ndFLink != UNASSIGNED_NODE &&
            tree[nodeNum]->BtNodes.BtLeaf.lnDesc.ndBLink == UNASSIGNED_NODE )
           {
            tree[0]->BtNodes.BtHeader.hnHdrRec.hrFirstLeaf = 
                tree[nodeNum]->BtNodes.BtLeaf.lnDesc.ndFLink;
            fNode = tree[nodeNum]->BtNodes.BtLeaf.lnDesc.ndFLink;
            tree[fNode]->BtNodes.BtLeaf.lnDesc.ndBLink = UNASSIGNED_NODE;
           }
        /* if nodeNum'th node in middle of a linked list */
        else if( tree[nodeNum]->BtNodes.BtLeaf.lnDesc.ndBLink != UNASSIGNED_NODE &&
            tree[nodeNum]->BtNodes.BtLeaf.lnDesc.ndFLink != UNASSIGNED_NODE )
           {
            bNode = tree[nodeNum]->BtNodes.BtLeaf.lnDesc.ndBLink;
            fNode = tree[nodeNum]->BtNodes.BtLeaf.lnDesc.ndFLink;
            tree[fNode]->BtNodes.BtLeaf.lnDesc.ndBLink = 
                                 tree[nodeNum]->BtNodes.BtLeaf.lnDesc.ndBLink;
            tree[bNode]->BtNodes.BtLeaf.lnDesc.ndFLink = 
                                 tree[nodeNum]->BtNodes.BtLeaf.lnDesc.ndFLink;
           }


        free( (char *) tree[nodeNum] );
        tree[nodeNum] = 0;

        /* update catalog header info */
        tree[0]->BtNodes.BtHeader.hnHdrRec.hrFreeNodes++;
        

        macClearBitmapRec((struct bitmap_record *)
	   (tree[0]->BtNodes.BtHeader.hnBitMap.bitmap), nodeNum);

        if ( treeType == CATALOG_TREE )
           {
            macSetBitmapRec ( &volume->catBitmap, 0 );
            macSetBitmapRec ( &volume->catBitmap, nodeNum );
            if ( bNode >= 0 )
               {
                macSetBitmapRec ( &volume->catBitmap, bNode );
               }
            if ( fNode >= 0 )
               {
                macSetBitmapRec ( &volume->catBitmap, fNode );
               }
           }
        else 
           {
            macSetBitmapRec ( &volume->extBitmap, 0 );
            macSetBitmapRec ( &volume->extBitmap, nodeNum );
            if ( bNode >= 0 )
               {
                macSetBitmapRec ( &volume->extBitmap, bNode );
               }
            if ( fNode >= 0 )
               {
                macSetBitmapRec ( &volume->extBitmap, fNode );
               }
           }
       }

    return(retval);
   }

 
/*--------------------- macDelRecFromTree ------------------------------
    Delete a record from recNum'th position in nodeNum'th node of tree
    assumes that all necessary checks have been made.
  ---------------------------------------------------------------------*/

int macDelRecFromTree( volume, nodeNum, recNum, treeType )
    struct m_volume  *volume;
    unsigned int      nodeNum;
    int               recNum;
    int               treeType;
   {
    int retval = E_NONE;
    struct BtreeNode **tree;
    char *funcname = "macDelRecFromTree";
    int j;
    unsigned int parNode = UNASSIGNED_NODE; 
    int parRec = UNASSIGNED_REC;

    if ( treeType == CATALOG_TREE )
       {
        tree = volume->catalog;
       }
    else
       {
        tree = volume->extents;
       }

    if( tree[nodeNum]->BtNodes.BtLeaf.lnDesc.ndType == LEAF_NODE &&
       ((struct thread_record *)tree[nodeNum]->BtNodes.BtLeaf.lnRecList[recNum])
        ->thdType != THREAD_REC && treeType == CATALOG_TREE )
       {
        retval = macDelRecFromDir( volume, recNum, nodeNum );
       }

    if ( retval == E_NONE )
       {
        /* look for the parent before we free things up */
        if ( recNum == 0 &&
            (retval = macFindParent(nodeNum,tree,&parNode,&parRec,
            treeType)) != E_NONE )
           {
            ;
           }
        else if( tree[nodeNum]->BtNodes.BtLeaf.lnDesc.ndType ==  INDEX_NODE )
           {
            free ( tree[nodeNum]->BtNodes.BtIndex.inRecList[recNum] );
            tree[nodeNum]->BtNodes.BtIndex.inRecList[recNum] = 0;
           }
        else if(tree[nodeNum]->BtNodes.BtLeaf.lnDesc.ndType == LEAF_NODE)
           {
            free ( tree[nodeNum]->BtNodes.BtLeaf.lnRecList[recNum] );
            tree[nodeNum]->BtNodes.BtLeaf.lnRecList[recNum] = 0;
           }
       }
    
    /* riffle existing records toward top of list  */
    for( j = recNum; 
         j < (int)(tree[nodeNum]->BtNodes.BtLeaf.lnDesc.ndRecs-1)
         && retval == E_NONE; j++ )
       {
        if( tree[nodeNum]->BtNodes.BtLeaf.lnDesc.ndType ==  INDEX_NODE )
           {
            tree[nodeNum]->BtNodes.BtIndex.inRecList[j] =
               tree[nodeNum]->BtNodes.BtIndex.inRecList[j+1]; 
           }
        else if(tree[nodeNum]->BtNodes.BtLeaf.lnDesc.ndType == LEAF_NODE)
           {
            tree[nodeNum]->BtNodes.BtLeaf.lnRecList[j] =
               tree[nodeNum]->BtNodes.BtLeaf.lnRecList[j+1]; 
           }
        else
           {
            retval = set_error(E_RANGE, Module, funcname, 
                     "Unrecognized node type in tree.");
           }
       }

    if( retval == E_NONE )
       {

        if (tree[nodeNum]->BtNodes.BtLeaf.lnDesc.ndType == LEAF_NODE )
           {
            tree[0]->BtNodes.BtHeader.hnHdrRec.hrTotalRecs--;
           }
        (tree[nodeNum]->BtNodes.BtLeaf.lnDesc.ndRecs) -= 1;

        if ( treeType == CATALOG_TREE )
           {
            macSetBitmapRec ( &volume->catBitmap, 0 );
            macSetBitmapRec ( &volume->catBitmap, nodeNum );
           }
        else 
           {
            macSetBitmapRec ( &volume->extBitmap, 0 );
            macSetBitmapRec ( &volume->extBitmap, nodeNum );
           }

        /* delete root node if now empty -- only with extents tree! */
        if( nodeNum == tree[0]->BtNodes.BtHeader.hnHdrRec.hrRootNode )
           {
            if( tree[nodeNum]->BtNodes.BtLeaf.lnDesc.ndRecs == 0 && 
                tree[0]->BtNodes.BtHeader.hnHdrRec.hrTotalRecs == 0 &&
                (retval = macDelNodeFromTree(volume, nodeNum, tree, treeType)) 
                          == E_NONE )
               {
                tree[0]->BtNodes.BtHeader.hnHdrRec.hrDepthCount = 0;
                tree[0]->BtNodes.BtHeader.hnHdrRec.hrRootNode = 0;
                tree[0]->BtNodes.BtHeader.hnHdrRec.hrFirstLeaf = 0;
                tree[0]->BtNodes.BtHeader.hnHdrRec.hrLastLeaf = 0;
               }
           }
        /* adjust parents of child nodes if deletion repercusses above */
        else if( recNum == 0 )
           {
            /* if records still remain in this node, update parent index node */
            if(retval == E_NONE &&
                tree[nodeNum]->BtNodes.BtLeaf.lnDesc.ndRecs > 0 )
               {
                retval = macCorrectParents( volume, nodeNum, 
                                recNum, parNode, parRec, treeType );
               }
            /* if no records remain, remove node from tree */
            else if( retval == E_NONE &&
                   (retval = macDelRecFromTree(volume, parNode, parRec, 
                                                 treeType)) == E_NONE )
               {
                retval = macDelNodeFromTree( volume, nodeNum, tree, treeType );
               }
            if( retval == E_NONE )
               {           
                if( tree[tree[0]->BtNodes.BtHeader.hnHdrRec.hrRootNode]->
                      BtNodes.BtIndex.inDesc.ndType == INDEX_NODE &&
                    tree[tree[0]->BtNodes.BtHeader.hnHdrRec.hrRootNode]->
                      BtNodes.BtIndex.inDesc.ndRecs ==  1 )
                   {
                    retval = macDelOldRootFromTree( volume, tree, treeType );
                   }
                }
            }
        }

    return( retval );
   }

/*--- macCorrectParents ----------------------------------
 * propagates change in key record to root of tree if necessary
 */

int macCorrectParents( volume, childNodeNum, childRecNum, parNodeNum, parRecNum,
    treeType )
    struct m_volume *volume;
    unsigned int  childNodeNum; 
    int           childRecNum;
    unsigned int  parNodeNum; 
    int           parRecNum;
    int           treeType;
   {
    int retval = E_NONE;
    struct BtreeNode **tree;
    unsigned int newParNode = 0;
    int  newParRec = 0;

    if ( treeType == CATALOG_TREE )
       {
        tree = volume->catalog;
       }
    else
       {
        tree = volume->extents;
       }
    if( tree[childNodeNum]->BtNodes.BtIndex.inDesc.ndType ==  INDEX_NODE )
        {
         /* update parent pointer record of a child pointer record */   
         memcpy( (char *)tree[parNodeNum]->BtNodes.BtIndex.
            inRecList[parRecNum], (char *)tree[childNodeNum]->BtNodes.BtIndex.
            inRecList[childRecNum], sizeof(struct record_key) );
         }
     else    /* if LEAF_NODE */
         {
          memcpy( (char *)tree[parNodeNum]->BtNodes.BtIndex.
              inRecList[parRecNum], (char *)tree[childNodeNum]->BtNodes.BtLeaf.
              lnRecList[childRecNum],  sizeof(struct record_key) );
          tree[parNodeNum]->BtNodes.BtIndex.inRecList[parRecNum]->ptrRecKey.
             key.ckr.ckrKeyLen = (treeType == EXTENTS_TREE)?
                EXTENT_KEYLENGTH : POINTER_REC_KEYLENGTH;
         }

     if( parRecNum == 0 && 
         parNodeNum != tree[0]->BtNodes.BtHeader.hnHdrRec.hrRootNode )
        {
         /* find parent node and record position */   
         retval = macFindParent(parNodeNum, tree, &newParNode, &newParRec, 
                                treeType);
  
         if( retval == E_NONE )
            {
             retval = macCorrectParents( volume, parNodeNum, parRecNum, 
                newParNode, newParRec, treeType );
            }
        }
    if ( retval == E_NONE )
       {
        if ( treeType == CATALOG_TREE )
           {
            macSetBitmapRec ( &volume->catBitmap, parNodeNum );
           }
        else 
           {
            macSetBitmapRec ( &volume->extBitmap, parNodeNum );
           }
       }

    return(retval);
   }
 

/*--- macAddRecToTree ----------------------------------
 * adds rec to currNode
 */

int macAddRecToTree( volume, currNode, rec, currRecNum, treeType )
    struct m_volume *volume;
    int          *currNode;
    void         *rec;
    int          *currRecNum;
    int           treeType;
   {
    int        retval = E_NONE,
               scanResult = 0,
               recLocus = 0;
    struct BtreeNode **tree;
    char      *funcname = "macAddRecToTree";
    unsigned int newNode = UNASSIGNED_NODE;
    void *newRec;
    unsigned int parNode;
    int parRec;

    if ( treeType == CATALOG_TREE )
       {
        tree = volume->catalog;
       }
    else 
       {
        tree = volume->extents;
       }

    /* following calls are to get a value in recLocus */
    if( tree[*currNode]->BtNodes.BtIndex.inDesc.ndType ==  INDEX_NODE )
       {
        retval = scanPtrRecs((struct record_key *)rec, (struct index_node *) 
            tree[*currNode], (unsigned int *)&scanResult, &recLocus, treeType);

        if( retval == E_NOTFOUND )
           {
            retval = clear_error();
           }

        if( scanResult >= tree[0]->BtNodes.BtHeader.hnHdrRec.hrTotalNodes || 
            (unsigned short)recLocus >  tree[*currNode]->BtNodes.BtIndex.inDesc.ndRecs ||
            recLocus < 0 )
            {
             retval = set_error(E_RANGE, Module, funcname, 
            "Can't add rec to tree: vib->drCTFlSize = %d and ndRecs =%d\n",
             volume->vib->drCTFlSize, tree[*currNode]->BtNodes.BtIndex.inDesc.ndRecs);
            }
        }
    else if( tree[*currNode]->BtNodes.BtLeaf.lnDesc.ndType ==  LEAF_NODE )
       {
        retval = scanLeafRecs( &((struct file_record *)rec)->filRecKey,
             (struct leaf_node *)tree[*currNode], &recLocus, treeType);

        if( (unsigned short)recLocus >  tree[*currNode]->BtNodes.BtLeaf.lnDesc.ndRecs ||
            recLocus < 0 )
            {
             retval = set_error(E_RANGE,Module,funcname,
                      "Can't add record to node.");
            }
    
        if( retval == E_NOTFOUND )
           {
            retval = clear_error();
           }
       }

    if( tree[*currNode]->BtNodes.BtIndex.inDesc.ndType ==  INDEX_NODE )
       {
        if ( (newRec = (struct pointer_record *) malloc
                       (sizeof (struct pointer_record)) ) 
                       == (struct pointer_record *)0 )
           {
            retval = set_error(E_MEMORY, Module, funcname, 
                               "Insufficent memory");
           }
        else
           {
            memcpy ( (char *)newRec, (char *)rec, 
                     sizeof( struct pointer_record));
           }
       }
    else if( tree[*currNode]->BtNodes.BtLeaf.lnDesc.ndType ==  LEAF_NODE )
       {
        if ( treeType == CATALOG_TREE )
           {
            switch ( *((char *)rec + sizeof(struct record_key)) )
               {
                case FILE_REC:
                    if ( (newRec = (struct file_record *) malloc
                           (sizeof (struct file_record)) ) 
                           == (struct file_record *)0 )
                       {
                        retval = set_error(E_MEMORY, Module, funcname, 
                                           "Insufficent memory");
                       }
                    else
                       {
                        memcpy ( (char *)newRec, (char *)rec, 
                                 sizeof( struct file_record));
                       }
                   break;
    
                case DIRECTORY_REC:
                    if ( (newRec = (struct dir_record *) malloc
                           (sizeof (struct dir_record)) ) 
                           == (struct dir_record *)0 )
                       {
                        retval = set_error(E_MEMORY, Module, funcname, 
                                           "Insufficent memory");
                       }
                    else
                       {
                        memcpy ( (char *)newRec, (char *)rec, 
                                 sizeof( struct dir_record));
                       }
                   break;
      
                case THREAD_REC:
                    if ( (newRec = (struct thread_record *) malloc
                           (sizeof (struct thread_record)) ) 
                           == (struct thread_record *)0 )
                       {
                        retval = set_error(E_MEMORY, Module, funcname, 
                                           "Insufficent memory");
                       }
                    else
                       {
                        memcpy ( (char *)newRec, (char *)rec, 
                                 sizeof( struct thread_record));
                       }
                   break;
    
                case ALIAS_REC:
                    if ( (newRec = (struct alias_record *) malloc
                           (sizeof (struct alias_record)) ) 
                           == (struct alias_record *)0 )
                       {
                        retval = set_error(E_MEMORY, Module, funcname, 
                                           "Insufficent memory");
                       }
                    else
                       {
                        memcpy ( (char *)newRec, (char *)rec, 
                                 sizeof( struct alias_record));
                       }
                   break;
      
                default:
                   retval = set_error (E_RANGE, Module, funcname, 
                                      "Unrecogized record type.");
                   break;
               }
           }
        else
           {
            if ( (newRec = (struct extent_record *) malloc
                   (sizeof (struct extent_record)) ) 
                   == (struct extent_record *)0 )
               {
                retval = set_error(E_MEMORY, Module, funcname, 
                                   "Insufficent memory");
               }
            else
               {
                memcpy ( (char *)newRec, (char *)rec, 
                             sizeof( struct extent_record));
               }
           }
       }

    if ( retval == E_NONE && recLocus == 0 &&
        tree[0]->BtNodes.BtHeader.hnHdrRec.hrDepthCount > 1 &&
        *currNode != tree[0]->BtNodes.BtHeader.hnHdrRec.hrRootNode )
       {
        retval = macFindParent( *currNode, tree, &parNode, &parRec, treeType );
       }

    if( retval == E_NONE )
       {
        retval = macInsertRec(volume, newRec, *currNode, recLocus,  
                              &newNode, treeType );
       }
 
    /* only update index nodes if new record replaced first record */
    if( retval == E_NONE && recLocus == 0 )
       {
        retval = macCorrectPtrRec( volume, tree, *currNode, parNode, parRec,
            treeType );
       }

    /* if new node was allocated, add pointer record for it */
    if( retval == E_NONE && newNode != UNASSIGNED_NODE )
       {
        retval = macAddNodeToTree( volume, *currNode, newNode, treeType );
       }

    if( retval == E_NONE )
       {
        *currRecNum = recLocus;
        if( tree[*currNode]->BtNodes.BtLeaf.lnDesc.ndType ==  LEAF_NODE )
           {
            tree[0]->BtNodes.BtHeader.hnHdrRec.hrTotalRecs++;
           }

        /* tell calling function where new record is */
        if ( newNode != UNASSIGNED_NODE &&
             newRec == tree[newNode]->BtNodes.BtLeaf.lnRecList[0] )
           {
            *currNode = newNode;
            *currRecNum = 0;
           }
        else if ( newNode != UNASSIGNED_NODE &&
             newRec == tree[newNode]->BtNodes.BtLeaf.lnRecList[1] )
           {
            *currNode = newNode;
            *currRecNum = 1;
           }
        if ( treeType == CATALOG_TREE )
           {
            macSetBitmapRec ( &volume->catBitmap, 0 );
           }
        else 
           {
            macSetBitmapRec ( &volume->extBitmap, 0 );
           }
       }
         
    return(retval);
   }

    
/*--------------- macInsertRec -------------------------------------------
 *
 * Currently this function always creates a new node if there is not room
 * in the current one. In order to conserve space in the tree, it should really
 * look for a forward link first, and try to add it there. This could
 * probably be done with a recursive call to macInsertRec() in the code
 * below. The trick is that when you pass the rec on to the next node, you
 * are forcing an update on the index record above it (if there is one),
 * since the inserted record will always be the first record in the forward
 * link. This modification could conceivably introduce some difficult-to-
 * discover bugs, and should be undertaken with extreme caution and a good
 * backup, as well as lots of testing. 
 */ 

int macInsertRec( volume, rec, currNode, recNum, newNodeNum, treeType )
    struct m_volume *volume;
    void         *rec;
    unsigned int  currNode;
    int           recNum;
    unsigned int *newNodeNum;
    unsigned int  treeType;
   {
    int        retval = E_NONE,
               counter = 0,
               currNodeRecs = 0,
               j;
    struct BtreeNode **tree;
    char      *funcname="macInsertRec";


    if (treeType == CATALOG_TREE )
       {
        tree = volume->catalog;
       }
    else
       {
        tree = volume->extents;
       }

    /* move existing records down to make room for new one */

    for( j = tree[currNode]->BtNodes.BtLeaf.lnDesc.ndRecs; 
         j > recNum && retval == E_NONE; j-- )
       {
        if( tree[currNode]->BtNodes.BtLeaf.lnDesc.ndType ==  INDEX_NODE )
           {
            tree[currNode]->BtNodes.BtIndex.inRecList[j] =                      
                tree[currNode]->BtNodes.BtIndex.inRecList[j-1]; 
           }
        else if(tree[currNode]->BtNodes.BtLeaf.lnDesc.ndType == LEAF_NODE)
           {
            tree[currNode]->BtNodes.BtLeaf.lnRecList[j] = 
                tree[currNode]->BtNodes.BtLeaf.lnRecList[j-1]; 
           }
        else
           {
            retval = set_error(E_RANGE, Module, funcname, 
                      "Unrecognized node type in tree.");
           }
       }
    /* add the new record */
    if( retval == E_NONE )
       {
         if( tree[currNode]->BtNodes.BtIndex.inDesc.ndType == INDEX_NODE )
           {
            tree[currNode]->BtNodes.BtIndex.inRecList[j] = 
                                             (struct pointer_record *)rec; 
            tree[currNode]->BtNodes.BtIndex.inDesc.ndRecs++;
           }
        else if(tree[currNode]->BtNodes.BtLeaf.lnDesc.ndType == LEAF_NODE)
           {
            tree[currNode]->BtNodes.BtLeaf.lnRecList[j] = rec;
            tree[currNode]->BtNodes.BtLeaf.lnDesc.ndRecs++;
           }
        else
           {
            retval = set_error(E_RANGE, Module, funcname, 
                               "Unrecognized node type in tree.");
           }
        if ( treeType == CATALOG_TREE )
           {
            macSetBitmapRec ( &volume->catBitmap, 0 );
            macSetBitmapRec ( &volume->catBitmap, currNode );
           }
        else 
           {
            macSetBitmapRec ( &volume->extBitmap, 0 );
            macSetBitmapRec ( &volume->extBitmap, currNode );
           }
       }
    
    /* calculate number of records desired in this node */
    if( retval == E_NONE )
       {
        currNodeRecs = macCountNdRecs((struct leaf_node *)tree[currNode],
								treeType);
       }

    /* add overflow record(s) to new node */
    if(  retval ==  E_NONE && 
        (unsigned short)currNodeRecs <
        (tree[currNode]->BtNodes.BtLeaf.lnDesc.ndRecs) )
       {
        if( (retval = macGetNextNode( volume, currNode, newNodeNum, treeType ))
                      != E_NONE )
           {
            ;   /* call for new node failed */
           }
        else if( tree[currNode]->BtNodes.BtLeaf.lnDesc.ndType ==  LEAF_NODE )
           {
            for( j = 0, counter = currNodeRecs; 
                 counter <
                 (int)((tree[currNode]->BtNodes.BtLeaf.lnDesc.ndRecs)+1);
                 counter++, j++ )
               {
                tree[*newNodeNum]->BtNodes.BtLeaf.lnRecList[j] =  
                   tree[currNode]->BtNodes.BtLeaf.lnRecList[counter];
                tree[currNode]->BtNodes.BtLeaf.lnRecList[counter] = 0;
                tree[*newNodeNum]->BtNodes.BtLeaf.lnDesc.ndRecs++;
                tree[currNode]->BtNodes.BtLeaf.lnDesc.ndRecs--;
               }
           }    
        else if(tree[currNode]->BtNodes.BtIndex.inDesc.ndType
            == INDEX_NODE )
           {
            for( j = 0, counter = currNodeRecs; 
                 counter <
                 (int)((tree[currNode]->BtNodes.BtLeaf.lnDesc.ndRecs)+1);
                 counter++, j++ )
               {
                tree[*newNodeNum]->BtNodes.BtIndex.inRecList[j] = 
                   tree[currNode]->BtNodes.BtIndex.inRecList[counter];
                tree[currNode]->BtNodes.BtIndex.inRecList[counter] = 0;
                tree[*newNodeNum]->BtNodes.BtIndex.inDesc.ndRecs++;
                tree[currNode]->BtNodes.BtIndex.inDesc.ndRecs--;
               }
           }
        if ( retval == E_NONE )
           {
            if ( treeType == CATALOG_TREE )
               {
                macSetBitmapRec ( &volume->catBitmap, *newNodeNum );
               }
            else 
               {
                macSetBitmapRec ( &volume->extBitmap, *newNodeNum );
               }

            if ( (unsigned short)recNum >= tree[currNode]->BtNodes.BtIndex.inDesc.ndRecs )
               {
                recNum = recNum -
                    tree[currNode]->BtNodes.BtIndex.inDesc.ndRecs;
                currNode = *newNodeNum;
               }
           }    
       }

    /* update altered node */
    if( retval ==  E_NONE && treeType == CATALOG_TREE )
       {
        if( tree[currNode]->BtNodes.BtLeaf.lnDesc.ndType == LEAF_NODE &&
            ((struct thread_record *)tree[currNode]->BtNodes.BtLeaf.
            lnRecList[recNum])->thdType != THREAD_REC )
           {
            retval = macAddRecToDir( volume, recNum, currNode );
           }
       }

    return(retval);
   }
    
/*--------------- macCorrectPtrRec -------------------------------------------
 *   updates index nodes if necessary.
 */ 

int macCorrectPtrRec( volume, tree, currNode, parNode, parRec, treeType )
    struct m_volume *volume;
    struct BtreeNode **tree;
    unsigned int  currNode;
    unsigned int parNode;
    int parRec;
    unsigned int  treeType;
   {
    int        retval = E_NONE;
    char      *funcname="macCorrectPtrRec";

    /* find parent index node */
    if( currNode == tree[0]->BtNodes.BtHeader.hnHdrRec.hrRootNode )
       {
        ;
       }
    else
       {
        if( retval == E_NONE )
           {
            /* check values returned */
            if( (unsigned short)parRec > 
                tree[parNode]->BtNodes.BtIndex.inDesc.ndRecs ||
                tree[parNode]->BtNodes.BtIndex.inRecList[parRec]->ptrPointer 
                != currNode)
               {
                retval = set_error(E_NOTFOUND,Module,funcname,
                   "Can't find parent node.");
               }
        
            /* update pointer record data  */
            if( retval == E_NONE )
               {        
                memcpy( (char *)tree[parNode]->BtNodes.BtIndex.
                   inRecList[parRec], (char *)tree[currNode]->BtNodes.
                   BtLeaf.lnRecList[0], sizeof(struct record_key) );
        
               /* reset correct pointer ckrKeyLen  */
               if ( treeType == CATALOG_TREE )
                  {
                   tree[parNode]->BtNodes.BtIndex.inRecList[parRec]->ptrRecKey.
                       key.ckr.ckrKeyLen = POINTER_REC_KEYLENGTH;
                   macSetBitmapRec (&volume->catBitmap, parNode);
                  }
               else
                  {
                   tree[parNode]->BtNodes.BtIndex.inRecList[parRec]->ptrRecKey.
                       key.ckr.ckrKeyLen = EXTENT_KEYLENGTH;
                   macSetBitmapRec (&volume->extBitmap, parNode);
                  }
              }
          }
       }

    return(retval);
   }

/*--------------- macAddNodeToTree -------------------------------------------
 *   
 * add new node to the tree.
 *
 */ 

int macAddNodeToTree(volume, currNode, newNode, treeType )
    struct m_volume *volume;
    unsigned int currNode;
    unsigned int newNode;
    unsigned int treeType;
   {
    int        retval = E_NONE,
               parRec = 0;
    unsigned int  parNode = 0;
    struct pointer_record newPtrRec;
    struct BtreeNode **tree;


    if ( treeType == CATALOG_TREE )
       {
        tree = volume->catalog;
       }
    else
       {
        tree = volume->extents;
       }
    /* special case: add level to tree  */
    if( currNode == tree[0]->BtNodes.BtHeader.hnHdrRec.hrRootNode )
       {
        retval = macAddNewRootToTree( volume, currNode, newNode, treeType ); 
       }
    else
       {
        /* find parent index node for currNode */
        retval = macFindParent( currNode, tree, &parNode, &parRec, treeType );
                
        /* slot for new pointer record will immediately follow that for 
           currNode */
        if( retval == E_NONE )
           { 
            memcpy( (char *)&newPtrRec.ptrRecKey.key.ckr, 
                (char *)tree[newNode]->BtNodes.BtLeaf.lnRecList[0],
                sizeof(struct record_key) );
                
            /* reset correct pointer ckrKeyLen  */
            if ( treeType == EXTENTS_TREE )
               {
                newPtrRec.ptrRecKey.key.ckr.ckrKeyLen = EXTENT_KEYLENGTH;
               }
            else
               {
                newPtrRec.ptrRecKey.key.ckr.ckrKeyLen = POINTER_REC_KEYLENGTH;
               }
            newPtrRec.ptrPointer = newNode;
                
            /* add the new pointer record to parent node */
            retval = macAddRecToTree( volume,(int *) &parNode,
	               (void *)&newPtrRec, &parRec, treeType );
                
            /* current strategy adds only successor leaf nodes */
             if( retval == E_NONE && 
                 tree[newNode]->BtNodes.BtLeaf.lnDesc.ndType == LEAF_NODE
                 &&  tree[newNode]->BtNodes.BtLeaf.lnDesc.ndFLink == 0 )
                {        
                 tree[0]->BtNodes.BtHeader.hnHdrRec.hrLastLeaf = newNode;
                }
           }
       }
                
    return(retval);
   }


/*--------------- macAddNewRootToTree -------------------------------------------
 *  creates a new root node for tree
 */ 

int macAddNewRootToTree( volume, child1, child2, treeType )
    struct m_volume *volume;
    unsigned int       child1;
    unsigned int      child2;
    unsigned int      treeType;
   {
    int        retval = E_NONE;
    unsigned int newRootNode;
    struct BtreeNode **tree;
    char *funcname = "macAddNewRootToTree";

    if ( treeType == CATALOG_TREE)
       {
        tree = volume->catalog;
       }
    else
       {
        tree = volume->extents;
       }
    if ( (retval = macGetNextNode(volume,UNASSIGNED_NODE,&newRootNode,
                                  treeType)) == E_NONE )
       {
        tree[newRootNode]->BtNodes.BtIndex.inDesc.ndFLink = UNASSIGNED_NODE;
        tree[newRootNode]->BtNodes.BtIndex.inDesc.ndBLink = UNASSIGNED_NODE;
        tree[newRootNode]->BtNodes.BtIndex.inDesc.ndType = INDEX_NODE;
        tree[newRootNode]->BtNodes.BtIndex.inDesc.ndLevel = 
           tree[0]->BtNodes.BtHeader.hnHdrRec.hrDepthCount+1;
        tree[newRootNode]->BtNodes.BtIndex.inDesc.ndRecs = (short)2;
        tree[newRootNode]->BtNodes.BtIndex.inDesc.ndReserved = 0;

        if ( (tree[newRootNode]->BtNodes.BtIndex.inRecList[0] =
             (struct pointer_record *)malloc(sizeof (struct pointer_record)) )
             == (struct pointer_record *)0)
           {
            retval = set_error (E_MEMORY, Module, funcname,
                                "Insufficient memory.");
           }
        else if  ( (tree[newRootNode]->BtNodes.BtIndex.inRecList[1] =
             (struct pointer_record *)malloc(sizeof (struct pointer_record)) )
             == (struct pointer_record *)0)
           {
            free (tree[newRootNode]->BtNodes.BtIndex.inRecList[0]);
            tree[newRootNode]->BtNodes.BtIndex.inRecList[0] = 0;
            retval = set_error (E_MEMORY, Module, funcname,
                                "Insufficient memory.");
           }

        if ( retval == E_NONE )
           {
            /* child1 */
            memcpy( (char *)tree[newRootNode]-> BtNodes.BtIndex.inRecList[0],
                (char *)tree[child1]-> BtNodes.BtLeaf.lnRecList[0],
                sizeof(struct record_key) );
            tree[newRootNode]->BtNodes.BtIndex.inRecList[0]->
                ptrRecKey.key.ckr.ckrKeyLen = (treeType == EXTENTS_TREE)?
                EXTENT_KEYLENGTH : POINTER_REC_KEYLENGTH;
            tree[newRootNode]->BtNodes.BtIndex.inRecList[0]->ptrPointer = 
                child1;
    
            /* child2 */
            memcpy( (char *)tree[newRootNode]->BtNodes.BtIndex.inRecList[1],
                (char *)tree[child2]->BtNodes.BtLeaf.lnRecList[0],
                sizeof(struct record_key) );
            tree[newRootNode]->BtNodes.BtIndex.  inRecList[1]->
                ptrRecKey.key.ckr.ckrKeyLen = (treeType == EXTENTS_TREE)?
                EXTENT_KEYLENGTH : POINTER_REC_KEYLENGTH;
            tree[newRootNode]->BtNodes.BtIndex.inRecList[1]->ptrPointer = 
                child2;
            tree[0]->BtNodes.BtHeader.hnHdrRec.hrRootNode = newRootNode;
            tree[0]->BtNodes.BtHeader.hnHdrRec.hrDepthCount++;
           }

        if ( retval == E_NONE )
           {
            if ( treeType == CATALOG_TREE )
               {
                macSetBitmapRec (&volume->catBitmap, 0);
                macSetBitmapRec (&volume->catBitmap, newRootNode);
               }
            else
               {
                macSetBitmapRec (&volume->extBitmap, 0);
                macSetBitmapRec (&volume->extBitmap, newRootNode);
               }
           }
       }

    return(retval);
   }

/*--------------- macDelOldRootFromTree --------------------------------------
 *  removes old root node for tree
 *  n.b. this function called only when old root node has only one record 
 */ 

int macDelOldRootFromTree( volume, tree, treeType )
    struct m_volume *volume;
    struct BtreeNode **tree;
    int treeType;
   {
    int        retval = E_NONE;
    unsigned int oldRootNode = tree[0]->BtNodes.BtHeader.hnHdrRec.hrRootNode;
    unsigned int childNode = 
                 tree[oldRootNode]->BtNodes.BtIndex.inRecList[0]->ptrPointer;
    char      *funcname = "macDelOldRootFromTree";


    /* child node cannot be part of a horizontal linked list */
    if( tree[childNode]->BtNodes.BtIndex.inDesc.ndFLink != UNASSIGNED_NODE ||
       tree[childNode]->BtNodes.BtIndex.inDesc.ndBLink != UNASSIGNED_NODE )
       {
        retval = set_error(E_DATA, Module, funcname, "Bad data in tree");
       }
    else 
       {
        free (tree[oldRootNode]->BtNodes.BtIndex.inRecList[0]);
        tree[oldRootNode]->BtNodes.BtIndex.inRecList[0] = 0;
        memcpy( (char *)tree[oldRootNode], (char *)tree[childNode],
            sizeof(struct leaf_node) );
        tree[0]->BtNodes.BtHeader.hnHdrRec.hrDepthCount -= 1;


        tree[0]->BtNodes.BtHeader.hnHdrRec.hrFreeNodes++;
        if ( tree[0]->BtNodes.BtHeader.hnHdrRec.hrFirstLeaf == childNode )
           {
            tree[0]->BtNodes.BtHeader.hnHdrRec.hrFirstLeaf = oldRootNode;
           }
        if ( tree[0]->BtNodes.BtHeader.hnHdrRec.hrLastLeaf == childNode )
           {
            tree[0]->BtNodes.BtHeader.hnHdrRec.hrLastLeaf = oldRootNode;
           }

        macClearBitmapRec((struct bitmap_record *)
          (tree[0]->BtNodes.BtHeader.hnBitMap.bitmap), childNode);
        free( (char *)tree[childNode] );
        tree[childNode] = (struct BtreeNode *)0;
       }
    if ( retval == E_NONE )
       {
        if ( treeType == CATALOG_TREE )
           {
            macSetBitmapRec (&volume->catBitmap, 0);
            macSetBitmapRec (&volume->catBitmap, oldRootNode);
            macSetBitmapRec (&volume->catBitmap, childNode);
           }
        else
           {
            macSetBitmapRec (&volume->extBitmap, 0);
            macSetBitmapRec (&volume->extBitmap, oldRootNode);
            macSetBitmapRec (&volume->extBitmap, childNode);
           }
       }


    return(retval);
   }

/*--------------- macGetNextNode -------------------------------------------
 *   gets next free tree tree node.
 */ 

int macGetNextNode(volume, currNode, newNode, treeType )
    struct m_volume *volume;
    unsigned int currNode;
    unsigned int *newNode;
    unsigned int treeType;
   {
    int        retval = E_NONE;
    char      *funcname = "macGetNextNode";
    struct BtreeNode **tree;
    int        tNode = -1;
    int a;
   
    if (treeType == CATALOG_TREE )
       {
        tree = volume->catalog;
       }
    else
       { 
        tree = volume->extents;
       }

    /* find next unused node in Catalog tree */
    *newNode = macNextFreeNode((struct bitmap_record *)
        tree[0]->BtNodes.BtHeader.hnBitMap.bitmap,
        (int)tree[0]->BtNodes.BtHeader.hnHdrRec.hrTotalNodes );

    if( *newNode == UNASSIGNED_NODE )
       {
        if( (retval = macAllocTreeExtent(volume, treeType)) == E_NONE )
           {
            retval = macGetNextNode( volume, currNode, newNode, treeType );
           }
       } 
    else
       {
        if((tree[*newNode]=(struct BtreeNode *)malloc(
           sizeof(struct BtreeNode))) == (struct BtreeNode *)0)
           {
            retval = set_error(E_MEMORY, Module, funcname,"");
           }

        if( retval == E_NONE )
           {
            tree[0]->BtNodes.BtHeader.hnHdrRec.hrFreeNodes--;
            macSetBitmapRec((struct bitmap_record *)
		   (tree[0]->BtNodes.BtHeader.hnBitMap.bitmap), *newNode);
            for ( a = 0; a < MAXRECORDS; a++ )
               {
                tree[*newNode]->BtNodes.BtLeaf.lnRecList[a] = 0;
               }
           }

        /* currNode check is used to create the free-standing root node */
        if ( retval == E_NONE && currNode != UNASSIGNED_NODE )
           {
            /* initialize new node -- index nodes of non-root level
                        DO use FLink & BLink */
            tree[*newNode]->BtNodes.BtLeaf.lnDesc.ndFLink = 
                tree[currNode]->BtNodes.BtLeaf.lnDesc.ndFLink;
            tree[currNode]->BtNodes.BtLeaf.lnDesc.ndFLink = *newNode;
            tree[*newNode]->BtNodes.BtLeaf.lnDesc.ndBLink = currNode;
            tree[*newNode]->BtNodes.BtLeaf.lnDesc.ndRecs = (short)0;
            tree[*newNode]->BtNodes.BtLeaf.lnDesc.ndLevel =
                tree[currNode]->BtNodes.BtLeaf.lnDesc.ndLevel;
            tree[*newNode]->BtNodes.BtLeaf.lnDesc.ndType =
                tree[currNode]->BtNodes.BtLeaf.lnDesc.ndType;

            if( tree[*newNode]->BtNodes.BtLeaf.lnDesc.ndType
                == LEAF_NODE &&
                tree[*newNode]->BtNodes.BtLeaf.lnDesc.ndFLink
                == UNASSIGNED_NODE )
               {
                tree[0]->BtNodes.BtHeader.hnHdrRec.hrLastLeaf = *newNode;
               }
 
            if( tree[*newNode]->BtNodes.BtLeaf.lnDesc.ndFLink 
                    != UNASSIGNED_NODE )
               {
                tNode = tree[*newNode]->BtNodes.BtLeaf.lnDesc.ndFLink;
                tree[tNode]->BtNodes.BtLeaf.lnDesc.ndBLink = *newNode;
               }
           }
       }
    if ( retval == E_NONE )
       {
        if ( treeType == CATALOG_TREE )
           {
            macSetBitmapRec (&volume->catBitmap, 0);
            macSetBitmapRec (&volume->catBitmap, currNode);
            macSetBitmapRec (&volume->catBitmap, *newNode);
            if ( tNode >= 0 )
               {
                macSetBitmapRec (&volume->catBitmap, tNode);
               }
           }
        else
           {
            macSetBitmapRec (&volume->extBitmap, 0);
            macSetBitmapRec (&volume->extBitmap, currNode);
            macSetBitmapRec (&volume->extBitmap, *newNode);
            if ( tNode >= 0 )
               {
                macSetBitmapRec (&volume->extBitmap, tNode);
               }
           }
       }


    return(retval);
   }


/*--- macCountNdRecs ---------------------------------------
 */

int macCountNdRecs( currNode, treeType )
    struct leaf_node  *currNode;
    unsigned int treeType;
   {
    /* usable space starts out at node size - size of the node descriptor -
       the one extra record offset at the end of the node (there is always one
       more than there are records, and it is two bytes) */
    int    usableSpace = NODE_SIZE-sizeof(struct node_descriptor)-
           (sizeof(short)),
           i = 0,
           recNum = 0,
           recSize = 0;


    while( usableSpace > 0 && (unsigned short)i <  currNode->lnDesc.ndRecs )
       {
        if ( (unsigned char)currNode->lnDesc.ndType == LEAF_NODE )
           {
            if ( treeType == EXTENTS_TREE )
               {
                recSize = sizeof (struct xkrKey) +
                          3 * sizeof (struct extent_descriptor);
               }
            else
               {
                macCalcRecSize(&recSize, currNode->lnRecList[i]);
               }
           }
        else
           {
            /* sizeof(struct pointer_record) includes an extra char 
               for null terminating name strings */
            recSize = sizeof( struct pointer_record ) - 1;
           }

        usableSpace -= recSize + sizeof(short); 

        if( usableSpace >= 0 )
           {
            (recNum)++;
           }
         i++;
        }

    return( recNum );
   }


/*--- macCalcRecSize ---------------------------------------
 */

int macCalcRecSize( recSize, currRec )
    int          *recSize;
    void         *currRec;
   {
    int    keylen,
           datalen,
           retval = E_NONE;
    char *funcname = "macCalcRecSize";

    /* record size includes key */
    if ( (keylen = ((struct record_key *)currRec)->key.ckr.ckrKeyLen+1) <= 0 ||
          keylen > sizeof(struct record_key) )
       {
        retval = set_error( E_RANGE, Module, funcname,"Illegal key length.");
       }
    else
       {
        /* keylens must be padded out */
        keylen += (keylen % 2)? 1 : 0;

        switch (  *( (char *)currRec + sizeof (struct record_key) ))
           {
            case FILE_REC:
                datalen = sizeof(struct file_record) - 
                          sizeof(struct record_key);
                break;
            case DIRECTORY_REC:
                datalen = sizeof(struct dir_record) - 
                          sizeof(struct record_key);
                break;
            case THREAD_REC:
                datalen = sizeof(struct thread_record) - 
                          sizeof(struct record_key);
                break;
            case ALIAS_REC:
                datalen = sizeof(struct alias_record) - 
                          sizeof(struct record_key);
                break;
            default:
                datalen = 0;
                retval = set_error(E_RANGE, Module, funcname, 
                                   "Illegal record type.");
                break;
           }
       }
    if ( retval == E_NONE )
       {
        *recSize = keylen + datalen;
       }

    return( retval );
   }  

/*--------------- macFindParent -------------------------------------------
 * searches tree for nodeno and recno of currNode's parent pointer record 
 * or where it would reside if it existed
 */ 

int macFindParent( currNode, tree, parNodeNum, parRecNum, treeType )
    unsigned int currNode;
    struct BtreeNode **tree;
    unsigned int *parNodeNum;
    int *parRecNum;
    int  treeType;
   {
    int        retval = E_NONE;
    unsigned int currSrchNode = 0;
    struct pointer_record targetPtrRec;

    /* update pointer record data to reflect new reference record */

    memcpy( (char *)&targetPtrRec.ptrRecKey,
       (char *) tree[currNode]->BtNodes.BtLeaf.lnRecList[0],
       sizeof(struct record_key) );

    targetPtrRec.ptrRecKey.key.ckr.ckrKeyLen = (treeType == EXTENTS_TREE)?
                EXTENT_KEYLENGTH : POINTER_REC_KEYLENGTH;
    targetPtrRec.ptrPointer = currNode;

    currSrchNode = tree[0]->BtNodes.BtHeader.hnHdrRec.hrRootNode;

    retval = macFindPtrRec(&targetPtrRec, 
        (tree[currNode]->BtNodes.BtIndex.inDesc.ndLevel), tree, 
        (unsigned int)treeType, currSrchNode, parRecNum, parNodeNum);

    if( retval == E_NOTFOUND && 
        tree[*parNodeNum]->BtNodes.BtIndex.inRecList[*parRecNum]->ptrPointer ==
        currNode )
       {
        retval = clear_error();
       }

    return(retval);
   }

/*-------------------------- macFindPtrRec ------------------------------
   finds location (or proper would-be) of pointer rec in index node(s) 
   convoluted is an understatement for the logic in this function. Some
   white knight who has a couple of hours to kill, please clean it up.
------------------------------------------------------------------------*/
int macFindPtrRec(targetRec, childLevel, tree, treeType, currNodeNo, 
                  recNum, nodeNum)
    struct pointer_record  *targetRec;
    int                 childLevel;
    struct BtreeNode   **tree;
    unsigned int        treeType;
    unsigned int        currNodeNo;
    int                *recNum;
    unsigned int       *nodeNum;
   {
    int          retval = E_NONE;
    unsigned int childNdNum;
    char     *funcname = "macFindPtrRec";

    *nodeNum = currNodeNo;

    retval = scanPtrRecs(&targetRec->ptrRecKey, 
        &tree[currNodeNo]->BtNodes.BtIndex, &childNdNum, recNum, treeType);

    /* test result of index node scan: */
    if( retval == E_NONE )
       {
        /* scanPtrRecs doesn't check ptrPointer */
        if( tree[currNodeNo]->BtNodes.BtIndex.inRecList[*recNum]->ptrPointer ==
            targetRec->ptrPointer )
           {
            *nodeNum = currNodeNo;
           }
       }

    /* if no hit, set up for next call to macFindPtrRec */
    if( retval == E_NOTFOUND )
       {
        if ( *recNum > 0 )
           {
            (*recNum)--;
           }
        if( tree[currNodeNo]->BtNodes.BtIndex.inRecList[*recNum]->
            ptrPointer == targetRec->ptrPointer )
           {
            *nodeNum = currNodeNo;
            retval = clear_error();
           }
        else
           {
            childNdNum = tree[currNodeNo]->BtNodes.BtIndex.
            inRecList[*recNum]->ptrPointer;
           }
    /* else leave retval what it be */
       }

    /* search next level if not found */
    if( retval == E_NONE && tree[childNdNum]->BtNodes.BtIndex.inDesc.ndLevel 
        > (unsigned char)childLevel )
       {
        retval = macFindPtrRec(targetRec, childLevel, tree, treeType,
           childNdNum, recNum, nodeNum);
       }

    if(  retval == E_NOTFOUND )
       {
        retval = clear_error();
        if (tree[childNdNum]->BtNodes.BtIndex.inDesc.ndLevel >
               (unsigned char)childLevel )
           {
            retval = macFindPtrRec(targetRec, childLevel, tree, treeType,
               childNdNum, recNum, nodeNum);
           }
        else
           {
            *nodeNum = childNdNum;

            retval = set_error(E_NOTFOUND,Module,funcname,
                "Can't find pointer record.");
           }
       }

    return(retval);
   }

/*-- macAllocTreeExtent ------------
 *
 */

int macAllocTreeExtent( volume, treeType )
    struct m_volume *volume;
    unsigned int treeType;
   {
    int retval = E_NONE;
    struct BtreeNode   **tree;
    struct extent_descriptor *ed;

    if ( treeType == CATALOG_TREE )
       {
        tree = volume->catalog;
       }
    else
       {
        tree = volume->extents;
       }

    retval = macAddExtentToTree(volume, treeType, &ed);

    if ( retval == E_NONE )
       {
        macAddExtentToVib(volume->vib, ed->ed_length);
        macAddExtentToVbm(volume->vbm, ed->ed_start, ed->ed_length);

        /* update tree header -- each node occupies one block */
        tree[0]->BtNodes.BtHeader.hnHdrRec.hrTotalNodes += ed->ed_length;
        tree[0]->BtNodes.BtHeader.hnHdrRec.hrFreeNodes += ed->ed_length;

        if ( treeType == CATALOG_TREE )
           {
            volume->vib->drCTFlSize +=
                (ed->ed_length * volume->vib->drAlBlkSiz);
           }
        else
           {
            volume->vib->drXTFlSize +=
                (ed->ed_length * volume->vib->drAlBlkSiz);
           }
       }
    if ( retval == E_NONE )
       {
        if ( treeType == CATALOG_TREE )
           {
            macSetBitmapRec (&volume->catBitmap, 0);
           }
        else
           {
            macSetBitmapRec (&volume->extBitmap, 0);
           }
       }


    return( retval );
   }

/*-- macAddExtentToTree ----------------------------------------
   points ed to next extent to be populated
 ---------------------------------------------------------------*/

int macAddExtentToTree( volume, treeType, ed)
    struct m_volume *volume;
    unsigned int treeType;
    struct extent_descriptor **ed;
   {
    int retval = E_NONE;
    int a, done = 0; 
    unsigned int xNodeNum = UNASSIGNED_NODE; 
    int xRecNum = UNASSIGNED_REC, blockCount = 0;
    int extSize = ((treeType == CATALOG_TREE) ? 
                  volume->vib->drCTClpSiz/volume->vib->drAlBlkSiz : 
                  volume->vib->drXTClpSiz/volume->vib->drAlBlkSiz );
    struct extent_descriptor *vibExtPtr = 
                  ((treeType == CATALOG_TREE) ? volume->vib->drCTExtRec : 
                  volume->vib->drXTExtRec);
    struct extent_record tempXRec;
    struct extent_record *rec;
    char *funcname = "macAddExtentToTree";

    *ed = 0;
    /* zero out tempXRec */
    for ( a = 0; a< sizeof(struct extent_record); a++ )
       {
        *((char *)(&tempXRec)+a) = '\0';
       }

    for ( a = 0; a < 3 && vibExtPtr[a].ed_length && retval == E_NONE; a++ )
       {
        blockCount += vibExtPtr[a].ed_length;
       }
    if( a < 3 )
       {
        *ed =  &vibExtPtr[a];
       }
    else 
       {
        /* xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */
        /* Implicit Assumption: no extents tree records */
        /* will be found or added to extents tree       */
        /* xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx */

        /* find locus for first extent record for this file */
        buildXRecKey((struct record_key *) &tempXRec, CATALOG_TREE_ID,
			       DATA_FORK, (unsigned short)blockCount);

        if( volume->extents[0]->BtNodes.BtHeader.hnHdrRec.hrTotalRecs == 0 )
           {
            retval = macPlantTree(volume, EXTENTS_TREE );
           }  
        if ( retval == E_NONE )
           {
            retval = searchTree(volume, (struct record_key *)&tempXRec, 
                        EXTENTS_TREE, &xRecNum, &xNodeNum);
           }  

        if( retval == E_NOTFOUND )
           {
            retval = clear_error();
           }

        /* if no extent records exist for this file, add one to tree  */
        if( retval == E_NONE &&
            ((unsigned short) xRecNum >= 
            volume->extents[xNodeNum]->BtNodes.BtLeaf.lnDesc.ndRecs ||
            ((struct extent_record *)volume->extents[xNodeNum]->
            BtNodes.BtLeaf.lnRecList[xRecNum])->xkrRecKey.key.xkr.xkrFNum != 
            CATALOG_TREE_ID) )
           {
            /* add this record to tree */
            if( retval == E_NONE )
               {
                retval = macAddRecToTree( volume, (int *)&xNodeNum, 
					   (void *)&tempXRec,
                                          &xRecNum, EXTENTS_TREE );
               }
               
            if( retval == E_NONE )
               {
                *ed = ((struct extent_record *)
                      volume->extents[xNodeNum]->BtNodes.BtLeaf.
                      lnRecList[xRecNum])->xkrExtents;
               }
           }
        /* found a record -- march through leaf records */
        else 
           {
            while( retval == E_NONE && !done )
               {
                rec = (struct extent_record *)volume->extents[xNodeNum]->
                                       BtNodes.BtLeaf.lnRecList[xRecNum];

                macSetBitmapRec (&volume->extBitmap, xNodeNum);

                for ( a = 0; done == 0 && a < 3; a++ )
                   {
                    if(  rec->xkrExtents[a].ed_length == 0 )
                       {
                        rec->xkrExtents[a].ed_length = 
                            tempXRec.xkrExtents[0].ed_length;
                        rec->xkrExtents[a].ed_start = 
                            tempXRec.xkrExtents[0].ed_start;
                        *ed = &rec->xkrExtents[a];
                        done = 1;
                       }
                    else
                       {
                        blockCount += rec->xkrExtents[a].ed_length;
                       }
                   }
                /* find or add next extent record */
                if( !done )
                   {
                    xRecNum++;

                    /* if no more records in this node */
                    if((unsigned short) xRecNum >= 
                        volume->extents[xNodeNum]->BtNodes.BtLeaf.
                       lnDesc.ndRecs)
                       {
                        /* if at last node or if next record not for this file */
                        if( volume->extents[xNodeNum]->BtNodes.BtLeaf.
                           lnDesc.ndFLink == UNASSIGNED_NODE || 
                           ((struct extent_record *)volume->
                           extents[volume->extents[xNodeNum]->
                           BtNodes.BtLeaf.lnDesc.ndFLink]->BtNodes.BtLeaf.
                           lnRecList[0])->xkrRecKey.key.xkr.xkrFNum 
                           != tempXRec.xkrRecKey.key.xkr.xkrFNum )
                           {
                            tempXRec.xkrRecKey.key.xkr.xkrFABN = (short)blockCount;
                            retval = macAddRecToTree(volume, (int *)&xNodeNum, 
                                  (void *) &tempXRec, &xRecNum, EXTENTS_TREE);
                           }
                        else
                           {
                            /* correct extent_record before adding to extents */
                            xNodeNum = volume->extents[xNodeNum]->BtNodes.
                                       BtLeaf.lnDesc.ndFLink;
                            xRecNum = 0;
                           }
                       }
                    /* if next record in this node is not for target file */
                    else if( ((struct extent_record *)
                        volume->extents[xNodeNum]->BtNodes.BtLeaf.
                        lnRecList[xRecNum])->xkrRecKey.key.xkr.xkrFNum
                        != tempXRec.xkrRecKey.key.xkr.xkrFNum )
                       {
                        tempXRec.xkrRecKey.key.xkr.xkrFABN = (short)blockCount;
                        retval = macAddRecToTree( volume, (int *)&xNodeNum,
			         (void *) &tempXRec, &xRecNum, EXTENTS_TREE );
                        done = 1;
                        if( retval == E_NONE )
                           {
                            *ed = ((struct extent_record *)
                                volume->extents[xNodeNum]->BtNodes.BtLeaf.
                                lnRecList[xRecNum])->xkrExtents;
                           }
                       }
                   }
               }
           }
       }
    if( retval == E_NONE && *ed )
       {
        if ( (retval = macFindBiggestExtent(volume->vib,volume->vbm,*ed))
            == E_NONE )
           {
            if( (*ed)->ed_length >= (unsigned short) extSize )
               {
                (*ed)->ed_length = (unsigned short) extSize;
               }
            else if ( (*ed)->ed_length < (unsigned short) extSize )
               {
                retval = set_error( E_SPACE, Module, funcname, "" );
               }
            /* added to bitmap in calling function */
           }
       }


    return( retval );
   }

/*-------------------------- macPlantTree ------------------------------
   puts first record into empty tree
------------------------------------------------------------------------*/
int macPlantTree(volume, treeType )
    struct m_volume *volume;
    unsigned int  treeType;
   {
    int    retval = E_NONE,
           a;
    struct BtreeNode   **tree;
    char   *funcname = "macPlantTree";

    if ( treeType == CATALOG_TREE )
       {
        tree = volume->catalog;
       }
    else
       {
        tree = volume->extents;
       }

    if((tree[1]=(struct BtreeNode *)malloc(sizeof(struct BtreeNode)))
       == (struct BtreeNode *)0)
       {
        retval = set_error(E_MEMORY, Module, funcname,"");
       }

    if( retval == E_NONE )
       {
        for ( a = 0; a< sizeof(struct BtreeNode); a++ )
           {
            *((char *)(tree[1])+a) = '\0';
           }

        /* initialize header node */
        tree[0]->BtNodes.BtHeader.hnHdrRec.hrDepthCount = 1;
        tree[0]->BtNodes.BtHeader.hnHdrRec.hrRootNode = 1;
        tree[0]->BtNodes.BtHeader.hnHdrRec.hrLastLeaf = 1;
        tree[0]->BtNodes.BtHeader.hnHdrRec.hrFirstLeaf = 1;

        tree[0]->BtNodes.BtHeader.hnHdrRec.hrFreeNodes--;
        macSetBitmapRec((struct bitmap_record *)
			(tree[0]->BtNodes.BtHeader.hnBitMap.bitmap), 1);

        /* initialize new node */
        tree[1]->BtNodes.BtLeaf.lnDesc.ndFLink = UNASSIGNED_NODE;
        tree[1]->BtNodes.BtLeaf.lnDesc.ndBLink = UNASSIGNED_NODE;
        tree[1]->BtNodes.BtLeaf.lnDesc.ndType = LEAF_NODE;
        tree[1]->BtNodes.BtLeaf.lnDesc.ndLevel = LEAF_LEVEL;
        tree[1]->BtNodes.BtLeaf.lnDesc.ndRecs = (short)0;
        if ( retval == E_NONE )
           {
            if ( treeType == CATALOG_TREE )
               {
                macSetBitmapRec (&volume->catBitmap, 0);
                macSetBitmapRec (&volume->catBitmap, 1);
               }
            else
               {
                macSetBitmapRec (&volume->extBitmap, 0);
                macSetBitmapRec (&volume->extBitmap, 1);
               }
           }

       }

    return( retval );
   }

/*-------------------------- scanPtrRecs ------------------------------
   searches index node for pointer record associated with targetKey
------------------------------------------------------------------------*/
int scanPtrRecs(targetKey, currIndex, childNodeNum, targetRec, treeType)
    struct record_key   *targetKey;
    struct index_node   *currIndex;
    unsigned int        *childNodeNum;
    int                 *targetRec;
    int                  treeType;
   {
    int    retval = E_NONE;

    if( treeType == EXTENTS_TREE ) 
       {
        retval = scanXPtrRecs(targetKey, currIndex, childNodeNum, targetRec);
       }
    else
       {
        retval = scanCPtrRecs(targetKey, currIndex, childNodeNum, targetRec);
       }

    return(retval);
   }

/*-------------------------- scanLeafRecs ------------------------------
   searches leaf node for record associated with targetKey entity
------------------------------------------------------------------------*/
int scanLeafRecs(targetKey, leafNode, targetRec, treeType)
    struct record_key   *targetKey;
    struct leaf_node    *leafNode;
    int                 *targetRec;
    unsigned int         treeType;
   {
    int retval = E_NONE;

    if( treeType == EXTENTS_TREE ) 
       {
        retval = scanXLeafRecs(targetKey, leafNode, targetRec);
       }
    else
       {
        retval = scanCLeafRecs(targetKey, leafNode, targetRec);
       }

    return(retval);
   }
    
/*-------------------------- macFreeTree ------------------------------
   frees a tree
------------------------------------------------------------------------*/
int macFreeTree( tree, treeType, vib )
    struct BtreeNode   ***tree;
    int treeType;
    struct m_VIB *vib;
   {
    int retval = E_NONE;

    int    nodeCount,
           treeSize = ((treeType == CATALOG_TREE) ? 
               vib->drCTFlSize : vib->drXTFlSize)/NODE_SIZE;
    int    i;

    /* loop through nodes of tree, freeing records... */

    for( nodeCount = 0; 
         *tree && nodeCount < treeSize; 
         nodeCount++ )
       {
        if( !(*tree)[nodeCount] )
           {
            ;     /* pass over unused nodes */
           } 
        else
           {
            switch(  (*tree)[nodeCount]->BtNodes.nodeBytes[NODE_TYPE_OFFSET] )
               {
                case INDEX_NODE:
                    for( i = 0; 
                         (*tree)[nodeCount]->BtNodes.BtIndex.inRecList[i];
                         i++ )
                       {
                        free ( (char *)(*tree)[nodeCount]->BtNodes.BtIndex.
                                                           inRecList[i]);
                        (*tree)[nodeCount]->BtNodes.BtIndex.inRecList[i] = 0;
                       }
                   break;
                case LEAF_NODE:
                    for( i = 0; 
                         (*tree)[nodeCount]->BtNodes.BtLeaf.lnRecList[i];
                         i++ )
                       {
                        free ( (char *)(*tree)[nodeCount]->BtNodes.BtLeaf.
                                                           lnRecList[i]);
                        (*tree)[nodeCount]->BtNodes.BtLeaf.lnRecList[i] = 0;
                       }
                   break;
               }
            free( (char *)(*tree)[nodeCount] );
           }
       }
         
    if ( *tree )
       {
        free( (char *)(*tree) );
        *tree = 0;
       }

    return(retval);
   }

 
/*--------------------- macWriteTreeExtent ------------------------------
    Writes an extent of the catalog or extent tree to disk.
 ---------------------------------------------------------------------*/

int macWriteTreeExtent ( volume, ed, buffer, bufOffset, treeType )
    struct m_volume *volume;
    struct extent_descriptor *ed;
    char *buffer;
    int bufOffset;
    int treeType;
   {
    int retval = E_NONE;
    unsigned int  block;
    unsigned int  blockStart;
    unsigned int  offset = 0;
    unsigned int  length;
    unsigned int  bytes_to_xfer = 0;
    unsigned int  bytes_xferred;
    struct bitmap_record *bitmap = (treeType == CATALOG_TREE) ? 
                                   &volume->catBitmap : &volume->extBitmap;
    int firstNode = bufOffset / NODE_SIZE;
    int lastNode = 
           firstNode + (ed->ed_length * volume->vib->drAlBlkSiz)/NODE_SIZE;
    int i = firstNode;
    int start, end;
    char *bufptr = buffer + bufOffset;
 

    /* Find actual block number. */
    blockStart = au_to_partition_block (volume->vib, ed->ed_start,
                                                volume->vstartblk);

    while( retval == E_NONE && i < lastNode )
       {
        /* Build block of contiguous nodes to write */

        while ( i < lastNode && !macInBitmapRec (bitmap, i) )
           {        
            i++;                /* skip over unused nodes */
           }
        if ( i < lastNode )
           {
            start = i;
            offset = (start - firstNode) * NODE_SIZE;
            while ( i < lastNode && macInBitmapRec(bitmap, i) )
               {
                i++;            /* build contiguous block of used nodes */
               }
            end = i;
            length = (end - start) * NODE_SIZE;
            block = blockStart + (start - firstNode);     
            bytes_xferred = 0;
            while( retval == E_NONE && bytes_xferred < length )
               {
                if( (length - bytes_xferred) > MAX_TRANSFER )
                   {
                    bytes_to_xfer = MAX_TRANSFER;
                   }
                else
                   {
                    bytes_to_xfer = (length - bytes_xferred);
                   }
         
                retval = macWriteBlockDevice( volume->device, bufptr+offset, 
                         block, bytes_to_xfer/SCSI_BLOCK_SIZE);

                block += bytes_to_xfer/SCSI_BLOCK_SIZE;
                offset += bytes_to_xfer;
                bytes_xferred += bytes_to_xfer;
               }
           }
       }

    return (retval);
   }


static unsigned int bitmapNodes(volume, bmp, totalnodes)
struct m_volume *volume;
struct bitmap_record * bmp;
unsigned int totalnodes;
{
    int count;
    unsigned int bitmapnodes = 0;

     /* temparary fix for Btree bitmap overflow */
    if (totalnodes > 2048)
	totalnodes = 2048;   

    for(count = 0; count < totalnodes; count++) {
        if ( macInBitmapRec(bmp, count) )
	    bitmapnodes++;
    }
    return (bitmapnodes);
}
