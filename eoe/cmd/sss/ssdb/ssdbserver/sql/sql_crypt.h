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

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

class SQL_CRYPT :public Sql_alloc
{
  struct rand_struct rand,org_rand;
  char decode_buff[256],encode_buff[256];
  uint shift;
  void crypt_init(ulong *seed);
 public:
  SQL_CRYPT(const char *seed);
  SQL_CRYPT(ulong *seed)
  {
    crypt_init(seed);
  }
  ~SQL_CRYPT() {}
  void init() { shift=0; rand=org_rand; }
  void encode(char *str, uint length);
  void decode(char *str, uint length);
};
