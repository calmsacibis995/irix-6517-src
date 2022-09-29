/*
 * unzip module
 *
 * This code was created by extensively shrinking down GNU gzip
 * version 1.2.4 by removing all non-zip-decompression code.
 *
 * The unzip code was written and put in the public domain by Mark Adler.
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License, see the file COPYING.
 */

#include <stdio.h>
#include "unzip.h"

typedef void *voidp;
typedef unsigned char  uch;
typedef unsigned short ush;
typedef unsigned int   uint;

#define	GZIP_MAGIC     "\037\213"   /* Magic header for gzip files, 1F 8B */
#define DEFLATED	8	    /* Compression method */

#define WSIZE		0x8000	    /* window size--must be a power of two */

#define ASCII_FLAG   0x01 /* bit 0 set: file probably ascii text */
#define CONTINUATION 0x02 /* bit 1 set: continuation of multi-part gzip file */
#define EXTRA_FIELD  0x04 /* bit 2 set: extra field present */
#define ORIG_NAME    0x08 /* bit 3 set: original file name present */
#define COMMENT	     0x10 /* bit 4 set: file comment present */
#define ENCRYPTED    0x20 /* bit 5 set: file is encrypted */
#define RESERVED     0xC0 /* bit 6,7:   reserved */

static uch *window;
static unsigned outcnt;
static uch *mem_ptr;
static uch *mem_limit;
static int (*getc_fn)(void);
static void (*putc_fn)(int c);
static unsigned bytes_out;

#define GETC()	(*getc_fn)()
#define PUTC(c)	(*putc_fn)(c)

/* Macros for getting two-byte and four-byte header values */
#define SH(p) ((ush)(uch)((p)[0]) | ((ush)(uch)((p)[1]) << 8))
#define LG(p) ((uint)(SH(p)) | ((uint)(SH((p)+2)) << 16))

static uint crc_32_tab[] = {
  0x00000000U, 0x77073096U, 0xee0e612cU, 0x990951baU, 0x076dc419U,
  0x706af48fU, 0xe963a535U, 0x9e6495a3U, 0x0edb8832U, 0x79dcb8a4U,
  0xe0d5e91eU, 0x97d2d988U, 0x09b64c2bU, 0x7eb17cbdU, 0xe7b82d07U,
  0x90bf1d91U, 0x1db71064U, 0x6ab020f2U, 0xf3b97148U, 0x84be41deU,
  0x1adad47dU, 0x6ddde4ebU, 0xf4d4b551U, 0x83d385c7U, 0x136c9856U,
  0x646ba8c0U, 0xfd62f97aU, 0x8a65c9ecU, 0x14015c4fU, 0x63066cd9U,
  0xfa0f3d63U, 0x8d080df5U, 0x3b6e20c8U, 0x4c69105eU, 0xd56041e4U,
  0xa2677172U, 0x3c03e4d1U, 0x4b04d447U, 0xd20d85fdU, 0xa50ab56bU,
  0x35b5a8faU, 0x42b2986cU, 0xdbbbc9d6U, 0xacbcf940U, 0x32d86ce3U,
  0x45df5c75U, 0xdcd60dcfU, 0xabd13d59U, 0x26d930acU, 0x51de003aU,
  0xc8d75180U, 0xbfd06116U, 0x21b4f4b5U, 0x56b3c423U, 0xcfba9599U,
  0xb8bda50fU, 0x2802b89eU, 0x5f058808U, 0xc60cd9b2U, 0xb10be924U,
  0x2f6f7c87U, 0x58684c11U, 0xc1611dabU, 0xb6662d3dU, 0x76dc4190U,
  0x01db7106U, 0x98d220bcU, 0xefd5102aU, 0x71b18589U, 0x06b6b51fU,
  0x9fbfe4a5U, 0xe8b8d433U, 0x7807c9a2U, 0x0f00f934U, 0x9609a88eU,
  0xe10e9818U, 0x7f6a0dbbU, 0x086d3d2dU, 0x91646c97U, 0xe6635c01U,
  0x6b6b51f4U, 0x1c6c6162U, 0x856530d8U, 0xf262004eU, 0x6c0695edU,
  0x1b01a57bU, 0x8208f4c1U, 0xf50fc457U, 0x65b0d9c6U, 0x12b7e950U,
  0x8bbeb8eaU, 0xfcb9887cU, 0x62dd1ddfU, 0x15da2d49U, 0x8cd37cf3U,
  0xfbd44c65U, 0x4db26158U, 0x3ab551ceU, 0xa3bc0074U, 0xd4bb30e2U,
  0x4adfa541U, 0x3dd895d7U, 0xa4d1c46dU, 0xd3d6f4fbU, 0x4369e96aU,
  0x346ed9fcU, 0xad678846U, 0xda60b8d0U, 0x44042d73U, 0x33031de5U,
  0xaa0a4c5fU, 0xdd0d7cc9U, 0x5005713cU, 0x270241aaU, 0xbe0b1010U,
  0xc90c2086U, 0x5768b525U, 0x206f85b3U, 0xb966d409U, 0xce61e49fU,
  0x5edef90eU, 0x29d9c998U, 0xb0d09822U, 0xc7d7a8b4U, 0x59b33d17U,
  0x2eb40d81U, 0xb7bd5c3bU, 0xc0ba6cadU, 0xedb88320U, 0x9abfb3b6U,
  0x03b6e20cU, 0x74b1d29aU, 0xead54739U, 0x9dd277afU, 0x04db2615U,
  0x73dc1683U, 0xe3630b12U, 0x94643b84U, 0x0d6d6a3eU, 0x7a6a5aa8U,
  0xe40ecf0bU, 0x9309ff9dU, 0x0a00ae27U, 0x7d079eb1U, 0xf00f9344U,
  0x8708a3d2U, 0x1e01f268U, 0x6906c2feU, 0xf762575dU, 0x806567cbU,
  0x196c3671U, 0x6e6b06e7U, 0xfed41b76U, 0x89d32be0U, 0x10da7a5aU,
  0x67dd4accU, 0xf9b9df6fU, 0x8ebeeff9U, 0x17b7be43U, 0x60b08ed5U,
  0xd6d6a3e8U, 0xa1d1937eU, 0x38d8c2c4U, 0x4fdff252U, 0xd1bb67f1U,
  0xa6bc5767U, 0x3fb506ddU, 0x48b2364bU, 0xd80d2bdaU, 0xaf0a1b4cU,
  0x36034af6U, 0x41047a60U, 0xdf60efc3U, 0xa867df55U, 0x316e8eefU,
  0x4669be79U, 0xcb61b38cU, 0xbc66831aU, 0x256fd2a0U, 0x5268e236U,
  0xcc0c7795U, 0xbb0b4703U, 0x220216b9U, 0x5505262fU, 0xc5ba3bbeU,
  0xb2bd0b28U, 0x2bb45a92U, 0x5cb36a04U, 0xc2d7ffa7U, 0xb5d0cf31U,
  0x2cd99e8bU, 0x5bdeae1dU, 0x9b64c2b0U, 0xec63f226U, 0x756aa39cU,
  0x026d930aU, 0x9c0906a9U, 0xeb0e363fU, 0x72076785U, 0x05005713U,
  0x95bf4a82U, 0xe2b87a14U, 0x7bb12baeU, 0x0cb61b38U, 0x92d28e9bU,
  0xe5d5be0dU, 0x7cdcefb7U, 0x0bdbdf21U, 0x86d3d2d4U, 0xf1d4e242U,
  0x68ddb3f8U, 0x1fda836eU, 0x81be16cdU, 0xf6b9265bU, 0x6fb077e1U,
  0x18b74777U, 0x88085ae6U, 0xff0f6a70U, 0x66063bcaU, 0x11010b5cU,
  0x8f659effU, 0xf862ae69U, 0x616bffd3U, 0x166ccf45U, 0xa00ae278U,
  0xd70dd2eeU, 0x4e048354U, 0x3903b3c2U, 0xa7672661U, 0xd06016f7U,
  0x4969474dU, 0x3e6e77dbU, 0xaed16a4aU, 0xd9d65adcU, 0x40df0b66U,
  0x37d83bf0U, 0xa9bcae53U, 0xdebb9ec5U, 0x47b2cf7fU, 0x30b5ffe9U,
  0xbdbdf21cU, 0xcabac28aU, 0x53b39330U, 0x24b4a3a6U, 0xbad03605U,
  0xcdd70693U, 0x54de5729U, 0x23d967bfU, 0xb3667a2eU, 0xc4614ab8U,
  0x5d681b02U, 0x2a6f2b94U, 0xb40bbe37U, 0xc30c8ea1U, 0x5a05df1bU,
  0x2d02ef8dU
};

static uint updcrc(uch *s, unsigned n)
{
    static uint crc = 0xffffffffU;
    uint c;

    if (s == (uch *)0) {
	c = 0xffffffffU;
    } else {
	c = crc;
	if (n) do {
	    c = crc_32_tab[(c ^ (*s++)) & 0xff] ^ (c >> 8);
	} while (--n);
    }
    crc = c;

    return c ^ 0xffffffffU;
}

static void flush_window(void)
{
    int		i;

    if (outcnt) {
	updcrc(window, outcnt);

	for (i = 0; i < outcnt; i++)
	    PUTC(window[i]);

	bytes_out += outcnt;

	outcnt = 0;
    }
}

struct huft {
  uch e;
  uch b;
  union {
    ush n;
    struct huft *t;
  } v;
};

#define flush_output(w) (outcnt=(w),flush_window())

static unsigned border[] = {
	16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
static ush cplens[] = {
	3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
	35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0, 0};

static ush cplext[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
	3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0, 99, 99};
static ush cpdist[] = {
	1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
	257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
	8193, 12289, 16385, 24577};
static ush cpdext[] = {
	0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
	7, 7, 8, 8, 9, 9, 10, 10, 11, 11,
	12, 12, 13, 13};

static uint bb;
static unsigned bk;

static ush mask_bits[] = {
    0x0000,
    0x0001, 0x0003, 0x0007, 0x000f, 0x001f, 0x003f, 0x007f, 0x00ff,
    0x01ff, 0x03ff, 0x07ff, 0x0fff, 0x1fff, 0x3fff, 0x7fff, 0xffff
};

#define NEEDBITS(n) {while(k<(n)){b|=((uint)GETC())<<k;k+=8;}}
#define DUMPBITS(n) {b>>=(n);k-=(n);}

#define LBITS	9
#define DBITS	6

#define BMAX 16
#define N_MAX 288

static int huft_build(unsigned *b, unsigned n, unsigned s,
		      ush *d, ush *e, struct huft **t, int *m)
{
  unsigned a;
  unsigned c[BMAX+1];
  unsigned f;
  int g;
  int h;
  register unsigned i;
  register unsigned j;
  register int k;
  int l;
  register unsigned *p;
  register struct huft *q;
  struct huft r;
  struct huft *u[BMAX];
  unsigned v[N_MAX];
  register int w;
  unsigned x[BMAX+1];
  unsigned *xp;
  int y;
  unsigned z;

  for (i = 0; i < BMAX + 1; i++)
    c[i] = 0;
  p = b;  i = n;
  do {
    c[*p]++;
    p++;
  } while (--i);
  if (c[0] == n)
  {
    *t = (struct huft *)0;
    *m = 0;
    return 0;
  }

  l = *m;
  for (j = 1; j <= BMAX; j++)
    if (c[j])
      break;
  k = j;
  if ((unsigned)l < j)
    l = j;
  for (i = BMAX; i; i--)
    if (c[i])
      break;
  g = i;
  if ((unsigned)l > i)
    l = i;
  *m = l;

  for (y = 1 << j; j < i; j++, y <<= 1)
    if ((y -= c[j]) < 0)
      return UNZIP_ERROR_FORMAT;
  if ((y -= c[i]) < 0)
    return UNZIP_ERROR_FORMAT;
  c[i] += y;

  x[1] = j = 0;
  p = c + 1;  xp = x + 2;
  while (--i) {
    *xp++ = (j += *p++);
  }

  p = b;  i = 0;
  do {
    if ((j = *p++) != 0)
      v[x[j]++] = i;
  } while (++i < n);

  x[0] = i = 0;
  p = v;
  h = -1;
  w = -l;
  u[0] = (struct huft *)0;
  q = (struct huft *)0;
  z = 0;

  for (; k <= g; k++)
  {
    a = c[k];
    while (a--)
    {
      while (k > w + l)
      {
	h++;
	w += l;

	z = (z = g - w) > (unsigned)l ? l : z;
	if ((f = 1 << (j = k - w)) > a + 1)
	{
	  f -= a + 1;
	  xp = c + k;
	  while (++j < z)
	  {
	    if ((f <<= 1) <= *++xp)
	      break;
	    f -= *xp;
	  }
	}
	z = 1 << j;

	q = (struct huft *) mem_ptr;
	mem_ptr += (z + 1)*sizeof(struct huft);
	if (mem_ptr > mem_limit)
	    return UNZIP_ERROR_MEMORY;
	*t = q + 1;
	*(t = &(q->v.t)) = (struct huft *)0;
	u[h] = ++q;

	if (h)
	{
	  x[h] = i;
	  r.b = (uch)l;
	  r.e = (uch)(16 + j);
	  r.v.t = q;
	  j = i >> (w - l);
	  u[h-1][j] = r;
	}
      }

      r.b = (uch)(k - w);
      if (p >= v + n)
	r.e = 99;
      else if (*p < s)
      {
	r.e = (uch)(*p < 256 ? 16 : 15);
	r.v.n = (ush)(*p);
	p++;
      }
      else
      {
	r.e = (uch)e[*p - s];
	r.v.n = d[*p++ - s];
      }

      f = 1 << (k - w);
      for (j = i >> w; j < z; j += f)
	q[j] = r;

      for (j = 1 << (k - 1); i & j; j >>= 1)
	i ^= j;
      i ^= j;

      while ((i & ((1 << w) - 1)) != x[h])
      {
	h--;
	w -= l;
      }
    }
  }

  /* Return true (1) if we were given an incomplete table */
  return y != 0 && g != 1;
}

static int inflate_codes(struct huft *tl,
			 struct huft *td,
			 int bl, int bd)
{
  register unsigned e;
  unsigned n, d;
  unsigned w;
  struct huft *t;
  unsigned ml, md;
  register uint b;
  register unsigned k;

  b = bb;
  k = bk;
  w = outcnt;

  ml = mask_bits[bl];
  md = mask_bits[bd];
  for (;;)
  {
    NEEDBITS((unsigned)bl)
    if ((e = (t = tl + ((unsigned)b & ml))->e) > 16)
      do {
	if (e == 99)
	  return UNZIP_ERROR_FORMAT;
	DUMPBITS(t->b)
	e -= 16;
	NEEDBITS(e)
      } while ((e = (t = t->v.t + ((unsigned)b & mask_bits[e]))->e) > 16);
    DUMPBITS(t->b)
    if (e == 16)
    {
      window[w++] = (uch)t->v.n;
      if (w == WSIZE)
      {
	flush_output(w);
	w = 0;
      }
    }
    else
    {
      if (e == 15)
	break;

      NEEDBITS(e)
      n = t->v.n + ((unsigned)b & mask_bits[e]);
      DUMPBITS(e);

      NEEDBITS((unsigned)bd)
      if ((e = (t = td + ((unsigned)b & md))->e) > 16)
	do {
	  if (e == 99)
	    return UNZIP_ERROR_FORMAT;
	  DUMPBITS(t->b)
	  e -= 16;
	  NEEDBITS(e)
	} while ((e = (t = t->v.t + ((unsigned)b & mask_bits[e]))->e) > 16);
      DUMPBITS(t->b)
      NEEDBITS(e)
      d = w - t->v.n - ((unsigned)b & mask_bits[e]);
      DUMPBITS(e)

      do {
	n -= (e = (e = WSIZE - ((d &= WSIZE-1) > w ? d : w)) > n ? n : e);
	do {
	  window[w++] = window[d++];
	} while (--e);
	if (w == WSIZE)
	{
	  flush_output(w);
	  w = 0;
	}
      } while (n);
    }
  }

  outcnt = w;
  bb = b;
  bk = k;

  return 0;
}

static int inflate_stored(void)
{
  unsigned n;
  unsigned w;
  register uint b;
  register unsigned k;

  b = bb;
  k = bk;
  w = outcnt;

  n = k & 7;
  DUMPBITS(n);

  NEEDBITS(16)
  n = ((unsigned)b & 0xffff);
  DUMPBITS(16)
  NEEDBITS(16)
  if (n != (unsigned)((~b) & 0xffff))
    return UNZIP_ERROR_FORMAT;
  DUMPBITS(16)

  while (n--)
  {
    NEEDBITS(8)
    window[w++] = (uch)b;
    if (w == WSIZE)
    {
      flush_output(w);
      w = 0;
    }
    DUMPBITS(8)
  }

  outcnt = w;
  bb = b;
  bk = k;
  return 0;
}

static int inflate_fixed(void)
{
  int i;
  struct huft *tl;
  struct huft *td;
  int bl;
  int bd;
  unsigned l[288];

  for (i = 0; i < 144; i++)
    l[i] = 8;
  for (; i < 256; i++)
    l[i] = 9;
  for (; i < 280; i++)
    l[i] = 7;
  for (; i < 288; i++)
    l[i] = 8;
  bl = 7;
  if ((i = huft_build(l, 288, 257, cplens, cplext, &tl, &bl)) != 0)
    return (i < 0) ? i : UNZIP_ERROR_FORMAT;

  for (i = 0; i < 30; i++)
    l[i] = 5;
  bd = 5;
  if ((i = huft_build(l, 30, 0, cpdist, cpdext, &td, &bd)) < 0)
    return i;

  return inflate_codes(tl, td, bl, bd);
}

static int inflate_dynamic(void)
{
  int i;
  unsigned j;
  unsigned l;
  unsigned m;
  unsigned n;
  struct huft *tl;
  struct huft *td;
  int bl;
  int bd;
  unsigned nb;
  unsigned nl;
  unsigned nd;
  unsigned ll[286+30];
  register uint b;
  register unsigned k;

  b = bb;
  k = bk;

  NEEDBITS(5)
  nl = 257 + ((unsigned)b & 0x1f);
  DUMPBITS(5)
  NEEDBITS(5)
  nd = 1 + ((unsigned)b & 0x1f);
  DUMPBITS(5)
  NEEDBITS(4)
  nb = 4 + ((unsigned)b & 0xf);
  DUMPBITS(4)
  if (nl > 286 || nd > 30)
    return UNZIP_ERROR_FORMAT;

  for (j = 0; j < nb; j++)
  {
    NEEDBITS(3)
    ll[border[j]] = (unsigned)b & 7;
    DUMPBITS(3)
  }
  for (; j < 19; j++)
    ll[border[j]] = 0;

  bl = 7;
  if ((i = huft_build(ll, 19, 19, (ush *)0, (ush *)0, &tl, &bl)) != 0)
    return (i < 0) ? i : UNZIP_ERROR_FORMAT;

  n = nl + nd;
  m = mask_bits[bl];
  i = l = 0;
  while ((unsigned)i < n)
  {
    NEEDBITS((unsigned)bl)
    j = (td = tl + ((unsigned)b & m))->b;
    DUMPBITS(j)
    j = td->v.n;
    if (j < 16)
      ll[i++] = l = j;
    else if (j == 16)
    {
      NEEDBITS(2)
      j = 3 + ((unsigned)b & 3);
      DUMPBITS(2)
      if ((unsigned)i + j > n)
	return UNZIP_ERROR_FORMAT;
      while (j--)
	ll[i++] = l;
    }
    else if (j == 17)
    {
      NEEDBITS(3)
      j = 3 + ((unsigned)b & 7);
      DUMPBITS(3)
      if ((unsigned)i + j > n)
	return UNZIP_ERROR_FORMAT;
      while (j--)
	ll[i++] = 0;
      l = 0;
    }
    else
    {
      NEEDBITS(7)
      j = 11 + ((unsigned)b & 0x7f);
      DUMPBITS(7)
      if ((unsigned)i + j > n)
	return UNZIP_ERROR_FORMAT;
      while (j--)
	ll[i++] = 0;
      l = 0;
    }
  }

  bb = b;
  bk = k;

  bl = LBITS;
  if ((i = huft_build(ll, nl, 257, cplens, cplext, &tl, &bl)) != 0)
    return (i < 0) ? i : UNZIP_ERROR_FORMAT;

  bd = DBITS;
  if ((i = huft_build(ll + nl, nd, 0, cpdist, cpdext, &td, &bd)) != 0)
    return (i < 0) ? i : UNZIP_ERROR_FORMAT;

  return inflate_codes(tl, td, bl, bd);
}

static int inflate_block(int *e)
{
  unsigned t;
  uint b;
  unsigned k;
  int r;
  uch *save_ptr;

  b = bb;
  k = bk;

  NEEDBITS(1)
  *e = (int)b & 1;
  DUMPBITS(1)

  NEEDBITS(2)
  t = (unsigned)b & 3;
  DUMPBITS(2)

  bb = b;
  bk = k;

  save_ptr = mem_ptr;

  switch (t) {
  case 2:
      r = inflate_dynamic();
      break;
  case 0:
      r = inflate_stored();
      break;
  case 1:
      r = inflate_fixed();
      break;
  default:
      r = UNZIP_ERROR_FORMAT;
      break;
  }

  mem_ptr = save_ptr;	/* Frees all hufts */

  return r;
}

static int inflate(void)
{
  int e;
  int r;

  outcnt = 0;
  bk = 0;
  bb = 0;

  do {
    if ((r = inflate_block(&e)) != 0)
      return r;
  } while (!e);

  flush_output(outcnt);

  return 0;
}

int unzip(int (*my_getc)(void),
	  void (*my_putc)(int c),
	  char *tmp, int tmpsize)
{
    uch flags;
    char magic[2];
    uint stamp;
    uint orig_crc, crc;
    uint orig_len;
    int n;
    uch buf[8];
    int res;
    int method;
    uint b;	 /* bit buffer */
    unsigned k;	 /* number of bits in bit buffer */

    mem_ptr = (uch *) tmp;
    mem_limit = (uch *) tmp + tmpsize;

    window = (uch *) mem_ptr;	/* Allocate window buffer */
    mem_ptr += 2 * WSIZE;
    if (mem_ptr > mem_limit)
	return UNZIP_ERROR_MEMORY;

    getc_fn = my_getc;
    putc_fn = my_putc;

    bytes_out = 0;
    outcnt = 0;

    magic[0] = (char)GETC();
    magic[1] = (char)GETC();

    if (magic[0] != GZIP_MAGIC[0] || magic[1] != GZIP_MAGIC[1])
	return UNZIP_ERROR_MAGIC;

    method = (int)GETC();
    flags  = (uch)GETC();

    if (method != DEFLATED && flags != 0)
	return UNZIP_ERROR_FORMAT;

    stamp  = (uint)GETC();	/* Time stamp */
    stamp |= ((uint)GETC()) << 8;
    stamp |= ((uint)GETC()) << 16;
    stamp |= ((uint)GETC()) << 24;

    (void)GETC();  /* Ignore extra flags */
    (void)GETC();  /* Ignore OS type */

    if ((flags & CONTINUATION) != 0) {
	(void) GETC();
	(void) GETC();
    }

    if ((flags & EXTRA_FIELD) != 0) {
	unsigned len = (unsigned) GETC();
	len |= ((unsigned) GETC())<<8;
	while (len--)
	    (void) GETC();
    }

    if ((flags & ORIG_NAME) != 0)
	while (GETC() != 0)
	    ;

    if ((flags & COMMENT) != 0)
	while (GETC() != 0)
	    ;

    updcrc((uch *)0, 0);

    if ((res = inflate()) != 0)
	return res;

    /*
     * We may have fetched more bytes than we needed.  Since we don't have
     * an ungetc, they'll be retrieved directly from the bit buffer after
     * discarding the unused portion of the last byte.
     */

    b = bb;
    k = bk;

    n = k & 7;
    DUMPBITS(n);

    for (n = 0; n < 8; n++)
	if (k >= 8) {
	    buf[n] = (uch)(b & mask_bits[8]);
	    DUMPBITS(8);
	} else
	    buf[n] = (uch)GETC();

    orig_crc = LG(buf);
    orig_len = LG(buf+4);

    crc = updcrc(buf, 0);

    if (orig_crc != crc)
	return UNZIP_ERROR_CRC;

    if (orig_len != bytes_out)
	return UNZIP_ERROR_LENGTH;

    return (int) orig_len;
}

char *unzip_errmsg(int errcode)
{
    switch (errcode) {
    case UNZIP_ERROR_NONE:
	return "No error";
    case UNZIP_ERROR_MAGIC:
	return "Invalid magic number";
    case UNZIP_ERROR_FORMAT:
	return "Bad data format";
    case UNZIP_ERROR_MEMORY:
	return "Out of memory";
    case UNZIP_ERROR_CRC:
	return "Data CRC mismatch";
    case UNZIP_ERROR_LENGTH:
	return "Data length mismatch";
    default:
	return "Unknown error";
    }
}

#ifdef TEST_MAIN
#include <stdio.h>
#include <stdlib.h>

int my_getc(void) { return getchar(); }
void my_putc(int c) { putchar(c); }

#define TMPSIZE		0x30000

void main()
{
    int r;
    char *tmp = malloc(TMPSIZE);

    if ((r = unzip(my_getc, my_putc, tmp, TMPSIZE)) < 0) {
	fprintf(stderr, "%d\n", r);
	exit(1);
    }

    exit(0);
}
#endif /* TEST_MAIN */
