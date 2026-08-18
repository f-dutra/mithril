#include <errno.h>
#include <locale.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>
#include <X11/Xresource.h>
#include <X11/extensions/Xrandr.h>

#include "util.h"
#include "drw.h"

#define SESSION_FILE "/tmp/mithril-session"
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
#define INTERSECT(x,y,w,h,m)    (MAX(0, MIN((x)+(w),(m)->wx+(m)->ww) - MAX((x),(m)->wx)) \
                               * MAX(0, MIN((y)+(h),(m)->wy+(m)->wh) - MAX((y),(m)->wy)))

#define TEXTW(x)			  (drw_text_getwidth(font, (x)))
#define TITLEH(c)			  ((c)->hasdecoration && !verticaltitle ? th : 0)
#define TITLEW(c)			  ((c)->hasdecoration && verticaltitle ? th : 0)

#define COFFY(c)			  (inverttitlebar ? 0 : TITLEH(c))
#define COFFX(c)			  (inverttitlebar ? 0 : TITLEW(c))

#define WIDTH(X)                ((X)->w + 2 * (X)->bw)
#define HEIGHT(X)               ((X)->h + 2 * (X)->bw)
#define ISINWS(C)			  ((C)->ws == C->mon->ws || (C)->sticky ? 1 : 0)
#define WSINDEX(M, W)		  (W + workspaces * (M)->num)

enum { ClkTitle, ClkResize, ClkClientWin, ClkRootWin, ClkClose,
	  ClkMax, ClkMin, ClkLast };
enum { MoveLeft, MoveRight, MoveUp, MoveDown };
enum { HvrNone, HvrClose, HvrMax, HvrMin, HvrMenu };
enum { CurNormal, CurMove, CurLast }; /* cursor */
enum { ClrNorm, ClrSel, ClrSpecial, ClrLast };
enum { SchemeNorm, SchemeSel, SchemeCloseNorm, SchemeMaxNorm,
	  SchemeMinNorm, SchemeCloseSel, SchemeMaxSel, SchemeMinSel,
	  SchemeCloseHvr, SchemeMaxHvr, SchemeMinHvr, SchemeLast }; /* colorschemes */
enum { EdgeTop, EdgeBot, EdgeLeft, EdgeRight, EdgeTopLeft,
	  EdgeTopRight, EdgeBotLeft, EdgeBotRight, EdgeNone}; /* window edges */
enum { NetSupported, NetWMName, NetWMState, NetWMCheck,
       NetWMFullscreen, NetActiveWindow, NetWMWindowType,
       NetWMWindowTypeDialog, NetWMWindowTypeNormal,
	  NetWMWindowTypeDock, NetWMWindowTypeDesktop, NetWMWindowTypeToolbar,
	  NetWMWindowTypeMenu, NetWMWindowTypeUtility, NetWMWindowTypeSplash,
	  NetWMWindowTypeDropdownMenu, NetWMWindowTypePopupMenu,
	  NetWMWindowTypeTooltip, NetWMWindowTypeNotification,
	  NetWMWindowTypeCombo, NetWMWindowTypeDnd,
	  NetClientList, NetClientListStacking, NetCloseWindow,
	  NetWMStrutPartial, NetWorkarea, NetWMFrameExtents,
	  NetWMMoveResize, NetWMMaximizedVert, NetWMMaximizedHorz,
	  NetWMHidden, NetNumberOfDesktops, NetCurrentDesktop,
	  NetDesktopNames, NetDesktopViewport, NetWMDesktop, NetLast }; /* EWMH atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus,
	  WMChangeState, WMLast }; /* ICCM atoms */
enum resource_type { STRING, INTEGER, FLOAT };

typedef struct Bar Bar;
typedef struct Client Client;
typedef struct Monitor Monitor;

typedef union {
	int i;
	unsigned int ui;
	float f;
	const void *v;
} Arg;

struct Bar {
	long strut[4];
	Window win;
	Monitor *mon;
	Bar *next;
};

typedef struct {
	unsigned int click;
	unsigned int mask;
	unsigned int button;
	void (*func)(const Arg *arg);
	const Arg arg;
} Button;

struct Client {
	char name[256];

	int x, y, h, w, bw;
	int ox, oy, oh, ow, obw; /* old geometry */

	int bcx, bmxx, bmnx, bmx;
	int bcw, bmxw, bmnw, bmw;
	int bcy, bmxy, bmny, bmy;
	int bch, bmxh, bmnh, bmh;
	int hvr;

	int floating, fullscreen, minimized, maximized, urgent;
	int tmpunmax, tmpunmin, fixed, sticky;

	int hasdecoration;
	int ohasdecoration; /* old decoration hints */
	int ws;

	Surf *srf;

	Client *next, *prev;
	Client *snext;
	Monitor *mon;

	Window frame;
	Window win;
};

typedef struct {
	unsigned int mod;
	KeySym keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Key;

typedef struct {
	const char *symbol;
	void (*arrange)(Monitor *);
} Layout;

typedef struct {
	char *name;
	int lt;
	int gappx;
	float mfact;
	int nmaster;
} Workspace;

struct Monitor {
	int mx, my, mw, mh;
	int wx, wy, ww, wh;
	int owx, owy, oww, owh;

	int num;
	int gappx;
	int ws;

	Client *clients, *sel, *stack;
	Monitor *next, *prev;

	Workspace *wsdata;
};

typedef struct {
	char *name;
	enum resource_type type;
	void *dst;
} ResourcePref;

static void arrange(Monitor *m);
static void attach(Client *c);
static void attachbar(Bar *b);
static void attachstack(Client *c);
static void buttonpress(XEvent *e);
static void cleanup(void);
static void cleanupmon(Monitor *mon);
static void clientmessage(XEvent *e);
static void closesel(const Arg *arg);
static void configurerequest(XEvent *e);
static Monitor *createmon(void);
static void destroynotify(XEvent *e);
static void detach(Client *c);
static void detachbar(Bar *b);
static void detachstack(Client *c);
static Monitor *dirtomon(int dir);
static void drawdecorations(Client *c, int border);
static void drawtitlebar(Client *c);
static void enternotify(XEvent *e);
static void expose(XEvent *e);
static void frame(Client *c);
static void focus(Client *c);
static void focusin(XEvent *e);
static void focusmon(const Arg *arg);
static void focusstack(const Arg *arg);
static void unfocus(Client *c, int setfocus);
static Atom getatomprop(Window w, Atom prop);
static int  getedge(Client *c, int x, int y);
static int  getrootptr(int *x, int *y);
static long getstate(Window w);
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void grabbuttons(Client *c, int focused);
static void grabkeys(void);
static void incmfact(const Arg *arg);
static void incnmaster(const Arg *arg);
static void keypress(XEvent *e);
static void killclient(Client *c);
static Client *lasttiled(Monitor *m);
static void leavenotify(XEvent *e);
static void load_xresources(void);
static void mapclient(Client *c);
static void manage(Window w, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void maximize(Client *c);
static void minimize(Client *c, int hide);
static void motionnotify(XEvent *e);
static void movekeyboard(const Arg *arg);
static void movemouse(const Arg *arg);
static Client *nexttiled(Client *c);
Monitor *numtomon(int num);
static Client *prevtiled(Client *c);
static void propertynotify(XEvent *e);
static void quit(const Arg *arg);
static Monitor *recttomon(int x, int y, int w, int h);
static void resize(Client *c, int x, int y, int w, int h);
static void resizeclamped(Client *c, int x, int y, int w, int h);
static void resizekeyboard(const Arg *arg);
static void resizemouse(const Arg *arg);
static void resource_load(XrmDatabase db, char *name, enum resource_type rtype, void *dst);
static void restack(Monitor *m);
static void restoresession(void);
static void run(void);
static void savesession(void);
static void scan(void);
static void sendclient(Client *c, Monitor *m, int ws, int warp);
static int sendevent(Client *c, Atom proto);
static void sendtows(const Arg *arg);
static void sendtomon(const Arg *arg);
static void setup(void);
static void setclientdesktop(Client *c);
static void setclientstate(Client *c);
static void setfocus(Client *c);
static void setfocusmon(Monitor *m, int warp);
static void setfullscreen(Client *c, int fullscreen);
static void setlayout(const Arg *arg);
static void setwmstate(Window w, long state);
static void showhide(Client *c);
static void sighup(int unused);
static void sigterm(int unused);
static void spawn(const Arg *arg);
static void swapclients(Client *c1, Client *c2);
static void swapmouse(const Arg *arg);
static void swaptiled(const Arg *arg);
static void tile(Monitor *m);
static void togglefloating(const Arg *arg);
static void togglefullscr(const Arg *arg);
static void togglemaximize(const Arg *arg);
static void toggleminimize(const Arg *arg);
static void togglesticky(const Arg *arg);
static void unframe(Client *c, int destroyed);
static void unmanage(Client *c, int destroyed);
static void unmapnotify(XEvent *e);
static void unmapclient(Client *c);
static void unmaximize(Client *c, int x, int y, int tmp);
static void updateclientlist(void);
static void updateclientliststacking(void);
static void updatecurrentdesktop(void);
static void updatedesktops(void);
static int  updatemons(void);
static void updatemotifhints(Client *c);
static void updatenumlockmask(void);
static void updatesizehints(Client *c);
static void updatestrut(void);
static void updatetitle(Client *c);
static void updatewindowtype(Client *c);
static void updateviewport(void);
static void view(const Arg *arg);
static Bar *wintobar(Window w);
static Client *wintoclient(Window w);
static Monitor *wintomon(Window w);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);

static unsigned int numlockmask = 0;
static int running = 1;
static int restart = 0;
static int screen, sh, sw;
static int th;
static Fnt *font, *icons;

static Cursor cursor[CurLast];
static Cursor curresize[EdgeNone];
static double scheme[SchemeLast][3][4];
static unsigned long borders[3];

static Bar *bars;
static Monitor *mons, *selmon;
static int nmons = 0;

static Colormap cmap;
static int depth;
static Display *dpy;
static Visual *visual;
static Window root, wmcheckwin;

static Atom utf8string, motifatom;
static Atom wmatom[WMLast], netatom[NetLast];
static int (*xerrorxlib)(Display *, XErrorEvent *);
static int xrandr_evbase, xrandr_errbase;
static int hasxrandr;

void (*handler[LASTEvent]) (XEvent *) = {
	[ButtonPress] = buttonpress,
	[ClientMessage] = clientmessage,
	[ConfigureRequest] = configurerequest,
	[DestroyNotify] = destroynotify,
	[EnterNotify] = enternotify,
	[Expose] = expose,
	[FocusIn] = focusin,
	[KeyPress] = keypress,
	[LeaveNotify] = leavenotify,
	[MappingNotify] = mappingnotify,
	[MapRequest] = maprequest,
	[MotionNotify] = motionnotify,
	[PropertyNotify] = propertynotify,
	[UnmapNotify] = unmapnotify
};

/* uncluding it here allows it to acces the code above */
#include "config.h"

/* implementations */
void
arrange(Monitor *m)
{
	if (m)
		showhide(m->stack);
	else for (m = mons; m; m = m->next)
		showhide(m->stack);
	if (m) {
		if(layouts[m->wsdata[m->ws].lt].arrange)
			layouts[m->wsdata[m->ws].lt].arrange(m);
		restack(m);
	} else for (m = mons; m; m = m->next) {
		if(layouts[m->wsdata[m->ws].lt].arrange)
			layouts[m->wsdata[m->ws].lt].arrange(m);
	}
}

void
attach(Client *c)
{
	c->next = c->mon->clients;
	c->prev = NULL;
	if (c->mon->clients)
		c->mon->clients->prev = c;
	c->mon->clients = c;
}

void
attachbar(Bar *b)
{
	b->next = bars;
	bars = b;
}

void
attachstack(Client *c)
{
	c->snext = c->mon->stack;
	c->mon->stack = c;
}

void
buttonpress(XEvent *e)
{
	static Time lasttime = 0;
	static Window lastwin = None;
	static int lastbtn, lastx, lasty;
	unsigned int i, click = ClkLast;
	Arg arg;
	Client *c;
	Monitor *m;
	XButtonPressedEvent *ev = &e->xbutton;

	if ((m = wintomon(ev->window)) && m != selmon)
		setfocusmon(m, 0);
	if ((c = wintoclient(ev->window))) {
		if (c != selmon->sel)
			focus(c);
		XAllowEvents(dpy, ReplayPointer, CurrentTime);
		if (ev->window == c->win) {
			click = ClkClientWin;
		} else if (ev->window == c->frame && c->hasdecoration) {
			arg.i = getedge(c, ev->x, ev->y);
			if (arg.i != EdgeNone) {
				click = ClkResize;
			} else if (ev->x >= c->bcx && ev->x < c->bcx + c->bcw && ev->y >= c->bcy && ev->y < c->bcy + c->bch) {
				click = ClkClose;
			} else if (ev->x >= c->bmxx && ev->x < c->bmxx + c->bmxw && ev->y >= c->bmxy && ev->y < c->bmxy + c->bmxh) {
				click = ClkMax;
			} else if (ev->x >= c->bmnx && ev->x < c->bmnx + c->bmnw && ev->y >= c->bmny && ev->y < c->bmny + c->bmnh) {
				click = ClkMin;
			} else {
				if (ev->time - lasttime <= 250 &&
					lastwin == ev->window &&
					lastbtn == ev->button &&
					lastx == ev->x &&
					lasty == ev->y &&
					ev->button == Button1)
					click = ClkMax;
				else
					click = ClkTitle;
				lasttime = ev->time;
				lastwin = ev->window;
				lastbtn = ev->button;
				lastx = ev->x;
				lasty = ev->y;
			}
		}
	}
	for (i = 0; i < LENGTH(buttons); i++)
		if (click == buttons[i].click && buttons[i].func && buttons[i].button == ev->button
		&& CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
			buttons[i].func(click == ClkResize ? &arg : &buttons[i].arg);
}

void
cleanup(void)
{
	Arg a = {.i = 0};
	Monitor *m;
	size_t i;

	view(&a);
	if (restart)
		savesession();
	for (m = mons; m; m = m->next)
		while (m->stack)
			unmanage(m->stack, 0);
	while (mons)
		cleanupmon(mons);
	for (i = 0; i < CurLast; i++)
		XFreeCursor(dpy, cursor[i]);
	for (i = 0; i < EdgeNone; i++)
		XFreeCursor(dpy, curresize[i]);

	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	XDestroyWindow(dpy, wmcheckwin);
	XSync(dpy, False);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	free(font);
}

void
cleanupmon(Monitor *mon)
{
	Monitor *m;

	if (mon == mons)
		mons = mons->next;
	else {
		for (m = mons; m && m->next != mon; m = m->next);
		m->next = mon->next;
	}
	free(mon->wsdata);
	free(mon);
}

void
clientmessage(XEvent *e)
{
	static const int dirtoedge[8] = {
		EdgeTopLeft, EdgeTop, EdgeTopRight, EdgeRight,
		EdgeBotRight, EdgeBot, EdgeBotLeft, EdgeLeft
	};
	XClientMessageEvent *cme = &e->xclient;
	Arg arg;
	Client *c;

	if (cme->message_type == netatom[NetCurrentDesktop]) {
		arg.i = (int)cme->data.l[0] % workspaces;
		setfocusmon(numtomon((int)cme->data.l[0] / workspaces), 1);
		view(&arg);
		return;
	}

	if (!(c = wintoclient(cme->window)))
		return;
	if (cme->message_type == netatom[NetWMMoveResize]) {
		arg.i = dirtoedge[cme->data.l[2]];

		if (cme->data.l[2] == 8) /* _NET_WM_MOVERESIZE_MOVE */
			movemouse(&arg);
		else if (cme->data.l[2] >= 0 && cme->data.l[2] <= 7) /* SIZE_* */
			resizemouse(&arg);
	} else if (cme->message_type == netatom[NetWMState]) {
		if (cme->data.l[1] == netatom[NetWMFullscreen]
		|| cme->data.l[2] == netatom[NetWMFullscreen])
			setfullscreen(c, (cme->data.l[0] == 1
				|| (cme->data.l[0] == 2 && !c->fullscreen)));
		if (cme->data.l[1] == netatom[NetWMMaximizedVert] ||
		    cme->data.l[1] == netatom[NetWMMaximizedHorz] ||
		    cme->data.l[2] == netatom[NetWMMaximizedVert] ||
		    cme->data.l[2] == netatom[NetWMMaximizedHorz]) {
			if (cme->data.l[0] == 0)
				unmaximize(c, c->ox, c->oy, 0);
			else if (cme->data.l[0] == 1)
				maximize(c);
			else if (cme->data.l[0] == 2)
				c->maximized ? unmaximize(c, c->ox, c->oy, 0) : maximize (c);
		}
	} else if (cme->message_type == wmatom[WMChangeState]) {
    		if (cme->data.l[0] == IconicState && !c->minimized)
        		minimize(c, 1);
	} else if (cme->message_type == netatom[NetActiveWindow]) {
		if (c->minimized)
			minimize(c, 0);
		focus(c);
	} else if (cme->message_type == netatom[NetCloseWindow]) {
		killclient(c);
	}
}

void
closesel(const Arg *arg)
{
	killclient(selmon->sel);
}

void
configure(Client *c)
{
	XConfigureEvent ce;

	ce.type = ConfigureNotify;
	ce.display = dpy;
	ce.event = c->win;
	ce.window = c->win;
	ce.x = c->x + TITLEW(c);
	ce.y = c->y + TITLEH(c);
	ce.width = c->w - TITLEW(c);
	ce.height = c->h - TITLEH(c);
	ce.border_width = 0;
	ce.above = None;
	ce.override_redirect = False;
	XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void
configurerequest(XEvent *e)
{
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;
	Client *c;

	if ((c = wintoclient(ev->window))) {
		if (ev->value_mask & (CWWidth | CWHeight))
			resizeclamped(c, c->x, c->y,
				(ev->value_mask & CWWidth)  ? ev->width  + TITLEW(c): c->w,
				(ev->value_mask & CWHeight) ? ev->height + TITLEH(c) : c->h);
		configure(c);
		return;
	}

	wc.x = ev->x;
	wc.y = ev->y;
	wc.width = ev->width;
	wc.height = ev->height;
	wc.border_width = ev->border_width;
	wc.sibling = ev->above;
	wc.stack_mode = ev->detail;
	XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
	XSync(dpy, False);
}

Monitor *
createmon(void)
{
	Monitor *m;

	m = ecalloc(1, sizeof(Monitor));
	m->ws = 0;

	if (workspaces > LENGTH(workspace_rules))
		workspaces = LENGTH(workspace_rules);

	m->wsdata = ecalloc(1, sizeof(Workspace) * workspaces);
	memcpy(m->wsdata, workspace_rules, workspaces * sizeof(Workspace));

	return m;
}

void
destroynotify(XEvent *e)
{
	Bar *b;
	Client *c;
	XDestroyWindowEvent *ev = &e->xdestroywindow;

	if ((c = wintoclient(ev->window))) {
		unmanage(c, 1);
	} else if ((b = wintobar(ev->window))) {
		detachbar(b);
		free(b);
		updatestrut();
	} else {
		updatestrut();
	}
}

void
detach(Client *c)
{
	if (c->prev)
		c->prev->next = c->next;
	else
		c->mon->clients = c->next;
	if (c->next)
		c->next->prev = c->prev;
	c->next = c->prev = NULL;
}

void
detachbar(Bar *b)
{
	Bar **tb;

	for (tb = &bars; *tb && *tb != b; tb = &(*tb)->next);
	*tb = b->next;
}

void
detachstack(Client *c)
{
	Client **tc, *t;

	for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext);
	*tc = c->snext;

	if (c == c->mon->sel) {
		for (t = c->mon->stack; t && (!ISINWS(t) || t->minimized); t = t->snext);
		c->mon->sel = t;
	}
}

Monitor *
dirtomon(int dir)
{
	Monitor *m = NULL;

	if (dir > 0) {
		if (!(m = selmon->next))
			m = mons;
	} else if (selmon == mons)
		for (m = mons; m->next; m = m->next);
	else
		for (m = mons; m->next != selmon; m = m->next);
	return m;
}

void
drawdecorations(Client *c, int border)
{
	if (!c)
		return;
	if (c->hasdecoration && border)
		XSetWindowBorder(dpy, c->frame, borders[c == selmon->sel]);
	if (c->hasdecoration)
		drawtitlebar(c);
}

void
drawtitlebar(Client *c)
{
	char name[256];
	int bw, bh, tx, ty;
	int sel = c == selmon->sel;
	int barx = inverttitlebar && verticaltitle ? c->w - th : 0;
	int bary = inverttitlebar && !verticaltitle ? c->h - th : 0;
	Drw *drw;

	if(!c->hasdecoration)
		return;

	drw = drw_create(c->srf);
	drw_set_scheme(drw, scheme[sel ? SchemeSel : SchemeNorm]);
	drw_rect(drw, 0, 0, c->w, c->h, 0, 1, 0);

	c->bcw = c->bmnw = c->bmxw = buttonwidth + lrpad;
	c->bch = c->bmnh = c->bmxh = buttonheight + lrpad;

	bw = c->bcw + c->bmnw + c->bmxw + outerpad;
	bh = c->bch + c->bmnh + c->bmxh + outerpad;

	drw_set_font(drw, font);
	strcpy(name, c->name);
	if (verticaltitle)
		drw_text_clamp(drw, name, c->h - (bh + lrpad) * 2, sizeof(name));
	else
		drw_text_clamp(drw, name, c->w - (bw + lrpad) * 2, sizeof(name));

	/* calculate button geometry */
	if (verticaltitle) {
		if(centeredtitle)
			ty = (c->h - TEXTW(name)) / 2;
		else
			ty = (invertbuttons ? outerpad : bh) + lrpad / 2;
		if (inverttitlebar) {
			tx = barx + (int)round((th - titleborderpx - (font->ascent + font->descent)) / 2.0) + titleborderpx - offset_y;
			drw_rect(drw, barx + titleborderpx  - titleborderpx / 2, 0, 0, c->h, 0, 0, titleborderpx);
			c->bcx = c->bmxx = c->bmnx = barx + titleborderpx + (int)round((th - titleborderpx - buttonwidth) / 2 + offset_y) - lrpad / 2;
		} else {
			tx = barx + (int)round((th - titleborderpx - (font->ascent + font->descent)) / 2.0) + offset_y;
			drw_rect(drw, barx + th - titleborderpx + titleborderpx / 2, 0, 0, c->h, 0, 0, titleborderpx);
			c->bcx = c->bmxx = c->bmnx = barx + (int)round((th - titleborderpx - buttonwidth) / 2 + offset_y) - lrpad / 2;
		}
		if (invertbuttons) {
			c->bcy = outerpad;
			c->bmxy = c->bcy + c->bch;
			c->bmny = c->bmxy + c->bmxh;
		} else {
			c->bcy = c->h - c->bch - outerpad;
			c->bmxy = c->bcy - c->bch;
			c->bmny = c->bmxy - c->bmxh;
		}
	} else {
		if(centeredtitle)
			tx = (c->w - TEXTW(name)) / 2;
		else
			tx = (invertbuttons ? outerpad : bw) + lrpad / 2;
		if (inverttitlebar) {
			ty = bary + (int)round((th - titleborderpx - (font->ascent + font->descent)) / 2.0) + titleborderpx - offset_y;
			drw_rect(drw, 0, bary + titleborderpx - titleborderpx / 2, c->w, 0, 0, 0, titleborderpx);
			c->bcy = c->bmxy = c->bmny = bary + titleborderpx + (int)round((th - titleborderpx - buttonheight) / 2 + offset_y) - lrpad / 2;
		} else {
			ty = bary + (int)round((th - titleborderpx - (font->ascent + font->descent)) / 2.0) + offset_y;
			drw_rect(drw, 0, bary + th - titleborderpx + titleborderpx / 2, c->w, 0, 0, 0, titleborderpx);
			c->bcy = c->bmxy = c->bmny = bary + (int)round((th - titleborderpx - buttonheight) / 2 + offset_y) - lrpad / 2;
		}
		if (invertbuttons) {
			c->bcx = outerpad;
			c->bmxx = c->bcx + c->bcw;
			c->bmnx = c->bmxx + c->bmxw;
		} else {
			c->bcx = c->w - c->bcw - outerpad;
			c->bmxx = c->bcx - c->bmnw;
			c->bmnx = c->bmxx - c->bmnw;
		}
	}

	drw_text(drw, name, tx, ty, verticaltitle);
	drw_set_font(drw, icons);
	drw_set_scheme(drw, scheme[c->hvr == HvrClose ? SchemeCloseHvr : sel ? SchemeCloseSel : SchemeCloseNorm]);
	drw_button(drw, btn_close_icn, c->bcx + lrpad/2, c->bcy + lrpad/2, c->bcw - lrpad, c->bch - lrpad, buttonradius, buttonborderpx);

	drw_set_scheme(drw, scheme[c->hvr == HvrMin ? SchemeMinHvr : sel ? SchemeMinSel : SchemeMinNorm]);
	drw_button(drw, btn_minimize_icn, c->bmnx + lrpad/2, c->bmny + lrpad / 2, c->bmnw - lrpad, c->bmnh - lrpad, buttonradius, buttonborderpx);

	drw_set_scheme(drw, scheme[c->hvr == HvrMax ? SchemeMaxHvr : sel ? SchemeMaxSel : SchemeMaxNorm]);
	drw_button(drw, btn_maximize_icn, c->bmxx + lrpad/2, c->bmxy + lrpad / 2, c->bmxw - lrpad, c->bmxh - lrpad, buttonradius, buttonborderpx);

	drw_destroy(drw);
}

void
enternotify(XEvent *e)
{
	Client *c;
	Monitor *m;
	XCrossingEvent *ev = &e->xcrossing;
	int edge;

	if (ev->mode != NotifyNormal || ev->detail == NotifyInferior)
		return;
	if ((m = wintomon(ev->window)) && m != selmon)
		setfocusmon(m, 0);
	if (!(c = wintoclient(ev->window)) || ev->window != c->frame)
		return;

	edge = getedge(c, ev->x, ev->y);

	XDefineCursor(dpy, c->frame, edge == EdgeNone || c->maximized ?
		cursor[CurNormal] : curresize[edge]);
}

void
expose(XEvent *e)
{
	Client *c;
	XExposeEvent *ev = &e->xexpose;

	/* only redraw on the last expose in a sequence */
	if (ev->count == 0 && (c = wintoclient(ev->window)))
		drawdecorations(c, 1);
}

void
focus(Client *c)
{
	if (!c || !ISINWS(c) || c->minimized)
		for (c = selmon->stack; c && (!ISINWS(c) || c->minimized); c = c->snext);
	if (selmon->sel && selmon->sel != c)
		unfocus(selmon->sel, 0);
	if (c) {
		detachstack(c);
		attachstack(c);
		grabbuttons(c, 1);
		XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
		updateclientliststacking();
		setfocus(c);
	} else {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
	selmon->sel = c;
	drawdecorations(c, 1);
	restack(selmon);
}

void
focusin(XEvent *e)
{
	XFocusChangeEvent *ev = &e->xfocus;

	if (selmon->sel && ev->window != selmon->sel->win)
		setfocus(selmon->sel);
}

void
focusmon(const Arg *arg)
{
	Monitor *m;

	if (!mons->next)
		return;
	if ((m = dirtomon(arg->i)) == selmon)
		return;
	setfocusmon(m, 1);
}

void
focusstack(const Arg *arg)
{
	Client *c;

	if(!(c = selmon->sel))
		return;

	do {
		if (arg->i > 0) {
			c = c->next ? c->next : selmon->clients;
		} else {
			c = c->prev;
			if (!c)
				for (c = selmon->clients; c->next; c = c->next);
		}
	} while ((c->minimized || !ISINWS(c)) && c != selmon->sel);

	if (!c->minimized)
		focus(c);
}

void
frame(Client *c)
{
	XSetWindowAttributes wa = {
		.background_pixel = 0x00000000,
		.border_pixel = 0,
		.colormap = cmap,
		.override_redirect = False,
		.backing_store = WhenMapped, /* cache the titlebar pixels */
	};

	c->frame = XCreateWindow(dpy, root, c->x, c->y, c->w, c->h, 0,
                       depth, InputOutput, visual,
                       CWBackPixel|CWOverrideRedirect|CWBackingStore|CWBorderPixel|CWColormap, &wa);

	XReparentWindow(dpy, c->win, c->frame, COFFX(c), COFFY(c));
	XSelectInput(dpy, c->frame, SubstructureRedirectMask|SubstructureNotifyMask|ButtonPressMask|
			   ExposureMask|EnterWindowMask|PointerMotionMask|LeaveWindowMask);
	XChangeProperty(dpy, c->frame, netatom[NetWMWindowType], XA_CARDINAL, 32,
		PropModeReplace, (unsigned char *)&netatom[NetWMWindowTypeNormal], 1);

	c->srf = drw_surf_create(dpy, c->frame, visual, c->w, c->h);
	drw_resize(c->srf, c->w, c->h);
}

Atom
getatomprop(Window w, Atom prop)
{
	int format;
	unsigned long nitems, dl;
	unsigned char *p = NULL;
	Atom da, atom = None;

	if (XGetWindowProperty(dpy, w, prop, 0L, sizeof atom, False, XA_ATOM,
		&da, &format, &nitems, &dl, &p) == Success && p) {
		if (nitems > 0 && format == 32)
			atom = *(long *)p;
		XFree(p);
	}
	return atom;
}

int
getedge(Client *c, int x, int y)
{
	int fw = c->w + c->bw;
	int fh = c->h + c->bw;

	if (!c->bw)
		return EdgeNone;

	if (x <= c->bw + 4 && y <= c->bw + 4)
		return EdgeTopLeft;
	else if (x >= fw - (c->bw + 4) && y <= c->bw + 4)
		return EdgeTopRight;
	else if (y <= c->bw)
		return EdgeTop;
	else if ((x >= fw - (c->bw + 4) && y >= fh - c->bw) || (x >= fw - c->bw && y >= fh - (c->bw + 4)))
		return EdgeBotRight;
	else if ((x <= c->bw + 4 && y >= fh - c->bw) || (x <= c->bw && y >= fh - (c->bw + 4)))
		return EdgeBotLeft;
	else if (y >= fh - c->bw)
		return EdgeBot;
	else if (x <= c->bw)
		return EdgeLeft;
	else if (x >= fw - c->bw)
		return EdgeRight;
	else
		return EdgeNone;
}

int
getrootptr(int *x, int *y)
{
	int di;
	unsigned int dui;
	Window dummy;

	return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

long
getstate(Window w)
{
	int format;
	long result = -1;
	unsigned char *p = NULL;
	unsigned long n, extra;
	Atom real;

	if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
		&real, &format, &n, &extra, &p) != Success)
		return -1;
	if (n != 0 && format == 32)
		result = *(long *)p;
	XFree(p);
	return result;
}

int
gettextprop(Window w, Atom atom, char *text, unsigned int size)
{
	char **list = NULL;
	int n;
	XTextProperty name;

	if (!text || size == 0)
		return 0;
	text[0] = '\0';
	if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems)
		return 0;
	if (name.encoding == XA_STRING || name.encoding == utf8string) {
		strncpy(text, (char *)name.value, size - 1);
	} else if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
		strncpy(text, *list, size - 1);
		XFreeStringList(list);
	}
	text[size - 1] = '\0';
	utf8truncate(text); /* guard against cut character bytes */
	XFree(name.value);
	return 1;
}

void
grabbuttons(Client *c, int focused)
{
	updatenumlockmask();
	unsigned int i, j;
	unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
	XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
	if (!focused)
		XGrabButton(dpy, AnyButton, AnyModifier, c->win, False,
			BUTTONMASK, GrabModeAsync, GrabModeAsync, None, None);
	for (i = 0; i < LENGTH(buttons); i++)
		if (buttons[i].click == ClkClientWin)
			for (j = 0; j < LENGTH(modifiers); j++)
				XGrabButton(dpy, buttons[i].button,
					buttons[i].mask | modifiers[j],
					c->win, False, BUTTONMASK,
					GrabModeAsync, GrabModeSync, None, None);
}

void
grabkeys(void)
{
	updatenumlockmask();
	unsigned int i, j, k;
	unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
	int start, end, skip;
	KeySym *syms;

	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	XDisplayKeycodes(dpy, &start, &end);
	syms = XGetKeyboardMapping(dpy, start, end - start + 1, &skip);
	if (!syms)
		return;
	for (k = start; k <= end; k++)
		for (i = 0; i < LENGTH(keys); i++)
			/* skip modifier codes, we do that ourselves */
			if (keys[i].keysym == syms[(k - start) * skip])
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabKey(dpy, k,
						 keys[i].mod | modifiers[j],
						 root, True,
						 GrabModeAsync, GrabModeAsync);
	XFree(syms);
}

void
incmfact(const Arg *arg)
{
	float f;

	if (!arg || !layouts[selmon->ws].arrange)
		return;
	f = arg->f < 1.0 ? arg->f + selmon->wsdata[selmon->ws].mfact : arg->f - 1.0;
	if (f < 0.05 || f > 0.95)
		return;
	selmon->wsdata[selmon->ws].mfact = f;
	arrange(selmon);
}

void
incnmaster(const Arg *arg)
{
	selmon->wsdata[selmon->ws].nmaster = MAX(selmon->wsdata[selmon->ws].nmaster + arg->i, 0);
	arrange(selmon);
}

void
keypress(XEvent *e)
{
	unsigned int i;
	KeySym keysym;
	XKeyEvent *ev = &e->xkey;

	keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);
	for (i = 0; i < LENGTH(keys); i++)
		if (keysym == keys[i].keysym
		&& CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
		&& keys[i].func)
			keys[i].func(&(keys[i].arg));
}

void
killclient(Client *c)
{
	if(!c)
		return;
	if (!sendevent(c, wmatom[WMDelete])) {
		XGrabServer(dpy);
		XSetErrorHandler(xerrordummy);
		XSetCloseDownMode(dpy, DestroyAll);
		XKillClient(dpy, c->win);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
}

Client *
lasttiled(Monitor *m)
{
	Client *c, *last = NULL;

	for (c = m->clients; c; c = c->next)
		if (!c->floating && !c->maximized && !c->minimized && !c->fullscreen && ISINWS(c))
			last = c;
	return last;
}

void
leavenotify(XEvent *e)
{
	Client *c;
	XCrossingEvent *ev = &e->xcrossing;

	if (!(c = wintoclient(ev->window)))
		return;
	if(ev->window == c->frame && c->hvr != HvrNone){
		c->hvr = HvrNone;
		drawdecorations(c, 0);
	}

	if (ev->window == c->frame || ev->window == c->win)
		XUndefineCursor(dpy, ev->window);
}

void
resource_load(XrmDatabase db, char *name, enum resource_type rtype, void *dst)
{
	int *idst = dst;
	float *fdst = dst;
	char *sdst = dst;
	char fullname[256];
	char *type;
	XrmValue ret;

	snprintf(fullname, sizeof(fullname), "%s.%s", "mithril", name);
	fullname[sizeof(fullname) - 1] = '\0';
	/* get resources that start with '*.' and 'mithril.' */
	XrmGetResource(db, fullname, "*", &type, &ret);
	if (!(ret.addr == NULL || strncmp("String", type, 64))) {
		if (rtype == STRING)
			strcpy(sdst, ret.addr);
		else if (rtype == INTEGER)
			*idst = strtoul(ret.addr, NULL, 10);
		else if (rtype == FLOAT)
			*fdst = strtof(ret.addr, NULL);
	}
}

void
load_xresources(void)
{
	Display *display;
	char *resm;
	XrmDatabase db;
	ResourcePref *p;

	display = XOpenDisplay(NULL);
	resm = XResourceManagerString(display);
	if (!resm)
		return;

	db = XrmGetStringDatabase(resm);
	for (p = resources; p < resources + LENGTH(resources); p++)
		resource_load(db, p->name, p->type, p->dst);
	XCloseDisplay(display);
}

void
mapclient(Client *c)
{
	if (!c) return;

	setwmstate(c->win, NormalState);
	XMapWindow(dpy, c->frame);
	XMapWindow(dpy, c->win);
}

void
manage(Window w, XWindowAttributes *wa)
{
	Bar *b;
	Client *c, *t;
	Window trans = None;
	XWindowChanges wc;

	if (getatomprop(w, netatom[NetWMWindowType]) == netatom[NetWMWindowTypeDock]) {
			b = ecalloc(1, sizeof(Bar));
			b->win = w;
			b->mon = recttomon(wa->x, wa->y, wa->height, wa->width);
			attachbar(b);
			XSelectInput(dpy, w, StructureNotifyMask|PropertyChangeMask);
			XMapWindow(dpy, w);
			updatestrut();
			arrange(b->mon);
			return;
	}

	c = ecalloc(1, sizeof(Client));
	c->win = w;
	c->hasdecoration = 1;
	c->x = c->ox = wa->x;
	c->y = c->oy = wa->y;
	c->h = c->oh = wa->height + TITLEH(c);
	c->w = c->ow = wa->width + TITLEW(c);
	c->obw = wa->border_width;

	if (XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans))) {
		c->mon = t->mon;
		c->ws = t->ws;
	} else {
		c->mon = selmon;
		c->ws = c->mon->ws;
	}

	XSelectInput(dpy, w, EnterWindowMask|FocusChangeMask|PropertyChangeMask
					|StructureNotifyMask|ExposureMask);

	frame(c);
	updatetitle(c);
	updatesizehints(c);
	updatemotifhints(c);
	grabbuttons(c, 0);

	c->bw = c->hasdecoration ? borderpx : 0;
	wc.border_width = c->bw;
	XConfigureWindow(dpy, c->frame, CWBorderWidth, &wc);

	if (!c->floating && (trans || c->fixed))
		c->floating = 1;
	if (c->floating)
		XRaiseWindow(dpy, c->win);
	attach(c);
	attachstack(c);
	setclientdesktop(c);
	updatewindowtype(c);
	XAddToSaveSet(dpy, c->win);
	if(ISINWS(c))
		mapclient(c);
	arrange(c->mon);
	if (c->floating || !layouts[c->mon->wsdata[c->mon->ws].lt].arrange)
		resizeclamped(c, c->mon->wx + (c->mon->ww - c->w)/2, c->mon->wy + (c->mon->wh - c->h)/2, c->w, c->h);
	updateclientlist();
	focus(c);
}

void
mappingnotify(XEvent *e)
{
	XMappingEvent *ev = &e->xmapping;

	XRefreshKeyboardMapping(ev);
	if (ev->request == MappingKeyboard)
		grabkeys();
}

void
maprequest(XEvent *e)
{
	static XWindowAttributes wa;
	XMapRequestEvent *ev = &e->xmaprequest;

	if (!XGetWindowAttributes(dpy, ev->window, &wa) || wa.override_redirect)
		return;
	if (!wintoclient(ev->window))
		manage(ev->window, &wa);
}

void
maximize(Client *c)
{
	if(!c || c->maximized)
		return;

	c->obw = c->bw;
	c->bw = 0;
	XSetWindowBorderWidth(dpy, c->frame, c->bw);
	resize(c, c->mon->wx, c->mon->wy, c->mon->ww - 2 * c->bw, c->mon->wh - 2 * c->bw);
	c->maximized = 1;
	c->tmpunmax = 0;
	setclientstate(c);
	arrange(c->mon);
}

void
minimize(Client *c, int hide)
{
	if (!c) return;

	c->minimized = hide;
	hide ? unmapclient(c) : mapclient(c);
	setclientstate(c);
	arrange(c->mon);
	focus(NULL);
}

void
motionnotify(XEvent *e)
{
	static Window lastwin = None;
	static int lastedge = EdgeNone;
	int edge, hvr;
	XMotionEvent *ev = &e->xmotion;
	Client *c;
	Monitor *m;

	if (ev->window == root) {
		if((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != selmon)
			setfocusmon(m, 0);
	} else if ((c = wintoclient(ev->window)) && ev->window == c->frame) {
		edge = getedge(c, ev->x, ev->y);
		if (c->hasdecoration) {
			if (ev->y < th - titleborderpx && ev->y > c->bw) {
				if(edge != EdgeNone)
					hvr = HvrNone;
				else if(ev->x >= c->bcx && ev->x < c->bcx + c->bcw)
					hvr = HvrClose;
				else if(ev->x >= c->bmxx && ev->x < c->bmxx + c->bmxw)
					hvr = HvrMax;
				else if(ev->x >= c->bmnx && ev->x < c->bmnx + c->bmnw)
					hvr = HvrMin;
				else
					hvr = HvrNone;
			} else { hvr = HvrNone; }

			if (c->hvr != hvr) {
				c->hvr = hvr;
				drawdecorations(c, 0);
			}
		}
		if (ev->window == lastwin && edge == lastedge)
			return;
		lastwin  = ev->window;
		lastedge = edge;

		XDefineCursor(dpy, c->frame, edge == EdgeNone || c->maximized ?
			cursor[CurNormal] : curresize[edge]);
	}
}

int /* this is needed because there is no client message mask */
moveresize_eventmask(Display *dpy, XEvent *ev, XPointer arg)
{
	switch (ev->type) {
	case ButtonPress:
	case ButtonRelease:
	case MotionNotify:
	case Expose:
	case ConfigureRequest:
	case MapRequest:
	case ClientMessage:
		return 1;
	default:
		return 0;
	}
}

void
movekeyboard(const Arg *arg)
{
	int nx, ny;
	Client *c;

	if (!(c = selmon->sel) || c->fullscreen)
		return;
	if (!c->floating)
		togglefloating(NULL);

	nx = c->x;
	ny = c->y;

	switch (arg->i) {
	case MoveLeft:  nx -= movestep; break;
	case MoveRight: nx += movestep; break;
	case MoveUp:    ny -= movestep; break;
	case MoveDown:  ny += movestep; break;
	}

	if (c->maximized && (nx != c->x || ny != c->y))
		unmaximize(c, nx, ny, 0);
	else
		resizeclamped(c, nx, ny, c->w, c->h);
}

void
movemouse(const Arg *arg)
{
	int x, y, ocx, ocy, nx, ny;
	XEvent ev;
	Time lasttime = 0;
	Client *c;
	Monitor *m = NULL;

	if (!(c = selmon->sel) || c->fullscreen)
		return;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, None, CurrentTime) != GrabSuccess)
		return;
	if (!getrootptr(&x, &y))
		return;

	ocx = c->maximized && x > c->x + c->ow / 2 ? x - c->ow / 2 : c->x;
	ocy = c->y;
	do {
		XIfEvent(dpy, &ev, moveresize_eventmask, NULL);
		switch(ev.type) {
		case ClientMessage:
			if (ev.xclient.data.l[2] != 11)
				handler[ev.type](&ev);
			break;
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / refreshrate))
				continue;
			lasttime = ev.xmotion.time;

			nx = ocx + (ev.xmotion.x - x);
			ny = ocy + (ev.xmotion.y - y);

			if ((nx == c->x && ny == c->y) ||
			   ((!c->floating || c->maximized) &&
			    MAX(nx, c->x) - MIN(nx, c->x) < 60 &&
			    MAX(ny, c->y) - MIN(ny, c->y) < th))
				continue;

			if (!c->floating) {
				c->floating = 1;
				arrange(c->mon);
			}
			if (c->maximized)
				unmaximize(c, nx, ny, 0);
			else
				resizeclamped(c, nx, ny, c->w, c->h);
			if ((m = recttomon(ev.xmotion.x_root, ev.xmotion.y_root, 1, 1)) != selmon) {
				selmon = m;
				updatecurrentdesktop();
				sendclient(c, m, m->ws, 0);
				focus(c);
			}
			break;
		}
	} while (ev.type != ButtonRelease && !(ev.type == ClientMessage &&
		ev.xclient.message_type == netatom[NetWMMoveResize] &&
		ev.xclient.data.l[2] == 11)); /* 11 means cancel move */
	XUngrabPointer(dpy, CurrentTime);
}

Client *
nexttiled(Client *c)
{
	for (; c && (c->floating || c->minimized || c->maximized || !ISINWS(c) || c->fullscreen); c = c->next);
	return c;
}

Monitor *
numtomon(int num)
{
	Monitor *m;

	for (m = mons; m; m = m->next)
		if (m->num == num)
			return m;
	return NULL;
}

Client *
prevtiled(Client *c)
{
	for (c = c->prev; c && (!ISINWS(c) || c->floating || c->maximized || c->fullscreen || c->minimized); c = c->prev);
	return c;
}

void
propertynotify(XEvent *e)
{
	Client *c;
	Window trans;
	XPropertyEvent *ev = &e->xproperty;

	if (ev->atom == netatom[NetWMStrutPartial]) {
		updatestrut();
		return;
	}

	if (ev->state == PropertyDelete)
		return;
	if ((c = wintoclient(ev->window)) && ev->window == c->win) {
		if (ev->atom == XA_WM_TRANSIENT_FOR)
			if (!c->floating && (XGetTransientForHint(dpy, c->win, &trans)) &&
				(c->floating = (wintoclient(trans)) != NULL))
				arrange(c->mon);
		if (ev->atom == XA_WM_NORMAL_HINTS) {
			updatesizehints(c);
			if (!c->floating && c->fixed) {
				c->floating = 1;
				arrange(c->mon);
			}
		}
		if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
			updatetitle(c);
			drawdecorations(c, 0);
		}
		if (ev->atom == netatom[NetWMWindowType])
			updatewindowtype(c);
	}
}

void
quit(const Arg *arg)
{
	if(arg->i) restart = 1;
 	running = 0;
}

Monitor *
recttomon(int x, int y, int w, int h)
{
	Monitor *m, *r = selmon;
	int a, area = 0;

	for (m = mons; m; m = m->next)
		if ((a = INTERSECT(x, y, w, h, m)) > area) {
			area = a;
			r = m;
		}
	return r;
}

void
resize(Client *c, int x, int y, int w, int h)
{
	c->ox = c->x; c->x = x;
	c->oy = c->y; c->y = y;
	c->ow = c->w; c->w = w;
	c->oh = c->h; c->h = h;

	XMoveResizeWindow(dpy, c->frame, x, y, c->w, c->h);
	XMoveResizeWindow(dpy, c->win, COFFX(c), COFFY(c), w - TITLEW(c), h - TITLEH(c));

	if (c->hasdecoration) {
		drw_resize(c->srf, c->w, c->h);
		drawdecorations(c, 1);
	}
}

void
resizeclamped(Client *c, int x, int y, int w, int h)
{
	Monitor *cm = recttomon(x, y, w, h); /* Which monitor the client intersects more */
	int minx = cm->wx + 30 - w;
	int miny = cm->wy;
	int maxx = cm->wx + cm->ww - 30;
	int maxy = cm->wy + cm->wh - th;
	int minw = 200;
	int minh = 150;
	int maxw = cm->ww;
	int maxh = cm->wh;

	if (!c->fullscreen) {
		if (w < minw && c->floating) {
			if (x != c->x) x += w - minw;
			w = minw;
		}
		if (h < minh && c->floating) {
			if (y != c->y) y += h - minh;
			h = minh;
		}
		if (w > maxw) w = maxw;
		if (h > maxh) h = maxh;
		if (x < minx) x = minx;
		if (y < miny) y = miny;
		if (x > maxx) x = maxx;
		if (y > maxy) y = maxy;
	}
	resize(c, x, y, w, h);
}

void
resizekeyboard(const Arg *arg)
{
	int nw, nh;
	Client *c;

	if (!(c = selmon->sel) || c->fullscreen || c->maximized || c->fixed)
		return;
	if (!c->floating)
		togglefloating(NULL);

	nw = c->w;
	nh = c->h;

	switch (arg->i) {
	case MoveLeft:  nw -= movestep; break;
	case MoveRight: nw += movestep; break;
	case MoveUp:    nh -= movestep; break;
	case MoveDown:  nh += movestep; break;
	}

	resizeclamped(c, c->x, c->y, nw, nh);
}

void
resizemouse(const Arg *arg)
{
	int x, y, ocx, ocy, ocw, och, nx, ny, nw, nh;
	int edge = arg->i;
	XEvent ev, discard;
	Time lasttime = 0;
	Client *c;

	if (!(c = selmon->sel) || c->fullscreen || c->maximized || c->fixed)
		return;
	if (edge < 0) {
		XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h - TITLEH(c) + c->bw - 1);
		XSync(dpy, False);
		/* discard motionnotify generated by XWarpPointer */
		while (XCheckTypedEvent(dpy, MotionNotify, &discard));
		edge = EdgeBotRight;
	}

	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
        None, curresize[edge], CurrentTime) != GrabSuccess)
        return;
	if (!getrootptr(&x, &y))
		return;
	nx = ocx = c->x; ny = ocy = c->y;
	nw = ocw = c->w; nh = och = c->h;

	do {
		XIfEvent(dpy, &ev, moveresize_eventmask, NULL);
		switch (ev.type) {
		case ClientMessage:
			if (ev.xclient.data.l[2] != 11)
				handler[ev.type](&ev);
			break;
		case ConfigureRequest:
			if (ev.xconfigurerequest.window == c->win)
				configure(c);
			else
				handler[ev.type](&ev);
			break;
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / refreshrate))
				continue;
			lasttime = ev.xmotion.time;

			int dx = ev.xmotion.x - x;
			int dy = ev.xmotion.y - y;

			if (edge == EdgeRight || edge == EdgeTopRight || edge == EdgeBotRight)
				nw = ocw + dx;
			if (edge == EdgeBot || edge == EdgeBotRight || edge == EdgeBotLeft)
				nh = och + dy;
			if (edge == EdgeLeft || edge == EdgeTopLeft || edge == EdgeBotLeft) {
				nx = ocx + dx;
				nw = ocw - dx;
			}
			if (edge == EdgeTop || edge == EdgeTopRight || edge == EdgeTopLeft) {
				ny = ocy + dy;
				nh = och - dy;
			}

			if (!c->floating) {
				c->floating = 1;
				arrange(c->mon);
			}
			resizeclamped(c, nx, ny, nw, nh);
			break;
		}
	} while (ev.type != ButtonRelease && !(ev.type == ClientMessage &&
		ev.xclient.message_type == netatom[NetWMMoveResize] &&
		ev.xclient.data.l[2] == 11)); /* 11 means cancel move */
	XUngrabPointer(dpy, CurrentTime);
}

void
restack(Monitor *m)
{
	Client *c;
	Bar *b;

	if (!m->sel)
		return;
	if (m->sel->floating || m->sel->maximized || m->sel->fullscreen || !layouts[m->wsdata[m->ws].lt].arrange)
		XRaiseWindow(dpy, m->sel->frame);
	if (layouts[m->wsdata[m->ws].lt].arrange) {
		for (c = m->stack; c; c = c->snext)
			if ((!c->floating && !c->maximized) && ISINWS(c))
				XLowerWindow(dpy, c->frame);
	}
	for (b = bars; b && !m->sel->fullscreen; b = b->next)
		if (m == b->mon)
			XRaiseWindow(dpy, b->win);

	XFlush(dpy);
}

void
restoresession(void)
{
	Monitor *m;
	Client *c;

	FILE *fr = fopen(SESSION_FILE, "r");
	if (!fr)
		return;

	char *str = ecalloc(1, 64 * sizeof(char));
	while (fscanf(fr, "%63[^\n] ", str) != EOF) {
		long unsigned int winId;
		int ismin, ismax, ws, mnum;
		int check = sscanf(str, "%lu %d %d %d %d", &winId, &ws, &ismin, &ismax, &mnum);
		if (check != 5)
			break;
		for(m = mons; m; m = m->next){
			for (c = m->clients; c ; c = c->next) {
				if (c->win == winId) {
					c->ws = ws;
					setclientdesktop(c);
					if (ismax)
						maximize(c);
					if (ismin)
						minimize(c, 1);
					if (mnum != c->mon->num)
						sendclient(c, numtomon(mnum), ws, 0);
					break;
				}
			}
		}
	}

	for (m = mons; m; m = m->next)
		arrange(m);

	focus(NULL);
	free(str);
	fclose(fr);
	remove(SESSION_FILE);
}

void
run(void)
{
	XEvent ev;

	XSync(dpy, False);
	while (running && !XNextEvent(dpy, &ev)) {
		if (ev.type < LASTEvent && handler[ev.type]) /* main events */
			handler[ev.type](&ev);
		else if (ev.type == xrandr_evbase + RRScreenChangeNotify
		 || ev.type == xrandr_evbase + RRNotify) /* xrandr monitor events */
			if (updatemons())
				focus(NULL);
	}
}

void
savesession(void)
{
	Monitor *m;
	Client *c;
	FILE *fw = fopen(SESSION_FILE, "w");
	if (!fw) return;

	for(m = mons; m; m = m->next)
		for (c = m->clients; c != NULL; c = c->next) /* write client data */
			fprintf(fw, "%lu %u %d %d %d\n", c->win, c->ws, c->minimized, c->maximized, c->mon->num);
	fclose(fw);
}

void
scan(void)
{
	unsigned int i, num;
	Window d1, d2, *wins = NULL;
	XWindowAttributes wa;

	if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
		for (i = 0; i < num; i++) {
			if (!XGetWindowAttributes(dpy, wins[i], &wa)
			|| wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
				continue;
			if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState || getstate(wins[i]) == NormalState)
				manage(wins[i], &wa);
		}
		for (i = 0; i < num; i++) { /* now the transients */
			if (!XGetWindowAttributes(dpy, wins[i], &wa))
				continue;
			if (XGetTransientForHint(dpy, wins[i], &d1)
			&& (wa.map_state == IsViewable || getstate(wins[i]) == IconicState || getstate(wins[i]) == NormalState))
				manage(wins[i], &wa);
		}
		if (wins)
			XFree(wins);
	}
}

int
sendevent(Client *c, Atom proto)
{
	int n;
	Atom *protocols;
	int exists = 0;
	XEvent ev;

	if (XGetWMProtocols(dpy, c->win, &protocols, &n)) {
		while (!exists && n--)
			exists = protocols[n] == proto;
		XFree(protocols);
	}
	if (exists) {
		ev.type = ClientMessage;
		ev.xclient.window = c->win;
		ev.xclient.message_type = wmatom[WMProtocols];
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = proto;
		ev.xclient.data.l[1] = CurrentTime;
		XSendEvent(dpy, c->win, False, NoEventMask, &ev);
	}
	return exists;
}

void
sendclient(Client *c, Monitor *m, int ws, int warp)
{
	Monitor *old;

	if(!c || ws < 0)
		return;

	c->ws = ws < workspaces ? ws : workspaces -1;

	if(m && m != c->mon){
		unfocus(c, 1);
		detach(c);
		detachstack(c);
		old = c->mon;
		c->mon = m;
		attach(c);
		attachstack(c);
		if (c->fullscreen)
			resize(c, m->mx, m->my, m->mw, m->mh);
		if (c->maximized) {
			unmaximize(c, c->ox, c->oy, 1);
			maximize(c);
		}
		if (warp && c->floating)
			resize(c, m->wx + (m->ww - c->w)/2, m->wy + (m->wh - c->h)/2, c->w, c->h);
		arrange(old);
	}
	setclientdesktop(c);
	focus(NULL);
	arrange(c->mon);
}

void
sendtomon(const Arg *arg)
{
	Monitor *m = dirtomon(arg->i);

	if (!selmon->sel || !m || !mons->next)
		return;
	sendclient(selmon->sel, m, m->ws, 1);
}

void
sendtows(const Arg *arg)
{
	sendclient(selmon->sel, selmon, arg->i, 0);
}

void
setclientdesktop(Client *c)
{
	long d = c->sticky ? -1 : WSINDEX(c->mon, c->ws);
	XChangeProperty(dpy, c->win, netatom[NetWMDesktop], XA_CARDINAL, 32,
		PropModeReplace, (unsigned char *)&d, 1);
}

void
setclientstate(Client *c)
{
	Atom states[6] = { 0 };
	int n = 0;

	if (c->maximized && !c->fullscreen) {
		states[n++] = netatom[NetWMMaximizedVert];
		states[n++] = netatom[NetWMMaximizedHorz];
	}
	if (c->fullscreen)
		states[n++] = netatom[NetWMFullscreen];
	if (c->minimized)
		states[n++] = netatom[NetWMHidden];

	XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
		PropModeReplace, (unsigned char *)states, n);
	XChangeProperty(dpy, c->frame, netatom[NetWMState], XA_ATOM, 32,
		PropModeReplace, (unsigned char *)states, n);
}

void
setfocus(Client *c)
{
	XChangeProperty(dpy, root, netatom[NetActiveWindow], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *)&c->win, 1);
	sendevent(c, wmatom[WMTakeFocus]);
}

void
setfullscreen(Client *c, int fullscreen)
{
	if (!c || fullscreen == c->fullscreen)
		return;

	if (fullscreen) {
		if(c->maximized)
			unmaximize(c, c->ox, c->oy, 1);

		c->ohasdecoration = c->hasdecoration;
		c->obw = c->bw;

		c->hasdecoration = 0;
		c->bw = 0;
		c->fullscreen = 1;

		XSetWindowBorderWidth(dpy, c->frame, 0);
		resize(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
		XRaiseWindow(dpy, c->frame);
	} else {
		c->hasdecoration = c->ohasdecoration;
		c->bw = c->obw;
		c->fullscreen = 0;

		XSetWindowBorderWidth(dpy, c->frame, c->bw);
		resize(c, c->ox, c->oy, c->ow, c->oh);

		if (c->tmpunmax)
			maximize(c);
	}
	setclientstate(c);
}

void
setfocusmon(Monitor *m, int warp)
{
	if (!m || m == selmon)
		return;
	unfocus(selmon->sel, 1);
	selmon = m;
	updatecurrentdesktop();
	focus(NULL);
	if (warp)
		XWarpPointer(dpy, None, root, 0, 0, 0, 0,
			m->wx + m->ww / 2, m->wy + m->wh / 2);
}

void
setlayout(const Arg *arg)
{
	if(arg->i >= 0 && arg->i < LENGTH(layouts))
		selmon->wsdata[selmon->ws].lt = arg->i;
	arrange(selmon);
}

void
setup(void)
{
	unsigned int i, j;
	struct sigaction sa;
	int rr_major, rr_minor;
	XSetWindowAttributes wa;
	XVisualInfo vinfo;

	/* do not transform children into zombies when they terminate */
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART;
	sa.sa_handler = SIG_IGN;
	sigaction(SIGCHLD, &sa, NULL);

	/* clean up any zombies (inherited from .xinitrc etc) immediately */
	while (waitpid(-1, NULL, WNOHANG) > 0);
	signal(SIGHUP, sighup);
	signal(SIGTERM, sigterm);

	screen = DefaultScreen(dpy);
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	root = RootWindow(dpy, screen);

	utf8string = XInternAtom(dpy, "UTF8_STRING", False);
	motifatom = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
	wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
	wmatom[WMChangeState] = XInternAtom(dpy, "WM_CHANGE_STATE", False);
	wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
	netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
	netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
	netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
	netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
	netatom[NetWMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
	netatom[NetWMHidden] = XInternAtom(dpy, "_NET_WM_STATE_HIDDEN", False);
	netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	netatom[NetWMWindowTypeNormal] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_NORMAL", False);
	netatom[NetWMWindowTypeDock] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
	netatom[NetWMWindowTypeDesktop] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
	netatom[NetWMWindowTypeToolbar] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_TOOLBAR", False);
	netatom[NetWMWindowTypeMenu] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_MENU", False);
	netatom[NetWMWindowTypeUtility] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_UTILITY", False);
	netatom[NetWMWindowTypeSplash] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_SPLASH", False);
	netatom[NetWMWindowTypeDropdownMenu] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", False);
	netatom[NetWMWindowTypePopupMenu] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_POPUP_MENU", False);
	netatom[NetWMWindowTypeTooltip] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_TOOLTIP", False);
	netatom[NetWMWindowTypeNotification] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_NOTIFICATION", False);
	netatom[NetWMWindowTypeCombo] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_COMBO", False);
	netatom[NetWMWindowTypeDnd] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DND", False);
	netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
	netatom[NetClientListStacking] = XInternAtom(dpy, "_NET_CLIENT_LIST_STACKING", False);
	netatom[NetCloseWindow] = XInternAtom(dpy, "_NET_CLOSE_WINDOW", False);
	netatom[NetWMStrutPartial] = XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False);
	netatom[NetWorkarea] = XInternAtom(dpy, "_NET_WORKAREA", False);
	netatom[NetWMFrameExtents] = XInternAtom(dpy, "_NET_FRAME_EXTENTS", False);
	netatom[NetWMMoveResize] = XInternAtom(dpy, "_NET_WM_MOVERESIZE", False);
	netatom[NetWMMaximizedVert] = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_VERT", False);
	netatom[NetWMMaximizedHorz] = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
	netatom[NetNumberOfDesktops] = XInternAtom(dpy, "_NET_NUMBER_OF_DESKTOPS", False);
	netatom[NetCurrentDesktop] = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
	netatom[NetDesktopViewport] = XInternAtom(dpy, "_NET_DESKTOP_VIEWPORT", False);
	netatom[NetDesktopNames] = XInternAtom(dpy, "_NET_DESKTOP_NAMES", False);
	netatom[NetWMDesktop] = XInternAtom(dpy, "_NET_WM_DESKTOP", False);

	if ((hasxrandr = (XRRQueryExtension(dpy, &xrandr_evbase, &xrandr_errbase)
	          && XRRQueryVersion(dpy, &rr_major, &rr_minor)
	          && (rr_major > 1 || (rr_major == 1 && rr_minor >= 2)))))
		XRRSelectInput(dpy, root, RRScreenChangeNotifyMask | RRCrtcChangeNotifyMask);

	updatemons();
	updatedesktops();
	updateviewport();
	updatecurrentdesktop();
	updatestrut();

	cursor[CurNormal]       = XCreateFontCursor(dpy, XC_left_ptr);
	cursor[CurMove]         = XCreateFontCursor(dpy, XC_fleur);
	curresize[EdgeTop]      = XCreateFontCursor(dpy, XC_top_side);
	curresize[EdgeBot]      = XCreateFontCursor(dpy, XC_bottom_side);
	curresize[EdgeLeft]     = XCreateFontCursor(dpy, XC_left_side);
	curresize[EdgeRight]    = XCreateFontCursor(dpy, XC_right_side);
	curresize[EdgeTopLeft]  = XCreateFontCursor(dpy, XC_top_left_corner);
	curresize[EdgeTopRight] = XCreateFontCursor(dpy, XC_top_right_corner);
	curresize[EdgeBotLeft]  = XCreateFontCursor(dpy, XC_bottom_left_corner);
	curresize[EdgeBotRight] = XCreateFontCursor(dpy, XC_bottom_right_corner);

	if (XMatchVisualInfo(dpy, screen, 32, TrueColor, &vinfo)) {
		visual = vinfo.visual;
		depth  = vinfo.depth;
		cmap   = XCreateColormap(dpy, root, visual, AllocNone);
	} else {
		die("mithril: couldn't allocate visual");
	}

	font = drw_font_create(titlefont, fontsize);
	icons = drw_font_create(iconfont, iconsize);
	th = font->h + 2 + titleborderpx + outerpad * 2;

	for (i = 0; i < LENGTH(colors); i++)
		for (j = 0; j < LENGTH(colors[i]); j++)
			drw_color_create(colors[i][j], scheme[i][j]);

	/* border drawing doesn't use cairo */
	borders[0] = drw_x11_color_create(bordernorm);
	borders[1] = drw_x11_color_create(bordersel);

	wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8,
		PropModeReplace, (unsigned char *) "dwm", 3);
	XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
		PropModeReplace, (unsigned char *) netatom, NetLast);
	XDeleteProperty(dpy, root, netatom[NetClientList]);

	wa.cursor = cursor[CurNormal];
	wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask
		|ButtonPressMask|PointerMotionMask|EnterWindowMask
		|StructureNotifyMask|PropertyChangeMask;
	XChangeWindowAttributes(dpy, root, CWEventMask|CWCursor, &wa);
	XSelectInput(dpy, root, wa.event_mask);

	grabkeys();
	focus(NULL);
}

void
setwmstate(Window w, long state)
{
	long data[] = { state, None };

	XChangeProperty(dpy, w, wmatom[WMState], wmatom[WMState], 32,
		PropModeReplace, (unsigned char *)data, 2);
}

void
showhide(Client *c)
{
	if (!c)
		return;
	if (ISINWS(c)) {
		if (!c->minimized)
			mapclient(c);
		showhide(c->snext);
	} else {
		showhide(c->snext);
		if (!c->minimized)
			unmapclient(c);
	}
}

void
sighup(int unused)
{
	restart = 1;
	running = 0;
}

void
sigterm(int unused)
{
	running = 0;
}

void
spawn(const Arg *arg)
{
	struct sigaction sa;

	if (fork() == 0) {
		if (dpy)
			close(ConnectionNumber(dpy));
		setsid();

		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sa.sa_handler = SIG_DFL;
		sigaction(SIGCHLD, &sa, NULL);

		execvp(((char **)arg->v)[0], (char **)arg->v);
		die("mithril: execvp '%s' failed:", ((char **)arg->v)[0]);
	}
}

void
swapclients(Client *c1, Client *c2)
{
	Client *p1, *n1, *p2, *n2;
	Monitor *m = c1->mon;

	if (!c1 || !c2 || c1 == c2)
		return;

	p1 = c1->prev;
	n1 = c1->next;
	p2 = c2->prev;
	n2 = c2->next;

	if (p1 && p1 != c2) p1->next = c2;
	if (n1 && n1 != c2) n1->prev = c2;
	if (p2 && p2 != c1) p2->next = c1;
	if (n2 && n2 != c1) n2->prev = c1;

	if (m->clients == c1) m->clients = c2;
	else if (m->clients == c2) m->clients = c1;

	if (n1 == c2) {
		c2->prev = p1;
		c2->next = c1;
		c1->prev = c2;
		c1->next = n2;
	} else if (n2 == c1) {
		c1->prev = p2;
		c1->next = c2;
		c2->prev = c1;
		c2->next = n1;
	} else {
		c1->prev = p2;
		c1->next = n2;
		c2->prev = p1;
		c2->next = n1;
	}
}

void
swaptiled(const Arg *arg)
{
	Client *c, *new;

	if (!(c = selmon->sel) || c->floating || c->maximized || c->fullscreen || c->minimized)
		return;
	if (!(new = arg->i > 0 ? nexttiled(c->next) : prevtiled(c)))
		if (!(new = arg->i > 0 ? nexttiled(c->mon->clients) : lasttiled(c->mon)))
			return;

	swapclients(c, new);
	arrange(selmon);
}

void
swapmouse(const Arg *arg)
{
	int x = 0, y = 0;
	XEvent ev;
	Time lasttime = 0;
	Client *c, *t;

	if (!(c = selmon->sel) || c->fullscreen || c->floating || c->maximized)
		return;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurMove], CurrentTime) != GrabSuccess)
		return;
	do {
		XIfEvent(dpy, &ev, moveresize_eventmask, NULL);
		switch(ev.type) {
		case ClientMessage:
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / refreshrate))
				continue;
			lasttime = ev.xmotion.time;

			x = ev.xmotion.x_root;
			y = ev.xmotion.y_root;

			break;
		}
	} while (ev.type != ButtonRelease);

	for (t = selmon->clients; t; t = nexttiled(t->next)) {
		if (t != c && x > t->x && x < t->x + t->w && y > t->y && y < t->y + t->h) {
			swapclients(c, t);
			arrange(selmon);
			break;
		}
	}
	XUngrabPointer(dpy, CurrentTime);
}

void
tile(Monitor *m)
{
	unsigned int i, n, h, mw, my, ty;
	int gap = m->wsdata[m->ws].gappx;
	int nm = m->wsdata[m->ws].nmaster;
	Client *c;

	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
	if (n == 0)
		return;

	if (n > nm)
		mw = nm ? m->ww * m->wsdata[m->ws].mfact : 0;
	else
		mw = m->ww - gap;
	for (i = 0, my = ty = gap, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
			if (i < nm) {
			h = (m->wh - my) / (MIN(n, nm) - i) - gap;
			resize(c, m->wx + gap, m->wy + my, mw - (2 * c->bw) - gap, h - (2 * c->bw));
			if (my + HEIGHT(c) + gap < m->wh)
				my += HEIGHT(c) + gap;
		} else {
			h = (m->wh - ty) / (n - i) - gap;
			resize(c, m->wx + mw + gap, m->wy + ty, m->ww - mw - (2*c->bw) - 2 * gap, h - (2 * c->bw));
			if (ty + HEIGHT(c) + gap < m->wh)
				ty += HEIGHT(c) + gap;
		}
}

void
togglefloating(const Arg *arg)
{
	if (!selmon->sel || selmon->sel->fullscreen || selmon->sel->maximized || selmon->sel->fixed)
		return;
	selmon->sel->floating = !selmon->sel->floating;
	arrange(selmon);
}

void
togglefullscr(const Arg *arg)
{
	if(selmon->sel)
		setfullscreen(selmon->sel, !selmon->sel->fullscreen);
}

void
togglemaximize(const Arg *arg)
{
	if (selmon->sel && !selmon->sel->fullscreen && !selmon->sel->fixed)
		selmon->sel->maximized ? unmaximize(selmon->sel, selmon->sel->ox, selmon->sel->oy, 0) : maximize(selmon->sel);
}

void
toggleminimize(const Arg *arg)
{
	if (selmon->sel)
		minimize(selmon->sel, !selmon->sel->minimized);
}

void
togglesticky(const Arg *arg)
{
	if (!selmon->sel)
		return;

	selmon->sel->sticky = selmon->sel->sticky ? 0 : 1;
	setclientdesktop(selmon->sel);
	showhide(selmon->sel);
	arrange(selmon);
}

void
unframe(Client *c, int destroyed)
{
	XUnmapWindow(dpy, c->frame);

	if(!destroyed){
		XReparentWindow(dpy, c->win, root, 0, 0);
		XRemoveFromSaveSet(dpy, c->win);
	}

	drw_surf_destroy(c->srf);
	c->srf = NULL;
	XDestroyWindow(dpy, c->frame);
	c->frame = None;
}

void
unfocus(Client *c, int setfocus)
{
	if (!c)
		return;
	grabbuttons(c, 0);
	c->mon->sel = NULL;
	drawdecorations(c, 1);
	if (setfocus) {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
}

void unmanage(Client *c, int destroyed)
{
	detach(c);
	detachstack(c);
	unframe(c, destroyed);
	if (!destroyed) {
		XGrabServer(dpy);
		XSetErrorHandler(xerrordummy);
		XSelectInput(dpy, c->win, NoEventMask);
		XUnmapWindow(dpy, c->win);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
	arrange(c->mon);
	free(c);
	updateclientlist();
	updateclientliststacking();
	focus(NULL);
}

void
unmapnotify(XEvent *e)
{
	Bar *b;
	Client *c;
	XUnmapEvent *ev = &e->xunmap;

	if ((c = wintoclient(ev->window))) {
		if (ev->send_event)
			setwmstate(c->win, WithdrawnState);
		else
			unmanage(c, 0);
		return;
	}
	if ((b = wintobar(ev->window))) {
		detachbar(b);
		updatestrut();
		arrange(b->mon);
		free(b);
	}
}

void
unmapclient(Client *c)
{
	XWindowAttributes fa, ra, wa;

	if (!c) return;

	XGrabServer(dpy);
	XGetWindowAttributes(dpy, root, &ra);
	XGetWindowAttributes(dpy, c->frame, &fa);
	XGetWindowAttributes(dpy, c->win, &wa);

	/* Prevent UnmapNotify events - very important */
	XSelectInput(dpy, root, ra.your_event_mask & ~SubstructureNotifyMask);
	XSelectInput(dpy, c->frame, fa.your_event_mask & ~SubstructureNotifyMask);
	XSelectInput(dpy, c->win, wa.your_event_mask & ~StructureNotifyMask);
	XUnmapWindow(dpy, c->frame);
	XUnmapWindow(dpy, c->win);

	setwmstate(c->win, IconicState);

	XSelectInput(dpy, root, ra.your_event_mask);
	XSelectInput(dpy, c->frame, fa.your_event_mask);
	XSelectInput(dpy, c->win, wa.your_event_mask);
	XUngrabServer(dpy);
}

void
unmaximize(Client *c, int x, int y, int tmp)
{
	if (!c || !c->maximized)
		return;

	c->bw = c->obw;
	XSetWindowBorderWidth(dpy, c->frame, c->bw);
	resizeclamped(c, x, y, c->ow, c->oh);
	c->maximized = 0;
	c->tmpunmax = tmp;
	setclientstate(c);
	arrange(c->mon);
}

void
updateclientlist(void)
{
	Client *c;
	Monitor *m;

	XDeleteProperty(dpy, root, netatom[NetClientList]);
	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			XChangeProperty(dpy, root, netatom[NetClientList],
				XA_WINDOW, 32, PropModeAppend,
				(unsigned char *)&(c->win), 1);
}

void
updateclientliststacking(void)
{
	Client *c;
	Monitor *m;

	XDeleteProperty(dpy, root, netatom[NetClientListStacking]);
	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			XChangeProperty(dpy, root, netatom[NetClientListStacking],
				XA_WINDOW, 32, PropModeAppend,
				(unsigned char *)&(c->win), 1);
}

void
updatecurrentdesktop(void)
{
	long idx = WSINDEX(selmon, selmon->ws);
	XChangeProperty(dpy, root, netatom[NetCurrentDesktop], XA_CARDINAL, 32,
		PropModeReplace, (unsigned char *)&idx, 1);
}

void
updatedesktops(void)
{
	Monitor *m;
	unsigned int i;
	long n = (long)workspaces * nmons;
	char names[1024];
	int off = 0, len, done = 0;

	XChangeProperty(dpy, root, netatom[NetNumberOfDesktops], XA_CARDINAL, 32,
		PropModeReplace, (unsigned char *)&n, 1);

	for (m = mons; m && !done; m = m->next) {
		for (i = 0; i < (unsigned)workspaces && i < LENGTH(workspace_rules); i++) {
			len = strlen(workspace_rules[i].name) + 1;
			if (off + len > (int)sizeof(names)) {
				done = 1;
				break;
			}
			memcpy(names + off, workspace_rules[i].name, len);
			off += len;
		}
	}
	XChangeProperty(dpy, root, netatom[NetDesktopNames], utf8string, 8,
		PropModeReplace, (unsigned char *)names, off);
}

int
updatemons(void)
{
	int dirty = 0;
	int i, n = nmons, nn = 0;
	Client *c;
	Monitor *m;
	XRRScreenResources *sr;
	XRROutputInfo *oi;
	XRRCrtcInfo *ci;

	struct { int x, y, w, h; } active[32]; /* active output geometries */

	sr = hasxrandr ? XRRGetScreenResources(dpy, root) : NULL;
	if (!sr) { /* xrandr not available */
		nmons = 1;
		if (!mons)
			mons = createmon();
		if (mons->mw != sw || mons->mh != sh) {
			dirty = 1;
			mons->mw = mons->ww = sw;
			mons->mh = mons->wh = sh;
		}
		if (!selmon)
			selmon = mons;
		return dirty;
	}

	for (i = 0; i < sr->noutput && nn < 32; i++) {
		if (!(oi = XRRGetOutputInfo(dpy, sr, sr->outputs[i])))
			continue;
		if (oi->connection == RR_Connected && oi->crtc) {
			if ((ci = XRRGetCrtcInfo(dpy, sr, oi->crtc))) {
				active[nn].x = ci->x;
				active[nn].y = ci->y;
				active[nn].w = ci->width;
				active[nn].h = ci->height;
				nn++;
				XRRFreeCrtcInfo(ci);
			}
		}
		XRRFreeOutputInfo(oi);
	}
	XRRFreeScreenResources(sr);
	nmons = nn;

	if (nn == 0)
		return 0;

	for (i = n; i < nn; i++) { /* new monitor added */
		for (m = mons; m && m->next; m = m->next);
		if (m)
			m->next = createmon();
		else
			mons = createmon();
	}
	for (i = 0, m = mons; i < nn && m; m = m->next, i++) {
		if (i >= n || active[i].x != m->mx || active[i].y != m->my
		|| active[i].w != m->mw || active[i].h != m->mh) {
			dirty = 1;
			m->num = i; /* update geometry */
			m->mx = m->wx = active[i].x;
			m->my = m->wy = active[i].y;
			m->mw = m->ww = active[i].w;
			m->mh = m->wh = active[i].h;
		}
	}
	for (i = nn; i < n; i++) { /* a monitor got removed */
		for (m = mons; m && m->next; m = m->next);
		while ((c = m->clients)) {
			dirty = 1;
			m->clients = c->next;
			detachstack(c);
			c->mon = mons;
			attach(c);
			attachstack(c);
		}
		if (m == selmon)
			selmon = mons;
		cleanupmon(m);
	}
	if (dirty) {
		selmon = mons;
		selmon = wintomon(root);
		updatestrut();
		updatedesktops();
		updateviewport();
		for (m = mons; m; m = m->next)
			arrange(m);
	}
	return dirty;
}

void
updatemotifhints(Client *c)
{
	Atom da;
	unsigned long n, dl;
	unsigned long *motif;
	int format;

	if (!decorhints)
		return;
	if (XGetWindowProperty(dpy, c->win, motifatom, 0L, 5L, False, motifatom,
	                       &da, &format, &n, &dl, (unsigned char **)&motif) == Success && motif != NULL) {
		if (motif[0] & (1 << 1)) {
			if(!(c->hasdecoration = (motif[2] & (1L << 1) || motif[2] & (1L << 2) ||
						 	motif[2] & (1L << 3) || motif[2] & (1L << 0))))
				c->bw = 0;
			XSetWindowBorderWidth(dpy, c->frame, c->bw);
			resizeclamped(c, c->x, c->y, WIDTH(c) - (2*c->bw), HEIGHT(c) - (2*c->bw));
		}
		XFree(motif);
	}
}

void
updatenumlockmask(void)
{
	int i, j;
	XModifierKeymap *modmap;

	numlockmask = 0;
	modmap = XGetModifierMapping(dpy);
	for (i = 0; i < 8; i++)
		for (j = 0; j < modmap->max_keypermod; j++)
			if (modmap->modifiermap[i * modmap->max_keypermod + j]
				== XKeysymToKeycode(dpy, XK_Num_Lock))
				numlockmask = (1 << i);
	XFreeModifiermap(modmap);
}

void
updatesizehints(Client *c)
{
	long msize;
	XSizeHints size;

	if (!XGetWMNormalHints(dpy, c->win, &size, &msize))
		size.flags = PSize;
	c->fixed = (size.max_width && size.max_height &&
			  size.max_width == size.min_width &&
		       size.max_height == size.min_height);
}

void
updatestrut(void)
{
	Atom type;
	Bar *b;
	Client *c;
	Monitor *m;
	int format;
	unsigned long n, after;
	long *strut = NULL;
	long workarea[4 * nmons];
	long *ms, *maxstrut = ecalloc(1, sizeof(long) * nmons * 4);

	for (b = bars; b; b = b->next, strut = NULL) {
		if (XGetWindowProperty(dpy, b->win, netatom[NetWMStrutPartial],
				0, 4, False, XA_CARDINAL, &type, &format, &n, &after,
				(unsigned char **)&strut) == Success && strut && n >= 4) {
			for (m = mons; m; m = m->next) {
				XWindowAttributes wa;
				if (m != b->mon || !XGetWindowAttributes(dpy, b->win, &wa) || wa.map_state != IsViewable)
					continue;
				ms = &maxstrut[b->mon->num * 4];
				if (strut[0] > ms[0]) ms[0] = strut[0]; /* left */
				if (strut[1] > ms[1]) ms[1] = strut[1]; /* right */
				if (strut[2] > ms[2]) ms[2] = strut[2]; /* top */
				if (strut[3] > ms[3]) ms[3] = strut[3]; /* bottom */
			}
		}
		XFree(strut);
	}
	for (m = mons; m; m = m->next) {
		ms = &maxstrut[m->num * 4];
		m->owx = m->wx;
		m->owy = m->wy;
		m->oww = m->ww;
		m->owh = m->wh;
		m->wx = m->mx + ms[0];
		m->ww = m->mw - (ms[0] + ms[1]);
		m->wy = m->my + ms[2];
		m->wh = m->mh - (ms[2] + ms[3]);
		workarea[m->num*4+0] = m->wx;
		workarea[m->num*4+1] = m->wy;
		workarea[m->num*4+2] = m->ww;
		workarea[m->num*4+3] = m->wh;
		for (c = m->stack; c; c = c->snext) {
			if (c->maximized && (m->wx != m->owx ||
				m->wy != m->owy || m->ww != m->oww || m->wh != m->owh)) {
				unmaximize(c, c->ox, c->oy, 0);
				maximize(c); /* remaximize to fit new workarea */
			}
		}
	}
	free(maxstrut);
	XChangeProperty(dpy, root, netatom[NetWorkarea], XA_CARDINAL, 32,
		PropModeReplace, (unsigned char *)workarea, nmons * 4);
}

void
updatetitle(Client *c)
{
	if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name))
		gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
	if (c->name[0] == '\0') /* hack to mark broken clients */
		strcpy(c->name, "broken");
}

void
updatewindowtype(Client *c)
{
	Atom state = getatomprop(c->win, netatom[NetWMState]);
	Atom wtype = getatomprop(c->win, netatom[NetWMWindowType]);

	if (state == netatom[NetWMFullscreen])
		setfullscreen(c, 1);
	if (state == netatom[NetWMMaximizedVert]
	  ||state == netatom[NetWMMaximizedHorz])
		maximize(c);
	if (wtype == netatom[NetWMWindowTypeDialog]
	  || wtype == netatom[NetWMWindowTypeUtility]
	  || wtype == netatom[NetWMWindowTypeToolbar]
	  || wtype == netatom[NetWMWindowTypeSplash]
	  || wtype == netatom[NetWMWindowTypeMenu]
	  || wtype == netatom[NetWMWindowTypeDropdownMenu]
	  || wtype == netatom[NetWMWindowTypePopupMenu]
	  || wtype == netatom[NetWMWindowTypeTooltip]
	  || wtype == netatom[NetWMWindowTypeNotification]
	  || wtype == netatom[NetWMWindowTypeCombo]
	  || wtype == netatom[NetWMWindowTypeDnd])
		c->floating = 1;
}

void
updateviewport(void) {
	int idx, i;
	long viewport[workspaces * nmons * 2];
	Monitor *m;

	for (m = mons; m; m = m->next) {
		for (i = 0; i < workspaces; i++) {
			idx = WSINDEX(m, i) * 2;
			viewport[idx] = m->mx;
			viewport[idx + 1] = m->my;
		}
	}
	XChangeProperty(dpy, root, netatom[NetDesktopViewport], XA_CARDINAL, 32,
		PropModeReplace, (unsigned char *)viewport, workspaces * nmons * 2);
}

void
view(const Arg *arg)
{
	if (selmon->ws == arg->i)
		return;
	selmon->ws = arg->i < workspaces && arg->i >= 0 ? arg->i : workspaces - 1;
	arrange(selmon);
	updatecurrentdesktop();
	focus(NULL);
}

Bar *
wintobar(Window w)
{
	Bar *b;

	for (b = bars; b; b = b->next)
		if (b->win == w)
			return b;
	return NULL;
}

Client *
wintoclient(Window w)
{
	Client *c;
	Monitor *m;

	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			if (c->win == w || c->frame == w)
				return c;
	return NULL;
}

Monitor *
wintomon(Window w)
{
	int x, y;
	Client *c;
	Bar *b;

	if (w == root && getrootptr(&x, &y))
		return recttomon(x, y, 1, 1);
	if((b = wintobar(w)))
		return b->mon;
	if ((c = wintoclient(w)))
		return c->mon;
	return selmon;
}

int
xerror(Display *dpy, XErrorEvent *ee)
{
	if (ee->error_code == BadWindow
	|| (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
	|| (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
	|| (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
	|| (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
	|| (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
	|| (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
		return 0;
	fprintf(stderr, "mithril: fatal error: request code=%d, error code=%d\n",
		ee->request_code, ee->error_code);
	return xerrorxlib(dpy, ee); /* may call exit */
}

int
xerrordummy(Display *dpy, XErrorEvent *ee)
{
	return 0;
}

int
main(int argc, char *argv[])
{
	if (!(dpy = XOpenDisplay(NULL)))
     	die("mithril: cannot open display");
	xerrorxlib = XSetErrorHandler(xerrordummy);
	XrmInitialize();
	load_xresources();
	setup();
	XSetErrorHandler(xerror);
	scan();
	restoresession();
	run();
	cleanup();
	if(restart) execvp(argv[0], argv);
	return 0;
}

