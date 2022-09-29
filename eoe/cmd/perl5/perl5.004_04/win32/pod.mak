CONVERTERS = pod2html pod2latex pod2man pod2text checkpods

HTMLROOT = /	# Change this to fix cross-references in HTML
POD2HTML = pod2html \
	    --htmlroot=$(HTMLROOT) \
	    --podroot=.. --podpath=pod:lib:ext:vms \
	    --libpods=perlfunc:perlguts:perlvar:perlrun:perlop

all: $(CONVERTERS) html

PERL = ..\miniperl.exe
PL2BAT = ..\win32\bin\pl2bat.pl

POD = \
	perl.pod	\
	perldelta.pod	\
	perldata.pod	\
	perlsyn.pod	\
	perlop.pod	\
	perlre.pod	\
	perlrun.pod	\
	perlfunc.pod	\
	perlvar.pod	\
	perlsub.pod	\
	perlmod.pod	\
	perlform.pod	\
	perllocale.pod	\
	perlref.pod	\
	perldsc.pod	\
	perllol.pod	\
	perltoot.pod	\
	perlobj.pod	\
	perltie.pod	\
	perlbot.pod	\
	perlipc.pod	\
	perldebug.pod	\
	perldiag.pod	\
	perlsec.pod	\
	perltrap.pod	\
	perlstyle.pod	\
	perlpod.pod	\
	perlbook.pod	\
	perlembed.pod	\
	perlapio.pod	\
	perlxs.pod	\
	perlxstut.pod	\
	perlguts.pod	\
	perlcall.pod	\
	perlfaq.pod	\
	perlfaq1.pod	\
	perlfaq2.pod	\
	perlfaq3.pod	\
	perlfaq4.pod	\
	perlfaq5.pod	\
	perlfaq6.pod	\
	perlfaq7.pod	\
	perlfaq8.pod	\
	perlfaq9.pod	\
	perltoc.pod

MAN = \
	perl.man	\
	perldelta.man	\
	perldata.man	\
	perlsyn.man	\
	perlop.man	\
	perlre.man	\
	perlrun.man	\
	perlfunc.man	\
	perlvar.man	\
	perlsub.man	\
	perlmod.man	\
	perlform.man	\
	perllocale.man	\
	perlref.man	\
	perldsc.man	\
	perllol.man	\
	perltoot.man	\
	perlobj.man	\
	perltie.man	\
	perlbot.man	\
	perlipc.man	\
	perldebug.man	\
	perldiag.man	\
	perlsec.man	\
	perltrap.man	\
	perlstyle.man	\
	perlpod.man	\
	perlbook.man	\
	perlembed.man	\
	perlapio.man	\
	perlxs.man	\
	perlxstut.man	\
	perlguts.man	\
	perlcall.man	\
	perlfaq.man	\
	perlfaq1.man	\
	perlfaq2.man	\
	perlfaq3.man	\
	perlfaq4.man	\
	perlfaq5.man	\
	perlfaq6.man	\
	perlfaq7.man	\
	perlfaq8.man	\
	perlfaq9.man	\
	perltoc.man

HTML = \
	perl.html	\
	perldelta.html	\
	perldata.html	\
	perlsyn.html	\
	perlop.html	\
	perlre.html	\
	perlrun.html	\
	perlfunc.html	\
	perlvar.html	\
	perlsub.html	\
	perlmod.html	\
	perlform.html	\
	perllocale.html	\
	perlref.html	\
	perldsc.html	\
	perllol.html	\
	perltoot.html	\
	perlobj.html	\
	perltie.html	\
	perlbot.html	\
	perlipc.html	\
	perldebug.html	\
	perldiag.html	\
	perlsec.html	\
	perltrap.html	\
	perlstyle.html	\
	perlpod.html	\
	perlbook.html	\
	perlembed.html	\
	perlapio.html	\
	perlxs.html	\
	perlxstut.html	\
	perlguts.html	\
	perlcall.html	\
	perlfaq.html	\
	perlfaq1.html	\
	perlfaq2.html	\
	perlfaq3.html	\
	perlfaq4.html	\
	perlfaq5.html	\
	perlfaq6.html	\
	perlfaq7.html	\
	perlfaq8.html	\
	perlfaq9.html
# not perltoc.html

TEX = \
	perl.tex	\
	perldelta.tex	\
	perldata.tex	\
	perlsyn.tex	\
	perlop.tex	\
	perlre.tex	\
	perlrun.tex	\
	perlfunc.tex	\
	perlvar.tex	\
	perlsub.tex	\
	perlmod.tex	\
	perlform.tex	\
	perllocale.tex	\
	perlref.tex	\
	perldsc.tex	\
	perllol.tex	\
	perltoot.tex	\
	perlobj.tex	\
	perltie.tex	\
	perlbot.tex	\
	perlipc.tex	\
	perldebug.tex	\
	perldiag.tex	\
	perlsec.tex	\
	perltrap.tex	\
	perlstyle.tex	\
	perlpod.tex	\
	perlbook.tex	\
	perlembed.tex	\
	perlapio.tex	\
	perlxs.tex	\
	perlxstut.tex	\
	perlguts.tex	\
	perlcall.tex	\
	perlfaq.tex	\
	perlfaq1.tex	\
	perlfaq2.tex	\
	perlfaq3.tex	\
	perlfaq4.tex	\
	perlfaq5.tex	\
	perlfaq6.tex	\
	perlfaq7.tex	\
	perlfaq8.tex	\
	perlfaq9.tex	\
	perltoc.tex

man:	pod2man $(MAN)

html:	pod2html $(HTML)

tex:	pod2latex $(TEX)

toc:
	$(PERL) -I..\lib buildtoc >perltoc.pod

.SUFFIXES: .pm .pod

.SUFFIXES: .man

.pm.man:
	$(PERL) -I..\lib pod2man $*.pm >$*.man

.pod.man:
	$(PERL) -I..\lib pod2man $*.pod >$*.man

.SUFFIXES: .html

.pm.html:
	$(PERL) -I..\lib $(POD2HTML) --infile=$*.pm --outfile=$*.html

.pod.html:
	$(PERL) -I..\lib $(POD2HTML) --infile=$*.pod --outfile=$*.html

.SUFFIXES: .tex

.pm.tex:
	$(PERL) -I..\lib pod2latex $*.pm

.pod.tex:
	$(PERL) -I..\lib pod2latex $*.pod

clean:
	del /f $(MAN) $(HTML) $(TEX)
	del /f pod2html-*cache
	del /f *.aux *.log

realclean:	clean
	del /f $(CONVERTERS)

distclean:	realclean

check:	checkpods
	@echo "checking..."; \
	$(PERL) -I..\lib checkpods $(POD)

# Dependencies.
pod2latex:	pod2latex.PL ..\lib\Config.pm
	$(PERL) -I..\lib pod2latex.PL
	$(PERL) $(PL2BAT) pod2latex

pod2html:	pod2html.PL ..\lib\Config.pm
	$(PERL) -I..\lib pod2html.PL
	$(PERL) $(PL2BAT) pod2html

pod2man:	pod2man.PL ..\lib\Config.pm
	$(PERL) -I..\lib pod2man.PL
	$(PERL) $(PL2BAT) pod2man

pod2text:	pod2text.PL ..\lib\Config.pm
	$(PERL) -I..\lib pod2text.PL
	$(PERL) $(PL2BAT) pod2text

checkpods:	checkpods.PL ..\lib\Config.pm
	$(PERL) -I..\lib checkpods.PL
	$(PERL) $(PL2BAT) checkpods


