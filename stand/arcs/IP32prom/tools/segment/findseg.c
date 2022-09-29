/*
 * main.c - 
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
   fprintf(stdout, "Usage: findseg <segmentName> <flashName>\n");
   fprintf(stdout, "   <segmentName>   The name of the segment you are\n");
   fprintf(stdout, "                   looking for (e.g. firmware, env)\n");
   fprintf(stdout, "   <flashName>     The name of the flash file      \n");
   fprintf(stdout, "\n");
   fprintf(stdout, "The output is the starting address of the data\n");
   fprintf(stdout, "offset from the beginning of the flash.\n");
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

   if (argc != 3)
   {
      printHelp();
      exit(1);
   }

   parseSegments(argv[2], &segmentCount, &segmentArray);

   for (i = 0; i < segmentCount; i++)
   {
      if (strcmp(argv[1], segmentArray[i]->SegmentName) == 0)
	 fprintf(stdout, "%0.8x\n", segmentArray[i]->DataOffset);
   }
}
