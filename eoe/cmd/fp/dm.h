/* 
   (C) Copyright Digital Instrumentation Technology, Inc. 1990 - 1993
       All Rights Reserved
*/


/* DIT TransferPro header file: dm.h                                      */

#ifndef M_H
#define M_H

#include <sys/param.h>
#include "macLibrary.h"

#define MAC_DEFAULT_NAME "MAC_EDEFAULT"

#define DEBUGMODE 0

/* definitions and structures */
/* defs for df functions */
#define MAX_OPEN_FILES		 30
#define MAX_MAC_VOLUMES          10
#define MAXMACNAME               30
#define MAXRECORDS               28
#define VIB_START                 2
#define VIB_BLOCKCNT              1
#define TYP_FNDR_SIZE          0x10 
#define TREE_RESREC_SIZE        128
#define M_SUCCESS                 0
#define M_FAILURE                -1
#define M_TRUE                    1
#define M_FALSE                   0

#define DO_DATA_FORK              0
#define DO_RESOURCE_FORK          1
#define DO_FILE_RECORD            2

#define FROM_BEGINNING            0
#define FROM_CURRENT              1
#define FROM_END                  2

/* some SCSI drive constants: */
#define SCSI_BLOCK_SIZE          512

/* partition constants */
#define  PT_DEVSIGVAL            0x4552
#define  PT_PMAPOLDSIGVAL        0x5453
#define  PT_PMAPSIGVAL           0x504d
#define  PT_DDTYPEMAC            1
#define  PT_PARTTYPEMFS          "Apple_MFS"
#define  PT_PARTTYPEHFS          "Apple_HFS"
#define  PT_PARTTYPEUNIXSVR2     "Apple_Unix_SVR2"
#define  PT_PARTTYPEPMAP         "Apple_partition_map"
#define  PT_PARTTYPEDRIVER       "Apple_Driver"
#define  PT_PARTTYPEPRODOS       "Apple_PRODOS"
#define  PT_PARTTYPEFREE         "Apple_Free"
#define  PT_PARTTYPESCRATCH      "Apple_Scratch"

#define  PTNBLK        1    /* partition block location */
#define  PTNBLKCNT     1    /* partition block count    */


/* some VIB constants: */
/* EBUFSIZE must be a multiple of the largest possible drAlBlkSiz */
#define MAX_EBUFSIZE         0x10000  /* 64K -- maximum amt of file in mem */
#define MAC_DATEFACTOR    2082823200
#define MAX_MAC_VOL_NAME          27
#define MAX_FNDRINFO_SIZE         32 
#define MAX_XTEXTREC               6
#define MAX_CTEXTREC               6
#define MAX_CATBLKBUFFER        8192
#define MAX_EXTBLKBUFFER        8192
#define NODE_SIZE                512
#define EXTENTS_FILE_ID            3
#define CATALOG_FILE_ID            4
#define DADSECTOR_FILE_ID          5   /* organize the bad sectors */
#define ROOT_DIR_ID                2
#define ROOT_PAR_ID                1
#define ROOT_LEVEL                 2

/* Directory entry definitions */
#define HEXBASE                   16

/* Extents tree definitions */
#define X_KEY_SIZE                8

/* Catalog tree definitions */
#define PARID_OFFSET               5
#define INDEX_NODE                0x00
#define HEADER_NODE               0x01
#define BITMAP_NODE               0x02 
#define LEAF_NODE                 0xff

#define DELETED_REC                  0
#define DIRECTORY_REC                1
#define FILE_REC                     2
#define THREAD_REC                   3
#define ALIAS_REC                    4
#define THREAD_RES_SIZE              8
#define POINTER_REC_SIZE            42
#define POINTER_REC_KEYLENGTH     0x25

#define NODE_TYPE_OFFSET             8
#define UNASSIGNED_NODE              0
#define UNASSIGNED_REC              -1
#define EXTENT_KEYLENGTH             7
#define EXTENT_NODE                  1

#define EXTENTS_TREE                 1
#define CATALOG_NODE                 0
#define CATALOG_TREE                 0
#define RESOURCE_FORK             0xff
#define DATA_FORK                    0
#define EXTENT_TREE_ID               3
#define CATALOG_TREE_ID              4
#define LEAF_LEVEL                   1

#define Atrb_HW_Locked  (1<<7)  /* Locked by HW */
#define Atrb_UnMounted  (1<<8)  /* Volume unmounted */
#define Atrb_SparedBad  (1<<9)  /* Volume has had bad blocks spared */
#define Atrb_SW_Locked  (1<<15) /* Locked by SW */

typedef void *DITDEVICE;

/* structure definitions: */

/* VIB info stored here for duration of disk session*/

struct extent_descriptor
   {
    unsigned short ed_start;
    unsigned short ed_length;
   };

/* VBM buffer */
struct m_VBM
   {
    unsigned char bitmap[NODE_SIZE];
	 };

/* VIB info stored in struct below for duration of disk session*/
struct m_VIB {               /* hex offsets: */
    unsigned short drSigWord;            /*  00-01   */
    unsigned int drCrDate,               /*  02-05   */
                 drLsMod;                /*  06-09   */
    unsigned short drAtrb,               /*  0a-0b   */
        drNmFls,                         /*  0c-0d   */
        drVBMSt,                         /*  0e-0f   */
        drAllocPtr,                      /*  10-11   */
        drNmAlblks;                      /*  12-13   */
    unsigned int drAlBlkSiz,             /*  14-17   */
        drClpSiz;                        /*  18-1b   */
    unsigned short drAlBlSt;             /*  1c-1d   */
    unsigned int drNxtCNID;              /*  1e-21   */
    unsigned short drFreeBks;            /*  22-23   */
    unsigned char drVolNameLen;          /*  25-3f   */
    char drVolName[MAX_MAC_VOL_NAME];    /*  25-3f   */
    unsigned int drVolBkUp;              /*  40-43   */
    unsigned short drVSeqNum;            /*  44-45   */
    unsigned int drWrCnt;                /*  46-49   */
    unsigned int drXTClpSiz,             /*  4a-4d   */
        drCTClpSiz;                      /*  4e-51   */
    unsigned short drNmRtDirs;           /*  52-53   */
    unsigned int drFilCnt,               /*  54-57   */
        drDirCnt;                        /*  58-5b   */
    unsigned char drFndrInfo[MAX_FNDRINFO_SIZE];  /*  5c-7b   */
    unsigned short drVCSiz,              /*  7c-7d   */
          drVBMSiz,                      /*  7e-7f   */
          drCtlCSiz;                     /*  80-81   */
    unsigned int drXTFlSize;             /*  82-85   */
    struct extent_descriptor drXTExtRec[3];   /*  86-91   */
    unsigned int drCTFlSize;                  /*  92-95   */
    struct extent_descriptor drCTExtRec[3];   /*  96-a5   */
    unsigned char drUnused[350];
   }; 

struct m_DriverDescriptor
   {
    unsigned int   ddBlock;      /* first block of driver */
    unsigned short ddSize;       /* driver size in blocks */
    unsigned short ddType;       /* systemType (1 for Macintosh) */
   };

struct m_DriverMap /* maps to first physical block of device */
   {
    unsigned short sbSig;        /* device identifier, always $4552 */
    unsigned short sbBlockSize;    /* block size of device */
    unsigned int   sbBlkCount;   /* no. blocks on device */
    unsigned short sbDevType;    /* internal */
    unsigned short sbDevID;      /* internal */
    unsigned int   sbData;       /* internal */
    unsigned short sbDrvrCount;  /* number of driver descriptors */
    struct m_DriverDescriptor sbDescriptorList[61]; /* list of desciptors */
    unsigned char sbUnused[ 6 ];
   };

struct m_PartitionMap /* maps to a partition map block */
   {
    unsigned short pmSig;          /* always $504D */
    unsigned short pmSigPad;       /* reserved */
    unsigned int pmMapBlkCnt;      /* number of blocks in partition map */
    unsigned int pmPyPartStart;    /* first physical block of partition */
    unsigned int pmPartBlkCnt;     /* number of blocks in partition */
    char         pmPartName[32];  /* partition name */
    char         pmPartType[32];  /* partition type */
    unsigned int pmLgDataStart;    /* first logical block of data area */
    unsigned int pmDataCnt;        /* number of blocks in data area */
    unsigned int pmPartStatus;     /* partition status information */
    unsigned int pmLgBootStart;    /* first logical block of boot code */
    unsigned int pmBootSize;       /* size in bytes of boot code */
    unsigned int pmBootLoad;       /* boot code load address */
    unsigned int pmBootLoad2;      /* additional boot load information */
    unsigned int pmBootEntry;      /* boot code entry point */
    unsigned int pmBootEntry2;     /* additional boot entry information */
    unsigned int pmBootCksum;      /* boot code checksum */
    char         pmProcessor[16]; /* processor type */
    unsigned char pmBootBytes[128];/* boot-specific bytes */
    unsigned char pmUnused[248];
   };

/* there will be a list of these on the volume, starting at block 1. The
   list is terminated by an element that is all 0's. See IV-292 of Inside
   Macintosh. */
#define MAX_OLDPARTITIONS     42
#define OLD_PARTTYPEHFS       "TFS1"
#define PARTTYPE_OLD          1
#define PARTTYPE_NEW          0
#define OLDPTNSIG0            0x54
#define OLDPTNSIG1            0x53


struct m_OldPartitionDescriptor
   {
    unsigned int   pdStart;
    unsigned int   pdSize;
    char           pdFSID[4]; /* "TFS1" for Mac Plus partitions */
   };

struct m_OldPartitionMap
   {
    unsigned short pdSig;
    struct m_OldPartitionDescriptor pdPartition[MAX_OLDPARTITIONS];
    unsigned char  pdUnused[6];
   };

/*-----------------------------------------------------------------------------*/
/* Record structure definitions */
/*-----------------------------------------------------------------------------*/

/* structures used by Finder; parts of directory_record and file_record */
struct MacPoint
   {
    unsigned short v; /* vertical coordinate */
    unsigned short h; /* horizontal coordinate */
   };       

struct MacRect
   {
    unsigned short top;
    unsigned short left;
    unsigned short bottom;
    unsigned short right;
   };

struct FdFlags
   {
    /* byte 1 */
    unsigned char isAlias:1,       /* File is alias file, 0 if directory */
                  isInvisible:1,   /* File or dir is invisible from Finder */
                  hasBundle:1,     /* File contains bundle resource-dir=0 */
                  nameLocked:1,    /* File or dir can't be renamed from Finder */
                  isStationery:1,  /* File is stationery pad-dir=0 */
                  hasCustomIcon:1, /* File or dir has customized icon */
                  Reserved:1,      /* Reserved; set to 0 */
                  hasBeenInited:1, /* Fndr recorded info to desktop database */

    /* byte 0 */
                  hasNoINITS:1,    /* File has no 'INIT' resources; set to 0 */
                  isShared:1,      /* App avail to mul users; for app,else 0 */
                  requiresSwitchLaunch:1, /* Unused and reserved; set to 0 */
                  colorReserved:1, /* Unused and reserved; set to 0 */
                  color:3,         /* 3 bits of color coding */
                  isOnDesk:1;      /* Unused and reserved; set to 0 */
   };

struct FrFlags
   {
    unsigned char isAlias:1,       /* Reserved for dir.; set to 0 */
                  isInvisible:1,   /* Dir invisible from Finder */
                  unused_1_20:1,
                  nameLocked:1,    /* Dir can't be renamed from the Finder */
                  isStationery:1,  /* Reserved for dir; set to 0 */
                  hasCustomIcon:1, /* Dir contains a customized icon */
                  Reserved:1,      /* Reserved; set to 0 */
                  hasBeeenInited:1,/* Finder put info from files's bundle into desktop database */
                  unused_0_fe:7,
                  isOnDesk:1;      /* Unused and reserved; set to 0 */
   };

struct FInfo
   {
    unsigned char   fdType[4];     /* 4-byte file type string */
    unsigned char   fdCreator[4];  /* 4-byte file creator string */
    struct FdFlags  fdFlags;       /* bit 13=1 if file has bundle (goodies in
                                      resource fork),
                                      bit 14=1 if file invisible */
    struct MacPoint fdLocation;    /* location of file in window
                                      (=0 when creating file) */
    unsigned short  fdFldr;        /* file's window: -3=trash, -2 = desktop,
                                      0 = disk window */
   };

struct FXInfo
   {
    unsigned short fdIconID;     /* ID of icon associated with file */
    unsigned char  fdUnused[6];  /* 6-byte reserved string */
    char    fdScript;     /* Script flag and code */
    char    fdXFlags;     /* Reserved */
    unsigned short fdComment;    /* ID of comment for file */
    unsigned int   fdPutAway;      /* ID of file's home directory */
   };

struct DInfo
   {
    struct MacRect  frRect;       /* Rectangle for folder's window */
    struct FrFlags  frFlags;       /* bit 14=1 if dir invisible */
    struct MacPoint frLocation;     /* Folder's location on desktop */
    unsigned short  frView;       /* Folder's view */
   };

struct DXInfo
   {
    struct MacPoint frScroll;     /* scroll position of folder window */
    unsigned int    frOpenChain;  /* directory ID chain of open folders */
    char     frScript;     /* Script flag and code */
    char     frXFlags;     /* Reserved */
    unsigned short  frComment;    /* ID of folder comment */
    unsigned int    frPutAway;    /* ID of folder containing folder */
   };

/* two germane records found in B * tree header node */
struct header_record 
   {
    unsigned short  hrDepthCount;
    unsigned int    hrRootNode,
                    hrTotalRecs,
                    hrFirstLeaf,
                    hrLastLeaf;
    unsigned short  hrNodeSize,
                    hrKeyLen;
    unsigned int    hrTotalNodes,
                    hrFreeNodes;
    unsigned char   hrUnused[ 76 ];
   };

struct bitmap_record 
   {
    unsigned char bitmap[256];
   };



struct xkrKey
   {
    unsigned char    xkrKeyLen,
                     xkrFkType;   /* 00 for data fork; ff for resource fork */
    unsigned int     xkrFNum;     /* file number */
    unsigned short   xkrFABN;     /* allocation block number within file */
   };

struct ckrKey
   {
    unsigned char    ckrKeyLen;
    unsigned char    ckrResrvl;   /* used internally */
    unsigned int     ckrParID;    /* parent directory ID */
    unsigned char    ckrNameLen;  /* name length */
    char             ckrCName[MAXMACNAME+1]; /* followed by name */
   };

struct record_key
   {
    union 
       {
        struct ckrKey ckr;
        struct xkrKey xkr;
       } key;
   };

/* the single record type found in index nodes */
struct pointer_record 
   {
    struct record_key ptrRecKey;
    unsigned int ptrPointer;
   };


/* the four records found in catalog leaf nodes:   */
/*             directory, thread, file, deleted    */
struct dir_record 
   {
    struct record_key dirRecKey;
    unsigned char     dirType,     /* always 1 for directory records */
                      dirReserv2;  /* used internally */
    unsigned short    dirFlags,    /* flags */
                      dirVal;      /* valence: # of files in this directory */
    unsigned int      dirDirID,    /* ID number for this directory */
                      dirCrDat,    /* Date and time of creation */
                      dirMdDat,    /* Date and time of last modification */
                      dirBkDat;    /* Date and time of last backup */
    struct DInfo      dirUsrInfo;  /* Info used by the Finder */
    struct DXInfo     dirFndrInfo; /* more info used by the Finder */
    unsigned char     dirReserv[TYP_FNDR_SIZE];    /* used internally */
   };

struct thread_record 
   {
    struct record_key thdRecKey;
    unsigned char     thdType,         /* always 3 for thread records */
                      thdReserv2;      /* used internally */
    unsigned char     thdReserv[8];    /* used internally */
    unsigned int      thdParID;        /* parent ID of associated directory */
    unsigned char     thdCNameLen;     /* name of associated directory -- */
    char              thdCName[MAXMACNAME]; /* name of associated dir -- */
                                       /* (name field ckrCName in the key */
                                       /*  is not used in this case) */
   };

struct file_record         /*        "meaty" stuff       */
   {
    struct record_key filRecKey;
    unsigned char     cdrType,     /* always 2 for file records */
                      cdrReserv2,  /* used internally */
                      filFlags,    /* file flags */
                      filType;     /* file version number (s/b 0) */
    struct FInfo      filUsrWds;   /* Info used by the Finder */
    unsigned int      filFlNum;    /* file ID number */
    unsigned short    filStBlk;    /* 1st alloc. block of data fork (obs.) */
    unsigned int      filLgLen,    /* logical end-of-file of data fork */
                      filPyLen;    /* physical end-of-file of data fork */
    unsigned short    filRStBlk;   /* 1st alloc. block of res. fork (obs.) */
    unsigned int      filRLgLen,   /* logical end-of-file of res. fork */
                      filRPyLen,   /* physical end-of-file of res. fork */
                      filCrDat,    /* date and time of file creation */
                      filMdDat,    /* date and time of last modification */
                      filBkDat;    /* date and time of last backup */
    struct FXInfo     filFndrInfo; /* more info used by the Finder */
    unsigned short     filClpSize; /* file clump size */
    struct extent_descriptor filExtRec[3];   /* 1st ext recs for data fork */
    struct extent_descriptor filRExtRec[3];  /* 1st ext recs for resource fork */
    unsigned int      filReserv;   /* used internally */
   };

struct alias_record        /*    new record for system 7 */
   {
    struct record_key alsRecKey;
    unsigned char     cdrType,     /* always 4 for alias records */
                      cdrReserv2,  /* used internally */
                      alsFlags[12];    /* file flags */
    unsigned char     alsNameLen;  /* alias file name length */
    char              alsName[MAXMACNAME]; /* alias file name */
   }; 
    
/* the one record found in extents file leaf nodes */
struct extent_record 
   {
    struct record_key xkrRecKey;
    struct extent_descriptor xkrExtents[3];  /* array of extents descriptors */
   };

/* the file list union */
struct file_list 
   {
    union 
       {
         struct file_record flFilRec;
         struct dir_record flDirRec;
         struct alias_record flAlsRec;
         unsigned char   flEntryBytes[ sizeof(struct file_record) ];
       } entry;
     struct file_list *flNext;
   };


/*-----------------------------------------------------------------------------*/
/* B * Tree node structures */
/*-----------------------------------------------------------------------------*/
 

#define NODE_DESC_SIZE 14

/* Bst node_descriptor */
struct node_descriptor 
   {
    unsigned int     ndFLink,      /* next node link (used only in leaves) */
                     ndBLink;      /* previous node link (used only in leaves) */
    unsigned char    ndType,       /* type of this node */
                     ndLevel;      /* level of this node in the hierarchy */
    unsigned short   ndRecs,       /* number of records stored in this node */
                     ndReserved;   /* used internally */
   } ;

struct header_node 
   {
    struct node_descriptor  hnDesc;
    struct header_record    hnHdrRec;
    unsigned char           hnResRec[ TREE_RESREC_SIZE ];
    struct bitmap_record    hnBitMap;
   };

struct index_node
   {
    struct node_descriptor  inDesc;
    struct pointer_record  *inRecList[ MAXRECORDS ];
   };

struct leaf_node
   {
    struct node_descriptor   lnDesc;
    void                    *lnRecList[ MAXRECORDS ];
   };

struct map_node
   {
    struct node_descriptor   mnDesc;
    struct bitmap_record     mnRecList;
   };

struct BtreeNode
   {
    union 
       {
        struct header_node   BtHeader;
        struct index_node    BtIndex;
        struct leaf_node     BtLeaf;
        struct map_node      BtMap;
        unsigned char        nodeBytes[ NODE_SIZE ];
       } BtNodes;
   };


struct m_volume
   {
    char mountPoint[MAXPATHLEN+1];
    void *device;
    struct m_VIB *vib;
    struct m_VBM *vbm;
    struct BtreeNode **extents;
    struct BtreeNode **catalog;
    struct bitmap_record extBitmap;
    struct bitmap_record catBitmap;
    struct ExtenList *Extensions;
    int CWDid;
    char curDir[MAXPATHLEN+1];
    int maxNodes;
    int vstartblk;
    int vblksize;
   };

/* file descriptor for single open file */
struct m_fd
   {   
    struct file_record *fdFileRec;      /* File record associated with file */
    struct extent_record *fdExtentRec;  /* Current extent record -- only used
                                           for writes, to keep track of current
                                           extent record */
    int fdExtentNode;                   /* node in Extent tree of current
                                           fdExtentRec -- used to make sure
                                           node gets updated upon flush when
                                           closing file */

    /* these variables are pointers into the file rec above for the 
       fork-specific info so the code can work for both */
    unsigned int   *fdLogicalLen;
    unsigned int   *fdPhysicalLen;
    struct extent_descriptor *fdExtents; /* extent pointer (for first three) */

    /* rest of descriptor info */
    long   fdOffset;                     /* Current offset into file */
    long   fdExtOffset;                  /* Current offset in extent (from 0)*/
    unsigned int    fdExtBlock;          /* Current block in extent (from 0) */
    struct extent_descriptor *fdExtentLoc; /* location of current extent */
    BOOL fdExtentNew;                    /* is this a newly alloced extent? */

    char  *fdExtent;                     /* Current extent (Ebuf) */
    unsigned int fdEbufOffset;           /* Current offset of extent buf
                                            within current extent */
    unsigned int fdEbufSize;             /* Current size of extent buf */
    BOOL fdEbufWritePending;             /* does the ebuf contain write
                                            data that needs to be flushed? */
    
    unsigned char   fdEOF;               /* EOF flag: true or false */
    unsigned char   fdMode;              /* Mode READMODE/WRITEMODE */
    unsigned int    fdFork;              /* which fork is open? */
    struct m_volume *fdVolume;                     /* current volume info */
   };

/* used to keep track of extension conversions */
struct ExtensionList
   {
    unsigned char  *exExtension,
          *exType,
          *exCreator;
   };

/* used to keep track of extension conversions for OL */
struct ExtenList
   {
    unsigned int eIdnum;
    char eType[4];
    char eCreator[4];
    struct ExtenList *next;
   };


/* ------------------------------------------------------- */
/* externally available entities residing in other files: */
/* ------------------------------------------------------- */
/* module macBitMap.c */
int macReadBitmap (struct m_volume *, int);
int macWriteBitmap (struct m_volume *);
void macSetBit(char *, int);
void macClearBit(char *, int);
int macGetBit(char *, int);
int macInBitmapRec(struct bitmap_record *, int);
void macSetBitmapRec(struct bitmap_record *, int);
int macNextFreeNode(struct bitmap_record *, int);
void macClearBitmapRec(struct bitmap_record *, int);

/* module macCatalog.c */
int macReadCatalog (struct m_volume *);
int macFindFile(struct m_volume *, int *, int *, struct file_record **, char *, int);
int macFindAlias(struct m_volume *, int *, int *, struct alias_record **, char *, int);
int macFindDir(struct m_volume *, unsigned int *, int *, struct dir_record  **, char *, int);
int macDirId(struct m_volume *, char *, int *);
int macAbsScandir(struct m_volume *, char *, int *, BOOL, struct file_list   **);
int macScandir(struct m_volume *, char *, int *, BOOL, struct file_list **);
int freeMacFileList(struct file_list **);
int macWriteCatalog (struct m_volume *);
void buildRecKey(struct record_key *, int, char *);
int scanCPtrRecs(struct record_key *, struct index_node *, unsigned int *, int *);
int scanCLeafRecs(struct record_key *, struct leaf_node *, int *);
int macAddRecToDir(struct m_volume *, int, unsigned int);
int macDelRecFromDir(struct m_volume *, int, unsigned int);

/* macDate.c */
char *macDateToString(int);
int macDate(void);

/* module macExtents.c */
int macReadExtentsTree (struct m_volume *);
int macWriteExtentsTree (struct m_volume *);
void buildXRecKey(struct record_key *, unsigned int, unsigned char, unsigned short);
int scanXPtrRecs(struct record_key *, struct index_node *, unsigned int *, int *);
int scanXLeafRecs(struct record_key *, struct leaf_node *, int *);
int macFindExtRec(struct m_fd *, unsigned int  *, unsigned int  *);
int macAddExtentsList(struct m_fd *);

/* module macFile.c */
int macReadFile (struct m_fd *, char *, unsigned int);
int macReadFileRecord (struct m_fd *, char *, unsigned int);
int macSetReadExtent(struct m_fd *);
int macSetReadExtentBuffer(struct m_fd *);
int macReadExtent(struct m_volume *, struct extent_descriptor *, char *);
int macFindExtent (struct m_fd *, struct extent_descriptor **);
int macWriteFile (struct m_fd *, char *, unsigned int);
int macWriteFileRecord (struct m_fd *, char *, unsigned int);
int macSetWriteExtent(struct m_fd *);
int macSetWriteExtentBuffer(struct m_fd *);
int macFlushFileExtent(struct m_fd *);
int macFlushFileExtentBuffer(struct m_fd *);
int macWriteExtent(struct m_volume *, struct extent_descriptor *, char *);
int macFreeFile(struct m_fd *);
int macRenameFileRec(struct m_volume *, char *, int, struct m_volume *, char *, int, struct file_record *, int, int);
int macRenameDirectory(struct m_volume *, char *, int, struct m_volume *, char *, int, struct dir_record *, int, int);
int macRmDir(struct m_volume *, unsigned int, int, int);
int macOpenFile(struct m_volume *, struct file_record *, int, int, int *);
int macCloseFile(int);
int initFileRec(struct m_volume *, struct file_record *, char *, int); 
int initDirRec(struct m_VIB *, struct dir_record *, char *, int); 
int initThdRec(struct dir_record *, struct thread_record *); 
int macGetNextID(struct m_VIB *);
void macAddExtentToVib(struct m_VIB *, int);
void macAddExtentToVbm(struct m_VBM *, unsigned int, unsigned int);
void macRemoveExtentFromVbm(struct m_VBM *, unsigned int, unsigned int);
int macAddExtentToFile(struct m_fd *, unsigned int, unsigned int);
void macFreeExtentArray( struct m_VIB *, struct m_VBM *, struct extent_descriptor *, unsigned int *);
int macAllocFileExtent(struct m_fd *);
int macExtendCurrentExtent(struct m_fd *);
int macFindBiggestExtent(struct m_VIB *, struct m_VBM *, struct extent_descriptor *);
struct m_fd *macGetFd(int);
int macNewFd(void);

/* module macPack.c */

int macPackDriverMap(struct m_DriverMap *, char *);
void macUnpackDriverMap(struct m_DriverMap *, char *);
int macUnPackHeaderNode(struct header_node *, char *);
int macUnPackIndexNode(int, struct index_node  *, char *);
int macUnPackLeafNode(int, struct leaf_node *, char *);
void macUnPackMapNode(struct map_node   *, char *);
int macPackTree(struct BtreeNode **, int, struct m_VIB *, char *);
void macPackHeaderNode(char *, struct header_node *);
void macPackIndexNode(int, char *, struct index_node  *);
int macPackLeafNode(int, char *, struct leaf_node *);
void macPackMapNode(char *, struct map_node *);
void macPackVIB(struct m_VIB *, char *);
void macUnpackVIB(struct m_VIB *, char *);

void macUnpackHeaderRec(struct header_record *, char *);
void macUnpackDirRecord(struct dir_record *, char *);
void macUnpackThreadRec(struct thread_record *, char *);
void macUnpackFileRec(struct file_record *, char *);
void macUnpackAliasRec(struct alias_record *, char *);
void macUnpackRecordKey(struct ckrKey *, char *);
void macUnpackExtRecordKey(struct xkrKey *, char *);
void macUnpackNodeDescriptor(struct node_descriptor *, char *);

int macPackHeaderRec(struct header_record *, char *);
int macPackDirRecord(struct dir_record *, char *);
int macPackThreadRec(struct thread_record *, char *);
int macPackFileRec(struct file_record *, char *);
int macPackAliasRec(struct alias_record *, char *);
int macPackRecordKey(char *, struct ckrKey *);
int macPackExtRecordKey(char *, struct xkrKey *);
int macPackNodeDescriptor(char *, struct node_descriptor *);

void macUnpackPartitionMap(struct m_PartitionMap *, char *);
void macUnpackOldPartitionMap(struct m_OldPartitionMap *, char *);

int macPackPartitionMap(struct m_PartitionMap *, char *);

int getRecordOffset(int, short *);
void putRecordOffset(short *, int, int);

/* macResource.c */
int rForkUnpack();
int getResourcesFromFork();
int rForkTemplate();
int resource_insert();
void resource_remove();
int resource_alloc();
int resource_free();
int resource_list_free();
int data_alloc();
void data_free();
int instance_alloc();
void instance_free();

/* module macTree.c */
int macBuildTree(struct m_volume *, unsigned int, char *);
int searchTree(struct m_volume *, struct record_key *, unsigned int, int *, unsigned int *);
int travTree(struct m_volume *, struct record_key *, unsigned int, unsigned int, int *, unsigned int *);
int macDelNodeFromTree(struct m_volume *, unsigned int, struct BtreeNode **, int);
int macDelRecFromTree(struct m_volume *, unsigned int, int, int);
int macCorrectParents(struct m_volume *, unsigned int, int, unsigned int, int, int);
int macAddRecToTree(struct m_volume *, int *, void *, int *, int);
int macInsertRec(struct m_volume *, void *, unsigned int, int, unsigned int *, unsigned int);
int macCorrectPtrRec(struct m_volume *, struct BtreeNode **, unsigned int, unsigned int, int, unsigned int);
int macAddNodeToTree(struct m_volume *, unsigned int, unsigned int, unsigned int);
int macAddNewRootToTree(struct m_volume *, unsigned int, unsigned int, unsigned int);
int macDelOldRootFromTree(struct m_volume *, struct BtreeNode **, int);
int macGetNextNode(struct m_volume *, unsigned int, unsigned int *, unsigned int);
int macCountNdRecs(struct leaf_node *, unsigned int);
int macCalcRecSize(int *, void *);
int macFindParent(unsigned int, struct BtreeNode **, unsigned int *, int *, int);
int macFindPtrRec(struct pointer_record *, int, struct BtreeNode **, unsigned int, unsigned int, int *, unsigned int *);
int macAllocTreeExtent(struct m_volume *, unsigned int);
int macAddExtentToTree(struct m_volume *, unsigned int, struct extent_descriptor **);
int macPlantTree(struct m_volume *, unsigned int);
int scanPtrRecs(struct record_key *, struct index_node *, unsigned int *, int *, int);
int scanLeafRecs(struct record_key *, struct leaf_node *, int *, unsigned int);
int macFreeTree(struct BtreeNode ***, int, struct m_VIB *);
int macWriteTreeExtent(struct m_volume *, struct extent_descriptor *, char *, int, int);


/* module macUtil.c */
int au_to_block (struct m_VIB *, unsigned int);
int au_to_partition_block (struct m_VIB *, unsigned int, int);
char *ditPathToMacPath(char *, char *);
char *macPathToDitPath();
void splitPathSpec(char *, char *, char *);
int macGetPath(struct m_volume *, char *, char *);

/* module macVIB.c */
int macReadVIB (struct m_volume *);
int macWriteVIB (struct m_volume *);

/* macVolume.c */
int macNewVolume();
int macFreeVolume();
struct m_volume *macGetVolume();
int macInit(struct m_volume *);
int macUpdateDisk(struct m_volume *);
int macResetVolume();
int mac_partition_init(void *),
    mac_block0_init(void *),
    mac_boot_init(void *),
    mac_vib_init(struct m_volume *, char *, int, int),
    mac_vbm_init(struct m_volume *),
    mac_extent_init(struct m_volume *),
    mac_catalog_init(struct m_volume *, char *),
    mac_desktop_init(struct m_volume *),
    mac_partition_init();



/* dm.c module:    */ 

int dm_mount();
int dm_unmount();
int dm_open();
int dm_close();
int dm_scandir();
int dm_read();
int dm_write();
int dm_dirselect();
int dm_getlabel();
int dm_getwd();
int dm_rename();
int dm_file_find();
int dm_rm();
int dm_mkdir();
int dm_chdir();
int dm_rmdir();
int dm_space_available();
int dm_format();
int dm_get_partition_data();
int dm_partition_init();
int dm_read_ftype_creator();
int dm_write_ftype_creator();
int dm_seek();
long dm_tell();
int getFileNum();

#endif

