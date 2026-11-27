/*
 * librudolf-ui - what every native app of Rudolf OS needs and the desktop page already has (1.6.2, docs/plan-1.6.2.md Part 39
 * step 1; docs/plan-2.0.md "How a native app is built"). C and GTK 3, as rudolf-native; linked into each app (/usr/bin/rudolf-APP).
 *
 *   - the look: a GTK 3 CSS theme made at build time from the page's own styles (mkui.py: buttons, fields, the tabs, headings,
 *     the message line, dialogs; light and dark; the accent colour by the page's rules, acVariant and acInk; Display Properties'
 *     font, its size and the display scaling), and the controls GTK cannot draw as the page does, drawn here in the page's
 *     values (a check box as Chromium's, the table of .tmt with its sorted headings, groups and selected row, the scroll bars as
 *     Chromium's, the right-click menu as the page's cmenu); so a native app looks like its web version, not a stock GTK app;
 *   - the settings, from rudolfd (desktop/settings: the account's, as the native shell reads them) and followed live (each
 *     change of the page's settings, "gen", applied at once to every window);
 *   - rudolfd's client: the token as rudolf-native gets it (from rudolfd's page, GET /, which rudolfd gives only to the desktop's
 *     own processes: its peer check), HTTP on 127.0.0.1, JSON (rudolf-native's own reader, taken from its source by mkui.py);
 *   - dialogs in Rudolf's look: Confirm (the page's sdlg), the administrator's permission (the page's elevate, the password
 *     checked by rudolfd: desktop/admin, so no app ever holds a password hash), the about box;
 *   - the translations: the page's language tables, as the page translates its writing (trText).
 * Security: a native app is a program of the desktop's: rudolf-native starts it, so it runs as the account's Linux user inside the
 * desktop's memory group (/sys/fs/cgroup/desktop), which is what rudolfd's peer check asks of every request; a program rudolfd
 * starts (a Linux app, Wine, Flatpak) is outside that group and is refused the token and the API as before. rudolfd's
 * confinement knows a native app by its program (/proc/PID/exe, as it knows rudolf-native) and lets it ask only for what its
 * code asks for (rudolfd-winapi.h's WINAPI_APPS, made by iso/systools/winapi.py from iso/sys/ui and iso/sys/apps).
 * GPL-2.0-or-later.
 */
#ifndef RUDOLF_UI_H
#define RUDOLF_UI_H
#include <gtk/gtk.h>

/* ---- start, settings */
typedef struct {
	char accent[16], acfg[8], ff[64], lang[8], tx[16], dm[16];
	int dark, fs, hc, scale;   /* dark: the page's html.dark (S.darkui and light or dark); fs: Display Properties' font size (px) */
	double z, gen;             /* z: the display scaling (scale / 100) */
	char family[96];           /* the Font setting's family as the page writes it (uiFontStack) */
} RuiSettings;
extern RuiSettings RUI;
/* gtk_init, rudolfd's token, the settings and the theme; 0 when rudolfd did not give the token (not the desktop's own program) */
int rui_init(int *argc, char ***argv, const char *app);
void rui_on_settings(void (*cb)(void *d), void *d);   /* called after the settings changed (the theme is already applied) */
double rui_px(double css_px);                          /* a page's CSS pixel on this screen (the display scaling) */
const char *rui_tr(const char *en);                    /* the page's translation of an English text, or the text */
const char *rui_version(void);                         /* the page's RVER (1.6.2) */
const char *rui_version_date(void);                    /* the page's RVERDATE */

/* ---- rudolfd */
typedef struct RuiJ RuiJ;   /* a JSON value (rudolf-native's JV) */
int rui_http(const char *method, const char *path, const char *body, size_t blen, char **out, size_t *outn);   /* the status (0: no answer) */
int rui_http_h(const char *method, const char *path, const char *headers, const char *body, size_t blen, char **out, size_t *outn);   /* headers: "Name: value\r\n"... */
RuiJ *rui_get(const char *path);                       /* GET, 200 and JSON: the value, else NULL */
int rui_post(const char *path, const char *body, char *err, size_t errn);   /* the status; err: rudolfd's "error" */
void rui_jfree(RuiJ *j);
RuiJ *rui_jprop(RuiJ *o, const char *k);
RuiJ *rui_jat(RuiJ *a, int i);
int rui_jlen(RuiJ *a);
double rui_jnum(RuiJ *o, const char *k, double d);
int rui_jbool(RuiJ *o, const char *k);
const char *rui_jstr(RuiJ *o, const char *k);          /* the string (NULL when not one) */
int rui_jtype(RuiJ *j);                                /* 0 null 1 bool 2 number 3 string 4 array 5 object, -1 none */
double rui_jval(RuiJ *j);
const char *rui_jsval(RuiJ *j);
const char *rui_jkey(RuiJ *o, int i);
RuiJ *rui_jparse(const char *s);

/* ---- windows and controls */
typedef struct {
	GtkWidget *win, *overlay, *body;   /* body: a vertical box, the page's window body (.win .body) */
	char app[32];
} RuiWin;
RuiWin *rui_window(const char *app, const char *title, int w, int h);
GtkWidget *rui_button(const char *label, int pri);
GtkWidget *rui_entry(const char *placeholder);
GtkWidget *rui_label(const char *text);
GtkWidget *rui_heading(const char *text);               /* .sh4 */
GtkWidget *rui_msg(const char *text);                   /* .msg */
GtkWidget *rui_check(const char *label, int on, void (*cb)(int on, void *d), void *d);   /* label.ck with its check box */
int rui_check_get(GtkWidget *c);
void rui_check_wrap(GtkWidget *c, double css_w, double css_h);  /* its words wrapped in css_w px (label.ck as wide as its cell), css_h tall */
void rui_check_set(GtkWidget *c, int on);
void rui_check_style(GtkWidget *c, int weight, int inl);   /* (1.6.3) its words in bold (600, 700), or (inl) a space after the box, as a plain <label> with its input */
/* .tmtabs: the tabs, their buttons; cb(i) when one is chosen */
GtkWidget *rui_tabs(const char *const *labels, int n, void (*cb)(int i, void *d), void *d);
void rui_tabs_set(GtkWidget *tabs, int i);
GtkWidget *rui_tabs_button(GtkWidget *tabs, int i);
/* a scrolled box, with scroll bars as Chromium draws them */
GtkWidget *rui_scroller(GtkWidget *child);

/* ---- the table (.tmt): columns, rows (cells' text), group rows, the selected row, sorting by a heading */
typedef struct RuiTable RuiTable;
enum { RUI_LEFT = 0, RUI_RIGHT = 1 };
RuiTable *rui_table_new(int ncols);
GtkWidget *rui_table_widget(RuiTable *t);
void rui_table_col(RuiTable *t, int c, const char *title, int align, double width /* <1: a share of the free width; 0 auto */);
void rui_table_heading(RuiTable *t, int c, const char *title);   /* (a heading with ▲ ▼) */
void rui_table_clear(RuiTable *t);
int rui_table_add(RuiTable *t, const char *const *cells, long id);   /* a row; id is the caller's (a PID); returns its index */
int rui_table_group(RuiTable *t, const char *bold, const char *rest);  /* a group's row across the columns: "Kind" "3 · 1.0% CPU · 20 MB" */
void rui_table_select(RuiTable *t, long id);            /* -1: none */
long rui_table_selected(RuiTable *t);
void rui_table_cell_style(RuiTable *t, int row, int col, int muted, int pad_left);
void rui_table_done(RuiTable *t);                       /* drawn again after a change */
void rui_table_on(RuiTable *t, void (*row)(long id, int how /* 0 click 1 right-click 2 Enter 3 Delete 4 Shift+Delete */, double x, double y, void *d),
	void (*head)(int c, void *d), void *d);
void rui_table_rect(RuiTable *t, long id, GdkRectangle *r);   /* (the tests) */
double rui_table_col_x(RuiTable *t, int c);
int rui_table_button_rect(RuiTable *t, long id, int col, int k, GdkRectangle *r);   /* the k-th button in row id's cell (the tests) */              /* where column c begins, in the widget (the tests) */
void rui_table_nohead(RuiTable *t, int on);              /* no heading row (a table of names and values) */
void rui_table_wrap(RuiTable *t, int on);                /* long writing wraps in its cell, the row as tall as its lines */
void rui_table_cell_bold(RuiTable *t, int row, int col);
void rui_table_set_button(RuiTable *t, int row, int col, const char *label, void (*cb)(long id, int col, void *d), void *d);   /* a button in a cell ("Switch to") */

/* ---- the right-click menu (the page's cmenu) */
typedef struct { const char *label, *icon, *key; int off, sep; } RuiMenuItem;
void rui_menu(GtkWidget *rel, double x, double y, const char *title, const RuiMenuItem *items, int n, void (*cb)(int i, void *d), void *d);
GtkWidget *rui_menu_window(void);                       /* the menu open now (the tests) */

/* ---- dialogs */
/* the page's sdlg over the window's body: a title, a text, buttons (the last one pri when pri); cb(i), -1 when closed with Esc */
void rui_confirm(RuiWin *w, const char *title, const char *text, const char *const *buttons, int n, int pri, void (*cb)(int i, void *d), void *d);
GtkWidget *rui_dialog_box(void);                        /* the dialog open now (the tests) */
/* (the page's dialog() with fields, which is what a dialog of settings is: a row of its label and its input per field, as
   its grid-template-columns:110px 1fr. The fields the callback is given are the form's own copy, and stand only while it
   runs -- a text's words as typed, a check's "1" or "0" -- so the dialog outlives the call that opened it; the callback's
   i is the button pressed, or -1 for Escape.) */
enum { RUI_F_TEXT = 0, RUI_F_NUM, RUI_F_CHECK };
typedef struct { const char *label; int kind; const char *value; } RuiField;
void rui_form(RuiWin *w, const char *title, const char *text, const RuiField *f, int n, const char *const *buttons, int nb,
	int pri, void (*cb)(int i, const RuiField *f, void *d), void *d);
/* the page's elevate: an administrator's account and password, checked by rudolfd; cb(1) allowed, cb(0) not */
void rui_elevate(RuiWin *w, const char *reason, void (*cb)(int ok, void *d), void *d);
/* the page's elevate(reason, {cred:true}): cred(name, password) once they are right (rudolfd checks them again with the request: send
   them in its body, then forget them), cred(NULL, NULL) when cancelled */
void rui_elevate_cred(RuiWin *w, const char *reason, void (*cred)(const char *n, const char *p, void *d), void *d);
void rui_about(RuiWin *w, const char *name, const char *what);
/* a notification as the page's notify (the desktop's toasts and notification centre, through rudolfd) */
void rui_notify(const char *title, const char *body, int error);

/* ---- drawing in the page's look (for an app's own drawn parts) */
PangoLayout *rui_layout(cairo_t *cr, const char *text, double px, int weight);
void rui_color(cairo_t *cr, const char *what, double alpha);   /* "wbg" "bg2" "bd" "mut" "fg" "ac" "acfg" */

/* ---- the native shell (rudolf-native): "open APP" (an app opened as the start menu opens it), "winop FRAME focus|end"; the windows'
   list as the page's Task Manager is given it (JSON), to cb */
void rui_shell(RuiWin *w, const char *ask);
void rui_shell_wins(RuiWin *w, void (*cb)(const char *json, void *d), void *d);
#endif
