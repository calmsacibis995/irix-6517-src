#ifndef __PARSE_H_
#define __PARSE_H_

#define NUMCHARS      256	/* number of bytes */
#define NAME_LENGTH   256
                                /* number of bytes needed to represent     */
#define MAXINT_SIZE   13	/* an ascii version of the largest integer */
#define MAXFLOAT_SIZE 1024	/* an ascii version of the largest float   */

/* 
 * Structure used to pass data between scanner and parser. yylval variable is
 * a pointer to this structure.
 */
typedef struct {
  union {
    __uint64_t val;		
    void *ptr;			/* string or unknown data  */
    double f;			
  } val;
  int type;			/* type of the data in the union structure */
} parse_s;

#define VAL   val.val
#define PTR   val.ptr
#define FLT   val.f
/* 
 * Actual data passed between scanner and parser.
 */
typedef parse_s *parse_s_ptr;

struct action_string {
  char *user;                 /* user to execute action as. */
  char **envp;                /* environment variables to set before */
  /* executing action */
  int timeout;                /* timeout value for action. */
  int retry;                  /* number of times to retry action. */
  char *executable;	      /* action to take when event occured. */
  char *args;		      /* arguments to action */
};

#endif /* __PARSE_H_ */
