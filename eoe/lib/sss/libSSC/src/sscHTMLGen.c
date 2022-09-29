/* --------------------------------------------------------------------------- */
/* -                             SSCHTMLGEN.C                                - */
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
#include <pthread.h>
#include <sys/types.h>
#include <malloc.h>

#include "sscHTMLGen.h"

/* --------------------------------------------------------------------------- */
typedef struct sss_s_ACTIVETHEAD {
   struct sss_s_ACTIVETHEAD *link;
   pthread_t thread;
   streamHandle output;
} activeThread;
/* --------------------------------------------------------------------------- */
static pthread_mutex_t htmlLock = PTHREAD_MUTEX_INITIALIZER;
static activeThread *activeThreads = 0;
static activeThread *activeThreadsFreeList = 0;
static unsigned long volatile activeThreadsAllocCnt = 0;
static unsigned long volatile activeThreadsBusyCnt  = 0;
static unsigned long volatile activeThreadsFreeCnt  = 0;
/* ---------------------------- findActiveThread ----------------------------- */
static activeThread **findActiveThread(void)
{  activeThread **p;
   pthread_t self;

   self = pthread_self();
   
   for(p = &activeThreads;*p;p = &(*p)->link)
   { if(pthread_equal(self,(*p)->thread)) return p;
   }
   return 0;
}
/* ------------------------------- getOutput --------------------------------- */
static streamHandle getOutput(void)
{  activeThread **p;
   streamHandle out;

   pthread_mutex_lock(&htmlLock);
   if((p = findActiveThread()) == 0) out = NULL;
   else                              out = (*p)->output;
   pthread_mutex_unlock(&htmlLock);

   return out;
}
/* --------------------------- getHTMLGeneratorInfo -------------------------- */
unsigned long getHTMLGeneratorInfo(int idx)
{ switch(idx) {
  case GETHTMLGENINFO_ALLOC : return activeThreadsAllocCnt;
  case GETHTMLGENINFO_BUSY  : return activeThreadsBusyCnt;
  case GETHTMLGENINFO_FREE  : return activeThreadsFreeCnt;
  };
  return 0;
}
/* -------------------------- createMyHTMLGenerator -------------------------- */
int createMyHTMLGenerator(streamHandle output)
{  activeThread *t;

   pthread_mutex_lock(&htmlLock);
   if((t = activeThreadsFreeList) != 0) { activeThreadsFreeList = t->link; activeThreadsFreeCnt--; }
   pthread_mutex_unlock(&htmlLock);

   if(!t) if((t = (activeThread *)malloc(sizeof(activeThread))) != 0) activeThreadsAllocCnt++;
   if(t)
   { memset(t,0,sizeof(activeThread));
     pthread_mutex_lock(&htmlLock);
     t->link   = activeThreads; activeThreads = t; activeThreadsBusyCnt++;
     t->thread = pthread_self();
     t->output = output;
     pthread_mutex_unlock(&htmlLock);
   }
   return (t != 0) ? 0 : (-1);
}
/* -------------------------- deleteMyHTMLGenerator -------------------------- */
int deleteMyHTMLGenerator(void)
{  activeThread **p, *t;
 
   pthread_mutex_lock(&htmlLock);
   if((p = findActiveThread()) == 0)
   { pthread_mutex_unlock(&htmlLock);
     return -1;
   }
   t = *p; *p = t->link;  activeThreadsBusyCnt--;
   t->link = activeThreadsFreeList; activeThreadsFreeList = t; activeThreadsFreeCnt++;
   pthread_mutex_unlock(&htmlLock);
   return 0;
}
/* ------------------------------ TableBegin --------------------------------- */
void TableBegin(const char *tabledesign)
{  streamHandle result = getOutput();
   if(result)
   { putString("\n\n<TABLE ",result);
     if(tabledesign) putString(tabledesign,result);
     putString(">\n",result);
   }
}
/* ------------------------------- TableEnd ---------------------------------- */
void TableEnd(void)
{  streamHandle result = getOutput();
   if(result) putString("</TABLE>\n\n",result);
}
/* ------------------------------- RowBegin ---------------------------------- */
void RowBegin(const char *rowdesign)
{  streamHandle result = getOutput();
   if(result)
   { if(rowdesign)
     { putString("<TR ",result);
       putString(rowdesign,result);
     }
     else putString("<TR",result);
     putString(">\n",result);
   }
}
/* -------------------------------- RowEnd ----------------------------------- */
void RowEnd(void)
{  streamHandle result = getOutput();
   if (result) putString("</TR>\n",result);
}
/* ------------------------------- CellBegin --------------------------------- */
void CellBegin(const char *celldesign)
{  streamHandle result = getOutput();
   if (result)
   { putString("<TD ",result);
     if(celldesign) putString(celldesign,result);
     putString("><FONT FACE=Arial,Helvetica>",result);
   }
}
/* ------------------------------- CellEnd ----------------------------------- */
void CellEnd(void)
{  streamHandle result = getOutput();
   if (result) putString("</TD>\n",result);
}
/* -------------------------------- Cell ------------------------------------- */
void Cell(const char *cellbody)
{  streamHandle result = getOutput();
   if (result)
   { putString("<TD><FONT FACE=Arial,Helvetica>",result);
     if(cellbody) putString(cellbody,result);
     putString("</TD>\n",result); 
   }
}
/* ----------------------------- FormatedCell -------------------------------- */
void FormatedCell(const char *format, ...)
{ char buff[2048];
  va_list args;
  streamHandle result = getOutput();
  if(result)
  { va_start(args, format); vsnprintf(buff, sizeof(buff), format, args); va_end(args);
    putString("<TD><FONT FACE=Arial,Helvetica>",result);
    putString(buff,result);
    putString("</TD>\n",result);
  }
}
/* ------------------------------ LinkBegin ---------------------------------- */
void LinkBegin(const char *url)
{  streamHandle result = getOutput();
   if (result)
   { putString("<A HREF =\"",result);
     if(url) putString(url,result);
     putString("\">",result);
   }
}
/* ------------------------------- LinkEnd ----------------------------------- */
void LinkEnd(void)
{  streamHandle result = getOutput();
   if (result) putString("</A>",result);
}
/* -------------------------------- Body ------------------------------------- */
void Body(const char *body)
{  streamHandle result = getOutput();
   if (result && body) putString(body,result);
}
/* ----------------------------- FormatedBody -------------------------------- */
void FormatedBody(const char *format, ...)
{  char buff[2048];
   streamHandle result;
   va_list args;

   if((result = getOutput()) != 0)
   { va_start(args, format); vsnprintf(buff, sizeof(buff), format, args);  va_end(args);
     putString(buff, result);
   }
}
/* ------------------------------ OListBegin --------------------------------- */
void OListBegin(const char *oldesign)
{  streamHandle result = getOutput();
   if(result)
   { if(oldesign)
     { putString("\n<OL ",result);
       putString(oldesign,result);
     }
     else putString("\n<OL",result);
     putString(">",result);
   }
}
/* ------------------------------ OListEnd ----------------------------------- */
void OListEnd(void)
{  streamHandle result = getOutput();
   if (result) putString("\n</OL>",result);
}
/* ----------------------------- UListBegin ---------------------------------- */
void UListBegin(const char *uldesign)
{  streamHandle result = getOutput();
   if (result)
   { if(uldesign)
     { putString("\n<UL ",result);
       putString(uldesign,result);
     }
     else putString("\n<UL",result);
     putString(">",result);
   }
}
/* ------------------------------ UListEnd ----------------------------------- */
void UListEnd(void)
{  streamHandle result = getOutput();
   if (result) putString("\n</UL>",result);
}
/* ---------------------------- DirListBegin --------------------------------- */
void DirListBegin(const char *dldesign)
{  streamHandle result = getOutput();
   if (result)
   { if(dldesign)
     { putString("\n<DIR ",result);
       putString(dldesign,result);
     }
     else putString("\n<DIR",result);
     putString(">",result);
   }
}
/* ---------------------------- DirListEnd ----------------------------------- */
void DirListEnd(void)
{  streamHandle result = getOutput();
   if (result) putString("\n</DIR>",result);
}
/* --------------------------- MenuListBegin --------------------------------- */
void MenuListBegin(const char *mldesign)
{  streamHandle result = getOutput();
   if (result)
   { if(mldesign)
     { putString("\n<MENU ",result);
       putString(mldesign,result);
     }
     else putString("\n<MENU",result);
     putString(">",result);
   }
}
/* ---------------------------- MenuListEnd ---------------------------------- */
void MenuListEnd(void)
{  streamHandle result = getOutput();
   if (result) putString("\n</MENU>",result);
}
/* ------------------------------ List --------------------------------------- */
void List(const char *listbody)
{  streamHandle result = getOutput();
   if (result)
   { putString("\n<LI>",result);
     if(listbody) putString(listbody,result);
   }
}

void DefListBegin(const char *dfldesign)
{  streamHandle result = getOutput();
   if (result)
   { if(dfldesign)
     { putString("\n<DL ",result);
       putString(dfldesign,result);
     }
     else putString("\n<DL",result);
     putString(">",result);
   }
}

void DefTerm(const char *term)
{  streamHandle result = getOutput();
   if (result)
   { putString("\n<DT>",result);
     putString(term,result);
   }
}

void DefDef(const char *definition)
{  streamHandle result = getOutput();
   if (result)
   { putString("\n<DD>",result);
     putString(definition,result);
   }
}

void DefListEnd(void)
{  streamHandle result = getOutput();
   if (result) putString("\n</DL>",result); 
}

void Heading1(const char *design)
{  streamHandle result = getOutput();
   if(result)
   { if(design)
     { putString("\n<H1 ",result);
       putString(design,result);
     }
     else putString("\n<H1",result);
     putString(">",result);
   }
}

void Heading1End(void)
{  streamHandle result = getOutput();
   if (result) putString("</H1>",result);
}

void Heading2(const char *design)
{  streamHandle result = getOutput();
   if(result)
   { if(design)
     { putString("\n<H2 ",result);
       putString(design,result);
     }
     else putString("\n<H2",result);
     putString(">",result);
   }
}

void Heading2End(void)
{  streamHandle result = getOutput();
   if (result) putString("</H2>",result);
}

void Heading3(const char *design)
{  streamHandle result = getOutput();
   if(result)
   { if(design)
     { putString("\n<H3 ",result);
       putString(design,result);
     }
     else putString("\n<H3",result);
     putString(">",result);
   }
}

void Heading3End(void)
{  streamHandle result = getOutput();
   if (result) putString("</H3>",result);
}

void Heading4(const char *design)
{  streamHandle result = getOutput();
   if(result)
   { if(design)
     { putString("\n<H4 ",result);
       putString(design,result);
     }
     else putString("\n<H4",result);
     putString(">",result);
   }
}

void Heading4End(void)
{  streamHandle result = getOutput();
   if(result) putString("</H4>",result);
}

void Heading5(const char *design)
{  streamHandle result = getOutput();
   if(result)
   { if(design)
     { putString("\n<H5 ",result);
       putString(design,result);
     }
     else putString("\n<H5",result);
     putString(">",result);
   }
}

void Heading5End(void)
{  streamHandle result = getOutput();
   if(result) putString("</H5>",result);
}

void Heading6(const char *design)
{  streamHandle result = getOutput();
   if(result)
   { if(design)
     { putString("\n<H6 ",result);
       putString(design,result);
     }
     else putString("\n<H6",result);
     putString(">",result);
   }
}

void Heading6End(void)
{  streamHandle result = getOutput();
   if (result) putString("</H6>",result);
}
