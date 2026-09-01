typedef struct {
	char *name;
	int type;
	void *dst;
} Parser;

typedef struct {
	char *classg;
	int ws;
	int floating;
	int monitor;
} Rule;

typedef struct {
	int num;
	char *name;
	char *layout;
	int gappx;
	double mfact;
	int nmaster;
} WorkspaceRule;

void cfg_cleanup(void);
void cfg_load(void);
void cfg_load_xresources(void);

extern double mfact;
extern int nmaster;
extern int gappx;

extern char layout[];
extern Key *keys;
extern int nkeys;
extern WorkspaceRule *workspacerules;
extern int nwsrule;

extern int movestep;
extern long unsigned int refreshrate;
extern unsigned int workspaces;
extern int decorhints;

extern int borderpx;
extern int verticaltitle;
extern int inverttitlebar;
extern int titleborderpx;
extern int titleheight;
extern char titlefont[];
extern double fontsize;
extern int outerpad;
extern int invertbuttons;
extern int lrpad;
extern int centeredtitle;
extern int offset_y;
extern int buttonradius;
extern int buttonwidth;
extern int buttonheight;

extern char iconfont[];
extern double iconsize;
extern char btn_close_icn[];
extern char btn_maximize_icn[];
extern char btn_minimize_icn[];
extern int buttonborderpx;

extern char bgnorm[];
extern char bgsel[];

extern char fgnorm[];
extern char fgsel[];

extern char bordernorm[];
extern char bordersel[];
extern char borderswaporig[];
extern char borderswapdest[];

extern char titlebordernorm[];
extern char titlebordersel[];

extern char closefgnorm[];
extern char closefgsel[];
extern char closefghover[];

extern char closebgnorm[];
extern char closebgsel[];
extern char closebghover[];

extern char closebordernorm[];
extern char closebordersel[];
extern char closeborderhover[];

extern char maximizefgnorm[];
extern char maximizefgsel[];
extern char maximizefghover[];

extern char maximizebgnorm[];
extern char maximizebgsel[];
extern char maximizebghover[];

extern char maximizebordernorm[];
extern char maximizebordersel[];
extern char maximizeborderhover[];

extern char minimizefgnorm[];
extern char minimizefgsel[];
extern char minimizefghover[];

extern char minimizebgnorm[];
extern char minimizebgsel[];
extern char minimizebghover[];

extern char minimizebordernorm[];
extern char minimizebordersel[];
extern char minimizeborderhover[];

