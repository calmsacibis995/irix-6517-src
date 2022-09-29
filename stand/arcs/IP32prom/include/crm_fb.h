/*
 * crm_fb.h - Header file for framebuffer conversion and addressing
 *
 */

/************************************************************************/
/* Framebuffer Information						*/
/* 									*/
/*    This section contains information relevant to accessing		*/
/*    information within a tile.					*/
/* 									*/
/************************************************************************/

/*******************/
/* Data Structures */
/*******************/

#if 0
/* So, in order to access (x,y) within a frame, we simply use the	*/
/* following convention:						*/
/*									*/
/*    crmTileYPixel[y]->XPixel_8[x]	(8-bit framebuffer)		*/
/*    crmTileYPixel[y]->XPixel_16[x]	(16-bit framebuffer)		*/
/*    crmTileYPixel[y]->XPixel_32[x]	(32-bit framebuffer)		*/

typedef union
{
   unsigned char XPixel_8[512];
   unsigned short XPixel_16[256];
   unsigned int XPixel_32[128];
} crmTileRow;

typedef crmTileRow crmTileYPixel[128];
typedef crmTileYPixel *crmTileYPixelPtr;

#else

typedef unsigned char crmTile[65536];
typedef crmTile *crmTilePtr;
typedef crmTilePtr *crmTileArray;

#endif


