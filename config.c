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
#include "extern.h"
#include "config.h"

#define GRAY		"#ff999999"
#define GRAYDARK	"#ff222222"
#define WHITE		"#ffe0e0e0"
#define WHITEALT	"#ffffffff"
#define RED		"#fff1212c"
#define REDALT		"#fff1414c"
#define BLUE		"#ff818cf1"

enum { NOTHING, INTEGER, DOUBLE, STRING, SPAWN };

typedef struct {
	char *name;
	int type;
	void *dst;
} Parser;

static void addclientrule(ClientRule rule);
static void addworkspacerule(WorkspaceRule rule);
static int findpath(char *out, size_t outsz);
static void loadresource(XrmDatabase db, char *name, int rtype, void *dst);
static char *nextfield(char **p);
static void parseclientrule(char *value);
static void parsevar(char *name, char *value);
static void parsewsrule(char *value);
static char *trim(char *s);

double mfact = 0.55; /* factor of master area size [0.05..0.95] */
int nmaster = 1;
int gappx = 10;
char layout[] = "master-stack";
int movestep = 20;
int refreshrate = 120;  /* refresh rate (per second) for client move/resize */
unsigned int workspaces 	  = 7;

int notiledtitle		  = 0;
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
	{ "notiledtitle", 		INTEGER, &notiledtitle },
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

ClientRule *clientrules;
WorkspaceRule *workspacerules;
int nwsrule = 0;
int ncrule = 0;

void
addclientrule(ClientRule rule)
{
	ClientRule *tmp;

	if (!(tmp = realloc(clientrules, (ncrule + 1) * sizeof(ClientRule))))
		die("realoc: ");
	clientrules = tmp;
	clientrules[ncrule++] = rule;
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
	int i;

	for (i = 0; i < nwsrule; i++) {
		free(workspacerules[i].name);
		free(workspacerules[i].mon);
		free(workspacerules[i].layout);
	}
	free(workspacerules);
	workspacerules = NULL;
	nwsrule = 0;

	for (i = 0; i < ncrule; i++) {
		free(clientrules[i].iname);
		free(clientrules[i].mon);
		free(clientrules[i].classg);
	}
	free(clientrules);
	clientrules = NULL;
	ncrule = 0;
}

void
cfg_load(void)
{
	FILE *file;
	char path[512];
	char line[2048];
	char *eq, *trimmed, *name, *value;

	if(!(findpath(path, sizeof(path))))
		return;
	if (!(file = fopen(path, "r")))
		return;

	while (fgets(line, sizeof(line), file)) {
		trimmed = trim(line);
		if (*trimmed == '\0' || *trimmed == '#')
			continue;
		if (!(eq = strchr(trimmed, '=')))
			continue;

		*eq = '\0';
		name = trim(trimmed);
		value = trim(eq + 1);
		if (strcmp(name, "ws-rule") == 0)
			parsewsrule(value);
		else if (strcmp(name, "client-rule") == 0)
			parseclientrule(value);
		else
			parsevar(name, value);
	}
	fclose(file);
}

void
cfg_load_xresources(void)
{
	char *resm;
	XrmDatabase db;
	Parser *p;

	resm = XResourceManagerString(dpy);
	if (!resm)
		return;

	db = XrmGetStringDatabase(resm);
	for (p = config; p < config + LENGTH(config); p++)
		loadresource(db, p->name, p->type, p->dst);
	XrmDestroyDatabase(db);
}

int
findpath(char *out, size_t outsz)
{
	const char *xdg = getenv("XDG_CONFIG_HOME");
	const char *home = getenv("HOME");

	if (xdg && *xdg) {
		snprintf(out, outsz, "%s/mithril/mithril.conf", xdg);
		if (access(out, R_OK) == 0)
			return 1;
	}
	if (home && *home) {
		snprintf(out, outsz, "%s/.config/mithril/mithril.conf", home);
		if (access(out, R_OK) == 0)
			return 1;
	}
	return 0;
}

void
loadresource(XrmDatabase db, char *name, int rtype, void *dst)
{
	char fullname[256];
	char *type;
	XrmValue ret;

	snprintf(fullname, sizeof(fullname), "%s.%s", "mithril", name);
	fullname[sizeof(fullname) - 1] = '\0';

	if (!XrmGetResource(db, fullname, "*", &type, &ret))
		return;
	if (strcmp(type, "String") != 0)
		return;

	if (rtype == STRING)
          strcpy(dst, ret.addr);
	else if (rtype == INTEGER)
          parseint(ret.addr, dst);
	else if (rtype == DOUBLE)
		parsedouble(ret.addr, dst);
}

char *
nextfield(char **p)
{
    char *field;

    if (!p || !*p)
        return NULL;

    field = strsep(p, ",");
    return trim(field);
}

void
parseclientrule(char *value)
{
	char buf[2048];
	char *open, *close, *p;
	char *classstr, *namestr, *monstr, *wsstr;
	char *floatstr, *stickystr, *decstr;
	ClientRule rule;

	strncpy(buf, value, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';

	if (!(open = strchr(buf, '{')) || !(close = strrchr(buf, '}')))
		return;

	*close = '\0';
	p = open + 1;

	classstr = nextfield(&p);
	namestr = nextfield(&p);
	monstr = nextfield(&p);
	wsstr = nextfield(&p);
	floatstr = nextfield(&p);
	stickystr = nextfield(&p);
	decstr = nextfield(&p);

	if (!classstr || !namestr || !monstr || !wsstr ||
	    !floatstr || !stickystr || !decstr)
		return;

	if (!*namestr || !*monstr || !classstr)
		return;
	if (!parseint(wsstr, &rule.ws))
		return;
	if (!parseint(floatstr, &rule.floating))
		return;
	if (!parseint(stickystr, &rule.sticky))
		return;
	if (!parseint(decstr, &rule.decor))
		return;
	if (p && *trim(p))
		return;
	rule.iname = strdup(namestr);
	rule.mon = strdup(monstr);
	rule.classg = strdup(classstr);

	if (!rule.iname || !rule.mon || !rule.classg) {
		free(rule.iname);
		free(rule.mon);
		free(rule.classg);
		return;
	}
	addclientrule(rule);
}

void
parsevar(char *name, char *value)
{
	for (size_t i = 0; i < LENGTH(config); i++) {
		if (strcmp(name, config[i].name) != 0)
			continue;
          if (config[i].type == INTEGER) {
               parseint(value, (int *)config[i].dst);
		} else if (config[i].type == DOUBLE) {
			parsedouble(value, (double *)config[i].dst);
		} else if (config[i].type == STRING) {
               strcpy((char *)config[i].dst, value);
		}
		break;
	}
}

void
parsewsrule(char *value)
{
	char buf[2048];
	char *open, *close, *p;
	char *numstr, *namestr, *monstr, *ltstr;
	char *gapstr, *mfactstr, *nmasterstr;
	WorkspaceRule rule;

	strncpy(buf, value, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';

	if (!(open = strchr(buf, '{')) || !(close = strrchr(buf, '}')))
		return;

	*close = '\0';
	p = open + 1;

	numstr = nextfield(&p);
	namestr = nextfield(&p);
	monstr = nextfield(&p);
	ltstr = nextfield(&p);
	gapstr = nextfield(&p);
	mfactstr = nextfield(&p);
	nmasterstr = nextfield(&p);

	if (!numstr || !namestr || !monstr || !ltstr ||
	    !gapstr || !mfactstr || !nmasterstr)
		return;

	if (!*namestr || !*monstr || !*ltstr)
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
	rule.mon = strdup(monstr);
	rule.layout = strdup(ltstr);

	if (!rule.name || !rule.mon || !rule.layout) {
		free(rule.name);
		free(rule.mon);
		free(rule.layout);
		return;
	}
	addworkspacerule(rule);
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

