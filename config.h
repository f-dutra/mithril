#define GRAY		"#ff999999"
#define GRAYDARK	"#ff222222"
#define WHITE		"#ffe0e0e0"
#define WHITEALT	"#ffffffff"
#define RED		"#fff1212c"
#define REDALT		"#fff1414c"
#define BLUE		"#ff818cf1"

#define MODKEY	Mod4Mask
#define SHIFT	ShiftMask

static const int movestep = 20;
static const float mfact = 0.55; /* factor of master area size [0.05..0.95] */
static const int nmaster = 1;
static const int gappx = 10;
static long unsigned int refreshrate = 120;  /* refresh rate (per second) for client move/resize */
static unsigned int workspaces = 7;
static const int decorhints  = 1;    /* 1 means respect decoration hints */

static int borderpx 		    = 1;	/* frame border */
static int verticaltitle		    = 0;
static int inverttitlebar	    = 0;
static int titleborderpx 	    = 0;	/* border between the titlebar and client window */
static char titlefont[] 	         = "monospace";	/* font used on window titles */
static double fontsize 	         = 13.00;
static int outerpad			    = 6;	/* titlebar padding */
static int invertbuttons 	    = 0;	/* 1 means buttons start on the left */
static int lrpad		 	    = 10; /* padding between buttons and text */
static int centeredtitle  	    = 1;  /* 0 means window title on the left corner */
static int offset_y			    = 1;  /* offset elements downwards */
static int buttonradius		    = 2;	/* button roundness */
static int buttonwidth		    = 11;
static int buttonheight		    = 11;

/* You can use a nerd font for button icons */
static char iconfont[] 	         = "monospace";
static double iconsize 	         = 10.00;
static char btn_close_icn[] 	    = "";
static char btn_maximize_icn[]    = "";
static char btn_minimize_icn[]    = "";
static int buttonborderpx	    = 0;	/* border/outline arround the buttons */

/* Colors */
/* The WM supports both rgb and argb hex colors */
static char bgnorm[]		    = "#ff222222"; /* norm means unfocused window */
static char bgsel[]	     	    = "#ff2a2a2a"; /* sel means focused window */

static char fgnorm[]    		    = GRAY;
static char fgsel[]     		    = WHITE;

static char bordernorm[]		    = "#ff555555";
static char bordersel[]		    = "#ff6a6a6a";
static char borderswaporig[]	    = "#ffeeeeee";
static char borderswapdest[]	    = RED;

static char titlebordernorm[]	    = "#ff111111";
static char titlebordersel[]	    = "#ff151515";

/* close button */
static char closefgnorm[] 	    = GRAY;
static char closefgsel[] 	    = GRAYDARK;
static char closefghover[] 	    = GRAYDARK;

static char closebgnorm[]	    = GRAY;
static char closebgsel[]	         = RED;
static char closebghover[]        = REDALT;

static char closebordernorm[]	    = GRAY;
static char closebordersel[]	    = RED;
static char closeborderhover[]    = REDALT;

/* maximize button */
static char maximizefgnorm[]      = GRAY;
static char maximizefgsel[]	    = GRAYDARK;
static char maximizefghover[]     = GRAYDARK;

static char maximizebgnorm[]      = GRAY;
static char maximizebgsel[]	    = WHITE;
static char maximizebghover[]	    = WHITEALT;

static char maximizebordernorm[]  = GRAY;
static char maximizebordersel[]   = WHITE;
static char maximizeborderhover[] = WHITEALT;

/* minimize button */
static char minimizefgnorm[]      = GRAY;
static char minimizefgsel[] 	    = WHITE;
static char minimizefghover[]     = GRAYDARK;

static char minimizebgnorm[]	    = GRAY;
static char minimizebgsel[]	    = WHITE;
static char minimizebghover[]	    = WHITEALT;

static char minimizebordernorm[]  = GRAY;
static char minimizebordersel[]   = WHITE;
static char minimizeborderhover[] = WHITEALT;

/* The WM uses this to convert the hex (a)rgb strings into cairo rgba colors */
/* borders are set with pure xlib, so they aren't included here */
/* drw.c expects a scheme to have 3 colors */
static char *colors[SchemeLast][3] = {
	[SchemeNorm]	   = { fgnorm, bgnorm, titlebordernorm },
	[SchemeSel]	   = { fgsel, bgsel, titlebordersel  },
	[SchemeCloseNorm] = { closefgnorm, closebgnorm, closebordernorm },
	[SchemeMaxNorm]   = { maximizefgnorm, maximizebgnorm, maximizebordernorm },
	[SchemeMinNorm]   = { minimizefgnorm, minimizebgnorm, minimizebordernorm },
	[SchemeCloseSel]  = { closefgsel, closebgsel, closebordersel },
	[SchemeMaxSel]    = { maximizefgsel, maximizebgsel, maximizebordersel },
	[SchemeMinSel]    = { minimizefgsel, minimizebgsel, minimizebordersel },
	[SchemeCloseHvr]  = { closefghover, closebghover, closeborderhover },
	[SchemeMaxHvr]    = { maximizefghover, maximizebghover, maximizeborderhover },
	[SchemeMinHvr]    = { minimizefghover, minimizebghover, minimizeborderhover },
};

/* used to read and write the values from xresources */
ResourcePref resources[] = {
	{ "borderpx",            INTEGER, &borderpx },
	{ "titleborderpx",       INTEGER, &titleborderpx },
	{ "titlefont",           STRING,  &titlefont },
	{ "fontsize",            FLOAT,   &fontsize },
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

static const Layout layouts[] = {
	{ "<><", NULL },
	{ "[]=", tile },
};

/* Default values for each workspace */
Workspace workspace_rules[] = {
	/* name	layout 	gappx	mfact  nmaster */
	{ "1", 	1, 		gappx,	mfact, nmaster },
	{ "2", 	1, 		gappx,	mfact, nmaster },
	{ "3", 	1, 		gappx,	mfact, nmaster },
	{ "4", 	1, 		gappx,	mfact, nmaster },
	{ "5", 	1, 		gappx,	mfact, nmaster },
	{ "6", 	1, 		gappx,	mfact, nmaster },
	{ "7", 	1, 		gappx,	mfact, nmaster },
	{ "8", 	1, 		gappx,	mfact, nmaster },
	{ "9", 	1, 		gappx,	mfact, nmaster },
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
static const Key keys[] = {
	{ MODKEY,           XK_w,      	spawn,          {.v = (const char*[]){ "firefox", NULL } } },
	{ MODKEY|SHIFT,     XK_w,      	spawn,          {.v = (const char*[]){ "firefoxprofile", NULL } } },
	{ MODKEY,           XK_v,      	spawn,          {.v = (const char*[]){ "dmenurecord", NULL } } },
	{ MODKEY,                       XK_comma,  focusmon,       {.i = -1 } },
	{ MODKEY,                       XK_period, focusmon,       {.i = +1 } },
	{ MODKEY|SHIFT,                    XK_comma,  sendtomon,       {.i = -1 } },
	{ MODKEY|SHIFT,                    XK_period, sendtomon,       {.i = +1 } },
	{ MODKEY,			XK_t,      	setlayout,   	 {.i = 1 } },
	{ MODKEY|SHIFT,	XK_f,      	setlayout,   	 {.i = 0 } },
	{ MODKEY,           XK_h,     	incmfact,       {.f = -0.05} },
	{ MODKEY,           XK_l,      	incmfact,       {.f = +0.05} },
	{ MODKEY,           XK_o,      	incnmaster,     {.i = +1 } },
	{ MODKEY|ShiftMask, XK_o,      	incnmaster,     {.i = -1 } },
	{ MODKEY,			XK_q,      	closesel,   	 {.v = 0 } },
	{ MODKEY|SHIFT,	XK_q,      	quit,     	 {.v = 0 } },
	{ MODKEY,			XK_a,      	togglegaps, {.v = 0 } },
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
	{ MODKEY, 	     XK_BackSpace, spawn,       {.v = (const char*[]){ "sysact", NULL } }},
	{ MODKEY|ShiftMask,	XK_equal,  spawn,          {.v = (const char*[]){ "sudo", "xbacklight", "-inc", "5", NULL } } },
	{ MODKEY|ShiftMask,	XK_minus,  spawn,          {.v = (const char*[]){ "sudo", "xbacklight", "-dec", "5", NULL } } },
	MOVERESIZEKEYS(	XK_Left,				  	MoveLeft)
	MOVERESIZEKEYS(	XK_Right,				  	MoveRight)
	MOVERESIZEKEYS(	XK_Up,				  	MoveUp)
	MOVERESIZEKEYS(	XK_Down,				  	MoveDown)
	WORKSPACEKEYS(		XK_1,                      	0)
	WORKSPACEKEYS(		XK_2,                      	1)
	WORKSPACEKEYS(		XK_3,                      	2)
	WORKSPACEKEYS(		XK_4,                      	3)
	WORKSPACEKEYS(		XK_5,                      	4)
	WORKSPACEKEYS(		XK_6,                      	5)
	WORKSPACEKEYS(		XK_7,                      	6)
	WORKSPACEKEYS(		XK_8,                      	7)
	WORKSPACEKEYS(		XK_9,                      	8)
	{ MODKEY, 	     XK_F1,     spawn,          SHCMD(MUTE) },
	{ MODKEY,		     XK_equal,  spawn,          SHCMD(VOLUP) },
	{ MODKEY,		     XK_F3,  	 spawn,          SHCMD(VOLUP) },
	{ MODKEY,		  	XK_minus,  spawn,          SHCMD(VOLDOWN) },
	{ MODKEY,		  	XK_F2,  	 spawn,          SHCMD(VOLDOWN) },
};

static const Button buttons[] = {
	{ ClkTitle,		0,         	Button1,        movemouse, {0} },
	{ ClkTitle,		MODKEY,    	Button1,        movemouse, {0} },
	{ ClkTitle,		0,         	Button2,        togglefloating, {0} },
	{ ClkTitle,		0,         	Button3,        swapmouse, {0} },
	{ ClkTitle,		MODKEY|SHIFT,  Button3,        swapmouse, {0} },
	{ ClkTitle,		MODKEY,    	Button2,        togglefloating, {0} },
	{ ClkTitle,		MODKEY,    	Button3,        resizemouse, {-2} },
	{ ClkResize, 		0,         	Button1,        resizemouse, {0} },
	{ ClkClose,         0,         	Button1,        closesel, {0} },
	{ ClkMax,	          0,         	Button1,        togglemaximize, {0} },
	{ ClkMin,	          0,         	Button1,        toggleminimize, {0} },
	{ ClkClientWin,	MODKEY,    	Button1,        movemouse, {0} },
	{ ClkClientWin,	MODKEY,    	Button2,        togglefloating, {0} },
	{ ClkClientWin,	MODKEY,     	Button3,        resizemouse, {-1} },
	{ ClkClientWin,	MODKEY|SHIFT,	Button3,        swapmouse, {0} },
};

