/*
  Author : David
  strintstr(src, from, pat) looks for an instance of pat in src
  backwards from pos from.  pat is not a regex(3) pattern, it is a literal
  string which must be matched exactly.
  The result 0 if the pattern was not found else it is the start char of
  the pattern counted from the begining of the string.
*/

#include <global.h>
#include "m_string.h"

uint r_strinstr(reg1 my_string str,int from, reg4 my_string search)
{
  reg2 my_string	i, j;
  int		len = strlen(search);
  /* pointer to the last char of buff */
  my_string	start = str + from - 1;
  /* pointer to the last char of search */
  my_string	search_end = search + len - 1;

 skipp:
  while (start >= str)		/* Cant be != because the first char */
  {
    if (*start-- == *search_end)
    {
      i = start; j = search_end - 1;
      while (j >= search && start > str)
	if (*i-- != *j--)
	  goto skipp;
      return (uint) ((start - len) - str + 3);
    }
  }
  return (0);
}
