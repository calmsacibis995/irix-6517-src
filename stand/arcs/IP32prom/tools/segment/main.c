/*
 * main2.c - 
 *
 */

#include <stdio.h>
#include "segment.h"

/*
 * Function:	printHelp
 *
 * Parameters:	(None)
 *
 * Description:
 *
 */

void
printHelp(void)
{
   fprintf(stdout, "Usage: main <flashName>\n");
   fprintf(stdout, "   <flashName>   The name of the flash file\n");
   fprintf(stdout, "\n");
   fprintf(stdout, "This function parses the flash file and presents\n");
   fprintf(stdout, "a menu which allows the user to look at the various\n");
   fprintf(stdout, "segment headers.\n");
   fflush(stdout);
}

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
      printHelp();
      exit(1);
   }

   parseSegments(argv[1], &segmentCount, &segmentArray);

#ifdef DEBUG_SH
   for (i = 0; i < segmentCount; i++)
      printSegmentHeader(segmentArray[i]);
#endif

   printMainMenu(segmentCount, segmentArray);
}

