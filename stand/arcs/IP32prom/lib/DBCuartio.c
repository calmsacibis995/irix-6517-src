/*
 * -------------------------------------------------------------
 *  $Revision: 1.1 $
 *  $Date: 1997/08/18 20:46:47 $
 * -------------------------------------------------------------
 *  a simple printf function for debugcard UART serial output only.
 */

#include <stdarg.h>
#include <inttypes.h>
#include <st16c1451.h>

#define Bhex    4
#define Shex    12
#define Ihex    28
#define Lhex    60
#define Boct    3
#define Soct    15
#define Ioct    30
#define Loct    63

char digit[] = "0123456789abcdef" ;

/*
 * Function: dprintudec(uint64_t val, int position)
 */
void
dprintudec (uint64_t val, int ovr, int size)
{
    char                 dc[22], c ; 
    register int         ptr, rem, indx, indx1 ;
    register uint64_t    v  ;

    if ((val == 0) && (ovr == 0)) {
      DPUTCHAR ('0') ;
      return ;
    }
    
    for (rem = 0 ; rem < 22 ; dc[rem++] = '0') ;
    ptr = 0   ;
    v   = val ;
    while (v > 10) {
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
        DPUTCHAR (dc[ptr]) ;
        ptr -= 1 ;
    }
}        

/*
 * Function: dprintdec(uint64_t val, int position)
 */
void
dprintdec (int64_t val, int ovr, int size)
{
    char                dc[22], c ; 
    register int        ptr, rem, indx, indx1 ;
    register int64_t    v  ;

    if ((val == 0) && (ovr == 0)) {
      DPUTCHAR ('0') ;
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
    while (v > 10) {
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
    if (c == '-') DPUTCHAR (c) ;
    while (ptr >= 0) {
        DPUTCHAR (dc[ptr]) ;
        ptr -= 1 ;
    }
}        

/*
 * Function: printhex(uint64_t val, int ovr, int pos, char C)
 * val : value to print
 * ovr : if 1, print leading 0's
 * pos : size of the number in bits
 * C   : current formatting charater
 */
void
dprinthex (uint64_t val, int ovr, int pos, char C) 
{
    char c ;
    int printedsomething=0;

    if (val == 0) {
	    DPUTCHAR('0');
	    return;
    }

    while (pos >= 0) {
	c = digit[(val >> pos) & 0xF];

        if ((C == 'X') && (c > '9'))
		c = c - 'a' + 'A' ;

        if ((c != '0') ||
	    ((c == '0') && (ovr || printedsomething))) {
		printedsomething = 1;
		DPUTCHAR (c) ;
        }
        pos -= 4 ; 
    }
}

/*
 * Function: dprintoct(uint64_t val, int position)
 */
void
dprintoct (uint64_t val, int ovr, int pos)
{
    register char c ;
    register int  pcount, indx ;

    pcount = pos ;
    indx   = 0   ;

    if ((val == 0) && (ovr == 0)) {
      DPUTCHAR ('0') ;
      return ;
    }
    
    while (pcount >= 0) {
      if (pcount == 0) c = digit[val & 0x7] ;
      else c = digit[(val >> pcount) & 0x7] ;
      if (c != '0') {
        indx += 1 ;
        DPUTCHAR (c) ;
      } else {
        if ((ovr != 0) || (indx != 0)) DPUTCHAR (c) ;
      }
      pcount -= 3 ; 
    }
}        
            
/*
 * Function: printbin(uint64_t val, int position)
 */
void
dprintbin (uint64_t val, int size)
{
    register char c ;
    register int  pcount ;

    pcount = size;
    while (pcount >= 0) {
        if (pcount == 0) c = digit[val & 0x1] ;
        else c = digit[(val >> pcount) & 0x1] ;
        DPUTCHAR (c) ;
        pcount -= 1 ; 
    }
}        
            
/*
 * This printf only suppot decimal (%d), octal (%o), hexdecimal (%x),
 * and string (%s), also binary (%b) and long long (64 bit %l).
 */
/* #define DebugCARD 1  */
#undef  DebugCARD
void
DPRINTF (char *fmt, ...)
{
#if defined(DebugCARD)
    va_list     args                 ;
    char        *str, *p, errQ[3]    ;
    int         state, i, size, errP ;
    int         ovr                  ;
    
    va_start (args, fmt) ;
    state  = 0   ;
    ovr    = 0   ;
    p      = fmt ;
    errP   = 0   ;
    while (*p) {
        switch (state) {
        case 0:
            if (*p != '%') DPUTCHAR (*p) ;
            else {
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
                DPUTCHAR ('%') ;
                break ; 
            case 'd': /* object type is integer, base 10.   */
            case 'i': /* object type is integer, base 10.   */
                switch (size) {  /* get the right size of the object    */
                case 64:    /* long long */
                    dprintdec ((uint64_t)va_arg(args, int64_t), ovr, 0) ;
                    break ;
                case 16:    /* short     */
                    dprintdec ((uint64_t)va_arg(args, int16_t), ovr, 4) ;
                    break ;
                case 8:     /* byte      */
                    dprintdec ((uint64_t)va_arg(args, int8_t), ovr, 3) ;
                    break ;
                default:    /* int       */
                    dprintdec ((uint64_t)va_arg(args, int32_t), ovr, 9) ;
                    break ;
                }
                break ;
            case 'u': /* object type is unsigned integer, base 10.   */
                switch (size) {  /* get the right size of the object    */
                case 64:    /* long long */
                    dprintudec ((uint64_t)va_arg(args, uint64_t), ovr, 0) ;
                    break ;
                case 16:    /* short     */
                    dprintudec ((uint64_t)va_arg(args, uint16_t), ovr, 4) ;
                    break ;
                case 8:     /* byte   */
                    dprintudec ((uint64_t)va_arg(args, uint8_t), ovr, 3) ;
                    break ;
                default:    /* int       */
                    dprintudec ((uint64_t)va_arg(args, uint32_t), ovr, 9) ;
                    break ;
                }
                break ;
            case 'o': /* object type is unsigned integer, base 8.   */
                switch (size) {  /* get the right size of the object    */
                case 64:    /* long long */
                    dprintoct ((uint64_t)va_arg(args, uint64_t), ovr, Loct) ;
                    break ;
                case 16:    /* short     */
                    dprintoct ((uint64_t)va_arg(args, uint16_t), ovr, Soct) ;
                    break ;
                case 8:     /* byte      */
                    dprintoct ((uint64_t)va_arg(args, uint8_t), ovr, Boct) ;
                    break ;
                default:    /* int       */
                    dprintoct ((uint64_t)va_arg(args, uint32_t), ovr, Ioct) ;
                    break ;
                }
                break ;
            case 'x': /* object type is unsigned integer, base 16.   */
            case 'X': /* object type is unsigned integer, base 16.   */
                switch (size) {  /* get the right size of the object    */
                case 64:    /* long long */
                    dprinthex ((uint64_t)va_arg(args, uint64_t),ovr,Lhex,*p) ;
                    break ;
                case 16:    /* short     */
                    dprinthex ((uint64_t)va_arg(args, uint16_t),ovr,Shex,*p) ;
                    break ;
                case 8:     /* byte     */
                    dprinthex ((uint64_t)va_arg(args, uint8_t),ovr,Bhex,*p) ;
                    break ;
                default:    /* int       */
                    dprinthex ((uint64_t)va_arg(args, uint32_t),ovr,Ihex,*p) ;
                    break ;
                }
                break ;
            case 'c': /* object type is unsigned char.   */
                DPUTCHAR ((char)va_arg(args, char)) ;
                break ;
            case 's': /* object type is a address point to a char. */
                str = (char *) va_arg(args, unsigned int) ;
                while (*str != '\0') DPUTCHAR (*str++)  ;
                break ;
            case '0': /* I consider this an error condition. */
            case 'l': /* I consider this an error condition. */
            case 'h': /* I consider this an error condition. */
            default:  /* This also indicate an error.        */ 
                for (i = 0 ; i < 3 ; i++) DPUTCHAR (errQ[i]) ;
                DPUTCHAR (*p++) ; /* move to next character. */
                break ;
            }
            state = 0 ; /* reset the state machine. */
            size  = 0 ; /* and reset the size.      */
            ovr   = 0 ; /* and 0 pedding flag.      */
            errP  = 0 ; /* reset the errQ.          */
            p++       ; /* point to next character. */
        }
    }
#endif
    return ;
}


/*
 * -------------------------------------------------------------
 *
 * $Log: DBCuartio.c,v $
 * Revision 1.1  1997/08/18 20:46:47  philw
 * updated file from bonsai/patch2039 tree
 *
 * Revision 1.1  1996/10/21  03:08:03  kuang
 * Merged in changes from Pulpwood IP32prom - up to v2.4 release
 *
 * Revision 1.6  1996/07/27  00:55:15  kuang
 * Added changes to support r10000
 *
 * Revision 1.5  1995/12/30  04:50:30  kuang
 * make DPRINTF a nop function for now
 *
 * Revision 1.4  1995/11/28  02:29:28  kuang
 * Fixed setCacheSpecifics, setCPUSpecifics, setFPUSpecifics ... - Configuration in general.
 *
 * Revision 1.3  1995/11/22  03:06:15  mwang
 * Fixed another bug in dprinthex, plus minor clean up.
 *
 * Revision 1.2  1995/11/21  03:11:00  mwang
 * Fixed dprinthex to print at least a single 0 if the value passed in was a
 * 0.
 *
 * Revision 1.1  1995/11/20  22:49:09  kuang
 * initial checkin
 *
 * -------------------------------------------------------------
 */


/* END OF FILE */
