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

/* interface for memory mapped files */

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

class mapped_files :public ilink {
  byte *map;
  ha_rows size;
  char *name;					// name of mapped file
  File file;					// >= 0 if open
  int  error;					// If not mapped
  uint use_count;

public:
  mapped_files(const my_string name,byte *magic,uint magic_length);
  ~mapped_files();

  friend class mapped_file;
  friend mapped_files *map_file(const my_string name,byte *magic,
				uint magic_length);
  friend void unmap_file(mapped_files *map);
};


class mapped_file
{
  mapped_files *file;
public:
  mapped_file(const my_string name,byte *magic,uint magic_length)
  {
    file=map_file(name,magic,magic_length);	/* old or new map */
  }
  ~mapped_file()
  {
    unmap_file(file);				/* free map */
  }
  byte *map()
  {
    return file->map;
  }
};
