
/* unixperf - an x11perf-style Unix performance benchmark */

/* test_c4.c solves Connect 4 game boards with an alpha/beta search and a space efficient hash table. */

/* This test requires about 5 megabytes of working memory so the
   benchmark should be cache limited on 2 megabyte or smaller secondary
   caches and doesn't even completely fit in a 4 megabyte cache. */

/* See README.c4 for details. */

#include "unixperf.h"
#include "test_c4.h"

#include "c4.h"

static char moves[] = "444333377755";

unsigned int
DoConnect4(void)
{
  static beenhere = 0;
  int result, movenr, i;

  if (!beenhere) {
    initbest();
    inittrans();
    initplay();
    beenhere = 1;
  }
  movenr = 0;
  newgame();
  for (i = 0; moves[i] != '\0'; i++) {
    if (++movenr & 1)
      makemovo(moves[i] - '0');
    else
      makemovx(moves[i] - '0');
  }
  window = 0;
  result = best();
  if (((result & 7) != 7) || ((result >> 3) != 21)) {
    TestProblemOccured = 1;
  }
  cleantrans();
  return nodes;
}
