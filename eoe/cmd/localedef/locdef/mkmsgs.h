#ifndef MKMSGS_H
#define MKMSGS_H

#include <sys/cdefs.h>
#include <pfmt.h>
#include <unistd.h>
#include <string.h>

#define GETTXT( id ) gettxt ( __CONCAT( id, _MSG_NUM_STR ) , \
                              __CONCAT( id, _DEF_MSG) )

#define GETTXTCAT( id ) gettxt( __CONCAT( id, _CAT_NUM_STR ) , \
                                __CONCAT( id, _DEF_MSG ) )

#define CATGETS( catd, id )      catgets( catd, __CONCAT( id, _MSG_SET_NUM ), \
                                                __CONCAT( id, _MSG_NUM ), \
                                                __CONCAT( id, _DEF_MSG) )


/* HCL Addition : Following Macros are  defined to cater for the pfmt function and
   to implement the new cataloging mechanism for already cataloged messages */
#define CAT_MSG_NUM( id ) __CONCAT( id, _CAT_NUM_STR )
#define DEF_MSG( id ) __CONCAT( id, _DEF_MSG )
#define MSG_NUM( id ) __CONCAT( id, _MSG_NUM_STR )
#define MSGNUM_DEFMSG( id ) __CONCAT( id, _MSG_NUM_STR_DEF_MSG)
#define CATNUM_DEFMSG( id ) __CONCAT( id, _CAT_NUM_STR_DEF_MSG)
#define PFMTTXTCAT( id )  __CONCAT( id, _CAT_NUM_STR_DEF_MSG)
#define PFMTTXT( id ) __CONCAT(id , _MSG_NUM_STR_DEF_MSG)

/* HCL Addition Ends here */
#endif
