#include <X11/Xlib.h>
#include <X11/keysym.h>

enum { MoveLeft, MoveRight, MoveUp, MoveDown };

typedef union {
	int i;
	unsigned int ui;
	double f;
	const void *v;
} Arg;

typedef struct {
	unsigned int mod;
	KeySym keysym;
	void (*func)(const Arg *);
	Arg arg;
} Key;

typedef struct {
	char name[16];
	int lt;
	int gappx;
	float mfact;
	int nmaster;
} Workspace;

void closesel(const Arg *arg);
void focusmon(const Arg *arg);
void focusstack(const Arg *arg);
void incmfact(const Arg *arg);
void incnmaster(const Arg *arg);
void movekeyboard(const Arg *arg);
void movemouse(const Arg *arg);
void quit(const Arg *arg);
void resizekeyboard(const Arg *arg);
void resizemouse(const Arg *arg);
void sendtows(const Arg *arg);
void sendtomon(const Arg *arg);
void setlayout(const Arg *arg);
void spawn(const Arg *arg);
void swapmouse(const Arg *arg);
void swaptiled(const Arg *arg);
void togglegaps(const Arg *arg);
void togglefloating(const Arg *arg);
void togglefullscr(const Arg *arg);
void togglemaximize(const Arg *arg);
void toggleminimize(const Arg *arg);
void togglesticky(const Arg *arg);
void view(const Arg *arg);

