/*
 * console_ui.c
 * Console presentation helpers. Windows colours go through
 * SetConsoleTextAttribute; other platforms use ANSI escapes. Both are
 * disabled when stdout is not an interactive terminal.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "console_ui.h"

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define UI_ISATTY(fd) _isatty(fd)
#else
#include <unistd.h>
#define UI_ISATTY(fd) isatty(fd)
#endif

#define UI_WIDTH 78            /* total width incl. the border chars */
#define UI_INNER (UI_WIDTH - 2)

static int stdout_tty = 0;
static int stdin_tty = 0;

#ifdef _WIN32
static HANDLE console_handle;
static WORD default_attrs = 7;
#endif

void ui_init(void)
{
    stdout_tty = UI_ISATTY(1) ? 1 : 0;
    stdin_tty = UI_ISATTY(0) ? 1 : 0;
#ifdef _WIN32
    if (stdout_tty) {
        CONSOLE_SCREEN_BUFFER_INFO info;
        console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
        if (GetConsoleScreenBufferInfo(console_handle, &info)) {
            default_attrs = info.wAttributes;
        }
    }
#endif
}

int ui_stdout_is_tty(void)
{
    return stdout_tty;
}

void ui_style(UiStyle style)
{
    if (!stdout_tty) {
        return;
    }
#ifdef _WIN32
    {
        WORD attrs = default_attrs;
        switch (style) {
        case UI_TITLE:
            attrs = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
            break;
        case UI_SUCCESS:
            attrs = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            break;
        case UI_ERROR:
            attrs = FOREGROUND_RED | FOREGROUND_INTENSITY;
            break;
        case UI_WARN:
            attrs = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            break;
        default:
            attrs = default_attrs;
            break;
        }
        SetConsoleTextAttribute(console_handle, attrs);
    }
#else
    switch (style) {
    case UI_TITLE:   fputs("\033[1;36m", stdout); break;
    case UI_SUCCESS: fputs("\033[1;32m", stdout); break;
    case UI_ERROR:   fputs("\033[1;31m", stdout); break;
    case UI_WARN:    fputs("\033[1;33m", stdout); break;
    default:         fputs("\033[0m", stdout);    break;
    }
#endif
}

void ui_styled_line(UiStyle style, const char *text)
{
    ui_style(style);
    fputs(text, stdout);
    ui_style(UI_DEFAULT);
    fputc('\n', stdout);
}

void ui_rule(char fill)
{
    int i;

    fputc('+', stdout);
    for (i = 0; i < UI_INNER; i++) {
        fputc(fill, stdout);
    }
    fputs("+\n", stdout);
}

void ui_box_center(UiStyle style, const char *text)
{
    int len = (int)strlen(text);
    int left;
    int right;
    int i;

    if (len > UI_INNER) {
        len = UI_INNER;   /* truncate over-long lines defensively */
    }
    left = (UI_INNER - len) / 2;
    right = UI_INNER - len - left;
    fputc('|', stdout);
    for (i = 0; i < left; i++) {
        fputc(' ', stdout);
    }
    ui_style(style);
    for (i = 0; i < len; i++) {
        fputc(text[i], stdout);
    }
    ui_style(UI_DEFAULT);
    for (i = 0; i < right; i++) {
        fputc(' ', stdout);
    }
    fputs("|\n", stdout);
}

void ui_kv(const char *label, const char *value)
{
    printf("  %-22s: %s\n", label, value);
}

void ui_banner(void)
{
    ui_rule('=');
    ui_box_center(UI_TITLE, "CSA0312 - DATA STRUCTURES");
    ui_box_center(UI_TITLE, "INTELLIGENT EMERGENCY EVACUATION ROUTING SYSTEM");
    ui_box_center(UI_DEFAULT, "Athisaya U - 192571001");
    ui_rule('=');
    ui_styled_line(UI_SUCCESS,
        "  Status: graph engine ready  [HashMap + HashSet + Min-Heap +"
        " Dijkstra + cache]");
}

void ui_clear(void)
{
    if (!stdout_tty) {
        return;
    }
#ifdef _WIN32
    (void)system("cls");
#else
    (void)system("clear");
#endif
}

void ui_pause(void)
{
    char buf[128];

    if (!stdin_tty) {
        return;
    }
    ui_style(UI_WARN);
    fputs("  Press Enter to continue...", stdout);
    ui_style(UI_DEFAULT);
    fflush(stdout);
    if (fgets(buf, (int)sizeof(buf), stdin) == NULL) {
        clearerr(stdin);
    }
}

/* Reads one line and parses a long strictly (whole line must be a
 * number, surrounding whitespace allowed). */
static int read_long_line(const char *prompt, long *out)
{
    char buf[128];
    char *end;
    char *p;
    long value;

    ui_style(UI_TITLE);
    fputs(prompt, stdout);
    ui_style(UI_DEFAULT);
    fputs(": ", stdout);
    fflush(stdout);
    if (fgets(buf, (int)sizeof(buf), stdin) == NULL) {
        return -2;
    }
    p = buf;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p == '\0' || *p == '\n') {
        return -1;
    }
    value = strtol(p, &end, 10);
    while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') {
        end++;
    }
    if (*end != '\0') {
        return -1;   /* trailing junk such as "12abc" */
    }
    *out = value;
    return 0;
}

int ui_read_long(const char *prompt, long *out)
{
    return read_long_line(prompt, out);
}

int ui_read_int(const char *prompt, int *out)
{
    long value;
    int rc = read_long_line(prompt, &value);

    if (rc != 0) {
        return rc;
    }
    if (value < INT_MIN || value > INT_MAX) {
        return -1;
    }
    *out = (int)value;
    return 0;
}
