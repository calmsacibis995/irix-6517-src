/*
 * main2.c - 
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

   if (argc != 2)
   {
      fprintf(stderr, "Invalid number of arguments\n");
      exit(1);
   }

   parseSegments(argv[1], &segmentCount, &segmentArray);

#ifdef DEBUG_SH
   for (i = 0; i < segmentCount; i++)
      printSegmentHeader(segmentArray[i]);
#endif

   printMainMenu(segmentCount, segmentArray);
}
