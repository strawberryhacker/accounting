#ifndef COMMAND_H
#define COMMAND_H

#include "command_line.h"

extern char* month_names[12];
extern char* day_names[7];

typedef struct {
  int type;

  int sort;
  bool sort_reverse;

  bool date_present;
  OptionsDate from;
  OptionsDate to;

  Filter* filter;

  bool filter_modified;
  bool unify;
  bool monthly;
  bool quarterly;
  bool yearly;
  bool is_short;
  bool running;
  bool sum;
  bool budget;
  bool print_ref;
  bool print_zeros;
  bool no_grid;
  bool percent;
  bool flat;
} Command;


void execute_command(Command* options);

#endif
