/* Copyright Abandoned 1998 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

/* readline for batch mode */

#include <global.h>
#include <my_sys.h>
#include <m_string.h>

typedef struct st_line_buffer
{
  File file;
  char *buffer;			/* The buffer itself, grown as needed. */
  char *end;			/* Pointer at buffer end */
  char *start_of_line,*end_of_line;
  uint bufread;			/* Number of bytes to get with each read(). */
  uint eof;
  ulong max_size;
} LINE_BUFFER;

static bool inited=0;
static LINE_BUFFER line_buff;

static int init_line_buffer(LINE_BUFFER *buffer,File file,ulong size,
			    ulong max_size);
static void free_line_buffer(LINE_BUFFER *buffer);
static uint fill_buffer(LINE_BUFFER *buffer);
static char *intern_read_line(LINE_BUFFER *buffer,uint *out_length);
static void init_line_buffer_from_string(LINE_BUFFER *buffer,my_string str);

my_bool batch_readline_init(ulong max_size)
{
  if (!inited && init_line_buffer(&line_buff,fileno(stdin),IO_SIZE,max_size))
    return 1;
  return 0;
}


char *batch_readline(void)
{
  char *pos;
  uint out_length;

  if (!inited)
    if (init_line_buffer(&line_buff,fileno(stdin),IO_SIZE,IO_SIZE*64))
      exit(1);
  if (!(pos=intern_read_line(&line_buff,&out_length)))
  {
    free_line_buffer(&line_buff);
    return 0;
  }
  if (out_length && pos[out_length-1] == '\n')
    out_length--;				/* Remove '\n' */
  pos[out_length]=0;
  return pos;
}


void batch_readline_end(void)
{
  free_line_buffer(&line_buff);
  inited=0;
}


void batch_readline_command(my_string str)
{
  init_line_buffer_from_string(&line_buff,str);
}

/*****************************************************************************
      Functions to handle buffered readings of lines from a stream
******************************************************************************/

int init_line_buffer(LINE_BUFFER *buffer,File file,ulong size,ulong max_buffer)
{
  inited=1;
  bzero((char*) buffer,sizeof(buffer[0]));
  buffer->file=file;
  buffer->bufread=size;
  buffer->max_size=max_buffer;
  if (!(buffer->buffer = my_malloc(buffer->bufread+1,MYF(MY_WME | MY_FAE))))
    return 1;
  buffer->end_of_line=buffer->end=buffer->buffer;
  buffer->buffer[0]=0;				/* For easy start test */
  return 0;
}


static void init_line_buffer_from_string(LINE_BUFFER *buffer,my_string str)
{
  uint length;
  inited=1;
  bzero((char*) buffer,sizeof(buffer[0]));
  length=strlen(str);
  buffer->buffer=buffer->start_of_line=buffer->end_of_line=
    (char*)my_malloc(length+2,MYF(MY_FAE));
  memcpy(buffer->buffer,str,length);
  buffer->buffer[length]='\n';
  buffer->buffer[length+1]=0;
  buffer->end=buffer->buffer+length+1;
  buffer->eof=1;
  buffer->max_size=1;
}


static void free_line_buffer(LINE_BUFFER *buffer)
{
  if (buffer->buffer)
  {
    my_free((gptr) buffer->buffer,MYF(0));
    buffer->buffer=0;
  }
}


/* Fill the buffer retaining the last n bytes at the beginning of the
   newly filled buffer (for backward context).	Returns the number of new
   bytes read from disk. */


static uint fill_buffer(LINE_BUFFER *buffer)
{
  uint read_count;
  uint bufbytes= (uint) (buffer->end - buffer->start_of_line);

  if (buffer->eof)
    return 0;					/* Everything read */

  /* See if we need to grow the buffer. */

  for (;;)
  {
    uint start_offset=(uint) (buffer->start_of_line - buffer->buffer);
    read_count=(buffer->bufread - bufbytes)/IO_SIZE;
    if ((read_count*=IO_SIZE))
      break;
    buffer->bufread *= 2;
    if (!(buffer->buffer = my_realloc(buffer->buffer, buffer->bufread+1,
				      MYF(MY_WME | MY_FAE))))
      return (uint) -1;
    buffer->start_of_line=buffer->buffer+start_offset;
    buffer->end=buffer->buffer+bufbytes;
  }

  /* Shift stuff down. */
  if (buffer->start_of_line != buffer->buffer)
  {
    bmove(buffer->buffer,buffer->start_of_line,(uint) bufbytes);
    buffer->end=buffer->buffer+bufbytes;
  }

  /* Read in new stuff. */
  if ((read_count= my_read(buffer->file, (byte*) buffer->end, read_count,
			   MYF(MY_WME))) == MY_FILE_ERROR)
    return read_count;

  DBUG_PRINT("fill_buff", ("Got %d bytes", read_count));

  /* Kludge to pretend every nonempty file ends with a newline. */
  if (!read_count && bufbytes && buffer->end[-1] != '\n')
  {
    buffer->eof = read_count = 1;
    *buffer->end = '\n';
  }
  buffer->end_of_line=(buffer->start_of_line=buffer->buffer)+bufbytes;
  buffer->end+=read_count;
  *buffer->end=0;				/* Sentinel */
  return read_count;
}



char *intern_read_line(LINE_BUFFER *buffer,uint *out_length)
{
  char *pos;
  uint length;
  DBUG_ENTER("intern_read_line");

  buffer->start_of_line=buffer->end_of_line;
  for (;;)
  {
    pos=buffer->end_of_line;
    while (*pos != '\n' && *pos)
      pos++;
    if (pos == buffer->end)
    {
      if ((uint) (pos - buffer->start_of_line) < buffer->max_size)
      {
	if (!(length=fill_buffer(buffer)) || length == (uint) -1)
	  DBUG_RETURN(0);
	continue;
      }
      pos--;					/* break line here */
    }
    buffer->end_of_line=pos+1;
    *out_length=(uint) (pos + 1 - buffer->eof - buffer->start_of_line);
    DBUG_RETURN(buffer->start_of_line);
  }
}
