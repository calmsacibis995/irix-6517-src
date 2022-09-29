/* --------------------------------------------------------------------------- */
/* -                             SSRV_BASE64.C                               - */
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

#include "ssrv_base64.h"

/* ------------------------------ base2i ------------------------------------- */
static unsigned long base2i(char base)
{ if(base >= 'A' && base <= 'Z') return base-'A';
  if(base >= 'a' && base <= 'z') return base-'a'+26;
  if(base >= '0' && base <= '9') return base-'0'+52;
  if(base == '+') return 62;
  if(base == '/') return 63;
  return 0xffff;
}
/* ----------------------- ssrvDecodeBase64String ---------------------------- */
int ssrvDecodeBase64String(const char *code, char *str, int dstsize)
{ unsigned long rc, bits;
  int in, out, bitsLen, codeLen;

  if(code && code[0] && str && dstsize)
  { codeLen = strlen(code);
    for(bits = 0,in = (out = (bitsLen = 0));in <= codeLen && out < dstsize;) 
    { if(bitsLen >= 8) 
      { str[out++] = (char)((bits>>(bitsLen-8)) & 0xff);
        bitsLen -= 8;
      }
      if(code[in] == '=' || code[in] == 0) break;
      if(bitsLen <= 18)
      { rc = base2i(code[in++]);
        if(rc == 0xffff) return 0;
        bits <<= 6; bits |= rc & 0x3f;
        bitsLen += 6;
      }
    }
    str[out] = 0;
  }
  return 1;
}
