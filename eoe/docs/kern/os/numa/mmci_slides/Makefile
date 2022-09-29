TITLE="MMCI Slides"
TARGET=mmci_slides


FMBATCH=$(TOOLROOT)/usr/frame/bin/fmbatch

.SUFFIXES: .frm .mif .ps
.frm.mif:
	@TMPFILE=./${@:.mif=.$$.tmp}; \
	echo "Open $<" > $${TMPFILE}; \
	echo "SaveAs m $< $*.mif" >> $${TMPFILE}; \
	echo "Quit $<" >> $${TMPFILE}; \
	${FMBATCH} $${TMPFILE}; \
        rm -f $${TMPFILE}

.frm.ps:
	@TMPFILE=./${@:.ps=.$$.tmp}; \
	echo "Open $<" > $${TMPFILE}; \
        echo "Open print_options.frm" >> $${TMPFILE}; \
	echo "Print  $<  print_options.frm"  >> $${TMPFILE}; \
	echo "Quit $<" >> $${TMPFILE}; \
	${FMBATCH} $${TMPFILE}; \
        rm -f $${TMPFILE}

##BASED=/usr/ns-home/docs/mempage
##DESTD=$(BASED)/`pwd | sed "s/\/.*\///g"`

${TARGET}: ${TARGET}.mif ${TARGET}.ps
##	webmaker -c ${TARGET}.wml -t $(TITLE) ${TARGET}.mif
##	wmgraph  -n ${TARGET}

##publish: 
##	@echo PUBLISHING TO $(DESTD) 
##	@if test ! -d $(DESTD); then mkdir $(DESTD); fi; cp $(TARGET).ps wmwork/www/* $(DESTD)

##install: $(TARGET) publish

default: ${TARGET}

install: ${TARGET}

prune:
	rm -f *~ *.backup

clobber: prune
	rm -fr *.mif *.ps wmwork		

