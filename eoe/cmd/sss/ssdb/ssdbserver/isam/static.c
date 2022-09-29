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

/*
  Static variables for pisam library. All definied here for easy making of
  a shared library
*/

#ifndef _global_h
#include "isamdef.h"
#endif

LIST	*nisam_open_list=0;
uchar	NEAR nisam_file_magic[]=
{ (uchar) 254, (uchar) 254,'\005', '\002', };
uchar	NEAR nisam_pack_file_magic[]=
{ (uchar) 254, (uchar) 254,'\006', '\001', };
my_string    nisam_log_filename="isam.log";
File	nisam_log_file= -1;
uint	nisam_quick_table_bits=9;
uint	nisam_block_size=1024;			/* Best by test */
my_bool nisam_flush=0;

/* read_vec[] is used for converting between P_READ_KEY.. and SEARCH_ */
/* Position is , == , >= , <= , > , < */

uint NEAR nisam_read_vec[]=
{
  SEARCH_FIND, SEARCH_FIND | SEARCH_BIGGER, SEARCH_FIND | SEARCH_SMALLER,
  SEARCH_NO_FIND | SEARCH_BIGGER, SEARCH_NO_FIND | SEARCH_SMALLER,
};
