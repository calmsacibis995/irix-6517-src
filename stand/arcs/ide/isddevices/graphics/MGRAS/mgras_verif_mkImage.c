/*
** Copyright 1993, Silicon Graphics, Inc.
** All Rights Reserved.
**
** This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
** the contents of this file may not be disclosed to third parties, copied or
** duplicated in any form, in whole or in part, without the prior written
** permission of Silicon Graphics, Inc.
**
** RESTRICTED RIGHTS LEGEND:
** Use, duplication or disclosure by the Government is subject to restrictions
** as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
** and Computer Software clause at DFARS 252.227-7013, and/or in similar or
** successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
** rights reserved under the Copyright Laws of the United States.
**
*/
#include <sys/types.h>
#include <math.h>
#include "uif.h"
#include "libsc.h"
#include "sys/re4_tex.h"
#include <sgidefs.h>
#include <sys/mgrashw.h>
#include "mgras_diag.h"
#include "mgras_hq3.h"
#include "ide_msg.h"

/* Description:
** The twelve framebuffer config are derived from
** {L,LA,RGB,RGBA}:{12,8,4} component:bit combinations.
**
** An image generation function exists for each of these
** combinations which fills the image buffer with grey ramps.
**
** The memory allocation of the void *image buffer is assumed
** to be as follows (in bytes):
**
** L   : 12 -  X * Y * 2
**     :  8 -  X * Y 
**     :  4 -  X * Y / 2
**
** LA  : 12 -  X * Y * 4
**     :  8 -  X * Y * 2
**     :  4 -  X * Y
**		    
** RGB : 12 -  X * Y * 6
**     :  8 -  X * Y * 3
**     :  4 -  X * Y * 2
**		    
** RGBA: 12 -  X * Y * 8
**     :  8 -  X * Y * 4
**     :  4 -  X * Y * 2
**
*/

__uint32_t tex_mddma_dma_4_array[12] = {
	0x14141f25,0x252e3535,
	0x3d46464b,0x56565a67,
	0x67697777,0x78888887,
	0x989896a9,0xa9a5b9b9,
	0xb4cacac2,0xdadad1eb,
	0xebe0fbfb,0xef040410
};

__uint32_t tex_lut4d_dma_4_array[16] = {
	0x17102040,0x26102040,
	0x35102040,0x44102040,
	0x53102040,0x62102040,
	0x71102040,0x80102040,
	0x8f102040,0x9e102040,
	0xad102040,0xbc102040,
	0xcb102040,0xda102040,
	0xe9102040,0x08102040
};

__uint32_t fatalError(char *msg)
{
  msg_printf(VRB,"%s\n",msg);
  exit(1);
}

/* L */
GLushort *L12(GLushort *shortdata, GLushort *in)
{

  if (*in == 0xfff)
    *in = 0;
  else
    *in += 0x111;

  *(shortdata++) = (*in << 4);

  return(shortdata);
}

GLubyte *L8(GLubyte *bytedata, GLubyte *in)
{
  if (*in == 0xff) 
    *in = 0;
  else
    *in += 0x11;

  *(bytedata++) = *in;

  return(bytedata);
}
GLubyte *L4(GLubyte *bytedata, GLubyte *in)
{
  GLubyte dblb;

  if (*in == 0xf) 
    *in = 0;
  else
    *in += 0x1;
  /* two texels per byte - pack 'em */
  dblb = (*in << 4) | *in & 0xf;
  *(bytedata++) = dblb;

  return(bytedata);
}

/* LA */
GLushort *LA12(GLushort *shortdata, GLushort *in)
{

  if (*in == 0xfff) 
    *in = 0;
  else
    *in += 0x111;

  *(shortdata++) = (*in << 4);
  *(shortdata++) = (*in << 4);

  return(shortdata);
}
GLushort *LA8(GLushort *shortdata, GLushort *in)
{
  if (*in == 0xffff)
    *in = 0;
  else
    *in += 0x1111;

  *(shortdata++) = *in;

  return(shortdata);
}
GLubyte *LA4(GLubyte *bytedata, GLubyte *in)
{
  if (*in == 0xff) 
    *in = 0;
  else
    *in += 0x11;

  *(bytedata++) = *in;

  return(bytedata);
}

/* RGB */
GLushort *RGB12(GLushort *shortdata, GLushort *in)
{

  if (*in == 0xfff)
    *in = 0;
  else
    *in += 0x111;

  *(shortdata++) = (*in << 4);
  *(shortdata++) = (*in << 4);
  *(shortdata++) = (*in << 4);

  return(shortdata);
}
GLubyte *RGB8(GLubyte *bytedata, GLubyte *in)
{
  if (*in == 0xff) 
    *in = 0;
  else
    *in += 0x11;

  *(bytedata++) = *in;
  *(bytedata++) = *in;
  *(bytedata++) = *in;

  return(bytedata);
}
GLushort *RGB4(GLushort *shortdata, GLushort *in)
{
  GLushort w0;

  if (*in == 0xfff)
    *in = 0;
  else
    *in += 0x111;

  w0 = (*in & 0xf) << 12 
    | (*in & 0xf)  << 8 
      | (*in & 0xf)  << 4;

  *(shortdata++) = w0;

  return(shortdata);
}

/* RGBA */
GLushort *RGBA12(GLushort *shortdata, GLushort *in)
{

  if (*in == 0xfff)
    *in = 0;
  else
    *in += 0x111;

  *(shortdata++) = (*in << 4);
  *(shortdata++) = (*in << 4);
  *(shortdata++) = (*in << 4);
  *(shortdata++) = (*in << 4);

  return(shortdata);
}
GLushort *RGBA8(GLushort *shortdata, GLushort *in)
{
  if (*in == 0xffff)
    *in = 0;
  else
    *in += 0x1111;

  *(shortdata++) = *in;
  *(shortdata++) = *in;

  return(shortdata);
}
GLushort *RGBA4(GLushort *shortdata, GLushort *in)
{
  GLushort w0;

  if (*in == 0xffff)
    *in = 0;
  else
    *in += 0x1111;

  w0 = ((*in & 0xf) << 12)
    | ((*in & 0xf)  << 8)
      | ((*in & 0xf)  << 4)
	| (*in & 0xf);

  *(shortdata++) = w0;

  return(shortdata);
}

/* RGBA RED */
GLushort *RGBA12R(GLushort *shortdata, GLushort *in)
{

  if (*in == 0x0)
    *in = 0xfff0;
  else
    *in -= 0x1110;

  *(shortdata++) = 0x0;
  *(shortdata++) = 0x0;
  *(shortdata++) = 0x0;
  *(shortdata++) = 0x0;

  return(shortdata);
}
GLushort *RGBA8R(GLushort *shortdata, GLushort *in)
{

  if (*in == 0xf000)
    *in = 0x0;
  else
    *in += 0x1000;

  *(shortdata++) = *in;
  *(shortdata++) = 0;

  return(shortdata);
}
GLushort *RGBA4R(GLushort *shortdata, GLushort *in)
{

  *(shortdata++) = 0x7777;

  return(shortdata);
}

void mkImageB(long xsizet, long ysizet, GLubyte  *image, pfB fillFuncB)
{
  int x;
  int y;
  GLubyte Patt = 0;
  GLubyte *imptr;

  for (y=0, imptr=image; y < ysizet; y++) {
    for (x=0, Patt = 0; x < xsizet; x++) {
      imptr = fillFuncB(imptr,&Patt);
    }
  }
}

void mkImageS(long xsizet, long ysizet, GLushort *image, pfS fillFuncS)
{
  int x;
  int y;
  GLushort Patt = 0;
  GLushort *imptr;

  for (y=0, imptr=image; y < ysizet; y++) {
    for (x=0, Patt = 0; x < xsizet; x++) {
      imptr = fillFuncS(imptr,&Patt);
    }
  }
}

void mkImage(GLenum datatype, GLenum format,
	long xsizet, long ysizet, void *image, GLuint offset, __uint32_t test)
{
  GLushort *shortimage;
  GLubyte *byteimage;
  pfS fillFuncS;
  pfB fillFuncB;
  int ByteMult = 1;
  int foo;
#ifdef DUMP
  FILE *fileptr;
  int k;
  int j;
  GLubyte *imptr;
#endif

  switch (format) {
  case GL_LUMINANCE:
    switch (datatype) { 
    case GL_UNSIGNED_SHORT:
      shortimage = (GLushort *)image + xsizet*ysizet*offset;
      fillFuncS = &L12;
      mkImageS(xsizet, ysizet, shortimage, fillFuncS);
      ByteMult = 2;
      break;
    case GL_UNSIGNED_BYTE:
      byteimage = (GLubyte *)image + xsizet*ysizet*offset;
      fillFuncB = &L8;
      mkImageB(xsizet, ysizet, byteimage, fillFuncB);
      break;
    case GL_UNSIGNED_SHORT_4_4_4_4_EXT:
      byteimage = (GLubyte *)image + (xsizet*ysizet*offset)/2;
      fillFuncB = &L4;
      mkImageB(xsizet, ysizet, byteimage, fillFuncB);
      ByteMult = 0;
      break;
    default:
      fatalError("mkImage: Unexpected datatype");
    }
    break;
  case GL_LUMINANCE_ALPHA:
    switch (datatype) { 
    case GL_UNSIGNED_SHORT:
      shortimage = (GLushort *)image + xsizet*ysizet*offset*2;
      fillFuncS = &LA12;
      mkImageS(xsizet, ysizet, shortimage, fillFuncS);
      ByteMult = 4;
      break;
    case GL_UNSIGNED_BYTE:
      shortimage = (GLushort *)image + xsizet*ysizet*offset;
      fillFuncS = &LA8;
      mkImageS(xsizet, ysizet, shortimage, fillFuncS);
      ByteMult = 2;
      break;
    case GL_UNSIGNED_SHORT_4_4_4_4_EXT:
      byteimage = (GLubyte *)image + xsizet*ysizet*offset;
      fillFuncB = &LA4;
      mkImageB(xsizet, ysizet, byteimage, fillFuncB);
      foo = sizeof(*byteimage)/sizeof(char);
      break;
    default:
      fatalError("mkImage: Unexpected datatype");
    }
    break;
  case GL_RGB:
    switch (datatype) { 
    case GL_UNSIGNED_SHORT:
      shortimage = (GLushort *)image + xsizet*ysizet*offset*3;
      fillFuncS = &RGB12;
      mkImageS(xsizet, ysizet, shortimage, fillFuncS);
      ByteMult = 6;
      break;
    case GL_UNSIGNED_BYTE:
      byteimage = (GLubyte *)image + xsizet*ysizet*offset*3;
      fillFuncB = &RGB8;
      mkImageB(xsizet, ysizet, byteimage, fillFuncB);
      ByteMult = 3;
      break;
    case GL_UNSIGNED_SHORT_4_4_4_4_EXT:
      shortimage = (GLushort *)image + xsizet*ysizet*offset;
      fillFuncS = &RGB4;
      mkImageS(xsizet, ysizet, shortimage, fillFuncS);
      ByteMult = 2;
      break;
    default:
      fatalError("mkImage: Unexpected datatype");
    }
    break;
  case GL_RGBA:
    switch (datatype) { 
    case GL_UNSIGNED_SHORT:
      shortimage = (GLushort *)image + xsizet*ysizet*offset*4;
      fillFuncS = &RGBA12;
      mkImageS(xsizet, ysizet, shortimage, fillFuncS);
      ByteMult = 8;
      break;
    case GL_UNSIGNED_BYTE:
      shortimage = (GLushort *)image + xsizet*ysizet*offset*2;
      fillFuncS = &RGBA8;
      mkImageS(xsizet, ysizet, shortimage, fillFuncS);
      ByteMult = 4;
      break;
    case GL_UNSIGNED_SHORT_4_4_4_4_EXT:
      shortimage = (GLushort *)image + xsizet*ysizet*offset;
      fillFuncS = &RGBA4;
      mkImageS(xsizet, ysizet, shortimage, fillFuncS);
      ByteMult = 2;
      break;
    default:
      fatalError("mkImage: Unexpected datatype");
    }
    break;
  default:
    fatalError("mkImage: Unexpected format");
  }

#ifdef DUMP
  if (fileptr = fopen("imagedump","a+")) {
    if(ByteMult) {
      imptr = (GLubyte *)image + xsizet*ysizet*offset;
      for(k=0; k < ysizet; k++) {
	for(j=0; j < xsizet*ByteMult; j++) {
	  fprintf(fileptr,"0x%x ",*(imptr + j + k*xsizet*ByteMult));
	}
	fprintf(fileptr,"\n");
      }
    } else {
      for(k=0; k < ysizet; k++) {
	for(j=0; j < xsizet/2; j++)
	  fprintf(fileptr,"0x%x ",*((GLubyte *)image+j+k*xsizet/2));
	fprintf(fileptr,"\n");
      }
    }
  }
  fclose(fileptr);
#endif
}

void mkImageRed(GLenum datatype, GLenum format,
	     long xsizet, long ysizet, void *image, GLuint offset)
{
  GLushort *shortimage;
  pfS fillFuncS;

  switch (format) {
  case GL_RGBA:
    switch (datatype) { 
    case GL_UNSIGNED_SHORT:
      shortimage = (GLushort *)image + xsizet*ysizet*offset;
      fillFuncS = &RGBA12R;
      mkImageS(xsizet, ysizet, shortimage, fillFuncS);
      break;
    case GL_UNSIGNED_BYTE:
      shortimage = (GLushort *)image + xsizet*ysizet*offset;
      fillFuncS = &RGBA8R;
      mkImageS(xsizet, ysizet, shortimage, fillFuncS);
      break;
    case GL_UNSIGNED_SHORT_4_4_4_4_EXT:
      shortimage = (GLushort *)image + xsizet*ysizet*offset;
      fillFuncS = &RGBA4R;
      mkImageS(xsizet, ysizet, shortimage, fillFuncS);
      break;
    default:
      fatalError("mkImage: Unexpected datatype");
    }
    break;
  default:
    fatalError("mkImage: Unexpected format");
  }

}

__uint32_t mkDMAbuf(GLenum datatype, GLenum format,
     long xsizet, long ysizet, __uint32_t *image, __uint32_t *dmaptr, 
	__uint32_t test)
{
  int ByteMult = 1;
  int i, j, k, l, pad, bitcount, format_val;

  switch (datatype) {  /* # of bits per component */
                case GL_UNSIGNED_SHORT: bitcount = 16;break;
                case GL_UNSIGNED_BYTE:  bitcount = 8; break;

                /* 4-bit data must be dma'd as 8-bit data except for rgba */
                case GL_UNSIGNED_SHORT_4_4_4_4_EXT:
                        if (format == GL_RGBA)
                                bitcount = 4;
                        else
                                bitcount = 8;
                        break;
  }
  switch (format) { /* # of components */
                case GL_LUMINANCE:              format_val = 1; break;
                case GL_LUMINANCE_ALPHA:        format_val = 2; break;
                case GL_RGB:                    format_val = 3; break;
                case GL_RGBA:                   format_val = 4; break;
  }

  /* pad = how many bytes to pad */
  pad = (xsizet * (bitcount/8) * format_val) % 8;

  switch (format) {
  case GL_LUMINANCE:
    switch (datatype) { 
    case GL_UNSIGNED_SHORT:
      ByteMult = 2;
      memcpy32(dmaptr,image,ysizet,xsizet,2,pad); /* 2 bytes per texel */
      break;
    case GL_UNSIGNED_BYTE:
      memcpy32(dmaptr,image,ysizet,xsizet,1,pad); /* 1 bytes per texel */
      break;
    case GL_UNSIGNED_SHORT_4_4_4_4_EXT:
      ByteMult = 0;
      for (i=0; i < xsizet*ysizet/2; i++) {
	*((GLubyte *)dmaptr + i) = *((GLubyte *)image+i) & 0xf0;
	*((GLubyte *)dmaptr + i + 1) = (*((GLubyte *)image+i) << 4) & 0xf0;
      }
      break;
    default:
      fatalError("mkImage: Unexpected datatype");
    }
    break;
  case GL_LUMINANCE_ALPHA:
    switch (datatype) { 
    case GL_UNSIGNED_SHORT:
      ByteMult = 4;
      memcpy32(dmaptr,image,ysizet,xsizet,4,pad); /* 4 bytes per texel */
      break;
    case GL_UNSIGNED_BYTE:
      ByteMult = 2;
	msg_printf(DBG, "ysizet: %d, xsizet: %d, bitcount: %d, pad: %d\n",	
		ysizet,xsizet,bitcount,pad);
      memcpy32(dmaptr,image,ysizet,xsizet,2,pad); /* 2 bytes per texel */
      break;
    case GL_UNSIGNED_SHORT_4_4_4_4_EXT:
      for (j=0, k=0, l=0; j < ysizet; j++) {
         for (i=0; i < xsizet; i++) {
		*((GLubyte *)dmaptr + k++) = *((GLubyte *)image + l) & 0xf0;
		*((GLubyte *)dmaptr + k++) = 
			(*((GLubyte *)image + l) << 4) & 0xf0;
		l++;
	  }
	  for (i = 0; i<pad; i++) {
		*((GLubyte *)dmaptr + k++) = 0xaa;
	  }
      }
      break;
    default:
      fatalError("mkImage: Unexpected datatype");
    }
    break;
  case GL_RGB:
    switch (datatype) { 
    case GL_UNSIGNED_SHORT:
      ByteMult = 6;
      memcpy32(dmaptr,image,ysizet,xsizet,6,pad); /* 6 bytes per texel */
      break;
    case GL_UNSIGNED_BYTE:
      ByteMult = 3;
      if ((test == (TEX_MDDMA_TEST | 0x400)) || (test == (TEX_MDDMA_TEST | 0x1400))) {
	/* create the necessary dma writes by hand, 49152 repeating words 
	   The repeating pattern is 12 words which repeat */
	for (i = 0, k=0; i < 4096; i++) {
		for (j = 0; j < 12; j++) {
			*(dmaptr + k) = tex_mddma_dma_4_array[j];
			k++;
		}
	}		
      }
      else
      	memcpy32(dmaptr,image,ysizet,xsizet,3,pad); /* 3 bytes per texel */
      break;
    case GL_UNSIGNED_SHORT_4_4_4_4_EXT:
      ByteMult = 2;
      memcpy32(dmaptr,image,ysizet,xsizet,3,pad); /* 3 bytes per texel */
      break;
    default:
      fatalError("mkImage: Unexpected datatype");
    }
    break;
  case GL_RGBA:
    switch (datatype) { 
    case GL_UNSIGNED_SHORT:
      ByteMult = 8;
      memcpy32(dmaptr,image,ysizet,xsizet,8,pad); /* 8 bytes per texel */
      break;
    case GL_UNSIGNED_BYTE:
      ByteMult = 4;
	msg_printf(DBG,"mkDMAbuf: rgba8, x: %d, y: %d, ogl: 0x%x, data: 0x%x\n",
		xsizet, ysizet, dmaptr, image);
      if ((test == (TEX_LUT4D_TEST | 0x400)) || (test == (TEX_LUT4D_TEST | 0x1400))) {
	/* create the necessary dma writes by hand, 65536 repeating words 
	   The repeating pattern is 16 words which repeat */
	for (i = 0, k=0; i < 4096; i++) {
		for (j = 0; j < 16; j++) {
			*(dmaptr + k) = tex_lut4d_dma_4_array[j];
			k++;
		}
	}		
      }
      else
      memcpy32(dmaptr,image,ysizet,xsizet,4,pad); /* 4 bytes per texel */
      break;
    case GL_UNSIGNED_SHORT_4_4_4_4_EXT:
      ByteMult = 2;
      memcpy32(dmaptr,image,ysizet,xsizet,2,pad); /* 2 bytes per texel */
      break;
    default:
      fatalError("mkImage: Unexpected datatype");
    }
    break;
  default:
    fatalError("mkImage: Unexpected format");
  }

  if (dmaptr == NULL)
    fatalError("malloc failed for dmaptr");

  return(0);
}
