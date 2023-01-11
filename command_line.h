#ifndef COMMAND_LINE_H
#define COMMAND_LINE_H

#include <stdbool.h>
#include "journal.h"
#include "date.h"

typedef struct Filter Filter;

enum {
  PRIMARY_FROM,
  PRIMARY_TO,
  PRIMARY_ACCOUNT,
  PRIMARY_DESCRIPTION,
  PRIMARY_AMOUNT,
  PRIMARY_NUMBER,
  PRIMARY_DAY,
  PRIMARY_WEEKDAY, // @Constructed and not parsed.
  PRIMARY_MONTH,
  PRIMARY_YEAR,
  PRIMARY_REF_PRESENT,
  PRIMARY_REF_SEARCH,
  PRIMARY_DAY_NUMBER,
};

enum {
  UNARY_NOT,
};

enum {
  BINARY_NONE,
  BINARY_EQUAL,
  BINARY_LESS_THAN,
  BINARY_GREATER_THAN,
  BINARY_LESS_EQUAL,
  BINARY_GREATER_EQUAL,
  BINARY_NOT_EQUAL,
  BINARY_OR,
  BINARY_AND,
  BINARY_MULTIPLY,
  BINARY_DIVIDE,
  BINARY_PLUS,
  BINARY_MINUS,
};

enum {
  FILTER_PRIMARY,
  FILTER_UNARY,
  FILTER_BINARY,
};

enum {
  COMMAND_PRINT,
  COMMAND_ADD,
  COMMAND_BALANCE,
  COMMAND_CLEAR,
};

enum {
  STATE_COMMAND,
  STATE_ADD,
};

enum {
  SORT_DATE,
  SORT_AMOUNT,
  SORT_FROM,
  SORT_TO,
};

typedef struct {
  int type;
  double number;
  char* string;
  int index;
  int count;
} PrimaryFilter;

typedef struct {
  int type;
  Filter* filter;
} UnaryFilter;

typedef struct {
  int type;
  Filter* left;
  Filter* right;
} BinaryFilter;

struct Filter {
  union {
    PrimaryFilter primary;
    UnaryFilter   unary;
    BinaryFilter  binary;
  };

  int type;
  bool parenthesized;
};

void command_line_handle(int keycode);
double apply_filter(Filter* filter, Transaction* transaction);
int apply_filter_account(Filter* filter, Account* node);
void print_filter(Filter* filter);

#endif
