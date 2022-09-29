/* Copyright (C) 1998 TcX AB & Monty Program KB & Detron HB

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


/*
 Functions to handle the encode() and decode() functions
 The strongness of this crypt is large based on how good the random
 generator is.  It should be ok for short strings, but for communication one
 needs something like 'ssh'.
*/

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"

SQL_CRYPT::SQL_CRYPT(const char *password)
{
  ulong rand_nr[2];
  hash_password(rand_nr,password);
  crypt_init(rand_nr);
}

void SQL_CRYPT::crypt_init(ulong *rand_nr)
{
  uint i;
  randominit(&rand,rand_nr[0],rand_nr[1]);

  for (i=0 ; i<=255; i++)
   decode_buff[i]= (char) i;

  for (i=0 ; i<= 255 ; i++)
  {
    int index= (uint) (rnd(&rand)*255.0);
    char a= decode_buff[index];
    decode_buff[index]= decode_buff[i];
    decode_buff[+i]=a;
  }
  for (i=0 ; i <= 255 ; i++)
   encode_buff[(unsigned char) decode_buff[i]]=i;
  org_rand=rand;
  shift=0;
}


void SQL_CRYPT::encode(char *str,uint length)
{
  for (uint i=0; i < (int) length; i++)
  {
    shift^=(uint) (rnd(&rand)*255.0);
    uint index= (uint) (uchar) str[0];
    *str++ = (char) ((uchar) encode_buff[index] ^ shift);
    shift^= index;
  }
}


void SQL_CRYPT::decode(char *str,uint length)
{
  for (uint i=0; i < length; i++)
  {
    shift^=(uint) (rnd(&rand)*255.0);
    uint index= (uint) ((unsigned char) str[0] ^ shift);
    *str = decode_buff[index];
    shift^= (uint) (uchar) *str++;
  }
}
