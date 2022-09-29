/* GNU DIFF main routine.
   Copyright (C) 1988, 1989 Free Software Foundation, Inc.

This file is part of GNU DIFF.

GNU DIFF is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GNU DIFF is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU DIFF; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* GNU DIFF was written by Mike Haertel, David Hayes,
   Richard Stallman and Len Tower.  */

#define GDIFF_MAIN
#include "regex.h"
#include "diff.h"
#include <getopt.h>


/* Nonzero for -r: if comparing two directories,
   compare their common subdirectories recursively.  */

int recursive;

/* For debugging: don't do discard_confusing_lines.  */

int no_discards;

/* Return a string containing the command options with which diff was invoked.
   Spaces appear between what were separate ARGV-elements.
   There is a space at the beginning but none at the end.
   If there were no options, the result is an empty string.

   Arguments: OPTIONVEC, a vector containing separate ARGV-elements, and COUNT,
   the length of that vector.  */

static char *
option_list (optionvec, count)
     char **optionvec;  /* Was `vector', but that collides on Alliant.  */
     int count;
{
  int i;
  int length = 0;
  char *result;

  for (i = 0; i < count; i++)
    length += strlen (optionvec[i]) + 1;

  result = (char *) xmalloc (length + 1);
  result[0] = 0;

  for (i = 0; i < count; i++)
    {
      strcat (result, " ");
      strcat (result, optionvec[i]);
    }

  return result;
}

int xargc;
char **xargv;
int gnudiffopts;
int sgidiffopts;

VOID
usage ()
{
  fprintf (stderr, "Usage: diff [-bcefhilnrstw -Dstring -Sname -xpattern ] file1 file2\n");
  fprintf (stderr, "\tGNU diff options: [-# -aBdHNpqTuv -Cnum -Fregexp -Iregexp -Llabel]\n");
  exit (2);
} 

VOID
specify_style (style)
     enum output_style style;
{
  if (output_style != OUTPUT_NORMAL
      && output_style != style)
    error ("conflicting specifications of output style");
  output_style = style;
}

main (argc, argv)
     int argc;
     char *argv[];
{
  int val;
  int c;
  int prev = -1;
  extern char *version_string;
  extern char *optarg;
  extern int optind;
  extern void setxpat(char *);

  program = argv[0];

  /* Do our initializations. */
  output_style = OUTPUT_NORMAL;
  always_text_flag = FALSE;
  ignore_space_change_flag = FALSE;
  ignore_all_space_flag = FALSE;
  length_varies = FALSE;
  ignore_case_flag = FALSE;
  ignore_blank_lines_flag = FALSE;
  ignore_regexp = 0;
  function_regexp = 0;
  print_file_same_flag = FALSE;
  entire_new_file_flag = FALSE;
  no_details_flag = FALSE;
  context = -1;
  line_end_char = '\n';
  tab_align_flag = FALSE;
  tab_expand_flag = FALSE;
  recursive = FALSE;
  paginate_flag = FALSE;
  ifdef_string = NULL;
  heuristic = FALSE;
  dir_start_file = NULL;
  msg_chain = NULL;
  msg_chain_end = NULL;
  no_discards = 0;
  gnudiffopts = 0;
  sgidiffopts = 0;
  xargc = 0;
  xargv = xmalloc ((argc+1) * sizeof(*xargv));
  bzero (xargv, (argc+1) * sizeof(*xargv));
  /*
   * Put all but the last 2 args (the filenames to diff)
   * in the xargv[] vector, to be passed to odiff by tryodiff().
   *
   * If "diff -r dir1 dir2" calls odiff on a big file
   * somewhere, must call odiff with "diff -r file1 file2"
   * not with original "dir1 dir2" args.  Otherwise get
   * all output repeated -- see SGI PV Incident 234771.
   * Would be better to also trim -r, but that seems
   * harder, and I can't see that it matters.  pj.
   */
  while (xargc < argc - 2) {
    xargv[xargc] = argv[xargc];
    xargc++;
  }
  xargv[xargc+2] = 0;

  /* Decode the options.  */

  while ((c = getopt (argc, argv,
			   "0123456789abBcC:dD:efF:hHiI:lL:nNpqrsS:tTuvwx:"
			   )) != EOF)
    {
      switch (c)
	{
	  /* All digits combine in decimal to specify the context-size.  */
	case '1':	gnudiffopts++;
	case '2':	gnudiffopts++;
	case '3':	gnudiffopts++;
	case '4':	gnudiffopts++;
	case '5':	gnudiffopts++;
	case '6':	gnudiffopts++;
	case '7':	gnudiffopts++;
	case '8':	gnudiffopts++;
	case '9':	gnudiffopts++;
	case '0':	gnudiffopts++;
	  if (context == -1)
	    context = 0;
	  /* If a context length has already been specified,
	     more digits allowed only if they follow right after the others.
	     Reject two separate runs of digits, or digits after -C.  */
	  else if (prev < '0' || prev > '9')
	    fatal ("context length specified twice");

	  context = context * 10 + c - '0';
	  break;

	case 'a':	gnudiffopts++;
	  /* Treat all files as text files; never treat as binary.  */
	  always_text_flag = 1;
	  break;

	case 'b':	sgidiffopts++;
	  /* Ignore changes in amount of whitespace.  */
	  ignore_space_change_flag = 1;
	  length_varies = 1;
	  break;

	case 'B':	gnudiffopts++;
	  /* Ignore changes affecting only blank lines.  */
	  ignore_blank_lines_flag = 1;
	  break;

	case 'C':	gnudiffopts++;
	  if (optarg)
	    {
	      if (context >= 0)
		fatal ("context length specified twice");
	      {
		char *p;
		for (p = optarg; *p; p++)
		  if (*p < '0' || *p > '9')
		    fatal ("invalid context length argument");
	      }
	      context = atoi (optarg);
	    }

	  /* Falls through.  */
	case 'c':	sgidiffopts++;
	  /* Make context-style output.  */
	  specify_style (c == 130 ? OUTPUT_UNIFIED : OUTPUT_CONTEXT);
	  break;

	case 'd':	gnudiffopts++;
	  /* Don't discard lines.  This makes things slower (sometimes much
	     slower) but will find a guaranteed minimal set of changes.  */
	  no_discards = 1;
	  break;

	case 'D':	sgidiffopts++;
	  /* Make merged #ifdef output.  */
	  specify_style (OUTPUT_IFDEF);
	  ifdef_string = optarg;
	  break;

	case 'e':	sgidiffopts++;
	  /* Make output that is a valid `ed' script.  */
	  specify_style (OUTPUT_ED);
	  break;

	case 'f':	sgidiffopts++;
	  /* Make output that looks vaguely like an `ed' script
	     but has changes in the order they appear in the file.  */
	  specify_style (OUTPUT_FORWARD_ED);
	  break;

	case 'F':	gnudiffopts++;
	  /* Show, for each set of changes, the previous line that
	     matches the specified regexp.  Currently affects only
	     context-style output.  */
	  function_regexp = optarg;
	  break;

	case 'h':	sgidiffopts++;
	  /* Split the files into chunks of around 1500 lines
	     for faster processing.  Usually does not change the result.

	     This currently has no effect.  */
	  break;

	case 'H':	gnudiffopts++;
	  /* Turn on heuristics that speed processing of large files
	     with a small density of changes.  */
	  heuristic = 1;
	  break;

	case 'i':	sgidiffopts++;
	  /* Ignore changes in case.  */
	  ignore_case_flag = 1;
	  break;

	case 'I':	gnudiffopts++;
	  /* Ignore changes affecting only lines that match the
	     specified regexp.  */
	  ignore_regexp = optarg;
	  break;

	case 'l':	sgidiffopts++;
	  /* Pass the output through `pr' to paginate it.  */
	  paginate_flag = 1;
	  break;

	case 'L':	gnudiffopts++;
	  /* Specify file labels for `-c' output headers.  */
	  if (!file_label[0])
	    file_label[0] = optarg;
	  else if (!file_label[1])
	    file_label[1] = optarg;
	  else
	    fatal ("too many file label options");
	  break;

	case 'n':	sgidiffopts++;
	  /* Output RCS-style diffs, like `-f' except that each command
	     specifies the number of lines affected.  */
	  specify_style (OUTPUT_RCS);
	  break;

	case 'N':	gnudiffopts++;
	  /* When comparing directories, if a file appears only in one
	     directory, treat it as present but empty in the other.  */
	  entire_new_file_flag = 1;
	  break;

	case 'p':	gnudiffopts++;
	  /* Make context-style output and show name of last C function.  */
	  specify_style (OUTPUT_CONTEXT);
	  function_regexp = "^[_a-zA-Z]";
	  break;

	case 'q':	gnudiffopts++;
	  no_details_flag = 1;
	  break;

	case 'r':	sgidiffopts++;
	  /* When comparing directories, 
	     recursively compare any subdirectories found.  */
	  recursive = 1;
	  break;

	case 's':	sgidiffopts++;
	  /* Print a message if the files are the same.  */
	  print_file_same_flag = 1;
	  break;

	case 'S':	sgidiffopts++;
	  /* When comparing directories, start with the specified
	     file name.  This is used for resuming an aborted comparison.  */
	  dir_start_file = optarg;
	  break;

	case 't':	sgidiffopts++;
	  /* Expand tabs to spaces in the output so that it preserves
	     the alignment of the input files.  */
	  tab_expand_flag = 1;
	  break;

	case 'T':	gnudiffopts++;
	  /* Use a tab in the output, rather than a space, before the
	     text of an input line, so as to keep the proper alignment
	     in the input line without changing the characters in it.  */
	  tab_align_flag = 1;
	  break;

	case 'v':	gnudiffopts++;
	  printf ("GNU diff version %s\n", version_string);
	  break;

	case 'u':	gnudiffopts++;
	  /* Output the context diff in unidiff format.  */
	  specify_style (OUTPUT_UNIFIED);
	  break;

	case 'w':	sgidiffopts++;
	  /* Ignore horizontal whitespace when comparing lines.  */
	  ignore_all_space_flag = 1;
	  length_varies = 1;
	  break;

	case 'x':	sgidiffopts++;
	  setxpat(optarg);
	  break;

	default:
	  usage ();
	}
      prev = c;
    }

  if (optind != argc - 2)
    usage ();

  if (ignore_regexp)
    {
      char *val;
      bzero (&ignore_regexp_compiled, sizeof ignore_regexp_compiled);
      val = re_compile_pattern (ignore_regexp, strlen (ignore_regexp),
				&ignore_regexp_compiled);
      if (val != 0)
	error ("%s: %s", ignore_regexp, val);
      ignore_regexp_compiled.fastmap = (char *) xmalloc (256);
    }

  if (function_regexp)
    {
      char *val;
      bzero (&function_regexp_compiled, sizeof function_regexp_compiled);
      val = re_compile_pattern (function_regexp, strlen (function_regexp),
				&function_regexp_compiled);
      if (val != 0)
	error ("%s: %s", function_regexp, val);
      function_regexp_compiled.fastmap = (char *) xmalloc (256);
    }

  if (output_style != OUTPUT_CONTEXT && output_style != OUTPUT_UNIFIED)
    context = 0;
  else if (context == -1)
    /* Default amount of context for -c.  */
    context = 3;

  switch_string = option_list (argv + 1, optind - 1);

  val = compare_files (0, argv[optind], 0, argv[optind + 1], 0);

  /* Print any messages that were saved up for last.  */
  print_message_queue ();

  if (ferror (stdout) || fclose (stdout) != 0)
    fatal ("write error");
  exit (val);
}

static char *
type( mode_t mode )
{
	if(S_ISFIFO(mode))
		return("fifo");
	else if(S_ISCHR(mode))
		return("character special");
	else if(S_ISBLK(mode))
		return("block special");
	else if(S_ISREG(mode))
		return("regular");
	else if(S_ISLNK(mode))
		return("symbolic link");
	else if(S_ISSOCK(mode))
		return("socket");
	else
		return("unknown");
}

/* Compare two files (or dirs) with specified names
   DIR0/NAME0 and DIR1/NAME1, at level DEPTH in directory recursion.
   (if DIR0 is 0, then the name is just NAME0, etc.)
   This is self-contained; it opens the files and closes them.

   Value is 0 if files are identical, 1 if different,
   2 if there is a problem opening them.  */

int
compare_files (dir0, name0, dir1, name1, depth)
     char *dir0, *dir1;
     char *name0, *name1;
     int depth;
{
  static char Standard_Input[] = "Standard Input";
  struct file_data inf[2];
  register int i;
  int val;
  int errorcount = 0;	/* Bit 1 for stat or open errors */
			/* Bit 2 for fifo, block and character special */
  int stat_result[2];

  /* If this is directory comparison, perhaps we have a file
     that exists only in one of the directories.
     If so, just print a message to that effect.  */

  if (! entire_new_file_flag && (name0 == 0 || name1 == 0))
    {
      char *name = name0 == 0 ? name1 : name0;
      char *dir = name0 == 0 ? dir1 : dir0;
      message ("Only in %s: %s\n", dir, name);
      /* Return 1 so that diff_dirs will return 1 ("some files differ").  */
      return 1;
    }

  /* Mark any nonexistent file with -1 in the desc field.  */
  /* Mark unopened files (i.e. directories) with -2. */

  inf[0].desc = name0 == 0 ? -1 : -2;
  inf[1].desc = name1 == 0 ? -1 : -2;

  /* Now record the full name of each file, including nonexistent ones.  */

  if (name0 == 0)
    name0 = name1;
  if (name1 == 0)
    name1 = name0;

  inf[0].name = dir0 == 0 ? name0 : concat (dir0, "/", name0);
  inf[1].name = dir1 == 0 ? name1 : concat (dir1, "/", name1);

  /* Stat the files.  Record whether they are directories.
     Record in stat_result whether stat fails.  */

  for (i = 0; i <= 1; i++)
    {
      bzero (&inf[i].stat, sizeof(struct stat));
      inf[i].dir_p = 0;
      stat_result[i] = 0;

      if (inf[i].desc != -1)
	{
	  char *filename = inf[i].name;

	  stat_result[i] = 
	    strcmp (filename, "-")
	      ? stat (filename, &inf[i].stat)
	      : fstat (0, &inf[i].stat);
		  
	  if (stat_result[i] < 0)
	    {
	      perror_with_name (filename);
	      errorcount |= 1;
	    }
	  else
	    inf[i].dir_p = 
	      S_IFDIR == (inf[i].stat.st_mode & S_IFMT)
	      && strcmp (filename, "-");
	}
    }

  /* See if the two named files are actually the same physical file.
     If so, we know they are identical without actually reading them.  */

  if (output_style != OUTPUT_IFDEF
      && inf[0].stat.st_ino == inf[1].stat.st_ino
      && inf[0].stat.st_dev == inf[1].stat.st_dev
      && stat_result[0] == 0
      && stat_result[1] == 0)
    {
      val = 0;
      goto done;
    }

  if (name0 == 0)
    inf[0].dir_p = inf[1].dir_p;
  if (name1 == 0)
    inf[1].dir_p = inf[0].dir_p;

  /* Open the files and record their descriptors.  */

  for (i = 0; i <= 1; i++)
    {
      if (inf[i].desc == -1)
	;
      else if (!strcmp (inf[i].name, "-"))
	{
	  inf[i].desc = 0;
	  inf[i].name = Standard_Input;
	}
      /* Don't bother opening if stat already failed.  */
      else if (stat_result[i] == 0 && ! inf[i].dir_p)
	{
	  char *filename = inf[i].name;

	  /* If both are directories */
#ifdef sgi
	  if(dir0 != 0 && dir1 != 0)
#else
	  if(strchr(inf[0].name,'/') && strchr(inf[1].name,'/'))
#endif
	  {
	    /* Check for a fifo, block or character special */
	    if(inf[i].stat.st_mode & (S_IFIFO|S_IFCHR|S_IFBLK))
	      errorcount |= 2;
	  }
	  if(!(errorcount&2))
	  {
	    inf[i].desc = open (filename, O_RDONLY, 0);
	    if (0 > inf[i].desc)
	      {
	        perror_with_name (filename);
	        errorcount |= 1;
	      }
	  }
	}
    }

  if (errorcount)
    {
       if(errorcount & 2)
       {
	 fprintf(stdout,"File %s is a %s while file %s is a %s\n",
		   inf[0].name,type(inf[0].stat.st_mode),
		   inf[1].name,type(inf[1].stat.st_mode));
	   
       }
       /* If either file should exist but fails to be opened, return 2.
          if it was just a file type mismatch, return 1 */

       val = (errorcount & 1) ? 2 : 1;

    }
  else if (inf[0].dir_p && inf[1].dir_p)
    {
      if (output_style == OUTPUT_IFDEF)
	fatal ("-D option not supported with directories");

      /* If both are directories, compare the files in them.  */

      if (depth > 0 && !recursive)
	{
	  /* But don't compare dir contents one level down
	     unless -r was specified.  */
	  message ("Common subdirectories: %s and %s\n",
		   inf[0].name, inf[1].name);
	  val = 0;
	}
      else
	{
	  val = diff_dirs (inf[0].name, inf[1].name, 
			   compare_files, depth, 0, 0);
	}

    }
  else if (depth == 0 && (inf[0].dir_p || inf[1].dir_p))
    {

      /* If only one is a directory, and it was specified in the command line,
	 use the file in that dir whose basename matches the other file.  */

      int dir_arg = (inf[0].dir_p ? 0 : 1);
      int fnm_arg = (inf[0].dir_p ? 1 : 0);
      char *p = rindex (inf[fnm_arg].name, '/');
      char *filename = concat (inf[dir_arg].name,  "/",
			       (p ? p+1 : inf[fnm_arg].name));

      if (inf[fnm_arg].name == Standard_Input)
	fatal ("can't compare - to a directory");

      inf[dir_arg].desc = open (filename, O_RDONLY, 0);

      if (0 > inf[dir_arg].desc)
	{
	  perror_with_name (filename);
	  val = 2;
	}
      else
	{
	  /* JF: patch from the net to check and make sure we can really free
	     this.  If it's from argv[], freeing it is a *really* bad idea */
	  if (0 != (dir_arg ? dir1 : dir0))
	    free (inf[dir_arg].name);
	  inf[dir_arg].name = filename;
	  if (fstat (inf[dir_arg].desc, &inf[dir_arg].stat) < 0)
	    pfatal_with_name (inf[dir_arg].name);

	  inf[dir_arg].dir_p
	    = (S_IFDIR == (inf[dir_arg].stat.st_mode & S_IFMT));
	  if (inf[dir_arg].dir_p)
	    {
	      error ("%s is a directory but %s is not",
		     inf[dir_arg].name, inf[fnm_arg].name);
	      val = 1;
	    }
	  else
	    val = diff_2_files (inf, depth);
	}

    }
  else if (depth > 0 && (inf[0].dir_p || inf[1].dir_p))
    {
      /* Perhaps we have a subdirectory that exists only in one directory.
	 If so, just print a message to that effect.  */

      if (inf[0].desc == -1 || inf[1].desc == -1)
	{
	  if (entire_new_file_flag && recursive)
	    val = diff_dirs (inf[0].name, inf[1].name, compare_files, depth,
			     inf[0].desc == -1, inf[1].desc == -1);
	  else
	    {
	      char *dir = (inf[0].desc == -1) ? dir1 : dir0;
	      message ("Only in %s: %s\n", dir, name0);
	      val = 1;
	    }
	}
      else
	{
	  /* We have a subdirectory in one directory
	     and a file in the other.  */

	  if (inf[0].dir_p)
	    message ("%s is a directory but %s is not\n",
		     inf[0].name, inf[1].name);
	  else
	    message ("%s is a directory but %s is not\n",
		     inf[1].name, inf[0].name);
	  /* This is a difference.  */
	  val = 1;
	}
    }
  else
    {

      /* Both exist and both are ordinary files.  */

      val = diff_2_files (inf, depth);

    }

  /* Now the comparison has been done, if no error prevented it,
     and VAL is the value this function will return.  */

  if (inf[0].desc >= 0)
    close (inf[0].desc);
  if (inf[1].desc >= 0)
    close (inf[1].desc);

 done:
  if (val == 0 && !inf[0].dir_p)
    {
      if (print_file_same_flag)
	message ("Files %s and %s are identical\n",
		 inf[0].name, inf[1].name);
    }
  else
    fflush (stdout);

  if (dir0 != 0)
    free (inf[0].name);
  if (dir1 != 0)
    free (inf[1].name);

  return val;
}
