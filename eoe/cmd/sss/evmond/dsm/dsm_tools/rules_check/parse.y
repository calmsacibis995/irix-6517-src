/*
 * YACC based grammar. Grammar is pretty simple All it is intended to do for
 * now is to parse the action string in a DSM rule.
 *
 * The action string consists of the user id, environment variable, 
 * retry count, timeout in seconds and the actual action itself.
 */
%{				
  /*
   * Top of generated C program.
   */
#include <stdio.h>
#include <pwd.h>
#include <assert.h>
#include <stdarg.h>
#include "parse.h"
#include <sys/types.h>
#include <sys/stat.h>

#define YYSTYPE parse_s_ptr
#define YYDEBUG 1
extern FILE *stdofp,*stdifp,*stderrfp;
extern YYSTYPE parse_error;
extern struct action_string Saction;

static int num_env=0;

/* 
 * Functions other than the automatically generated ones.
 */
YYSTYPE setprefix(YYSTYPE Puser,YYSTYPE Pretry,YYSTYPE Ptimeout);
YYSTYPE setenviron(int nptrs,...);
YYSTYPE concat(YYSTYPE ,YYSTYPE );
int setuser(char *name);
void yyerror(char *s);
%}

/* 
 * tokens or terminal symbols generated by SCANNER.
 */
%token INT FLOAT
%token STRING SYMBOL
%token USER_TOKEN
%token ENV_TOKEN
%token RETRY_TOKEN
%token TIME_TOKEN
%token ERROR_TOKEN		/* not generated currently. 10/28/98 */

%%				/* Grammar starts here */

/* 
 * start rule.
 */
input      :    RULE
             {
	       if($$==parse_error)
		 {
		   fprintf(stderrfp,"Error while parsing.\n");
		   exit(-1);
		 }
	       /* 
		* Enter rule into ssdb here. 
		*/
#if PARSE_DEBUG
	       fprintf(stderrfp,"User=%s, Retry=%d, Time=%d\n",
		       Saction.user,Saction.retry,Saction.timeout);
	       if(Saction.envp)
		 {
		   int i=0;
		   for(i=0;Saction.envp[i];i++)
		     fprintf(stderrfp,"Env[%d]=%s\n",i,Saction.envp[i]);
		 }
	       fprintf(stderrfp,"Action=%s %s\n",
		       Saction.executable,
		       Saction.args);
#endif
	       /* 
		* Set user id to user.
		*/
	       if(Saction.user)
		 {
		   if(setuser(Saction.user))
		     {
		       fprintf(stderrfp,"Unable to set user-id to %s\n",
			       Saction.user);
		       exit(-1);
		     }
		 }
	       /* 
		* Check if file is executable as user.
		*/
	       if(checkexecutable(Saction.executable,Saction.user))
		 {
		   exit(-1);
		 }

	       /* 
		* Free up the allocated memory.
		*/
	       if(Saction.user)
		 {
		   free(Saction.user); /* user */
		 }
	       if(Saction.envp)      /* environment variables */
		 {
		   int i=0;
		   for(i=0;Saction.envp[i];i++)
		     free(Saction.envp[i]);
		   free(Saction.envp);
		 }
	       if(Saction.executable)/* action string */
		 free(Saction.executable);
	       if(Saction.args)      /* arguments */
		 free(Saction.args);

	       exit(0);		     /* terminate */
	     }
;

RULE       :     PREFIX SYMBOL ARGS ';'
             {
	       if($1 == parse_error || $2 == parse_error || $3 == parse_error)
		 {
		   $$=parse_error;
		 }
	       else
		 {
		   Saction.executable=$2->PTR;
		   free($2);
		   if($3)
		     {
		       Saction.args=$3->PTR;
		       free($3);
		     }
		   else
		     Saction.args=NULL;
		   /* 
		    * put everything into the Saction structure.
		    */
		   $$=NULL;
		 }
	     }
           |     PREFIX STRING ARGS ';'
             {
	       if($1 == parse_error || $2 == parse_error || $3 == parse_error)
		 {
		   $$=parse_error;
		 }
	       else
		 {
		   Saction.executable=$2->PTR;
		   free($2);
		   if($3)
		     {
		       Saction.args=$3->PTR;
		       free($3);
		     }
		   else
		     Saction.args=NULL;
		   /* 
		    * put everything into the Saction structure.
		    */
		   $$=NULL;
		 }
	     }
;

PREFIX     :     ENVIRONS USER ENVIRONS RETRY ENVIRONS TIME ENVIRONS
             {
	       if($1 == parse_error || $3 == parse_error || 
		  $5 == parse_error || $7 == parse_error )
		 $$=parse_error;
	       else
		 {
		   $$=setprefix($2,$4,$6);
		 }
	     }
           |     ENVIRONS RETRY ENVIRONS USER ENVIRONS TIME ENVIRONS
             {
	       if($1 == parse_error || $3 == parse_error || 
		  $5 == parse_error || $7 == parse_error )
		 $$=parse_error;
	       else
		 {
		   $$=setprefix($4,$2,$6);
		 }
	     }
           |     ENVIRONS RETRY ENVIRONS TIME ENVIRONS USER ENVIRONS
             {
	       if($1 == parse_error || $3 == parse_error || 
		  $5 == parse_error || $7 == parse_error )
		 $$=parse_error;
	       else
		 {
		   $$=setprefix($6,$2,$4);
		 }
	     }
           |     ENVIRONS TIME ENVIRONS RETRY ENVIRONS USER ENVIRONS
             {
	       if($1 == parse_error || $3 == parse_error || 
		  $5 == parse_error || $7 == parse_error )
		 $$=parse_error;
	       else
		 {
		   $$=setprefix($6,$4,$2);
		 }
	     }
           |     ENVIRONS TIME ENVIRONS USER ENVIRONS RETRY ENVIRONS
             {
	       if($1 == parse_error || $3 == parse_error || 
		  $5 == parse_error || $7 == parse_error )
		 $$=parse_error;
	       else
		 {
		   $$=setprefix($4,$6,$2);
		 }
	     }
           |     ENVIRONS USER ENVIRONS TIME ENVIRONS RETRY ENVIRONS
             {
	       if($1 == parse_error || $3 == parse_error || 
		  $5 == parse_error || $7 == parse_error )
		 $$=parse_error;
	       else
		 {
		   $$=setprefix($2,$6,$4);
		 }
	     }
;

USER       :     USER_TOKEN '(' SYMBOL ')'
             { 
	       $$ = $3;
	     }
;

ENVIRONS   :
             { 
	       $$ = NULL; 
	     }
           |      ENVIRONS ENVIRON
             { $$ = NULL; }            
;


ENVIRON    :      ENV_TOKEN '(' SYMBOL ')'
             {
	       $$=setenviron(1,$3->PTR);
	       free($3->PTR);
	       free($3);
	     }
           |     ENV_TOKEN '(' SYMBOL '=' SYMBOL ')'
             {
	       $$=setenviron(3,$3->PTR,"=",$5->PTR);
	       free($3->PTR);
	       free($3);
	       free($5->PTR);
	       free($5);
	     }
           |     ENV_TOKEN '(' SYMBOL '=' STRING ')'
             {
	       $$=setenviron(3,$3->PTR,"=",$5->PTR);
	       free($3->PTR);
	       free($3);
	       free($5->PTR);
	       free($5);
	     }
;

RETRY      :     RETRY_TOKEN '(' INT ')'
             { 
	       $$ = $3;   
	     }
           |     RETRY_TOKEN error
             { 
	       $$ = parse_error; 
	     }
;

TIME       :     TIME_TOKEN '(' INT ')'
             { $$ = $3;   }
           |     TIME_TOKEN error
             { $$ = parse_error; }
;

ARGS       :
             { $$=NULL;   }
           |     ARGS INT	
             {
	       int i=$2->VAL;

	       $2->PTR=calloc(MAXINT_SIZE,1);
	       snprintf($2->PTR,MAXINT_SIZE,"%d",i);
	       $$=concat($1,$2);
	     }
           |     ARGS FLOAT	/* should think about FLOAT here too. */
             {
	       double f=$2->FLT;

	       $2->PTR=calloc(MAXFLOAT_SIZE,1);
	       snprintf($2->PTR,MAXFLOAT_SIZE,"%f",f);
	       $$=concat($1,$2);
	     }
           |     ARGS SYMBOL
             {
	       if(isbadformat($2->PTR))
		 {
		   fprintf(stderr,"Error in format type:%s\n",$2->PTR);
		   exit(-1);
		 }
	       $$=concat($1,$2);
	     }
           |     ARGS STRING
             {
	       if(isbadformat($2->PTR))
		 {
		   fprintf(stderr,"Error in format type:%s\n",$2->PTR);
		   exit(-1);
		 }
	       $$=concat($1,$2);
	     }
           |     error
             { 
	       $$ = parse_error; 
#if PARSE_DEBUG
	       fprintf(stderr,"Error while parsing argument(s).\n");
#endif
	     }
;

%%				/* code starts here. */
  
/*
 * Function to handle parse errors.
 */
void yyerror(char *s)
{
  extern int characters_readin;
  int i=0;

  /* 
   * Free our allocated memory.
   */
  if(Saction.user)
    free(Saction.user);
  if(Saction.envp)
    {
      int i=0;
      for(i=0;Saction.envp[i];i++)
	free(Saction.envp[i]);
      free(Saction.envp);
    }
  if(Saction.executable)
    free(Saction.executable);
  if(Saction.args)
    free(Saction.args);
  
  for(i=0;i<characters_readin;i++)
    fprintf(stderrfp," ");
  fprintf(stderrfp,"^^\n");

  fprintf(stderrfp,"%s.\n",s);

  exit(-1);
}

/* 
 * Concatenate two arguments to the action.
 * Function takes care of doing any free's.
 */
YYSTYPE concat(YYSTYPE a0,YYSTYPE a1)
{
  if(!a0)
    return a1;

  if(a1)
    {
      assert(a1->PTR);
      a0->PTR=realloc(a0->PTR,(strlen(a0->PTR)+strlen(a1->PTR)+2));
      strcat(a0->PTR," ");
      strcat(a0->PTR,a1->PTR);
      free(a1->PTR);
      free(a1);
    }
  
  return a0;
}

/* 
 * Set the environment variables.
 * Caller's responsibility to do the free of memory.
 */
YYSTYPE setenviron(int nptrs,...)
{
  va_list ap;
  int i=0;
  char buf[NUMCHARS+1];

  /* 
   * Allocate the environment variable array.
   * if this the first time around do a calloc otherwise just realloc the
   * memory.
   */
  if(!Saction.envp)
    {
      Saction.envp=(char **)calloc(sizeof(char *),num_env+2);
    }
  else
    {
      Saction.envp=(char **)realloc(Saction.envp,(num_env+2)*sizeof(char *));
    }

  if(!Saction.envp)		/* memory allocation failed. */
    return parse_error;

  /* 
   * Iterate over all the arguments.
   */
  va_start(ap, nptrs);
  while(i<nptrs)
    {
    if(i++==0)
      snprintf(buf,NUMCHARS,"%s",va_arg(ap, char *));
    else
      snprintf(buf,NUMCHARS,"%s%s",buf,va_arg(ap, char *));
    }
  va_end(ap);

  /* 
   * Duplicate the string.
   */
  Saction.envp[num_env]=strdup(buf);
  
  /* 
   * strdup failed.
   */
  if(!Saction.envp[num_env++])
    return parse_error;

  /* 
   * Null terminate the environment variable array.
   */
  Saction.envp[num_env]=0;

  return NULL;
}

/* 
 * set the prefix part of the action in the action_string structure.
 * The prefix part of the string essentially consists of the
 *  o user name
 *  o retry value
 *  o timeout
 * Function takes care of doing any free's.
 */
YYSTYPE setprefix(YYSTYPE Puser,YYSTYPE Pretry,YYSTYPE Ptimeout)
{
  if(!Puser || !Pretry || !Ptimeout)
    return parse_error;
  
  if(Puser == parse_error || Pretry == parse_error || Ptimeout == parse_error)
    return parse_error;

  Saction.user    = Puser->PTR; 	 /* user    */
  Saction.timeout = Ptimeout->VAL;       /* timeout */
  Saction.retry   = Pretry->VAL;	 /* retry   */

  /* 
   *  free the YYSTYPE structures.
   */
  free(Puser),free(Pretry),free(Ptimeout);
  return NULL;
}

/* 
 * Check if the user is a valid user.
 */
int setuser(char *name)
{
  struct passwd *pw=NULL;

  if(!name)
    return 0;

  if(!(pw=getpwnam(name)))
    {
      fprintf(stderrfp,"WARN: No such user:%s\n",name);
      return -1;
    }
  
  if(setgid(pw->pw_gid))
    {
      fprintf(stderrfp,"WARN: Unable to set group-id to %d\n",pw->pw_gid);
      return -1;
    }

  if(setuid(pw->pw_uid))
    {
      fprintf(stderrfp,"WARN: Unable to set user-id to %d\n",pw->pw_uid);
      return -1;
    }

  return 0;
}

/*
 * check the executable file if permissions etc. are ok.
 */
int checkexecutable(char *file,char *user)
{
  struct passwd *pw=NULL;
  struct stat Sstat;

  if(!file || !user)
    return 0;

  if(!(pw=getpwnam(user)))
    {
      return 0;
    }

  if(stat(file,&Sstat))
    {
      fprintf(stderrfp,"Stat failed for %s\n",file);
      return -1;
    }

  if(!S_ISREG(Sstat.st_mode))
    {
      fprintf(stderr,"Warn: File is not a regular file.\n");
      return -1;
    }
  if(pw->pw_uid == Sstat.st_uid)
    {
      /* 
       * If user id match then the file should be executable by owner.
       */
      if(!(Sstat.st_mode & S_IXUSR))
	{
	  fprintf(stderrfp,"Warn: File %s is not executable by %s\n",
		  file,user);
	}
      if(Sstat.st_mode & S_ISUID)
	fprintf(stderrfp,"Warn: File's suid bit for user is on.\n");
    }
  else if(pw->pw_gid == Sstat.st_gid)
    {
      /* 
       * If group id match then the file should be executable by group.
       */
      if(!(Sstat.st_mode & S_IXGRP))
	{
	  fprintf(stderrfp,"Warn: File %s is not executable by %s\n",
		  file,user);
	  return -1;
	}
      if(Sstat.st_mode & S_ISGID)
	fprintf(stderrfp,"Warn: File's suid bit for group is on.\n");
    }
  else 
    {
      /* 
       * Neither owner nor group id match, so file should be executable by
       * other.
       */
      if(!(Sstat.st_mode & S_IXOTH))
	{
	  fprintf(stderrfp,"Warn: File %s is not executable by %s\n",
		  file,user);
	}
    }
  return 0;
}

/*
 * This should match what is accepted in dsm_children.c.
 */
static int isspecialchar(char c)
{
  return (c == 'C' || c == 'T' || c == 'P' || c == 'O' || c == 'D' ||
	  c == 'd' || c == 'H' || c == 's' || c == 'm' || c == 'M' ||
	  c == 'h' || c == 'y' || c == 't' || c == '%' );
}

/* 
 * Checks if the characters with % symbols in them are bad.
 */
static int isbadformat(char *buffer)
{
  if(!buffer || !*buffer)
    return 1;
  
  if(*buffer == '%' && !isspecialchar(*(buffer+1)))
    return 1;

  for(;*buffer;buffer++)
    {
      if(*buffer == '%' && !isspecialchar(*(buffer+1)))
	return 1;
    }

  return 0;
}
