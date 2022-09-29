/**************************************************************************
 *                                                                        *
 * Copyright (C) 1995 Silicon Graphics, Inc.                         	  *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident  "$Revision: 1.4 $"

#include <dirent.h>
#include <getopt.h>
#include <langinfo.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sgi_nl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <msgs/uxsgicore.h>
#include "localecmdI.h"

#define  LOCALE_PATH "/usr/lib/locale"
#define  CHARMAP_PATH "/usr/lib/locale/charmap"

#define	 NCATEGORIES	6

struct lconv *localecnv;

char    cmd_label[] = "UX:locale";

int	error_flag = 0;

char	* get_locale_info(int nl_item);

enum value_type { none, string, stringarray, byte, bytearray, integer };

typedef  struct cat_item
  {
    int item_id;
    const char *name;
    enum { std, opt } status;
    enum value_type value_type;
    int min;
    int max;
  } cat_item_def;

typedef struct category_struct
{
  int cat_id;
  const char *name;
  size_t number;
  cat_item_def item_desc[50];
}category_type;

static category_type category[NCATEGORIES] = {
	{LC_COLLATE, "LC_COLLATE", 0, {0}},
	{LC_CTYPE, "LC_CTYPE", 1, {
                     { CODESET, "charmap", std, string },
                     { 0 },
		    },
	},
	{LC_MONETARY, "LC_MONETARY", 15, {
                  { INT_CURR_SYMBOL,   "int_curr_symbol",   std, string },
                  { CURRENCY_SYMBOL,   "currency_symbol",   std, string },
                  { MON_DECIMAL_POINT, "mon_decimal_point", std, string },
                  { MON_THOUSANDS_SEP, "mon_thousands_sep", std, string },
                  { MON_GROUPING,      "mon_grouping",      std, bytearray },
                  { POSITIVE_SIGN,     "positive_sign",     std, string },
                  { NEGATIVE_SIGN,     "negative_sign",     std, string },
                  { INT_FRAC_DIGITS,   "int_frac_digits",   std, byte },
                  { FRAC_DIGITS,       "frac_digits",       std, byte },
                  { P_CS_PRECEDES,     "p_cs_precedes",     std, byte, 0, 1 },
                  { P_SEP_BY_SPACE,    "p_sep_by_space",    std, byte, 0, 2 },
                  { N_CS_PRECEDES,     "n_cs_precedes",     std, byte, 0, 1 },
                  { N_SEP_BY_SPACE,    "n_sep_by_space",    std, byte, 0, 2 },
                  { P_SIGN_POSN,       "p_sign_posn",       std, byte, 0, 4 },
                  { N_SIGN_POSN,       "n_sign_posn",       std, byte, 0, 4 },
                  { 0 },
		},
	},
	{LC_NUMERIC, "LC_NUMERIC", 3, {
                  { DECIMAL_POINT, "decimal_point", std, string },
                  { THOUSANDS_SEP, "thousands_sep", std, string },
                  { GROUPING,      "grouping",      std, bytearray },
                  { 0 },
		},
	},
	{LC_TIME, "LC_TIME", 13, {
                  { ABDAY_1,    "abday",      std, stringarray,  7,  7 },
                  { DAY_1,      "day",        std, stringarray,  7,  7 },
                  { ABMON_1,    "abmon",      std, stringarray, 12, 12 },
                  { MON_1,      "mon",        std, stringarray, 12, 12 },
                  { AM_STR,     "am_pm",      std, stringarray,  2,  2 },
                  { D_T_FMT,    "d_t_fmt",    std, string },
                  { D_FMT,      "d_fmt",      std, string },
                  { T_FMT,      "t_fmt",      std, string },
                  { T_FMT_AMPM, "t_fmt_ampm", std, string },
                  { ERA,        "era",        opt, string },
                  { ERA_YEAR,   "era_year",   opt, string },
                  { ERA_D_FMT,  "era_d_fmt",  opt, string },
                  { ALT_DIGITS, "alt_digits", opt, stringarray,  0, 100 },
                  { 0 },
		},
	},
	{LC_MESSAGES, "LC_MESSAGES", 4, {
                  { YESEXPR, "yesexpr", std, string },
                  { NOEXPR,  "noexpr",  std, string },
                  { YESSTR,  "yesstr",  opt, string },
                  { NOSTR,   "nostr",   opt, string },
                  { 0 },
		},
	},
};

/* If set print the name of the category.  */
static int show_category_name;

/* If set print the name of the item.  */
static int show_keyword_name;

/* Prototypes for local functions.  */
static void usage (int status); 
static void write_locales (void);
static void write_charmaps (void);
static void show_locale_vars (void);
static void show_info (const char *name);
static void dump_category (const char *name);


main (int argc, char *argv[])
{
  int optchar;
  int do_all = 0;
  int do_help = 0;
  int do_version = 0;
  int do_charmaps = 0;

  /* Set initial values for global varaibles.  */
  show_category_name = 0;
  show_keyword_name = 0;

  /* Set locale.  Do not set LC_ALL because the other categories must
     not be affected (acccording to POSIX.2).  */
  (void) setlocale(LC_ALL, "");
  (void)setcat("uxsgicore");
  (void)setlabel(cmd_label);

  while ((optchar = getopt (argc, argv, "achkm")) != EOF)
    switch (optchar)
      {
      case '\0':
	break;
      case 'a':
	do_all = 1;
	break;
      case 'c':
	show_category_name = 1;
	break;
      case 'h':
	do_help = 1;
	break;
      case 'k':
	show_keyword_name = 1;
	break;
      case 'm':
	do_charmaps = 1;
	break;
      default:
	printf("illegal option \"%s\"", optarg);
	break;
      }

  /* Help is requested.  */
  if (do_help)
    usage (EXIT_SUCCESS);

  /* `-a' requests the names of all available locales.  */
  if (do_all != 0)
    {
      write_locales ();
      exit (EXIT_SUCCESS);
    }

  /* `m' requests the names of all available charmaps.  The names can be
     used for the -f argument to localedef(3).  */
  if (do_charmaps != 0)
    {
      write_charmaps ();
      exit (EXIT_SUCCESS);
    }

  localecnv = localeconv();
  /* If no real argument is given we have to print the contents of the
     current locale definition variables.  These are LANG and the LC_*.  */
  if (optind == argc && show_keyword_name == 0 && show_category_name == 0)
    {
      show_locale_vars ();
      exit (EXIT_SUCCESS);
    }

  /* Process all given names.  */
  while (optind <  argc)
    show_info (argv[optind++]);

  if (error_flag) exit (1);
  else exit (EXIT_SUCCESS);
}


/* Display usage information and exit.  */
static void
usage(int status)
{
	char	usage_string[] = "\
Usage: locale [OPTION]... name\n\
Mandatory arguments to long options are mandatory for short options too.\n\
  -h, --help            display this help and exit\n\
  -a, --all-locales     write names of available locales\n\
  -m, --charmaps        write names of available charmaps\n\
  -c, --category-name   write names of selected categories\n\
  -k, --keyword-name    write names of selected keywords\n\
";

  _sgi_nl_usage(SGINL_USAGE, cmd_label, gettxt(_SGI_MMX_locale_usage,
		usage_string));
  exit (status);
}


/* Write the names of all available locales to stdout.  */
static void
write_locales (void)
{
  DIR *dir;
  struct dirent *dirent;

  /* `POSIX' locale is always available (POSIX.2 4.34.3).  */
  puts ("POSIX");

  dir = opendir(LOCALE_PATH);
  if (dir == NULL)
    {
      _sgi_nl_error(SGINL_NOSYSERR, cmd_label, 
	gettxt(_SGI_MMX_locale_locpath,
      	    "cannot read locale directory <%s>"), LOCALE_PATH);
      error_flag = 1;
      return;
    }

  if (chdir(LOCALE_PATH) != 0)
    {
      _sgi_nl_error(SGINL_NOSYSERR, cmd_label, 
	gettxt(_SGI_MMX_CannotChdir,
      	    "Cannot chdir to %s"), LOCALE_PATH);
      error_flag = 1;
      return;
    }

  /* Now we can look for all files in the directory.  */
  while ((dirent = readdir (dir)) != NULL)
    if (strcmp (dirent->d_name, ".") != 0
	&& strcmp (dirent->d_name, "..") != 0
	&& strcmp (dirent->d_name, "POSIX") != 0)
      {
        struct stat stbuf;
        char locbuf[sizeof(dirent->d_name)+10];
        strcpy(locbuf, dirent->d_name);
        strcat(locbuf, "/");
        strcat(locbuf, "LC_CTYPE");
        if (stat(locbuf, &stbuf) == 0)
          puts (dirent->d_name);
      }

  closedir (dir);
}


/* Write the names of all available character maps to stdout.  */
static void
write_charmaps (void)
{
  DIR *dir;
  struct dirent *dirent;

  dir = opendir (CHARMAP_PATH);
  if (dir == NULL)
    {
      _sgi_nl_error(SGINL_NOSYSERR, cmd_label, 
	gettxt(_SGI_MMX_locale_charpath,
      	    "cannot read charmap directory <%s>"), CHARMAP_PATH);
      error_flag = 1;
      return;
    }

  /* Now we can look for all files in the directory.  */
  while ((dirent = readdir (dir)) != NULL)
    if (strcmp (dirent->d_name, ".") != 0
	&& strcmp (dirent->d_name, "..") != 0)
      puts (dirent->d_name);

  closedir (dir);
}


/* We have to show the contents of the environments determining the
   locale.  */
static void
show_locale_vars (void)
{
  size_t cat_no;
  const char *lcall = getenv ("LC_ALL");
  const char *lang = getenv ("LANG");

  if (!lang) lang = "POSIX";
  /* LANG has to be the first value.  */
  printf ("LANG=%s\n", lang);

  /* Now all categories in an unspecified order.  */
  for (cat_no = 0; cat_no < NCATEGORIES; ++cat_no)
  {
	char *val = getenv(category[cat_no].name);
	if (lcall != NULL )
         	printf ("%s=\"%s\"\n", category[cat_no].name, lcall);
	else if ( val != NULL )
		printf ("%s=%s\n", category[cat_no].name, val); 
	else /* lcall == NULL and val == NULL */
	        printf ("%s=\"%s\"\n", category[cat_no].name, 
			setlocale(category[cat_no].cat_id, NULL));
	

  }
  /* The last is the LC_ALL value.  */
  printf ("LC_ALL=%s\n", lcall ? lcall : "");
}

void print_item (struct cat_item *item)
{
    if (show_keyword_name != 0)
	printf ("%s=", item->name);

    switch (item->value_type)
	{
	case string:
	  printf ("%s%s%s", show_keyword_name ? "\"" : "",
		  get_locale_info (item->item_id) ? get_locale_info (item->item_id) : "",
		  show_keyword_name ? "\"" : "");
	  break;
	case stringarray:
	  {
	    int cnt;
	    const char *val;

	    /*
	    if (show_keyword_name)
	      putchar ('"');
	    */

	    for (cnt = 0; cnt < item->max - 1; ++cnt)
	      {
		val = get_locale_info (item->item_id + cnt);
		printf ("\"%s\";", val ? val : "");
	      }

	    val = get_locale_info (item->item_id + cnt);
	    printf ("\"%s\"", val ? val : "");

	    /* 
	    if (show_keyword_name)
	      putchar ('"');
	    */
	  }
	  break;
	case byte:
	  {
	    const char *val = get_locale_info (item->item_id);

	    if (val != NULL)
	      printf ("%d", *val == CHAR_MAX ? -1 : *val);
	  }
	  break;
	case bytearray:
	  {
	    const char *val = get_locale_info (item->item_id);
	    int cnt = val ? strlen (val) : 0;

	    while (cnt > 1)
	      {
		printf ("%d;", *val == CHAR_MAX ? -1 : *val);
              --cnt;
		++val;
	      }

	    printf ("%d", cnt == 0 || *val == CHAR_MAX ? -1 : *val);
	  }
	  break;
	default: break;
	}
    putchar ('\n');
}

/* Show the information request for NAME.  */
static void
show_info (const char *name)
{
  size_t cat_no;

  for (cat_no = 0; cat_no < NCATEGORIES; ++cat_no)
    {
      size_t item_no;

      if (strcmp (name, category[cat_no].name) == 0)
	/* Print the whole category.  */
	{
	  if (show_category_name != 0)
	    puts (category[cat_no].name);

	  for (item_no = 0; item_no < category[cat_no].number; ++item_no)
	    print_item (&category[cat_no].item_desc[item_no]);

	  return;
	}
      
      for (item_no = 0; item_no < category[cat_no].number; ++item_no)
	if (strcmp (name, category[cat_no].item_desc[item_no].name) == 0)
	  {
	    if (show_category_name != 0)
	      puts (category[cat_no].name);

	    print_item (&category[cat_no].item_desc[item_no]);
	    return;
	  }
    }
    _sgi_nl_error(SGINL_NOSYSERR, cmd_label, 
	gettxt(_SGI_MMX_locale_invname,
        "Not a recognized keyword <%s>"), name);
    error_flag = 1;
}

char	* get_locale_info(int	nl_item)
{
	if (nl_item >= DECIMAL_POINT && nl_item <= N_SIGN_POSN)
	{
	   switch(nl_item) 
	   {
		case DECIMAL_POINT: 
			return (localecnv->decimal_point); 
			break;
		case THOUSANDS_SEP: 
			return (localecnv->thousands_sep); 
			break;
		case GROUPING: 
			return (localecnv->grouping); 
			break;
		case INT_CURR_SYMBOL: 
			return (localecnv->int_curr_symbol); 
			break;
		case CURRENCY_SYMBOL:
			return (localecnv->currency_symbol);
			break;
		case MON_DECIMAL_POINT:
			return (localecnv->mon_decimal_point);
			break;
		case MON_THOUSANDS_SEP:
			return (localecnv->mon_thousands_sep);
			break;
		case MON_GROUPING:
			return (localecnv->mon_grouping);
			break;
		case POSITIVE_SIGN:
			return (localecnv->positive_sign);
			break;
		case NEGATIVE_SIGN:
			return (localecnv->negative_sign);
			break;
		case INT_FRAC_DIGITS:
			return (&localecnv->int_frac_digits);
			break;
		case FRAC_DIGITS:
			return (&localecnv->frac_digits);
			break;
		case P_CS_PRECEDES:
			return (&localecnv->p_cs_precedes);
			break;
		case P_SEP_BY_SPACE:
			return (&localecnv->p_sep_by_space);
			break;
		case N_CS_PRECEDES:
			return (&localecnv->n_cs_precedes);
			break;
		case N_SEP_BY_SPACE:
			return (&localecnv->n_sep_by_space);
			break;
		case P_SIGN_POSN:
			return (&localecnv->p_sign_posn);
			break;
		case N_SIGN_POSN:
			return (&localecnv->n_sign_posn);
			break;
		default: break;
	   }
	} else {
		return(nl_langinfo(nl_item));
	}
}
