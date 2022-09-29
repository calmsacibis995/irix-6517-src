/* Copyright Abandoned 1998 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

/****************************************************************************
** Add all options from files named "group".conf from the default_directories
** before the command line arguments.
** As long as the program uses the last argument for conflicting
** options one only have to add a call to "load_defaults" to enable
** use of default values.
** pre- and end 'blank space' are removed from options and values. The
** following escape sequences are recognized in values:  \b \t \n \r \\
** If one uses --no-defaults as the first argument, no options are read.
****************************************************************************/

#undef SAFEMALLOC

#include "mysys_priv.h"
#include "m_string.h"
#include "m_ctype.h"

/* Which directories are searched for options (and in which order) */

my_string default_directories[]= {
#ifdef __WIN32__
"C:/",
#else
"/etc/",
#endif
#ifdef DATADIR
DATADIR,
#endif
#ifndef __WIN32__
"~/",
#endif
NullS,
};

#define default_ext   ".cnf"		/* extension for config file */

static my_bool search_default_file(DYNAMIC_ARRAY *args, MEM_ROOT *alloc,
				   my_string dir, const char *config_file,
				   TYPELIB *group);


void load_defaults(const char *conf_file, my_string *groups,
		   int *argc, char ***argv)
{
  DYNAMIC_ARRAY args;
  my_string *dirs;
  TYPELIB group;
  my_bool print_defaults=0;
  MEM_ROOT alloc;
  char *ptr,**res;
  DBUG_ENTER("load_defaults");

  init_alloc_root(&alloc,128);
  if (*argc >= 2 && !strcmp(argv[0][1],"--no-defaults"))
  {
    /* remove the --no-defaults argument and return only the other arguments */
    uint i;
    if (!(ptr=(char*) alloc_root(&alloc,sizeof(alloc)+
				 (*argc + 1)*sizeof(char*))))
      goto err;
    res= (char**) (ptr+sizeof(alloc));
    res[0]= **argv;				/* Copy program name */
    for (i=2 ; i < (uint) *argc ; i++)
      res[i-1]=argv[0][i];
    (*argc)--;
    *argv=res;
    memcpy(ptr,&alloc,sizeof(alloc));		/* Save alloc root for free */
    DBUG_VOID_RETURN;
  }

  group.count=0;
  group.name="defaults";
  group.type_names=groups;
  for (; *groups ; groups++)
    group.count++;

  if (init_dynamic_array(&args, sizeof(char*),*argc, 32))
    goto err;
  if (dirname_length(conf_file))
  {
    if (search_default_file(&args, &alloc, NullS, conf_file, &group))
      goto err;
  }
  else
  {
    for (dirs=default_directories ; *dirs; dirs++)
    {
      if (search_default_file(&args, &alloc, *dirs, conf_file, &group))
	goto err;
    }
  }
  if (!(ptr=(char*) alloc_root(&alloc,sizeof(alloc)+
			       (args.elements + *argc +1) *sizeof(char*))))
    goto err;
  res= (char**) (ptr+sizeof(alloc));

  /* copy name + found arguments + command line arguments to new array */
  res[0]=argv[0][0];
  memcpy((gptr) (res+1), args.buffer, args.elements*sizeof(char*));

  /* Check if we wan't to see the new argument list */
  if (*argc >= 2 && !strcmp(argv[0][1],"--print-defaults"))
  {
    print_defaults=1;
    --*argc; ++*argv;				/* skipp argument */
  }

  memcpy((gptr) (res+1+args.elements), (char*) ((*argv)+1),
	 (*argc-1)*sizeof(char*));
  res[args.elements+ *argc]=0;			/* last null */

  (*argc)+=args.elements;
  *argv= (char**) res;
  memcpy(ptr,&alloc,sizeof(alloc));		/* Save alloc root for free */
  delete_dynamic(&args);
  if (print_defaults)
  {
    int i;
    printf("%s would have been started with the following arguments:\n",
	   **argv);
    for (i=1 ; i < *argc ; i++)
      printf("%s ", (*argv)[i]);
    puts("");
    exit(1);
  }
  DBUG_VOID_RETURN;

 err:
  fprintf(stderr,"Program aborted\n");
  exit(1);
}


void free_defaults(char **argv)
{
  MEM_ROOT ptr;
  memcpy((char*) &ptr,(char *) argv - sizeof(ptr),sizeof(ptr));
  free_root(&ptr);
}


static my_bool search_default_file(DYNAMIC_ARRAY *args, MEM_ROOT *alloc,
				   my_string dir, const char *config_file,
				   TYPELIB *group)
{
  char name[FN_REFLEN+10],buff[257],*ptr,*end,*value,*tmp;
  FILE *fp;
  uint line=0;
  my_bool read_values=0,found_group=0;

  if ((dir ? strlen(dir) : 0 )+strlen(config_file) >= FN_REFLEN-3)
    return 0;					/* Ignore wrong paths */
  if (dir)
  {
    strmov(name,dir);
    convert_dirname(name);
    if (dir[0] == FN_HOMELIB)			/* Add . to filenames in home */
      strcat(name,".");
    strxmov(strend(name),config_file,default_ext,NullS);
  }
  else
  {
    strmov(name,config_file);
  }
  if (!(fp = my_fopen(fn_format(name,name,"","",4),O_RDONLY,MYF(0))))
    return 0;					/* Ignore wrong files */

  while (fgets(buff,sizeof(buff)-1,fp))
  {
    line++;
    /* Ignore comment and empty lines */
    for (ptr=buff ; isspace(*ptr) ; ptr++ ) ;
    if (*ptr == '#' || *ptr == ';' || !*ptr)
      continue;
    if (*ptr == '[')				/* Group name */
    {
      found_group=1;
      if (!(end=(char *) strchr(++ptr,']')))
      {
	fprintf(stderr,
		"error: Wrong group definition in config file: %s at line %d\n",
		name,line);
	goto err;
      }
      for ( ; isspace(end[-1]) ; end--) ;	/* Remove end space */
      end[0]=0;
      read_values=find_type(ptr,group,3) > 0;
      continue;
    }
    if (!found_group)
    {
      fprintf(stderr,
	      "error: Found option without preceding group in config file: %s at line: %d\n",
	      name,line);
      goto err;
    }
    if (!read_values)
      continue;
    if (!(end=value=strchr(ptr,'=')))
      end=strend(ptr);				/* Option without argument */
    for ( ; isspace(end[-1]) ; end--) ;
    if (!value)
    {
      if (!(tmp=alloc_root(alloc,(uint) (end-ptr)+3)))
	goto err;
      strmake(strmov(tmp,"--"),ptr,(uint) (end-ptr));
      if (insert_dynamic(args,(gptr) &tmp))
	goto err;
    }
    else
    {
      /* Remove pre- and end space */
      char *value_end;
      for (value++ ; isspace(*value); value++) ;
      value_end=strend(value);
      for ( ; isspace(value_end[-1]) ; value_end--) ;

      if (!(tmp=alloc_root(alloc,(uint) (end-ptr)+3 +
			   (uint) (value_end-value)+1)))
	goto err;
      if (insert_dynamic(args,(gptr) &tmp))
	goto err;
      ptr=strnmov(strmov(tmp,"--"),ptr,(uint) (end-ptr));
      *ptr++= '=';
      for ( ; value != value_end; value++)
      {
	if (*value == '\\' && value != value_end-1)
	{
	  switch(*++value) {
	  case 'n':
	    *ptr++='\n';
	    break;
	  case 't':
	    *ptr++= '\t';
	    break;
	  case 'r':
	    *ptr++ = '\r';
	    break;
	  case 'b':
	    *ptr++ = '\b';
	    break;
	  case 's':
	    *ptr++= ' ';			/* space */
	    break;
	  case '\\':
	    *ptr++= '\\';
	    break;
	  default:				/* Unknown; Keep '\' */
	    *ptr++= '\\';
	    *ptr++= *value;
	    break;
	  }
	}
	else
	  *ptr++= *value;
      }
      *ptr=0;
    }
  }
  my_fclose(fp,MYF(0));
  return(0);

 err:
  my_fclose(fp,MYF(0));
  return 1;
}
