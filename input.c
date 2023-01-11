#include "input.h"
#include "terminal.h"
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

Input input;
static char drag_and_drop_buffer[1024];

static bool is_letter_or_number(char c) {
  char lowercase = c | 0x20;
  return (('a' <= lowercase) && (lowercase <= 'z')) || (c == '_') || (('0' <= c) && (c <= '9'));
}

static void delete_character(bool left) {
  if ( left && !input.cursor) return;
  if (!left && (input.cursor == input.size)) return;

  int index = input.cursor;
  if (!left) index++;

  memmove(&input.data[index - 1], &input.data[index], input.size - index);

  input.size--;
  input.data[input.size] = 0;
}

static void move_cursor(bool left) {
  if ( left && input.cursor) input.cursor--;
  if (!left && input.cursor < input.size) input.cursor++;
}

static bool can_move_cursor_or_delete(bool left) {
  return (left && input.cursor) || (!left && (input.cursor < input.size));
}

static void move_cursor_or_delete(bool left, bool delete, bool word_operation) {
  if (!can_move_cursor_or_delete(left)) return;

  char c;
  char first                     = input.data[left ? input.cursor - 1 : input.cursor];
  char first_is_space            = (first == ' ');
  char first_is_letter_or_number = is_letter_or_number(first);

  do {
    if (delete) delete_character(left);
    move_cursor(left);

    if (!can_move_cursor_or_delete(left)) return;

    c = input.data[left ? input.cursor - 1 : input.cursor];

    if (first_is_space && is_letter_or_number(c)) {
      first_is_letter_or_number = true;
      first_is_space = false;
    } 
  } while (word_operation && ((first_is_letter_or_number && is_letter_or_number(c)) || (first_is_space && (c == ' '))));
}

void input_clear() {
  input.offset = 0;
  input.size = 0;
  input.cursor = 0;
  input.data[0] = 0;
}

static void update_offset() {
  input.offset = compute_offset(input.cursor, input.offset, input.width, input.padding, 0);
}

void input_update_width(int width, int padding) {
  input.offset = 0;
  input.width = width;
  input.padding = padding;
  update_offset();
}

void input_handle(int code) {
  if (code == KEYCODE_NONE) return;

  switch (code) {
    case KEYCODE_LEFT:
      move_cursor_or_delete(true, false, false);
      break;
    case KEYCODE_CTRL_LEFT:
      move_cursor_or_delete(true, false, true);
      break;
    case KEYCODE_RIGHT:
      move_cursor_or_delete(false, false, false);
      break;
    case KEYCODE_CTRL_RIGHT:
      move_cursor_or_delete(false, false, true);
      break;
    case KEYCODE_CTRL_DELETE:
      move_cursor_or_delete(true, true, true);
      break;
    case KEYCODE_DELETE:
      move_cursor_or_delete(true, true, false);
      break;
    case KEYCODE_END:
      input.cursor = input.size;
      break;
    case KEYCODE_HOME:
      input.cursor = 0;
      break;
    default:
      if (KEYCODE_PRINTABLE_START <= code && code <= KEYCODE_PRINTABLE_END) {
        memmove(&input.data[input.cursor + 1], &input.data[input.cursor], input.size - input.cursor);
        input.data[input.cursor] = (char)code;
        input.cursor++;
        input.size++;
      }
  }

  input.data[input.size] = 0;
  update_offset();
}

void input_replace(char* data, int delete_size, int index) {
  int size = strlen(data);
  memmove(&input.data[index + size], &input.data[index + delete_size], input.size - index - delete_size);
  memcpy(&input.data[index], data, size);
  input.cursor = index + size;
  input.size = input.size + size - delete_size;
  input.offset = 0;
  update_offset();
}

void print_input() {
  int width = min(input.width, input.size - input.offset);
  print("%.*s", width, input.data + input.offset);
}

int get_input_cursor() {
  return input.cursor - input.offset;
}

bool input_is_ready() {
  fd_set set;
  FD_ZERO(&set);
  FD_SET(STDIN_FILENO, &set);

  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;

  return select(1, &set, 0, 0, &timeout) == 1;
}

char* get_drag_and_drop_buffer() {
  return drag_and_drop_buffer;
}

int get_input_keycode() {
  if (!input_is_ready()) return KEYCODE_NONE;

  char keys[256];
  int  size = read(STDIN_FILENO, keys, sizeof(keys) - 1);
  int  code = KEYCODE_NONE;

  keys[size] = 0;

  char escape  = (keys[0] == '\033');
  char bracket = (keys[1] == '[');

  if (size == 1 && keys[0] > 0) {
    code = keys[0];
  } else if (size > 2 && escape && bracket) {
    if (size == 3) {
      switch (keys[2]) {
        case 'A': code = KEYCODE_UP;      break;
        case 'B': code = KEYCODE_DOWN;    break;
        case 'D': code = KEYCODE_LEFT;    break;
        case 'C': code = KEYCODE_RIGHT;   break;
        case 'H': code = KEYCODE_HOME;    break;
        case 'F': code = KEYCODE_END;     break;
      }
    } else if ((size == 6) && (keys[2] == '1') && (keys[3] == ';') && (keys[4] == '5')) {
      switch (keys[5]) {
        case 'A': code = KEYCODE_CTRL_UP;    break;
        case 'B': code = KEYCODE_CTRL_DOWN;  break;
        case 'D': code = KEYCODE_CTRL_LEFT;  break;
        case 'C': code = KEYCODE_CTRL_RIGHT; break;
      }
    }
  } else if (size >= 6 && !escape) {
    // Some terminals paste the path inside single or double quotes.
    char* source = keys;
    char quote = source[0];

    if (quote == '\'' || quote == '"') {
      source++;
      for (int i = 0; i < size; i++) {
        if (source[i] == quote) {
          source[i] = 0;
          break;
        }
      }
    }

    char wsl_path[] = "/mnt/";
    char* dest = drag_and_drop_buffer;

    // Convert C: to /mnt/c
    if (source[1] == ':') {
      source[1] = source[0] | 0x20;
      source++;
      strcpy(dest, wsl_path);
      dest += sizeof(wsl_path) - 1;
    }

    // Replace \ with / and escape spaces.
    while (*source) {
      if (*source == '\\') {
        *dest = '/';
      } else if (*source == ' ') {
        *dest++ = '\\';
        *dest   = ' ';
      } else {
        *dest = *source;
      }

      source++;
      dest++;
    }

    *source = 0;
    code = KEYCODE_DRAG_AND_DROP_PATH;
  }

  return code;
}
