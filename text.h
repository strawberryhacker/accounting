#ifndef TEXT_H
#define TEXT_H

#include "stdbool.h"

char* string_save(char* data);
bool is_number(char c);
bool is_letter(char c);
bool is_blank(char c);
void skip_blank(char** cursor);
void skip_line(char** cursor);
bool skip_char(char** cursor, char c);
bool skip_string(char** cursor, char* string);
int get_number(char** cursor);
double get_double(char** cursor);
char* get_string(char** cursor);
char* get_string_size(char** cursor, int* size);
char* get_quoted_string(char** cursor);

#endif
