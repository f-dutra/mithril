#include <cairo/cairo.h>
#define	TEXTW(x)	(drw_text_getwidth(font, (x)))

enum { ColFg, ColBg, ColBorder };

typedef struct {
	char *name;
	double size, h, ascent, descent;
} Fnt;

typedef cairo_surface_t Surf;

typedef struct {
	cairo_t *cr;
	Surf *surf;
	Fnt *fnt;
	double (*scm)[4];
} Drw;

void drw_button(Drw *drw, char *text, int x, int y, int w, int h, int r, int linew);
void drw_color_create(char *hex, double *rgba);
Fnt *drw_font_create(char *fontname, double fontsize);
Drw *drw_create(Surf *s);
void drw_destroy(Drw *drw);
void drw_rect(Drw *drw, int x, int y, int w, int h, int r, int fill, int linew);
void drw_resize(Surf *s, int w, int h);
void drw_set_font(Drw *drw, Fnt *fnt);
void drw_set_color(Drw *drw, double *col);
void drw_set_scheme(Drw *drw, double scheme[3][4]);
void drw_scheme_setup();
Surf *drw_surf_create(Display *dpy, Drawable d, Visual *v, int w, int h);
void drw_surf_destroy(Surf *s);
void drw_text(Drw *drw, char *text, int x, int y);
void drw_text_clamp(Drw *drw, char *text, int w, size_t textsize);
int drw_text_getwidth(Fnt *fnt, char *text);
unsigned long drw_rgba_to_argb(double color[4]);
unsigned long drw_x11_color_create(char *clr);

