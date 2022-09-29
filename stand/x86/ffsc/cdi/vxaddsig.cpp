//
// vxaddsig.cpp - Add CDI signature to vxworks header
//
// usage: vxaddsig <filename>
//
// signature: 5Ah A5h A5h 5Ah at offset 0x18-0x1A in file
//
// Bill Lovegrove
//
// V1.0.0 June 10, 1996
//

#include <stdio.h>
#include <stdlib.h>

main(int argc, char *argv[])
{
   FILE              *fp;
   int               result;
   unsigned char     sigbuf[4];
   size_t            size;

   printf("vxaddsig V1.0.0 - Add signature to bootable VxWorks file\n");
   printf("(C) Copyright 1996, Lines Unlimited\n\n");

   // Check for filename argument
   if ( argc != 2 )
      {
      printf("usage: vxaddsig <filename>\n");
      exit(1);
      }

   // Open the file
   fp = fopen( argv[1], "r+b" );
   if ( fp == NULL )
      {
      printf("error: Can't open file %s.\n", argv[1]);
      exit(2);
      }

   // Locate and check existing signature
   result = fseek( fp, 0x18, SEEK_SET );
   if ( result != 0 )
      {
      printf("error: Can't locate signature position in file.\n");
      exit(3);
      }
   size = fread( sigbuf, sizeof(unsigned char), 4, fp);
   if ( size != 4 )
      {
      printf("error: Unable to read signature field (file may not be VxWorks).\n");
      exit(4);
      }
   if ( (sigbuf[0] == 0x5A) && (sigbuf[1] == 0xA5) && (sigbuf[2] == 0xA5)
	&& (sigbuf[3] == 0x5A) )
      {
      printf("warning: Signature already present.\n");
      exit(0);
      }
   if ( (sigbuf[0] != 0) || (sigbuf[1] != 0) || (sigbuf[2] != 0)
	|| (sigbuf[3] != 0) )
      {
      printf("error: Signature not blank.\n");
      exit(5);
      }

   // Write the signature
   sigbuf[0] = 0x5A;
   sigbuf[1] = 0xA5;
   sigbuf[2] = 0xA5;
   sigbuf[3] = 0x5A;
   result = fseek( fp, 0x18, SEEK_SET );
   size = fwrite( sigbuf, sizeof(char), 4, fp);
   if ( size != 4 )
      {
      printf("error: Unable to write signature field.\n");
      exit(6);
      }

   // Close file
   result = fclose( fp );
   if ( result != 0 )
      {
      printf("error: Can't close the file.\n");
      exit(7);
      }

   // Exit normally
   return 0;
}

