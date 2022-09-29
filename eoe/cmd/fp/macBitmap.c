 
/* 
   (C) Copyright Digital Instrumentation Technology, Inc. 1990 - 1993
       All Rights Reserved
*/

/*

    TransferPro macBitMap.c - Functions that handle  Macintosh  Bitmaps

*/

#include "macPort.h"
#include "macSG.h"
#include "dm.h"

/*----------------- macReadBitmap -----------------------------------
   Reads the Volume Bitmap.
--------------------------------------------------------------------*/

int macReadBitmap (volume, vbmBlockCount)
struct m_volume *volume;
int vbmBlockCount;           /* size of bitmap in bytes */
{
    int retval = E_NONE;

    /* Read in the bitmap */

    if ( (retval = macReadBlockDevice ((void *)volume->device, 
                                          (char *)volume->vbm, 
                       volume->vib->drVBMSt+volume->vstartblk,
                                    vbmBlockCount)) != E_NONE )
       {
        ;
       }

    return (retval);
}

/*----------------- macWriteBitmap -----------------------------------
   Writes the Volume Bitmap.
--------------------------------------------------------------------*/

int macWriteBitmap (volume )
struct m_volume *volume;
{
    int retval = E_NONE;

    /* Write the bitmap */

    if ( (retval = macWriteBlockDevice ((void *)volume->device,
                                           (char *)volume->vbm, 
                        volume->vib->drVBMSt+volume->vstartblk, 
	           volume->vib->drAlBlSt-volume->vib->drVBMSt)) 
                                                    != E_NONE )
       {
        ;
       }

    return (retval);
}


/*--------------- macSetBit -----------------------------------------
   Sets the bit in the bit map corresponding to the given allocation 
   unit.
---------------------------------------------------------------------*/

void macSetBit (bitmap, unit)
char *bitmap;
int unit;
{
    int offset;
    char mask;

    offset = unit / 8;               /* Find byte in bitmap */
    mask = 0x80 >> (unit % 8);
    *(bitmap + offset) |= mask;      /* Set bit in this byte */
    
}

/*--------------- macClearBit -----------------------------------------
   Clears the bit in the bit map corresponding to the given allocation 
   unit.
---------------------------------------------------------------------*/

void macClearBit (bitmap, unit)
char *bitmap;
int unit;
{
    int offset;
    char mask;

    offset = unit / 8;               /* Find byte in bitmap */
    mask = ~(0x80 >> (unit % 8));
    *(bitmap + offset) &= mask;      /* Clear bit in this byte */
 
}


/*--------------- macGetBit -----------------------------------------
   Gets the bit in the bit map corresponding to the given allocation 
   unit.
---------------------------------------------------------------------*/

int macGetBit (bitmap, unit)
char *bitmap;
int unit;
{
    unsigned char bit;
    int offset;
    char mask;

    offset = unit / 8;                    /* Find byte in bitmap */
    mask = 0x80 >> (unit % 8);
    bit = *(bitmap+offset) & mask;      /* Set bit in this byte */
 
    return ( bit != 0 );
}


/*-- macInBitmapRec -----------
 *
 */

int macInBitmapRec( bitMap, index )
    struct bitmap_record *bitMap;
    int                   index;
   {
    unsigned char bit;
    int offset;
    char mask;

    offset = index / 8;                    /* Find byte in bitmaprec */
    mask = 0x80 >> (index % 8);
    bit = bitMap->bitmap[offset] & mask;      /* Set bit in this byte */
 
    return (bit);
   }

/*--- macSetBitmapRec ---------------------------------------
 */

void macSetBitmapRec( bitMap, index )
    struct bitmap_record  *bitMap;
    int index;
   {
    int offset;
    char mask;

    offset = index / 8;               /* Find byte in BitmapRec */
    mask = 0x80 >> (index % 8);
    bitMap->bitmap[offset] |= mask;    /* Set bit in this byte */
    
   }


/*--- macNextFreeNode ---------------------------------------
 *
 */
/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
   Someday ... change to search possible map nodes 
   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */

int macNextFreeNode( bitMap, length )
    struct bitmap_record  *bitMap;
    int length;
   {
    int count;

    for ( count = 1;
        count < length && macInBitmapRec(bitMap,count);
        count++ )
       {
        ;
       }
    if ( count == length )
       {
        count = UNASSIGNED_NODE;
       }

    return( count );
   }


/*--- macClearBitmapRec ---------------------------------------
 */

void macClearBitmapRec( bitMap, index)
    struct bitmap_record  *bitMap;
    int index;
   {
    unsigned char mask;

    mask = 0x80 >> (index % 8);
    bitMap->bitmap[index/8] &= ~mask; /* clr bit in this byte */

   }

