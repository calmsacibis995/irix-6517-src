/*
 * segment.c - 
 *
 */

#include <stdio.h>
#include "segment.h"

/************************************************************************/
/* Segment Header Functions						*/
/************************************************************************/

/*
 * Function:	checkSegmentHeader
 *
 * Parameters:	fp		The input file stream
 *
 * Description:	This function takes a pointer to a file, which then
 *		proceeds to make a list of byte offsets for the
 *		various segments.
 *
 *		Note: This function does not check that the header
 *		      information is correct.
 *
 */

int
checkSegmentHeader(FILE *fp, SegmentInfoPtr s)
{
   char	resByte;

   s->HeaderOffset = ftell(fp);

   /* Read Header Data */

   fread(&s->Reserved1, sizeof(unsigned int), 1, fp);
   fread(&s->Reserved2, sizeof(unsigned int), 1, fp);
   fread(&s->SegmentMagic, sizeof(unsigned int), 1, fp);
   fread(&s->SegmentLength, sizeof(unsigned int), 1, fp);

   fread(&s->NameLength, sizeof(char), 1, fp);
   fread(&s->VersionLength, sizeof(char), 1, fp);
   fread(&s->SegmentType, sizeof(char), 1, fp);
   fread(&resByte, sizeof(char), 1, fp);

   s->SegmentName = (char *) malloc(sizeof(char)*(s->NameLength+1));
   fread(s->SegmentName, sizeof(char), s->NameLength, fp);
   s->SegmentName[s->NameLength] = '\0';

   s->SegmentVersion = (char *) malloc(sizeof(char)*(s->VersionLength+1));
   fread(s->SegmentVersion, sizeof(char), s->VersionLength, fp);
   s->SegmentVersion[s->VersionLength] = '\0';

   fread(&s->HeaderChecksum, sizeof(unsigned int), 1, fp);

   /* Calculate Other Segment Information */

   s->HeaderLength = 23 + s->NameLength + s->VersionLength;
   s->DataOffset = s->HeaderOffset + s->HeaderLength;
   s->DataLength = s->SegmentLength - s->HeaderLength;

   /* Verify Header Data (check segment magic and header checksum) */

   if (s->SegmentMagic != 0x53484452)
      return -1;
}


/*
 * Function:	parseSegments
 *
 * Parameters:	fp
 *		segmentCount
 *		segmentArray
 *
 * Description:	
 *
 */

void
parseSegments(char *filename, int *segmentCount, SegmentInfoPtr **segmentArray)
{
   int i, sFlag;
   int byteOffset = 0;
   FILE *fp;
   SegmentInfoPtr s, headerBuffer[MAX_SEGMENTS];

   fp = fopen(filename, "r");

   *segmentCount = 0;
   headerBuffer[0] = (SegmentInfoPtr) malloc(sizeof(SegmentInfo));

   while (1)
   {
      if (feof(fp))
      {
	 free(headerBuffer[*segmentCount]);
	 break;
      }

      (void) fseek(fp, byteOffset, SEEK_SET);

      sFlag = checkSegmentHeader(fp, headerBuffer[*segmentCount]);

      if (sFlag > 0)
      {
	 *segmentCount += 1;
	 headerBuffer[*segmentCount] = (SegmentInfoPtr) malloc(sizeof(SegmentInfo));

	 if (*segmentCount > MAX_SEGMENTS)
	 {
	    fprintf(stderr, "Sorry, there are too many segments to parse.\n");
	    exit(1);
	 }
      }

      byteOffset += FLASH_PAGE_BOUNDARY;
   }

   *segmentArray = (SegmentInfoPtr *)
		  malloc(sizeof(SegmentInfoPtr)*(*segmentCount));

   for (i = 0; i < *segmentCount; i++)
      (*segmentArray)[i] = headerBuffer[i];

   fclose(fp);
}

/*
 * Function:	printMainMenu
 *
 * Parameters:
 *
 * Description:
 *
 */

void
printMainMenu(int segmentCount, SegmentInfoPtr *s)
{
   int choice, i;

   while (1)
   {
      fprintf(stdout, "Segment Decoder Main Menu\n");
      for (i = 0; i < segmentCount; i++)
	 fprintf(stdout, "%d\) View header for segment #%d\n", i+1, i+1);
      fprintf(stdout, "%d) Exit program\n\n", i+1);

      fprintf(stdout, "Enter Choice >");
      fflush(stdout);
      fflush(stdin);
      fscanf(stdin, "%d", &choice);

      if (choice == (segmentCount + 1))
	 return;
      else if ((choice > 0) && (choice <= segmentCount))
	 printSegmentHeader(s[choice-1]);
      else
      {
	 fprintf(stderr, "Invalid choice.  Try again.\n\n");
	 fflush(stderr);
      }
   }
}


/*
 * Function:	printSegmentHeader
 *
 * Parameters:	s
 *
 * Description:	
 *
 */

void
printSegmentHeader(SegmentInfoPtr s)
{
   fprintf(stdout, "\nSegment Header Information at %0.8x\n\n", s->HeaderOffset);
   fprintf(stdout, "Reserved (Word 1):  %0.8x\n", s->Reserved1);
   fprintf(stdout, "Reserved (Word 2):  %0.8x\n", s->Reserved2);
   fprintf(stdout, "Segment Magic:      %0.8x\n", s->SegmentMagic);
   fprintf(stdout, "Segment Length:     %d\n", s->SegmentLength);

   fprintf(stdout, "   Name Length:     %d\n", s->NameLength);
   fprintf(stdout, "   Version Length:  %d\n", s->VersionLength);
   fprintf(stdout, "   Segment Type:    %d\n", s->SegmentType);

   fprintf(stdout, "Segment Name:       %s\n", s->SegmentName);
   fprintf(stdout, "Segment Version:    %s\n", s->SegmentVersion);
   fprintf(stdout, "Segment Checksum:   %0.8x\n\n", s->HeaderChecksum);
   fflush(stdout);
}


/*
 * Function:	printSegmentHeaders
 *
 * Parameters:	sCount
 *		s
 *
 * Description:	
 *
 */

void
printSegmentHeaders(int sCount, SegmentInfoPtr *s)
{
   int i;

   for (i = 0; i < sCount; i++)
   {
      fprintf(stdout, "\nSegment Header Information at %0.8x\n\n", s[i]->HeaderOffset);
      fprintf(stdout, "Reserved (Word 1):  %0.8x\n", s[i]->Reserved1);
      fprintf(stdout, "Reserved (Word 2):  %0.8x\n", s[i]->Reserved2);
      fprintf(stdout, "Segment Magic:      %0.8x\n", s[i]->SegmentMagic);
      fprintf(stdout, "Segment Length:     %d\n", s[i]->SegmentLength);

      fprintf(stdout, "   Name Length:     %d\n", s[i]->NameLength);
      fprintf(stdout, "   Version Length:  %d\n", s[i]->VersionLength);
      fprintf(stdout, "   Segment Type:    %d\n", s[i]->SegmentType);

      fprintf(stdout, "Segment Name:       %s\n", s[i]->SegmentName);
      fprintf(stdout, "Segment Version:    %s\n", s[i]->SegmentVersion);
      fprintf(stdout, "Segment Checksum:   %0.8x\n\n", s[i]->HeaderChecksum);
      fflush(stdout);
   }
}
