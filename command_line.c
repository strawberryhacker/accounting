#define _GNU_SOURCE

#include "command_line.h"
#include "input.h"
#include "terminal.h"
#include "suggestions.h"
#include "journal.h"
#include "add.h"
#include "command.h"
#include "history.h"
#include "date.h"
#include <string.h>
#include <assert.h>
#include <time.h>

static Filter* parse_filter(char** cursor);

static int state;
char* error_message;
Command options;

const char* binop[] = {
  "#",
  "=",
  "<",
  ">",
  "<=",
  ">=",
  "!=",
  "or",
  "and",
  "*",
  "/",
  "+",
  "-",
};

#define ARENA_SIZE (160 * sizeof(Filter) + 10000)
static char arena[ARENA_SIZE];
static int arena_index;

static void arena_clear() {
    arena_index = 0;
    memset(arena, 0, ARENA_SIZE);
}

static void* arena_allocate(int size) {
    void* pointer = &arena[arena_index];
    arena_index += size;
    return pointer;
}

static void* new_filter(int type) {
    Filter* filter = arena_allocate(sizeof(Filter));
    filter->type = type;
    return filter;
}

static char* get_string_arena(char** cursor) { // Hack because injecting 0 would corrupt the data.
  int size;
  char* data = get_string_size(cursor, &size);
  char* buffer = arena_allocate(size + 1);
  memcpy(buffer, data, size);
  buffer[size] = 0;
  return buffer;
}

static void* parse_filter_primary(char** cursor) {
  PrimaryFilter* primary = new_filter(FILTER_PRIMARY);
  bool fill_account_info = 0;

  if (skip_string(cursor, "from")) {
    primary->type = PRIMARY_FROM;
    fill_account_info = true;
  } else if (skip_string(cursor, "to")) {
    primary->type = PRIMARY_TO;
    fill_account_info = true;
  } else if (skip_string(cursor, "account")) {
    primary->type = PRIMARY_ACCOUNT;
    fill_account_info = true;
  } else if (skip_string(cursor, "amount")) {
    primary->type = PRIMARY_AMOUNT;
  } else if (skip_string(cursor, "desc")) {
    primary->type = PRIMARY_DESCRIPTION;
    primary->string = get_quoted_string(cursor);
  } else if (skip_string(cursor, "day")) {
      primary->type = PRIMARY_DAY;
  } else if (skip_string(cursor, "month")) {
    primary->type = PRIMARY_MONTH;
  } else if (skip_string(cursor, "year")) {
    primary->type = PRIMARY_YEAR;
  } else if (skip_string(cursor, "ref")) {
    primary->type = PRIMARY_REF_PRESENT;
  } else {
    skip_blank(cursor);

    if (is_letter(**cursor)) {
      char* text = get_string_arena(cursor);
      int day = get_day(text);
      if (day > 0) {
        primary->type = PRIMARY_DAY_NUMBER;
        primary->number = day;
      } else {
        int month = get_month(text);
        if (month == 0) {
          error_message = "expecting a day or month name";
          return 0;
        }
      }
    } else if (is_number(**cursor) || (**cursor == '-' && is_number(*(*cursor + 1)))) {
      primary->type = PRIMARY_NUMBER;
      primary->number = get_double(cursor);
    } else {
      error_message = "expecting a number";
      return 0;
    }
  }

  if (fill_account_info) {
    char* data = get_string_arena(cursor);    
    Account* node = get_account(data);

    if (!node) {
      error_message = "invalid account";
      return 0;
    }

    primary->string = data;
    primary->index  = node->index;
    primary->count  = node->count;
  }

  return primary;
}

static void* parse_filter_unary(char** cursor) {
  if (skip_string(cursor, "not")) {
    UnaryFilter* unary = new_filter(FILTER_UNARY);
    unary->type = UNARY_NOT;
    unary->filter = parse_filter_unary(cursor);
    if (unary->filter == 0) return 0;
    return unary;
  } else if (skip_char(cursor, '(')) {
    Filter* filter = parse_filter(cursor);
    if (!skip_char(cursor, ')')) {
      error_message = "expecting closing parenthesis";
      return 0;
    }
    if (filter) filter->parenthesized = true;
    return filter;
  } else {
    return parse_filter_primary(cursor);
  }
}

static int get_binary_type(char** cursor) {
       if (skip_string(cursor, "and")) return BINARY_AND;
  else if (skip_string(cursor, "or" )) return BINARY_OR;
  else if (skip_string(cursor, "!=" )) return BINARY_NOT_EQUAL;
  else if (skip_string(cursor, ">=" )) return BINARY_GREATER_EQUAL;
  else if (skip_string(cursor, "<=" )) return BINARY_LESS_EQUAL;
  else if (skip_string(cursor, "="  )) return BINARY_EQUAL;
  else if (skip_string(cursor, "<"  )) return BINARY_LESS_THAN;
  else if (skip_string(cursor, ">"  )) return BINARY_GREATER_THAN;
  else if (skip_string(cursor, "*"  )) return BINARY_MULTIPLY;
  else if (skip_string(cursor, "/"  )) return BINARY_DIVIDE;
  else if (skip_string(cursor, "+"  )) return BINARY_PLUS;
  else if (skip_string(cursor, "- "  )) return BINARY_MINUS; // Hack for multiple filters.
  else                                 return BINARY_NONE;
}

static int get_precedence(int type) {
  switch (type) {
    case BINARY_MULTIPLY:
    case BINARY_DIVIDE:
      return 6;
    case BINARY_PLUS:
    case BINARY_MINUS:
      return 5;
    case BINARY_EQUAL:
    case BINARY_NOT_EQUAL:
      return 4;
    case BINARY_LESS_THAN:
    case BINARY_LESS_EQUAL:
    case BINARY_GREATER_THAN:
    case BINARY_GREATER_EQUAL:
      return 3;
    case BINARY_AND:
      return 2;
    case BINARY_OR:
      return 1;
    default:
      return 0;
  }
} 

static bool filter_is_number(Filter* filter) {
  return (filter->type == FILTER_PRIMARY) && (filter->primary.type == PRIMARY_NUMBER);
}

static bool filter_is_day_number(Filter* filter) {
  return (filter->type == FILTER_PRIMARY) && (filter->primary.type == PRIMARY_DAY_NUMBER);
}

static bool filter_is_day(Filter* filter) {
  return (filter->type == FILTER_PRIMARY) && (filter->primary.type == PRIMARY_DAY);
}

static bool filter_is_ref_present(Filter* filter) {
  return (filter->type == FILTER_PRIMARY) && (filter->primary.type == PRIMARY_REF_PRESENT);
}

static Filter* parse_filter_recursive(char** cursor, int previous_precedence) {
  Filter* left = parse_filter_unary(cursor);
  if (!left) return 0;

  while (true) {
    char* saved_cursor = *cursor;

    int type = get_binary_type(cursor);
    int precedence = get_precedence(type);

    if (precedence <= previous_precedence || precedence == 0) {
      *cursor = saved_cursor;
      return left;
    }

    Filter* right = parse_filter_recursive(cursor, precedence);
    if (!right) return 0;

    // Determine if we can evaluate a constant expression.
    bool left_is_number      = filter_is_number(left);
    bool left_is_day_number  = filter_is_day_number(left);
    bool left_is_const       = left_is_number  || left_is_day_number; 

    bool right_is_number     = filter_is_number(right);
    bool right_is_day_number = filter_is_day_number(right);
    bool right_is_const      = right_is_number || right_is_day_number; 

    if (filter_is_day(left) && right_is_day_number) {
      left->primary.type = PRIMARY_WEEKDAY;
    }

    if (filter_is_ref_present(left) && right_is_const) {
      left->primary.type = PRIMARY_REF_SEARCH;
    }

    if (left_is_const && right_is_const) {
      double x = left->primary.number;
      double y = right->primary.number;
      double r;

      switch (type) {
        case BINARY_EQUAL:         r = x == y; break;
        case BINARY_LESS_THAN:     r = x <  y; break;
        case BINARY_GREATER_THAN:  r = x >  y; break;
        case BINARY_LESS_EQUAL:    r = x <= y; break;
        case BINARY_GREATER_EQUAL: r = x >= y; break;
        case BINARY_NOT_EQUAL:     r = x != y; break;
        case BINARY_OR:            r = x || y; break;
        case BINARY_AND:           r = x && y; break;
        case BINARY_MULTIPLY:      r = x *  y; break;
        case BINARY_DIVIDE:        r = x /  y; break;
        case BINARY_PLUS:          r = x +  y; break;
        case BINARY_MINUS:         r = x -  y; break;
        default: assert(0);
      }

      options.filter_modified = true;
      
      left->type = FILTER_PRIMARY;
      left->primary.number = r;

      if (left_is_day_number || right_is_day_number) {
        left->primary.type = PRIMARY_DAY_NUMBER;
      } else {
        left->primary.type = PRIMARY_NUMBER;
      }
    } else {
      BinaryFilter* binary = new_filter(FILTER_BINARY);

      binary->type  = type;
      binary->left  = left;
      binary->right = right;

      left = (Filter*)binary;
    }
  }
}

static Filter* parse_filter(char** cursor) {
  skip_blank(cursor);
  if (**cursor == 0) return 0;
  return parse_filter_recursive(cursor, -1);
}

// Prints the filter the same way as the user wrote it, with all constant expressions evaluated.
void print_filter(Filter* filter) {
  if (!filter) return;

  if (filter->type == FILTER_PRIMARY) {
    switch (filter->primary.type) {
      case PRIMARY_FROM:
        print("from %s ", filter->primary.string);
        break;
      case PRIMARY_TO:
        print("to %s ", filter->primary.string);
        break;
      case PRIMARY_ACCOUNT:
        print("account %s ", filter->primary.string);
        break;
      case PRIMARY_DESCRIPTION:
        print("desc '%s' ", filter->primary.string);
        break;
      case PRIMARY_AMOUNT:
        print("amount ");
        break;
      case PRIMARY_WEEKDAY:
        print("weekday ");
        break;
      case PRIMARY_DAY:
        print("day ");
        break;
      case PRIMARY_MONTH:
        print("month ");
        break;
      case PRIMARY_YEAR:
        print("year ");
        break;
      case PRIMARY_NUMBER:
        print("%.2lf ", filter->primary.number);
        break;
      case PRIMARY_DAY_NUMBER: {
        int n = (int)filter->primary.number;
        if (1 <= n && n <= 7) {
          print("%s ", day_names[n - 1]);
        } else {
          print("error (%d) ", n);
        }
      } break;
    }
  } else if (filter->type == FILTER_UNARY) {
    print("not ", filter->unary.type);
    print_filter(filter->unary.filter);
  } else if (filter->type == FILTER_BINARY) {
    if (filter->parenthesized) print("(");
    print_filter(filter->binary.left);
    print("%s ", binop[filter->binary.type]);
    print_filter(filter->binary.right);
    if (filter->parenthesized) print(") ");
  }
}

double apply_filter(Filter* filter, Transaction* transaction) {
  switch (filter->type) {
    case FILTER_BINARY: {
      Filter* left  = filter->binary.left;
      Filter* right = filter->binary.right;

      double x = apply_filter(left,  transaction);
      double y = apply_filter(right, transaction);
      double r;

      switch (filter->binary.type) {
        case BINARY_EQUAL:         r = x == y; break;
        case BINARY_LESS_THAN:     r = x <  y; break;
        case BINARY_GREATER_THAN:  r = x >  y; break;
        case BINARY_LESS_EQUAL:    r = x <= y; break;
        case BINARY_GREATER_EQUAL: r = x >= y; break;
        case BINARY_NOT_EQUAL:     r = x != y; break;
        case BINARY_OR:            r = x || y; break;
        case BINARY_AND:           r = x && y; break;
        case BINARY_MULTIPLY:      r = x *  y; break;
        case BINARY_DIVIDE:        r = x /  y; break;
        case BINARY_PLUS:          r = x +  y; break;
        case BINARY_MINUS:         r = x -  y; break;
        default: assert(0);
      }

      return r;
    }

    case FILTER_PRIMARY: {
      int type  = filter->primary.type;
      int start = filter->primary.index;
      int end   = filter->primary.count + start;

      if (!transaction && type != PRIMARY_NUMBER) return 0;

      switch (type) {
        case PRIMARY_FROM:
          return (double)(start <= transaction->from && transaction->from < end);
        case PRIMARY_TO:
          return (double)(start <= transaction->to && transaction->to < end);
        case PRIMARY_ACCOUNT:
          return (double)((start <= transaction->from && transaction->from < end) ||
                          (start <= transaction->to && transaction->to < end));
        case PRIMARY_DESCRIPTION:
          return (double)(transaction->description && strcasestr(transaction->description, filter->primary.string) != 0);
        case PRIMARY_WEEKDAY:
          return date_to_weekday(transaction->date.day, transaction->date.month, transaction->date.year);
        case PRIMARY_AMOUNT:
          return transaction->amount;
        case PRIMARY_DAY:
          return transaction->date.day;
        case PRIMARY_MONTH:
          return transaction->date.month;
        case PRIMARY_YEAR:
          return transaction->date.year;
        case PRIMARY_NUMBER:
        case PRIMARY_DAY_NUMBER:
          return filter->primary.number;
        case PRIMARY_REF_PRESENT:
          return transaction->reference >= 0;
        case PRIMARY_REF_SEARCH:
          return transaction->reference;
        default:
          assert(0);
      }

      return 0;
    }

    case FILTER_UNARY:
      assert(filter->unary.type == UNARY_NOT);
      return (double)!(bool)apply_filter(filter->unary.filter, transaction);
  }

  assert(0);
  return 0;
}

// Probably stupid to have a separate evaluator just to hide some accounts in the balance view...
int apply_filter_account(Filter* filter, Account* node) {
  switch (filter->type) {
    case FILTER_BINARY: {
      Filter* left  = filter->binary.left;
      Filter* right = filter->binary.right;

      int x = apply_filter_account(left,  node);
      int y = apply_filter_account(right, node);
      int r;

      if (x < 0) return y;
      if (y < 0) return x;

      switch (filter->binary.type) {
        case BINARY_EQUAL:         r = x == y; break;
        case BINARY_LESS_THAN:     r = x <  y; break;
        case BINARY_GREATER_THAN:  r = x >  y; break;
        case BINARY_LESS_EQUAL:    r = x <= y; break;
        case BINARY_GREATER_EQUAL: r = x >= y; break;
        case BINARY_NOT_EQUAL:     r = x != y; break;
        case BINARY_OR:            r = x || y; break;
        case BINARY_AND:           r = x && y; break;
        case BINARY_MULTIPLY:      r = x *  y; break;
        case BINARY_DIVIDE:        r = x /  y; break;
        case BINARY_PLUS:          r = x +  y; break;
        case BINARY_MINUS:         r = x -  y; break;
        default: assert(0);
      }

      return r;
    }

    case FILTER_PRIMARY: {
      int type  = filter->primary.type;
      int start = filter->primary.index;
      int end   = filter->primary.count + start;

      switch (type) {
        case PRIMARY_FROM:
        case PRIMARY_TO:
        case PRIMARY_ACCOUNT:
          return (start <= node->index) && (node->index < end);
        case PRIMARY_AMOUNT:
        case PRIMARY_WEEKDAY:
        case PRIMARY_DAY:
        case PRIMARY_DESCRIPTION:
        case PRIMARY_MONTH:
        case PRIMARY_YEAR:
        case PRIMARY_REF_SEARCH:
        case PRIMARY_REF_PRESENT:
        case PRIMARY_NUMBER:
        case PRIMARY_DAY_NUMBER:
          return -1;
        default:
          assert(0);
      }

      return 0;
    }

    case FILTER_UNARY:
      assert(filter->unary.type == UNARY_NOT);
      int status = apply_filter_account(filter->unary.filter, node);
      return (status == -1) ? -1 : !status;
  }

  assert(0);
  return 0;
}

static bool find_indices(char* data, char* match, int cursor, int* index, int* size) {
  char* data_start = data;
  char* cursor_pointer = data + cursor;

  while (true) {
    data = strstr(data, match);
    if (!data) return false;
    data += strlen(match);

    char* start = data;

    if (is_letter(*data)) {
      while (*data && !is_blank(*data) && *data != ')') { // @Hack.
        data++;
      }
    }

    char* end = data;

    if (start <= cursor_pointer && cursor_pointer <= end) {
      *index = start - data_start;
      *size  = end - start;
      return true;
    }
  }
}

static bool try_parse_date(char** cursor, OptionsDate* date) {
  char* data = *cursor;
  skip_blank(&data);

  if (*data == '*') {
    data++;
    date->wild = true;
  } else {
    int* numbers = &date->day;
    for (int i = 0; i < 3; i++) {
      if (i && *data != '.') break;
      if (*data == '.') data++;

      int n;
      if (sscanf(data, "%d%n", &numbers[i], &n) == 1) {
        date->count++;
        data += n;
        continue;
      }

      char* start = data;
      while (*data && (*data != '.') && (*data != ' ')) data++;

      int size = data - start;
      if (size != 3) break;

      int month = get_month(start);
      if (month == 0) break;

      numbers[i] = month;
      date->count++;
    }
  }

  *cursor = data;
  return date->count || date->wild;
}

static bool parse_date_option(char** cursor) {
  if (!try_parse_date(cursor, &options.from)) return false;

  char* saved_cursor = *cursor;
  if (!try_parse_date(cursor, &options.to)) {
    *cursor = saved_cursor;
  }

  options.date_present = true;
  clean_up_dates(&options.from, &options.to);
  return true;
}

static bool parse_sort_option(char** data) {
  if (skip_char(data, '!')) options.sort_reverse = true;

         if (skip_string(data, "date")) {
    options.sort = SORT_DATE;
  } else if (skip_string(data, "from")) {
    options.sort = SORT_FROM;
  } else if (skip_string(data, "to")) {
    options.sort = SORT_TO;
  } else if (skip_string(data, "amount")) {
    options.sort = SORT_AMOUNT;
  } else {
    error_message = "unknown sort option";
    return false;
  }

  return true;
}

static bool skip_option(char** data, char* option, char* short_option) {
  return skip_string(data, option) || skip_string(data, short_option);
}

static bool parse_options(char* data) {
  while (true) {
    skip_blank(&data);
    debug("data: %s\n", data);

    if (*data == 0) return true;

           if (skip_option(&data, "-monthly "  , "-m ")) {
      options.monthly = true;
    } else if (skip_option(&data, "-running "  , "-r ")) {
      options.running = true;
    } else if (skip_option(&data, "-nogrid "   , "-g ")) {
      options.no_grid = true;
    } else if (skip_option(&data, "-percent "   , "-p ")) {
      options.percent = true;
    } else if (skip_option(&data, "-sum "      , "-e ")) {
      options.sum = true;
    } else if (skip_option(&data, "-budget "   , "-b ")) {
      options.budget = true;
    } else if (skip_option(&data, "-refs "     , "-l ")) {
      options.print_ref = true;
    } else if (skip_option(&data, "-flat "     , "-c ")) {
      options.flat = true;
    } else if (skip_option(&data, "-zero "     , "-z ")) {
      options.print_zeros = true;
    } else if (skip_option(&data, "-date "     , "-d ")) {
      if (!parse_date_option(&data)) return false;
    } else if (skip_option(&data, "-sort "     , "-s ")) {
      if (!parse_sort_option(&data)) return false;
    } else if (skip_option(&data, "-unify "    , "-u ")) {
      options.unify = true;
    } else if (skip_option(&data, "-quarterly ", "-q ")) {
      options.quarterly = true;
    } else if (skip_option(&data, "-yearly "   , "-y")) {
      options.yearly = true;
    } else if (skip_option(&data, "-short "    , "-t")) {
      options.is_short = true;
    } else if (skip_option(&data, "-filter "   , "-f ")) {
      options.filter = parse_filter(&data);
      if (!options.filter) return false;
      apply_filter(options.filter, 0);
    } else {
      error_message = "unknown option";
      return false;
    }
  }
}

static void print_date(OptionsDate* date, char* name) {
  debug("%s: ", name);
  if (date->wild) {
    debug("*\n");
  } else {
    for (int i = 0; i < date->count; i++) {
      if (i) debug(".");
      debug("%d", ((int*)&date->day)[i]);
    }
    debug("\n");
  }
}

static bool parse_command_line() {
  memset(&options, 0, sizeof(options));
  char buffer[INPUT_SIZE];
  strcpy(buffer, input.data);

  // Append a space to match -option(space).
  buffer[input.size    ] = ' ';
  buffer[input.size + 1] = 0;

  char* data = buffer;

  skip_blank(&data);

  int command;
  if (skip_string(&data, "add")) {
    print("\n");
    add_transaction_init();
    state = STATE_ADD;
    return true;
  } else if (skip_string(&data, "print")) {
    options.type = COMMAND_PRINT;
  } else if (skip_string(&data, "clear")) {
    options.type = COMMAND_CLEAR;
  } else if (skip_string(&data, "balance")) {
    options.type = COMMAND_BALANCE;
  } else {
    error_message = "unknown command";
    return false;
  }

  if (!parse_options(data)) return false;

  debug("Options: \n");
  debug("Sort: %d\n", options.sort);
  
  print_date(&options.from, "From");
  print_date(&options.to, "To  ");

  return true;

}

void command_line_handle(int keycode) {
  int screen_width, screen_height;
  get_size(&screen_width, &screen_height);

  input_handle(keycode);


  if (keycode != KEYCODE_LEFT && keycode != KEYCODE_RIGHT) {
    if (keycode == KEYCODE_DOWN) {
      if (suggestion_active()) {
        suggestion_down();
      } else if (state == STATE_COMMAND) {
        history_down();
      }
    } else if (keycode == KEYCODE_UP) {
      if (suggestion_active()) {
        suggestion_up();
      } else if (state == STATE_COMMAND) {
        history_up();
      }
    } else {
      clear_suggestions();
      if (state == STATE_ADD) {
        if (add_transaction_update(keycode)) {
          print("\n");
          state = STATE_COMMAND;
          journal_parse();
          input_clear();
        }
      } else {
        input_update_width(screen_width - 5, 10);
        int match_index, match_size;

        char* filter_start = strstr(input.data, "-f");
        bool match_active = false;

        if (filter_start) {
          match_active = find_indices(input.data, "from ",    input.cursor, &match_index, &match_size)
                      || find_indices(input.data, "to ",      input.cursor, &match_index, &match_size)
                      || find_indices(input.data, "account ", input.cursor, &match_index, &match_size);
        }

        if (keycode == KEYCODE_ENTER) {
          char* suggestion = get_suggestion();
          if (input.size || suggestion) {
            history_enter();
            if (suggestion) {
              input_replace(suggestion, match_size, match_index);
            } else {
              if (!parse_command_line()) {
                print("\r\n   \033[31mError:\033[0m %s\n\r\n", error_message);
              } else {
                if (state == STATE_COMMAND) {
                  print("\r\n");
                  execute_command(&options);
                  print("\r\n");
                }
              }
              input_clear();
            }
          } else {
            print("\r\n");
          }
        } else {
          if (match_active) {
            char tmp[512];
            memcpy(tmp, input.data + match_index, match_size);
            tmp[match_size] = 0;
            suggest_account(tmp, true);
            set_suggestions_minimum_index(0);
            set_suggestions_print_cursor(match_index - input.offset + 3);
          }
        }
      }
    }
  }

  clear_line();
  set_x_cursor(0);
  print(" ");
  int cursor;

  if (state == STATE_ADD) {
    cursor = add_transaction_render(1);
  } else {
    print_input();
    cursor = 1;
  }

  print_suggestions();
  set_x_cursor(cursor + get_input_cursor());
  flush();
}
