/*********************************************/
/*    Metagraphics Software Corporation      */
/*        Copyright (c) 1987-1993            */
/*********************************************/
/* FILE:    guiconst.h                       */
/* PURPOSE: module contains special struct-  */
/*          ures and globals                 */
/*********************************************/
/*  %kw # %v    %n    %d    %t # */
/* Version # 3    GUICONST.H    8-Jun-93    16:21:52 # */


#ifdef GUIMAIN
#define GUIXTERN
#else
#define GUIXTERN   extern
#endif

#ifdef __386__
# define REGWRD w
# define INTCALL int386
#else
# define REGWRD x
# define INTCALL int86
#endif

/* abnormal end error codes */
#define ER_NODRVR   -101
#define ER_MEM      -102
#define ER_LOAD     -103

#define MAXLEN      80        /* maximum string length */
#define ENVRNVAR    "METAPATH"  /* environment variable name for file search */
#define MKCHAR      '&'  /* special character in menu string for Menu Key */

/* Menu Entry Structure */
typedef struct {           
   char  menuName[20];     /* Menu bar title               */
   int   menuCount;        /* Number of menu entries       */
   int   menuFlags;        /* Menu flags                   */
   int   menuWidth;        /* Menu title pixel width       */
   rect  menuBar;          /* Menu bar rect                */
   rect  menuRect;         /* Menu entries rectangle       */
   char  *menuEntry[10];   /* Menu entries text            */
}  menus;


#define allMenus  0xFF
#define AWAKEMU   0x44     /* Key to make menu bar active (0x44 = F10) */


/* colors structure */
struct _colors {
    int desktop;           /* desktop color */
    int deskpat;           /* desktop pattern */
    int bar;               /* menu bar color */
    int text;              /* text color    */
    int htkey;             /* hot key color */
    int high;              /* bevel highlight color */         
    int low;               /* bevel lowlight color */
};

#ifdef GUIMAIN
struct _colors color_tab[] =  {
    { White, 25, White, Black, Black, White, Black },   /* monochrome */
    { White, 25, 3, 2, Black, White, Black },           /* 4 color */
    { Gray, 1, 6, Black, Black, 5, Blue },                   /* 16 color */
    { Gray, 1, 6, Black, Black, 5, Blue }                    /* 256 color */
}; 
#else
extern struct _colors color_tab[];
#endif
GUIXTERN struct _colors *colors;

/* Operator Selection Event structure */
GUIXTERN struct {
   int      selectID;
   int      selMenu;
   int      selItem;
   int      selX;
   int      selY;
   int      selState;
   char     selChar;
}  opSelect;


/* Select IDs  */
#define nullSelect 0x00
#define menuSelect 0x01
#define keySelect  0x02
#define exitSelect 0x80


/* Event Types */
#define mouseDown     1
#define mouseUp       2
#define charEvent     3
#define ctrl_C        5

/* Event State Masks */
#define  ALTKEY 0x0008


/* dialog box structure */
typedef struct {
    rect   BoundRect;   /* Dialog rectangle limits      */
    rect   TxR;         /* Dialog text rectangle        */
    rect   KeyR;        /* Dialog key-in rectangle      */
    rect   OkR;         /* Dialog "ok" rectangle        */
    rect   CanR;        /* Dialog "cancel" rectangle    */
    rect   BoxR;        /* Dialog misc. box rect        */
    int    flags;       /* misc flags                   */
    image *holdImage;   /* pointer to image save area   */
} dialogBox;

#define diaNull       0      /* Dialog types, null */
#define diaChr        1      /* character string   */
#define diaNum        2      /* numeric string     */
#define diaHex        4      /* hex  string        */
#define diaAlert      5      /* alert (only ok button) */
#define diaFile       6      /* file selection     */
#define diaKey        7      /* key entry          */
#define diaSNum       8      /* signed numeric string */
#define diaBox        9      /* plain box          */

#define diaBallot     20     /* ballot box dialog type */
#define diaFix        21     /* fixed point dialog type */

#define Ok            1
#define Cancel        0


/* scroll bar structure */
typedef struct {
    rect    SBRect;     /* scroll bar bounding rect   */
    rect    thumb;      /* current thumb box position */
    rect    upbox;      /* up arrow bounding box      */
    rect    dnbox;      /* down arrow bounding box    */
    int     minval;     /* minimum value for scroll   */
    int     maxval;     /* maximum value for scroll   */
    int     curval;     /* current value              */
    void    (*sbfunc)(int); /* function to call to do scrolling */
    int     flags;      /* misc flags                 */
}   scrollBar;

/* scroll bar functions */
#define up            1
#define down          2
#define pgup          3
#define pgdown        4
#define position      5  

/* icon characters */
#define uparrow       (char) 128
#define dnarrow       (char) 129
#define lfarrow       (char) 130
#define rtarrow       (char) 131
#define logo          (char) 132
#define pencil        (char) 133

/* form entry structure */
typedef struct {
     char *tag;            /* name of field*/
     int  tagX;            /* origin of tag */
     int  tagY;
     rect inbox;           /* field input rectangle */
     int  thingtype;       /* data type code */
     void *thing;          /* pointer to data */
     int  ckMin;           /* range of valid input */
     int  ckMax;
} formEntry;

/* misc. globals */
GUIXTERN dialogBox mscDlgBox;
GUIXTERN char *viewBuf;
GUIXTERN dialogBox viewDlgBox;
GUIXTERN scrollBar viewSBar;

GUIXTERN int      curCursor;
#define arrowCurs 0        
#define crossCurs 2
#define handCurs  4
#define watchCurs 7

GUIXTERN grafPort *scrnPort;   /* pointer to default screen port   */
GUIXTERN int   GrafixCard;     /* graphics card ID                 */
GUIXTERN int   GrafixInput;    /* pointing device ID               */
GUIXTERN char  rasterBuf[1024];/* buffer for raster lines          */
GUIXTERN char  fileNumber;     /* unique file number counter       */
GUIXTERN long  resX;           /* GUIs idea of screen resoultion   */
GUIXTERN long  resY;
GUIXTERN mwEvent opEvent;      /* current event queue element       */
GUIXTERN fontRcd *sysFont;     /* pointer to system font record     */
GUIXTERN long  sizeFont;       /* max size of font buffer           */
GUIXTERN char  msg1[MAXLEN];   /* misc text buffer                  */
GUIXTERN rect  barRect;        /* menu bar rectangle                */
GUIXTERN int   barFlags;       /* active menu bar flags             */
GUIXTERN unsigned int dblClick;/* min double click interval         */

GUIXTERN char  filName[MAXLEN];/* current edit file name            */


/* End of File  -  GUICONST.H */

