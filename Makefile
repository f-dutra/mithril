PREFIX  = /usr
X11INC  = /usr/include
X11LIB  = /usr/lib
CAIROINC = /usr/include/cairo
PANGOINCS := $(shell pkg-config --cflags pango pangocairo)
PANGOLIBS := $(shell pkg-config --libs pango pangocairo)
INCS = -I$(X11INC) -I${CAIROINC} ${PANGOINCS}

CPPFLAGS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE=700L -DVERSION=\"${VERSION}\" ${XINERAMAFLAGS}
CFLAGS  = -std=c99 -pedantic -Wall -Wno-deprecated-declarations -Wno-c2y-extensions -Os ${INCS} ${CPPFLAGS}
LDFLAGS = -L$(X11LIB) -lX11 -lXrender -lcairo -lm -lXrandr ${PANGOLIBS}

CC      = cc

SRC = mithril.c config.c drw.c util.c ipc.c
OBJ = ${SRC:.c=.o}

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: config.h drw.h util.h extern.h ipc.h

all: mithril mithrilctl

mithril: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

mithrilc: mithrilc.c
	${CC} -std=c99 -pedantic -Wall -Os -o $@ mithrilc.c

clean:
	rm -f mithril
	rm -f mithrilctl

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m 755 mithril $(DESTDIR)$(PREFIX)/bin/mithril
	install -m 755 mithrilctl $(DESTDIR)$(PREFIX)/bin/mithrilctl

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/mithril
	rm -f $(DESTDIR)$(PREFIX)/bin/mithrilctl

.PHONY: all clean install uninstall
