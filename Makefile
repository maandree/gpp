# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.  This file is offered as-is,
# without any warranty.

VERSION = 1.5

PREFIX = /usr
BIN = /bin
DATA = /share
BINDIR = $(PREFIX)$(BIN)
DATADIR = $(PREFIX)$(DATA)
DOCDIR = $(DATADIR)/doc
INFODIR = $(DATADIR)/info
MANDIR = $(DATADIR)/man
MAN1DIR = $(MANDIR)/man1
LICENSEDIR = $(DATADIR)/licenses

PKGNAME = gpp
COMMAND = gpp

PY = python
SHEBANG = /usr$(BIN)/env $(PY)


.PHONY: default
default: cmd info

.PHONY: all
all: cmd doc

.PHONY: cmd
cmd: bin/gpp

bin/gpp: src/gpp.py
	@mkdir -p bin
	env VERSION='$(VERSION)' SHEBANG='$(SHEBANG)' $(PY) "$<" < "$<" > "$@"

.PHONY: doc
doc: info pdf dvi ps

.PHONY: info
info: bin/gpp.info
bin/%.info: obj/%.texinfo obj/fdl.texinfo
	@mkdir -p bin
	$(MAKEINFO) $<
	mv $*.info $@

.PHONY: pdf
pdf: bin/gpp.pdf
bin/%.pdf: obj/%.texinfo obj/fdl.texinfo
	@! test -d obj/pdf || rm -rf obj/pdf
	@mkdir -p bin obj/pdf
	cd obj/pdf && texi2pdf ../../"$<" < /dev/null
	mv obj/pdf/$*.pdf $@

.PHONY: dvi
dvi: bin/gpp.dvi
bin/%.dvi: obj/%.texinfo obj/fdl.texinfo
	@! test -d obj/dvi || rm -rf obj/dvi
	@mkdir -p bin obj/dvi
	cd obj/dvi && $(TEXI2DVI) ../../"$<" < /dev/null
	mv obj/dvi/$*.dvi $@

.PHONY: ps
ps: bin/gpp.ps
bin/%.ps: obj/%.texinfo obj/fdl.texinfo
	@! test -d obj/ps || rm -rf obj/ps
	@mkdir -p bin obj/ps
	cd obj/ps && texi2pdf --ps ../../"$<" < /dev/null
	mv obj/ps/$*.ps $@

obj/gpp.texinfo: doc/info/gpp.texinfo bin/gpp
	@mkdir -p obj
	$(PY) bin/gpp -s '?' -D GPP=$(COMMAND) < "$<" > "$@"

obj/fdl.texinfo: doc/info/fdl.texinfo
	@mkdir -p obj
	cp "$<" "$@"



.PHONY: install
install: install-core install-info install-man

.PHONY: install
install-all: install-core install-doc

.PHONY: install-core
install-core: install-cmd install-license

.PHONY: install-cmd
install-cmd: bin/gpp
	install -dm755 -- "$(DESTDIR)$(BINDIR)"
	install -m755 $< -- "$(DESTDIR)$(BINDIR)/$(COMMAND)"

.PHONY: install-license
install-license:
	install -dm755 -- "$(DESTDIR)$(LICENSEDIR)/$(PKGNAME)"
	install -m644 COPYING LICENSE -- "$(DESTDIR)$(LICENSEDIR)/$(PKGNAME)"

.PHONY: install-doc
install-doc: install-info install-pdf install-dvi install-ps install-man

.PHONY: install-info
install-info: bin/gpp.info
	install -dm755 -- "$(DESTDIR)$(INFODIR)"
	install -m644 $< -- "$(DESTDIR)$(INFODIR)/$(PKGNAME).info"

.PHONY: install-pdf
install-pdf: bin/gpp.pdf
	install -dm755 -- "$(DESTDIR)$(DOCDIR)"
	install -m644 $< -- "$(DESTDIR)$(DOCDIR)/$(PKGNAME).pdf"

.PHONY: install-dvi
install-dvi: bin/gpp.dvi
	install -dm755 -- "$(DESTDIR)$(DOCDIR)"
	install -m644 $< -- "$(DESTDIR)$(DOCDIR)/$(PKGNAME).dvi"

.PHONY: install-ps
install-ps: bin/gpp.ps
	install -dm755 -- "$(DESTDIR)$(DOCDIR)"
	install -m644 $< -- "$(DESTDIR)$(DOCDIR)/$(PKGNAME).ps"

.PHONY: install-man
install-man: doc/man/gpp.1
	install -dm755 -- "$(DESTDIR)$(MAN1DIR)"
	install -m644 $< -- "$(DESTDIR)$(MAN1DIR)/$(COMMAND).1"



.PHONY: uninstall
uninstall:
	-rm -- "$(DESTDIR)$(BINDIR)/$(COMMAND)"
	-rm -- "$(DESTDIR)$(LICENSEDIR)/$(PKGNAME)/COPYING"
	-rm -- "$(DESTDIR)$(LICENSEDIR)/$(PKGNAME)/LICENSE"
	-rmdir -- "$(DESTDIR)$(LICENSEDIR)/$(PKGNAME)"
	-rm -- "$(DESTDIR)$(INFODIR)/$(PKGNAME).info"
	-rm -- "$(DESTDIR)$(DOCDIR)/$(PKGNAME).pdf"
	-rm -- "$(DESTDIR)$(DOCDIR)/$(PKGNAME).dvi"
	-rm -- "$(DESTDIR)$(DOCDIR)/$(PKGNAME).ps"
	-rm -- "$(DESTDIR)$(MAN1DIR)/$(COMMAND).1"



.PHONY: clean
clean:
	-rm -r bin obj

