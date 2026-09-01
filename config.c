#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <unistd.h>
#include <X11/keysym.h>
#include <X11/Xresource.h>

#include "util.h"
#include "defs.h"
#include "config.h"

#define GRAY		"#ff999999"
#define GRAYDARK	"#ff222222"
#define WHITE		"#ffe0e0e0"
#define WHITEALT	"#ffffffff"
#define RED		"#fff1212c"
#define REDALT		"#fff1414c"
#define BLUE		"#ff818cf1"

#define MODKEY	Mod4Mask
#define SHIFT	ShiftMask

enum { NOTHING, INTEGER, DOUBLE, STRING, SPAWN };


typedef struct {
	char *name;
	int mask;
} ModParser;

typedef struct {
	char *name;
	void (*func)(const Arg *);
	int argtype;
} FuncParser;

static void addkeybind(Key k);
static void addworkspacerule(WorkspaceRule rule);
static int  parsearg(FuncParser *fe, char *argstr, Arg *arg);
static void parsebind(char *value);
static int  parsedouble(const char *s, double *result);
static int  parseint(const char *s, int *result);
static int  parsemod(char *modstr, unsigned int *mod);
static void parsevar(char *name, char *value);
static void parsewsrule(char *value);
static void resource_load(XrmDatabase db, char *name, int rtype, void *dst);
static char *trim(char *s);

double mfact = 0.55; /* factor of master area size [0.05..0.95] */
int nmaster = 1;
int gappx = 10;
char layout[] = "master-stack";
int movestep = 20;
long unsigned int refreshrate = 120;  /* refresh rate (per second) for client move/resize */
unsigned int workspaces 	  = 7;

int decorhints  		  = 1;    /* 1 means respect decoration hints */
int borderpx 		       = 1;	/* frame border */
int verticaltitle		  = 0;
int inverttitlebar	       = 0;
int titleborderpx 	       = 0;	/* border between the titlebar and client window */
int titleheight		  = 32;
char titlefont[] 	       = "monospace";	/* font used on window titles */
double fontsize 	       = 13.00;
int outerpad			  = 6;	/* titlebar padding */
int invertbuttons 	       = 0;	/* 1 means buttons start on the left */
int lrpad		 	       = 10; /* padding between buttons and text */
int centeredtitle  	    	  = 1;  /* 0 means window title on the left corner */
int offset_y			  = 1;  /* offset elements downwards */
int buttonradius		  = 2;	/* button roundness */
int buttonwidth		  = 11;
int buttonheight		  = 11;

/* You can use a nerd font for button icons */
char iconfont[] 	       = "monospace";
double iconsize 	       = 10.00;
char btn_close_icn[] 	  = "";
char btn_maximize_icn[]    = "";
char btn_minimize_icn[]    = "";
int buttonborderpx	       = 0;	/* border/outline arround the buttons */

/* Colors */
/* The WM supports both rgb and argb hex colors */
char bgnorm[]		       = "#ff222222"; /* norm means unfocused window */
char bgsel[]	     	  = "#ff2a2a2a"; /* sel means focused window */

char fgnorm[]    		  = GRAY;
char fgsel[]     		  = WHITE;

char bordernorm[]		  = "#ff555555";
char bordersel[]		  = "#ff6a6a6a";
char borderswaporig[]	  = "#ffeeeeee";
char borderswapdest[]	  = RED;

char titlebordernorm[]	  = "#ff111111";
char titlebordersel[]	  = "#ff151515";

/* close button */
char closefgnorm[] 	       = GRAY;
char closefgsel[] 	       = GRAYDARK;
char closefghover[] 	  = GRAYDARK;

char closebgnorm[]	    	  = GRAY;
char closebgsel[]	       = RED;
char closebghover[]        = REDALT;

char closebordernorm[]	  = GRAY;
char closebordersel[]	  = RED;
char closeborderhover[]    = REDALT;

/* maximize button */
char maximizefgnorm[]      = GRAY;
char maximizefgsel[]	  = GRAYDARK;
char maximizefghover[]     = GRAYDARK;

char maximizebgnorm[]      = GRAY;
char maximizebgsel[]	  = WHITE;
char maximizebghover[]	  = WHITEALT;

char maximizebordernorm[]  = GRAY;
char maximizebordersel[]   = WHITE;
char maximizeborderhover[] = WHITEALT;

/* minimize button */
char minimizefgnorm[]      = GRAY;
char minimizefgsel[] 	  = WHITE;
char minimizefghover[]     = GRAYDARK;

char minimizebgnorm[]	  = GRAY;
char minimizebgsel[]	  = WHITE;
char minimizebghover[]	  = WHITEALT;

char minimizebordernorm[]  = GRAY;
char minimizebordersel[]   = WHITE;
char minimizeborderhover[] = WHITEALT;

/* used to parse the config */
Parser config[] = {
	{ "layout", 			STRING,  layout },
	{ "movestep", 			INTEGER, &movestep },
	{ "mfact",			DOUBLE,  &mfact },
	{ "nmaster", 			INTEGER, &nmaster },
	{ "gappx", 			INTEGER, &gappx },
	{ "refreshrate", 		INTEGER, &refreshrate },
	{ "workspaces", 		INTEGER, &workspaces },
	{ "decorhints", 		INTEGER, &decorhints },
	{ "borderpx", 			INTEGER, &borderpx },
	{ "verticaltitle", 		INTEGER, &verticaltitle },
	{ "inverttitlebar", 	INTEGER, &inverttitlebar },
	{ "titleborderpx", 		INTEGER, &titleborderpx },
	{ "titleheight", 		INTEGER, &titleheight },
	{ "titlefont", 		STRING,  titlefont },
	{ "fontsize", 			DOUBLE,  &fontsize },
	{ "outerpad", 			INTEGER, &outerpad },
	{ "invertbuttons", 		INTEGER, &invertbuttons },
	{ "lrpad", 			INTEGER, &lrpad },
	{ "centeredtitle", 		INTEGER, &centeredtitle },
	{ "offset_y", 			INTEGER, &offset_y },
	{ "buttonradius", 		INTEGER, &buttonradius },
	{ "buttonwidth", 		INTEGER, &buttonwidth },
	{ "buttonheight", 		INTEGER, &buttonheight },
	{ "iconfont", 			STRING,  iconfont },
	{ "iconsize", 			DOUBLE,  &iconsize },
	{ "btn_close_icn", 		STRING,  btn_close_icn },
	{ "btn_maximize_icn", 	STRING,  btn_maximize_icn },
	{ "btn_minimize_icn", 	STRING,  btn_minimize_icn },
	{ "buttonborderpx", 	INTEGER, &buttonborderpx },
	{ "bgnorm", 			STRING,  bgnorm },
	{ "bgsel", 			STRING,  bgsel },
	{ "fgnorm", 			STRING,  fgnorm },
	{ "fgsel", 			STRING,  fgsel },
	{ "bordernorm", 		STRING,  bordernorm },
	{ "bordersel", 		STRING,  bordersel },
	{ "borderswaporig", 	STRING,  borderswaporig },
	{ "borderswapdest", 	STRING,  borderswapdest },
	{ "titlebordernorm", 	STRING,  titlebordernorm },
	{ "titlebordersel", 	STRING,  titlebordersel },
	{ "closefgnorm", 		STRING,  closefgnorm },
	{ "closefgsel", 		STRING,  closefgsel },
	{ "closefghover", 		STRING,  closefghover },
	{ "closebgnorm", 		STRING,  closebgnorm },
	{ "closebgsel", 		STRING,  closebgsel },
	{ "closebghover", 		STRING,  closebghover },
	{ "closebordernorm", 	STRING,  closebordernorm },
	{ "closebordersel", 	STRING,  closebordersel },
	{ "closeborderhover", 	STRING,  closeborderhover },
	{ "maximizefgnorm", 	STRING,  maximizefgnorm },
	{ "maximizefgsel", 		STRING,  maximizefgsel },
	{ "maximizefghover", 	STRING,  maximizefghover },
	{ "maximizebgnorm", 	STRING,  maximizebgnorm },
	{ "maximizebgsel", 		STRING,  maximizebgsel },
	{ "maximizebghover", 	STRING,  maximizebghover },
	{ "maximizebordernorm",	STRING,  maximizebordernorm },
	{ "maximizebordersel", 	STRING,  maximizebordersel },
	{ "maximizeborderhover", STRING,  maximizeborderhover },
	{ "minimizefgnorm", 	STRING,  minimizefgnorm },
	{ "minimizefgsel",	 	STRING,  minimizefgsel },
	{ "minimizefghover", 	STRING,  minimizefghover },
	{ "minimizebgnorm", 	STRING,  minimizebgnorm },
	{ "minimizebgsel", 		STRING,  minimizebgsel },
	{ "minimizebghover", 	STRING,  minimizebghover },
	{ "minimizebordernorm", 	STRING,  minimizebordernorm },
	{ "minimizebordersel", 	STRING,  minimizebordersel },
	{ "minimizeborderhover", STRING,  minimizeborderhover },
};

/* used to read and write the values from xresources */
Parser resources[] = {
	{ "borderpx",            INTEGER, &borderpx },
	{ "titleborderpx",       INTEGER, &titleborderpx },
	{ "titlefont",           STRING,  &titlefont },
	{ "fontsize",            DOUBLE,  &fontsize },
	{ "invertbuttons",      	INTEGER, &invertbuttons },
	{ "lrpad",               INTEGER, &lrpad },
	{ "centeredtitle",       INTEGER, &centeredtitle },
	{ "offset_y",            INTEGER, &offset_y },
	{ "btn_close_icn",       STRING,  &btn_close_icn },
	{ "btn_maximize_icn", 	STRING,  &btn_maximize_icn },
	{ "btn_minimize_icn",    STRING,  &btn_minimize_icn },
	{ "bordernorm",          STRING,  &bordernorm },
	{ "bgnorm",         	STRING,  &bgnorm },
	{ "titlebordernorm",     STRING,  &titlebordernorm },
	{ "fgnorm",            	STRING,  &fgnorm },
	{ "closefgnorm",        	STRING,  &closefgnorm },
	{ "maximizefgnorm",     	STRING,  &maximizefgnorm },
	{ "minimizefgnorm",     	STRING,  &minimizefgnorm },
	{ "bordersel",           STRING,  &bordersel },
	{ "bgsel",          	STRING,  &bgsel },
	{ "titlebordersel",      STRING,  &titlebordersel },
	{ "fgsel",             	STRING,  &fgsel },
	{ "closefgsel",         	STRING,  &closefgsel },
	{ "maximizefgsel",      	STRING,  &maximizefgsel },
	{ "minimizefgsel",      	STRING,  &minimizefgsel },
	{ "closefghover",        STRING,  &closefghover },
	{ "maximizefghover",     STRING,  &maximizefghover },
	{ "minimizefghover",     STRING,  &minimizefghover },
};

ModParser mods[] = {
	{ "super",   Mod4Mask },
	{ "shift",   ShiftMask },
	{ "ctrl",    ControlMask },
	{ "control", ControlMask },
	{ "alt",     Mod1Mask },
	{ "altgr",   Mod5Mask },
	{ "none",    0 },
};

FuncParser funcs[] = {
	{ "spawn",           spawn,           SPAWN   },
	{ "quit",            quit,            INTEGER },
	{ "close",	      closesel,        NOTHING },
	{ "togglegaps",      togglegaps,      NOTHING },
	{ "maximize",  	 togglemaximize,  NOTHING },
	{ "minimize",  	 toggleminimize,  NOTHING },
	{ "fullscreen",   	 togglefullscr,   NOTHING },
	{ "togglesticky",    togglesticky,    NOTHING },
	{ "togglefloating",  togglefloating,  NOTHING },
	{ "focus",	      focusstack,      INTEGER },
	{ "swaptiled",       swaptiled,       INTEGER },
	{ "focusmon",        focusmon,        INTEGER },
	{ "sendtomon",       sendtomon,       INTEGER },
	{ "view",            view,            INTEGER },
	{ "sendtows",        sendtows,        INTEGER },
	{ "incnmaster",      incnmaster,      INTEGER },
	{ "incmfact",        incmfact,        DOUBLE  },
	{ "setlayout",       setlayout,       STRING  },
	{ "move", 	   	 movekeyboard,    INTEGER },
	{ "resize",  		 resizekeyboard,  INTEGER },
};


#define MOVERESIZEKEYS(KEY, DIRECTION) \
	{ MODKEY,                       KEY,      movekeyboard,   {.i = DIRECTION} },\
	{ MODKEY|SHIFT,                 KEY,      resizekeyboard, {.i = DIRECTION} },
#define WORKSPACEKEYS(KEY, WS) \
	{ MODKEY,                       KEY,      view,           {.i = WS }},\
	{ MODKEY|SHIFT,                 KEY,      sendtows,       {.i = WS }},

#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

#define VOLUP "wpctl set-volume @DEFAULT_AUDIO_SINK@ 2%+; kill -44 $(pidof dwmblocks)"
#define VOLDOWN "wpctl set-volume @DEFAULT_AUDIO_SINK@ 2%-; kill -44 $(pidof dwmblocks)"
#define MUTE "wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle; kill -44 $(pidof dwmblocks)"

static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, NULL };
static const char *termcmd[]  = { "st", NULL };

/* keybinds */
static Key defaultkeys[] = {
	{ MODKEY,           XK_w,      	spawn,          {.v = (const char*[]){ "firefox", NULL } } },
	{ MODKEY,           XK_comma,  	focusmon,       {.i = -1 } },
	{ MODKEY,           XK_period, 	focusmon,       {.i = +1 } },
	{ MODKEY|SHIFT,     XK_comma,  	sendtomon,      {.i = -1 } },
	{ MODKEY|SHIFT,     XK_period, 	sendtomon,      {.i = +1 } },
	{ MODKEY,			XK_t,      	setlayout,   	 {.v = (const char[]){ "master-stack" } } },
	{ MODKEY|SHIFT,	XK_t,      	setlayout,   	 {.v = (const char[]){ "monocle" } } },
	{ MODKEY|SHIFT,	XK_f,      	setlayout,   	 {.v = (const char[]){ "floating" } } },
	{ MODKEY,           XK_h,     	incmfact,       {.f = -0.05} },
	{ MODKEY,           XK_l,      	incmfact,       {.f = +0.05} },
	{ MODKEY,           XK_o,      	incnmaster,     {.i = +1 } },
	{ MODKEY|ShiftMask, XK_o,      	incnmaster,     {.i = -1 } },
	{ MODKEY,			XK_q,      	closesel,   	 {.v = 0 } },
	{ MODKEY|SHIFT,	XK_q,      	quit,     	 {.v = 0 } },
	{ MODKEY,			XK_a,      	togglegaps, 	 {.v = 0 } },
	{ MODKEY,			XK_m,      	togglemaximize, {.v = 0 } },
	{ MODKEY|SHIFT,	XK_m,      	toggleminimize, {.v = 0 } },
	{ MODKEY,			XK_f,      	togglefullscr,  {.v = 0 } },
	{ MODKEY,			XK_j,	 	focusstack,	 {.i = 1 } },
	{ MODKEY,			XK_k,	 	focusstack,	 {.i = -1 } },
	{ MODKEY|SHIFT,	XK_j,	 	swaptiled,	 {.i = 1 } },
	{ MODKEY|SHIFT,	XK_k,	 	swaptiled,	 {.i = -1 } },
	{ MODKEY,           XK_Return, 	spawn,          {.v = termcmd } },
	{ MODKEY,           XK_d,      	spawn,          {.v = dmenucmd } },
	{ MODKEY,           XK_Tab,      	spawn,          {.v = (const char*[]){ "sws", "-a", NULL } } },
	{ MODKEY|SHIFT,	XK_BackSpace,	quit,		 {1} },
	{ MODKEY,			XK_space,  	togglesticky,   {0} },
	{ MODKEY|SHIFT,	XK_space,  	togglefloating, {0} },
	MOVERESIZEKEYS(	XK_Left,				  	 MoveLeft)
	MOVERESIZEKEYS(	XK_Right,				  	 MoveRight)
	MOVERESIZEKEYS(	XK_Up,				  	 MoveUp)
	MOVERESIZEKEYS(	XK_Down,				  	 MoveDown)
	WORKSPACEKEYS(		XK_1,                      	 1)
	WORKSPACEKEYS(		XK_2,                      	 2)
	WORKSPACEKEYS(		XK_3,                      	 3)
	WORKSPACEKEYS(		XK_4,                      	 4)
	WORKSPACEKEYS(		XK_5,                      	 5)
	WORKSPACEKEYS(		XK_6,                      	 6)
	WORKSPACEKEYS(		XK_7,                      	 7)
	WORKSPACEKEYS(		XK_8,                      	 8)
	WORKSPACEKEYS(		XK_9,                      	 9)
	{ MODKEY, 	     XK_F1,     spawn,          SHCMD(MUTE) },
	{ MODKEY,		     XK_equal,  spawn,          SHCMD(VOLUP) },
	{ MODKEY,		     XK_F3,  	 spawn,          SHCMD(VOLUP) },
	{ MODKEY,		  	XK_minus,  spawn,          SHCMD(VOLDOWN) },
	{ MODKEY,		  	XK_F2,  	 spawn,          SHCMD(VOLDOWN) },
};

Key *keys = defaultkeys;
int nkeys = LENGTH(defaultkeys);

WorkspaceRule *workspacerules;
int nwsrule = 0;

void
addkeybind(Key k)
{
	Key *tmp;

	if (keys == defaultkeys) {
		tmp = ecalloc(1, sizeof(Key));
		keys = tmp;
		nkeys = 0;
	} else {
		if (!(tmp = realloc(keys, (nkeys + 1) * sizeof(Key))))
			die("realoc: ");
		keys = tmp;
	}
	keys[nkeys++] = k;
}

void
addworkspacerule(WorkspaceRule rule)
{
	WorkspaceRule *tmp;

	if (!(tmp = realloc(workspacerules, (nwsrule + 1) * sizeof(WorkspaceRule))))
		die("realoc: ");
	workspacerules = tmp;
	workspacerules[nwsrule++] = rule;
}

void
cfg_cleanup(void)
{
	if (keys != defaultkeys)
		free(keys);
}

void
cfg_load(void)
{
	FILE *file;
	char path[512];
	char line[2048];
	char *home = getenv("HOME");
	char *eq, *trimmed, *name, *value;
	int line_no = 0;

	if(!home)
		return;

	snprintf(path, sizeof(path), "%s/.config/mithril/mithril.conf", home);
	if (access(path, R_OK) != 0)
		return;

	if (!(file = fopen(path, "r")))
		if (!(file = fopen("/etc/mithril/mithril.conf", "r")))
			return;

	while (fgets(line, sizeof(line), file)) {
		line_no++;

		trimmed = trim(line);
		if (*trimmed == '\0' || *trimmed == '#')
			continue;
		if (!(eq = strchr(trimmed, '=')))
			continue;

		*eq = '\0';
		name = trim(trimmed);
		value = trim(eq + 1);
		if (strcmp(name, "bind") == 0) {
			parsebind(value);
		} else if (strcmp(name, "ws-rule") == 0) {
			parsewsrule(value);
		} else {
			parsevar(name, value);
		}
	}
	fclose(file);
}

void
cfg_load_xresources(void)
{
	Display *display;
	char *resm;
	XrmDatabase db;
	Parser *p;

	display = XOpenDisplay(NULL);
	resm = XResourceManagerString(display);
	if (!resm)
		return;

	db = XrmGetStringDatabase(resm);
	for (p = resources; p < resources + LENGTH(resources); p++)
		resource_load(db, p->name, p->type, p->dst);
	XCloseDisplay(display);
}

int
parsearg(FuncParser *fe, char *argstr, Arg *arg)
{
	char **argv;
	char *cmd;

	if (fe->argtype == NOTHING) {
		arg->i = 0;
		return 1;
	}

	if (!argstr || *argstr == '\0')
		return 0;

	if (fe->argtype == INTEGER) {
		return parseint(argstr, &arg->i);
	} else if (fe->argtype == DOUBLE) {
		return parsedouble(argstr, &arg->f);
	} else if (fe->argtype == STRING) {
		if (!(arg->v = strdup(argstr)))
			return 0;
		return 1;
	} else if (fe->argtype == SPAWN) {
		if (!(cmd = strdup(argstr)))
			return 0;
		if (!(argv = malloc(4 * sizeof(char *)))) {
			free(cmd);
			return 0;
		}
		argv[0] = "/bin/sh";
		argv[1] = "-c";
		argv[2] = cmd;
		argv[3] = NULL;
		arg->v = argv;
		return 1;
	}

	return 0;
}

void
parsebind(char *value)
{
	char buf[2048];
	char *open, *close, *p;
	char *modstr, *keystr, *cmdstr, *argstr;
	unsigned int mod;
	KeySym keysym;
	FuncParser *fe = NULL;
	Arg arg = {0};
	Key k;
	size_t i;

	strncpy(buf, value, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';

	if (!(open = strchr(buf, '{')) || !(close = strrchr(buf, '}')))
		return;

	*close = '\0';
	p = open + 1;

	modstr = strsep(&p, ",");
	if (!p)
		return;

	keystr = strsep(&p, ",");
	if (!p)
		return;

	cmdstr = strsep(&p, ",");
	argstr = p; /* arg - may be NULL */

	modstr = trim(modstr);
	keystr = trim(keystr);
	cmdstr = trim(cmdstr);
	if (argstr)
		argstr = trim(argstr);

	if (!parsemod(modstr, &mod))
		return;
	if (*keystr == '\0' || (keysym = XStringToKeysym(keystr)) == NoSymbol)
		return;

	for (i = 0; i < LENGTH(funcs); i++) {
		if (strcmp(cmdstr, funcs[i].name) == 0) {
			fe = &funcs[i];
			break;
		}
	}
	if (!fe)
		return;
	if (!parsearg(fe, argstr, &arg))
		return;

	k.mod = mod;
	k.keysym = keysym;
	k.func = fe->func;
	k.arg = arg;
	addkeybind(k);
}

int
parsedouble(const char *s, double *result)
{
	char *end;
	double value;

	errno = 0;
	value = strtod(s, &end);

	if (s == end || *trim(end) != '\0' ||
	    errno == ERANGE)
		return 0;

	*result = value;
	return 1;
}

int
parseint(const char *s, int *result)
{
	char *end;
	long value;

	errno = 0;
	value = strtol(s, &end, 10);

	if (s == end || *trim(end) != '\0' ||
	    errno == ERANGE || value < INT_MIN || value > INT_MAX)
		return 0;

	*result = (int)value;
	return 1;
}

int
parsemod(char *modstr, unsigned int *mod)
{
	char buf[128];
	char *tok, *p;
	size_t i;
	int found, matched = 0;

	*mod = 0;
	strncpy(buf, modstr, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';

	p = buf;
	while ((tok = strsep(&p, "|"))) {
		tok = trim(tok);
		if (*tok == '\0')
			continue;

		found = 0;
		for (i = 0; i < LENGTH(mods); i++) {
			if (strcasecmp(tok, mods[i].name) == 0) {
				*mod |= mods[i].mask;
				found = 1;
				matched = 1;
				break;
			}
		}
		if (!found)
			return 0; /* invalid bind */
	}
	return matched;
}

void
parsevar(char *name, char *value)
{
	for (size_t i = 0; i < LENGTH(config); i++) {
		if (strcmp(name, config[i].name) != 0)
			continue;
		printf("\nmatch: %s with name: %s", config[i].name, name);
          if (config[i].type == INTEGER) {
               *(int *)config[i].dst = atoi(value);
		} else if (config[i].type == DOUBLE) {
              *(double *)config[i].dst = atof(value);
		} else if (config[i].type == STRING) {
               strcpy((char *)config[i].dst, value);
          	//((char *)config[i].dst)[255] = '\0';
		}
		break;
	}
}

void
parsewsrule(char *value)
{
	char buf[2048];
	char *open, *close, *p;
	char *numstr, *namestr, *ltstr;
	char *gapstr, *mfactstr, *nmasterstr;
	WorkspaceRule rule;

	strncpy(buf, value, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';

	if (!(open = strchr(buf, '{')) || !(close = strrchr(buf, '}')))
		return;

	*close = '\0';
	p = open + 1;

	numstr     = strsep(&p, ",");
	namestr    = strsep(&p, ",");
	ltstr      = strsep(&p, ",");
	gapstr     = strsep(&p, ",");
	mfactstr   = strsep(&p, ",");
	nmasterstr = strsep(&p, ",");

	if (!numstr || !namestr || !ltstr ||
	    !gapstr || !mfactstr || !nmasterstr)
		return;

	numstr     = trim(numstr);
	namestr    = trim(namestr);
	ltstr      = trim(ltstr);
	gapstr     = trim(gapstr);
	mfactstr   = trim(mfactstr);
	nmasterstr = trim(nmasterstr);

	if (!*namestr || !*ltstr)
		return;
	if (!parseint(numstr, &rule.num))
		return;
	if (!parseint(gapstr, &rule.gappx))
		return;
	if (!parsedouble(mfactstr, &rule.mfact))
		return;
	if (!parseint(nmasterstr, &rule.nmaster))
		return;
	if (p && *trim(p))
		return;
	rule.name = strdup(namestr);
	rule.layout = strdup(ltstr);

	if (!rule.name || !rule.layout) {
		free(rule.name);
		free(rule.layout);
		return;
	}

	addworkspacerule(rule);
}

void
resource_load(XrmDatabase db, char *name, int rtype, void *dst)
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
		else if (rtype == DOUBLE)
			*fdst = strtof(ret.addr, NULL);
	}
}

char
*trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    if (*s == 0)
	    return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return s;
}

