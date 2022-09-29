#ifndef __GPARSER_H__
#define __GPARSER_H__

#ifndef TRUE
#define TRUE               1
#endif

#ifndef FALSE
#define FALSE              !TRUE
#endif

#define OPTION_BUF_SIZE    256
#define GET_TOKEN          1


typedef struct gParser_t {
  char *OptionStr;
  int  Flags;
  int  *Found;
  char *OptionBuf;
  int  OptionBufSize;
} GPARSER_T;

typedef struct {
  char *str;
  int found;
  char buf[OPTION_BUF_SIZE];
} OPTIONS_T;

int gParser( int argc, char *argv[], int *Nargc_p, GPARSER_T *gParser_p, char *ArgBufp, long ArgBufLen );

#endif /* __GPARSER_H__ */
