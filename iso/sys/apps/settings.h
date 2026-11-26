/*
 * rudolf-settings' parts (1.6.2, docs/plan-1.6.2.md Part 39): what settings.c (the window, the sidebar, Search in Settings, the
 * page's layout, the footer) gives the pages (settings_PAGE.c), each a native copy of a page of the web Settings.
 *
 * A page is built as the web one builds its HTML, block by block, and laid out as the page's CSS lays it out (settings.c measures
 * and places each block: .sc's padding, the margins collapsing between blocks, the .r2 grid of a label and its controls), so the
 * native page and the web one put every row at the same place. Lengths are the page's CSS pixels; the display scaling is applied
 * when they are placed. GPL-2.0-or-later.
 */
#ifndef RUDOLF_SETTINGS_H
#define RUDOLF_SETTINGS_H
#include "rudolf-ui.h"

/* ---- the window and the account */
extern RuiWin *SW;                                   /* the Settings window */
extern char ACCT[80], ME[64], ROLE[16];               /* the account signed in (its id, name, role), from rudolfd's desktop/session */
int set_admin(void);                                  /* the page's isAdmin() */
double set_now(void);                                 /* the time now (s), RUDOLF_SET_NOW for the tests (the page's clock) */
const char *set_engine_now(void);                     /* the desktop engine running now ("native", "chromium", "webkit"; the page's RD.engine) */

/* ---- the page's storage: the account's settings (bd95set:ACCT) as rudolfd keeps the page's copy (lsync) */
RuiJ *set_stored(void);                               /* the saved settings (SC): an object, never NULL (free with rui_jfree) */
int set_store(const char *const *keys, const char *const *json_vals, int n);   /* the page's persistS: those keys of SC saved (bd95set and bd95set:ACCT) */
const char *set_jstr(RuiJ *o, const char *k, const char *dflt);
RuiJ *set_ls_get(const char *k);                      /* the page's ld(k): {"x": the value} (NULL: none) */
int set_ls_set(const char *k, const char *json);      /* the page's svj(k, value): the value as JSON */
char *set_jesc(const char *s);                        /* a string's JSON escapes (without its quotes) */
void set_jw(GString *b, RuiJ *v);                    /* (1.6.3) a JSON value written again as it was (numbers as JavaScript writes them) */

/* ---- building a page (between set_page_begin and set_page_end, called by settings.c) */
typedef struct Cell Cell;
void p_h3(const char *t);                             /* .sc h3: the page's title */
void p_h4(const char *t);                             /* .sh4: a section */
void p_sgrp(const char *t);                           /* .sh4.sgrp: a group of sections (settings31.js) */
GtkWidget *p_msg(const char *t, int grey);            /* .msg (grey: the page's color:#6b7280, else the page's own colour) */
Cell *p_r2(const char *label);                        /* a row of .r2: its label, its controls in the cell (each row of the page's grid) */
void p_r2_more(Cell *c);                              /* (the next row is in the same .r2: no margin between) */
void c_add(Cell *c, GtkWidget *w, double gap, double dy, int full);   /* a control in the cell's line: after gap px, dy px down; full: the cell's width (2: what its line leaves) */
void c_line(Cell *c, double mt);                      /* the next controls on a line of their own, mt px under */
GtkWidget *c_msg(Cell *c, const char *t, int grey);   /* a .msg in the cell */
GtkWidget *c_text(Cell *c, const char *t, int weight);
void c_gap(GtkWidget *w, double gap);
void c_dy(GtkWidget *w, double dy);                   /* (1.6.3) a writing's place below its line's top (a range's value beside it) */
void p_r2_mt(Cell *c, double mt);                    /* (1.6.3) the row's own margin above it */
int set_https_get(const char *url, char **body, char *err, size_t en);   /* (1.6.3) the status; *body (free it) */
void p_r2_top(Cell *c, double mt);                   /* (1.6.3) the row's label at its top, mt px down (align-self: start; margin-top) */
/* (1.6.3) a .msg of .sp25 (settings25.js's sections: 12 px, --mut): p_msg / c_msg's grey 2; a button 12 px, 1px 8px (class set-small) */                 /* (1.6.3) a writing's room before it in its line (a space: the page's inline .msg after a select) */   /* plain writing in the cell (the Preview's: 700 bold, 400) */
GtkWidget *c_label(Cell *c, const char *t, double dy); /* a <label> in the cell's line ("After"), dy px down: found by Search in Settings */
GtkWidget *c_msg_m(Cell *c, const char *t, int grey, double mt, double mb);   /* a .msg with its own margins (a <p class=msg>) */
GtkWidget *c_pre(Cell *c, const char *t);             /* a <pre> (11px monospace, 160 px at most) */
GtkWidget *c_buttons(Cell *c, const char *const *labels, int n, GCallback *cbs, void *d);   /* .rmrow in the cell (its buttons wrap) */
void p_text_set(GtkWidget *w, const char *t);         /* a .msg's, a writing's or a pre's words changed */
GtkWidget *c_check(Cell *c, const char *t, int on, void (*cb)(int on, void *d), void *d);   /* label.ck, as wide as the cell (its words wrap) */
GtkWidget *p_buttons(const char *const *labels, int n, GCallback *cbs, void *d);   /* .rmrow of buttons, at the left */
void p_block(GtkWidget *w, double mt, double mb);     /* a block of the page's own (its height its own), between margins */
void p_space(double mt);
typedef void (*SetFoot)(int what /* 0 apply 1 revert 2 reset */);
void p_footer(int (*dirty)(void), SetFoot act);      /* .sfoot: Apply, Revert, Reset to defaults, at the page's bottom */
void p_footer_box(void);                              /* (1.6.3) the top of the box a sticky footer stays in */
void p_footer_here(void);                             /* (1.6.3) the footer's place in the page: sticky there (position: sticky; bottom: 0) */
void p_footer_update(void);                           /* the footer's state again (after a change) */
void p_footer_note(const char *t);                    /* "✓ t" in the footer, then its state again */
void p_relayout(void);                                /* (a block's size changed: laid out again) */
void p_label_named(const char *name, GtkWidget *w);   /* a part named for the tests' state file */

/* ---- controls the page has that librudolf-ui does not draw yet: the drop-down (select) and the date field */
GtkWidget *s_select(const char *const *values, const char *const *labels, int n, const char *cur, void (*cb)(const char *v, void *d), void *d);
const char *s_select_get(GtkWidget *s);
void s_select_set(GtkWidget *s, const char *v);
void s_select_disable(GtkWidget *s, int i, int off);  /* an option greyed */
void s_select_label(GtkWidget *s, int i, const char *t);
void s_select_width(GtkWidget *s, double css_w);      /* 0: as wide as its widest option (the page's select) */
GtkWidget *s_date(int y, int m, int d, void (*cb)(int y, int m, int d, void *dd), void *dd);   /* input type=date */
GtkWidget *s_range(double min, double max, double step, double v, void (*cb)(double v, void *d), void *d);   /* input type=range (220x16 CSS px; cb as its input event) */
double s_range_get(GtkWidget *w);
void s_range_set(GtkWidget *w, double v);
void p_custom_find(const char *t, int h4);            /* the custom block just added is found by Search in Settings (h4: a heading) */
void s_date_set(GtkWidget *w, int y, int m, int d);
void s_range_on_change(GtkWidget *w, void (*cb)(double v, void *d));   /* the page's change event: a drag ended, a key's step (cb as the input's) */
PangoLayout *s_layout(const char *t, double px, int wt, double w);   /* (1.6.3 Part 3) the page's writing laid out (free with g_object_unref) */
double s_asc(double px, int wt, double *desc);
void s_ckbox(cairo_t *cr, double x, double y, int on, int sens);   /* Chromium's check box at (x, y) CSS px */
void s_radio(cairo_t *cr, double x, double y, int on, int sens);   /* (1.6.3) Chromium's radio button at (x, y) CSS px */

/* ---- the pages: each a function that builds its blocks (settings_PAGE.c) */
void page_about(void);
void page_sys(void);
typedef int (*SetDirty)(void);
/* the page's state for the tests (RUDOLF_SET_STATE) */
void set_note(const char *fmt, ...) G_GNUC_PRINTF(1, 2);
void set_state_soon(void);
void set_show(const char *k);                         /* another page (the page's W.show) */
void set_show_keep(const char *k);                    /* (1.6.3) the page built again, not scrolled */
void set_notify(const char *title, const char *body, int error);

/* (About's blocks) */
GtkWidget *s_logo_block(const char *title, const char *sub, const char *mark, int warn);   /* the start button's logo (64 px), the name in bold 18px, a .msg under it, Official or Custom */
GtkWidget *s_table2(void);                                    /* .tmt.ab: names and values, its columns as CSS's automatic table layout */
void s_table2_add(GtkWidget *t, const char *name, const char *value);
char *set_date_str(double t);                                 /* the page's sysDateStr: a day in the time zone and date format of the settings */
/* (the pages' own blocks: Update and recovery's card, table, rows) */
GtkWidget *p_msg_m(const char *t, int grey, double mt, double mb);   /* a <p class=msg> with its margins; \x03 first: Pango markup */
GtkWidget *p_check(const char *t, int on, void (*cb)(int on, void *d), void *d);   /* a label.ck of the page's own, its words wrapping */
GtkWidget *p_buttons_h(const char *const *labels, int n, GCallback *cbs, void *d, double minh);   /* .rmrow whose buttons are minh tall (stretched) */
void p_custom(double (*lay)(void *d, double x, double y, double w), void *d, double mt, double mb);   /* a block laid out by the page itself: its height */
void p_put(GtkWidget *w);                             /* a part of a custom block, put on the page */
void p_place(GtkWidget *w, double x, double y, double wd, double h);   /* ...placed (CSS px; 0: its own size) */
double p_nat_w(GtkWidget *w);
double p_nat_h(GtkWidget *w);
void s_text(cairo_t *cr, const char *t, double x, double y, double px, int wt, double w, double lh);
int s_lines(const char *t, double px, int wt, double w, double *tw);
double s_lh(double px, int wt);
void s_rrect(cairo_t *cr, double x, double y, double w, double h, double r);
void s_col(cairo_t *cr, const char *what, double a);
double s_z(void);
void page_upd(void);
void page_kbd(void);
void page_mouse(void);
void page_disp(void);  /* settings_disp.c (1.6.3 PR 5: in progress) */
void page_vol(void);   /* settings_vol.c */
void page_mm(void);    /* settings_mm.c */
void page_acc(void);   /* settings_acc.c */
void page_bt(void);    /* settings_bt.c (1.6.3 PR 6) */
void page_remote(void); /* settings_remote.c (1.6.3 PR 6) */
void page_mob(void);   /* settings_mob.c (1.6.3 PR 6) */
void page_net(void);   /* settings_net.c (1.6.3 PR 6) */
void page_web(void);   /* settings_web.c (1.6.3 PR 6): Oracle */
void page_notif(void); /* settings_notif.c (1.6.3 PR 7) */
void page_devs(void);  /* settings_devs.c (1.6.3 PR 7) */
void page_apps(void);  /* settings_apps.c (1.6.3 PR 7) */
/* (settings_net.c: the Web search and Downloads rows, shown on Network Settings and Oracle: one working copy, one footer) */
void net_search_rows(const char *pfx, int part);       /* part 1: Web search, 2: Downloads, 0: both */
void net_search_msg(void);
void net_search_forget(void);
int net_dirty(void);
void net_foot(int what);
/* (settings_vol.c) sound through PulseAudio's simple API, opened when first needed: 16-bit mono at rate, played in a thread of its own */
void set_play_pcm(const short *pcm, size_t n, int rate);
void sec_bring(void); /* settings_bring.c */
void set_log_evt(const char *k, const char *m);   /* the page's logEvt (bd95seclog's list k, the last 100) */
void bring_step(const char *disk, void (*cb)(int ok, void *d), void *d);
#endif
