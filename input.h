#ifndef INPUT_H
#define INPUT_H

#include "basic.h"
#include "text.h"

typedef struct Input Input;

#define INPUT_SIZE 1024

struct Input {
  char data[INPUT_SIZE];
  int size;
  int cursor;
  int offset;
  int width;
  int padding;
};

extern Input input;

int get_input_keycode();
bool input_is_ready();
char* get_drag_and_drop_buffer();

void input_update_width(int width, int padding);
void input_clear();
void input_handle(int code);
void print_input();
int  get_input_cursor();
void input_replace(char* data, int delete_size, int index);

#endif