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

   if (argc != 3)
   {
      fprintf(stderr, "Invalid number of arguments\n");
      exit(1);
   }

   parseSegments(argv[2], &segmentCount, &segmentArray);

   for (i = 0; i < segmentCount; i++)
   {
       if (strcmp(argv[1], segmentArray[i]->SegmentName) == 0) {
         fprintf(stdout, "%0.8x\n", segmentArray[i]->DataOffset);
	     fprintf(stdout, "%0.8x\n", segmentArray[i]->HeaderLength);
       }
   }
}
