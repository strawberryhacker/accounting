#ifndef SUGGESTIONS_H
#define SUGGESTIONS_H

#include "stdarg.h"
#include <stdbool.h>

void suggestion_down();
void suggestion_up();
bool suggestion_active();
void set_suggestions_minimum_index(int index);
void set_suggestions_print_cursor(int cursor);

bool  add_suggestion(char* data, ...);
char* get_suggestion();
void  print_suggestions();
void  clear_suggestions();

void suggest_account(char* name, bool allow_cathegories);
void suggest_description(char* name);

#endif
