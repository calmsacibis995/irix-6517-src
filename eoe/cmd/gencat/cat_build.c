/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)gencat:cat_build.c	1.1.3.1"

#include <dirent.h>
#include <stdio.h>
#include "nl_types.h"
#include <malloc.h>
#include <pfmt.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/fcntl.h>

extern int list;
extern FILE *tempfile;
extern char msg_buf[];
extern const char nomem[], badread[], badwritetmp[];

extern struct cat_set *sets;
static void strcpy_conv();
static void cat_malloc_build(FILE *, char *);

static void cat_mmp_build(FILE *, char *);

/*
 * Read a catalog and build the internal structure
 */
int
cat_build(catname)
  char *catname;
{
  FILE *fd;
  long magic;
  
    
  /*
   * Test for mkmsgs
   * or old style malloc
   */

  if ((fd = fopen(catname, "r")) == 0){
    /*
     * does not exist
     */
    return 0;
  }

  if (fread(&magic, sizeof(long), 1, fd) != 1){
    pfmt(stderr, MM_ERROR, badread, catname, strerror(errno));
    fatal();
  }
  if (magic == CAT_MAGIC) 
    cat_malloc_build(fd,catname);
  else 
    cat_mmp_build(fd,catname);

  fclose(fd);
  
  return 1;
}

static void
cat_malloc_build (fd,catname)
  FILE *fd;
  char  *catname;
{
  struct cat_hdr hdr;
  struct cat_set *cur_set, *set_last;
  struct cat_msg *cur_msg, *msg_last;
  char *data;
  struct _cat_set_hdr set_hdr;
  struct _cat_msg_hdr msg_hdr;
  register int i;

    
  /*
   * Read malloc file header
   */
  rewind(fd);
  if (fread((char *)&hdr, sizeof(struct cat_hdr), 1, fd) != 1){
    pfmt(stderr, MM_ERROR, badread, catname, strerror(errno));
    fatal();
  }
  /*
   * Read sets headers
   */
  for (i = 0 ; i < hdr.hdr_set_nr ; i++){
    struct cat_set *new;
    if ((new = (struct cat_set *)malloc(sizeof(struct cat_set))) == 0){
      pfmt(stderr, MM_ERROR, nomem, strerror(errno));
      fatal();
    }
    if (fread((char *)&set_hdr, sizeof(struct _cat_set_hdr), 1, fd) != 1){
      pfmt(stderr, MM_ERROR, badread, catname, strerror(errno));
      fatal();
    }
    new->set_nr = set_hdr.shdr_set_nr;
    new->set_msg_nr = set_hdr.shdr_msg_nr;
    new->set_next = 0;
    if (sets == 0)
      sets = new;
    else
      set_last->set_next = new;
    set_last = new;
  }

  /*
   * Read messages headers
   */
  for (cur_set = sets ; cur_set != 0 ; cur_set = cur_set->set_next){
    cur_set->set_msg = 0;
    for (i = 0 ; i < cur_set->set_msg_nr ; i++){
      struct cat_msg *new;

      if ((new = (struct cat_msg *)malloc(sizeof(struct cat_msg))) == 0){
        pfmt(stderr, MM_ERROR, nomem, strerror(errno));
        fatal();
      }
      if (fread((char *)&msg_hdr, sizeof(struct _cat_msg_hdr), 1, fd) != 1){
        pfmt(stderr, MM_ERROR, badread, catname, strerror(errno));
        fatal();
      }
      new->msg_nr = msg_hdr.msg_nr;
      new->msg_len = msg_hdr.msg_len;
      new->msg_next = 0;
      if (cur_set->set_msg == 0)
        cur_set->set_msg = new;
      else
        msg_last->msg_next = new;
      msg_last = new;
    }
  }
  
  /*
   * Read messages.
   */
  for (cur_set = sets ; cur_set != 0 ; cur_set = cur_set->set_next){
    for (cur_msg = cur_set->set_msg ;cur_msg!= 0;cur_msg= cur_msg->msg_next){

      if (fread(msg_buf, 1, cur_msg->msg_len, fd) != cur_msg->msg_len ){
        pfmt(stderr, MM_ERROR, badread, catname, strerror(errno));
        fatal();
      }

      /*
       * Put message in the temp file and keep offset
       */
      cur_msg->msg_off = ftell(tempfile);

      if (list)
	pfmt(stdout, MM_INFO, 
		":1:Set %d,Message %d,Offset %d,Length %d\n%.*s\n*\n",
		cur_set->set_nr, cur_msg->msg_nr, cur_msg->msg_off,
		cur_msg->msg_len, cur_msg->msg_len, msg_buf);

      if (fwrite(msg_buf, 1, cur_msg->msg_len, tempfile) != cur_msg->msg_len){
        pfmt(stderr, MM_ERROR, badwritetmp, strerror(errno));
        fatal();
      }
    }
  }
}

static void
cat_mmp_build (fd,catname)
  FILE *fd;
  char  *catname;
{
  struct cat_set *cur_set, *set_last;
  struct cat_msg *cur_msg, *msg_last;
  struct _m_cat_set set;
  register int i;
  register int j;
  register int k;
  int no_sets;
  int msg_ptr;
  int mfd;
  caddr_t addr;
  char *p;
  struct cat_set *new;
  char message_file[MAXNAMLEN];
  char *temp_text = NULL;		/* was char temp_text[NL_TEXTMAX] */
  struct stat sb;
  extern int nl_textmax;

  /*
   * get the number of sets
   * of a set file
   */
  rewind(fd);
  if (fread(&no_sets, 1, sizeof(int), fd) != sizeof(int) ){
    pfmt(stderr, MM_ERROR, ":2:%s: Cannot get number of sets\n", catname);
    fatal();
  }
  
  /*  Open the message file for reading  */
  sprintf(message_file, "%s.m", catname);

  if ((mfd = open(message_file, O_RDONLY)) < 0) {
    pfmt(stderr, MM_ERROR, "0:Cannot open message file %s:%s\n", 
      message_file, strerror(errno));
    fatal();
  }

  if (fstat(mfd, &sb) < 0)
  {
    pfmt(stderr, MM_ERROR, "0:Could not stat message file: %s\n",
      strerror(errno));
    fatal();
  }

  if ((temp_text = (char *)malloc(nl_textmax)) == 0){
      pfmt(stderr, MM_ERROR, nomem, strerror(errno));
      fatal();
  }

  addr = mmap(0, sb.st_size, PROT_READ, MAP_SHARED, mfd, 0);
  close(mfd);

  if (addr == (caddr_t)-1)
  {
    pfmt(stderr, MM_ERROR, "0:cannot map message file: %s\n", strerror(errno));
    fatal();
  }

  for(i=1;i<=no_sets;i++) {
    if (fread(&set,1,sizeof(struct _m_cat_set),fd) != sizeof(struct _m_cat_set) ){
      pfmt(stderr, MM_ERROR, ":4:%s: Cannot get set information\n", catname);
      munmap(addr, sb.st_size);
      fatal();
    }
    if ((new = (struct cat_set *)malloc(sizeof(struct cat_set))) == 0){
      pfmt(stderr, MM_ERROR, nomem, strerror(errno));
      munmap(addr, sb.st_size);
      fatal();
    }
    if (set.first_msg == 0)
      continue;
    new->set_nr = i;
    new->set_next = 0;
    if (sets == 0)
      sets = new;
    else
      set_last->set_next = new;
    set_last = new;

    /*
     * create message headers
     */
    new->set_msg = 0;
    for(j=1, k=set.first_msg; j <=set.last_msg && k != 0; k++, j++) {
        msg_ptr = *((int *)(addr + (k * sizeof(int))));
        p = addr + msg_ptr;
        strcpy_conv(temp_text, p);

	if (strcmp(temp_text,DFLT_MSG)) {
          struct cat_msg *new_msg;
          if ((new_msg=(struct cat_msg *)malloc(sizeof(struct cat_msg))) == 0){
            pfmt(stderr, MM_ERROR, nomem, strerror(errno));
	    munmap(addr, sb.st_size);
            fatal();
	  }
          if (new->set_msg == 0) 
	    new->set_msg = new_msg;
	  else
	    msg_last->msg_next = new_msg;
	  new_msg->msg_nr = j;
	  new_msg->msg_len = strlen(temp_text)+1;
	  new_msg->msg_next = 0;
	  msg_last = new_msg;
	  /*
	   * write message to tempfile
	   */
          new_msg->msg_off = ftell(tempfile);
	  if (list)
	    pfmt(stdout, MM_INFO, ":5:Set %d,Message %d,Text %s\n*\n", i, j, 
	    	temp_text);
          if(fwrite(temp_text,1,new_msg->msg_len,tempfile)!=new_msg->msg_len){
            pfmt(stderr, MM_ERROR, badwritetmp, strerror(errno));
	    munmap(addr, sb.st_size);
            fatal();
          }
	}
    }
  }
  munmap(addr, sb.st_size);
  
  free(temp_text);
}

/* 
 * mkmsgs cant handle the new line
 */
static void
strcpy_conv ( a , b )
char *a,*b;
{

  while (*b) {

    switch (*b) {

      case '\n' :
        *a++ = '\\';
        *a++ = 'n';
	b++;
        break;

      case '\\' :
        *a++ = '\\';
        *a++ = '\\';
	b++;
        break;

      default :
	*a++ = *b++;
    }
  }
  *a = '\0';
}
