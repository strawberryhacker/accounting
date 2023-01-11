#ifndef HISTORY_H
#define HISTORY_H

#include <stdbool.h>

void load_history_from_file();
void history_up();
void history_down();
void history_enter();
void history_other();
bool history_active();

#endif
