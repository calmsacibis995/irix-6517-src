/*
 * segment.h - Header file for PROM image segments
 *
 */

/****************************/
/* Preprocessor Definitions */
/****************************/
#define	SH_SEG_TYPE_MASK		0x00ff0000
#define SH_SEG_TYPE_SHIFT		16
#define SH_VERSION_LENGTH_MASK		0x0000ff00
#define SH_VERSION_LENGTH_SHIFT		8
#define SH_NAME_LENGTH_MASK		0x000000ff
#define SH_NAME_LENGTH_SHIFT		0

#define FLASH_PAGE_BOUNDARY		512

#define MAX_SEGMENTS			100


/*******************/
/* Data Structures */
/*******************/

typedef struct segment_info
{
   /* Contents of Header */

   unsigned int Reserved1;
   unsigned int Reserved2;
   unsigned int SegmentMagic;
   unsigned int SegmentLength;
   char NameLength;
   char VersionLength;
   char SegmentType;
   char *SegmentName;
   char *SegmentVersion;
   unsigned int HeaderChecksum;

   /* Other Segment Information */

   unsigned int HeaderOffset;
   unsigned int HeaderLength;
   unsigned int DataOffset;
   unsigned int DataLength;

} SegmentInfo, *SegmentInfoPtr;


/***********************/
/* Function Prototypes */
/***********************/

int checkSegmentHeader(FILE *fp, SegmentInfoPtr s);
void parseSegments(char *filename, int *segmentCount, SegmentInfoPtr **segmentArray);
void printSegmentHeader(SegmentInfoPtr s);
void printSegmentHeaders(int sCount, SegmentInfoPtr *s);
