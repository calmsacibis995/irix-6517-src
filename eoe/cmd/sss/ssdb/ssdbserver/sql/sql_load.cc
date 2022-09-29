/* Copyright (C) 1979-1997 TcX AB & Monty Program KB & Detron HB

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

/* Copy data from a textfile to table */

#include "mysql_priv.h"
#include "handler.h"
#include <my_dir.h>
#include <m_ctype.h>

class READ_INFO {
  File	file;
  byte	*buffer,			/* Buffer for read text */
	*end_of_buff;			/* Data in bufferts ends here */
  uint	buff_length,			/* Length of buffert */
	max_length;			/* Max length of row */
  char	*field_term_ptr,*line_term_ptr,*line_start_ptr,*line_start_end;
  uint	field_term_length,line_term_length,enclosed_length;
  int	field_term_char,line_term_char,enclosed_char,escape_char;
  int	*stack,*stack_pos;
  bool	found_end_of_line,start_of_line,eof;
  IO_CACHE cache;
  NET *io_net;

public:
  bool error,line_cuted,found_null,enclosed;
  byte	*row_start,			/* Found row starts here */
	*row_end;			/* Found row ends here */

  READ_INFO(File file,uint tot_length,
	    String &field_term,String &line_start,String &line_term,
	    String &enclosed,int escape,bool get_it_from_net);
  ~READ_INFO();
  int read_field();
  int read_fixed_length(void);
  int next_line(void);
  char unescape(char chr);
  int terminator(char *ptr,uint length);
  bool find_start_of_fields();
};

static int read_fixed_length(THD *thd,COPY_INFO &info,TABLE *table,
			     List<Item> &fields, READ_INFO &read_info);
static int read_sep_field(THD *thd,COPY_INFO &info,TABLE *table,
			  List<Item> &fields, READ_INFO &read_info,
			  String &enclosed);


int mysql_load(THD *thd,sql_exchange *ex,TABLE_LIST *table_list,
	       List<Item> &fields, enum enum_duplicates handle_duplicates,
	       bool read_file_from_client)
{
  char name[FN_REFLEN];
  File file;
  TABLE *table;
  int error;
  String *field_term=ex->field_term,*escaped=ex->escaped,
    *enclosed=ex->enclosed;
  DBUG_ENTER("mysql_load");

  if (escaped->length() > 1 || enclosed->length() > 1)
  {
    my_message(ER_WRONG_FIELD_TERMINATORS,ER(ER_WRONG_FIELD_TERMINATORS),
	       MYF(0));
    DBUG_RETURN(-1);
  }

  if (!(table = open_ltable(thd,table_list,TL_WRITE)))
    DBUG_RETURN(-1);
  if (!fields.elements)
  {
    Field **field;
    for (field=table->field; *field ; field++)
      fields.push_back(new Item_field(*field));
  }
  else
  {						// Part field list
    thd->dupp_field=0;
    if (setup_fields(thd,table_list,fields,1,0) < 0)
      DBUG_RETURN(-1);
    if (thd->dupp_field)
    {
      my_error(ER_FIELD_SPECIFIED_TWICE, MYF(0), thd->dupp_field->field_name);
      DBUG_RETURN(-1);
    }
  }

  uint tot_length=0;
  bool use_blobs=0,use_timestamp=0;
  List_iterator<Item> it(fields);

  Item_field *field;
  while ((field=(Item_field*) it++))
  {
    if (field->field->flags & BLOB_FLAG)
    {
      use_blobs=1;
      tot_length+=256;				// Will be extended if needed
    }
    else
      tot_length+=field->field->field_length;
    if (!field_term->length() && !(field->field->flags & NOT_NULL_FLAG))
      field->field->set_notnull();
    if (field->field == table->timestamp_field)
      use_timestamp=1;
  }
  if (use_blobs && !ex->line_term->length() && !field_term->length())
  {
    my_message(ER_BLOBS_AND_NO_TERMINATED,ER(ER_BLOBS_AND_NO_TERMINATED),
	       MYF(0));
    DBUG_RETURN(-1);
  }

  /* We can't give an error in the middle when using LOCAL files */
  if (read_file_from_client && handle_duplicates == DUP_ERROR)
    handle_duplicates=DUP_IGNORE;

  if (read_file_from_client && (thd->client_capabilities & CLIENT_LOCAL_FILES))
  {
    char tmp [FN_REFLEN+1],*end;
    DBUG_PRINT("info",("reading local file"));
    tmp[0] = (char) 251;				/* NULL_LENGTH */
    end=strnmov(tmp+1,ex->file_name,sizeof(tmp)-2);
    (void) my_net_write(&thd->net,tmp,(uint) (end-tmp));
    (void) net_flush(&thd->net);
    file = -1;
  }
  else
  {
    read_file_from_client=0;
#ifdef DONT_ALLOW_FULL_LOAD_DATA_PATHS
    ex->file_name+=dirname_length(ex->file_name);
#endif
    if (!dirname_length(ex->file_name) &&
	strlen(ex->file_name)+strlen(mysql_data_home)+strlen(thd->db)+3 <
	FN_REFLEN)
    {
      (void) sprintf(name,"%s/%s/%s",mysql_data_home,thd->db,ex->file_name);
      unpack_filename(name,name);		/* Convert to system format */
    }
    else
    {
      unpack_filename(name,ex->file_name);
#ifndef	__WIN32__
      MY_STAT stat_info;
      if (!my_stat(name,&stat_info,MYF(MY_FAE)))
	DBUG_RETURN(-1);
      if (!(stat_info.st_mode & S_IROTH))
      {
	my_error(ER_TEXTFILE_NOT_READABLE,MYF(0),name);
	DBUG_RETURN(-1);
      }
#endif
    }
    if ((file=my_open(name,O_RDONLY,MYF(MY_WME))) < 0)
      DBUG_RETURN(-1);
  }

  COPY_INFO info;
  bzero((char*) &info,sizeof(info));
  info.handle_duplicates=handle_duplicates;
  info.escape_char=escaped->length() ? (*escaped)[0] : INT_MAX;

  READ_INFO read_info(file,tot_length,*field_term,
		      *ex->line_start, *ex->line_term, *enclosed,
		      info.escape_char,read_file_from_client);
  if (read_info.error)
  {
    if  (file >= 0)
      my_close(file,MYF(0));			// no files in net reading
    DBUG_RETURN(-1);				// Can't allocate buffers
  }

  restore_record(table,2);

  thd->count_cuted_fields=1;			/* calc cuted fields */
  thd->cuted_fields=0L;
  if (ex->line_term->length() && field_term->length())
  {
    while (ex->skip_lines--)
    {
      if (read_info.next_line())
	break;
    }
  }
  if (!(error=test(read_info.error)))
  {
    uint save_time_stamp=table->time_stamp;
    if (use_timestamp)
      table->time_stamp=0;
    table->next_number_field=table->found_next_number_field;
    VOID(ha_extra(table,HA_EXTRA_WRITE_CACHE));
    table->copy_blobs=1;
    if (!field_term->length() && !enclosed->length())
      error=read_fixed_length(thd,info,table,fields,read_info);
    else
      error=read_sep_field(thd,info,table,fields,read_info,*enclosed);
    if (ha_extra(table,HA_EXTRA_NO_CACHE))
      error=1; /* purecov: inspected */
    table->time_stamp=save_time_stamp;
    table->next_number_field=0;
    if (thd->lock)
    {
      mysql_unlock_tables(thd->lock);
      thd->lock=0;
    }
  }
  if  (file >= 0) my_close(file,MYF(0));
  free_blobs(table);				/* if pack_blob was used */
  table->copy_blobs=0;
  thd->count_cuted_fields=0;			/* Don`t calc cuted fields */
  if (error)
    DBUG_RETURN(-1);				// Error on read
  sprintf(name,ER(ER_LOAD_INFO),info.records,info.deleted,
	  info.records-info.copied,thd->cuted_fields);
  send_ok(&thd->net,info.copied+info.deleted,0L,name);
  mysql_update_log.write(thd->query);
  DBUG_RETURN(0);
}


/****************************************************************************
** Read of rows of fixed size + optional garage + optonal newline
****************************************************************************/

static int
read_fixed_length(THD *thd,COPY_INFO &info,TABLE *table,List<Item> &fields,
		  READ_INFO &read_info)
{
  List_iterator<Item> it(fields);
  Item_field *sql_field;
  DBUG_ENTER("read_fixed_length");

  /* No fields can be null in this format. mark all fields as not null */
  while ((sql_field= (Item_field*) it++))
      sql_field->field->set_notnull();

  while (!read_info.read_fixed_length())
  {
    if (thd->killed)
    {
      my_error(ER_SERVER_SHUTDOWN,MYF(0));
      DBUG_RETURN(1);
    }
    it.rewind();
    byte *pos=read_info.row_start;
#ifdef HAVE_purify
    read_info.row_end[0]=0;
#endif
    while ((sql_field= (Item_field*) it++))
    {
      Field *field=sql_field->field;
      if (pos == read_info.row_end)
      {
	thd->cuted_fields++;			/* Not enough fields */
	field->reset();
      }
      else
      {
	uint length;
	byte save_chr;
	if ((length=(uint) (read_info.row_end-pos)) >
	    field->field_length)
	  length=field->field_length;
	save_chr=pos[length]; pos[length]='\0';	// Safeguard aganst malloc
	field->store((char*) pos,length);
	pos[length]=save_chr;
	if ((pos+=length) > read_info.row_end)
	  pos= read_info.row_end;	/* Fills rest with space */
      }
    }
    if (pos != read_info.row_end)
      thd->cuted_fields++;			/* To long row */
    if (write_record(thd,table,&info))
      DBUG_RETURN(1);
    if (table->next_number_field)
      table->next_number_field->reset();	// Clear for next record
    if (read_info.next_line())			// Skipp to next line
      break;
    if (read_info.line_cuted)
      thd->cuted_fields++;			/* To long row */
  }
  DBUG_RETURN(test(read_info.error));
}



static int
read_sep_field(THD *thd,COPY_INFO &info,TABLE *table,
	       List<Item> &fields, READ_INFO &read_info,
	       String &enclosed)
{
  List_iterator<Item> it(fields);
  Item_field *sql_field;
  uint enclosed_length;
  DBUG_ENTER("read_sep_field");

  enclosed_length=enclosed.length();

  for (;;it.rewind())
  {
    if (thd->killed)
    {
      my_error(ER_SERVER_SHUTDOWN,MYF(0));
      DBUG_RETURN(1);
    }
    while ((sql_field=(Item_field*) it++))
    {
      uint length;
      byte *pos;

      if (read_info.read_field())
	break;
      pos=read_info.row_start;
      length=(uint) (read_info.row_end-pos);
      Field *field=sql_field->field;

      if (!read_info.enclosed &&
	  (enclosed_length && length == 4 && !memcmp(pos,"NULL",4)) ||
	  (length == 1 && read_info.found_null))
      {
	field->reset();
	field->set_null();
	if (!field->maybe_null())
	{
	  if (field->type() == FIELD_TYPE_TIMESTAMP)
	    ((Field_timestamp*) field)->set_time();
	  else
	    thd->cuted_fields++;
	}
	continue;
      }
      field->set_notnull();
      read_info.row_end[0]=0;			// Safe to change end marker
      field->store((char*) read_info.row_start,length);
    }
    if (read_info.error)
      break;
    if (sql_field)
    {						// Last record
      if (sql_field == (Item_field*) fields.head())
	break;
      for ( ; sql_field ; sql_field=(Item_field*) it++)
      {
	sql_field->field->set_null();
	sql_field->field->reset();
	thd->cuted_fields++;
      }
    }
    if (write_record(thd,table,&info))
      DBUG_RETURN(1);
    if (table->next_number_field)
      table->next_number_field->reset();	// Clear for next record
    if (read_info.next_line())			// Skipp to next line
      break;
    if (read_info.line_cuted)
      thd->cuted_fields++;			/* To long row */
  }
  DBUG_RETURN(test(read_info.error));
}


/* Unescape all escape characters, mark \N as null */

char
READ_INFO::unescape(char chr)
{
  switch(chr) {
  case 'n': return '\n';
  case 't': return '\t';
  case 'r': return '\r';
  case 'b': return '\b';
  case '0': return 0;				// Ascii null
  case 'Z': return '\032';			// Win32 end of file
  case 'N': found_null=1;

    /* fall through */
  default:  return chr;
  }
}


	/* Read a line using buffering */
	/* If last line is empty (in line mode) then it isn't outputed */


READ_INFO::READ_INFO(File file_par, uint tot_length, String &field_term,
		     String &line_start, String &line_term,
		     String &enclosed_par, int escape, bool get_it_from_net)
  :file(file_par),escape_char(escape)
{
  field_term_ptr=(char*) field_term.ptr();
  field_term_length= field_term.length();
  line_term_ptr=(char*) line_term.ptr();
  line_term_length= line_term.length();
  if (line_start.length() == 0)
  {
    line_start_ptr=0;
    start_of_line= 0;
  }
  else
  {
    line_start_ptr=(char*) line_start.ptr();
    line_start_end=line_start_ptr+line_start.length();
    start_of_line= 1;
  }
  /* If field_terminator == line_terminator, don't use line_terminator */
  if (field_term_length == line_term_length &&
      !memcmp(field_term_ptr,line_term_ptr,field_term_length))
  {
    line_term_length=0;
    line_term_ptr="";
  }
  enclosed_char= (enclosed_length=enclosed_par.length()) ? enclosed_par[0] :
    INT_MAX;
  field_term_char= field_term_length ? field_term_ptr[0] : INT_MAX;
  line_term_char= line_term_length ? line_term_ptr[0] : INT_MAX;
  error=eof=found_end_of_line=found_null=line_cuted=0;
  buff_length=tot_length;


  /* Set of a stack for unget if long terminators */
  uint length=max(field_term_length,line_term_length)+1;
  set_if_bigger(length,line_start.length());
  stack=stack_pos=(int*) sql_alloc(sizeof(int)*length);

  if (!(buffer=(byte*) my_malloc(buff_length+1,MYF(0))))
    error=1; /* purecov: inspected */
  else
  {
    end_of_buff=buffer+buff_length;
    if (init_io_cache(&cache,file,0,
		      (get_it_from_net) ? READ_NET : READ_CACHE,0L,1,
		      MYF(MY_WME)))
    {
      my_free((gptr) buffer,MYF(0)); /* purecov: inspected */
      error=1;
    }
  }
}


READ_INFO::~READ_INFO()
{
  if (!error)
  {
    end_io_cache(&cache);
    my_free((gptr) buffer,MYF(0));
    error=1;
  }
}


#define GET (stack_pos != stack ? *--stack_pos : my_b_get(&cache))
#define PUSH(A) *(stack_pos++)=(A)


inline int READ_INFO::terminator(char *ptr,uint length)
{
  int chr=0;					// Keep gcc happy
  uint i;
  for (i=1 ; i < length ; i++)
  {
    if ((chr=GET) != *++ptr)
    {
      break;
    }
  }
  if (i == length)
    return 1;
  PUSH(chr);
  while (i-- > 1)
    PUSH(*--ptr);
  return 0;
}


int READ_INFO::read_field()
{
  int chr,found_enclosed_char;
  byte *to,*new_buffer;

  found_null=0;
  if (found_end_of_line)
    return 1;					// One have to call next_line

  /* Skipp until we find 'line_start' */

  if (start_of_line)
  {						// Skipp until line_start
    start_of_line=0;
    if (find_start_of_fields())
      return 1;
  }
  if ((chr=GET) == my_b_EOF)
  {
    found_end_of_line=eof=1;
    return 1;
  }
  to=buffer;
  if (chr == enclosed_char)
  {
    found_enclosed_char=enclosed_char;
    *to++=(byte) chr;				// If error
  }
  else
  {
    found_enclosed_char= INT_MAX;
    PUSH(chr);
  }

  for (;;)
  {
    while ( to < end_of_buff)
    {
      chr = GET;
#ifdef USE_BIG5CODE
      if ((to+1 < end_of_buff) && isbig5head(chr))
      {
       *to++= (byte) chr;
       chr = GET;
       if (isbig5tail(chr))
       {
         *to++= (byte) chr;
         continue;
       }
      }
#elif defined(USE_MB)
      if (ismbhead(chr) && to+mbcharlen(chr) <= end_of_buff) {
	  uchar* p = (uchar*)to;
	  *to++ = chr;
	  int ml = mbcharlen(chr);
	  for (int i=1; i<ml; i++) {
	      chr = GET;
	      if (chr == my_b_EOF)
		  goto found_eof;
	      *to++ = chr;
	  }
	  if (ismbchar(p, (uchar*)to))
	      continue;
	  for (int i=1; i<ml; i++)
	      PUSH(*--to);
      }
#endif
			if (chr == my_b_EOF)
	goto found_eof;
      if (chr == escape_char)
      {
	if ((chr=GET) == my_b_EOF)
	{
	  *to++= (byte) escape_char;
	  goto found_eof;
	}
	*to++ = (byte) unescape((char) chr);
	continue;
      }
#ifdef ALLOW_LINESEPARATOR_IN_STRINGS
      if (chr == line_term_char)
#else
      if (chr == line_term_char && found_enclosed_char == INT_MAX)
#endif
      {
	if (terminator(line_term_ptr,line_term_length))
	{					// Maybe unexpected linefeed
	  enclosed=0;
	  found_end_of_line=1;
	  row_start=buffer;
	  row_end=  to;
	  return 0;
	}
      }
      if (chr == found_enclosed_char)
      {
	if ((chr=GET) == found_enclosed_char)
	{					// Remove dupplicated
	  *to++ = (byte) chr;
	  continue;
	}
	// End of enclosed field if followed by field_term or line_term
	if (chr == my_b_EOF ||
	    chr == line_term_char && terminator(line_term_ptr,
						line_term_length))
	{					// Maybe unexpected linefeed
	  enclosed=1;
	  found_end_of_line=1;
	  row_start=buffer+1;
	  row_end=  to;
	  return 0;
	}
	if (chr == field_term_char &&
	    terminator(field_term_ptr,field_term_length))
	{
	  enclosed=1;
	  row_start=buffer+1;
	  row_end=  to;
	  return 0;
	}
	/* Copy the found '"' character */
	PUSH(chr);
	chr='"';
      }
      else if (chr == field_term_char && found_enclosed_char == INT_MAX)
      {
	if (terminator(field_term_ptr,field_term_length))
	{
	  enclosed=0;
	  row_start=buffer;
	  row_end=  to;
	  return 0;
	}
      }
      *to++ = (byte) chr;
    }
    /*
    ** We come here if buffer is too small. Enlarge it and continue
    */
    if (!(new_buffer=(byte*) my_realloc((char*) buffer,buff_length+1+IO_SIZE,
					MYF(MY_WME))))
      return (error=1);
    to=new_buffer + (to-buffer);
    buffer=new_buffer;
    buff_length+=IO_SIZE;
    end_of_buff=buffer+buff_length;
  }

found_eof:
  enclosed=0;
  found_end_of_line=eof=1;
  row_start=buffer;
  row_end=to;
  return 0;
}

/*
** One can't use fixed length with USE_BIG5CODE, because with USE_BIG5CODE
** there can't be any fixed length records...
*/

int READ_INFO::read_fixed_length()
{
  int chr;
  byte *to;
  if (found_end_of_line)
    return 1;					// One have to call next_line

  if (start_of_line)
  {						// Skipp until line_start
    start_of_line=0;
    if (find_start_of_fields())
      return 1;
  }

  to=row_start=buffer;
  while (to < end_of_buff)
  {
    if ((chr=GET) == my_b_EOF)
      goto found_eof;
    if (chr == escape_char)
    {
      if ((chr=GET) == my_b_EOF)
      {
	*to++= (byte) escape_char;
	goto found_eof;
      }
      *to++ =(byte) unescape((char) chr);
      continue;
    }
    if (chr == line_term_char)
    {
      if (terminator(line_term_ptr,line_term_length))
      {						// Maybe unexpected linefeed
	found_end_of_line=1;
	row_end=  to;
	return 0;
      }
    }
    *to++ = (byte) chr;
  }
  row_end=to;					// Found full line
  return 0;

found_eof:
  found_end_of_line=eof=1;
  row_start=buffer;
  row_end=to;
  return to == buffer ? 1 : 0;
}


int READ_INFO::next_line()
{
  line_cuted=0;
  start_of_line= line_start_ptr != 0;
  if (found_end_of_line || eof)
  {
    found_end_of_line=0;
    return eof;
  }
  found_end_of_line=0;
  if (!line_term_length)
    return 0;					// No lines
  for (;;)
  {
    int chr;
#ifdef USE_BIG5CODE
   chr = GET;
   if (isbig5head(chr))
     if ((chr = GET) == escape_char)
      continue;
   if (chr == my_b_EOF)
#else
#ifdef USE_MB
   chr = GET;
   if (ismbhead(chr)) {
       for (int i=1; chr != my_b_EOF && i<mbcharlen(chr); i++)
	   chr = GET;
       if (chr == escape_char)
	   continue;
   }
   if (chr == my_b_EOF)
#else
   if ((chr=GET) == my_b_EOF)
#endif
#endif
   {
      eof=1;
      return 1;
    }
    if (chr == escape_char)
    {
      line_cuted=1;
      if (GET == my_b_EOF)
	return 1;
      continue;
    }
    if (chr == line_term_char && terminator(line_term_ptr,line_term_length))
      return 0;
    line_cuted=1;
  }
}


bool READ_INFO::find_start_of_fields()
{
  int chr;
 try_again:
  do
  {
    if ((chr=GET) == my_b_EOF)
    {
      found_end_of_line=eof=1;
      return 1;
    }
  } while ((char) chr != line_start_ptr[0]);
  for (char *ptr=line_start_ptr+1 ; ptr != line_start_end ; ptr++)
  {
    chr=GET;					// Eof will be checked later
    if ((char) chr != *ptr)
    {						// Can't be line_start
      PUSH(chr);
      while (--ptr != line_start_ptr)
      {					// Restart with next char
	PUSH(*ptr);
      }
      goto try_again;
    }
  }
  return 0;
}
