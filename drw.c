#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <X11/Xlib.h>
#include <cairo/cairo-xlib.h>
#include "util.h"
#include "drw.h"

void
drw_button(Drw *drw, char *text, int x, int y, int w, int h, int r, int linew)
{
	double ty = (int)round(y + (h - (drw->fnt->ascent + drw->fnt->descent)) / 2.0 + drw->fnt->ascent);
	double tx = (int)round(x + (w - drw_text_getwidth(drw->fnt, text)) / 2);

	drw_rect(drw, x, y, w, h, r, 1, 0);
	if (linew)
		drw_rect(drw, x, y, w, h, r, 0, linew);
	drw_text(drw, text, tx, ty);

	return;
}

void
drw_color_create(char *hex, double *rgba)
{
	unsigned int a, ri, gi, bi;

	if (sscanf(hex + 1, "%02x%02x%02x%02x", &a, &ri, &gi, &bi) != 4) {
		if (sscanf(hex + 1, "%02x%02x%02x", &ri, &gi, &bi) != 3) {
			ri = 255;
			gi = 0;
			bi = 255;
		}
		a = 255;
	}

	rgba[0] = ri / 255.0;
	rgba[1] = gi / 255.0;
	rgba[2] = bi / 255.0;
	rgba[3] = a / 255.0;
}

Drw *
drw_create(Surf *s)
{
	Drw *drw;

	drw = ecalloc(1, sizeof(Drw));
	drw->surf = s;
	drw->cr = cairo_create(s);
	//cairo_set_operator(drw->cr, CAIRO_OPERATOR_SOURCE);

	return drw;
}

void
drw_destroy(Drw *drw)
{
	cairo_destroy(drw->cr);
	cairo_surface_flush(drw->surf);
	free(drw);
}

Fnt*
drw_font_create(char *fontname, double fontsize)
{
    cairo_font_extents_t fe;
    cairo_surface_t *tmp;
    cairo_t *cr;
    Fnt *f;
    tmp = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    cr = cairo_create(tmp);
    f = ecalloc(1, sizeof(Fnt));
    cairo_select_font_face(cr, fontname, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, fontsize);
    cairo_font_extents(cr, &fe);
    f->name = fontname;
    f->size = fontsize;
    f->h = fe.height;
    f->ascent = fe.ascent;
    f->descent = fe.descent;
    cairo_destroy(cr);
    cairo_surface_destroy(tmp);
    return f;
}

unsigned long
drw_rgba_to_argb(double color[4])
{
	return ((unsigned long)(color[3] * 255.0 + 0.5) << 24) |
		  ((unsigned long)(color[0] * 255.0 + 0.5) << 16) |
		  ((unsigned long)(color[1] * 255.0 + 0.5) <<  8) |
		  (unsigned long)(color[2] * 255.0 + 0.5);
}

void
drw_resize(Surf *s, int w, int h)
{
	cairo_xlib_surface_set_size(s, w, h);
}

void
drw_rect(Drw *drw, int x, int y, int w, int h, int r, int fill, int linew)
{
	double degrees = M_PI / 180.0;

	drw_set_color(drw, drw->scm[fill ? ColBg : ColBorder]);

	/* clamp radius so it never exceeds half the smaller dimension */
	if (r > w / 2) r = w / 2;
	if (r > h / 2) r = h / 2;

	if (r <= 0) {
		/* fall back to a plain rectangle */
		cairo_rectangle(drw->cr, x, y, w, h);
	} else {
		cairo_new_sub_path(drw->cr);
		cairo_arc(drw->cr, x + w - r, y + r,     r, -90 * degrees,   0 * degrees);
		cairo_arc(drw->cr, x + w - r, y + h - r, r,   0 * degrees,  90 * degrees);
		cairo_arc(drw->cr, x + r,     y + h - r, r,  90 * degrees, 180 * degrees);
		cairo_arc(drw->cr, x + r,     y + r,     r, 180 * degrees, 270 * degrees);
		cairo_close_path(drw->cr);
	}

	if (fill) {
		cairo_fill(drw->cr);
	} else if (linew > 0) {
		cairo_set_line_width(drw->cr, linew);
		cairo_stroke(drw->cr);
	}
}

void
drw_set_color(Drw *drw, double *col)
{
	cairo_set_source_rgba(drw->cr, col[0], col[1], col[2], col[3]);
}

void
drw_set_font(Drw *drw, Fnt *fnt)
{
	cairo_select_font_face(drw->cr, fnt->name, CAIRO_FONT_SLANT_NORMAL,
			CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(drw->cr, fnt->size);
	drw->fnt = fnt;
}

void
drw_set_scheme(Drw *drw, double scheme[3][4])
{
	if (drw) drw->scm = scheme;
}

Surf *
drw_surf_create(Display *dpy, Drawable d, Visual *v, int w, int h)
{
	return cairo_xlib_surface_create(dpy, d, v, w, h);
}

void
drw_surf_destroy(Surf *s)
{
	cairo_surface_destroy(s);
}

void
drw_text(Drw *drw, char *text, int x, int y)
{
	drw_set_color(drw, drw->scm[ColFg]);
	cairo_move_to(drw->cr, x, y);
	cairo_show_text(drw->cr, text);
}

void
drw_text_clamp(Drw *drw, char *text, int w, size_t textsize)
{
	const char *ellipsis = "...";
	char buf[512];
	int avail, len = 0, newlen;
	cairo_text_extents_t cur, te, ellipsis_te;

	cairo_text_extents(drw->cr, ellipsis, &ellipsis_te);
	cairo_text_extents(drw->cr, text, &te);

	if (te.x_advance <= w)
		return;

	avail = w - ellipsis_te.x_advance;
	if (avail <= 0) {
		text[0] = '\0';
		return;
	}

	while (text[len]) {
		newlen = len + 1;
		while (text[newlen] && (text[newlen] & 0xC0) == 0x80)
			newlen++;

		snprintf(buf, sizeof(buf), "%.*s", newlen, text);
		cairo_text_extents(drw->cr, buf, &cur);

		if (cur.x_advance > avail)
			break;

		len = newlen;
	}

	snprintf(buf, sizeof(buf), "%.*s%s", len, text, ellipsis);
	strncpy(text, buf, textsize - 1);
	text[textsize - 1] = '\0';
}

int
drw_text_getwidth(Fnt *fnt, char *text)
{
	cairo_text_extents_t te;
	cairo_surface_t *tmp;
	cairo_t *cr;

	tmp = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
	cr = cairo_create(tmp);

	cairo_select_font_face(cr, fnt->name, CAIRO_FONT_SLANT_NORMAL,
			CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, fnt->size);
	cairo_text_extents(cr, text, &te);

	cairo_destroy(cr);
	cairo_surface_destroy(tmp);

	return te.x_advance;
}

unsigned long
drw_x11_color_create(char *hex)
{
	unsigned int clr;

	if (!sscanf(hex + 1, "%08x", &clr))
		if (!sscanf(hex + 1, "%06x", &clr))
			return 0xffff00ff;
	return clr | 0xff << 24;
}

