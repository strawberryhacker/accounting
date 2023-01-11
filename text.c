#include "text.h"
#include "stdlib.h"
#include "assert.h"
#include "stdio.h"
#include "basic.h"
#include <string.h>

char* string_save(char* source) {
  int size = strlen(source);
  char* dest = malloc(size + 1);
  strcpy(dest, source);
  return dest;
}

bool is_number(char c) {
  return '0' <= c && c <= '9';
}

bool is_letter(char c) {
  char lowercase = c | 0x20;
  return ('a' <= lowercase && lowercase <= 'z') || c == '_' || c == '.';
}

bool is_blank(char c) {
  return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

bool is_string_letter(char c) {
  return is_number(c) || is_letter(c) || c == '.' || c == '_';
}

void skip_blank(char** cursor) {
  char* data = *cursor;

  while (is_blank(*data)) {
    data++;
  }

  *cursor = data;
}

void skip_line(char** cursor) {
  char* data = *cursor;

  while (*data && *data != '\n' && *data != '\r') {
    data++;
  }

  *cursor = data;
}

bool skip_char(char** cursor, char c) {
  skip_blank(cursor);

  if (**cursor != c) return false;

  *cursor += 1;

  return true;
}

bool skip_string(char** cursor, char* data) {
  skip_blank(cursor);

  int size = strlen(data);
  if (strncmp(*cursor, data, size)) return false;

  *cursor += size;

  return true;
}

int get_number(char** cursor) {
  skip_blank(cursor);

  int n;
  int value;
  assert(sscanf(*cursor, "%d%n", &value, &n) == 1);
  
  *cursor += n;

  return value;
}

double get_double(char** cursor) {
  skip_blank(cursor);

  char* end;
  double value = strtof(*cursor, &end);
  assert(end != *cursor);

  *cursor = end;

  return value;
}

char* get_string(char** cursor) {
  skip_blank(cursor);

  char* data = *cursor;
  char* start = data;

  while (is_string_letter(*data)) data++;
  assert(data != start);

  if (*data) {
    *data++ = 0;
  }

  *cursor = data;

  return start;
}

char* get_quoted_string(char** cursor) {
  skip_blank(cursor);

  char* data = *cursor;

  if (*data++ != '\'') return 0;

  char* start = data;

  while (*data && *data != '\'') data++;
  assert(*data);

  *data++ = 0;
  *cursor = data;

  return start; 
}

char* get_string_size(char** cursor, int* size) {
  skip_blank(cursor);

  char* data = *cursor;
  char* start = data;

  while (is_string_letter(*data)) data++; // @Hack!
  *size = data - start;
  if (*size == 0) return 0;

  *cursor = data;
  return start;
}
