/*
 * main.c - 
 *
 */

#include <stdio.h>
#include "segment.h"


/*
 * Function:	main
 *
 * Parameters:	argc		The number of arguments
 *		argv		Pointer to the array of arguments
 *
 * Description:	
 *
 */

void main(int argc, char **argv)
{
   int i, segmentCount;
   FILE *inputFP;
   SegmentInfoPtr *segmentArray;
   unsigned int  length ; 

   if (argc != 3)
   {
      fprintf(stderr, "Invalid number of arguments\n");
      exit(1);
   }

   parseSegments(argv[2], &segmentCount, &segmentArray);

   for (i = 0; i < segmentCount; i++)
   {
       if (strcmp(argv[1], segmentArray[i]->SegmentName) == 0) {
           length = segmentArray[i]->DataOffset ;
           if (length > 0x4000) length = length - 0x4000 ;
           length = (length >> 2) + 1 ;
	       fprintf(stdout, "%d\n", length) ;
       }
   }
}
