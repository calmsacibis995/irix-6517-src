/* --------------------------------------------------------------------------- */
/* -                              SWC_MAIN.C                                 - */
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
/* $Revision: 1.7 $ */
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h> 
#include <malloc.h>

#include "rgAPI.h"
#include "ssrv_swc.h"
#include "ssrv_http.h"
#include "SSC.h"

/* --------------------------------------------------------------------------- */
static SSRVEXPORT ssrvexf;
static int iSSS_RootSize = 0;
/* --------------------------------------------------------------------------- */
#if 0
static const char szIncorrectPassword[] = "<HTML><HEAD><TITLE>Unauthorized Request</TITLE></HEAD>\n\
<H1>Unauthorized Request</H1>\n<BODY TEXT=\"#000000\" BGCOLOR=\"#FFFFFF\">\
Your <B>\"UserName\"</B> or/and <B>\"Password\"</B> is/are incorrect.</BODY></HTML>";
#endif
/* --------------------------------------------------------------------------- */
static const char szInternalServerError_CantAllocXXXObject[] = "<HTML><HEAD><TITLE>Internal Server Error</TITLE></HEAD>\n\
<H1>Internal Server Error</H1>\n<BODY TEXT=\"#000000\" BGCOLOR=\"#FFFFFF\">\
<B>\"Switcher\"</B> can't create <B>\"%s\"</B> object.</BODY></HTML>";
/* --------------------------------------------------------------------------- */
static const char szInternalServerErrorHeader[] = "<HTML><HEAD><TITLE>Internal Server Error</TITLE></HEAD>\n\
<H1>Internal Server Error</H1>\n<BODY TEXT=\"#000000\" BGCOLOR=\"#FFFFFF\">";
/* --------------------------------------------------------------------------- */
static const char szNotFound[] = "<HTML><HEAD><TITLE>Not Found</TITLE></HEAD>\n\
<H1>Not Found</H1><BODY TEXT=\"#000000\" BGCOLOR=\"#FFFFFF\">\n\
<B>\"Switcher\"</B> can't find <B>\"%s\"</B> object.</BODY></HTML>";
/* --------------------------------------------------------------------------- */
static const char szRedirectedPageHeader[] = "<HTML><HEAD><TITLE>Redirected/Moved/See Other</TITLE></HEAD>\n\
<H1>Redirected/Moved/See Other</H1>\n<BODY TEXT=\"#000000\" BGCOLOR=\"#FFFFFF\"><br>New location is:<br>";
/* --------------------------------------------------------------------------- */
static const char szEndOfHtmlBody[] = "</BODY></HTML>";
static const char szUnknownError[] = "Unknown Error";
static const char szSSS_ROOT[] = SSS_ROOT;

/* --------------------- addFileToOutputBuffer ------------------------------- */
static int addFileToOutputBuffer(const char *fname, char *buffer,int maxsize)
{ struct stat statbuf;
  char tmp[1024];
  FILE *fp;
  int i,retcode = 0;
  
  if(fname && fname[0] && buffer && maxsize)
  { if((i = sscGetSharedString(2,tmp,sizeof(tmp))) != 0)
    { if((i+strlen(fname)) < sizeof(tmp))
      { if(tmp[i-1] != '/' || tmp[i-1] != '\\') strcat(tmp,"/");
        strcat(tmp,fname);
        if(!stat(tmp,&statbuf) && S_ISREG(statbuf.st_mode) && (fp = fopen(tmp,"rb")) != NULL)
        { if(maxsize >= (int)statbuf.st_size)
          { if(fread(buffer,1,(int)statbuf.st_size,fp) == (int)statbuf.st_size) retcode = (int)statbuf.st_size;
          }
        }
      }
    }
  }
  return retcode;
}
/* ------------------- swcCreateInternalErrorPage ---------------------------- */
static int swcCreateInternalErrorPage(streamHandle hs,char *buf,int msize)
{ int i, j;
  
  rewindStream(hs);
  if(sizeofStream(hs))
  { msize -= 1;
    if((i = addFileToOutputBuffer("swc_interr_beg.html",buf,msize)) == 0)
    { for(;i < msize;i++)
      { if((buf[i] = szInternalServerErrorHeader[i]) == 0) break;
      }
    }
    while(i < msize)
    { if((j = getChar(hs)) == (-1)) break;
      buf[i++] = (char)j;  
    }
    if((j = addFileToOutputBuffer("swc_interr_end.html",&buf[i],msize-i)) == 0)
    { for(j = 0;i < msize;j++)
      { if((buf[i++] = szEndOfHtmlBody[j]) == 0) break;
      }
    }
    else i += j;
    buf[i] = 0;
  }
  else i = snprintf(buf, msize, "%s%s%s",szInternalServerErrorHeader,szUnknownError,szEndOfHtmlBody);
  return i;
}  
/* --------------------- swcCreateRedirectedPage ----------------------------- */
static int swcCreateRedirectedPage(streamHandle hs,char *buf,int msize)
{ int j;
  int i = 0;
  rewindStream(hs);
  if(sizeofStream(hs))
  { if(msize > 0)
    { i = snprintf(buf, msize,szRedirectedPageHeader);
      while(i < msize)
      { if((j = getChar(hs)) == (-1)) break;
        buf[i++] = (char)j;  
      }
      buf[i < msize ? i : msize-1] = 0;
      i += snprintf(&buf[i], msize-i,szEndOfHtmlBody);
    }
  }
  return i;
}
/* --------------------- swcCopyErrorOutputToBuf ----------------------------- */
static int swcCopyErrorOutputToBuf(streamHandle hs,char *buf,int msize)
{ int j;
  int i = 0;
  if(msize > 0)
  { buf[0] = 0;
    rewindStream(hs);
    if(sizeofStream(hs))
    { while(i < msize)
      { if((j = getChar(hs)) == (-1)) break;
        buf[i++] = (char)j;  
      }
      buf[i < msize ? i : msize-1] = 0;
    }
  }
  return i;
}
/* --------------------------- prepareURL ------------------------------------ */
static char *prepareURL(char *url)
{ int i;
  char *c,*par = 0;
  
  if((c = url) != 0)
  { while(*url && *url != '?') url++;
    if(*url == '?')
    { *url++ = 0;
      if((i = strlen(c)) != 0) { for(--i;i >= 0 && (c[i] == ' ' || c[i] == '\t');i--) c[i] = 0;  }
      while(*url == ' ' || *url == '\t') url++;
      if(*url) par = url;
    }
  }
  return par;
}
/* -------------------------- swcBeginProc ----------------------------------- */
int swcBeginProc(hTaskControlBlock hTcb)
{ char *url,*wbuf,*param;
  int retcode,wbufmaxsize,wbufactsize;
  sscErrorHandle hErr;
  streamHandle hInput,hResult;

  ssrvexf.fp_ssrvSetStringByTcb(hTcb,SSRVSETS_USERARGS,0,0); /* clear pointer to hErr handle */
  ssrvexf.fp_ssrvSetStringByTcb(hTcb,SSRVSETS_USERARGS,1,0); /* clear pointer to hResult memory stream handle */

  if((ssrvexf.struct_size != sizeof(SSRVEXPORT)) ||
     ((url = (char*)ssrvexf.fp_ssrvGetStringByTcb(hTcb,SSRVGETS_URL,0)) == 0) ||
     ((wbuf = (char*)ssrvexf.fp_ssrvGetStringByTcb(hTcb,SSRVGETS_WORKBUFPTR,0)) == 0) ||
     ((wbufmaxsize = ssrvexf.fp_ssrvGetIntegerByTcb(hTcb,SSRVGETI_MAXWORKBUFSIZE,0)) == 0)) return 0;

  if((hErr = sscCreateErrorObject()) == 0)
  { wbufactsize = snprintf(wbuf,wbufmaxsize,szInternalServerError_CantAllocXXXObject,"Error");    /* Prepare message in work buffer */
    ssrvexf.fp_ssrvSetIntegerByTcb(hTcb,SSRVSETI_HTTPRETCODE,0,HTTP_INTERNAL_SERVER_ERROR); /* Set HTTP return code */
    ssrvexf.fp_ssrvSetIntegerByTcb(hTcb,SSRVSETI_CONTENTLENGTH,0,wbufactsize);     /* Set "Content-length" value */
    ssrvexf.fp_ssrvSetIntegerByTcb(hTcb,SSRVSETI_ACTWORKBUFSIZE,0,wbufactsize);    /* Set actual size for work buffer */
    return 0;
  }
  ssrvexf.fp_ssrvSetStringByTcb(hTcb,SSRVSETS_USERARGS,0,(const char*)hErr);

  if((hResult = newOutputStream()) == 0)
  { wbufactsize = snprintf(wbuf,wbufmaxsize,szInternalServerError_CantAllocXXXObject,"Output Stream");   /* Prepare message in work buffer */
    ssrvexf.fp_ssrvSetIntegerByTcb(hTcb,SSRVSETI_HTTPRETCODE,0,HTTP_INTERNAL_SERVER_ERROR); /* Set HTTP return code */
    ssrvexf.fp_ssrvSetIntegerByTcb(hTcb,SSRVSETI_CONTENTLENGTH,0,wbufactsize);     /* Set "Content-length" value */
    ssrvexf.fp_ssrvSetIntegerByTcb(hTcb,SSRVSETI_ACTWORKBUFSIZE,0,wbufactsize);    /* Set actual size for work buffer */
    return 0;
  }
  ssrvexf.fp_ssrvSetStringByTcb(hTcb,SSRVSETS_USERARGS,1,(const char*)hResult);
  
  /* ----------------------------------------------------------------------------- */
  /* Direct Report Generator Call */
  if(!strncasecmp(url,szSSS_ROOT,iSSS_RootSize))
  { retcode = sscHTMLProcessor(hErr,(const char *)url,hResult);
    if(retcode & RGPERR_NOCACHE) ssrvexf.fp_ssrvSetIntegerByTcb(hTcb,SSRVSETI_NOCACHEFLG,0,1); /* Set "No-Cache" flag */
    if(sscWereErrors(hErr) || ((retcode & RGPERR_RETCODE_MASK) != RGPERR_SUCCESS))
    { if((retcode & RGPERR_RETCODE_MASK) == RGPERR_REDIRECTED)
      { wbufactsize = swcCopyErrorOutputToBuf(sscGetErrorStream(hErr),wbuf,wbufmaxsize);   /* Copy "new location" to buffer */     
        ssrvexf.fp_ssrvSetStringByTcb(hTcb,SSRVSETS_LOCATION,0,wbuf);                      /* Copy url to tcb structure */
        ssrvexf.fp_ssrvSetIntegerByTcb(hTcb,SSRVSETI_HTTPRETCODE,0,wbufactsize ? HTTP_MOVED_TEMPORARILY : HTTP_INTERNAL_SERVER_ERROR);/* Set HTTP return code */
        wbufactsize = swcCreateRedirectedPage(sscGetErrorStream(hErr),wbuf,wbufmaxsize);   /* Prepare message in work buffer */
        ssrvexf.fp_ssrvSetIntegerByTcb(hTcb,SSRVSETI_CONTENTLENGTH,0,wbufactsize);         /* Set "Content-length" value */
        ssrvexf.fp_ssrvSetIntegerByTcb(hTcb,SSRVSETI_ACTWORKBUFSIZE,0,wbufactsize);        /* Set actual size for work buffer */
      }
      else
      { wbufactsize = swcCreateInternalErrorPage(sscGetErrorStream(hErr),wbuf,wbufmaxsize);     /* Prepare message in work buffer */
        ssrvexf.fp_ssrvSetIntegerByTcb(hTcb,SSRVSETI_HTTPRETCODE,0,HTTP_INTERNAL_SERVER_ERROR); /* Set HTTP return code */
        ssrvexf.fp_ssrvSetIntegerByTcb(hTcb,SSRVSETI_CONTENTLENGTH,0,wbufactsize);              /* Set "Content-length" value */
        ssrvexf.fp_ssrvSetIntegerByTcb(hTcb,SSRVSETI_ACTWORKBUFSIZE,0,wbufactsize);             /* Set actual size for work buffer */
      }
      return 0;
    }
    rewindStream(hResult);
    ssrvexf.fp_ssrvSetIntegerByTcb(hTcb,SSRVSETI_HTTPRETCODE,0,HTTP_OK);                       /* Set HTTP return code */
    ssrvexf.fp_ssrvSetIntegerByTcb(hTcb,SSRVSETI_CONTENTLENGTH,0,sizeofStream(hResult));       /* Set "Content-length" value */
    return 1;
  }

  /* ----------------------------------------------------------------------------- */
  /* Processing HTML file and (may be) inderect Report Generator call */
  param = prepareURL(url);
  if((hInput = newFileInputStream(url)) == 0)
  { wbufactsize = snprintf(wbuf,wbufmaxsize,szNotFound,url);                       /* Prepare message in work buffer */
    ssrvexf.fp_ssrvSetIntegerByTcb(hTcb,SSRVSETI_HTTPRETCODE,0,HTTP_NOT_FOUND);    /* Set HTTP return code */
    ssrvexf.fp_ssrvSetIntegerByTcb(hTcb,SSRVSETI_CONTENTLENGTH,0,wbufactsize);     /* Set "Content-length" value */
    ssrvexf.fp_ssrvSetIntegerByTcb(hTcb,SSRVSETI_ACTWORKBUFSIZE,0,wbufactsize);    /* Set actual size for work buffer */
    return 0;
  }

  retcode = sscHTMLPreProcessor(hErr,hInput,hResult,(const char*)param);
  destroyStream(hInput);
  if(retcode & RGPERR_NOCACHE) ssrvexf.fp_ssrvSetIntegerByTcb(hTcb,SSRVSETI_NOCACHEFLG,0,1); /* Set "No-Cache" flag */
  if(sscWereErrors(hErr) || ((retcode & RGPERR_RETCODE_MASK) != RGPERR_SUCCESS))
  { if((retcode & RGPERR_RETCODE_MASK) == RGPERR_REDIRECTED)
    { wbufactsize = swcCopyErrorOutputToBuf(sscGetErrorStream(hErr),wbuf,wbufmaxsize);   /* Copy "new location" to buffer */     
      ssrvexf.fp_ssrvSetStringByTcb(hTcb,SSRVSETS_LOCATION,0,wbuf);                      /* Copy url to tcb structure */
      ssrvexf.fp_ssrvSetIntegerByTcb(hTcb,SSRVSETI_HTTPRETCODE,0,wbufactsize ? HTTP_MOVED_TEMPORARILY : HTTP_INTERNAL_SERVER_ERROR); /* Set HTTP return code */
      wbufactsize = swcCreateRedirectedPage(sscGetErrorStream(hErr),wbuf,wbufmaxsize);   /* Prepare message in work buffer */
      ssrvexf.fp_ssrvSetIntegerByTcb(hTcb,SSRVSETI_CONTENTLENGTH,0,wbufactsize);         /* Set "Content-length" value */
      ssrvexf.fp_ssrvSetIntegerByTcb(hTcb,SSRVSETI_ACTWORKBUFSIZE,0,wbufactsize);        /* Set actual size for work buffer */
    }
    else
    { wbufactsize = swcCreateInternalErrorPage(sscGetErrorStream(hErr),wbuf,wbufmaxsize);     /* Prepare message in work buffer */
      ssrvexf.fp_ssrvSetIntegerByTcb(hTcb,SSRVSETI_HTTPRETCODE,0,HTTP_INTERNAL_SERVER_ERROR); /* Set HTTP return code */
      ssrvexf.fp_ssrvSetIntegerByTcb(hTcb,SSRVSETI_CONTENTLENGTH,0,wbufactsize);              /* Set "Content-length" value */
      ssrvexf.fp_ssrvSetIntegerByTcb(hTcb,SSRVSETI_ACTWORKBUFSIZE,0,wbufactsize);             /* Set actual size for work buffer */
    }
    return 0;
  }
  rewindStream(hResult);
  ssrvexf.fp_ssrvSetIntegerByTcb(hTcb,SSRVSETI_HTTPRETCODE,0,HTTP_OK);                       /* Set HTTP return code */
  ssrvexf.fp_ssrvSetIntegerByTcb(hTcb,SSRVSETI_CONTENTLENGTH,0,sizeofStream(hResult));       /* Set "Content-length" value */
  return 1;
}
/* -------------------------- swcContProc ------------------------------------ */
int swcContProc(hTaskControlBlock hTcb)
{ streamHandle hResult;
  char *wbuf;
  int i,wbufmaxsize,wbufactsize;

  if((ssrvexf.struct_size != sizeof(SSRVEXPORT)) ||
     ((wbuf = (char*)ssrvexf.fp_ssrvGetStringByTcb(hTcb,SSRVGETS_WORKBUFPTR,0)) == 0) ||
     ((wbufmaxsize = ssrvexf.fp_ssrvGetIntegerByTcb(hTcb,SSRVGETI_MAXWORKBUFSIZE,0)) == 0) ||
     (ssrvexf.fp_ssrvGetStringByTcb(hTcb,SSRVGETS_USERARGS,0) == 0) ||
     ((hResult = (streamHandle)ssrvexf.fp_ssrvGetStringByTcb(hTcb,SSRVGETS_USERARGS,1)) == 0)) return 0;

  for(wbufactsize = 0;wbufactsize < wbufmaxsize;wbufactsize++)
  { if((i = getChar(hResult)) < 0) break;
    wbuf[wbufactsize] = (char)i;
  }
  ssrvexf.fp_ssrvSetIntegerByTcb(hTcb,SSRVSETI_ACTWORKBUFSIZE,0,wbufactsize); /* Set actual size for work buffer */
  return wbufactsize;
}
/* -------------------------- swcEndProc ------------------------------------- */
int swcEndProc(hTaskControlBlock hTcb)
{ sscErrorHandle hErr;
  streamHandle hResult;
  if(ssrvexf.struct_size != sizeof(SSRVEXPORT)) return 0; /* not inited */

  hErr = (sscErrorHandle)ssrvexf.fp_ssrvGetStringByTcb(hTcb,SSRVGETS_USERARGS,0); /* get hErr handle */
  hResult = (streamHandle)ssrvexf.fp_ssrvGetStringByTcb(hTcb,SSRVGETS_USERARGS,1); /* get hResult memory stream handle */
  ssrvexf.fp_ssrvSetStringByTcb(hTcb,SSRVSETS_USERARGS,0,0); /* clear pointer to hErr handle */
  ssrvexf.fp_ssrvSetStringByTcb(hTcb,SSRVSETS_USERARGS,1,0); /* clear pointer to hResult memory stream handle */

  if(sscIsErrorObject(hErr)) sscDeleteErrorObject(hErr);
  if(isStreamValid(hResult)) destroyStream(hResult);

  return 1;
}
/* -------------------------- swcInitSwitcher -------------------------------- */
int swcInitSwitcher(SSRVEXPORT *exf)
{ int retcode = 0;
  if(exf && exf->struct_size == sizeof(SSRVEXPORT))
  { memset(&ssrvexf,0,sizeof(SSRVEXPORT));
    memcpy(&ssrvexf,exf,sizeof(SSRVEXPORT));
    retcode = sscHTMLProcessorInit();
  }
  iSSS_RootSize = strlen(szSSS_ROOT);
  sscSetSharedLong(0,(unsigned long)ssrvexf.fp_ssrvGetIntegerByTcb(0,SSRVGETI_WEBSRVPORT,0));
  sscSetSharedLong(1,(unsigned long)ssrvexf.fp_ssrvGetIntegerByTcb(0,SSRVGETI_WEBSRVVMAJOR,0));
  sscSetSharedLong(2,(unsigned long)ssrvexf.fp_ssrvGetIntegerByTcb(0,SSRVGETI_WEBSRVVMINOR,0));
  sscSetSharedLong(3,(unsigned long)ssrvexf.fp_ssrvGetIntegerByTcb(0,SSRVGETI_USERNAMEKEY,0));
  sscSetSharedLong(4,(unsigned long)ssrvexf.fp_ssrvGetIntegerByTcb(0,SSRVGETI_PASSWORDKEY,0));
  sscSetSharedString(0,ssrvexf.fp_ssrvGetStringByTcb(0,SSRVGETS_SRVLEGALNAME,0));
  sscSetSharedString(1,ssrvexf.fp_ssrvGetStringByTcb(0,SSRVGETS_SRVVERTIME,0));
  sscSetSharedString(2,ssrvexf.fp_ssrvGetStringByTcb(0,SSRVGETS_SRVRESPATH,0));
  sscSetSharedString(3,ssrvexf.fp_ssrvGetStringByTcb(0,SSRVGETS_SRVISOCKNAME,0));
   
  return retcode;  
}
/* -------------------------- swcDoneSwitcher -------------------------------- */
void swcDoneSwitcher(void)
{ memset(&ssrvexf,0,sizeof(SSRVEXPORT));
  sscHTMLProcessorDone();
}
