#!smake -M
#
#ident  "$Revision: 1.10 $"
#
# lib/libcurses/screen makefile
#

DEPTH=.

include $(ROOT)/usr/include/make/libleafdefs

SUBDIR_CDEFS= -DNOBLIT
SUBDIR_CINCS=-I.
HFLAGS= ROOTDIR=/usr
HCC=	$(HFLAGS) /usr/bin/cc 
.SUFFIXES: .H .p

#
# Local Definitions
#
SLIB=	$(TOP)$(LIBRARY)
HBIN=	$(TOOLROOT)/bin
CP=	cp
RM=	rm
ED=	ed

GENERATED_HEADERS=curses.h term.h
HEADERS=$(GENERATED_HEADERS) unctrl.h curses_inc.h curshdr.h curses_wchar.h
BUILTC=	keyname.c termcap.c tiget.c tnames.c tinames.c tifnames.c tifget.c
BUILT=	$(GENERATED_HEADERS) keycaps $(BUILTC)

BSRC= print.c iexpand.c
CSRC= captoinfo.c otermcap.c $(BSRC)
ISRC= infocmp.c cexpand.c infotocap.c $(BSRC)
TICHDRS= compiler.h object.h
TICSRC= tic_main.c tic_hash.c tic_captab.c tic_scan.c tic_error.c \
	tic_parse.c tic_read.c 


#
# List of objects
#
BOBJ=	print.o iexpand.o
COBJ=	captoinfo.o otermcap.o $(BOBJ)
IOBJ=	infocmp.o cexpand.o infotocap.o $(BOBJ)
TICOBJS=	$(TICSRC:.c=.o) tnames.o tinames.o tifnames.o
HTICOBJS=	$(TICSRC:.c=.H) tnames.o tinames.o tifnames.o

CFILES0=	_addch.c _addchnstr.c _addchstr.c _addnstr.c \
		_addstr.c _attroff.c _attron.c _attrset.c \
		_beep.c _bkgd.c _bkgdset.c _border.c _box.c \
		_clear.c _clrtobot.c _clrtoeol.c _crmode.c \
		_del_curterm.c _delch.c _deleteln.c _echo.c \
		_echochar.c _erase.c _fixterm.c _flash.c \
		_garbagdlins.c _garbagedwin.c _getch.c \
		_getstr.c _halfdelay.c _hline.c _inch.c \
		_inchnstr.c _inchstr.c _innstr.c _insch.c \
		_insdelln.c _insertln.c _insnstr.c _insstr.c \
		_instr.c _intrflush.c _meta.c _move.c _mvaddch.c \
		_mvaddchnstr.c _mvaddchstr.c _mvaddnstr.c \
		_mvaddstr.c _mvdelch.c _mvgetch.c _mvgetstr.c \
		_mvhline.c _mvinch.c _mvinchnstr.c _mvinchstr.c \
		_mvinnstr.c _mvinsch.c _mvinsnstr.c _mvinsstr.c \
		_mvinstr.c _mvvline.c _mvwaddch.c _mvwaddchnst.c \
		_mvwaddchstr.c _mvwaddnstr.c _mvwaddstr.c _mvwdelch.c \
		_mvwgetch.c _mvwgetstr.c _mvwhline.c _mvwinch.c \
		_mvwinchnst.c _mvwinchstr.c _mvwinnstr.c _mvwinsch.c \
		_mvwinsnstr.c _mvwinsstr.c _mvwinstr.c _mvwvline.c \
		_newterm.c _nl.c _nocrmode.c _noecho.c _nonl.c \
		_noqiflush.c _overlay.c _overwrite.c _qiflush.c \
		_refresh.c _resetterm.c _saveterm.c _scr_init.c \
		_scr_restore.c _scr_set.c _scrl.c _scroll.c \
		_set_curterm.c _set_term.c _setscrreg.c _slk_init.c \
		_standend.c _standout.c _subpad.c _timeout.c \
		_touchline.c _unctrl.c _vline.c _waddchstr.c \
		_waddstr.c _wclear.c _wdeleteln.c _werase.c \
		_winsertln.c _winsstr.c _wstandend.c _wstandout.c

CFILES1=	V2.__sscans.c V2._sprintw.c V2.makenew.c V3.box.c \
		V3.initscr.c V3.m_addch.c V3.m_addstr.c \
		V3.m_clear.c V3.m_erase.c V3.m_initscr.c V3.m_move.c \
		V3.m_newterm.c V3.m_refresh.c V3.newterm.c \
		V3.pechochar.c V3.upd_old_y.c V3.vidattr.c V3.vidputs.c \
		V3.waddch.c V3.wattroff.c V3.wattron.c V3.wattrset.c \
		V3.wechochar.c V3.winsch.c baudrate.c can_change.c \
		color_cont.c cbreak.c chkinput.c clearok.c copywin.c \
		curs_set.c curserr.c curses.c def_prog.c delay.c \
		delay_out.c delkey.c delkeymap.c delscreen.c delterm.c \
		delwin.c derwin.c doupdate.c draino.c dupwin.c endwin.c \
		erasechar.c flushinp.c getattrs.c getbegyx.c getbkgd.c \
		getmaxyx.c getparyx.c getsyx.c gettmode.c getwin.c \
		getyx.c has_colors.c has_ic.c has_il.c idcok.c idlok.c \
		immedok.c init_acs.c init_color.c init_costs.c \
		init_pair.c initscr.c is_linetou.c is_wintou.c \
		keypad.c killchar.c leaveok.c

CFILES2=	longname.c makenew.c memSset.c meta.c mouse.c mvcur.c \
		mvderwin.c mvprintw.c mvscanw.c mvwin.c mvwprintw.c \
		mvwscanw.c napms.c newkey.c newpad.c newscreen.c \
		newwin.c nocbreak.c nodelay.c noraw.c \
		notimeout.c outch.c overlap.c pair_cont.c \
		pechochar.c pnoutref.c prefresh.c printw.c putp.c \
		putwin.c quick_echo.c raw.c redrawwin.c reset_sh.c \
		resetty.c restart.c ring.c ripoffline.c savetty.c \
		scanw.c scr_all.c scr_dump.c scr_ll_dump.c scr_reset.c \
		scrollok.c setcurscreen.c setcurterm.c setecho.c \
		setkeymap.c setnonl.c setqiflush.c setsyx.c setterm.c \
		setupterm.c slk_atroff.c slk_atron.c slk_atrset.c \
		slk_clear.c slk_label.c slk_noutref.c slk_refresh.c \
		slk_restore.c slk_set.c slk_start.c slk_touch.c \
		start_col.c subwin.c syncok.c tcsearch.c termattrs.c \
		termerr.c termname.c tgetch.c tgoto.c \
		tinputfd.c  \
		touchwin.c tparm.c tputs.c trace.c tstp.c ttimeout.c \
		typeahead.c unctrl.c ungetch.c use_env.c \
		untouchwin.c vidputs.c vidupdate.c vsscanf.c vwprintw.c \
		vwscanw.c waddch.c waddchnstr.c waddnstr.c wattroff.c \
		wattron.c wattrset.c wbkgd.c wbkgdset.c wborder.c \
		wclrtobot.c wclrtoeol.c wdelch.c wechochar.c wgetch.c \
		wgetstr.c whline.c winch.c winchnstr.c winchstr.c \
		winnstr.c winsch.c winsdelln.c winsnstr.c winstr.c \
		wmove.c wnoutref.c wprintw.c wredrawln.c wrefresh.c \
		wscanw.c wscrl.c wsetscrreg.c wsyncdown.c wsyncup.c \
		wtimeout.c wtouchln.c wvline.c 

CFILES3=	mbaddch.c mbcharlen.c mbdisplen.c mbgetwidth.c \
		mbinch.c mbinsshift.c mbmove.c mbtranslate.c \
		pechowchar.c tgetwch.c ungetwch.c waddnwstr.c \
		waddwch.c waddwchnstr.c wechowchar.c wgetwstr.c \
		wgetwch.c winnwstr.c winsnwstr.c winswch.c \
		winwch.c winwchnstr.c winwstr.c

CFILES4=	_addnwstr.c _addwch.c _addwchnstr.c _addwchstr.c \
		_addwstr.c _echowchar.c _getnwstr.c _getwch.c \
		_getwstr.c _innwstr.c _insnwstr.c _inswch.c \
		_inswstr.c _inwch.c _inwchnstr.c _inwchstr.c \
		_inwstr.c _mvaddnwstr.c _mvaddwch.c _mvaddwchns.c \
		_mvaddwchstr.c _mvaddwstr.c _mvgetnwstr.c _mvgetwch.c \
		_mvgetwstr.c _mvinnwstr.c _mvinsnwstr.c _mvinswch.c \
		_mvinswstr.c _mvinwch.c _mvinwchnstr.c _mvinwchstr.c \
		_mvinwstr.c _mvwaddnwstr.c _mvwaddwch.c _mvwaddwchns.c \
		_mvwaddwchs.c _mvwaddwstr.c _mvwgetnwstr.c _mvwgetwch.c \
		_mvwgetwstr.c _mvwinnwstr.c _mvwinsnwstr.c _mvwinswch.c \
		_mvwinswstr.c _mvwinwch.c _mvwinwchns.c _mvwinwchstr.c \
		_mvwinwstr.c _waddwchstr.c _waddwstr.c _winswstr.c \
		_winwchstr.c

# added to support the multibyte functions - these routines also appear in libw
CFILES5=	mbstowcs.c mbtowc.c wcstombs.c wctomb.c

LIBCFILES = $(BUILTC) $(CFILES0) $(CFILES1) $(CFILES2) $(CFILES3) $(CFILES4) \
	$(CFILES5)
CFILES = $(LIBCFILES) $(CSRC) $(ISRC) $(TICSRC)
# the *.new, *.tmp, and tmp.* are in case any of the *.ed scripts fail
LDIRT = $(TICSRC:.c=.H) $(BUILT) *.p *.new *.new1 *.new2 *.tmp tmp.* maketerm.edx
#TARGETS = $(SLIB) tic tput captoinfo infocmp dumpti chgtinfo \
#	tic_host llib-lcurses.ln
TARGETS += tic tput captoinfo infocmp dumpti chgtinfo llib-lcurses.ln

LIBOBJECTS = $(LIBCFILES:.c=.o)
PLIBOBJECTS = $(LIBCFILES:.c=.p)

#default: $(GENERATED_HEADERS) $(SLIB) tools host_tools llib-lcurses.ln
#
# The tools should be made by default, but do this after the compiler
# driver knows that building -non_shared means to look in /usr/lib/nonshared
# for the libraries.
#
#default! $(GENERATED_HEADERS) $(SLIB) tools
default! $(GENERATED_HEADERS) $(SLIB)

#
# Don't want the object files to be deleted after the making the archive
# so don't use LIBLEAFRULES.
# include $(LIBLEAFRULES)
#
include $(COMMONRULES)

tools:  tic tput captoinfo infocmp dumpti chgtinfo llib-lcurses.ln

host_tools: tic_host

.c.H:
	-mv $*.o $*.O
	$(HCC) -D_SGI_SOURCE -DBSD42 $(LCOPTS) -I -I. -I/usr/include -Dident=undef -c $*.c
	mv $*.o $*.H
	-mv $*.O $*.o

.c.p:
	$(CC) $(CFLAGS) -p -c $*.c -o $@

headers: $(GENERATED_HEADERS) $(_FORCE)
	$(INSTALL) -F /usr/include -m 444 "curses.h term.h unctrl.h"

#
# Don't delete .o from src directory.
#
$(SLIB): $(LIBOBJECTS)
	$(AR) clr $@ $(LIBOBJECTS)

#
# Profiled library
#
p:	$(TOP)p$(LIBRARY)

$(TOP)p$(LIBRARY): $(PLIBOBJECTS)
	$(AR) clr $@ $?

llib-lcurses.ln:	llib-lcurses
	$(LINT) -Y$(ROOT)/usr/include -vxd -Xa -ocurses -n llib-lcurses

captoinfo: $(GENERATED_HEADERS) $(COBJ) $(SLIB)
	$(CCF) $(COBJ) $(SLIB) -o captoinfo $(LDFLAGS)

infocmp: $(GENERATED_HEADERS) $(IOBJ) $(SLIB)
	$(CCF) $(IOBJ) $(SLIB) -o infocmp $(LDFLAGS)

dumpti: setupterm.c curses.h term.h tnames.o tinames.o tifnames.o termerr.o curses.o mbgetwidth.o
	-rm -f dumpti.c
	ln -s setupterm.c dumpti.c
	$(CCF) $(CFLAGS) -DDUMPTI dumpti.c tnames.o tinames.o tifnames.o termerr.o curses.o mbgetwidth.o -o dumpti $(LDFLAGS) ; rm -rf dumpti.c

tic: $(GENERATED_HEADERS) $(TICOBJS)
	$(CCF) -o tic $(TICOBJS) $(LDFLAGS)

tput:	$(GENERATED_HEADERS) $(SLIB) tput.o
	$(CCF) -o $@ tput.o $(SLIB) $(LDFLAGS)

tsetterminfo:	$(GENERATED_HEADERS) $(SLIB) tsetterminfo.o
	$(CCF) -o $@ tsetterminfo.o $(SLIB) $(LDFLAGS)

# tic_host should be built with native tools, since it's intermediate tool.
tic_host:	$(HTICOBJS)
	$(HCC) -o $@ $(LCOPTS) $(HTICOBJS)


#
# These rules single thread creation of the maketerm-generated
# files.
#
.SUFFIXES: .edx .ed
MAKETERM_GENERATED=term.h tnames.c tinames.c tifnames.c
$(MAKETERM_GENERATED): caps maketerm.edx
maketerm.edx: maketerm.ed
	rm -f $(MAKETERM_GENERATED)
	$(ED) - < maketerm.ed
	chmod a-w $(MAKETERM_GENERATED)
	touch maketerm.edx

keycaps: caps
	grep "KEY_" caps > _caps
	sh -c "if cmp -s _caps keycaps ; then \
		rm -f _caps ; \
	else \
		mv _caps keycaps ; \
	fi"

curses.h: keycaps curses.ed
	rm -f curses.h
	$(ED) - < curses.ed
	chmod -w curses.h

keyname.c: keycaps keyname.sh
	rm -f keyname.c
	sh keyname.sh
	chmod -w keyname.c

setkeymap.c: keycaps
	@if tail -1 keycaps | grep 'KEY_MOUSE' > /dev/null;then :; \
		else echo new keys!! must change setkeymap.c; exit 1;fi

termcap.c:	termcap.ed caps
	rm -f termcap.c
	$(ED) - < termcap.ed
	chmod -w termcap.c

tifget.c:	tifget.ed caps
	$(ED) - < tifget.ed
	chmod -w tifget.c

tiget.c:	tiget.ed caps
	rm -f tiget.c
	cat /dev/null > tiget.c
	$(ED) - < tiget.ed
	chmod -w tiget.c

# this is needed so the generated headers will get built if
# make depend isn't run
$(LIBOBJECTS): $(GENERATED_HEADERS)

$(PLIBOBJECTS): $(GENERATED_HEADERS)

# have to build the generated headers and .c files to do depends...
depend incdepend:  $(GENERATED_HEADERS) $(BUILTC)
