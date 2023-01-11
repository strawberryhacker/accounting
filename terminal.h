#ifndef TERMINAL_H
#define TERMINAL_H

#include "basic.h"
#include "stdbool.h"

enum {
  KEYCODE_CTRL_C          = 3,
  KEYCODE_CTRL_DELETE     = 8,
  KEYCODE_TAB             = 9,
  KEYCODE_ENTER           = 10,
  KEYCODE_ESCAPE          = 27,
  KEYCODE_PRINTABLE_START = 32,
  KEYCODE_PRINTABLE_END   = 126,
  KEYCODE_DELETE          = 127,
  KEYCODE_ASCII_END       = 255,
  KEYCODE_NONE,
  KEYCODE_UP,
  KEYCODE_DOWN,
  KEYCODE_LEFT,
  KEYCODE_RIGHT,
  KEYCODE_CTRL_UP,
  KEYCODE_CTRL_DOWN,
  KEYCODE_CTRL_LEFT,
  KEYCODE_CTRL_RIGHT,
  KEYCODE_END,
  KEYCODE_HOME,
  KEYCODE_DRAG_AND_DROP_PATH,
};

void terminal_init();
int  get_input_keycode();
char* get_drag_and_drop_buffer();
int  print(const char* text, ...);
void flush();
void get_size(int* width, int* height);

static inline void bold()                   { print("\033[1m"); }
static inline void faint()                  { print("\033[2m\033[3m"); }
static inline void underline()              { print("\033[4m"); }
static inline void not_bold()               { print("\033[22m"); }
static inline void clear_all_right()        { print("\033[0J"); }
static inline void push_cursor()            { print("\033[s"); }
static inline void pop_cursor()             { print("\033[u"); }
static inline void cursor_up(int n)         { print("\033[%dA", n); }
static inline void cursor_down(int n)       { print("\033[%dB", n); }
static inline void cursor_right(int n)      { print("\033[%dC", n); }
static inline void cursor_left(int n)       { print("\033[%dD", n); }
static inline void clear_to_right()         { print("\033[K"); }
static inline void clear_line()             { print("\033[2K"); }
static inline void cursor_style_line()      { print("\033[5 q"); }
static inline void format_off()             { print("\033[0m"); }
static inline void hide_cursor()            { print("\033[?25l"); }
static inline void show_cursor()            { print("\033[?25h"); }
static inline void clear_all()              { print("\033[2J"); }
static inline void invert()                 { print("\033[7m"); }
static inline void set_x_cursor(int x)      { print("\033[%dG", x + 1); }
static inline void set_cursor(int x, int y) { print("\033[%d;%dH", y + 1, x + 1); }

#endif
