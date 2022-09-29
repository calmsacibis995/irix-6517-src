/* --------------------------------------------------------------------------- */
/* -                              SSCPAIR.C                                  - */
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
#include <string.h>
#include <ctype.h> 

#include "sscPair.h"

/* ---------------------- sscValidateURLString ------------------------------- */
int SSCAPI sscValidateURLString(char *url)
{ int i;
  char tmp[8],*in;
  
  if(!url || !url[0]) return 0;
  for(in = url;;)
  { if(*in == '%')
    { in++;
      if(isxdigit(*in) && isxdigit(*(in+1)))
      { tmp[0] = *in++; tmp[1] = *in++; tmp[2] = 0;
        if(sscanf(tmp,"%x",&i) != 1) return 0;
        *url++ = (char)(i&0xff);
      }
      else return 0; /* incorrect %2HEX */
    }
    else if(*in == '+') { *url++ = ' '; in++; }
    else { if((*url++ = *in++) == 0) break; }
  }
  return 1;
}
/* -------------------------- sscInitPair ------------------------------------ */
int SSCAPI sscInitPair(char *cmdstr,CMDPAIR *cmdp,int cmdpsize)
{ int retsize = 0;
  if(cmdp && cmdpsize)
  { memset(cmdp,0,sizeof(CMDPAIR)*cmdpsize);
    if(cmdstr)
    { while(retsize < (cmdpsize-1) && *cmdstr)
      { cmdp[retsize].keyname = cmdstr;
        while(*cmdstr && *cmdstr != '=') cmdstr++;
        if(*cmdstr)
        { *cmdstr++ = 0;
          cmdp[retsize].value = cmdstr;
          while(*cmdstr && *cmdstr != '&') cmdstr++;
          if(*cmdstr) *cmdstr++ = 0;
          if(cmdp[retsize].keyname) sscValidateURLString(cmdp[retsize].keyname);
          if(cmdp[retsize].value)   sscValidateURLString(cmdp[retsize].value);
          retsize++;
        }
      }
    }
  }
  return retsize;
}
/* ----------------------- sscFindPairByKey ---------------------------------- */
int SSCAPI sscFindPairByKey(CMDPAIR *cmdp,int startidx,const char *szKeyName)
{ int i;
  if(szKeyName && cmdp)
  { for(i = 0;i < 0xfffff;i++)
    { if(!cmdp[i].keyname || !cmdp[i].value) break;
    }
    if(startidx <= i)
    { for(;startidx <= i;startidx++)
       if(cmdp[startidx].keyname && !strcasecmp(szKeyName,cmdp[startidx].keyname)) return startidx;
    }
  }
  return (-1);
}
/* --------------------- sscFindPairStrictByKey ------------------------------ */
int SSCAPI sscFindPairStrictByKey(CMDPAIR *cmdp,int startidx,const char *szKeyName)
{ int i;
  if(szKeyName && cmdp)
  { for(i = 0;i < 0xfffff;i++)
    { if(!cmdp[i].keyname || !cmdp[i].value) break;
    }
    if(startidx <= i)
    { for(;startidx <= i;startidx++)
       if(cmdp[startidx].keyname && !strcmp(szKeyName,cmdp[startidx].keyname)) return startidx;
    }
  }
  return (-1);
}
/* --------------------- sscGetPairCounterByKey ------------------------------ */
int SSCAPI sscGetPairCounterByKey(CMDPAIR *cmdp,const char *szKeyName)
{ int i;
  int cnt = 0;
  
  if(szKeyName && cmdp)
  { for(i = 0;cmdp[i].keyname && i < 0xffffff;i++)
    { if(!strcasecmp(szKeyName,cmdp[i].keyname)) cnt++;
    }
  }
  return cnt;
}
/* ------------------- sscGetPairStrictCounterByKey -------------------------- */
int SSCAPI sscGetPairStrictCounterByKey(CMDPAIR *cmdp,const char *szKeyName)
{ int i;
  int cnt = 0;
  
  if(szKeyName && cmdp)
  { for(i = 0;cmdp[i].keyname && i < 0xffffff;i++)
    { if(!strcmp(szKeyName,cmdp[i].keyname)) cnt++;
    }
  }
  return cnt;
}
