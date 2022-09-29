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

#ident	"@(#)gencat:cat_misc.c	1.1.2.1"
#include <stdio.h>
#include <nl_types.h>
#include <pfmt.h>
#include <string.h>
#include <errno.h>

extern struct cat_set *sets;
extern int curset;
extern FILE *tempfile;
extern struct cat_set * get_set_ptr();
extern const char nomem[], badwritetmp[], badsetnr[], badmsgnr[];

void add_set(int);
void add_msg(int, int, int, char *);
void del_all_msgs(struct cat_msg *);


/*
 * Add a set in the set list
 */
void
add_set(set_nr)
  int set_nr;
{
  struct cat_set *ptr, *ptr2, *new;
  
  if (set_nr > NL_SETMAX || set_nr <= 0){
    pfmt(stderr, MM_WARNING, badsetnr, set_nr);
    return;
  }

  ptr2 = ptr = sets;
  
  /*
   * Scan list to find set or bigger
   */
  while (ptr != 0){
    if (ptr->set_nr == set_nr){
      /*
       * Found set in the list
       */
      curset = set_nr;
      return;
    }
    if (ptr->set_nr > set_nr)
      /*
       * Found a bigger set number
       */
      break;
    ptr2 = ptr;
    ptr = ptr->set_next;
  }
  
  /*
   * Insert new set in list, before ptr;
   */
  if ((new = (struct cat_set *)malloc(sizeof(struct cat_set))) == 0){
    pfmt(stderr, MM_ERROR, nomem, strerror(errno));
    fatal();
  }
  new->set_nr = set_nr;
  new->set_msg_nr = 0;
  new->set_next = ptr;
  new->set_msg = 0;
  
  if (ptr2 == ptr)
    sets = new;
  else
    ptr2->set_next = new;
  curset = set_nr;
} 

/*
 * Delete a set
 */
void
del_set(set_nr)
  int set_nr;
{
  struct cat_set *ptr, *ptr2, *new;
  int found;
  
  if (set_nr > NL_SETMAX || set_nr <= 0){
    pfmt(stderr, MM_WARNING, badsetnr, set_nr);
    return;
  }

  ptr2 = ptr = sets;
  found = 0;
  
  /*
   * Scan list to find set or bigger
   */
  while (ptr != 0){
    if (ptr->set_nr == set_nr){
      /*
       * Found set in the list
       */
      found = 1;
      break;
    }
    if (ptr->set_nr > set_nr)
      /*
       * Found a bigger set number
       */
      break;
    ptr2 = ptr;
    ptr = ptr->set_next;
  }
  if (!found){
    pfmt(stderr, MM_WARNING, ":6:del_set : Set not found %d -- Ignored\n", 
    	set_nr);
    return;
  }
  
  /*
   * Delete message queue
   */
  del_all_msgs(ptr->set_msg);
  
  /*
   * Delete from queue
   */
  if (ptr2 == ptr)
    sets = ptr->set_next;
  else
    ptr2->set_next = ptr->set_next;
  free((char *)ptr);

  /*
   * if is default set
   * reinit it
   */
  if (set_nr == 1)
	add_set(1);

  if (set_nr == curset)
    curset = NL_SETD;
}

/*
 * Delete a message
 */
void
del_msg(set_nr, msg_nr)
  int set_nr;
  int msg_nr;
{
  struct cat_msg *ptr, *ptr2, *new;
  struct cat_set *set_ptr;
  int found;
    
  /*
   * Get a pointer to the set
   */
  if ((set_ptr = get_set_ptr(set_nr)) == 0){
    pfmt(stderr, MM_WARNING, badsetnr, set_nr);
    return;
  }

  if (msg_nr > NL_MSGMAX || msg_nr <= 0){
    pfmt(stderr, MM_WARNING, badmsgnr, msg_nr);
    return;
  }

  ptr2 = ptr = set_ptr->set_msg;
  found = 0;
  
  /*
   * Scan list to find msg or bigger
   */
  while (ptr != 0){
    if (ptr->msg_nr == msg_nr){
      /*
       * Found set in the list
       */
      found = 1;
      break;
    }
    if (ptr->msg_nr > msg_nr)
      /*
       * Found a bigger msg number
       */
      break;
    ptr2 = ptr;
    ptr = ptr->msg_next;
  }
  if (!found){
    pfmt(stderr, MM_WARNING, ":7:message not found %d -- Ignored\n", 
    	msg_nr);
    return;
  }
  
  /*
   * Delete from queue
   */
  if (ptr2 == ptr)
    set_ptr->set_msg = ptr->msg_next;
  else
    ptr2->msg_next = ptr->msg_next;
  free((char *)ptr);
}

/*
 * Add a message in a set.
 */
void
add_msg(set_nr, msg_nr, msg_len, msg_buf)
  int set_nr, msg_nr, msg_len;
  char *msg_buf;
{
  struct cat_set *set_ptr;
  struct cat_msg *ptr, *ptr2, *new;
  int replace;
  extern int nl_textmax;
  
  /*
   * Get a pointer to the set
   */
  if ((set_ptr = get_set_ptr(set_nr)) == 0){
    pfmt(stderr, MM_WARNING, badsetnr, set_nr);
    return;
  }

  if (msg_nr > NL_MSGMAX || msg_nr <= 0){
    pfmt(stderr, MM_WARNING, badmsgnr, msg_nr);
    return;
  }
  
  if (msg_len > nl_textmax + 1 || msg_len < 0){
    pfmt(stderr, MM_WARNING, ":8:Invalid message length %d -- Ignored\n", 
    	msg_len);
    return;
  }
  
  /*
   * Scan message queue
   */
  ptr = ptr2 = set_ptr->set_msg;
  replace = 0;
  
  while (ptr != 0){
    if (ptr->msg_nr == msg_nr){
      new = ptr;
      replace = 1;
      break;
    }
    if (ptr->msg_nr > msg_nr)
      break;
    ptr2 = ptr;
    ptr = ptr->msg_next;
  }
  
  /*
   * Not a replacement : alloc msg header
   */
  if (!replace){
    if ((new = (struct cat_msg *)malloc(sizeof(struct cat_msg))) == 0){
      pfmt(stderr, MM_ERROR, nomem, strerror(errno));
      fatal();
    }
    new->msg_nr = msg_nr;
    new->msg_next = ptr;

    if (ptr == ptr2)
      set_ptr->set_msg = new;
    else
      ptr2->msg_next = new;
  }
  /*
   * Put message in the temp file and keep offset
   */
  new->msg_off = ftell(tempfile);
  new->msg_len = msg_len;

  if (fwrite(msg_buf, 1, msg_len, tempfile) != msg_len){
    pfmt(stderr, MM_ERROR, badwritetmp, strerror(errno));
    fatal();
  }
}

/*
 * Delete all messages from a queue
 */
void
del_all_msgs(ptr)
  struct cat_msg *ptr;
{
  struct cat_msg *ptr2;
  
  while (ptr != 0){
    ptr2 = ptr->msg_next;
    free((char *)ptr);
    ptr = ptr2;
  }
}
/*
 * Return a pointer to a set
 */
struct cat_set *
get_set_ptr(set_nr)
  int set_nr;
{
  struct cat_set *ptr;
  
  ptr = sets;
  
  while (ptr != 0){
    if (ptr->set_nr == set_nr)
      return ptr;
    ptr = ptr->set_next;
  }
  return 0;
}
