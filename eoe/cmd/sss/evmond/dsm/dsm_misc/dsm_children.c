#include "common.h"
#include "dsm_events.h"
#include "dsm_events_private.h"
#include "dsm_rules.h"
#include "dsm.h"
#include "dsm_errors.h"
#include "limits.h"
#include <sys/stat.h>

#if EVENTS_DEBUG
unsigned int num_actions_taken=0;
#endif

extern int global_action_flag;
/*
 * Some more we need to set are USER, TMPDIR and LOGNAME. What about DISPLAY ?.
 */
static char *old_environ_variables[] = 
{
  "HOME",          NULL,               "LANG",            NULL,
  "LC_COLLATE",    NULL,               "LC_CTYPE",        NULL,
  "LC_MESSAGES",   NULL,               "LC_MONETARY",     NULL,
  "LC_NUMERIC",    NULL,               "LC_TIME",         NULL,
  "MSGVERB",       NULL,               "NOMSGLABEL",      NULL, 
  "NOMSGSEVERITY", NULL,               "SEV_LEVEL",       NULL,
  "NLSPATH",       NULL,               "LD_LIBRARY_PATH", NULL,
  "_RLD_ARGS",     NULL,               "PATH",            NULL,
  "SHELL",         NULL,               "TERM",            NULL,
  "TZ",            NULL,               "MAIL",            NULL,
  0,
};

/*
 * Initialize the things necessary to run actions.
 *    o currently all this does is to setup the environment variables.
 */
void dsm_init_children()
{
  int i;

  for(i=0;old_environ_variables[i];i+=2)
    old_environ_variables[i+1]=getenv(old_environ_variables[i]);
}

/* 
 * harvest terminated children without waiting on them.
 */
void dsm_harvest_children()
{
  int status=0;

  while (waitpid(-1, &status, WNOHANG) > (pid_t)0)
    ;
}

/*
 * check the executable file if permissions etc. are ok.
 */
__uint64_t checkexecutable(char *file,char *user)
{
  struct passwd *pw=NULL,pw1;
  struct stat Sstat;
  char buf[BUFSIZ];

  if(!file || !user)
    return DSM_ERROR(DSM_MAJ_RULE_ERR,DSM_MIN_INVALID_ARG);

  /* 
   * Read the password file.
   */
  if(getpwnam_r(user,&pw1,buf,BUFSIZ,&pw) || !pw)
    {
      return DSM_ERROR(DSM_MAJ_RULE_ERR,DSM_MIN_RULE_USER);
    }

  /* 
   * Stat the file, if it is not there return error.
   */
  if(stat(file,&Sstat))
    {
      return DSM_ERROR(DSM_MAJ_RULE_ERR,DSM_MIN_RULE_FILE);
    }

  /* 
   * Is it a regular file ?.
   */
  if(!S_ISREG(Sstat.st_mode))
    {
      return DSM_ERROR(DSM_MAJ_RULE_ERR,DSM_MIN_RULE_FILE);
    }

  /* 
   * Check the file permissions and if the execute permission is there.
   */
  if(pw->pw_uid == Sstat.st_uid)
    {
      /* 
       * If user id match then the file should be executable by owner.
       */
      if(Sstat.st_mode & S_ISUID)
	return DSM_ERROR(DSM_MAJ_RULE_ERR,DSM_MIN_RULE_SUID);

      if(!(Sstat.st_mode & S_IXUSR))
	return DSM_ERROR(DSM_MAJ_RULE_ERR,DSM_MIN_RULE_USER);
    }
  else if(pw->pw_gid == Sstat.st_gid)
    {
      if(Sstat.st_mode & S_ISGID)
	return DSM_ERROR(DSM_MAJ_RULE_ERR,DSM_MIN_RULE_SUID);
      /* 
       * If group id match then the file should be executable by group.
       */
      if(!(Sstat.st_mode & S_IXGRP))
	return DSM_ERROR(DSM_MAJ_RULE_ERR,DSM_MIN_RULE_USER);
    }
  else 
    {
      /* 
       * Neither owner nor group id match, so file should be executable by
       * other.
       */
      if(!(Sstat.st_mode & S_IXOTH))
	return DSM_ERROR(DSM_MAJ_RULE_ERR,DSM_MIN_RULE_USER);
    }
  return 0;
}

/* 
 * Create the environment structure for the executing process.
 * set generic and any specific environment variables.
 */
static __uint64_t dsm_create_env(char **PPenv[],char *oldenv[],char *envp[])
{
  int i=0;
  int j=0;
  int len=0;
  char buf[1024];

  /*
   * How many environment variables are there in the generic & specific 
   * environment structure ?.
   */
  i=0,j=0;
  while(oldenv[j])
    j+=2,i++;

  /*
   * How many environment variables are there in the specific environment
   * structure ?.
   */
  j=0;
  if(envp)
    {
      while(envp[j++]);
    }

  /*
   * Create the final env variable array to pass to the executable.
   */
  *PPenv=(char **)sem_mem_alloc_temp((i+j+1)*sizeof(char *));
  if(!*PPenv)
    return DSM_ERROR(DSM_MAJ_ACTION_ERR,DSM_MIN_ALLOC_MEM);

  /*
   * Copy over the old environment variables to the final env variable array.
   */
  i=0;
  for(i=0,j=0;oldenv[j];i++,j+=2)
    {
      if(oldenv[j+1])
	len=snprintf(buf,1023,"%s=%s",oldenv[j],oldenv[j+1]);
      else
	len=snprintf(buf,1023,"%s=",oldenv[j]);
      buf[len]='\0';

      (*PPenv)[i]=(char *)sem_mem_alloc_temp((len+1)*sizeof(char));
      if(!(*PPenv)[i])
	return DSM_ERROR(DSM_MAJ_ACTION_ERR,DSM_MIN_ALLOC_MEM);
      strncpy((*PPenv)[i],buf,len+1);
    }

  /*
   * Copy over the newer environment variables to the final env variable array.
   */
  if(envp)
    {
      for(j=0;envp[j];j++,i++)
	(*PPenv)[i]=envp[j];
    }

  /*
   * Make Null the last environment variable entry. Others depend on this.
   */
  (*PPenv)[i]=0;

  return 0;
}

#define isquote(c) (c ==  '\'' || c == '\"')
/*
 * Essentially scans in a quoted string
 */
char *dsm_readin_quote(char *string,char c)
{
  /*
   * The first character of the string must be a quote character.
   */
  if(*(string++) != c)
    return NULL;

  while(*string && *string != c)
    {
      if(isquote(*string) && (*(string-1) != '\\'))
	{
	  string=dsm_readin_quote(string,*string);
	}
      else
	string++;
    }
  return string;
}


/*
 * Expand one argument in the argument string specified in the rule 
 * to its full form.
 * Also replace the special tokens (%?) with their expanded versions.
 */
char *dsm_parse_arg(char **Pstring,
                    struct rule *Prule,
                    struct event_private *Pev)
{
  char *Pchar=*Pstring,*Pparsed_arg;
  time_t t;
  struct tm *Ptm;
  char *Ptime;
  int len,unparsed_length;
  int i,j;
  char buf[100];

  /* 
   * Silly check.
   */
  if(!Pchar || !*Pchar || !Prule || !Pev)
    return NULL;
  
  /* 
   *  First get the unparsed length of the string.
   */
  while(Pchar && *Pchar && !isspace(*Pchar))
    {
      if(isquote(*Pchar))
	Pchar=dsm_readin_quote(Pchar,*Pchar);
      else
	Pchar++;
    }

  /* 
   * can happen if dsm_readin_quote encounters 
   * an error. 
   */
  if(!Pchar)
    {
      return NULL;
    }
    

  unparsed_length=len=Pchar-*Pstring;

  Pchar=*Pstring;
  for(i=0,j=0;i<len;i++)
    {				/* Now update the length for */
				/* sss defined format symbols -- %e %t etc. */
      /*
       * SSS defined symbols
       *   --- %C = event class
       *   --- %T = event type
       *   --- %P = priority value, eventmon related.
       *   --- %O = object id for the rule.
       *   --- %D = event data (this is the structure as received from the api,
       *                        fields in the strucure are demarcated.)
       *   --- %H = host name from which event originated
       *   --- %S = Event time stamp
       *   --- %F = forwarder hostname
       *   --- %I = sys id
       *   --- %t = time string
       *   --- %s = seconds since Jan 1 1970
       *   --- %m = current minute of the hour 0-59
       *   --- %M = current month of the year 0-11
       *   --- %h = current hour of the day 0-23
       *   --- %y = current year
       *   --- %d = day of the month
       */
      if(Pchar[i] == '%')
	{
	  switch(Pchar[i+1])
	    {
	    case('C'):          /* event class. */
	    case('T'):	        /* event type. */
	    case('P'):		/* priority field. */
	    case('O'):		/* object id for the rule. */
	      j+=13;		/* integer */
	      i++;
	      break;
	    case ('D'):		/* event data. */
	      if(Pev->DSMDATA.Pevent_data)
		j+=strlen(Pev->DSMDATA.Pevent_data);
	      i++;
	      break;
	    case('d'):		/* day of the month */
	      t = time(NULL);
	      Ptm=localtime(&t);
	      j+=2;
	      i++;
	      break;
	    case('H'):		/* hostname */
	      j+=strlen(Pev->DSMDATA.hostname);
	      i++;
	      break;
            case('F'):
	      if ( Prule->forward_hname ) {
		j+=strlen(Prule->forward_hname);
	      }
	      i++;
	      break;
	    case 'I':
		j+=26;
		i++;
		break;
            case('S'):
	    case('s'):		/* time in seconds since Jan 1970. */
	      t = time(NULL);
	      j+=13;		/* integer */
	      i++;
	      break;
	    case('m'):		/* current minute of the hour. */
	    case('M'):		/* current month of the year. */
	    case('h'):		/* current hour of the day. */
	    case('y'):		/* current year. */
	      t = time(NULL);
	      Ptm=localtime(&t);
	      j+=2;
	      i++;
	      break;
	    case('t'):		/* time string as returned by date. */
	      t = time(NULL);
	      Ptime = asctime(localtime(&t));
	      Ptime[strlen(Ptime)-1]=0;	/* asctime puts a '\n' in there. */
	      j+=strlen(Ptime);
	      i++;
	      break;
	    default:
	      break;
	    }
	}
    }
  
  len += j+1;

  /*
   * Allocate the large string which will contain all the arguments.
   */
  if(!(Pparsed_arg=sem_mem_alloc_temp(len*sizeof(char))))
    return NULL;
  Pparsed_arg[0]=0;


  for(i=0,j=0;i<unparsed_length;i++)
    {
      j=strlen(Pparsed_arg);
      if(j>=len)
	{
	  /* 
	   * Error: The parsed length is becoming greater than we expect it
	   * to be. So just return NULL.
	   */
	  sem_mem_free(Pparsed_arg);
	  return NULL;
	}
      if(Pchar[i] == '%')
	{
	  switch(Pchar[i+1])
	    {
	    case('C'):		/* event class */
	      j+=snprintf(buf,100,"%d",Pev->event_class);
	      strcat(Pparsed_arg,buf);
	      i++;
	      break;
	    case('T'):		/* event type */
	      j+=snprintf(buf,100,"%d",Pev->event_type);
	      strcat(Pparsed_arg,buf);
	      i++;
	      break;
	    case ('P'):		/* priority. */
	      j+=snprintf(buf,100,"%d",RUN(Pev)->priority);
	      strcat(Pparsed_arg,buf);
	      i++;
	      break;
	    case ('O'):		/* event id. */
	      j+=snprintf(buf,100,"%d",Pev->DSMDATA.event_id);
	      strcat(Pparsed_arg,buf);
	      i++;
	      break;
            case ('S'):
	      j+=snprintf(buf,100,"%d", RUN(Pev)->time_stamp);
	      strcat(Pparsed_arg,buf);
	      i++;
	      break;
	    case ('F'):		/* forward hostname */
	      if ( Prule->forward_hname) {
	        j+=strlen(Prule->forward_hname);
	        strcat(Pparsed_arg, Prule->forward_hname);
	      }
	      i++;
	      break;
	    case 'I':		/* sys id. */
		j+=snprintf(buf,100,"%llX",Pev->sys_id);
		strcat(Pparsed_arg,buf);
		i++;
		break;
	    case ('D'):		/* event data. */
	      if(Pev->DSMDATA.Pevent_data)
		{
		  j+=strlen(Pev->DSMDATA.Pevent_data);
		  strcat(Pparsed_arg,Pev->DSMDATA.Pevent_data);
		}
	      i++;
	      break;
	    case('d'):		/* day of the month */
	      j+=snprintf(buf,100,"%d",Ptm->tm_mday);
	      strcat(Pparsed_arg,buf);
	      i++;
	      break;
	    case('H'):		/* hostname */
	      j+=strlen(Pev->DSMDATA.hostname);
	      strcat(Pparsed_arg,Pev->DSMDATA.hostname);
	      i++;
	      break;
	    case('s'):		/* seconds since 1970 */
	      j+=snprintf(buf,100,"%d",t);
	      strcat(Pparsed_arg,buf);
	      i++;
	      break;
	    case('m'):		/* current minute of the hour. */
	      j+=snprintf(buf,100,"%d",Ptm->tm_min);
	      strcat(Pparsed_arg,buf);	      
	      i++;
	      break;
	    case('M'):		/* current month of the year. */
	      j+=snprintf(buf,100,"%d",Ptm->tm_mon);
	      strcat(Pparsed_arg,buf);	      
	      i++;
	      break;
	    case('h'):		/* current hour of the day. */
	      j+=snprintf(buf,100,"%d",Ptm->tm_hour);
	      strcat(Pparsed_arg,buf);	      
	      i++;
	      break;
	    case('y'):		/* current year */
	      j+=snprintf(buf,100,"%d",Ptm->tm_yday);
	      strcat(Pparsed_arg,buf);	      
	      i++;
	      break;
	    case('t'):		/* time string. */
	      j+=strlen(Ptime);
	      strcat(Pparsed_arg,Ptime);
	      i++;
	      break;
	    default:		
	      Pparsed_arg[j++]=Pchar[i];
	      if(Pchar[i+1] == '%') /* We got a %%. */
		i++;
	      break;
	    }
	}
      else
	{	
	  /* 
	   * the char in the original argument string, specified in the
	   * rule.
	   */	  
	  Pparsed_arg[j++]=Pchar[i];
	}
      Pparsed_arg[j]=0;
    }

  /*
   * Return the pointer to the next argument.
   */
  Pchar+=unparsed_length;
  while(*Pchar && isspace(*Pchar))
    Pchar++;
  *Pstring=Pchar;

  return Pparsed_arg;
}

/* 
 * break action string into separate components.
 *  o arguments.
 *  o also make a copy of the action to be taken in the event structure.
 */
__uint64_t dsm_action_string_into_args(char *action,
				       struct event_private *Pev,
                                       struct rule *Prule,
				       char *args[])
{
  int i=0;
  int len=0;
  char *Pchar;
  int numquote=0;

  Pchar=action;

  while(*Pchar && isspace(*Pchar)) /* Skip over spaces to get to first arg. */
    Pchar++;

  /*
   * Return the expanded form of each argument.
   */
  i=0;
  while((i < (MAX_ARGS-1)) && (args[i++]=dsm_parse_arg(&Pchar,Prule,Pev)));
  if(i==MAX_ARGS-1)
    args[i]=NULL;

  /* 
   * Find the total length of all the arguments.
   */
  i=0;
  len=0;
  while(args[i])
    {
      len+=strlen(args[i++])+3;
    }
  
  /* 
   * Allocate and make a copy of the action taken string.
   * If the previous action already exists free it.
   */
  if(RUN(Pev)->action_taken)
    {
      sem_mem_free(RUN(Pev)->action_taken);
      RUN(Pev)->action_taken=NULL;
      RUN(Pev)->action_time=0;
    }
  RUN(Pev)->action_taken=(char *)sem_mem_alloc_temp(len+1);
  if(!RUN(Pev)->action_taken)
    return DSM_ERROR(DSM_MAJ_ACTION_ERR,DSM_MIN_ALLOC_MEM);

#if !KLIB_LIBRARY
  memset(RUN(Pev)->action_taken,0,len+1);
#endif

  i=0;
  while(args[i])
    {
      /* 
       * copy each argument.
       */
      strcat(RUN(Pev)->action_taken,args[i++]);
      strcat(RUN(Pev)->action_taken," ");
    }

  /*
   * the time the action is going to be taken.
   */
  RUN(Pev)->action_time=(unsigned long)time(NULL);

  return 0;
}


/*
 * Start the wrapper, which will initiate the action to execute.
 * Make sure the arguments are given in the correct order to the wrapper.
 */
__uint64_t dsm_start_wrapper(char *wrapper,struct rule *Prule,
			     struct event_private *Pev)
{
  char **args=NULL;
  __uint64_t err=DSM_ERROR(DSM_MAJ_ACTION_ERR,DSM_MIN_ALLOC_MEM);
  int i=0;
  char *buf=NULL;
  char **child_environ=NULL;
  struct stat statbuf;
  int timeout, retry;
  char *action, *user, **envp;
  struct passwd *pw;
  
  if(!Prule || !Prule->Paction)
    return DSM_ERROR(DSM_MAJ_RULE_ERR,DSM_MIN_INVALID_ARG);
  
  timeout= Prule->Paction->timeout;
  retry  = Prule->Paction->retry;
  action = Prule->Paction->action;
  user   = Prule->Paction->user;
  envp   = Prule->Paction->envp;

  if(!wrapper || !action || !user)
    return DSM_ERROR(DSM_MAJ_RULE_ERR,DSM_MIN_INVALID_ARG);


  if(!(pw=getpwuid(geteuid())))
    return DSM_ERROR(DSM_MAJ_RULE_ERR,DSM_MIN_RULE_USER);

  if(err=checkexecutable(wrapper,pw->pw_name))
    goto local_error;

  /* 
   * check if the action can be executed.
   */
  if(!global_action_flag)
    return 0;

  /* 
   * temporary buffer.
   */
  buf=(char *)sem_mem_alloc_temp(sizeof(char)*100);  

  /* 
   * array of all the arguments.
   */
  args=(char **)sem_mem_alloc_temp(sizeof(char *)*MAX_ARGS);

  if(!args || !buf)
    goto local_error;

  /* 
   * One by one copy all the arguments into the environment variables array.
   *
   * program name.
   */  
  args[0]=wrapper;

  /* 
   * timeout value.
   */  
  args[1]=(char *)sem_mem_alloc_temp((1+snprintf(buf,100,"%d",timeout))*
				 sizeof(char));
  if(!args[1])
    goto local_error;
  strcpy(args[1],buf);

  /*
   * retry value.
   */
  args[2]=(char *)sem_mem_alloc_temp((1+snprintf(buf,100,"%d",
					     retry))*sizeof(char));
  if(!args[2])
    goto local_error;
  strcpy(args[2],buf);

  /* 
   * user id.
   */
  args[3]=(char *)sem_mem_alloc_temp((1+snprintf(buf,100,"%s",user))*sizeof(char));
  if(!args[3])
    goto local_error;
  strcpy(args[3],buf);

  /* 
   * free the temporary buffer.
   */
  sem_mem_free(buf);

  /* 
   * split the action string into its component arguments.
   */
  err=dsm_action_string_into_args(action,Pev,Prule,&args[4]);
  if(err)
    goto local_error;

  err=checkexecutable(args[4],args[3]);
  if(err)
    goto local_error;
    
  /* 
   * Create the environment variables array in child_environ.
   */
  err=dsm_create_env(&child_environ,old_environ_variables,envp);
  if(err)
    goto local_error;

  /*
   * Record that the action was taken in the ssdb.
   * Do the write to the ssdb before the action is actually taken, since there
   * is no saying how long the action can take.
   */
  dsm_create_action_taken_record_ssdb(Prule,Pev);

#if EVENTS_DEBUG
  num_actions_taken++;
#endif

  /* 
   * Start the child.
   */
  if(fork() == 0)
    {
      /* child. */
      /*
       * Not necessary to call dsm_clean_everything.. as only one thread
       * exists on a fork. Code here relies on this behavior and does not
       * attempt to do a join of other threads.
       *
       * Also the execve needs to be done right away.
       */
      
      if(execve(wrapper,args,child_environ)<0)
	{			
	  /* 
	   * execve failed, Should the error be written to syslog ?.
	   * Not doing this now because of the possibility of a deadlock.
	   */
	  exit(-1);
	}
      exit(-1);
    }

local_error:
  /* 
   * Free the memory allocated for the child environment.
   */
  if(child_environ)
    {
      for(i=0;i<(sizeof(old_environ_variables)/(sizeof(char *)*2));i++)
	if(child_environ[i])
	  sem_mem_free(child_environ[i]);
      sem_mem_free(child_environ);
    }

  /* 
   * Free the memory allocated for the arguments.
   */
  if(args)
    {
      i=1;
      while(args[i])
	sem_mem_free(args[i++]);
      sem_mem_free(args);
    }

  return err;
}
