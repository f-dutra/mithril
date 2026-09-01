PREFIX  = /usr
X11INC  = /usr/include
X11LIB  = /usr/lib
CAIROINC = /usr/include/cairo
PANGOINCS := $(shell pkg-config --cflags pango pangocairo)
PANGOLIBS := $(shell pkg-config --libs pango pangocairo)
INCS = -I$(X11INC) -I${CAIROINC} ${PANGOINCS}

CPPFLAGS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE=700L -DVERSION=\"${VERSION}\" ${XINERAMAFLAGS}
CFLAGS  = -std=c99 -pedantic -Wall -Wno-deprecated-declarations -Os ${INCS} ${CPPFLAGS}
LDFLAGS = -L$(X11LIB) -lX11 -lXrender -lcairo -lm -lXrandr ${PANGOLIBS}

CC      = cc

SRC = mithril.c config.c drw.c util.c
OBJ = ${SRC:.c=.o}

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: config.h drw.h util.h defs.h

all: mithril

mithril: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -f mithril

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m 755 mithril $(DESTDIR)$(PREFIX)/bin/mithril

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/mithril

.PHONY: all clean install uninstall
