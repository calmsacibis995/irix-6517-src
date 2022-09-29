/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef _SCREEN_EXTERN_DOT_H_
#define _SCREEN_EXTERN_DOT_H_

#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision"

/* extern(s) for the screen directory */

/* termcap.c */
extern int _tcsearch(char *,short [], char *[], int, int);

/* _halfdelay.c */
extern int ttimeout(int);

/* _over*.c */
extern int _overlap(WINDOW *, WINDOW *, int);

/* _scr_*.c */
extern int _scr_all(char *, int);

/* V2.makenew.c */
extern WINDOW *_makenew(int, int, int, int);

/* copywin.c */
extern int _mbclrch(WINDOW *, int, int);

/* delay_out.c */
extern int _delay(int, int (*)(int));

/* delkey.c */
extern void delkeymap(TERMINAL *);

/* delkeymap.c */
extern void _blast_keys(TERMINAL *);

/* delterm.c */
extern int delkey(char *, int);

/* newwin.c */
extern int _image(WINDOW  *);

/* endwin.c */
extern int force_doupdate(void);

/* init_costs.c */
extern void _init_costs(void);

/* setupterm.c */
extern void _blast_keys(TERMINAL *);

/* memSset.c */
extern void memSset(chtype *, chtype, int);

/* init_pair.c */
extern void _init_HP_pair(short, short, short);

/* setkeymap.c */
extern int setkeymap(void);

/* tgetch.c */
extern int tgetch(int);

/* init_acs.c.c */
extern int init_acs(void);

/* outch.c */
extern int _outch(int);
extern int _outwch(chtype);

/* prefresh.c */
extern int _prefresh(int(*)(WINDOW *), WINDOW *, int, int, int, int, int, int);
extern int _padjust(WINDOW *, int, int, int, int, int, int);

/* prefresh.c */
extern int scr_reset(FILE *, int);

/* scr_ll_dump.c */
extern int scr_ll_dump(FILE *);

/* endwind.c */
extern int force_doupdate(void);

/* mbgetwidth.c */
extern void mbgetwidth(void);
extern int wcscrw(wchar_t);

/* slk_refresh.c */
extern int _slk_update(void);

/* vidupdate.c */
extern void _change_color(short, short, int (*)(int));

/* doscan.c */
extern int _doscan(FILE *, unsigned char *, ...);

/* mbtranslate.c */
extern char *_strcode2byte(wchar_t *, char *, int);

/* vsscanf.c */
/*extern int vsscanf(char *, char *, ...);*/
extern int vsscanf(char *, char *, char *);

/* mbaddch.c */
extern int _mbvalid(WINDOW *);
extern int _mbaddch(WINDOW *, chtype, chtype);

/* mbgetwidth.c */
extern int mbscrw(int);
extern int mbeucw(int);

/* chkinput.c */
extern int _chkinput(void);

/* mbinsshift.c */
extern int _mbinsshift(WINDOW *, int);

/* mbmove.c */
extern int wmbmove(WINDOW *, int, int);

/* wctomb.c */
extern int _curs_wctomb(char *, wchar_t);

/* wctomb.c */
extern int _curs_mbtowc(wchar_t *, const char *, size_t);

/* wmbinch.c */
extern char *wmbinch(WINDOW *, int, int);

/* tic_main.c */
extern void check_dir(char);

/* tic_hash.c */
extern void make_hash_table(void);
extern struct name_table_entry *find_entry(char *);

/* tic_parse.c */
extern void compile(void);

/* tic_error.c */
extern int syserr_abort(char *fmt, ...);
extern int err_abort(char *fmt, ...);
extern int warning(char *fmt, ...);

/* tic_scan.c */
extern void panic_mode(int);
extern void reset_input(void);
extern int get_token(void);

/* tic_read.c */
extern int must_swap(void);
extern int read_entry(char *, struct _bool_struct *,
		struct _num_struct *,struct _str_struct *);

#ifdef __cplusplus
}
#endif

#endif /* _SCREEN_EXTERN_DOT_H_ */
