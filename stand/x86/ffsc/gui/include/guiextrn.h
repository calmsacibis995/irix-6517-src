/*********************************************/
/*    Metagraphics Software Corporation      */
/*        Copyright (c) 1987-1993            */
/*********************************************/
/* FILE:    guiextrn.h                       */
/* PURPOSE: module contains C function       */
/*          prototypes                       */
/*********************************************/
/*  %kw # %v    %n    %d    %t # */
/* Version # 5    GUIEXTRN.H    22-Mar-95    13:31:48 # */

#ifdef __cplusplus      /* If C++, disable "mangling" */
extern "C" {
#endif

/* In Application */
void  Init          (int argc, char *argv[]);
void  Close         (void); 
void  ProcessMenu   (void);
void  IdleProcess   (void);
void  RegenDesk     (void);

/* BARGRF */
void	cancelBargraphDisplay	(void);

/* GUIDILG */
void  InitDialog    (void);
int   Dialog        (int, char *, char *);
void  ShowDialog    (int, dialogBox *);
void  HideDialog    (int, dialogBox *);
void  OpenWindow    ( dialogBox * );
void  CloseWindow   ( dialogBox * );
void  DrawButton    (rect *, char * );
void  DialogButton  (rect *, int );
void  OnButton      ( rect * );
void  Button3D      ( rect * );
int   DialogText    (int, dialogBox *, char * ); 
void  DilgShowText  ( rect *, char * );
void  DrawScrollBar ( scrollBar * );
void  PositionSB    ( scrollBar * );  
void  DoScrollBar   ( scrollBar * );

/* GUIFILE */
int   FileDialog    ( char * );

/* GUIINIT */
void  InitGUI       (int, char *[]);
void  ResetGUIPalette (void);
void  MergeGUIPalette ( palData *DstPal );

/* GUIMAIN */
void  main         (int, char *[]);
void  abend        (int, char *);
#if WatcomC
   int far CriticalError( unsigned deverror, unsigned errcode, unsigned far *devhdr);
#else
   int CriticalError( int ,  int );
#endif

/* GUIMOTF */
void  InitMenu      (void);
int   CheckIfMenu   (void);
void  WaitMenuSelect(void);

/* GUIFORM */
void  DrawForm      (dialogBox *, formEntry *, int );
int   DoForm        (dialogBox *, formEntry *, int );
void  DrawBallot    ( formEntry * );
void  OnBallot      ( formEntry *, int );
int   DoBallot      ( formEntry * );
int   FormText      ( dialogBox *, formEntry *, int);

/* GUIVIEW */
void  ViewFile      ( char *fileName, char *wname );
void  ViewBuffer    ( char *buf, char *wname );
char*  ViewItemList ( char *buf, char *wname );
int   GUIWindow     ( dialogBox *dia, char *name );
int   CheckClose    ( dialogBox *dia );
void  DrawView      ( char *name );

/* GUIUTIL */
void  delay         (unsigned int);
void  DoBeep        (void);
int   picX          (int);
int   picY          (int);
int   equX          (int);
int   equY          (int);
void  ShadowRect    (rect *, int);
void  OnBevel       (rect *);
void  OffBevel      (rect *);
void  FlushEvents   (void);
/* int   KeyGUIEvent   (int mwWAIT ); */			/* moved to swevt.c */
int   PeekGUIEvent  (int mwEVNO );
char*  DosError     (int);

/* SWEVT */
void initdisplay(void);
int   KeyGUIEvent   (int mwWAIT );

#ifdef __cplusplus
}
#endif

/* End of File  -  GUIEXTRN.H */

