#include "terminal.h"
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <string.h>

#define OUTPUT_BUFFER_SIZE (4096 * 4096 * 16)
static char output_buffer[OUTPUT_BUFFER_SIZE];
static int  output_size;

static void handle_resize();

int print(const char* text, ...) {
  if (output_size + 4000 > OUTPUT_BUFFER_SIZE) {
    flush();
  }
  va_list arguments;
  va_start(arguments, text);
  int size = vsnprintf(&output_buffer[output_size], OUTPUT_BUFFER_SIZE - output_size, text, arguments);
  va_end(arguments);
  output_size += size;
  return size;
}

void flush() {
  assert(write(STDOUT_FILENO, output_buffer, output_size) == output_size);
  output_size = 0;
}

struct termios default_terminal;

void terminal_reset() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &default_terminal);
}

void terminal_init() {
  setvbuf(stdout, NULL, _IONBF, 0);
  tcgetattr(STDIN_FILENO, &default_terminal);
  struct termios terminal = default_terminal;
  atexit(terminal_reset);

  terminal.c_iflag     &= ~(INPCK  | ISTRIP | IXON);
  terminal.c_lflag     &= ~(ICANON | ECHO  ); // | ISIG);
  terminal.c_oflag     &= ~(OPOST  | ONLCR);
  terminal.c_cc[VMIN]   = 0;
  terminal.c_cc[VTIME]  = 1;

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal);
  signal(SIGWINCH, (void* )handle_resize);
  handle_resize();
  cursor_style_line();
  flush();
}

static void get_cursor_now(int* x, int* y) {
  printf("\033[6n");

  char data[32];
  int size = 0;

  while ((size_t)size < sizeof(data)) {
    assert(read(STDIN_FILENO, &data[size], 1) == 1);
    if (data[size] == 'R') break;
    size++;
  }

  data[size] = 0;

  assert(data[0] == '\033' && data[1] == '[');
  assert(sscanf(&data[2], "%d;%d", y, x) == 2);
  *x -= 1;
  *y -= 1;
}

static void set_cursor_now(int x, int y) {
  printf("\033[%d;%dH", y + 1, x + 1);
}

void get_size_internal(int* width, int* height) {
  int x, y;
  get_cursor_now(&x, &y);
  set_cursor_now(500, 500);
  get_cursor_now(width, height);
  set_cursor_now(x, y);
}

static int screen_height, screen_width;

static void handle_resize() {
  get_size_internal(&screen_width, &screen_height);
}

void get_size(int* width, int* height) {
  *width  = screen_width;
  *height = screen_height;
}
