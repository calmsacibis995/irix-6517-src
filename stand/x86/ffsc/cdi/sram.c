/*
** =======================================================================
**
**  Module Name     :   sram.c
**
**  Project Name    :   SBC-FFSC
**
**  Module Synopsis :   Driver routines for 8K SRAM.
**
**  Author          :   Kenneth J. Smith
**
**  Creation Date   :   May 24, 1996
**
**  Compiler Env.   :   GNU C
**
**                    (c) Copyright Lines Unlimited
**                        1996 All Rights Reserved
**
** =======================================================================
*/

#include <stdio.h>
#include "sram.h"
#include "syslib.h"
#include "intlib.h"

/*****************
 *  Prototypes.  *
 *****************/

int sram_on ( );
void sram_off ( );


/*
** =======================================================================
**
**  name     :   sram_read()
**
**  synopsis :   This routine copies the contents of non-volatile memory
**               into a specified string.  The string is not terminated.
**
**  globals  :   none
**
**  returns  :   int retVal
**
**                 -3 NOT_INITIALIZED  The SRAM has not been written to.
**                 -2 CHECKSUM_FAILED  Corrupted data.
**                 -1 ERR              Read past the 8K boundary.
**                  1 VALID_READ       Successful read.
**
** =======================================================================
*/

int sram_read ( char *str, int strLen, int offSet )
  {

    unsigned int sum;
    int i,j, retVal, lockKey;
    static int first_read = 1;
    sram_data *sram;

    /********************* 
     *  Initialization.  *
     *********************/

       sum = 0;
       retVal = 0;


    /*************************************************
     *  Enable SRAM, video memory will be disabled.  *
     *************************************************/

       lockKey = sram_on ( );

    /******************************************************
     *  Set data pointer to SRAM memory address 0xA2000.  * 
     *  (Base address 0xA000 + 8K SRAM offset 0x2000.)    *
     ******************************************************/

       sram = (struct ram_data *) SRAM_START;

    /****************************************************
     *  Validate the signature 0xAA55 in the first two  *
     *  bytes starting at memory address 0xA2000.       *
     ****************************************************/

       if ( sram->signature != (short)SIGNATURE )

       /***********************************************************
        *  Tell the user that the sram has not been initialized.  *
        ***********************************************************/

          retVal = NOT_INITIALIZED;

       else

         {

       /*******************************************************
        *  We have a valid signature.  If this is the first   *
        *  call to sram_read(), evaluate the checksum.  The   *
        *  checksum data area is 8188 bytes starting at addr  *
        *  0xA2004.                                           * 
        *******************************************************/
              
          if ( first_read )
				  
            {

              for ( i = sizeof( struct ram_data ); i < BUFF_SIZE; i++ )
                sum += *((unsigned char *)sram+i);

              sum = sum & 0x0000ffff;
              sum += sram->checksum;

             /****************************************************
              *  If the sum is not valid (non zero),  Clear the  *
              *  entire sram and return error code (-1).         *
              ****************************************************/

                if ( sum )
                  {
                    for ( i = 0; i < BUFF_SIZE; i++ )
                      *((char *)sram+i) = 0;
                    retVal = CHECKSUM_FAILED;
                  }

             /*********************************************************
              *  If the sum is valid (zero), set the first_read flag  *
              *  to zero so that we do not check the sum during       *
              *  subsequent reads.                                    *
              *********************************************************/

                else
                  first_read = 0;

           /*********************
            *  End first read.  *
            *********************/

              }

        /******************************************************
         *  If the checksum has been verified, retVal will    *
         *  still have a value of 0 from the initialization.  *
         ******************************************************/

           if ( !retVal )

             {

             /********************************************************
              *  Read the specified number of bytes starting at the  *  
              *  base address + user defined offset + header bytes.  *
              ********************************************************/

                for ( i = 0, j = offSet + sizeof( struct ram_data );
                      i < strLen; i++, j++ )
                  *(str+i) = *((char *)sram+j);

             /***********************************************
              *  If memory outside the sram address range   *
              *  was accessed, return an error code of -1.  *
              ************************************************/

                if ((offSet + strLen) > BUFF_SIZE)
                  retVal = ERR;

             /******************************
              *  Return valid read flag.  *
              ******************************/

                else
                  retVal = VALID_READ;

          /*********************
           *  End if !retVal.  *
           *********************/

             }

         }

    /********************************************************
     *  Re-enable the video memory, SRAM will be disabled.  *
     ********************************************************/

       sram_off ( lockKey );

    /*********************************
     *  Pass back the return value.  *
     *********************************/

       return ( retVal );

  }


/*
** =======================================================================
**
**  name     :   sram_write()
**
**  synopsis :   This routine copies a specified string into non-volatile
**               RAM.
**
**  globals  :   none
**
**  returns  :   int retVal
**
**                 -1 ERR              Wrote past the 8K boundary.
**                  1 VALID_WRITE      Successful write.
**
** =======================================================================
*/

int sram_write ( char *str, int strLen, int offSet )
  {

    int i,j, retVal, lockKey;
    unsigned int sum;
    sram_data *sram;

    /********************* 
     *  Initialization.  *
     *********************/

       sum = 0;
       retVal = 0;

    /*************************************************
     *  Enable SRAM, video memory will be disabled.  *
     *************************************************/

       lockKey = sram_on ( );

    /******************************************************
     *  Set data pointer to SRAM memory address 0xA2000.  * 
     *  (Base address 0xA000 + 8K SRAM offset 0x2000.)    *
     ******************************************************/

       sram = (struct ram_data *) SRAM_START;

    /************************************************************
     *  If a valid signature is not found, then this is either  *
     *  the first write, or sram_read() could not validate the  *
     *  checksum.  In either case, reset the header data.       *
     ************************************************************/

       if ( sram->signature != (short)SIGNATURE )
         {
           sram->signature = (short)SIGNATURE;
           sram->reserve = 0;
           sram->loader_flag = 0;
         }

    /*********************************************************
     *  Write the specified number of bytes starting at the  *  
     *  base address + user defined offset + header bytes.   *
     *********************************************************/

       for ( i = 0,j = offSet + sizeof( struct ram_data ); i < strLen; i++,j++ )
         *((char *)sram+j) = *(str+i);

    /*******************************************************
     *  The checksum is always calculated so that it can   *
     *  be validated during the first read after boot-up.  *
     *******************************************************/

       sram->checksum = 0;
       for ( i = (sizeof( struct ram_data ) - sizeof ( sram->checksum ));
             i < BUFF_SIZE; i++ )
         sum += *((unsigned char *)sram+i);
       sram->checksum = 0 - (sum & 0x0000ffff);

    /***********************************************
     *  If memory outside the sram address range   *
     *  was accessed, return an error code of -1.  *
     ***********************************************/

       if ((offSet + strLen) > BUFF_SIZE)
         retVal = ERR;

    /******************************
     *  Return valid write flag.  *
     ******************************/

       else
         retVal = VALID_WRITE;

    /********************************************************
     *  Re-enable the video memory, SRAM will be disabled.  *
     ********************************************************/

       sram_off ( lockKey );

    /*********************************
     *  Pass back the return value.  *
     *********************************/

       return ( retVal );

  }


/*
** =======================================================================
**
**  name     :   sram_on()
**
**  synopsis :   This routine disables interrupts and video memory thus
**               enabling SRAM which is set to the first bank.
**
**  globals  :   none
**
**  returns  :   int lockKey;  Used by sram_off() when enabling interrupts.
**
** =======================================================================
*/

int sram_on ( )
  {

    int lockKey;

    /*************************
     *  Lock out interrupts  *
     *************************/

     lockKey = intLock ();

    /*********************************
     *  Disable video / enable SRAM  *
     *********************************/

     sysOutByte ( VGA_DISABLE, WRITE_STR );  

    /*********************************************
     *  Select first SRAM bank  (zero relative)  *
     *********************************************/

     sysOutByte ( PAGE_SELECT, PAGE_0 );  

    /******************************
     *  Return the lockKey value  *
     ******************************/

	 return ( lockKey );

  }


/*
** =======================================================================
**
**  name     :   sram_off()
**
**  synopsis :   This routine disables SRAM and enables video memory
**               along with interrupts.         
**
**  globals  :   none
**
**  returns  :   none
**
** =======================================================================
*/

void sram_off ( int lockKey )
  {

    /**********************************
    *  Enable video / disable SRAM.   *
    ***********************************/

    sysOutByte ( VGA_ENABLE, WRITE_STR );  

    /************************
    *  Enable interrupts.   *
    *************************/

    intUnlock ( lockKey );

  }


/*
** =======================================================================
**
**  name     :   enable_downloader()
**
**  synopsis :   Set bit 7 of I/P port 71h.  When the board re-boots,
**               the bios routines will see the bit and run the serial
**               downloader instead of vxWorks.  This will allow the user
**               to store a new vxWorks image in Flash memory.
**
**  globals  :   none
**
**  returns  :   none
**
** =======================================================================
*/

void enable_downloader ()
  {

    unsigned char byt;

    /************************************
     *  Access I/O port 0x70 with 0x7e  *
     ************************************/

       sysOutByte( PORT_70, 0x7e );

    /*****************************
     *  Read current byte value  *
     *****************************/

       byt = sysInByte( PORT_71 );  

    /************************************
     *  Access I/O port 0x70 with 0x7e  *
     ************************************/

       sysOutByte( PORT_70, 0x7e );

   /********************************
    *  Set the load_flag to true.  *
    ********************************/

       sysOutByte( PORT_71, byt | 0x80 );

  }


/*
** =======================================================================
**
**  name     :   disable_downloader()
**
**  synopsis :   Unset bit 7 at I/O port 71h so that the a vxWorks image
**               will boot instead of the serial downloader.
**   
**
**  globals  :   none
**
**  returns  :   none
**
** =======================================================================
*/

void disable_downloader ()
  {

    unsigned char byt;

    /************************************
     *  Access I/O port 0x70 with 0x7e  *
     ************************************/

       sysOutByte( PORT_70, 0x7e );

    /*****************************
     *  Read current byte value  *
     *****************************/

       byt = sysInByte( PORT_71 );  

    /************************************
     *  Access I/O port 0x70 with 0x7e  *
     ************************************/

       sysOutByte( PORT_70, 0x7e );

   /*********************************
    *  Set the load_flag to flase.  *
    *********************************/

       sysOutByte( PORT_71, byt & 0x7f );

  }
