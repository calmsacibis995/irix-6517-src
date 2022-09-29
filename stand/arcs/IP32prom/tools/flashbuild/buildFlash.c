#include <assert.h>
#include <errno.h>
#include "fb.h"
#include <fcntl.h>
#include <flash.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>


int  loadFile(FbState_t*);
int  writeFiles(FbState_t*);
int  loadDataFile(FbState_t*);
int  initSegment(FbState_t*);
int  finishSegment(FbState_t*);

/*
 * We build an image of the flash by reading in and processing each file
 * specified by the user.  Each file contains the contents of one flash
 * segment.  For each file, we maintain three descriptors:
 *
 * 1) flashDesc - describes the allocation of the segment in the flash
 *    itself.  BaseOffset and endOffset are offsets from virtual address
 *    0 (i.e. they are 32 bit virtual addresses).  For code that
 *    is executed directly from flash, these are the values used for
 *    relocation.  
 *
 * 2) ramDesc - describes the allocation of the segment when it is copied
 *    to RAM.  BaseOffset and enfOffset are offsets from virtual address
 *    0 (i.e. they are 32 bit virtual addresses).  For code that
 *    is executed directly from flash, only the BSS section is allocated
 *    here and code relocation referencing the BSS section is relocated using
 *    these offsets while other code is relocated using the offsets in
 *    flashDesc.  For code that is copied to RAM, and all relocation is
 *    performed relative to these offsets.
 *
 * 3) imageDesc - describes the allocation of the segment in the flash memory
 *    image.  BaseOffset and endOffset are offsets from the beginning of the
 *    image.
 */


/************
 * buildFlash	Process user arguments and build a flash image
 */
int
buildFlash(FbArgs_t* args){
  FbState_t fbs;
  /*
   * Initialize the flash image and state block
   * Note that we align the image on flash page boundries to simplify
   *    alignment checks during image building.
   */
  fbs.end = fbs.image = (char*)memalign(args->flashPageSize,args->flashSize);
  memset(fbs.image,0,args->flashSize);
  fbs.args = args;

  for (fbs.fileIndex=0;
       fbs.fileIndex< fbs.args->fileCount;
       fbs.fileIndex++)
    if (loadFile(&fbs))
      return 1;

  return writeFiles(&fbs);
}


/**********
 * loadFile	load one file into the flash image
 */
int
loadFile(FbState_t* fbs){
  FbFile_t* file = &fbs->args->files[fbs->fileIndex];
  int       sts;

  /* Initialize the current flash segment */
  if (sts = initSegment(fbs))
    return sts;

  /* Load file */
  if (file->segmentType & FS_CODE)
    sts = loadElfFile(fbs);
  else
    sts = loadDataFile(fbs);
  if (sts)
    return sts;

  /* Finish the current flash segment */
  finishSegment(fbs);
  return 0;
}



/****************
 * writeImageFile	Write out an image of the FLASH PROM
 */
void
writeImageFile(FbState_t* fbs){
  int fd = open(fbs->args->imageFileName,O_WRONLY|O_CREAT,
		S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
  int len = fbs->end - fbs->image;

  if (fd<0){
    printf(" unable to open image file %s \n", fbs->args->imageFileName);
    return;
  }

  if (write(fd,fbs->image,len)!=len)
    printf(" write error while writing file %s, %s\n",
	   fbs->args->imageFileName,
	   strerror(errno));
  close(fd);
}


/************
 * writeFiles	Write image to files.
 */
int
writeFiles(FbState_t* fbs){
  if (fbs->args->imageFileName)
    writeImageFile(fbs);
  if (fbs->args->hexFileName)
    writeHexFile(fbs);
  if (fbs->args->elfFileName)
    writeElfFile(fbs);
  return 0;
}


/**************
 * loadDataFile		Copy a data file into the segment
 */
int
loadDataFile(FbState_t* fbs){
  FbFile_t*     file = CURRENT_FILE(fbs);
  int           fd;
  int           sts = 0;
  int           len;
  int           maxData;

  /* Open the data file */
  if ((fd=open(file->fileName,O_RDONLY))<0){
    printf(" unable to open file %s, %s\n", file->fileName,strerror(errno));
    return 1;
  }

  /* Check and reject compressed data */
  if (file->segmentType & FS_COMPRESS){
    printf(" compressed data segments not supported yet\n");
    sts = 1;
    goto exit;
  }

  /*
   * Set maxData to the space remaining in the segment minus enough to 
   * allow rounding up to the nearest long and adding a segment checksum.
   * Then read in upto maxData bytes.
   */
  maxData = fbs->args->flashSize - file->imageDesc.endOffset -8;
  len = read(fd,fbs->image+file->imageDesc.endOffset,maxData);

  /* If we actually read maxData bytes, the file is too big. */
  if (len==maxData){
    printf(" data file %s is too long\n", file->fileName);
    sts = 1;
    goto exit;
  }
  len = (len +3) & ~3;

  /* Update the descriptors */
  file->imageDesc.endOffset += len;
  file->flashDesc.endOffset += len;

 exit:
  close(fd);
  return sts;
}



/*************
 * initSegment	Initializes a flash segment header
 */
int
initSegment(FbState_t* fbs){
  FbFile_t*     file = CURRENT_FILE(fbs);
  FlashSegment* seg;

  /* Initialize the descriptors */
  file->flashDesc.baseOffset = file->flashDesc.endOffset =
    fbs->args->flashVaddr + IMAGE_LENGTH(fbs);
  file->ramDesc.baseOffset = file->ramDesc.endOffset =
    file->ramAddress;
  file->imageDesc.baseOffset = file->imageDesc.endOffset =
    fbs->end - fbs->image;

  /* Construct and initialize a segment header in the flash image area */
  seg = (FlashSegment*)(fbs->image+IMAGE_LENGTH(fbs));

  /* Announce segment */
  printf("Segment: %-12s Version: %s, Header length: 0x%0x\n",
	 file->segmentName, file->segmentVersion, hdrSize(seg));

  return 0;
}


/***************
 * finishSegment
 *
 * Add the checksums and then update fbs->segment and fbs->end to
 * point to the next flash page.
 */
int
finishSegment(FbState_t* fbs){
  FbFile_t*     file  = CURRENT_FILE(fbs);
  FlashSegment* seg   = (FlashSegment*)(fbs->image+IMAGE_LENGTH(fbs));
  unsigned long align = fbs->args->flashPageSize-1;
  long*         fp;
  long          xsum;

  /*
   * Compute the segment length including the checksum.
   * Update the descriptors.
   */
  seg->magic = FLASH_SEGMENT_MAGICx;
  seg->nameLen = strlen(file->segmentName);
  memcpy(name(seg),file->segmentName,seg->nameLen);
  seg->vsnLen = strlen(file->segmentVersion);
  memcpy(version(seg),file->segmentVersion,seg->vsnLen);
  seg->segType = file->segmentType;
  seg->segLen = (file->imageDesc.endOffset - file->imageDesc.baseOffset)+4;
  assert((seg->segLen&3) == 0);
  file->imageDesc.endOffset += 4;
  file->flashDesc.endOffset += 4;

  /*
   * If segmentPadToLength is set, pad out the segment length.
   */
  if (file->segmentPadToLength){
    long delta = file->segmentPadToLength - seg->segLen;
    if (delta>0){
      if (fbs->args->verbose)
	printf(" padding segment to length 0x%08x [+0x%04x]\n",
	       file->segmentPadToLength,
	       delta);
      file->imageDesc.endOffset += delta;
      file->flashDesc.endOffset += delta;
      seg->segLen = file->segmentPadToLength;
    }
    if (delta<0){
      printf(" segment larger than pad request\n");
      return 1;
    }
  }

  /* Calculate checksums */
  xsum = 0;
  fp = (long*)seg;
  while (fp!=chksum(seg))
    xsum += *fp++;
  *fp++ = - xsum;
  xsum = 0;
  while (fp!=segChksum(seg))
    xsum += *fp++;
  *fp++ = -xsum;

  /* Check that our accounting is correct */
  assert((char*)fp == fbs->image+file->imageDesc.endOffset);

  /*
   * Update global image accounting
   */
  fbs->end = (char*)fp;
  fbs->end = (char*)((((unsigned long)fbs->end) + align) & ~align);

  /* Summarize where we put everything */
  if (fbs->args->verbose){
    printf(" image:\t\t\t\t%08x:%08x [%05x]\n",
	   file->imageDesc.baseOffset,
	   file->imageDesc.endOffset,
	   file->imageDesc.endOffset-file->imageDesc.baseOffset
	   );
    if (file->segmentType & FS_RAM)
      printf(" ram:\t\t\t\t%08x:%08x [%05x]\n",
	     file->ramDesc.baseOffset,
	     file->ramDesc.endOffset,
	     file->ramDesc.endOffset-file->ramDesc.baseOffset);
    else
      printf(" flash:\t\t\t\t%08x:%08x [%05x]\n",
	     file->flashDesc.baseOffset,
	     file->flashDesc.endOffset,
	     file->flashDesc.endOffset-file->flashDesc.baseOffset);
  }
}




