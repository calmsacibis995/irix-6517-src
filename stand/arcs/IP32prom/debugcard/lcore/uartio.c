/*
 * -------------------------------------------------------------
 *  $Revision: 1.1 $
 *  $Date: 1997/08/18 20:42:21 $
 * -------------------------------------------------------------
 *  a simple printf function,
 */

#include <stdarg.h>
#include <inttypes.h>
#include <st16c1451.h>

#define Bhex        4
#define Shex        12
#define Ihex        28
#define Lhex        60
#define Boct        3
#define Soct        15
#define Ioct        30
#define Loct        63

#define CTRL(x)     ((x)&0x1f)
#define DEL         0x7f
#define INTR        CTRL('C')
#define BELL        0x7
#define LINESIZE    128
#define PROMPT      "? "
#define SCALABLE    'I'

char digit[] = "0123456789abcdef" ;

/*
 * Function: gethdec()
 *  Get all the pending chars and xtranlate it
 *  into a number (ibase=16, obase=10), anything
 *  greater the int size will be transcated, i.e.
 *  only the last 8 characters will be honored.
 */
#ifdef  UNIXSIM
#define GETC    getchar
#else
#define GETC    getc
#endif

/*
 * Function: Getting a decimal number from tty
 */
uint64_t
getdec ()
{
    register char        datain ;
    register int         ccount ;
    register int64_t     value  ;

    datain = '!' ;
    ccount = value = 0x0 ;

    while (((datain = (char) GETC ()) != '\012') && (datain != '\015')) {
      if ((datain == '\010') && (ccount > 0)) {
          ccount -= 1 ;           /* decrement the char count.    */
          value = value / 10 ;    /* remove previous char.        */
	  putchar ('\010');
          putchar (' ');
          putchar ('\010');
        } else {
          putchar (datain) ;      /* Echo what we got.            */
          if ((datain >= '0') && (datain <= '9')) {
            ccount += 1 ;
            value = (value * 10) + (datain - '0') ;
          } else {
            /* we simply ignore the invalid input characters. */
            /* or we can terminate the get process ?????      */
            return -1 ;
          }
        }
    }

    if (ccount == 0x0) value = -1 ; /* no valid input character. */
    return ((uint64_t)value) ;
}

/*
 * Function: Getting a hex number from tty.
 */
uint64_t
gethex ()
{
    register char        datain ;
    register int         ccount ;
    register int64_t     value  ;

    datain = '!' ;
    ccount = value = 0x0 ;

    while (((datain = (char) GETC ()) != '\012') && (datain != '\015')) {
        if ((datain == '\010') && (ccount > 0)) {
            ccount -= 1 ;           /* decrement the char count.    */
            value = value >> 4 ;    /* remove previous char.        */
            putchar ('\010');
            putchar (' ');
            putchar ('\010');
        } else {
          putchar (datain) ;        /* Echo what we got.            */ 
            if ((datain >= '0') && (datain <= '9')) {
                ccount += 1 ;
                value = (value << 4) + (datain - '0') ;
            } else if ((datain >= 'a') && (datain <= 'f')) {
                ccount += 1 ;
                value = (value << 4) + (datain - 'a' + 10) ;
            } else if ((datain >= 'A') && (datain <= 'F')) {
                ccount += 1 ;
                value = (value << 4) + (datain - 'A' + 10) ;
            } /* we simply ignore the invalid input characters. */
              /* or we can terminate the get process ?????      */
        }
    }

    if (ccount == 0x0) value = -1 ; /* no valid input character. */
    return ((uint64_t)value) ;
}


/*
 * Function: printudec(uint64_t val, int position)
 */
void
printudec (uint64_t val, int ovr, int size)
{
    char                dc[22], c ;
    register int        ptr, indx, indx1 ;
    register uint64_t	v, rem  ;

    if ((val == 0) && (ovr == 0)) {
      putchar ('0') ;
      return ;
    }

    for (rem = 0 ; rem < 22 ; dc[rem++] = '0') ;
    ptr = 0   ;
    v   = val ;
    while (v >= 10) {
        rem  = v/10 ;
        indx = v - (rem * 10) ;
        dc[ptr] = digit[indx] ;
        v = rem ;
        if (indx != 0) indx1 = ptr ;
        ptr += 1 ;
    }
    dc[ptr] = digit[v] ;
    if (v != 0) indx1 = ptr ; /* the leading non zero digit. */
    if ((ovr != 0) && (size != 0x0)) ptr = size ;
    else ptr = indx1 ; /* don't print leading 0s'   */
    while (ptr >= 0) {
        putchar (dc[ptr]) ;
        ptr -= 1 ;
    }
}

/*
 * Function: printdec(uint64_t val, int position)
 */
void
printdec (int64_t val, int ovr, int size)
{
    char                dc[22], c ;
    register int        ptr, indx, indx1 ;
    register int64_t	v, rem  ;

    if ((val == 0) && (ovr == 0)) {
      putchar ('0') ;
      return ;
    }

    for (rem = 0 ; rem < 22 ; rem++) dc[rem] = '0' ;
    ptr = 0 ;
    if (val < 0) {
        c = '-'  ;
        v = -val ;
    } else {
        c = ' '  ;
        v = val  ;
    }
    while (v >= 10) {
        rem  = v/10 ;
        indx = v - (rem * 10) ;
        dc[ptr] = digit[indx] ;
        v = rem ;
        if (indx != 0) indx1 = ptr ;
        ptr += 1 ;
    }
    dc[ptr] = digit[v] ;
    if (v != 0) indx1 = ptr ; /* the leading non zero digit. */
    if ((ovr != 0) && (size != 0x0)) ptr = size ;
    else ptr = indx1 ; /* don't print leading 0s'   */
    if (c == '-') putchar (c) ;
    while (ptr >= 0) {
        putchar (dc[ptr]) ;
        ptr -= 1 ;
    }
}

/*
 * Function: printhex(uint64_t val, int position)
 */
void
printhex (uint64_t val, int ovr, int pos, char C) 
{
    register char c ;
    register int  pcount, nonzero ;

    pcount = pos ;
    nonzero   = 0   ;

    if ((val == 0) && (ovr == 0)) {
      putchar ('0') ;
      return ;
    }

    while (pcount >= 0) {
      if (pcount == 0) c = digit[val & 0xF] ;
      else c = digit[(val >> pcount) & 0xF] ;
      if ((C == 'X') && (c > '9')) c = c - 'a' + 'A' ;
      if (c == '0') {
        if ((nonzero > 0) || (ovr != 0)) {
          putchar (c);
        }
      } else {
        nonzero++;
        putchar (c);
      }
      pcount -= 4 ;
    }

    return ;
}

/*
 * Function: printoct(uint64_t val, int position)
 */
void
printoct (uint64_t val, int ovr, int pos)
{
    register char c ;
    register int  pcount, indx ;

    pcount = pos ;
    indx   = 0   ;

    if ((val == 0) && (ovr == 0)) {
      putchar ('0') ;
      return ;
    }
    
    while (pcount >= 0) {
      if (pcount == 0) c = digit[val & 0x7] ;
      else c = digit[(val >> pcount) & 0x7] ;
      if (c != '0') {
        indx += 1 ;
        putchar (c) ;
      } else {
        if ((ovr != 0) && (indx != 0)) putchar (c) ;
      }
      pcount -= 3 ; 
    }

    return ; 
}        
            
/*
 * Function: printbin(uint64_t val, int position)
 */
void
printbin (uint64_t val, int size)
{
    register char c ;
    register int  pcount ;

    pcount = size;
    while (pcount >= 0) {
        if (pcount == 0) c = digit[val & 0x1] ;
        else c = digit[(val >> pcount) & 0x1] ;
        putchar (c) ;
        pcount -= 1 ; 
    }
}        

/*
 * Function: char *cachefmt(char *), move the string to cache
 *           then return the address.
 */
void
cachefmt (uint64_t val, int size)
{
    register char c ;
    register int  pcount ;

    pcount = size;
    while (pcount >= 0) {
        if (pcount == 0) c = digit[val & 0x1] ;
        else c = digit[(val >> pcount) & 0x1] ;
        putchar (c) ;
        pcount -= 1 ; 
    }
}        

/*
 * This printf only suppot decimal (%d), octal (%o), hexdecimal (%x),
 * and string (%s), also binary (%b) and long long (64 bit %l).
 */
void
printf (char *fmt, ...)
{
    va_list         args                 ;
    char            *str, *p, errQ[3]    ;
    register int    state, i, size, errP ;
    register int    ovr                  ;
    
    va_start (args, fmt) ;
    state  = 0   ;
    ovr    = 0   ;
    p      = fmt ;
    errP   = 0   ;
    while (*p) {
        switch (state) {
        case 0:
            if (*p != '%') {
                if ((*p == '\r') || (*p == '\n')) {
                    putchar ('\r');
                    putchar ('\n');
                } else putchar (*p) ;
            } else {
                errQ[errP++] = '%' ;
                state = 1 ; 
            }
            p++   ;  /* point to next character, this also skip */
            break ;  /* the '%' character.                      */
        case 1: /* check the 0 pedding after '%' - '0' */
            state  = 2 ;    /* to the correct state.                      */
            if (*p == '0') {
                errQ[errP++] = '0' ; 
                ovr = 1 ;   /* set the 0 pedding flag.    */
                p++     ;   /* get the next character.    */
            } else ovr = 0 ;/* no 0 pedding.              */
            break ;
        case 2: /* check the size of the obj to be printed. */
            state  = 3  ;
            switch (*p) {
            case 'l':
                errQ[errP++] = 'l' ;
                size  = 64 ; /* set to 64 bit data size.    */
                p++        ; /* move to next character.     */       
                break      ;
            case 'h':
                errQ[errP++] = 'h' ;
                size  = 16 ; /* set to 16 bit data size.    */
                p++        ; /* point to next character.    */
                break      ;
            case 'b':
                errQ[errP++] = 'b' ;
                size  = 8  ; /* set to 8  bit data size.    */
                p++        ; /* point to next character.    */
                break      ;
            default:
                size  = 32 ; /* set to default data size.   */
                break      ;
            }
            break ;
        case 3: /* this is the heart of the printf.   */
            switch (*p) {
            case '%':
                putchar ('%') ;
                break ; 
            case 'd': /* object type is integer, base 10.   */
            case 'i': /* object type is integer, base 10.   */
                switch (size) {  /* get the right size of the object    */
                case 64:    /* long long */
                    printdec ((uint64_t)va_arg(args, int64_t), ovr, 18) ;
                    break ;
                case 16:    /* short     */
                    printdec ((uint64_t)va_arg(args, int16_t), ovr, 4) ;
                    break ;
                case 8:     /* byte      */
                    printdec ((uint64_t)va_arg(args, int8_t), ovr, 2) ;
                    break ;
                default:    /* int       */
                    printdec ((uint64_t)va_arg(args, int32_t), ovr, 9) ;
                    break ;
                }
                break ;
            case 'u': /* object type is unsigned integer, base 10.   */
                switch (size) {  /* get the right size of the object    */
                case 64:    /* long long */
                    printudec ((uint64_t)va_arg(args, uint64_t), ovr, 0) ;
                    break ;
                case 16:    /* short     */
                    printudec ((uint64_t)va_arg(args, uint16_t), ovr, 4) ;
                    break ;
                case 8:     /* byte   */
                    printudec ((uint64_t)va_arg(args, uint8_t), ovr, 3) ;
                    break ;
                default:    /* int       */
                    printudec ((uint64_t)va_arg(args, uint32_t), ovr, 9) ;
                    break ;
                }
                break ;
            case 'o': /* object type is unsigned integer, base 8.   */
                switch (size) {  /* get the right size of the object    */
                case 64:    /* long long */
                    printoct ((uint64_t)va_arg(args, uint64_t), ovr, Loct) ;
                    break ;
                case 16:    /* short     */
                    printoct ((uint64_t)va_arg(args, uint16_t), ovr, Soct) ;
                    break ;
                case 8:     /* byte      */
                    printoct ((uint64_t)va_arg(args, uint8_t), ovr, Boct) ;
                    break ;
                default:    /* int       */
                    printoct ((uint64_t)va_arg(args, uint32_t), ovr, Ioct) ;
                    break ;
                }
                break ;
            case 'x': /* object type is unsigned integer, base 16.   */
            case 'X': /* object type is unsigned integer, base 16.   */
                switch (size) {  /* get the right size of the object    */
                case 64:    /* long long */
                    printhex ((uint64_t)va_arg(args, uint64_t),ovr,Lhex,*p) ;
                    break ;
                case 16:    /* short     */
                    printhex ((uint64_t)va_arg(args, uint16_t),ovr,Shex,*p) ;
                    break ;
                case 8:     /* byte     */
                    printhex ((uint64_t)va_arg(args, uint8_t),ovr,Bhex,*p) ;
                    break ;
                default:    /* int       */
                    printhex ((uint64_t)va_arg(args, uint32_t),ovr,Ihex,*p) ;
                    break ;
                }
                break ;
            case 'c': /* object type is unsigned char.   */
                putchar ((char)va_arg(args, char)) ;
                break ;
            case 's': /* object type is a address point to a char. */
                str = (char *) va_arg(args, unsigned int) ;
                while (*str != '\0') {
                    if ((*str == '\r') || (*str == '\n')) {
                        putchar ('\r');
                        putchar ('\n');
                    } else putchar (*str) ;
                    str++;
                }
                break ;
            case '0': /* I consider this an error condition. */
            case 'l': /* I consider this an error condition. */
            case 'h': /* I consider this an error condition. */
            default:  /* This also indicate an error.        */ 
                for (i = 0 ; i < 3 ; i++) putchar (errQ[i]) ;
                putchar (*p++) ; /* move to next character. */
                break ;
            }
            state = 0 ; /* reset the state machine. */
            size  = 0 ; /* and reset the size.      */
            ovr   = 0 ; /* and 0 pedding flag.      */
            errP  = 0 ; /* reset the errQ.          */
            p++       ; /* point to next character. */
        }
    }
    return ;
}


#if defined(UARTIOworks)
/*
 * showchar -- print character in visible manner
 */
void
showchar(int c)
{
    c &= 0xff;
    if (isprint(c))
        putchar(c);
    else switch (c) {
    case '\b':
        puts("\\b");
        break;
    case '\f':
        puts("\\f");
        break;
    case '\n':
        puts("\\n");
        break;
    case '\r':
        puts("\\r");
        break;
    case '\t':
        puts("\\t");
        break;
    default:
        putchar('\\');
        putchar(((c&0300) >> 6) + '0');
        putchar(((c&070) >> 3) + '0');
        putchar((c&07) + '0');
        break;
    }
}

/*
 * like stdio fgets, but fd instead of FILE ptr, and has all
 * the echo'ing, etc. that is needed for standalone.
 */
char *
ttygets(char *buf, int len)
{
  int c;
  char *bufp, *rvalp=NULL;

  
  bufp = buf;
  for (;;) {
    switch (c = getc()) {
    case CTRL('V'):     /* quote next char */
      c = getc();
      /*
       * Make sure there's room for this character
       * plus a trailing \n and 0 byte
       */
      if (bufp < &buf[len-3]) {
        *bufp++ = (char)c;
        showchar(c);
      } else
        putchar(BELL);
      break;

    case CTRL('D'):     /* handle ^D for eof */
    case EOF:
      if(bufp == buf) {
        buf[0] = '\0';
        rvalp = NULL;
        break;
      }                 /* else fall through */
    case '\n':
    case '\r':
      putchar('\r');
      putchar('\n');
      *bufp = 0;
      rvalp = buf;
      break;

    case CTRL('H'):
    case DEL:
      /*
       * Change this to a hardcopy erase????
       */
      if (bufp > buf) {
        if (*(bufp-1) == '\t') {
          bufp--;
          goto ctrl_R;
        }
        *--bufp = 0;
        putchar(CTRL('H'));
        putchar(' ');
        putchar(CTRL('H'));
      }
      break;

    case CTRL('R'):
ctrl_R:
      *bufp = '\0';
      printf("^R\n%s%s",PROMPT, buf);
      break;

    case CTRL('U'):
      if (bufp > buf) {
        printf("^U\n%s", PROMPT);
        bufp = buf;
      }
      break;

    case '\t':
      *bufp++ = (char)c;
      putchar(c);
      break;
      
    default:
      /*
       * Make sure there's room for this character
       * plus a trailing \n and 0 byte
       */
      if (isprint(c) && bufp < &buf[len-3]) {
        *bufp++ = (char)c;
        putchar(c);
      } else
        putchar(BELL);
      break;
    }
    
    if (rvalp) break ;
  }
  
  return(rvalp);
}

/* assume max len of LINESIZE, which is correct for almost all callers */
char *
gets(char *buf)
{
	return ttygets(buf, LINESIZE);
}
#endif

/*
 * -------------------------------------------------------------
 *
 * $Log: uartio.c,v $
 * Revision 1.1  1997/08/18 20:42:21  philw
 * updated file from bonsai/patch2039 tree
 *
 * Revision 1.4  1996/10/31  21:51:55  kuang
 * Bring Bonsai IP32 debugcard up to the level of IP32 debugcard v2.4 on Pulpwood
 *
 * Revision 1.3  1995/12/30  03:28:28  kuang
 * First moosehead lab bringup checkin, corresponding to 12-29-95 d15
 *
 * Revision 1.2  1995/11/28  02:20:47  kuang
 * General cleanup and fixed several bugs in the printhex routine.
 *
 * Revision 1.1  1995/11/15  00:42:49  kuang
 * initial checkin
 *
 * Revision 1.2  1995/11/14  23:33:39  kuang
 * Rearranged the rcs keyword a bit and get ready for
 * ptool checkin
 *
 * Revision 1.1  1995/11/01  21:31:48  kuang
 * Initial revision
 *
 * -------------------------------------------------------------
 */


/* END OF FILE */
