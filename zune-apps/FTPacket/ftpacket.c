/*
 * FTPacket - FTP Client for AROS x64
 *
 * Zune/MUI GUI front-end using bsdsocket.library for networking.
 * Passive-mode (PASV) data connections only.
 *
 * NOTE: All FTP operations are synchronous and run on the main task.
 * This means the UI freezes during transfers.  A threaded design
 * is the logical next step for a production version.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/muimaster.h>
#include <proto/alib.h>
#include <proto/intuition.h>
#include <utility/hooks.h>
#include <clib/alib_protos.h>

/* BSD socket — per-task SocketBase pattern required on AROS hosted networking.
 * Global SocketBase causes Exec_OpenResource crash; tc_UserData is the fix. */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#define SocketBase FindTask(NULL)->tc_UserData
#define __BSDSOCKET_NOLIBBASE__
#include <proto/bsdsocket.h>
#include <bsdsocket/socketbasetags.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * Constants
 * ================================================================ */

#define FTP_CTRL_BUF  2048
#define FTP_DATA_BUF  8192
#define MAX_PATH       512
#define DIR_PREFIX    "[DIR] "
#define DIR_PREFIX_LEN 6

/* ================================================================
 * Global library bases
 * ================================================================ */

struct Library *MUIMasterBase = NULL;

/* ================================================================
 * FTP state
 * ================================================================ */

struct FTPConn {
    int  ctrl;
    BOOL connected;
    char cwd[MAX_PATH];
};

static struct FTPConn g_ftp;
static char           g_local_dir[MAX_PATH];

/* ================================================================
 * UI objects
 * ================================================================ */

static Object *app, *win;
static Object *str_host, *str_port, *str_user, *str_pass;
static Object *btn_connect, *btn_disconnect;
static Object *lv_local,  *lst_local;
static Object *lv_remote, *lst_remote;
static Object *btn_upload, *btn_download;
static Object *btn_local_up, *btn_remote_up;
static Object *txt_local_path, *txt_remote_path;
static Object *txt_status;

/* ================================================================
 * Hooks
 * ================================================================ */

static struct Hook hook_disp_local;
static struct Hook hook_disp_remote;
static struct Hook hook_connect_h;
static struct Hook hook_disconnect_h;
static struct Hook hook_upload_h;
static struct Hook hook_download_h;
static struct Hook hook_local_up_h;
static struct Hook hook_remote_up_h;
static struct Hook hook_local_dbl_h;
static struct Hook hook_remote_dbl_h;

#define HOOK_INIT(h,fn) \
    do { (h).h_Entry = HookEntry; (h).h_SubEntry = (HOOKFUNC)(fn); } while(0)

/* ================================================================
 * Network helpers
 * ================================================================ */

static BOOL net_open(void)
{
    static LONG s_errno = 0, s_herrno = 0;
    if (SocketBase) return TRUE;
    SocketBase = OpenLibrary("bsdsocket.library", 4);
    if (!SocketBase) return FALSE;
    SocketBaseTags(
        SBTM_SETVAL(SBTC_ERRNOLONGPTR),  (IPTR)&s_errno,
        SBTM_SETVAL(SBTC_HERRNOLONGPTR), (IPTR)&s_herrno,
        TAG_DONE);
    return TRUE;
}

static void net_close(void)
{
    if (SocketBase) { CloseLibrary(SocketBase); SocketBase = NULL; }
}

/* ================================================================
 * FTP protocol
 * ================================================================ */

/* Read exactly one line from the control socket (strips \r\n) */
static int ftp_readline(char *buf, int maxlen)
{
    int i = 0; char c;
    while (i < maxlen - 1) {
        if (recv(g_ftp.ctrl, &c, 1, 0) <= 0) break;
        if (c == '\n') break;
        if (c != '\r') buf[i++] = c;
    }
    buf[i] = '\0';
    return i;
}

/*
 * Read a complete (possibly multi-line) FTP response.
 * Returns the 3-digit numeric code, and copies the final line
 * into out/outlen if non-NULL.
 */
static int ftp_getresp(char *out, int outlen)
{
    char line[FTP_CTRL_BUF]; int code = 0;
    do {
        ftp_readline(line, sizeof(line));
        /* Final line: "NNN <text>" (space after code, not hyphen) */
        if ((int)strlen(line) >= 4 && line[3] == ' ') {
            code = atoi(line);
            if (out) { strncpy(out, line, outlen - 1); out[outlen-1] = '\0'; }
            break;
        }
    } while (1);
    return code;
}

/* Send command and return response code */
static int ftp_cmd(const char *cmd, char *out, int outlen)
{
    char buf[FTP_CTRL_BUF];
    snprintf(buf, sizeof(buf), "%s\r\n", cmd);
    if (send(g_ftp.ctrl, buf, strlen(buf), 0) < 0) return -1;
    return ftp_getresp(out, outlen);
}

/*
 * Open a passive data connection.
 * Sends PASV, parses the response, and returns a connected socket
 * or -1 on failure.
 */
static int ftp_open_pasv(void)
{
    char resp[256];
    if (ftp_cmd("PASV", resp, sizeof(resp)) != 227) return -1;

    const char *p = strchr(resp, '(');
    if (!p) return -1; p++;

    int h1,h2,h3,h4,p1,p2;
    if (sscanf(p, "%d,%d,%d,%d,%d,%d", &h1,&h2,&h3,&h4,&p1,&p2) != 6) return -1;

    char ip[32]; snprintf(ip, sizeof(ip), "%d.%d.%d.%d", h1,h2,h3,h4);

    struct sockaddr_in addr; memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip);
    addr.sin_port = htons((unsigned short)((p1 << 8) | p2));

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return -1;
    if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        CloseSocket(s); return -1;
    }
    return s;
}

/* Ask server for current directory and store in g_ftp.cwd */
static void ftp_update_cwd(void)
{
    char resp[512];
    if (ftp_cmd("PWD", resp, sizeof(resp)) != 257) return;
    char *a = strchr(resp, '"'); if (!a) return; a++;
    char *b = strchr(a, '"');   if (!b) return;
    int len = (int)(b - a);
    if (len >= MAX_PATH) len = MAX_PATH - 1;
    strncpy(g_ftp.cwd, a, len);
    g_ftp.cwd[len] = '\0';
}

/*
 * Extract the filename/dirname from a raw Unix-style LIST line.
 * Returns a pointer into the original string.
 * For a line like:  drwxr-xr-x  2 user group 4096 Jan 1 12:00 dirname
 * this returns "dirname" (last whitespace-delimited token).
 */
static const char *ftp_entry_name(const char *line)
{
    const char *last = NULL, *p = line;
    while (*p) {
        if (*p != ' ' && (p == line || *(p-1) == ' ')) last = p;
        p++;
    }
    return last ? last : line;
}

/* ================================================================
 * Status helper
 * ================================================================ */

static void ui_status(const char *msg)
{
    set(txt_status, MUIA_Text_Contents, msg);
}

static void ui_set_connected(BOOL on)
{
    set(btn_connect,    MUIA_Disabled,  on);
    set(btn_disconnect, MUIA_Disabled, !on);
    set(btn_upload,     MUIA_Disabled, !on);
    set(btn_download,   MUIA_Disabled, !on);
    set(btn_remote_up,  MUIA_Disabled, !on);
}

/* ================================================================
 * Local directory helpers (AmigaOS/AROS paths: VOL: or VOL:sub/dir)
 * ================================================================ */

static void local_refresh(void)
{
    DoMethod(lst_local, MUIM_List_Clear);

    BPTR lock = Lock(g_local_dir, ACCESS_READ);
    if (!lock) { ui_status("Cannot lock local directory"); return; }

    struct FileInfoBlock *fib =
        (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);
    if (fib) {
        if (Examine(lock, fib)) {
            while (ExNext(lock, fib)) {
                char entry[300];
                BOOL isdir = fib->fib_DirEntryType > 0;
                snprintf(entry, sizeof(entry), "%s%s",
                    isdir ? DIR_PREFIX : "      ",
                    fib->fib_FileName);
                DoMethod(lst_local, MUIM_List_InsertSingle,
                         entry, MUIV_List_Insert_Sorted);
            }
        }
        FreeDosObject(DOS_FIB, fib);
    }
    UnLock(lock);
    set(txt_local_path, MUIA_Text_Contents, g_local_dir);
}

/* Navigate into a subdirectory of the current local path */
static void local_enter(const char *dirname)
{
    int len = strlen(g_local_dir);
    if (g_local_dir[len-1] == ':') {
        strncat(g_local_dir, dirname, MAX_PATH - len - 1);
    } else {
        strncat(g_local_dir, "/",     MAX_PATH - len - 1);
        strncat(g_local_dir, dirname, MAX_PATH - len - 2);
    }
    local_refresh();
}

/* Navigate one level up */
static void local_up(void)
{
    char *slash = strrchr(g_local_dir, '/');
    if (slash) {
        *slash = '\0';
    } else {
        /* Already at volume root: e.g. "SYS:Sub" -> "SYS:" */
        char *colon = strchr(g_local_dir, ':');
        if (colon && *(colon+1) != '\0') *(colon+1) = '\0';
    }
    local_refresh();
}

/* ================================================================
 * Remote directory helpers
 * ================================================================ */

static void remote_refresh(void)
{
    if (!g_ftp.connected) return;
    DoMethod(lst_remote, MUIM_List_Clear);

    int ds = ftp_open_pasv();
    if (ds < 0) { ui_status("PASV failed for LIST"); return; }

    char resp[256];
    int code = ftp_cmd("LIST", resp, sizeof(resp));
    if (code != 125 && code != 150) {
        CloseSocket(ds); ui_status(resp); return;
    }

    /* Read listing, split on newlines, insert each line */
    char buf[FTP_DATA_BUF]; char line[512]; int lp = 0; int n;
    while ((n = recv(ds, buf, sizeof(buf), 0)) > 0) {
        for (int i = 0; i < n; i++) {
            char c = buf[i];
            if (c == '\n') {
                line[lp] = '\0'; lp = 0;
                if (line[0])
                    DoMethod(lst_remote, MUIM_List_InsertSingle,
                             line, MUIV_List_Insert_Bottom);
            } else if (c != '\r' && lp < (int)sizeof(line)-1) {
                line[lp++] = c;
            }
        }
    }
    CloseSocket(ds);
    ftp_getresp(resp, sizeof(resp)); /* consume 226 Transfer complete */
    set(txt_remote_path, MUIA_Text_Contents, g_ftp.cwd);
}

/* ================================================================
 * List display hooks (entries are plain char* via ConstructHook_String)
 * ================================================================ */

static ULONG disp_str_func(struct Hook *h, char **strings, char *entry)
{
    (void)h;
    if (entry) strings[0] = entry;
    return 0;
}

/* ================================================================
 * Button / action hooks
 * ================================================================ */

static ULONG do_connect(struct Hook *h, Object *obj, APTR msg)
{
    (void)h; (void)obj; (void)msg;
    if (g_ftp.connected) return 0;
    if (!net_open()) { ui_status("Cannot open bsdsocket.library"); return 0; }

    char host[256] = {0}; IPTR port_val = 21;
    char user[64]  = {0}, pass[64] = {0};

    STRPTR sp = NULL;
    get(str_host, MUIA_String_Contents, &sp);
    if (sp) strncpy(host, sp, sizeof(host)-1);

    get(str_port, MUIA_String_Integer, &port_val);

    sp = NULL; get(str_user, MUIA_String_Contents, &sp);
    if (sp) strncpy(user, sp, sizeof(user)-1);

    sp = NULL; get(str_pass, MUIA_String_Contents, &sp);
    if (sp) strncpy(pass, sp, sizeof(pass)-1);

    if (!host[0]) { ui_status("Enter a hostname"); return 0; }
    if (!port_val) port_val = 21;

    ui_status("Resolving hostname...");
    struct hostent *he = gethostbyname(host);
    if (!he) { ui_status("Hostname lookup failed"); return 0; }

    g_ftp.ctrl = socket(AF_INET, SOCK_STREAM, 0);
    if (g_ftp.ctrl < 0) { ui_status("socket() failed"); return 0; }

    struct sockaddr_in sa; memset(&sa, 0, sizeof(sa));
    sa.sin_family = he->h_addrtype;
    sa.sin_port   = htons((unsigned short)port_val);
    CopyMem(he->h_addr, &sa.sin_addr, he->h_length);

    ui_status("Connecting...");
    if (connect(g_ftp.ctrl, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        CloseSocket(g_ftp.ctrl); g_ftp.ctrl = -1;
        ui_status("Connection refused"); return 0;
    }

    char resp[256]; int code;
    code = ftp_getresp(resp, sizeof(resp));
    if (code != 220) {
        CloseSocket(g_ftp.ctrl); g_ftp.ctrl = -1; ui_status(resp); return 0;
    }

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "USER %s", user[0] ? user : "anonymous");
    code = ftp_cmd(cmd, resp, sizeof(resp));
    if (code == 331) {
        snprintf(cmd, sizeof(cmd), "PASS %s", pass[0] ? pass : "guest@");
        code = ftp_cmd(cmd, resp, sizeof(resp));
    }
    if (code != 230) {
        CloseSocket(g_ftp.ctrl); g_ftp.ctrl = -1; ui_status(resp); return 0;
    }

    g_ftp.connected = TRUE;
    ftp_update_cwd();
    ui_set_connected(TRUE);
    remote_refresh();
    ui_status("Connected and logged in.");
    return 0;
}

static ULONG do_disconnect(struct Hook *h, Object *obj, APTR msg)
{
    (void)h; (void)obj; (void)msg;
    if (!g_ftp.connected) return 0;
    char resp[64]; ftp_cmd("QUIT", resp, sizeof(resp));
    CloseSocket(g_ftp.ctrl); g_ftp.ctrl = -1;
    g_ftp.connected = FALSE; g_ftp.cwd[0] = '\0';
    DoMethod(lst_remote, MUIM_List_Clear);
    set(txt_remote_path, MUIA_Text_Contents, "");
    ui_set_connected(FALSE);
    ui_status("Disconnected.");
    return 0;
}

static ULONG do_download(struct Hook *h, Object *obj, APTR msg)
{
    (void)h; (void)obj; (void)msg;
    if (!g_ftp.connected) return 0;

    IPTR pos = XGET(lst_remote, MUIA_List_Active);
    if ((LONG)pos < 0) { ui_status("Select a remote file first"); return 0; }

    char *entry = NULL;
    DoMethod(lst_remote, MUIM_List_GetEntry, pos, &entry);
    if (!entry) return 0;
    if (entry[0] == 'd') { ui_status("Cannot download a directory"); return 0; }

    const char *fname = ftp_entry_name(entry);

    char resp[256]; ftp_cmd("TYPE I", resp, sizeof(resp));

    int ds = ftp_open_pasv();
    if (ds < 0) { ui_status("PASV failed"); return 0; }

    char cmd[MAX_PATH+16];
    snprintf(cmd, sizeof(cmd), "RETR %s", fname);
    int code = ftp_cmd(cmd, resp, sizeof(resp));
    if (code != 125 && code != 150) {
        CloseSocket(ds); ui_status(resp); return 0;
    }

    int ldlen = strlen(g_local_dir);
    char local_path[MAX_PATH*2];
    snprintf(local_path, sizeof(local_path), "%s%s%s",
        g_local_dir,
        g_local_dir[ldlen-1] == ':' ? "" : "/",
        fname);

    BPTR fh = Open(local_path, MODE_NEWFILE);
    if (!fh) { CloseSocket(ds); ui_status("Cannot create local file"); return 0; }

    char buf[FTP_DATA_BUF]; int n; ULONG total = 0;
    while ((n = recv(ds, buf, sizeof(buf), 0)) > 0) {
        Write(fh, buf, n); total += n;
    }
    Close(fh); CloseSocket(ds);
    ftp_getresp(resp, sizeof(resp));

    char status[256];
    snprintf(status, sizeof(status), "Downloaded '%s' (%lu bytes)", fname, total);
    ui_status(status);
    local_refresh();
    return 0;
}

static ULONG do_upload(struct Hook *h, Object *obj, APTR msg)
{
    (void)h; (void)obj; (void)msg;
    if (!g_ftp.connected) return 0;

    IPTR pos = XGET(lst_local, MUIA_List_Active);
    if ((LONG)pos < 0) { ui_status("Select a local file first"); return 0; }

    char *entry = NULL;
    DoMethod(lst_local, MUIM_List_GetEntry, pos, &entry);
    if (!entry) return 0;
    if (strncmp(entry, DIR_PREFIX, DIR_PREFIX_LEN) == 0)
        { ui_status("Cannot upload a directory"); return 0; }

    /* Strip the 6-char padding prefix from file entries ("      ") */
    char *fname = entry;
    while (*fname == ' ') fname++;

    int ldlen = strlen(g_local_dir);
    char local_path[MAX_PATH*2];
    snprintf(local_path, sizeof(local_path), "%s%s%s",
        g_local_dir,
        g_local_dir[ldlen-1] == ':' ? "" : "/",
        fname);

    BPTR fh = Open(local_path, MODE_OLDFILE);
    if (!fh) { ui_status("Cannot open local file"); return 0; }

    char resp[256]; ftp_cmd("TYPE I", resp, sizeof(resp));

    int ds = ftp_open_pasv();
    if (ds < 0) { Close(fh); ui_status("PASV failed"); return 0; }

    char cmd[MAX_PATH+16];
    snprintf(cmd, sizeof(cmd), "STOR %s", fname);
    int code = ftp_cmd(cmd, resp, sizeof(resp));
    if (code != 125 && code != 150) {
        Close(fh); CloseSocket(ds); ui_status(resp); return 0;
    }

    char buf[FTP_DATA_BUF]; LONG n; ULONG total = 0;
    while ((n = Read(fh, buf, sizeof(buf))) > 0) {
        send(ds, buf, n, 0); total += n;
    }
    Close(fh); CloseSocket(ds);
    ftp_getresp(resp, sizeof(resp));

    char status[256];
    snprintf(status, sizeof(status), "Uploaded '%s' (%lu bytes)", fname, total);
    ui_status(status);
    remote_refresh();
    return 0;
}

static ULONG do_local_up(struct Hook *h, Object *obj, APTR msg)
{
    (void)h; (void)obj; (void)msg;
    local_up();
    return 0;
}

static ULONG do_remote_up(struct Hook *h, Object *obj, APTR msg)
{
    (void)h; (void)obj; (void)msg;
    if (!g_ftp.connected) return 0;
    char resp[256]; ftp_cmd("CDUP", resp, sizeof(resp));
    ftp_update_cwd();
    remote_refresh();
    return 0;
}

static ULONG do_local_dbl(struct Hook *h, Object *obj, APTR msg)
{
    (void)h; (void)obj; (void)msg;
    IPTR pos = XGET(lst_local, MUIA_List_Active);
    if ((LONG)pos < 0) return 0;
    char *entry = NULL;
    DoMethod(lst_local, MUIM_List_GetEntry, pos, &entry);
    if (!entry || strncmp(entry, DIR_PREFIX, DIR_PREFIX_LEN) != 0) return 0;
    local_enter(entry + DIR_PREFIX_LEN);
    return 0;
}

static ULONG do_remote_dbl(struct Hook *h, Object *obj, APTR msg)
{
    (void)h; (void)obj; (void)msg;
    if (!g_ftp.connected) return 0;
    IPTR pos = XGET(lst_remote, MUIA_List_Active);
    if ((LONG)pos < 0) return 0;
    char *entry = NULL;
    DoMethod(lst_remote, MUIM_List_GetEntry, pos, &entry);
    if (!entry || entry[0] != 'd') return 0;

    const char *dname = ftp_entry_name(entry);
    char cmd[MAX_PATH+8]; char resp[256];
    snprintf(cmd, sizeof(cmd), "CWD %s", dname);
    if (ftp_cmd(cmd, resp, sizeof(resp)) == 250) {
        ftp_update_cwd();
        remote_refresh();
    } else {
        ui_status(resp);
    }
    return 0;
}

/* ================================================================
 * main
 * ================================================================ */

int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    MUIMasterBase = OpenLibrary("muimaster.library", 0);
    if (!MUIMasterBase) return 1;

    memset(&g_ftp, 0, sizeof(g_ftp));
    g_ftp.ctrl = -1;
    strncpy(g_local_dir, "SYS:", sizeof(g_local_dir)-1);

    /* Display hooks must be initialised before the UI objects that use them */
    HOOK_INIT(hook_disp_local,  disp_str_func);
    HOOK_INIT(hook_disp_remote, disp_str_func);

    /* ---------------------------------------------------------- */
    /* Build UI                                                    */
    /* ---------------------------------------------------------- */
    app = ApplicationObject,
        MUIA_Application_Title,       "FTPacket",
        MUIA_Application_Version,     "$VER: FTPacket 0.1 (2026)",
        MUIA_Application_Copyright,   "FTP Client for AROS",
        MUIA_Application_Author,      "AROS Tools",
        MUIA_Application_Description, "FTP client for AROS x64",
        MUIA_Application_Base,        "FTPACKET",

        SubWindow, win = WindowObject,
            MUIA_Window_Title,  "FTPacket - FTP Client",
            MUIA_Window_Width,  740,
            MUIA_Window_Height, 540,

            WindowContents, VGroup,

                /* --- Connection bar --- */
                Child, HGroup,
                    GroupFrame,
                    MUIA_FrameTitle, "Connection",

                    Child, Label("Host:"),
                    Child, str_host = StringObject, StringFrame,
                        MUIA_String_Contents, "",
                        MUIA_Weight, 100,
                    End,

                    Child, Label("Port:"),
                    Child, str_port = StringObject, StringFrame,
                        MUIA_String_Contents, "21",
                        MUIA_String_Accept,   "0123456789",
                        MUIA_Weight, 22,
                    End,

                    Child, Label("User:"),
                    Child, str_user = StringObject, StringFrame,
                        MUIA_String_Contents, "anonymous",
                        MUIA_Weight, 60,
                    End,

                    Child, Label("Pass:"),
                    Child, str_pass = StringObject, StringFrame,
                        MUIA_String_Secret,   TRUE,
                        MUIA_String_Contents, "",
                        MUIA_Weight, 60,
                    End,

                    Child, btn_connect    = SimpleButton("Connect"),
                    Child, btn_disconnect = SimpleButton("Disconnect"),
                End,

                /* --- File panels --- */
                Child, HGroup,

                    /* Local panel */
                    Child, VGroup,
                        GroupFrame, MUIA_FrameTitle, "Local",
                        Child, txt_local_path = TextObject, TextFrame,
                            MUIA_Text_Contents, "SYS:",
                        End,
                        Child, lv_local = ListviewObject,
                            MUIA_Listview_List, lst_local = ListObject,
                                InputListFrame,
                                MUIA_List_ConstructHook, MUIV_List_ConstructHook_String,
                                MUIA_List_DestructHook,  MUIV_List_DestructHook_String,
                                MUIA_List_DisplayHook,   &hook_disp_local,
                                MUIA_List_Format,        "",
                            End,
                        End,
                        Child, HGroup,
                            Child, btn_local_up = SimpleButton("Parent Dir"),
                        End,
                    End,

                    /* Transfer buttons column */
                    Child, VGroup,
                        MUIA_Weight, 8,
                        Child, RectangleObject, End,
                        Child, btn_upload   = SimpleButton("->"),
                        Child, RectangleObject, MUIA_Weight, 4, End,
                        Child, btn_download = SimpleButton("<-"),
                        Child, RectangleObject, End,
                    End,

                    /* Remote panel */
                    Child, VGroup,
                        GroupFrame, MUIA_FrameTitle, "Remote",
                        Child, txt_remote_path = TextObject, TextFrame,
                            MUIA_Text_Contents, "",
                        End,
                        Child, lv_remote = ListviewObject,
                            MUIA_Listview_List, lst_remote = ListObject,
                                InputListFrame,
                                MUIA_List_ConstructHook, MUIV_List_ConstructHook_String,
                                MUIA_List_DestructHook,  MUIV_List_DestructHook_String,
                                MUIA_List_DisplayHook,   &hook_disp_remote,
                                MUIA_List_Format,        "",
                            End,
                        End,
                        Child, HGroup,
                            Child, btn_remote_up = SimpleButton("Parent Dir"),
                        End,
                    End,

                End, /* HGroup file panels */

                /* --- Status bar --- */
                Child, txt_status = TextObject,
                    TextFrame,
                    MUIA_Text_Contents, "Ready.",
                End,

            End, /* VGroup */
        End, /* Window */
    End; /* Application */

    if (!app) {
        CloseLibrary(MUIMasterBase);
        return 1;
    }

    /* Initial button states */
    set(btn_disconnect, MUIA_Disabled, TRUE);
    set(btn_upload,     MUIA_Disabled, TRUE);
    set(btn_download,   MUIA_Disabled, TRUE);
    set(btn_remote_up,  MUIA_Disabled, TRUE);

    /* Close window -> quit application */
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             (IPTR)app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

    /* ---------------------------------------------------------- */
    /* Wire up hooks                                               */
    /* ---------------------------------------------------------- */
    HOOK_INIT(hook_connect_h,    do_connect);
    HOOK_INIT(hook_disconnect_h, do_disconnect);
    HOOK_INIT(hook_upload_h,     do_upload);
    HOOK_INIT(hook_download_h,   do_download);
    HOOK_INIT(hook_local_up_h,   do_local_up);
    HOOK_INIT(hook_remote_up_h,  do_remote_up);
    HOOK_INIT(hook_local_dbl_h,  do_local_dbl);
    HOOK_INIT(hook_remote_dbl_h, do_remote_dbl);

    DoMethod(btn_connect,    MUIM_Notify, MUIA_Pressed, FALSE,
             (IPTR)app, 2, MUIM_CallHook, (IPTR)&hook_connect_h);
    DoMethod(btn_disconnect, MUIM_Notify, MUIA_Pressed, FALSE,
             (IPTR)app, 2, MUIM_CallHook, (IPTR)&hook_disconnect_h);
    DoMethod(btn_upload,     MUIM_Notify, MUIA_Pressed, FALSE,
             (IPTR)app, 2, MUIM_CallHook, (IPTR)&hook_upload_h);
    DoMethod(btn_download,   MUIM_Notify, MUIA_Pressed, FALSE,
             (IPTR)app, 2, MUIM_CallHook, (IPTR)&hook_download_h);
    DoMethod(btn_local_up,   MUIM_Notify, MUIA_Pressed, FALSE,
             (IPTR)app, 2, MUIM_CallHook, (IPTR)&hook_local_up_h);
    DoMethod(btn_remote_up,  MUIM_Notify, MUIA_Pressed, FALSE,
             (IPTR)app, 2, MUIM_CallHook, (IPTR)&hook_remote_up_h);

    /* Double-click notifications go on the ListView, not the List */
    DoMethod(lv_local,  MUIM_Notify, MUIA_Listview_DoubleClick, TRUE,
             (IPTR)app, 2, MUIM_CallHook, (IPTR)&hook_local_dbl_h);
    DoMethod(lv_remote, MUIM_Notify, MUIA_Listview_DoubleClick, TRUE,
             (IPTR)app, 2, MUIM_CallHook, (IPTR)&hook_remote_dbl_h);

    /* Populate local panel and open window */
    local_refresh();
    set(win, MUIA_Window_Open, TRUE);

    /* ---------------------------------------------------------- */
    /* Main event loop                                             */
    /* ---------------------------------------------------------- */
    ULONG sigs = 0;
    while (DoMethod(app, MUIM_Application_NewInput, (IPTR)&sigs)
           != MUIV_Application_ReturnID_Quit)
    {
        if (sigs) {
            sigs = Wait(sigs | SIGBREAKF_CTRL_C);
            if (sigs & SIGBREAKF_CTRL_C) break;
        }
    }

    /* ---------------------------------------------------------- */
    /* Cleanup                                                     */
    /* ---------------------------------------------------------- */
    set(win, MUIA_Window_Open, FALSE);

    if (g_ftp.connected) {
        char resp[64]; ftp_cmd("QUIT", resp, sizeof(resp));
        CloseSocket(g_ftp.ctrl);
    }
    net_close();
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
