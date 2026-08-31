/*
 * console_ui.h
 * Small console-presentation helpers for the interactive dashboard:
 * colours, borders, banners, screen clearing, pausing and safe numeric
 * input. Pure presentation - no routing logic lives here.
 *
 * Colours use SetConsoleTextAttribute on Windows and ANSI escapes
 * elsewhere, and are disabled automatically when stdout is redirected
 * to a file, so captured output never contains escape garbage.
 * Only portable ASCII characters are printed.
 */
#ifndef CONSOLE_UI_H
#define CONSOLE_UI_H

/* Visual styles (mapped to colours where available). */
typedef enum {
    UI_DEFAULT = 0,
    UI_TITLE,     /* cyan   - section titles                */
    UI_SUCCESS,   /* green  - success messages              */
    UI_ERROR,     /* red    - errors / unreachable results  */
    UI_WARN       /* yellow - warnings / progress notices   */
} UiStyle;

/* Detects terminal capabilities. Call once at program start. */
void ui_init(void);

/* 1 when stdout is an interactive terminal (colours/clearing on). */
int ui_stdout_is_tty(void);

/* Switches the current output colour; ui_style(UI_DEFAULT) restores. */
void ui_style(UiStyle style);

/* Prints one whole line in the given style (adds the newline). */
void ui_styled_line(UiStyle style, const char *text);

/* Prints a full-width border line, e.g. ui_rule('='): +====...====+ */
void ui_rule(char fill);

/* Prints "| <text centered> |" across the full width. */
void ui_box_center(UiStyle style, const char *text);

/* Prints "  <label> : <value>" with the label padded for alignment. */
void ui_kv(const char *label, const char *value);

/* Project banner (title, course, author, status line). */
void ui_banner(void);

/* Clears the screen when interactive; no-op when redirected. */
void ui_clear(void);

/* "Press Enter to continue" when stdin is interactive; no-op when
 * input is piped (keeps scripted runs from desynchronising). */
void ui_pause(void);

/*
 * Safe line-based numeric input. Both print the prompt, read one line
 * with fgets and parse it strictly.
 * Return  0 on success,
 *        -1 when the line is not a valid number (caller may re-prompt),
 *        -2 on end of input (caller should cancel the operation).
 */
int ui_read_int(const char *prompt, int *out);
int ui_read_long(const char *prompt, long *out);

#endif /* CONSOLE_UI_H */
