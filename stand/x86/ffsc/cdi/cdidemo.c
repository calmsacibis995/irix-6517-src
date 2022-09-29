#include <stdio.h>
#include <string.h>
#include "sysLib.h"
#include "pc.h"
#include "ioLib.h"
#include "config.h"

extern int sram_read();
extern int sram_write();
extern void enable_downloader();
extern void disable_downloader();

/*
** =======================================================================
**
**  Module Name     :   cdidemo.c
**
**  Project Name    :   SBC-FFSC
**
**  Module Synopsis :   demo task that can be spawned from the vxWorks shell
**
**  Author          :   Kenneth J. Smith
**
**  Creation Date   :   May 24, 1996
**
**  Caveats         :   This code was written quick and dirty.  Its purpose was
**                      to demonstrate how to link new routines into vxWorks.
**
**  NOTE            :   This routine was linked into the vxWorks image with
**                      the line MACH_EXTRA = cdidemo.o found in:
**                        \tornado\target\config\pc486\makefile
**                      This is where user defined routines can be added.
**
**  Compiler Env.   :   GNU C
**
**                    (c) Copyright Lines Unlimited
**                        1996 All Rights Reserved
**
** =======================================================================
*/

void cdiDemo ()
  {
    int i,length,offset,port,status,fd[6],device[9];
    char action='x', wstr[100], rstr[100], outStr[100];

    printf("CDI Demo Task\n\n");

    /*************************************************
	 * Create 6 tty's to interface with serial ports *
     *************************************************/

    for ( i = 0; i < NUM_TTY; i++ )
      {
        sprintf(device,"%s%d","/tyCo/",i);
        fd[i] = open(device,O_WRONLY,0666);
        if (fd[i] == ERROR)
          printf("fd ERROR\n");
        status = ioctl(fd[i],FIOBAUDRATE,9600);
        if (status == ERROR)
          printf("status ERROR\n");
      }

    /*************************
	 * 'q' will end the task *
     *************************/
                         
    while ( action != 'q')
      {
        if ( action != '\n' && action != '\0' )
          {
            printf("(r)ead from SRAM\n");
            printf("(w)rite to SRAM\n");
            printf("(e)nable boot loader\n");
            printf("(d)isable boot loader\n");
            printf("(p)ort output\n");
            printf("(q)uit\n\n");
            printf("==> ");
          }
        action = getchar();
        if ( action != '\n' && action != '\0' )
          printf("\n");

        /*******************************
	     * write data to a serial port *
         *******************************/

        if ( action == 'p' )
          {
            printf("Enter Serial Port#: ");
            scanf("%d",&port);
            printf("Enter string: ");
            scanf("%s",outStr);
            status = write(fd[port-1],outStr,strlen(outStr));
          }

        /*************************
	     * enable the downloader *
         *************************/

        else if ( action == 'e' )
          {
            printf("Enabling downloader.\n");
            enable_downloader();
          }

        /**************************
	     * disable the downloader *
         **************************/

        else if ( action == 'd' )
          {
            printf("Disabling downloader.\n");
            disable_downloader();
          }

        /************************************************
	     * read or write to the SRAM and display status *
         ************************************************/

        else if ( action == 'w' || action == 'r' )
          {
            if ( action == 'w' )
              {
                memset(wstr,'\0',100);
                printf("Enter string: ");
                scanf("%s",wstr);
                printf("Enter length: ");
                scanf("%d",&length);
                printf("Enter offset: ");
                scanf("%d",&offset);
                printf("\n");
                i = sram_write(wstr,length,offset);
              }	
            else
              {
                memset(rstr,'\0',100);
                printf("Enter length: ");
                scanf("%d",&length);
                printf("Enter offset: ");
                scanf("%d",&offset);
                printf("\n");
                i = sram_read(rstr,length,offset);
                printf("string = [%s]\n\n",rstr);
              }

            switch (i)
              {
                case -3 : printf("status = NOT_INITIALIZED\n"); break;
                case -2 : printf("status = CHECKSUM_FAILED\n"); break;
                case -1 : printf("status = ERROR\n"); break;
                case  1 : printf("status = VALID\n"); break;
                default : printf("status = UNKNOWN\n"); break;
              }
          }
      }
  }
