#ifndef _fb_h_
#define _fb_h_

#include <flash.h>

#define FILE_COUNT 32
#define DEFAULT_FLASH_SIZE  (1024*512)
#define DEFAULT_FLASH_VADDR (0xbfc00000)
#define DEFAULT_FLASH_PAGE_SIZE (512)

/*
 * Descriptor used to keep track of the various allocations of FLASH
 * parts in RAM, in the FLASH itself, in the in-memory FLASH image, etc.
 */
typedef struct{
  unsigned long baseOffset;		/* Offset to base of segment */
  unsigned long endOffset;		/* Offset to end of segment */
} FbDesc_t;


/* File specific data */
typedef struct{
  char*  	fileName;
  char*		segmentName;
  char*		segmentVersion;
  int           segmentType;
  unsigned long segmentPadToLength;
  unsigned long ramAddress;	/* RAM address of segment */
  FbDesc_t	flashDesc;	/* Allocation in flash */
  FbDesc_t      ramDesc;	/* Allocation in RAM */
  FbDesc_t	imageDesc;	/* Allocation in flash image */
} FbFile_t;


/* Command arguments */
typedef struct{
  unsigned long flashSize;	/* Size of flash in bytes */
  unsigned long flashVaddr;	/* Flash address as seen by system */
  unsigned long flashBase;	/* Flash address used for writing HEX images */
  unsigned long flashPageSize;	/* Flash page size */
  char*         imageFileName;	/* Name of image output file */
  char*         hexFileName;	/* Name of HEX output file */
  char*         elfFileName;	/* Name of elf output file (UNIMPLEMENTED) */
  FbFile_t      files[FILE_COUNT];
  int           fileCount;
  int		verbose;
} FbArgs_t;


/* Build state */
typedef struct{
  FbArgs_t*     args;		/* Pointer to arguments */
  int           fileIndex;	/* Index of current file */
  char*         image;		/* Start of flash image*/
  char*         end;		/* End of flash image */
} FbState_t;

#define IMAGE_LENGTH(fbs) (fbs->end - fbs->image)

#define CURRENT(s,t) (t)(s->image+s->current)
#define CURRENT_FILE(s) (&s->args->files[s->fileIndex])

int buildFlash(FbArgs_t*);
int loadElfFile(FbState_t*);
void writeHexFile(FbState_t*);
void writeElfFile(FbState_t*);

#endif
