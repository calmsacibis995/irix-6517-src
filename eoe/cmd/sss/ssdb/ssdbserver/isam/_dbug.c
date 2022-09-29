/* Copyright (C) 1979-1996 TcX AB & Monty Program KB & Detron HB

   This software is distributed with NO WARRANTY OF ANY KIND.  No author or
   distributor accepts any responsibility for the consequences of using it, or
   for whether it serves any particular purpose or works at all, unless he or
   she says so in writing.  Refer to the Free Public License (the "License")
   for full details.
   Every copy of this file must include a copy of the License, normally in a
   plain ASCII text file named PUBLIC.	The License grants you the right to
   copy, modify and redistribute this file, but only under certain conditions
   described in the License.  Among other things, the License requires that
   the copyright notice and this notice be preserved on all copies. */

/* Support rutiner with are using with dbug */

#include "isamdef.h"

	/* Print a key in user understandable format */

void _ni_print_key(FILE *stream, register N_KEYSEG *keyseg, const uchar *key)
{
  int flag;
  short int s_1;
  long	int l_1;
  float f_1;
  double d_1;
  uchar *end;

  VOID(fputs("Key: \"",stream));
  flag=0;
  for (; keyseg->base.type ;keyseg++)
  {
    if (flag++)
      VOID(putc('-',stream));
    end= (uchar*) key+ keyseg->base.length;
    switch (keyseg->base.type) {
    case HA_KEYTYPE_BINARY:
      if (!(keyseg->base.flag & HA_SPACE_PACK) && keyseg->base.length == 1)
      {						/* packed binary digit */
	VOID(fprintf(stream,"%d",(uint) *key++));
	break;
      }
      /* fall through */
    case HA_KEYTYPE_TEXT:
    case HA_KEYTYPE_NUM:
      if (keyseg->base.flag & HA_SPACE_PACK)
      {
	VOID(fprintf(stream,"%.*s",(int) *key,key+1));
	key+= (int) *key+1;
      }
      else
      {
	VOID(fprintf(stream,"%.*s",(int) keyseg->base.length,key));
	key=end;
      }
      break;
    case HA_KEYTYPE_INT8:
      VOID(fprintf(stream,"%d",(int) *((signed char*) key)));
      key=end;
      break;
    case HA_KEYTYPE_SHORT_INT:
      shortget(s_1,key);
      VOID(fprintf(stream,"%d",(int) s_1));
      key=end;
      break;
    case HA_KEYTYPE_USHORT_INT:
      {
	ushort u_1;
	ushortget(u_1,key);
	VOID(fprintf(stream,"%u",(uint) u_1));
	key=end;
	break;
      }
    case HA_KEYTYPE_LONG_INT:
      longget(l_1,key);
      VOID(fprintf(stream,"%ld",l_1));
      key=end;
      break;
    case HA_KEYTYPE_ULONG_INT:
      longget(l_1,key);
      VOID(fprintf(stream,"%lu",(ulong) l_1));
      key=end;
      break;
    case HA_KEYTYPE_INT24:
      VOID(fprintf(stream,"%ld",sint3korr(key)));
      key=end;
      break;
    case HA_KEYTYPE_UINT24:
      VOID(fprintf(stream,"%ld",uint3korr(key)));
      key=end;
      break;
    case HA_KEYTYPE_FLOAT:
      bmove((byte*) &f_1,(byte*) key,(int) sizeof(float));
      VOID(fprintf(stream,"%g",(double) f_1));
      key=end;
      break;
    case HA_KEYTYPE_DOUBLE:
      doubleget(d_1,key);
      VOID(fprintf(stream,"%g",d_1));
      key=end;
      break;
#ifdef HAVE_LONG_LONG
    case HA_KEYTYPE_LONGLONG:
    {
      longlong tmp;
      longlongget(tmp,key);
      VOID(fprintf(stream,"%g",(double) tmp));
      key=end;
      break;
    }
    case HA_KEYTYPE_ULONGLONG:
    {
      ulonglong tmp;
      longlongget(tmp,key);
      VOID(fprintf(stream,"%g",(double) (longlong) tmp));
      key=end;
      break;
    }
#endif
    default: break;			/* This never happens */
    }
  }
  VOID(fputs("\n",stream));
  return;
} /* print_key */
