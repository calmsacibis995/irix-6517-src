/*
** =======================================================================
**  DERIVED FROM...
**
**  Header Name     :   if00sxa.cpp
**
**  Class Leniage   :   Storage
**
**  Class Name      :   IntelFlashFileStorage
**
**  Description     :   Methods for the IntelFlashFileStorage class
**
**  Author          :   Kenneth J. Smith
**
**  Creation Date   :   June 8, 1996
**
**  Compiler Env.   :   Borland C++
**
**                    (c) Copyright Lines Unlimited
**                        1996 All Rights Reserved
**
** BY JEFF BECKER, SGI
** =======================================================================
*/


#include <vxworks.h>
#include <iolib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysLib.h>
#include <timers.h>

#include "ffsc.h"
#include "flash.h"


/* struct timespec shortDelay = { 0, 10000 };*/
struct timespec shortDelay = { 0, 100000 };
/*struct timespec longDelay = { 0, 1600000 }; */
struct timespec longDelay = { 1, 0 };
/* struct timespec longDelay = { 1, 600000000 }; */

int socketArray[] = { 0x80,0x88,0x90,0x98,0xa0,0xa8,0xb0,0xb8,
			0x01,0x09,0x11,0x19,0x21,0x29,0x31,0x39  };
int flashArray[] = { 0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,
		       0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04  };

void initFlash(struct flashStruct *flash)
{

  flash->flashVPP = FLASH_ENABLE_DEFAULT;
  flash->deviceOpen = 0;
  flash->numDevices = NUMDEVICES;
  flash->position = 0;
  flash->blockSize = BLOCK_SIZE_32K;
  flash->deviceSocket = 1;
  flash->pageSelectPort = PAGE_SELECT;
  flash->VGAOn = RAM_VGA_ENABLE;
  flash->VGAOff = RAM_VGA_DISABLE;
  flash->windowSegment = 0x0a0000;
  flash->deviceSize = 2048;
  flash->SGISocket = socketArray;
  flash->SGIFlash  = flashArray;
}

/*char *
SGIRAMMap::OpenWindow( unsigned block, int access_mode ) */

/* 
 * Open the FLASH RAM window 
 */

char *openFlashWindow( struct flashStruct *flash,
		      unsigned block, int access_mode )

{

  unsigned long   offset;
  unsigned        page, pageRegValue, segment;
  char            *retPtr;

  /*
   **  Calculate the absolute offset from the block number and size
   */
  offset = (long)block * flash->blockSize;

  /*
   **  Range check the block requested
   */
  if( offset > ( flash->deviceSize * 1024L ))
    {
      /*
       **  The requested block excedes the
       **  device size.  Return NULL
       */
      retPtr = NULL;
      
    }
  else
    {

      /*
       **  Now calculate which 128K page this offset falls into.
       */
      page = offset / SIZE_128K;

      /*
       **  Calculate a segment that starts where the requested
       **  block begins.  Because the only valid block sizes
       **  are multiples of 16 they will always line up on
       **  a segment boundary.
       */

      offset %= SIZE_128K;

      /*      segment = (unsigned)(flash->windowSegment + (offset >> 4 ));*/
      segment = (unsigned)(flash->windowSegment + offset);

      /*
       **  The return value is a pointer to where the requested
       **  block begins within the page.
       */
/*      retPtr = (char *)MK_FP( segment, 0000 ); */
      retPtr = (char *) (segment);


      /*
       **  Disable VGA, thus enabeling access to the RAM/ROM sockets
       */
      sysOutByte( flash->VGAOff, 0x0ff );

      /*
       **  Set up the page register based on the device type
       **  specified
       */
      pageRegValue = flash->SGISocket[page];
      if ( access_mode == CMD_WRITE )
	pageRegValue |= flash->SGIFlash[page];

      sysOutByte( flash->pageSelectPort, pageRegValue );
    }

/*  delay( 10 ); */
  nanosleep(&shortDelay, NULL);
  return( retPtr );

}

/* 
 * Close the RAM/ROM window
 */

void
closeFlashWindow(struct flashStruct *flash)
{

  unsigned        pageRegValue;

  /*
   ** Set Vppl on the first flash device
   */
  pageRegValue = flash->SGISocket[0];
  sysOutByte( flash->pageSelectPort, pageRegValue );

  /*
   ** Set Vppl on the second flash device
   */

  pageRegValue = flash->SGISocket[8];
  sysOutByte( flash->pageSelectPort, pageRegValue );
	
  /*
   **  Enable VGA, thus disabeling access to the RAM/ROM sockets
   */
  sysOutByte( flash->VGAOn, 0x0ff );
/*  delay( 10 ); */
  nanosleep(&shortDelay, NULL);
  
}

/*
 * Clear a block of Flash RAM
 */

int clearFlashBlock( struct flashStruct *flash,
		     int deviceBlock,
		     int FeedbackFD )
{
	int i, j;
	volatile unsigned char *flashPtr;
	unsigned char statreg = 0;

	/* Reiterate until the retry count is exceeded or we succeed */
	for (i = 0;  i < ffscTune[TUNE_CLEAR_FLASH_RETRY];  ++i) {

		flashPtr = openFlashWindow( flash, deviceBlock, CMD_WRITE );
		*flashPtr = CMD_CLR_STATUS;
		nanosleep(&longDelay, NULL);

		if (FeedbackFD >= 0) {
			fdprintf(FeedbackFD, ".");
		}
		ffscMsgN(".");

		*flashPtr = CMD_ERASE_SET;
		nanosleep(&longDelay, NULL);

		if (FeedbackFD >= 0) {
			fdprintf(FeedbackFD, ".");
		}
		ffscMsgN(".");

		*flashPtr = CMD_ERASE_CONF;
		nanosleep(&longDelay, NULL);

		if (FeedbackFD >= 0) {
			fdprintf(FeedbackFD, ".");
		}
		ffscMsgN(".");

		/*
		**  Loop until status is READY 
		*/

		j = ffscTune[TUNE_CLEAR_FLASH_LOOPS];
		flashPtr = openFlashWindow( flash, deviceBlock, CMD_READ );
		while ( j-- ) {
			statreg = *flashPtr;
			if ( statreg & 0x80 ) {
				break;
			}
		}

		/* 
		**  Timeout - status register never reset to READY
		*/
		if ( !j ) {
			ffscMsg("Timed out waiting for deviceBlock %d",
				deviceBlock);
			return STATUS_REGISTER_ERROR;
		}

		/* 
		**  Make sure the current Intel block was erased
		*/

		if ( ! (statreg & 0x20) ) {

			/* The block was successfully erased - we are done */

			if (FeedbackFD >= 0) {
				fdprintf(FeedbackFD, ",");
			}
			ffscMsgN(",");

			return STATUS_OK;
		}

		/* The block erase failed - try again */
		ffscMsg("Got erase error for deviceBlock %d, %d loops",
			deviceBlock,
			ffscTune[TUNE_CLEAR_FLASH_LOOPS] - j);

		if (FeedbackFD >= 0) {
			fdprintf(FeedbackFD, "E");
		}
	}

	return STATUS_ERASE_ERROR;
}


/*
 * Set up for action.  Make sure that there's an i28F00xsa device out there. 
 */

int openFlash( struct flashStruct *flash, int mode, int FeedbackFD ) {

    char    *flashPtr1, *flashPtr2;

    unsigned char lowStatus, highStatus;

    int     k,
            flash2,
            retVal,
            deviceBlock;


    /*
    **  Initialize
    */
    flash2 = 1;       /* Assume flash 2 is present until proven otherwise */
    flash->deviceOpen = 1;
    retVal = STATUS_OK;  /* 0 */

    /*
    **  Device starts with position 0
    */
    flash->position = 0;

    /*
    **  Attempt to detect each device by toggling the flash voltage and
    **  monitoring the voltage
    **
    **  First attempt to get the status register with VPP disabled.
    **
    **
    */

    /*
    **  Calculate current block
    */

    deviceBlock = 0;

    /*
    **  Open the current block for READ (FLASH DEVICE #1)
    */
    flashPtr1 = openFlashWindow( flash, deviceBlock, CMD_READ );

    /*
    **  Get the intelligent identifier bytes ( 0x89 and 0xa2 )
    */
  
    *flashPtr1 = CMD_IDENT;
    lowStatus = *flashPtr1;
    highStatus = *(flashPtr1 + 1);

    /*
    **  Reset the device for READ
    */

    *flashPtr1 = CMD_READ;

    /*
    **  Make sure the correct identifier was read
    */

    if ( lowStatus != 0x89 || highStatus != 0xa2 )
      {
        retVal = STATUS_IDENTIFIER_ERROR;
        goto done;
      }

    /*
    **  Open the current block for READ (FLASH DEVICE #2)
    */
    flashPtr2 = openFlashWindow( flash, deviceBlock+32, CMD_READ );

    *flashPtr2 = CMD_IDENT;
    lowStatus = *flashPtr2;
    highStatus = *(flashPtr2 + 1);
    *flashPtr2 = CMD_READ;
    if ( lowStatus != 0x89 || highStatus != 0xa2 )
      flash2 = 0;  /* Flash 2 isn't on-board */

    /*
    **  The first flash device was found, we may proceed.
    **  The second device may or may not have been found.
    */

    /*
    **  If open for WRITE mode, clear all data form device(s)
    */

    if ( mode == WRITE_MODE ) {

	    /*
	    **  Say what we are about to do if desired
	    */

	    ffscMsg("Clearing flash storage");
	    if (FeedbackFD >= 0) {
		    fdprintf(FeedbackFD,
			     "\r\n"
			     "Clearing non-volatile storage\r\n"
			     "This will take approximately 100 seconds\r\n");
	    }

	    /*
	    **  The flash is divided into blocks ( 64K x 16 )
	    */

	    for ( k = 0; k < INTEL_BLOCKS; k++ ) {

		    /*
		    **  The CDI blocks are 32K so only write to every
		    **  other block which will clear Intel 64K blocks
		    */

		    deviceBlock = ( k * 2 );

		    retVal = clearFlashBlock(flash, deviceBlock, FeedbackFD);
		    if (retVal != STATUS_OK) {
			    ffscMsg("Unable to clear block %d/1", k);
			    goto done;
		    }

		    if (flash2) {
			    retVal = clearFlashBlock(flash,
						     deviceBlock+32,
						     FeedbackFD);
			    if (retVal != STATUS_OK) {
				    ffscMsg("Unable to clear block %d/2", k);
				    goto done;
			    }
		    }

	    }
    }

    done:
      closeFlashWindow(flash);
      return ( retVal );

}


/* 
 *close flash device
 */

int
closeFlash(struct flashStruct *flash)
{

  /*
   **  Make sure the device is closed
   */
  closeFlashWindow(flash);

  flash->deviceOpen = 0;

  return STATUS_OK;

}

int
readFlash( struct flashStruct *flash, char *data, unsigned length )
{

  unsigned    blockSize = flash->blockSize;
  unsigned    block, remain, offset;
  char    *devicePtr, *dataPtr =  data;
  int status;

  /*
   **  First, it's very possible that our current position
   **  is not on a block boundary.
   **
   **  We will determine what block we are in and how far
   **  from the boundary we are. If the requested data length
   **  fits within the rest of the block then everything's OK.
   **  Otherwise we will need to start a block reading loop
   */

  status = STATUS_OK;

  block = flash->position / blockSize;
  
  offset = flash->position % blockSize;
  
  remain = blockSize - offset;


  /*
   **  Open the RAM/ROM window
   */
  if ((devicePtr = openFlashWindow( flash, block, CMD_READ )))
    {
      /*
       **  move our pointer to the byte
       **  we want
       */
      devicePtr += offset;

      if( length <= remain )
	{
	  /*
	   **  We can get the data in one chunk!
	   **
	   **  Move the device data to the buffer
	   */
	  bcopy( devicePtr, dataPtr, length );

	  /*
	   **  Update the position
	   */
	  flash->position += length;

	  status = STATUS_OK;
	}
      else
	{
	  /*
	   **  The data is bigger than the block so
	   **  we need to bring it in block by block.
	   **
	   **  Move the rest of this block to the buffer
	   */
	  bcopy( devicePtr, dataPtr, remain );

	  /*
	   **  Update the position
	   */
            flash->position += remain;

	  /*
	   **  reduce our remaining length
	   */
	  length -= remain;

	  /*
	   **  Update data pointer
	   */
	  dataPtr += remain;
	  
	  /*
	   **  We are now lined up on a block boundary
	   **  so as long as the remaining length is
	   **  at least as long as a block we can just
	   **  eat blocks.
	   */

	  while( length >= blockSize )
	    {

	      block = flash->position / blockSize;
	      devicePtr = openFlashWindow( flash, block, CMD_READ );

	      /*
	       **  Read in the whole block
	       */
	      bcopy( devicePtr, dataPtr, blockSize );

	      /*
	       **  Update the position
	       */
	      flash->position += blockSize;

	      /*
	       **  reduce our remaining length
	       */
	      length -= blockSize;
	      
	      /*
	       **  Update data pointer
	       */
	      dataPtr += blockSize;
	      
	    }

	  /*
	   **  Our last bit of data did not fill a whole
	   **  block (if any data is left at all)
	   **  We are still on a block boundary so
	   **  just read in the rest of it
	   */

	  if( length )
	    {

	      block = flash->position / blockSize;
	      devicePtr = openFlashWindow( flash, block, CMD_READ );

	      /*
	       **  Read in the whole block
	       */
	      bcopy( devicePtr, dataPtr, length );

	      /*
	       **  Update the position
	       */
	      flash->position += length;

	      /*
	       **  Update data pointer
	       */
	      dataPtr += length;
	      
	    }
	}
    }
  else
    {
      /*
       **  Couldn't open the map window
       */
      status = STATUS_OPEN_WINDOW_ERROR;
      
    }

  /*
   **  Close the window down
   */
  closeFlashWindow(flash);

  return( status );

}

int
writeFlashBytes( char *writePtr, char *dataPtr, unsigned lng)
{

  int i,j,k,
  retry,
  retVal;
  unsigned char status;
  char *statReg;
  
  /*
   **  Initialize
   */
  
  retVal = WRITE_OK;      /* 0 */
  retry = RETRY_LIMIT;    /* 100 */
  statReg = writePtr;
  
  /*
   **  Copy each byte of the data string to the Flash ROM
   */
  
  for ( i = 0, k=0; i < lng;)
    {
      
      /*
       **  Clear the Status Register
       */
      
      *statReg = CMD_CLR_STATUS;
      
      /*
       **  Loop until the Write State Machine Status is READY (0x80)
       */
      
      j = LOOP_DOWN;  /* 10000 */
      while ( j-- )
	{
	  *statReg = CMD_CLR_STATUS;
	  *statReg = CMD_READ_STATUS;
	  status = *statReg;
	  if ( status & 0x80 )
	    break;
	}
      
      /* 
       **  The unable to clear the status register
       */
      
      if ( !j )
	{
	  retVal = CLEAR_STATUS_ERROR;
	  goto done;
	}
      
      /*
       **  Clear the Status Register
       */
      
      *statReg = CMD_CLR_STATUS;
      
      /*
       **  Byte write setup plus byte write
       */
      
      *writePtr = CMD_WRITE;
      *writePtr = *dataPtr;
      
      /*
       **  Loop until the Write State Machine Status is READY (0x80)
       */
      
      j = LOOP_DOWN;
      while ( j-- )
	{
	  *statReg = CMD_CLR_STATUS;
	  *statReg = CMD_READ_STATUS;
	  status = *statReg;
	  if ( status & 0x80 )
	    break;
	}
      
      /* 
       **  Unable to get a Write state machine status of READY (0x80)
       */
      
      if ( !j )
	{
	  retVal = WRITE_STATUS_READY_ERROR;
	  goto done;
	}
      
      /* 
       **  Byte Write status error
       */

      if ( status & 0x10 )
	retVal = WRITE_ERROR;
      
      /* 
       **  Byte Write status ok
       */

      else
	{
	  /* 
	   **  Make sure the data byte was stored properly
	   */

	  *statReg = CMD_READ;
	  
	  if ( *writePtr != *dataPtr )
	    {
	      retVal = DATA_NOT_EQUAL_ERROR;
	      if ( !(--retry) )
		goto done;
	    }
	  
	  /*
	   **  Byte written successfully, increment pointers & counters
	   */
	  
	  else
	    {
	      retry = LOOP_DOWN;  /* 10000 */
	      i++; k++;
	      writePtr++; dataPtr++;
	    }
	  
	}
      
      /*
       **  End FOR loop
       */
      
    }

 done:

  return ( retVal );

}

int
writeFlash( struct flashStruct *flash, char *data, unsigned length )
{

  unsigned    blockSize = flash->blockSize;
  unsigned    block, remain, offset;
  char    *devicePtr, *dataPtr = data, status;

  /*
   **  First, it's very possible that our current position
   **  is not on a block boundary.
   **
   **  We will determine what block we are in and how far
   **  from the boundary we are. If the requested data length
   **  fits within the rest of the block then everything's OK.
   **  Otherwise we will need to start a block reading loop
   */

  status = STATUS_OK;

  block = flash->position / blockSize;
  
  offset = flash->position % blockSize;
  
  remain = blockSize - offset;


  /*
   **  Open the RAM/ROM window
   */
  if ((devicePtr = openFlashWindow( flash, block, CMD_WRITE )))
    {
      /*
       **  move our pointer to the byte
       **  we want
       */
      devicePtr += offset;
      
      if( length <= remain )
	{
	  /*
	   **  We can get the data in one chunck!
	   **
	   **  Move the device data to the buffer
	   */

	  if ( (status = writeFlashBytes( devicePtr, dataPtr, length )) )
	    {
	      goto done;
	    }

	  /*
	   **  Update the position
	   */
	  flash->position += length;

	}
      else
	{
	  /*
	   **  The data is bigger than the block so
	   **  we need to bring it in block by block.
	   **
	   **  Move the rest of this block to the buffer
	   */

	  if ( (status = writeFlashBytes( devicePtr, dataPtr, length ) ) ) 
	  {
	    goto done;
	  }

	  /*
	   **  Update the position
	   */
	  flash->position += remain;

	  /*
	   **  reduce our remaining length
	   */
	  length -= remain;

	  /*
	   **  Update data pointer
	   */
	  dataPtr += remain;

	  /*
	   **  We are now lined up on a block boundary
	   **  so as long as the remaining length is
	   **  at least as long as a block we can just
	   **  eat blocks.
	   */

	  while( length >= blockSize )
	    {

	      block = flash->position / blockSize;
	      devicePtr = openFlashWindow( flash, block, CMD_WRITE );

	      /*
	       **  Read in the whole block
	       */

	      if ( (status = writeFlashBytes( devicePtr, dataPtr, length )) )
	      {
		goto done;
	      }
	      /*
	       **  Update the position
	       */
	      flash->position += blockSize;

	      /*
	       **  reduce our remaining length
	       */
	      length -= blockSize;

	      /*
	       **  Update data pointer
	       */
	      dataPtr += blockSize;

	    }

	  /*
	   **  Our last bit of data did not fill a whole
	   **  block (if any data is left at all)
	   **  We are still on a block boundary so
	   **  just read in the rest of it
	   */

	  if( length )
	    {

	      block = flash->position / blockSize;
	      devicePtr = openFlashWindow( flash, block, CMD_WRITE );

	      /*
	       **  Read in the whole block
	       */
              
	      if ( (status = writeFlashBytes( devicePtr, dataPtr, length )) )
		{
		  goto done;
		}

	      /*
	       **  Update the position
	       */
	      flash->position += length;

	      /*
	       **  Update data pointer
	       */
	      dataPtr += length;

	    }
	}
    }
  else
    {
      /*
       **  Couldn't open the map window
       */
      status = STATUS_OPEN_WINDOW_ERROR;
      
    }
  
 done:
  
  /*
   **  Close the window down
   */

  closeFlashWindow(flash);

  return( status );

}

/* flashtest: a simple command to see if flash is doing writes properly -
   numK <= 64 should work */

void flashtest(int numK)
{
  int bufsize, i, result;
  int offset;
  struct flashStruct slashTheFlash;
  unsigned char *stuff;

  bufsize = 1024 * numK;
  ffscMsg("numK is %d, bufsize is %d", numK, bufsize);
  stuff = malloc(bufsize);
  for (i=0; i<bufsize; i++)
    stuff[i] = i%201;
  initFlash(&slashTheFlash);
  result = openFlash(&slashTheFlash, WRITE_MODE, -1);
  ffscMsg("openFlash returned %d", result);
  if (result >= 0 ) {
	  for (offset = 0;  offset < bufsize;  offset += 128) {
	      result = writeFlash(&slashTheFlash, stuff+offset, 128);
	      ffscMsg("Offset=%d  Position=%ld  Result=%d",
		      offset, slashTheFlash.position, result);
	      if (result < 0) {
		      return;
	      }
	  }
  }
  if (result >= 0 ) {
    slashTheFlash.position = 0;
    for (offset = 0;  offset < bufsize &&  result >= 0;  offset+=128) {
	    result = readFlash(&slashTheFlash, stuff+offset, 128);
	    ffscMsg("Offset=%d  Position=%ld  Result=%d",
		    offset, slashTheFlash.position, result);
    }

    if (result >= 0) {
      for (i=0; i<bufsize; i++) {
	if (stuff[i] != i%201) {
	  ffscMsg("Bad read at i=%d", i);
	  return;
	}
      }
      ffscMsg("Buffer reads as written!");
    }
  }
}
