/* --------------------------------------------------------------------------- */
/* -                             SSCSTREAMS.C                                - */
/* --------------------------------------------------------------------------- */
/*                                                                             */
/* Copyright 1992-1998 Silicon Graphics, Inc.                                  */
/* All Rights Reserved.                                                        */
/*                                                                             */
/* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;      */
/* the contents of this file may not be disclosed to third parties, copied or  */
/* duplicated in any form, in whole or in part, without the prior written      */
/* permission of Silicon Graphics, Inc.                                        */
/*                                                                             */
/* RESTRICTED RIGHTS LEGEND:                                                   */
/* Use, duplication or disclosure by the Government is subject to restrictions */
/* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data      */
/* and Computer Software clause at DFARS 252.227-7013, and/or in similar or    */
/* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -     */
/* rights reserved under the Copyright Laws of the United States.              */
/*                                                                             */
/* --------------------------------------------------------------------------- */
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h> 
#include <pthread.h>
#include <sys/types.h> 
#include <malloc.h>

#include "sscStreams.h"
/* --------------------------------------------------------------------------- */
#define SSC_STREAMS_MAXMEMBLOCKS (60)
#define SSC_STREAMS_MINFREEBLOCKS (10)
#define BLOCKSIZE (1024)
typedef struct sss_s_MEMBLOCK {
    struct sss_s_MEMBLOCK *link;
    char                  data[BLOCKSIZE];
} MBlock;
/* --------------------------------------------------------------------------- */
typedef struct sss_s_STREAM {
 unsigned long struct_signature; /* sizeof(Stream) */
 int tag;
 struct sss_s_STREAM *next;
 union {
   /*------------------------- Stream in memory ----------------------*/
   struct { 
     MBlock *memHead;
     MBlock *memTail;
     int    blockIndx;
     int    totalBytes;
   } mem;
   /*------------------------- Stream in file ----------------------*/
   struct {
     FILE *file;
   } file;
 } u;
} Stream;
/* --------------------------------------------------------------------------- */
enum streamTypes {
    IN_file = 1,
    OUT_file,
    IN_mem,
    OUT_mem
};
/* --------------------------------------------------------------------------- */
static pthread_mutex_t streamsFreeListMutex = PTHREAD_MUTEX_INITIALIZER;
static Stream *streamsFreeList = 0;  /* Stream structures free list */
static MBlock *mblockFreeList = 0;   /* Memory block free list */
static unsigned long volatile mblockAllocCnt = 0;
static unsigned long volatile mblockFreeCnt = 0;
static unsigned long volatile streamAllocCnt = 0;
static unsigned long volatile streamFreeCnt = 0;
static unsigned long volatile uniqFnameCounter = 0;

/* ------------------------- allocMBlock ------------------------------------- */
static MBlock *allocMBlock(void)
{ MBlock *mb = 0;
  pthread_mutex_lock(&streamsFreeListMutex);
  if((mb = mblockFreeList) != 0) { mblockFreeList = mb->link; mblockFreeCnt--; }
  if(!mb) if((mb = (MBlock*)malloc(sizeof(MBlock))) != 0) mblockAllocCnt++;
  pthread_mutex_unlock(&streamsFreeListMutex);
  if(mb) memset(mb,0,sizeof(MBlock));
  return mb;
}
/* -------------------------- freeMBlock ------------------------------------- */
static void freeMBlock(MBlock *mb)
{ pthread_mutex_lock(&streamsFreeListMutex);
  mb->link = mblockFreeList; mblockFreeList = mb; mblockFreeCnt++;
  pthread_mutex_unlock(&streamsFreeListMutex);
}  
/* ------------------------ allocStreamStructure ----------------------------- */
static Stream *allocStreamStructure(int tag)
{ Stream *s;
  pthread_mutex_lock(&streamsFreeListMutex);
  if((s = streamsFreeList) != 0) { streamsFreeList = s->next; streamFreeCnt--; }
  pthread_mutex_unlock(&streamsFreeListMutex);
  if(!s) if((s = (Stream *)malloc(sizeof(Stream))) != 0) streamAllocCnt++;
  if(s)
  { memset(s,0,sizeof(Stream));
    s->struct_signature = sizeof(Stream);
    s->tag = tag;
  }
  return s;
}
/* ------------------------ freeStreamStructure ------------------------------ */
static Stream *freeStreamStructure(Stream *s)
{ if(s && s->struct_signature == sizeof(Stream))
  { pthread_mutex_lock(&streamsFreeListMutex);
    s->next = streamsFreeList; streamsFreeList = s; s->tag = (-1);
    streamFreeCnt++;
    pthread_mutex_unlock(&streamsFreeListMutex);
  }
  return 0;
}

/* -------------------------- streamGetInfo ---------------------------------- */
unsigned long streamGetInfo(int idx)
{ switch(idx) {
  case STREAMLIB_GETMBLOCKALLOC : return mblockAllocCnt;
  case STREAMLIB_GETMBLOCKFREE  : return mblockFreeCnt;
  case STREAMLIB_GETSTREAMALLOC : return streamAllocCnt;
  case STREAMLIB_GETSTREAMFREE  : return streamFreeCnt;
  };
  return 0;
}
/* -------------------------- isStreamValid ---------------------------------- */
int isStreamValid(streamHandle str)
{ Stream *stream = (Stream *)str;
  return (stream && stream->struct_signature == sizeof(Stream));
}
/* ------------------------- newOutputStream ---------------------------------- */
streamHandle newOutputStream(void)
{ char tmpfname[512];
  streamHandle s;
  unsigned long fnameidx = 0;

  pthread_mutex_lock(&streamsFreeListMutex);
  if(mblockAllocCnt >= SSC_STREAMS_MAXMEMBLOCKS &&
     mblockFreeCnt < SSC_STREAMS_MINFREEBLOCKS) fnameidx = ++uniqFnameCounter;
  pthread_mutex_unlock(&streamsFreeListMutex);
  if(fnameidx)
  { sprintf(tmpfname,"/tmp/.tmpstreams%lX%lX",(unsigned long)getpid(),fnameidx);
    unlink(tmpfname);
    if((s = newFileOutputStream(tmpfname)) != 0) unlink(tmpfname);
    return s;
  } 
  return newMemoryStream();
}
/* -------------------------- newMemoryStream -------------------------------- */
streamHandle newMemoryStream(void)
{ return allocStreamStructure(OUT_mem);
}
/* ------------------------- newFileInputStream ------------------------------ */
streamHandle newFileInputStream(const char *filename)
{  Stream  *stream = 0;
   if(filename)
   { if((stream = allocStreamStructure(IN_file)) != 0)
     { if((stream->u.file.file = fopen(filename, "r")) == 0)
       { stream = freeStreamStructure(stream);
       }
     }
   }
   return stream;
}
/* ------------------------- newFileOutputStream ----------------------------- */
streamHandle newFileOutputStream(const char *filename)
{  Stream  *stream;
   if((stream = allocStreamStructure(OUT_file)) != 0)
   { if((stream->u.file.file = fopen(filename,"w+")) == 0)
     { stream = freeStreamStructure(stream);
     }
   }
   return stream;
}
/* --------------------------- destroyStream --------------------------------- */
int destroyStream(streamHandle str)
{ MBlock *ma,*mb;
  Stream *stream = (Stream *)str;
  if(!stream || stream->struct_signature != sizeof(Stream)) return -1;
  switch (stream->tag) {
  case IN_mem  :
  case OUT_mem : for(mb = stream->u.mem.memHead;mb != 0;)
                 { ma = mb; mb = mb->link;
                   freeMBlock(ma);
                 }
                 break;
  case IN_file :
  case OUT_file: if(stream->u.file.file) fclose(stream->u.file.file);
                 stream->u.file.file = 0;
                 break;
  default:       return -1;
  };
  freeStreamStructure(stream);
  return 0;
}
/* ----------------------------- putChar ------------------------------------- */
int putChar(const int c, streamHandle str)
{  MBlock *mb;
   Stream *stream = (Stream *)str;
   if(!stream || stream->struct_signature != sizeof(Stream)) return -1;
  
   switch (stream->tag) {
   case OUT_mem:
          if (stream->u.mem.memHead == 0) 
          {  stream->u.mem.memHead = allocMBlock();
             if (stream->u.mem.memHead == 0) return -1;
             stream->u.mem.memHead->link = 0;
             stream->u.mem.memTail = stream->u.mem.memHead;
          }
          if (stream->u.mem.blockIndx >= BLOCKSIZE)
          {  if (stream->u.mem.memTail->link == 0)
             {  mb = allocMBlock();
                if (mb == 0) return -1;
                mb->link = 0;
                stream->u.mem.memTail->link = mb;
                stream->u.mem.memTail = mb;
             }
             else
             {  stream->u.mem.memTail = stream->u.mem.memTail->link;
             }
             stream->u.mem.blockIndx -= BLOCKSIZE;
          }
          stream->u.mem.memTail->data[stream->u.mem.blockIndx] = c;
          stream->u.mem.blockIndx++;
          stream->u.mem.totalBytes++;
          break;
   case OUT_file:
          fputc(c, stream->u.file.file);
          break;
   default:
          return -1;
   };
   return c;
}
/* --------------------------- putString ------------------------------------- */
int putString(const char *s, streamHandle str)
{  int c;
   if(s)
   { while (*s != 0) 
     { c = putChar(*s++, str);
       if (c < 0) return -1;
     }
   }
   return 0;
}
/* ---------------------------- putStringFmt --------------------------------- */
int putStringFmt(streamHandle str, char *msg,...)
{ char buff[2048];
  va_list args;
  va_start(args, msg);
  vsnprintf(buff, sizeof(buff), msg, args);
  va_end(args);
  return putString((const char *)buff,str);
}
/* --------------------------- putStream ------------------------------------- */
int putStream(streamHandle from, streamHandle to)
{  int ch;
   rewindStream(from);
   for(;;) 
   {  if (eofStream(from)) break;
      ch = getChar(from);
      if (ch == -1) break;
      ch = putChar(ch, to);
      if (ch <   0) break;
   }
   return 0;
}
/* --------------------------- getChar -------------------------------------- */
int getChar(streamHandle str)
{  int c;
   Stream *stream = (Stream *)str;
   if(!stream || stream->struct_signature != sizeof(Stream)) return -1;
   
   switch (stream->tag) {
   case IN_mem:
          if (stream->u.mem.totalBytes <= 0) return -1;
          if (stream->u.mem.blockIndx >= BLOCKSIZE)
          {  stream->u.mem.memTail = stream->u.mem.memTail->link;
             stream->u.mem.blockIndx -= BLOCKSIZE;
          }
          c = stream->u.mem.memTail->data[stream->u.mem.blockIndx];
          stream->u.mem.blockIndx++;
          stream->u.mem.totalBytes--;
          return c;
   case OUT_file:
          return fgetc(stream->u.file.file);
   case IN_file:
          return fgetc(stream->u.file.file);
   default:
          return -1;
   };
}

/*
int unputChar(streamHandle str)
{  MBlock *mb;
   Stream *stream = (Stream *)str;
   if(!stream || stream->struct_signature != sizeof(Stream)) return -1;
  
   switch (stream->tag)
   { case OUT_mem:
          if (stream->u.mem.totalBytes <= 0) return -1;
          stream->u.mem.blockIndx--;
          if (stream->u.mem.blockIndx < 0)
          {  mb = stream->u.mem.memHead;
             while (mb->link != stream->u.mem.memTail) mb = mb->link;
             stream->u.mem.memTail = mb;
             stream->u.mem.blockIndx += BLOCKSIZE;
          }
          stream->u.mem.totalBytes--;
          return 0;
     default:
          return -1;
   }
}
*/
/* -----------------------  rewriteStream ------------------------------------ */
int rewriteStream(streamHandle str)
{ Stream *stream = (Stream *)str;
  if(!stream || stream->struct_signature != sizeof(Stream)) return -1;

  switch (stream->tag) {
  case OUT_mem:
  case IN_mem:
          stream->u.mem.memTail = stream->u.mem.memHead;
          stream->u.mem.blockIndx  = 0;
          stream->u.mem.totalBytes = 0;
          stream->tag = OUT_mem;
          return 0;
  default:
          return -1;
  };
}
/* --------------------------- rewindStream ---------------------------------- */
int rewindStream(streamHandle str)
{  MBlock *mb;
   Stream *stream = (Stream *)str;
   if(!stream || stream->struct_signature != sizeof(Stream)) return -1;

   switch (stream->tag) {
   case OUT_mem:
          stream->u.mem.memTail = stream->u.mem.memHead;
          stream->u.mem.blockIndx  = 0;
          stream->tag = IN_mem;
          return 0;
   case IN_mem:
          mb = stream->u.mem.memHead;
          while (mb != stream->u.mem.memTail)
          {  stream->u.mem.totalBytes += BLOCKSIZE;
             mb = mb->link;
          }
          stream->u.mem.totalBytes += stream->u.mem.blockIndx;
          stream->u.mem.memTail = stream->u.mem.memHead;
          stream->u.mem.blockIndx  = 0;
          return 0;
   case OUT_file:
          rewind(stream->u.file.file);
	  return 0;
   default:
          return -1;
   };
}

int eofStream(streamHandle str)
{  Stream *stream = (Stream *)str;
   if(!stream || stream->struct_signature != sizeof(Stream)) return -1;

   switch (stream->tag) {
   case IN_mem:
          return (stream->u.mem.totalBytes <= 0);
   case IN_file:
          return feof(stream->u.file.file);
   default:
          return -1;
   };
}
/* ---------------------------- sizeofStream --------------------------------- */
int sizeofStream(streamHandle str)
{  long pos;
   int size = -1;
   Stream *stream = (Stream *)str;
   if(!stream || stream->struct_signature != sizeof(Stream)) return -1;

   switch (stream->tag) {
   case OUT_mem:
   case IN_mem:
          return stream->u.mem.totalBytes;
   case OUT_file:
          if((pos = ftell(stream->u.file.file)) >= 0)
	  { fseek(stream->u.file.file,0,SEEK_END);
            size = (int)ftell(stream->u.file.file);
	    fseek(stream->u.file.file,pos,SEEK_SET);
	  }
          return size;
   default:
          return -1;
   };
}
