# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.  This file is offered as-is,
# without any warranty.

VERSION = 1

PREFIX = /usr
DATA = /share
BIN = /bin
PKGNAME = gpp
PY3 = python3
SHEBANG = /usr$(BIN)/env $(PY3)
COMMAND = gpp
LICENSES = $(DATA)/licenses


.PHONY: all
all: gpp doc

.PHONY: doc
doc: info

.PHONY: info
info: gpp.info.gz

%.info.gz: info/%.texinfo
	makeinfo "$<"
	gzip -9 -f "$*.info"

gpp: src/gpp.py
	VERSION=$(VERSION) SHEBANG="$(SHEBANG)" $(PY3) "$<" -u -u < "$<" > "$@"

.PHONY: install
install: install-core install-doc

.PHONY: install-core
install-core: install-cmd install-license

.PHONY: install-cmd
install-cmd: gpp
	install -dm755 -- "$(DESTDIR)$(PREFIX)$(BIN)"
	install -m755 gpp -- "$(DESTDIR)$(PREFIX)$(BIN)/$(COMMAND)"

.PHONY: install-license
install-license:
	install -dm755 -- "$(DESTDIR)$(PREFIX)$(LICENSES)/$(PKGNAME)"
	install -m644 COPYING LICENSE -- "$(DESTDIR)$(PREFIX)$(LICENSES)/$(PKGNAME)"

.PHONY: install-doc
install-doc: install-info

.PHONY: install-info
install-info: gpp.info.gz
	install -dm755 -- "$(DESTDIR)$(PREFIX)$(DATA)/info"
	install -m644 gpp.info.gz -- "$(DESTDIR)$(PREFIX)$(DATA)/info/$(PKGNAME).info.gz"

.PHONY: uninstall
uninstall:
	-rm -- "$(DESTDIR)$(PREFIX)$(BIN)/$(COMMAND)"
	-rm -- "$(DESTDIR)$(PREFIX)$(LICENSES)/$(PKGNAME)/COPYING"
	-rm -- "$(DESTDIR)$(PREFIX)$(LICENSES)/$(PKGNAME)/LICENSE"
	-rmdir -- "$(DESTDIR)$(PREFIX)$(LICENSES)/$(PKGNAME)"
	-rm -- "$(DESTDIR)$(PREFIX)$(DATA)/info/$(PKGNAME).info.gz"

.PHONY: clean
clean:
	-rm -f gpp gpp.info gpp.info.gz

