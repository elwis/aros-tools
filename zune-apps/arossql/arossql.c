#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <libraries/mui.h>
#include <mui/TextEditor_mcc.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>
#include <proto/asl.h>
#include <proto/alib.h>
#include <clib/alib_protos.h>
#include <utility/hooks.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* ------------------------------------------------------------ */
#define MAX_CONNECTIONS 50
#define MAX_TABS        10
#define MAX_COLS        20
#define PREFS_FILE      "ENV:AROSQL.prefs"
#define HISTORY_FILE    "ENV:AROSQL/history.txt"

struct conn_entry {
    char *alias;
    char *path;
};

struct result_row {
    struct tab_state *tab;
    char *cols[];        /* flexible array, cols[num_cols] */
};

struct tab_state {
    Object *page;
    Object *sql_editor;
    Object *btn_run;
    Object *btn_clear;
    Object *txt_status;
    Object *lv_results;
    Object *lst_results;
    struct Hook hook_disp_results;
    /* schema browser */
    Object *lv_tables, *lst_tables;
    Object *lv_cols, *lst_cols;
    Object *txt_col_label;
    Object *btn_schema;
    char   *alias;
    char   *path;
    char   *col_names[MAX_COLS];
    char    fmt_string[64];
    int     num_cols;
    int     conn_idx;
};

/* ------------------------------------------------------------ */
/* MUIMasterBase declared extern in proto/muimaster.h */

/* forward declarations */
static void conn_add(const char *alias, const char *path);
static void save_connections(void);
static void save_history(const char *dbpath, const char *sql);

static Object *app, *win;
static Object *lv_conn, *lst_conn;
static Object *btn_add, *btn_remove, *btn_browse;
static Object *tab_register;
static Object *txt_welcome;

static char *active_titles[MAX_TABS + 1];  /* NULL-terminated, rebuilt on change */

static struct conn_entry g_conns[MAX_CONNECTIONS];
static int g_conn_count = 0;

static struct tab_state g_tabs[MAX_TABS];
static int g_tab_count = 0;

/* ------------------------------------------------------------ */
static struct Hook hook_disp_conn;
static struct Hook hook_add;
static struct Hook hook_remove;
static struct Hook hook_browse;
static struct Hook hook_conn_dbl;
static struct Hook hook_tab_run;
static struct Hook hook_tab_clear;

#define HOOK_INIT(h, fn) \
    do { (h).h_Entry = HookEntry; (h).h_SubEntry = (HOOKFUNC)(fn); } while(0)

/* safe strdup replacement (avoids stdlib.library strlen crash) */
static char *my_strdup(const char *s)
{
    char *d;
    size_t len;
    if (!s) return NULL;
    len = 0;
    while (s[len]) len++;
    d = malloc(len + 1);
    if (!d) return NULL;
    {
        size_t i;
        for (i = 0; i <= len; i++)
            d[i] = s[i];
    }
    return d;
}

/* ------------------------------------------------------------ */
static void fill_result_strings(char **strings, struct tab_state *t,
                                 struct result_row *entry)
{
    int i;
    if (!entry) {
        for (i = 0; i < t->num_cols; i++)
            strings[i] = t->col_names[i];
        strings[t->num_cols] = NULL;
    } else {
        for (i = 0; i < t->num_cols; i++)
            strings[i] = entry->cols[i] ? entry->cols[i] : "";
        strings[t->num_cols] = NULL;
    }
}

static AROS_UFH3(void, disp_results_func,
    AROS_UFHA(struct Hook *,    h, A0),
    AROS_UFHA(char **,          strings, A2),
    AROS_UFHA(struct result_row *, entry, A1))
{
    AROS_USERFUNC_INIT
    fill_result_strings(strings, (struct tab_state *)h->h_Data, entry);
    AROS_USERFUNC_EXIT
}

static void clear_results(struct tab_state *t)
{
    int i;
    if (!t->lst_results) return;
    while (1) {
        struct result_row *row = NULL;
        DoMethod(t->lst_results, MUIM_List_GetEntry, 0, &row);
        if (!row) break;
        DoMethod(t->lst_results, MUIM_List_Remove, 0);
        for (i = 0; i < t->num_cols; i++)
            free(row->cols[i]);
        free(row);
    }
    for (i = 0; i < MAX_COLS; i++) {
        free(t->col_names[i]);
        t->col_names[i] = NULL;
    }
    t->num_cols = 0;
}

/* ------------------------------------------------------------ */
static ULONG do_tab_run(struct Hook *h, Object *obj, APTR msg)
{
    int slot, rc, num_cols, num_rows, i;
    struct tab_state *t;
    STRPTR sqltext;
    BPTR fsql, fout;
    LONG slen, n;
    char cmd[1024], outbuf[8192], status[128];
    char *header_line, *nl, *hdr, *line, *eol, *end, *val, *sep;
    struct result_row *row;
    size_t rowsize;

    (void)h; (void)msg;
    slot = (int)XGET(obj, MUIA_UserData);
    if (slot < 0 || slot >= MAX_TABS || g_tabs[slot].alias == NULL) return 0;

    t = &g_tabs[slot];
    if (!t->path || !t->path[0]) {
        set(t->txt_status, MUIA_Text_Contents, "No database path set.");
        return 0;
    }

    sqltext = (STRPTR)DoMethod(t->sql_editor, MUIM_TextEditor_ExportText);
    if (!sqltext || !*sqltext) {
        set(t->txt_status, MUIA_Text_Contents, "No SQL to execute.");
        return 0;
    }

    fsql = Open("T:arossql.sql", MODE_NEWFILE);
    if (!fsql) {
        set(t->txt_status, MUIA_Text_Contents, "Cannot write temp SQL file.");
        return 0;
    }
    slen = strlen((char *)sqltext);
    Write(fsql, sqltext, slen);
    Close(fsql);

    snprintf(cmd, sizeof(cmd),
        "C:sqlite3 -header -csv -separator \"|\" \"%s\" <T:arossql.sql >T:arossql_out",
        t->path);
    set(t->txt_status, MUIA_Text_Contents, "Running query...");

    rc = system(cmd);
    if (rc != 0) {
        set(t->txt_status, MUIA_Text_Contents, "sqlite3 execution failed.");
        return 0;
    }

    fout = Open("T:arossql_out", MODE_OLDFILE);
    if (!fout) {
        set(t->txt_status, MUIA_Text_Contents, "Cannot read query output.");
        return 0;
    }
    n = Read(fout, outbuf, sizeof(outbuf) - 1);
    Close(fout);
    if (n < 0) n = 0;
    outbuf[n] = '\0';

    clear_results(t);

    /* parse output: |-separated CSV, first line = header */
    num_cols = 0;
    num_rows = 0;
    header_line = outbuf;
    nl = strchr(outbuf, '\n');
    if (!nl) nl = outbuf + strlen(outbuf);
    *nl = '\0';

    hdr = header_line;
    while (*hdr && num_cols < MAX_COLS) {
        end = strchr(hdr, '|');
        if (!end) end = hdr + strlen(hdr);
        t->col_names[num_cols] = malloc(end - hdr + 1);
        memcpy(t->col_names[num_cols], hdr, end - hdr);
        t->col_names[num_cols][end - hdr] = '\0';
        num_cols++;
        hdr = (*end) ? end + 1 : end;
    }
    t->num_cols = num_cols;

    memset(t->fmt_string, ',', num_cols > 1 ? num_cols - 1 : 0);
    t->fmt_string[num_cols > 1 ? num_cols - 1 : 0] = '\0';

    line = nl + 1;
    while (*line && num_rows < 10000) {
        eol = strchr(line, '\n');
        if (!eol) eol = line + strlen(line);
        *eol = '\0';

        rowsize = sizeof(struct result_row) + num_cols * sizeof(char *);
        row = malloc(rowsize);
        if (!row) break;
        memset(row, 0, rowsize);
        row->tab = t;

        val = line;
        for (i = 0; i < num_cols; i++) {
            sep = strchr(val, '|');
            if (!sep) sep = val + strlen(val);
            if (sep > val) {
                row->cols[i] = malloc(sep - val + 1);
                memcpy(row->cols[i], val, sep - val);
                row->cols[i][sep - val] = '\0';
            } else {
                row->cols[i] = my_strdup("");
            }
            val = (*sep) ? sep + 1 : sep;
        }

        DoMethod(t->lst_results, MUIM_List_InsertSingle, row, MUIV_List_Insert_Bottom);
        num_rows++;

        line = *eol ? eol + 1 : eol;
        *eol = '\n';
    }

    set(t->lst_results, MUIA_List_Format, t->fmt_string);
    set(t->lst_results, MUIA_List_Title, (IPTR)(num_cols > 0));

    DoMethod(t->lst_results, MUIM_List_Redraw, MUIV_List_Redraw_All);

    snprintf(status, sizeof(status), "%d rows in %d column(s)", num_rows, num_cols);
    set(t->txt_status, MUIA_Text_Contents, status);

    save_history(t->path, (char *)sqltext);

    DeleteFile("T:arossql.sql");
    DeleteFile("T:arossql_out");

    return 0;
}

static ULONG do_tab_clear(struct Hook *h, Object *obj, APTR msg)
{
    (void)h; (void)msg;
    int slot = (int)XGET(obj, MUIA_UserData);
    if (slot < 0 || slot >= MAX_TABS || g_tabs[slot].alias == NULL) return 0;
    struct tab_state *t = &g_tabs[slot];
    set(t->sql_editor, MUIA_TextEditor_Contents, "");
    set(t->txt_status, MUIA_Text_Contents, "");
    return 0;
}

/* ------------------------------------------------------------ */
static struct Hook hook_tab_schema;
static struct Hook hook_tab_tblsel;
static struct Hook hook_tab_tbldbl;

static void run_one_query(const char *dbpath, const char *sql, char *out, int outlen)
{
    char cmd[1024];
    BPTR fsql, fout;
    LONG n;

    if (!dbpath || !dbpath[0] || !sql || !sql[0] || !out) {
        if (out) out[0] = '\0';
        return;
    }

    fsql = Open("T:arossql_s.sql", MODE_NEWFILE);
    if (!fsql) { out[0] = '\0'; return; }
    Write(fsql, sql, strlen(sql));
    Close(fsql);

    snprintf(cmd, sizeof(cmd),
        "C:sqlite3 -header -csv -separator \"|\" \"%s\" <T:arossql_s.sql >T:arossql_s.out",
        dbpath);
    system(cmd);

    fout = Open("T:arossql_s.out", MODE_OLDFILE);
    if (!fout) { out[0] = '\0'; DeleteFile("T:arossql_s.sql"); return; }
    n = Read(fout, out, outlen - 1);
    Close(fout);
    if (n < 0) n = 0;
    out[n] = '\0';

    DeleteFile("T:arossql_s.sql");
    DeleteFile("T:arossql_s.out");
}

static ULONG do_tab_schema(struct Hook *h, Object *obj, APTR msg)
{
    (void)h; (void)msg;
    struct tab_state *t;
    int slot, i;
    char outbuf[8192], *line, *eol, *val;
    slot = (int)XGET(obj, MUIA_UserData);
    if (slot < 0 || slot >= MAX_TABS || g_tabs[slot].alias == NULL) return 0;
    t = &g_tabs[slot];

    DoMethod(t->lst_tables, MUIM_List_Clear);
    DoMethod(t->lst_cols, MUIM_List_Clear);
    set(t->txt_col_label, MUIA_Text_Contents, "Columns:");

    run_one_query(t->path,
        "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' "
        "ORDER BY name;",
        outbuf, sizeof(outbuf));

    line = outbuf;
    /* skip header line */
    eol = strchr(line, '\n');
    if (!eol) { line = NULL; }
    else line = eol + 1;

    i = 0;
    while (line && *line && i < 1000) {
        eol = strchr(line, '\n');
        if (!eol) eol = line + strlen(line);
        *eol = '\0';
        val = my_strdup(line);
        if (val) DoMethod(t->lst_tables, MUIM_List_InsertSingle, val, MUIV_List_Insert_Bottom);
        i++;
        line = *eol ? eol + 1 : NULL;
    }
    return 0;
}

static void refresh_columns(struct tab_state *t, const char *tablename)
{
    char sql[512], outbuf[8192], label[256];
    char *line, *eol;
    int i;

    DoMethod(t->lst_cols, MUIM_List_Clear);
    snprintf(label, sizeof(label), "Columns: %s", tablename);
    set(t->txt_col_label, MUIA_Text_Contents, label);

    snprintf(sql, sizeof(sql), "PRAGMA table_info('%s');", tablename);
    run_one_query(t->path, sql, outbuf, sizeof(outbuf));

    line = outbuf;
    eol = strchr(line, '\n');
    if (!eol) return;
    line = eol + 1;

    i = 0;
    while (line && *line && i < 500) {
        eol = strchr(line, '\n');
        if (!eol) eol = line + strlen(line);
        *eol = '\0';
        /* PRAGMA table_info format: cid|name|type|notnull|dflt_value|pk */
        char *name_col = strchr(line, '|');
        if (name_col) {
            name_col++;
            char *type_col = strchr(name_col, '|');
            if (type_col) {
                *type_col = '\0';
                type_col++;
                char *next = strchr(type_col, '|');
                if (next) *next = '\0';
                {
                    char entry[256];
                    snprintf(entry, sizeof(entry), "%-24s %s", name_col, type_col);
                    char *dup = my_strdup(entry);
                    if (dup) DoMethod(t->lst_cols, MUIM_List_InsertSingle, dup, MUIV_List_Insert_Bottom);
                }
                if (next) *next = '|';
            }
            if (type_col) *(type_col - 1) = '|';
        }
        i++;
        line = *eol ? eol + 1 : NULL;
    }
}

static ULONG do_tab_tblsel(struct Hook *h, Object *obj, APTR msg)
{
    (void)h; (void)msg;
    struct tab_state *t;
    int slot;
    IPTR pos;
    char *entry;

    slot = (int)XGET(obj, MUIA_UserData);
    if (slot < 0 || slot >= MAX_TABS || g_tabs[slot].alias == NULL) return 0;
    t = &g_tabs[slot];

    pos = XGET(t->lst_tables, MUIA_List_Active);
    if ((LONG)pos < 0) return 0;
    DoMethod(t->lst_tables, MUIM_List_GetEntry, pos, &entry);
    if (!entry) return 0;
    refresh_columns(t, entry);
    return 0;
}

static ULONG do_tab_tbldbl(struct Hook *h, Object *obj, APTR msg)
{
    (void)h; (void)msg;
    struct tab_state *t;
    int slot;
    IPTR pos;
    char *entry, sql[512];

    slot = (int)XGET(obj, MUIA_UserData);
    if (slot < 0 || slot >= MAX_TABS || g_tabs[slot].alias == NULL) return 0;
    t = &g_tabs[slot];

    pos = XGET(t->lst_tables, MUIA_List_Active);
    if ((LONG)pos < 0) return 0;
    DoMethod(t->lst_tables, MUIM_List_GetEntry, pos, &entry);
    if (!entry) return 0;

    snprintf(sql, sizeof(sql), "SELECT * FROM %s LIMIT 100;", entry);
    set(t->sql_editor, MUIA_TextEditor_Contents, sql);
    return 0;
}

/* ------------------------------------------------------------ */
static void rebuild_titles(void)
{
    int j = 0;
    int i;

    /* welcome is always child #0 */
    active_titles[j++] = "Welcome";
    for (i = 0; i < MAX_TABS; i++) {
        if (g_tabs[i].alias != NULL)
            active_titles[j++] = g_tabs[i].alias;
    }
    active_titles[j] = NULL;
    SetAttrs(tab_register, MUIA_Register_Titles, active_titles, TAG_DONE);
}

/* ------------------------------------------------------------ */
static void tab_open(const char *alias, const char *path, int conn_idx)
{
    int slot;
    if (!alias || !alias[0] || !path || !path[0]) return;
    for (slot = 0; slot < MAX_TABS; slot++)
        if (g_tabs[slot].alias == NULL) break;
    if (slot >= MAX_TABS) return;

    struct tab_state *t = &g_tabs[slot];
    t->alias = my_strdup(alias);
    t->path  = my_strdup(path);
    if (!t->alias || !t->path) {
        free(t->alias); free(t->path);
        t->alias = NULL; t->path = NULL;
        return;
    }
    t->conn_idx = conn_idx;
    t->num_cols = 0;
    memset(t->col_names, 0, sizeof(t->col_names));

    /* per-tab display hook: h_Data points back to tab_state */
    t->hook_disp_results.h_Entry   = HookEntry;
    t->hook_disp_results.h_SubEntry = (HOOKFUNC)disp_results_func;
    t->hook_disp_results.h_Data    = t;

    t->page = (Object *)VGroup,

        /* top area: schema browser + SQL editor */
        Child, HGroup,

            /* --- schema browser (left, 25%) --- */
            Child, VGroup,
                MUIA_Weight, 25,
                GroupFrame, MUIA_FrameTitle, "Schema",

                Child, HGroup,
                    Child, t->btn_schema = SimpleButton("Refresh"),
                End,

                Child, t->lv_tables = ListviewObject,
                    MUIA_Listview_List, t->lst_tables = ListObject,
                        InputListFrame,
                        MUIA_List_ConstructHook, MUIV_List_ConstructHook_String,
                        MUIA_List_DestructHook,  MUIV_List_DestructHook_String,
                        MUIA_List_Format,        "",
                    End,
                End,

                Child, t->txt_col_label = TextObject,
                    TextFrame,
                    MUIA_Text_Contents, "Columns:",
                End,

                Child, t->lv_cols = ListviewObject,
                    MUIA_Listview_List, t->lst_cols = ListObject,
                        InputListFrame,
                        MUIA_List_ConstructHook, MUIV_List_ConstructHook_String,
                        MUIA_List_DestructHook,  MUIV_List_DestructHook_String,
                        MUIA_List_Format,        "",
                    End,
                End,
            End,

            /* --- SQL editor (right, 75%) --- */
            Child, VGroup,

                Child, HGroup,
                    Child, t->btn_run = SimpleButton("Run"),
                    Child, t->btn_clear = SimpleButton("Clear"),
                    Child, t->txt_status = TextObject,
                        TextFrame,
                        MUIA_Text_Contents, "",
                    End,
                End,

                Child, t->sql_editor = TextEditorObject,
                    MUIA_TextEditor_Contents,
                        "SELECT * FROM sqlite_master;\n",
                    MUIA_TextEditor_ReadOnly, FALSE,
                    MUIA_Weight, 40,
                End,

                /* results table */
                Child, t->lv_results = ListviewObject,
                    MUIA_Listview_List, t->lst_results = ListObject,
                        InputListFrame,
                        MUIA_List_DisplayHook, &t->hook_disp_results,
                        MUIA_List_Title,        FALSE,
                        MUIA_List_Format,       "",
                    End,
                    MUIA_Weight, 60,
                End,
            End,

        End,

    End;

    /* store slot in button user data for hook lookup */
    set(t->btn_run,     MUIA_UserData, slot);
    set(t->btn_clear,   MUIA_UserData, slot);
    set(t->btn_schema,  MUIA_UserData, slot);

    DoMethod(t->btn_run,    MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_CallHook, &hook_tab_run);
    DoMethod(t->btn_clear,  MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_CallHook, &hook_tab_clear);
    DoMethod(t->btn_schema, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_CallHook, &hook_tab_schema);

    /* table list selection -> show columns */
    DoMethod(t->lst_tables, MUIM_Notify, MUIA_List_Active, MUIV_EveryTime,
             app, 2, MUIM_CallHook, &hook_tab_tblsel);
    /* table list double-click -> fill SQL */
    DoMethod(t->lv_tables, MUIM_Notify, MUIA_Listview_DoubleClick, MUIV_EveryTime,
             app, 2, MUIM_CallHook, &hook_tab_tbldbl);

    DoMethod(tab_register, OM_ADDMEMBER, t->page);
    rebuild_titles();
    g_tab_count++;
}

static void tab_close(int slot)
{
    if (slot < 0 || slot >= MAX_TABS || g_tabs[slot].alias == NULL) return;

    struct tab_state *t = &g_tabs[slot];

    /* disconnect notifications */
    DoMethod(t->btn_run,    MUIM_KillNotify, MUIA_Pressed);
    DoMethod(t->btn_clear,  MUIM_KillNotify, MUIA_Pressed);
    DoMethod(t->btn_schema, MUIM_KillNotify, MUIA_Pressed);
    DoMethod(t->lst_tables, MUIM_KillNotify, MUIA_List_Active);
    DoMethod(t->lv_tables,  MUIM_KillNotify, MUIA_Listview_DoubleClick);

    /* free schema list entries */
    do { char *e = NULL; DoMethod(t->lst_tables, MUIM_List_GetEntry, 0, &e);
        if (!e) break; DoMethod(t->lst_tables, MUIM_List_Remove, 0); free(e); } while(1);
    do { char *e = NULL; DoMethod(t->lst_cols, MUIM_List_GetEntry, 0, &e);
        if (!e) break; DoMethod(t->lst_cols, MUIM_List_Remove, 0); free(e); } while(1);

    /* free result data */
    clear_results(t);

    DoMethod(tab_register, OM_REMMEMBER, t->page);
    MUI_DisposeObject(t->page);

    free(t->alias); t->alias = NULL;
    free(t->path);  t->path  = NULL;
    t->conn_idx = -1;
    t->page        = NULL;
    t->sql_editor  = NULL;
    t->btn_run     = NULL;
    t->btn_clear   = NULL;
    t->btn_schema  = NULL;
    t->txt_status  = NULL;
    t->lv_results  = NULL;
    t->lst_results = NULL;
    t->lv_tables   = NULL;
    t->lst_tables  = NULL;
    t->lv_cols     = NULL;
    t->lst_cols    = NULL;
    t->txt_col_label = NULL;

    rebuild_titles();
    g_tab_count--;
}

/* ------------------------------------------------------------ */
static void save_connections(void)
{
    int i;
    BPTR fh;
    char line[1024];

    fh = Open(PREFS_FILE, MODE_NEWFILE);
    if (!fh) return;
    for (i = 0; i < g_conn_count; i++) {
        int len = snprintf(line, sizeof(line), "%s|%s\n",
            g_conns[i].alias, g_conns[i].path);
        if (len > 0) Write(fh, line, len);
    }
    Close(fh);
}

static void load_connections(void)
{
    BPTR fh;
    char buf[4096], *line, *nl, *sep;

    fh = Open(PREFS_FILE, MODE_OLDFILE);
    if (!fh) return;
    {
        LONG n = Read(fh, buf, sizeof(buf) - 1);
        Close(fh);
        if (n <= 0) return;
        buf[n] = '\0';
    }

    line = buf;
    while (*line && g_conn_count < MAX_CONNECTIONS) {
        nl = strchr(line, '\n');
        if (!nl) nl = line + strlen(line);
        *nl = '\0';
        sep = strchr(line, '|');
        if (sep && line[0] && sep[1]) {
            *sep = '\0';
            conn_add(line, sep + 1);
        }
        line = *nl ? nl + 1 : line + (*line ? strlen(line) : 0);
    }
    /* delete corrupted prefs if nothing loaded */
    if (g_conn_count == 0) DeleteFile(PREFS_FILE);
}

static void save_history(const char *dbpath, const char *sql)
{
    BPTR fh;
    time_t now;
    struct tm *ltm;
    char line[2048], ts[64];
    int len;

    if (!dbpath || !sql) return;

    now = time(NULL);
    ltm = localtime(&now);
    if (ltm)
        snprintf(ts, sizeof(ts), "%04d-%02d-%02d %02d:%02d:%02d",
            ltm->tm_year + 1900, ltm->tm_mon + 1, ltm->tm_mday,
            ltm->tm_hour, ltm->tm_min, ltm->tm_sec);
    else
        strncpy(ts, "unknown", sizeof(ts) - 1);

    fh = Open(HISTORY_FILE, MODE_READWRITE);
    if (!fh) {
        /* create file first */
        fh = Open(HISTORY_FILE, MODE_NEWFILE);
        if (!fh) return;
        Close(fh);
        fh = Open(HISTORY_FILE, MODE_READWRITE);
        if (!fh) return;
    }
    /* seek to end */
    Seek(fh, 0, OFFSET_END);
    len = snprintf(line, sizeof(line), "[%s] (%s) %s\n", ts, dbpath, sql);
    if (len > 0) Write(fh, line, len);
    Close(fh);
}

/* ------------------------------------------------------------ */
static void conn_add(const char *alias, const char *path)
{
    if (!alias || !alias[0] || !path || !path[0]) return;
    if (g_conn_count >= MAX_CONNECTIONS) return;
    struct conn_entry *e = &g_conns[g_conn_count++];
    e->alias = my_strdup(alias);
    e->path  = my_strdup(path);
    if (!e->alias || !e->path) {
        free(e->alias); free(e->path);
        g_conn_count--;
        return;
    }
    DoMethod(lst_conn, MUIM_List_InsertSingle, e, MUIV_List_Insert_Bottom);
}

/* ------------------------------------------------------------ */
static AROS_UFH3(void, disp_conn_func,
    AROS_UFHA(struct Hook *,   h, A0),
    AROS_UFHA(char **,         strings, A2),
    AROS_UFHA(struct conn_entry *, entry, A1))
{
    AROS_USERFUNC_INIT
    if (entry)
        strings[0] = entry->alias;
    AROS_USERFUNC_EXIT
}

/* ------------------------------------------------------------ */
static void filename_to_alias(const char *path, char *out, int outlen)
{
    const char *name, *p;
    char *dot;
    if (!path || !out || outlen <= 0) return;
    name = path;
    p = path;
    while (*p) { if (*p == '/' || *p == ':') name = p + 1; p++; }
    out[0] = '\0';
    strncat(out, name, outlen - 1);
    dot = strrchr(out, '.');
    if (dot && (strcasecmp(dot, ".db") == 0 ||
                strcasecmp(dot, ".sqlite") == 0 ||
                strcasecmp(dot, ".sqlite3") == 0))
        *dot = '\0';
}

static void pick_and_add(void)
{
    struct FileRequester *fr;
    fr = (struct FileRequester *)AllocAslRequestTags(ASL_FileRequest,
        ASLFR_TitleText,  "Select SQLite Database",
        ASLFR_DoPatterns, TRUE,
        ASLFR_InitialPattern, "#?.db|#?.sqlite|#?.sqlite3",
        ASLFR_InitialDrawer, "SYS:",
        TAG_END);
    if (!fr) return;
    if (AslRequest(fr, NULL)) {
        char full[512], alias[128];
        const char *dir, *file;
        dir  = (const char *)fr->rf_Dir;
        file = (const char *)fr->rf_File;
        if (!dir || !file || !dir[0] || !file[0]) { FreeAslRequest(fr); return; }
        full[0] = '\0';
        strncat(full, dir, sizeof(full) - 1);
        strncat(full, file, sizeof(full) - strlen(full) - 1);
        if (!full[0]) { FreeAslRequest(fr); return; }
        filename_to_alias(full, alias, sizeof(alias));
        if (alias[0]) {
            conn_add(alias, full);
            save_connections();
        }
    }
    FreeAslRequest(fr);
}

/* Add: allow creating new DB (file doesn't need to exist yet) */
static void pick_and_create(void)
{
    struct FileRequester *fr;
    fr = (struct FileRequester *)AllocAslRequestTags(ASL_FileRequest,
        ASLFR_TitleText,     "Name for new SQLite Database",
        ASLFR_DoSaveMode,    TRUE,
        ASLFR_InitialDrawer, "SYS:",
        TAG_END);
    if (!fr) return;
    if (AslRequest(fr, NULL)) {
        char full[512], alias[128];
        const char *dir, *file;
        dir  = (const char *)fr->rf_Dir;
        file = (const char *)fr->rf_File;
        if (!dir || !file || !dir[0] || !file[0]) { FreeAslRequest(fr); return; }
        full[0] = '\0';
        strncat(full, dir, sizeof(full) - 1);
        strncat(full, file, sizeof(full) - strlen(full) - 1);
        if (!full[0]) { FreeAslRequest(fr); return; }
        filename_to_alias(full, alias, sizeof(alias));
        if (alias[0]) {
            conn_add(alias, full);
            save_connections();
        }
    }
    FreeAslRequest(fr);
}

static ULONG do_add(struct Hook *h, Object *obj, APTR msg)
{
    (void)h; (void)obj; (void)msg;
    pick_and_create();
    return 0;
}

static ULONG do_remove(struct Hook *h, Object *obj, APTR msg)
{
    (void)h; (void)obj; (void)msg;
    IPTR pos = XGET(lst_conn, MUIA_List_Active);
    if ((LONG)pos < 0) return 0;
    struct conn_entry *e = NULL;
    DoMethod(lst_conn, MUIM_List_GetEntry, pos, &e);
    if (!e) return 0;

    /* close any tab using this connection */
    int i;
    for (i = 0; i < MAX_TABS; i++)
        if (g_tabs[i].conn_idx == (int)pos)
            tab_close(i);

    DoMethod(lst_conn, MUIM_List_Remove, pos);
    free(e->alias);
    free(e->path);
    int j;
    for (j = (int)pos; j < g_conn_count - 1; j++)
        g_conns[j] = g_conns[j + 1];
    g_conn_count--;
    save_connections();

    /* fix up tab conn_idx references */
    for (i = 0; i < MAX_TABS; i++)
        if (g_tabs[i].conn_idx > (int)pos)
            g_tabs[i].conn_idx--;
    return 0;
}

static ULONG do_browse(struct Hook *h, Object *obj, APTR msg)
{
    (void)h; (void)obj; (void)msg;
    pick_and_add();
    return 0;
}

/* ------------------------------------------------------------ */
static ULONG do_conn_dbl(struct Hook *h, Object *obj, APTR msg)
{
    (void)h; (void)obj; (void)msg;
    struct conn_entry *e = NULL;
    IPTR pos = XGET(lst_conn, MUIA_List_Active);
    if ((LONG)pos < 0) return 0;
    DoMethod(lst_conn, MUIM_List_GetEntry, pos, &e);
    if (!e || !e->alias || !e->path) return 0;
    tab_open(e->alias, e->path, (int)pos);
    return 0;
}

/* ------------------------------------------------------------ */
int main(void)
{
    int i;
    for (i = 0; i < MAX_TABS; i++) {
        g_tabs[i].alias = NULL;
        g_tabs[i].path  = NULL;
        g_tabs[i].conn_idx = -1;
        g_tabs[i].num_cols = 0;
    }
    active_titles[0] = "Welcome";
    active_titles[1] = NULL;

    MUIMasterBase = OpenLibrary("muimaster.library", 0);
    if (!MUIMasterBase) return 1;

    HOOK_INIT(hook_disp_conn, disp_conn_func);
    HOOK_INIT(hook_add,       do_add);
    HOOK_INIT(hook_remove,    do_remove);
    HOOK_INIT(hook_browse,    do_browse);
    HOOK_INIT(hook_conn_dbl,  do_conn_dbl);
    HOOK_INIT(hook_tab_run,    do_tab_run);
    HOOK_INIT(hook_tab_clear,  do_tab_clear);
    HOOK_INIT(hook_tab_schema, do_tab_schema);
    HOOK_INIT(hook_tab_tblsel, do_tab_tblsel);
    HOOK_INIT(hook_tab_tbldbl, do_tab_tbldbl);

    app = ApplicationObject,
        MUIA_Application_Title,       "AROSQL",
        MUIA_Application_Version,     "$VER: AROSQL 0.1 (2026)",
        MUIA_Application_Base,        "AROSSQL",

        SubWindow, win = WindowObject,
            MUIA_Window_Title,  "AROSQL - SQLite Client",
            MUIA_Window_Width,  800,
            MUIA_Window_Height, 600,

            WindowContents, HGroup,

                Child, VGroup,
                    MUIA_Weight, 25,
                    GroupFrame, MUIA_FrameTitle, "Connections",

                    Child, lv_conn = ListviewObject,
                        MUIA_Listview_List, lst_conn = ListObject,
                            InputListFrame,
                            MUIA_List_DisplayHook, &hook_disp_conn,
                            MUIA_List_Format,      "",
                        End,
                    End,

                    Child, HGroup,
                        Child, btn_add    = SimpleButton("Add..."),
                        Child, btn_remove = SimpleButton("Remove"),
                        Child, btn_browse = SimpleButton("Browse..."),
                    End,
                End,

                Child, tab_register = RegisterObject,
                    MUIA_Register_Titles, active_titles,
                    MUIA_Register_Frame,  TRUE,

                    Child, VGroup,
                        Child, txt_welcome = TextObject,
                            TextFrame,
                            MUIA_Text_Contents,
                                "Welcome to AROSQL\n\n"
                                "Browse or Add a connection on the left,\n"
                                "then double-click to open a query tab.",
                        End,
                    End,
                End,

            End, /* HGroup */
        End, /* Window */
    End; /* Application */

    if (!app) {
        CloseLibrary(MUIMasterBase);
        return 1;
    }

    DoMethod(btn_add,    MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_CallHook, &hook_add);
    DoMethod(btn_remove, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_CallHook, &hook_remove);
    DoMethod(btn_browse, MUIM_Notify, MUIA_Pressed, FALSE,
             app, 2, MUIM_CallHook, &hook_browse);

    /* double-click connection -> open tab */
    DoMethod(lv_conn, MUIM_Notify, MUIA_Listview_DoubleClick, MUIV_EveryTime,
             app, 2, MUIM_CallHook, &hook_conn_dbl);

    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

    set(win, MUIA_Window_Open, TRUE);
    load_connections();

    ULONG sigs = 0;
    while (DoMethod(app, MUIM_Application_NewInput, &sigs)
           != MUIV_Application_ReturnID_Quit)
    {
        if (sigs) {
            sigs = Wait(sigs | SIGBREAKF_CTRL_C);
            if (sigs & SIGBREAKF_CTRL_C) break;
        }
    }

    /* cleanup tabs */
    for (i = MAX_TABS - 1; i >= 0; i--)
        if (g_tabs[i].alias != NULL)
            tab_close(i);

    save_connections();

    set(win, MUIA_Window_Open, FALSE);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
