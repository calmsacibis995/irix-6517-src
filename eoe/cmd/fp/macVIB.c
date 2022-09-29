/* 
   (C) Copyright Digital Instrumentation Technology, Inc., 1990 - 1993 
       All Rights Reserved
*/

/*
     DIT TransferPro macVIB.c  -  Functions for managing the 
                                  Macintosh VIB structure.
*/

#include "macPort.h"
#include "macSG.h"
#include "dm.h"
#include "DiskDevice.h"

/*------------------- macReadVIB -------------------------------------
    Reads the VIB.
---------------------------------------------------------------------*/

int macReadVIB (volume)
struct m_volume *volume;
{
    int retval = E_NONE;
    char buffer[512];

    if( (retval = macReadBlockDevice ((void *)volume->device, buffer, 
            VIB_START+volume->vstartblk, VIB_BLOCKCNT)) != E_NONE )
       {
        retval = macReadBlockDevice ((void *)volume->device, buffer,
                              au_to_partition_block(volume->vib,
                                        volume->vib->drNmAlblks,
                                               volume->vstartblk),
                                                      VIB_BLOCKCNT);
       }
    if (retval == E_NONE)
        macUnpackVIB (volume->vib, buffer);

    return (retval);
}

/*------------------- macWriteVIB -------------------------------------
    Writes the VIB.
---------------------------------------------------------------------*/

int macWriteVIB (volume)
struct m_volume *volume;
{
    int retval = E_NONE;
    char buffer[512];
    struct DiskDevice *diskdevice = volume->device;

    macPackVIB (volume->vib, buffer);

    /* Write first VIB */
    retval = macWriteBlockDevice ((void *)volume->device, buffer, 
                   VIB_START+volume->vstartblk, VIB_BLOCKCNT);


    /* If that goes all right, write second VIB */
    if ( retval == E_NONE )
       {
        if ( diskdevice->deviceBlockCount == 0 )
           {
            retval = macWriteBlockDevice ((void *)volume->device, buffer,
               au_to_partition_block(volume->vib,volume->vib->drNmAlblks,
                                        volume->vstartblk), VIB_BLOCKCNT);
           }
        else
           {
            retval = macWriteBlockDevice ((void *)volume->device, buffer,
                    diskdevice->deviceBlockCount - 2, VIB_BLOCKCNT);
           }
       }
    
    return (retval);
}
