/* Copyright (C) 1979-1998 TcX AB & Monty Program KB & Detron HB

   This software is distributed with NO WARRANTY OF ANY KIND.  No author or
   distributor accepts any responsibility for the consequences of using it, or
   for whether it serves any particular purpose or works at all, unless he or
   she says so in writing.  Refer to the Free Public License (the "License")
   for full details.

   Every copy of this file must include a copy of the License, normally in a
   plain ASCII text file named PUBLIC.	The License grants you the right to
   copy, modify and redistribute this file, but only under certain conditions
   described in the License.  Among other things, the License requires that
   the copyright notice and this notice be preserved on all copies. */

#define NO_YACC_SYMBOLS
#include <global.h>
#include <my_sys.h>
#include <m_string.h>
#ifndef __GNU_LIBRARY__
#define __GNU_LIBRARY__				// Skipp warnings in getopt.h
#endif
#include <getopt.h>
#include "mysql_version.h"
#include "lex.h"

bool opt_search=0,opt_verbose=0;

#define horror	   7000		// Don't generate bigger arrays than this
#define max_symbol 32767	// Use this for 'not found'
#define start_value 2003

uint how_much_for_mod=horror;
#define how_much_and INT_MAX24

static uint how_long_symbols,function_plus,function_mod;
static uint char_table[256];
static uchar bits[how_much_and/8+1];

struct rand_struct {
  unsigned long seed1,seed2,max_value;
  double max_value_dbl;
};

void randominit(struct rand_struct *rand,ulong seed1, ulong seed2)
{						/* For mysql 3.21.# */
  rand->max_value= 0x3FFFFFFFL;
  rand->max_value_dbl=(double) rand->max_value;
  rand->seed1=seed1%rand->max_value ;
  rand->seed2=seed2%rand->max_value;
}

double rnd(struct rand_struct *rand)
{
  rand->seed1=(rand->seed1*3+rand->seed2) % rand->max_value;
  rand->seed2=(rand->seed1+rand->seed2+33) % rand->max_value;
  return (((double) rand->seed1)/rand->max_value_dbl);
}


static void make_char_table(ulong t1,ulong t2,int type)
{
  uint i;
  struct rand_struct rand;
  randominit(&rand,t1,t2);

  for (i=0 ; i < 256 ; i++)
  {
    switch (type) {
    case 0: char_table[i]= i + (i << 8); 		break;
    case 1: char_table[i]= i + ((i ^255 ) << 8);	break;
    case 2: char_table[i]= i;				break;
    case 3: char_table[i]= i + ((uint) (rnd(&rand)*255) << 8); break;
    }
  }
  char_table[0]|=1+257;				// Avoid problems with 0
  for (i=0 ; i < 256 ; i++)
  {
    uint tmp=(uint) (rnd(&rand)*255);
    swap(uint,char_table[i],char_table[tmp]);
  }
  /* lower characters should be mapped to upper */
  for (i='a' ; i <= 'z' ; i++)
  {
    char_table[i]=char_table[(uchar) (i- 'a' + 'A')];	// Assume ascii
  }
}


#ifndef QQ
#define how_much_for_plus  8
#define USE_char_table

static ulong tab_index_function (const char *s,uint add)
{
  register ulong nr=start_value;		// Nice value
  for ( ; *s ; s++)
  {
    nr= (nr ^ (char_table[(uchar) *s] + (nr << add)));
  }
  return nr & INT_MAX24;
}

#else

#define how_much_for_plus  255
#define USE_char_table

static ulong tab_index_function(const char *s,uint add)
{
  ulong tab = 0;
  uint i;
  for (i=0;s[i];i++)
    tab = ((((i+add)*tab+1) * char_table[s[i]])) & how_much_and;
  return tab;
}
#endif

static int funkcije (bool write_warning) 
{
  uint size_symbols = sizeof(symbols)/sizeof(SYMBOL);
  uint size_functions = sizeof(sql_functions)/sizeof(SYMBOL);
  uint size=size_symbols + size_functions;
  uint i=0,found;
  ulong order;
  int igra[horror],test_count=INT_MAX;
  uint possible_plus[how_much_for_plus];

  how_long_symbols = sizeof(symbols)/sizeof(SYMBOL);

  bzero((char*) possible_plus,sizeof(possible_plus));
  found=0;

  /* Check first which function_plus are possible */
  for (function_plus = 1; function_plus <= how_much_for_plus; function_plus++)
  {
    bzero((char*) bits,sizeof(bits));    
    for (i=0;i<size;i++)
    {
      ulong order= tab_index_function ((i < how_long_symbols) ?
				       symbols[i].name :
				       sql_functions[i-how_long_symbols].name,
				       function_plus);
      uint pos=order/8;
      uint bit=order & 7;
      if (bits[pos] & (1 << bit))
	break;
      bits[pos]|=1 << bit;
    }
    if (i == size)
    {
      possible_plus[found++]=function_plus;
    }
  }
  if (!found)
  {
    fprintf(stderr,"\
The hash function didn't return a unique value for any parameter\n\
You have to change gen_lex_code.cc, function 'tab_index_function' to\n\
generate unique values for some parameter.  When you have succeeded in this,\n\
you have to change 'main' to print out the new function\n");
    return(1);
  }

  if (opt_verbose)
    fprintf (stderr,"Info: Possible add values: %d\n",found);

  for (function_mod = (size & ~1)+1 ;
       function_mod < how_much_for_mod;
       function_mod+=2)
  {
    if (opt_verbose && (function_mod % 100) == 1)
      fprintf (stderr,"Info: Function mod = %d\n",function_mod);
    for (uint j=0 ; j < found ; j++)
    {
      function_plus=possible_plus[j];
      if (test_count++ == INT_MAX)
      {
	test_count=1;
	bzero((char*) igra,sizeof(igra));
      }
      for (i=0;i<size;i++)
      {
	order= tab_index_function ((i < how_long_symbols) ? symbols[i].name
				   : sql_functions[i - how_long_symbols].name,
				   function_plus);
	order %= function_mod;
	if (igra[order] == test_count)
	  break;
	igra[order] = test_count;
      }
      if (i == size)
	goto end;
    }
  }
 end:
  if (function_mod > horror)
  {
    if (write_warning)
      fprintf (stderr,"Fatal error when generating hash for symbols\n\
Didn't find suitable values for perfect hashing:\n\
You have to edit gen_lex_hase.cc to generate a new hashing function.\n\
You can try running gen_lex_hash with --search to find a suitable value\n\
Symbol array size = %d\n",function_mod);
    return -1;
  }
  return 0;
}


void print_arrays()
{
  uint size_symbols = sizeof(symbols)/sizeof(SYMBOL);
  uint size_functions = sizeof(sql_functions)/sizeof(SYMBOL);
  uint size=size_symbols + size_functions;
  uint i;

  fprintf(stderr,"Symbols: %d  Functions: %d;  Total: %d\nShifts per char: %d,  Array size: %d\n",
	  size_symbols,size_functions,size_symbols+size_functions,
	  function_plus,function_mod);

  int *prva= (int*) my_alloca(sizeof(int)*function_mod);
  for (i=0 ; i <= function_mod; i++)
    prva[i]= max_symbol;

  for (i=0;i<size;i++)
  {
    ulong order = tab_index_function ((i < how_long_symbols) ? symbols[i].name : sql_functions[i - how_long_symbols].name,function_plus);
    order %= function_mod;
    prva [order] = i;
  }

#ifdef USE_char_table
  printf("static uint16 char_table[] = {\n");
  for (i=0; i < 255 ;i++)			// < 255 is correct
  {
    printf("%u,",char_table[i]);
    if (((i+1) & 15) == 0)
      puts("");
  }
  printf("%d\n};\n\n\n",char_table[i]);
#endif

  printf("static uint16 my_function_table[] = {\n");
  for (i=0; i < function_mod-1 ;i++)
  {
    printf("%d,",prva[i]);
    if (((i+1) % 12) == 0)
      puts("");
  }
  printf("%d\n};\n\n\n",prva[i]);
  my_afree((gptr) prva);
}


static struct option long_options[] =
{
  {"search",	    no_argument,	   0, 'S'},
  {"verbose",	    no_argument,	   0, 'v'},
  {"version",	    no_argument,	   0, 'V'},
  {"rnd1",	    required_argument,	   0, 'r'},
  {"rnd2",	    required_argument,	   0, 'R'},
  {"type",	    required_argument,	   0, 't'},
  {0, 0, 0, 0}
};


static void usage(int version)
{
  printf("%s  Ver 1.0 Distrib %s, for %s (%s)\n",
	 my_progname, MYSQL_SERVER_VERSION, SYSTEM_TYPE, MACHINE_TYPE);
  if (version)
    return;
  puts("By TCX Datakonsult AB, by Monty");
  puts("All rights reserved. Se the file PUBLIC for licence information.");
  puts("This software comes with ABSOLUTELY NO WARRANTY: see the file PUBLIC for details.\n");
  puts("This program generates a perfect hashing function for the sql_lex.cc");
  printf("Usage: %s [OPTIONS]\n", my_progname);
  printf("\n\
-r, --rnd1=#		Set 1 part of rnd value for hash generator\n\
-R, --rnd2=#		Set 2 part of rnd value for hash generator\n\
-t, --type=#		Set type of char table to generate\n\
-S, --search		Search after good rnd1 and rnd2 values\n\
-v, --verbose		Write some information while the program executes\n\
-V, --version		Output version information and exit\n");

}

static uint best_type;
static ulong best_t1,best_t2;

static int get_options(int argc, char **argv)
{
  int c,option_index=0;

  while ((c=getopt_long(argc,argv,"?SvVr:R:t:",
			long_options, &option_index)) != EOF)
  {
    switch(c) {
    case 'r':
      best_t1=atol(optarg);
      break;
    case 'R':
      best_t2=atol(optarg);
      break;
    case 't':
      best_type=atoi(optarg);
      break;
    case 'S':
      opt_search=1;
      break;
    case 'v':
      opt_verbose=1;
      break;
    case 'V': usage(1); exit(0);
    case 'I':
    case '?':
      usage(0);
      exit(0);
    default:
      fprintf(stderr,"illegal option: -%c\n",opterr);
      usage(0);
      exit(1);
    }
  }
  argc-=optind;
  argv+=optind;
  if (argc >= 1)
  {
    usage(0);
    exit(1);
  }
  return(0);
}



int main(int argc,char **argv)
{
  struct rand_struct rand;
  static uint best_mod,best_add;
  int error;
  MY_INIT(argv[0]);
 
 best_t1=437541L ; best_t2=4144328L; best_type=1; // 2515 found with --search
 if (get_options(argc,(char **) argv))
   exit(1);

  make_char_table(best_t1,best_t2,best_type);

  if ((error=funkcije(1)) > 0 || error && !opt_search)
    exit(1);					// This should work
  best_mod=function_mod; best_add=function_plus;
  how_much_for_mod=best_mod-1;

  if (opt_search)
  {
    time_t start_time=time((time_t*) 0);
    randominit(&rand,start_time,start_time/2);		// Some random values

    for (uint i=0 ; i < 100000 ; i++)
    {
      ulong t1=(ulong) (rnd(&rand)*INT_MAX24);
      ulong t2=(ulong) (rnd(&rand)*INT_MAX24);
      uint type=(int) (rnd(&rand)*4);
      make_char_table(t1,t2,type);
      if (!funkcije(0))
      {
	if (function_mod < best_mod)
	{
	  best_mod=function_mod; best_add=function_plus;
	  best_t1=t1; best_t2=t2; best_type=type;
	  how_much_for_mod=best_mod-1;
	}
	printf("mod: %d  add: %d  t1: %ld  t2: %ld  type: %d\n",
	       best_mod,best_add,best_t1,best_t2,best_type);
      }
    }
  }

  function_mod=best_mod; function_plus=best_add;
  make_char_table(best_t1,best_t2,best_type);

  printf("/* This code is generated by program for seeking hash algorithms, copyright TcX Datakonsult AB */\n\n");
  printf("#include \"lex.h\"\n\n\n");

  print_arrays();

  printf("inline SYMBOL *get_hash_symbol(const char *s,unsigned int length,bool function)\n\
{\n\
  uint i; ulong index = %lu;\n\
  SYMBOL *sim;\n\
  const char *start=s;\n\
  for (i=length;i--;)\n\
    index= (index ^ (char_table[(uchar) *s++] + (index << %d)));\n\
  index=my_function_table[(index & %d) %% %d];\n\
  if (index >= %d)\n\
  {\n\
    if (!function || index >= %d) return (SYMBOL*) 0;\n\
    sim=sql_functions + (index - %d);\n\
  }\n\
  else\n\
    sim=symbols + index;\n\
  if ((length != sim->length) || my_casecmp(start,sim->name,length))\n\
    return  (SYMBOL *)0;\n\
  return sim;\n\
}\n",start_value,function_plus,how_much_and,function_mod,how_long_symbols,max_symbol,how_long_symbols);

}
