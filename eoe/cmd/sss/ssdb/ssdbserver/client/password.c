/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

/* password checking routines */
/*****************************************************************************
  The main idea is that no password are sent between client & server on
  connection and that no password are saved in mysql in a decodable form.

  On connection a random string is generated and sent to the client.
  The client generates a new string with a random generator inited with
  the hash values from the password and the sent string.
  This 'check' string is sent to the server where it is compared with
  a string generated from the stored hash_value of the password and the
  random string.

  The password is saved (in user.password) by using the PASSWORD() function in
  mysql.

  Example:
    update user set password=PASSWORD("hello") where user="test"
  This saves a hashed number as a string in the password field.
*****************************************************************************/

#include <global.h>
#include <my_sys.h>
#include <m_string.h>
#include "mysql.h"


void randominit(struct rand_struct *rand,ulong seed1, ulong seed2)
{						/* For mysql 3.21.# */
  rand->max_value= 0x3FFFFFFFL;
  rand->max_value_dbl=(double) rand->max_value;
  rand->seed1=seed1%rand->max_value ;
  rand->seed2=seed2%rand->max_value;
}

void old_randominit(struct rand_struct *rand,ulong seed1)
{						/* For mysql 3.20.# */
  rand->max_value= 0x01FFFFFFL;
  rand->max_value_dbl=(double) rand->max_value;
  seed1%=rand->max_value;
  rand->seed1=seed1 ; rand->seed2=seed1/2;
}

double rnd(struct rand_struct *rand)
{
  rand->seed1=(rand->seed1*3+rand->seed2) % rand->max_value;
  rand->seed2=(rand->seed1+rand->seed2+33) % rand->max_value;
  return (((double) rand->seed1)/rand->max_value_dbl);
}

void hash_password(ulong *result, const char *password)
{
  register ulong nr=1345345333L, add=7, nr2=0x12345671L;
  ulong tmp;
  for (; *password ; password++)
  {
    if (*password == ' ' || *password == '\t')
      continue;			/* skipp space in password */
    tmp= (ulong) (uchar) *password;
    nr^= (((nr & 63)+add)*tmp)+ (nr << 8);
    nr2+=(nr2 << 8) ^ nr;
    add+=tmp;
  }
  result[0]=nr & (((ulong) 1L << 31) -1L); /* Don't use sign bit (str2int) */;
  result[1]=nr2 & (((ulong) 1L << 31) -1L);
  return;
}

void make_scrambled_password(char *to,const char *password)
{
  ulong hash_res[2];
  hash_password(hash_res,password);
  sprintf(to,"%08lx%08lx",hash_res[0],hash_res[1]);
}

static inline uint char_val(char X)
{
  return (uint) (X >= '0' && X <= '9' ? X-'0' :
		 X >= 'A' && X <= 'Z' ? X-'A'+10 :
		 X-'a'+10);
}

/*
** This code assumes that len(password) is divideable with 8 and that
** res is big enough (2 in mysql)
*/

void get_salt_from_password(ulong *res,const char *password)
{
  res[0]=res[1]=0;
  if (password)
  {
    while (*password)
    {
      ulong val=0;
      uint i;
      for (i=0 ; i < 8 ; i++)
	val=(val << 4)+char_val(*password++);
      *res++=val;
    }
  }
  return;
}

/*
 * Genererate a new message based on message and password
 * The same thing is done in client and server and the results are checked.
 */

char *scramble(char *to,const char *message,const char *password,
	       my_bool old_ver)
{
  struct rand_struct rand;
  ulong hash_pass[2],hash_message[2];
  if (password && password[0])
  {
    char *to_start=to;
    hash_password(hash_pass,password);
    hash_password(hash_message,message);
    if (old_ver)
      old_randominit(&rand,hash_pass[0] ^ hash_message[0]);
    else
      randominit(&rand,hash_pass[0] ^ hash_message[0],
		 hash_pass[1] ^ hash_message[1]);
    while (*message++)
      *to++= (char) (floor(rnd(&rand)*31)+64);
    if (!old_ver)
    {						/* Make it harder to break */
      char extra=(char) (floor(rnd(&rand)*31));
      while (to_start != to)
	*(to_start++)^=extra;
    }
  }
  *to=0;
  return to;
}


my_bool check_scramble(const char *scramble, const char *message,
		       ulong *hash_pass, my_bool old_ver)
{
  struct rand_struct rand;
  ulong hash_message[2];
  char buff[16],*to,extra;			/* Big enough for check */
  const char *pos;

  hash_password(hash_message,message);
  if (old_ver)
    old_randominit(&rand,hash_pass[0] ^ hash_message[0]);
  else
    randominit(&rand,hash_pass[0] ^ hash_message[0],
	       hash_pass[1] ^ hash_message[1]);
  to=buff;
  for (pos=scramble ; *pos ; pos++)
    *to++=(char) (floor(rnd(&rand)*31)+64);
  if (old_ver)
    extra=0;
  else
    extra=(char) (floor(rnd(&rand)*31));
  to=buff;
  while (*scramble)
  {
    if (*scramble++ != (char) (*to++ ^ extra))
      return 1;					/* Wrong password */
  }
  return 0;
}
