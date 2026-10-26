/* titem_editor.c  --  三国志9 PK 宝物编辑器 (Win32, ANSI, 无第三方依赖)
 *
 * 目标文件: 我的文档\Koei\San9PK Tc\TItem\D_TItem.s9
 * 格式: 16 字节文件头 + 176 条 * 24 字节 (每条 12 个 little-endian u16)
 * 安全: 保存前自动备份 -> 写 .tmp -> 回读逐字节校验 -> 覆盖 -> 删除 .tmp
 *
 * 编译 (MinGW 32 位):
 *   gcc -O2 -mwindows -o San9TItemEditor.exe titem_editor.c -lgdi32 -luser32 -lkernel32
 */

#include <windows.h>
#include <stdlib.h>
#include <string.h>

#define REC_COUNT   176
#define REC_SIZE    24
#define HD_SIZE     16
#define FILE_SIZE   (HD_SIZE + REC_COUNT * REC_SIZE)   /* 4240 */

typedef struct {
    const char *name;
    int ro;
    int minv;
    int maxv;
} FIELD;

static const FIELD g_fields[12] = {
    { "序号",     1,     0,   175 },
    { "名称ID",   0,     0, 65535 },
    { "保留2",    0,     0, 65535 },
    { "图像代号", 0,     0,   113 },
    { "宝物种类", 0,     0,    32 },
    { "忠诚度",   0,     0,   255 },
    { "能力值",   0,     0,    15 },
    { "兵法代号", 0,     0, 65535 },
    { "持有武将", 0,     0, 65535 },
    { "状态",     0,     0,     2 },
    { "地点代号", 0,     0,   183 },
    { "保留11",   0,     0, 65535 }
};

static unsigned char  g_header[HD_SIZE];
static unsigned short g_rec[REC_COUNT][12];
static char  g_path[512];
static char  g_buf[1024];
static char  g_buf2[1024];

static HWND  g_main, g_list, g_status, g_pathEdit;
static HFONT g_font;
static HWND  g_edit[12];
static HWND  g_lbl[12];
static HWND  g_btnLoad, g_btnSave, g_btnCheck, g_btnClear, g_btnCopy;
static int   g_sel = -1;
static int   g_loaded = 0;

/* ---------- 工具 ---------- */

static void status_set(const char *s)
{
    SetWindowTextA(g_status, s);
}

static void msgbox(const char *s, UINT flags)
{
    MessageBoxA(g_main, s, "宝物编辑器", flags);
}

static int file_size_of(const char *path)
{
    HANDLE h;
    DWORD sz;
    h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return -1;
    sz = GetFileSize(h, NULL);
    CloseHandle(h);
    return (sz == INVALID_FILE_SIZE) ? -1 : (int)sz;
}

/* 读整个文件到缓冲; 返回实际字节数, 失败 -1 */
static int file_read(const char *path, unsigned char *dst, int cap)
{
    HANDLE h;
    DWORD rd = 0;
    h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return -1;
    if (!ReadFile(h, dst, (DWORD)cap, &rd, NULL)) {
        CloseHandle(h);
        return -1;
    }
    CloseHandle(h);
    return (int)rd;
}

static int file_write(const char *path, const unsigned char *src, int len)
{
    HANDLE h;
    DWORD wr = 0;
    h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    if (!WriteFile(h, src, (DWORD)len, &wr, NULL) || (int)wr != len) {
        CloseHandle(h);
        return 0;
    }
    CloseHandle(h);
    return 1;
}

/* ---------- 编解码 ---------- */

static void decode_file(const unsigned char *b)
{
    int n, f;
    for (n = 0; n < HD_SIZE; n++) g_header[n] = b[n];
    for (n = 0; n < REC_COUNT; n++) {
        int base = HD_SIZE + n * REC_SIZE;
        for (f = 0; f < 12; f++) {
            g_rec[n][f] = (unsigned short)(b[base + f * 2] |
                          ((unsigned short)b[base + f * 2 + 1] << 8));
        }
    }
}

static void encode_file(unsigned char *b)
{
    int n, f;
    ZeroMemory(b, FILE_SIZE);
    for (n = 0; n < HD_SIZE; n++) b[n] = g_header[n];
    for (n = 0; n < REC_COUNT; n++) {
        int base = HD_SIZE + n * REC_SIZE;
        for (f = 0; f < 12; f++) {
            unsigned short v = g_rec[n][f];
            b[base + f * 2]     = (unsigned char)(v & 0xFF);
            b[base + f * 2 + 1] = (unsigned char)((v >> 8) & 0xFF);
        }
    }
}

/* ---------- 校验 ---------- */

static int validate(char *report, int cap)
{
    int n, f, errs = 0, warns = 0;
    int off = 0;
    report[0] = 0;

    for (n = 0; n < REC_COUNT; n++) {
        for (f = 0; f < 12; f++) {
            int v = (int)g_rec[n][f];
            int iserr = (f == 6 || f == 9);
            if (v < g_fields[f].minv || v > g_fields[f].maxv) {
                if (iserr) errs++; else warns++;
                if (off < cap - 120) {
                    off += wsprintfA(report + off, "第%d条 %s=%d 超出 %d~%d%s\r\n",
                                     n + 1, g_fields[f].name, v,
                                     g_fields[f].minv, g_fields[f].maxv,
                                     iserr ? " [错误]" : "");
                }
            }
        }
        if ((int)g_rec[n][6] > 15) {
            errs++;
            if (off < cap - 120) {
                off += wsprintfA(report + off,
                                 "第%d条 能力值 %d > 15 (游戏内会异常) [错误]\r\n",
                                 n + 1, (int)g_rec[n][6]);
            }
        }
        if ((int)g_rec[n][9] == 0) {
            warns++;
            if (off < cap - 120) {
                off += wsprintfA(report + off,
                                 "第%d条 状态=0 (非 1/2, 可能未初始化)\r\n", n + 1);
            }
        }
    }
    return (errs * 100000) + warns;   /* 打包返回, 便于调用方判断 */
}

/* ---------- 列表 ---------- */

static void list_refresh(void)
{
    int n, i;
    SendMessageA(g_list, LB_RESETCONTENT, 0, 0);
    for (n = 0; n < REC_COUNT; n++) {
        int empty = 1;
        for (i = 1; i < 12; i++) {
            if (g_rec[n][i] != 0 && g_rec[n][i] != 0xFFFF) { empty = 0; break; }
        }
        if (empty) {
            wsprintfA(g_buf, "%3d |  (空)", n + 1);
        } else {
            wsprintfA(g_buf, "%3d | ID=%d 种=%d 忠=%d 能=%d 法=%d 态=%d",
                      n + 1, (int)g_rec[n][1], (int)g_rec[n][4],
                      (int)g_rec[n][5], (int)g_rec[n][6],
                      (int)g_rec[n][7], (int)g_rec[n][9]);
        }
        SendMessageA(g_list, LB_ADDSTRING, 0, (LPARAM)g_buf);
    }
}

static void fields_show(void)
{
    int f;
    for (f = 0; f < 12; f++) {
        if (g_sel < 0 || g_sel >= REC_COUNT) {
            SetWindowTextA(g_edit[f], "");
            EnableWindow(g_edit[f], FALSE);
        } else {
            wsprintfA(g_buf, "%d", (int)g_rec[g_sel][f]);
            SetWindowTextA(g_edit[f], g_buf);
            EnableWindow(g_edit[f], g_fields[f].ro ? FALSE : TRUE);
        }
    }
}

/* ---------- 动作 ---------- */

static void do_load(void)
{
    int sz, rd;
    unsigned char *raw;

    GetWindowTextA(g_pathEdit, g_path, sizeof(g_path) - 1);
    if (g_path[0] == 0) { msgbox("请先填写 D_TItem.s9 路径", MB_ICONWARNING); return; }

    sz = file_size_of(g_path);
    if (sz < 0) { msgbox("文件不存在或无法访问。", MB_ICONERROR); return; }
    if (sz != FILE_SIZE) {
        wsprintfA(g_buf, "文件长度异常：期望 %d 字节，实际 %d 字节。\r\n\r\n请确认这是 D_TItem.s9（16B头 + 176 x 24B）。",
                  FILE_SIZE, sz);
        msgbox(g_buf, MB_ICONERROR);
        return;
    }

    raw = (unsigned char *)HeapAlloc(GetProcessHeap(), 0, FILE_SIZE);
    if (!raw) { msgbox("内存不足", MB_ICONERROR); return; }
    rd = file_read(g_path, raw, FILE_SIZE);
    if (rd != FILE_SIZE) {
        HeapFree(GetProcessHeap(), 0, raw);
        msgbox("读取失败（长度不符）。", MB_ICONERROR);
        return;
    }
    decode_file(raw);
    HeapFree(GetProcessHeap(), 0, raw);

    g_loaded = 1;
    g_sel = -1;
    list_refresh();
    fields_show();

    sz = validate(g_buf2, sizeof(g_buf2) - 1);
    wsprintfA(g_buf, "已载入 176 条；校验：%d 个错误 / %d 个提示",
              sz / 100000, sz % 100000);
    status_set(g_buf);
}

static void do_check(void)
{
    int r;
    if (!g_loaded) { msgbox("请先载入文件", MB_ICONWARNING); return; }
    r = validate(g_buf2, sizeof(g_buf2) - 1);
    wsprintfA(g_buf, "校验完成：%d 个错误，%d 个提示。\r\n\r\n%.700s",
              r / 100000, r % 100000, g_buf2);
    if (r == 0) lstrcatA(g_buf, "\r\n(未发现问题)");
    msgbox(g_buf, (r / 100000) ? MB_ICONERROR : MB_ICONINFORMATION);
}

static void do_apply(void)
{
    int f, v;
    char tmp[64];
    if (g_sel < 0 || !g_loaded) { msgbox("请先在左侧选择一条记录", MB_ICONWARNING); return; }
    for (f = 0; f < 12; f++) {
        if (g_fields[f].ro) continue;
        GetWindowTextA(g_edit[f], tmp, sizeof(tmp) - 1);
        v = atoi(tmp);
        if (v < 0 || v > 65535) {
            wsprintfA(g_buf, "字段「%s」的值非法：%s", g_fields[f].name, tmp);
            msgbox(g_buf, MB_ICONERROR);
            return;
        }
        g_rec[g_sel][f] = (unsigned short)v;
    }
    list_refresh();
    SendMessageA(g_list, LB_SETCURSEL, (WPARAM)g_sel, 0);
    fields_show();
    status_set("已应用到当前记录（尚未写盘，点「保存」生效）");
}

static void do_clear(void)
{
    int f;
    if (g_sel < 0 || !g_loaded) { msgbox("请先选择一条记录", MB_ICONWARNING); return; }
    wsprintfA(g_buf, "把第 %d 条清零（保留序号列）？", g_sel + 1);
    if (MessageBoxA(g_main, g_buf, "确认", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    for (f = 1; f < 12; f++) g_rec[g_sel][f] = 0;
    list_refresh();
    SendMessageA(g_list, LB_SETCURSEL, (WPARAM)g_sel, 0);
    fields_show();
    status_set("已清空当前记录（尚未写盘）");
}

static void do_copy(void)
{
    int n, f, src;
    if (g_sel < 0 || !g_loaded) { msgbox("请先选择一条记录", MB_ICONWARNING); return; }
    src = g_sel;
    for (n = 0; n < REC_COUNT; n++) {
        int empty = 1;
        if (n == src) continue;
        for (f = 1; f < 12; f++) {
            if (g_rec[n][f] != 0 && g_rec[n][f] != 0xFFFF) { empty = 0; break; }
        }
        if (empty) {
            for (f = 1; f < 12; f++) g_rec[n][f] = g_rec[src][f];
            g_rec[n][0] = (unsigned short)n;
            g_sel = n;
            list_refresh();
            SendMessageA(g_list, LB_SETCURSEL, (WPARAM)n, 0);
            fields_show();
            wsprintfA(g_buf, "已复制到空位：第 %d 条（尚未写盘）", n + 1);
            status_set(g_buf);
            return;
        }
    }
    msgbox("没有找到空位（176 条都已占用）。\r\n可先选一条点「清空本条」腾出位置。", MB_ICONWARNING);
}

static void timestamp(char *out)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    wsprintfA(out, "%04d%02d%02d_%02d%02d%02d",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
}

static void do_save(void)
{
    unsigned char *out, *back;
    char bak[640], tmp[640], ts[32];
    int r, ok;

    if (!g_loaded) { msgbox("请先载入文件", MB_ICONWARNING); return; }

    r = validate(g_buf2, sizeof(g_buf2) - 1);
    if (r / 100000 > 0) {
        wsprintfA(g_buf, "存在 %d 个【错误】级问题，游戏内可能异常：\r\n\r\n%.600s\r\n\r\n仍要保存吗？",
                  r / 100000, g_buf2);
        if (MessageBoxA(g_main, g_buf, "确认保存", MB_YESNO | MB_ICONWARNING) != IDYES) return;
    }

    /* 1) 备份 */
    timestamp(ts);
    lstrcpynA(bak, g_path, sizeof(bak) - 40);
    lstrcatA(bak, ".bak.");
    lstrcatA(bak, ts);
    if (!CopyFileA(g_path, bak, FALSE)) {
        msgbox("备份失败，已中止保存（原文件未改动）。", MB_ICONERROR);
        return;
    }

    /* 2) 编码并写临时文件 */
    out = (unsigned char *)HeapAlloc(GetProcessHeap(), 0, FILE_SIZE);
    if (!out) { msgbox("内存不足", MB_ICONERROR); return; }
    encode_file(out);

    lstrcpynA(tmp, g_path, sizeof(tmp) - 8);
    lstrcatA(tmp, ".tmp");
    if (!file_write(tmp, out, FILE_SIZE)) {
        HeapFree(GetProcessHeap(), 0, out);
        msgbox("写临时文件失败，原文件未改动。", MB_ICONERROR);
        return;
    }

    /* 3) 回读逐字节校验 */
    back = (unsigned char *)HeapAlloc(GetProcessHeap(), 0, FILE_SIZE);
    if (!back) { HeapFree(GetProcessHeap(), 0, out); msgbox("内存不足", MB_ICONERROR); return; }
    if (file_read(tmp, back, FILE_SIZE) != FILE_SIZE) {
        ok = 0;
    } else {
        ok = (memcmp(out, back, FILE_SIZE) == 0);
    }
    HeapFree(GetProcessHeap(), 0, back);

    if (!ok) {
        HeapFree(GetProcessHeap(), 0, out);
        DeleteFileA(tmp);
        msgbox("回读校验失败，已中止（原文件未改动）。", MB_ICONERROR);
        return;
    }

    /* 4) 覆盖正式文件 */
    if (!CopyFileA(tmp, g_path, FALSE)) {
        HeapFree(GetProcessHeap(), 0, out);
        DeleteFileA(tmp);
        msgbox("覆盖正式文件失败（原文件可能未改动）。", MB_ICONERROR);
        return;
    }
    DeleteFileA(tmp);
    HeapFree(GetProcessHeap(), 0, out);

    lstrcpynA(g_buf, "已保存。\r\n\r\n备份：", sizeof(g_buf) - 300);
    lstrcatA(g_buf, bak);
    status_set("已保存");
    msgbox(g_buf, MB_ICONINFORMATION);
}

/* ---------- 窗口过程 ---------- */

#define ID_PATH    1001
#define ID_LOAD    1002
#define ID_SAVE    1003
#define ID_CHECK   1004
#define ID_LIST    1005
#define ID_CLEAR   1006
#define ID_COPY    1007
#define ID_EDIT0   1100

static void create_ui(void)
{
    int i;
    HFONT hf = (HFONT)CreateFontA(15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                  "宋体");
    g_font = hf;

    g_pathEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                 0, 0, 100, 24, g_main, (HMENU)ID_PATH,
                                 GetModuleHandleA(NULL), NULL);

    g_btnLoad  = CreateWindowExA(0, "BUTTON", "载入",
                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                 0, 0, 70, 24, g_main, (HMENU)ID_LOAD, NULL, NULL);
    g_btnSave  = CreateWindowExA(0, "BUTTON", "保存(自动备份)",
                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                                 0, 0, 130, 24, g_main, (HMENU)ID_SAVE, NULL, NULL);
    g_btnCheck = CreateWindowExA(0, "BUTTON", "仅校验",
                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                 0, 0, 80, 24, g_main, (HMENU)ID_CHECK, NULL, NULL);

    g_list = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "",
                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                             LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                             0, 0, 300, 200, g_main, (HMENU)ID_LIST, NULL, NULL);

    for (i = 0; i < 12; i++) {
        g_lbl[i] = CreateWindowExA(0, "STATIC", g_fields[i].name,
                                   WS_CHILD | WS_VISIBLE | SS_LEFT,
                                   0, 0, 80, 20, g_main, NULL, NULL, NULL);
        g_edit[i] = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                    ES_AUTOHSCROLL | ES_NUMBER,
                                    0, 0, 80, 22, g_main,
                                    (HMENU)(ID_EDIT0 + i), NULL, NULL);
    }

    g_btnClear = CreateWindowExA(0, "BUTTON", "清空本条",
                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                 0, 0, 90, 24, g_main, (HMENU)ID_CLEAR, NULL, NULL);
    g_btnCopy  = CreateWindowExA(0, "BUTTON", "复制到空位",
                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                 0, 0, 100, 24, g_main, (HMENU)ID_COPY, NULL, NULL);

    g_status = CreateWindowExA(WS_EX_CLIENTEDGE, "STATIC", "就绪",
                               WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
                               0, 0, 300, 22, g_main, NULL, NULL, NULL);
}

static void apply_font(void)
{
    int i;
    HWND all[24];
    int c = 0;
    all[c++] = g_pathEdit; all[c++] = g_list; all[c++] = g_status;
    all[c++] = g_btnLoad; all[c++] = g_btnSave; all[c++] = g_btnCheck;
    all[c++] = g_btnClear; all[c++] = g_btnCopy;
    for (i = 0; i < 12; i++) { all[c++] = g_edit[i]; all[c++] = g_lbl[i]; }
    for (i = 0; i < c; i++) SendMessageA(all[i], WM_SETFONT, (WPARAM)g_font, TRUE);
}

static void do_layout(HWND hwnd)
{
    RECT rc;
    int W, H, i, top = 8, leftW;
    GetClientRect(hwnd, &rc);
    W = rc.right - rc.left;
    H = rc.bottom - rc.top;

    /* 顶栏 */
    {
      int pw = W - 8 - 8 - 70 - 130 - 80 - 18;
      if (pw < 120) pw = 120;
      MoveWindow(g_pathEdit, 8, top, pw, 24, TRUE);
    }
    { int x = W - 8 - 70 - 130 - 80 - 12;
      MoveWindow(g_btnLoad,  x, top, 70, 24, TRUE);
      MoveWindow(g_btnSave,  x + 74, top, 130, 24, TRUE);
      MoveWindow(g_btnCheck, x + 208, top, 80, 24, TRUE);
    }

    /* 底栏 */
    MoveWindow(g_status, 8, H - 30, W - 16, 22, TRUE);

    /* 左列表 */
    leftW = (W * 55) / 100;
    if (leftW < 320) leftW = 320;
    MoveWindow(g_list, 8, top + 32, leftW - 8, H - top - 32 - 40, TRUE);

    /* 右侧字段 */
    {
        int x = leftW + 8, y = top + 32;
        int iw = 90, lw = 84;
        for (i = 0; i < 12; i++) {
            MoveWindow(g_lbl[i], x, y + i * 28 + 3, lw, 20, TRUE);
            MoveWindow(g_edit[i], x + lw, y + i * 28, iw, 22, TRUE);
        }
        MoveWindow(g_btnClear, x, y + 12 * 28 + 10, 90, 26, TRUE);
        MoveWindow(g_btnCopy,  x + 96, y + 12 * 28 + 10, 100, 26, TRUE);
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        create_ui();
        apply_font();
        do_layout(hwnd);
        /* 默认路径 */
        {
            char home[MAX_PATH];
            char cand1[512], cand2[512];
            if (GetEnvironmentVariableA("USERPROFILE", home, MAX_PATH) > 0) {
                wsprintfA(cand1, "%s\\Documents\\Koei\\San9PK Tc\\TItem\\D_TItem.s9", home);
                wsprintfA(cand2, "%s\\Documents\\Koei\\San9 Tc\\TItem\\D_TItem.s9", home);
                if (file_size_of(cand1) > 0) SetWindowTextA(g_pathEdit, cand1);
                else if (file_size_of(cand2) > 0) SetWindowTextA(g_pathEdit, cand2);
                else SetWindowTextA(g_pathEdit, cand1);
            }
        }
        fields_show();
        SetTimer(hwnd, 1, 200, NULL);   /* 首次自动载入 */
        return 0;

    case WM_TIMER:
        if (wp == 1) {
            KillTimer(hwnd, 1);
            if (file_size_of(g_path) == -1) {
                GetWindowTextA(g_pathEdit, g_path, sizeof(g_path) - 1);
            }
            if (file_size_of(g_path) == FILE_SIZE) do_load();
        }
        return 0;

    case WM_SIZE:
        do_layout(hwnd);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_LOAD:  do_load();  return 0;
        case ID_SAVE:  do_save();  return 0;
        case ID_CHECK: do_check(); return 0;
        case ID_CLEAR: do_clear(); return 0;
        case ID_COPY:  do_copy();  return 0;
        default: break;
        }
        if (LOWORD(wp) == ID_EDIT0 || (LOWORD(wp) > ID_EDIT0 && LOWORD(wp) < ID_EDIT0 + 12)) {
            if (HIWORD(wp) == EN_CHANGE) { /* 即时不同步, 点应用才写 */ }
            return 0;
        }
        if (LOWORD(wp) == ID_LIST && HIWORD(wp) == LBN_SELCHANGE) {
            int idx = (int)SendMessageA(g_list, LB_GETCURSEL, 0, 0);
            if (idx >= 0 && idx < REC_COUNT) { g_sel = idx; fields_show(); }
            return 0;
        }
        break;

    case WM_DESTROY:
        if (g_font) DeleteObject(g_font);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdline, int nShow)
{
    WNDCLASSA wc;
    MSG msg;
    HWND hwnd;

    (void)hPrev;

    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "San9TItemEditorWnd";
    if (!RegisterClassA(&wc)) return 1;

    hwnd = CreateWindowExA(0, "San9TItemEditorWnd",
                           "三国志9 PK · 宝物编辑器（离线 / 校验 / 自动备份）",
                           WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT, 900, 600,
                           NULL, NULL, hInst, NULL);
    if (!hwnd) return 1;
    g_main = hwnd;

    ShowWindow(hwnd, (nShow == 0) ? SW_SHOWNORMAL : nShow);
    UpdateWindow(hwnd);

    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    (void)cmdline;
    return 0;
}
