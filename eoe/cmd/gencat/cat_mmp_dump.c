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

#ident	"@(#)gencat:cat_mmp_dump.c	1.1.3.1"
#include <dirent.h>
#include <stdio.h>
#include <ctype.h>
#include <nl_types.h>
#include <pfmt.h>
#include <errno.h>
#include <string.h>

extern FILE *tempfile;
extern struct cat_set *sets;
static char *bldtmp();
extern char *msg_buf;
extern const char nomem[], badwritetmp[], badwrite[], badopen[], badcreate[],
  badseektmp[], badreadtmp[];
/*
 * dump a memory mapped gettxt library.
 */

void
cat_mmp_dump(catalog)
char *catalog;
{
  FILE *f_shdr;
  FILE *f_msg;
  struct cat_set *p_sets;
  struct cat_msg *p_msg;
  struct _m_cat_set set;
  int status;
  int msg_len;
  int nmsg = 1;
  int i;
  int no_sets = 0;
  char *tmp_file;
  char message_file[MAXNAMLEN];
  

  f_shdr = fopen(catalog,"w+");
  if (f_shdr == NULL) {
    pfmt(stderr, MM_ERROR, badopen, catalog, strerror(errno));
    fatal();
  }
  if (fwrite((char *)&no_sets, sizeof (no_sets) , 1, f_shdr) != 1){
    pfmt(stderr, MM_ERROR, badwrite, catalog, strerror(errno));
    fatal();
  }
  /*
   * create a tempory for messages
   */
  tmp_file = bldtmp();
  if (tmp_file == (char *)NULL) {
    pfmt(stderr, MM_ERROR, nomem, strerror(errno));
    fatal();
  }
  f_msg = fopen(tmp_file,"w+");
  if (f_msg == NULL) {
    pfmt(stderr, MM_ERROR, badcreate, tmp_file, strerror(errno));
    fatal();
  }
  /* 
   * for all the sets
   */
  p_sets = sets;
  nmsg = 1;
  while (p_sets != 0){
    no_sets++;
    /*
     * if set holes then
     * fill them
     */
    set.first_msg = 0;
    set.last_msg = 0;

    while (no_sets != p_sets->set_nr) {
      if (fwrite((char *)&set, sizeof (struct _m_cat_set) , 1, f_shdr) != 1){
        pfmt(stderr, MM_ERROR, badwritetmp, strerror(errno));
        fatal();
      }
      no_sets++;
    }
    p_msg = p_sets->set_msg;
    
    /*
     * Keep offset in shdr temp file to mark the set's begin
     */
    if (p_msg) {
      set.last_msg = 0;
      set.first_msg = nmsg;
      while (p_msg != 0){
        msg_len = p_msg->msg_len;
        /*
         * Get message from main temp file
         */
        if (fseek(tempfile, p_msg->msg_off, 0) != 0){
          pfmt(stderr, MM_ERROR, badseektmp, strerror(errno));
          fatal();
        }
        if (fread(msg_buf, 1, msg_len, tempfile) != msg_len){
          pfmt(stderr, MM_ERROR, badreadtmp, strerror(errno));
          fatal();
        }
	msg_buf[msg_len-1] = '\n';
        
        /*
         * Put it in the messages temp file and keep offset
         */
        for (i=set.last_msg+1;i <p_msg->msg_nr;i++,nmsg++) {
          if (fwrite(DFLT_MSG, 1, strlen(DFLT_MSG), f_msg) != strlen(DFLT_MSG)){
            pfmt(stderr, MM_ERROR, badwritetmp, strerror(errno));
            fatal();
          }
          if (fwrite("\n", 1, 1, f_msg) != 1 ) {
            pfmt(stderr, MM_ERROR, badwritetmp, strerror(errno));
            fatal();
          }
	}
        set.last_msg  = p_msg->msg_nr;
        if (fwrite(msg_buf, 1, msg_len, f_msg) != msg_len){
          pfmt(stderr, MM_ERROR, badwritetmp, strerror(errno));
          fatal();
        }
        nmsg++;
        p_msg = p_msg->msg_next;
      }
    } else {
      set.first_msg = 0;
      set.last_msg = 0;
    }
    
    /*
     * Put set hdr into set temp file
     */
    if (fwrite((char *)&set, sizeof (struct _m_cat_set) , 1, f_shdr) != 1){
      pfmt(stderr, MM_ERROR, badwritetmp, strerror(errno));
      fatal();
    }
    p_sets = p_sets->set_next;
  }
  
  /*
   * seek to begining of  file
   * and then write total sets
   */
  if (fseek(f_shdr, 0 , 0) != 0){
    pfmt(stderr, MM_ERROR, ":9:seek() failed in %s: %s\n", catalog,
      strerror(errno));
    fatal();
  }
  if (fwrite((char *)&no_sets, sizeof (no_sets) , 1, f_shdr) != 1){
    pfmt(stderr, MM_ERROR, badwrite, catalog, strerror(errno));
    fatal();
  }

  fclose(tempfile);
  fclose(f_shdr);
  fclose(f_msg);

  sprintf(message_file,"%s%s",catalog,M_EXTENSION);
  if (fork() == 0) {
	
    execlp(BIN_MKMSGS,BIN_MKMSGS,"-o",tmp_file,message_file,(char*)NULL);
    pfmt(stderr, MM_ERROR, ":10:Cannot execute %s: %s\n", BIN_MKMSGS,
      strerror(errno));
    fatal();

  }
  wait(&status);
  unlink(tmp_file);
  if (status) {
    pfmt(stderr, MM_ERROR, ":11:%s failed\n", BIN_MKMSGS);
    fatal();
  }
}

static 
char *
bldtmp()
{

  char *getenv();
  static char buf[MAXNAMLEN];
  char *tmp;
  char *mktemp();

  tmp = getenv("TMPDIR");
  if (tmp==(char *)NULL || *tmp == '\0')
    tmp="/tmp";

  sprintf(buf,"%s/%s",tmp,mktemp("gencat.XXXXXX"));
  return buf;
}
