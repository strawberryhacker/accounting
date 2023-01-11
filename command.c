#include "command.h"
#include "basic.h"
#include "terminal.h"
#include "journal.h"
#include "date.h"
#include <string.h>
#include <assert.h>


static Transaction* transactions[MAX_TRANSACTIONS];
static Transaction* sorted_transactions[MAX_TRANSACTIONS];
static int transaction_count;
static int max_number_width;

double running_sum[MAX_ACCOUNTS];

typedef struct {
  Date date;
  double sum[MAX_ACCOUNTS];
} Period;

#define MAX_PERIODS 24

static Period periods[MAX_PERIODS];
static int period_count;

#define COPY_SUM(source, dest) memcpy(dest, source, journal.account_count * sizeof(double))
#define CLEAR_SUM(sum) memset(sum, 0, journal.account_count * sizeof(double))

#define LEFT_INDENTATION 3

static void start_line() {
  set_x_cursor(LEFT_INDENTATION);
}

static void set_this_cursor(int x) {
  set_x_cursor(LEFT_INDENTATION + x);
}

static Transaction* sort_date_get_first(Transaction* a, Transaction* b) {
  return date_is_smaller(&a->date, &b->date) ? a : b;
}

static Transaction* sort_from_get_first(Transaction* a, Transaction* b) {
  return (a->from < b->from) ? a : b;
}

static Transaction* sort_to_get_first(Transaction* a, Transaction* b) {
  return (a->to < b->to) ? a : b;
}

static Transaction* sort_amount_get_first(Transaction* a, Transaction* b) {
  return (a->amount < b->amount) ? a : b;
}

static int get_max_account_path_length(int indentation) {
  int max = 0;
  for (int i = 1; i < journal.account_count; i++) {
    Account* account = &journal.accounts[i];
    int current = indentation * (account->level - 1) + account->path_length;
    if (current > max) max = current;
  }

  return max;
}

static int get_max_account_name_length(int indentation) {
  int max = 0;
  for (int i = 1; i < journal.account_count; i++) {
    Account* account = &journal.accounts[i];
    int current = indentation * (account->level - 1) + account->name_length;
    if (current > max) max = current;
  }

  return max;
}

static int month_to_quarter(int month) {
  return 1 + (month - 1) / 4;
}

static bool is_new_period(Command* options, Transaction* prev, Transaction* current) {
  if (prev == 0)
    return false;

  if (options->monthly && prev->date.month != current->date.month) 
    return true;

  if (options->quarterly && month_to_quarter(prev->date.month) != month_to_quarter(current->date.month)) 
    return true;

  if (options->yearly && prev->date.year != current->date.year) 
    return true;

  return false;
}

static double compute_category_sums(Account* account, double* output_data) {
  double sum = 0;

  if (!account->is_category) return output_data[account->index];

  Account* child = account->childs;
  while (child) {
    sum += compute_category_sums(child, output_data);
    child = child->next;
  }

  output_data[account->index] = sum;
  return sum;
}

static void save_period_info(Transaction* transaction_in_period) {
  Period* period = &periods[period_count++];
  compute_category_sums(journal.root_account, running_sum);
  COPY_SUM(running_sum, period->sum);
  period->date = transaction_in_period->date;
}

static void clear_category_sums(double* sums) {
  for (int i = 0; i < journal.account_count; i++) {
    Account* account = &journal.accounts[i];
    if (account->is_category) sums[i] = 0;
  }
}

static void get_periods(Command* command) {
  if (transaction_count == 0) return;
  period_count = 0;

  if (!command->running) CLEAR_SUM(running_sum);

  Transaction* prev = 0;
  for (int i = 0; i < transaction_count; i++) {
    Transaction* current = transactions[i];

    if (is_new_period(command, prev, current)) {
      save_period_info(prev);
      
      if (command->running) {
        clear_category_sums(running_sum);
      } else {
        CLEAR_SUM(running_sum);
      }

      if (period_count == MAX_PERIODS) return;
    }

    running_sum[current->to]   += current->amount;
    running_sum[current->from] -= current->amount;
    prev = current;
  }

  save_period_info(prev);
}

void print_balance_splitter(int account_width, int number_width, int column_count, bool ignore_first_column) {
  start_line();
  if (ignore_first_column) {
    set_this_cursor(account_width);
  } else {
    while (account_width--) print("-");
  }

  while (column_count--) {
    print("+");
    for (int i = 0; i < number_width; i++) print("-");
  }

  print("\n");
}

void print_number_in_field(bool print_zero, double number, int width, bool positive_color) {
  if (print_zero || number) {
    width -= get_digit_count(number) + 3; // .00
    assert(width >= 0);
  }

  while (width--) print(" ");

  if (print_zero || number) {
    if (number < 0) print("\033[31m");
    if (number > 0 && positive_color) print("\033[34m"); 
    print("%.2lf", number);
    format_off();
  }
}

void print_balance(Command* command) {
  get_periods(command);
  if (transaction_count == 0) return;
  if (!command->flat) command->is_short = true;
  if (!command->monthly) command->budget = false;
  if (command->running) command->budget = false;
  print("\n");

  int width, height;
  get_size(&width, &height);

  int indentation = (command->flat) ? 0 : 4;
  int name_width = command->is_short ? get_max_account_name_length(indentation) : get_max_account_path_length(indentation);
  int name_field_width = name_width + 1;
  int number_width = 0;
  int number_field_width;
  int column_count;

  // Compute the maximum number width.
  for (int i = 0; i < period_count; i++) {
    for (int j = 0; j < journal.account_count; j++) {
      int width = get_digit_count(periods[i].sum[j]);
      if (width > number_width) {
        number_width = width;
      }
    }
  }

  int date_width;
  if (command->monthly) {
    date_width = sizeof("jan.2000") - 1;
  } else if (command->quarterly) {
    date_width = sizeof("q1.2000") - 1;
  } else if (command->yearly) {
    date_width = sizeof("2000") - 1;
  } else {
    date_width = sizeof("01.jan.2000") - 1;
  }

  number_width = max(number_width + 3, date_width);
  number_field_width = number_width + 1;
  column_count = min(period_count, (width - LEFT_INDENTATION - name_field_width) / number_field_width);

  // Print dates.
  start_line();
  set_this_cursor(name_field_width);
  debug("Running status: %s\n", command->running ? "running" : "not");

  for (int i = 0; i < column_count; i++) {
    int padding = diff(number_width, date_width);
    assert(padding >= 0);
    while (padding--) print(" ");

    Date* date = &periods[i].date;
    if (command->monthly) {
      print("%s.", month_names[date->month - 1]);
    } else if (command->quarterly) {
      print("q%d.", month_to_quarter(date->month));
    } else if (!command->yearly) {
      print("%02d.%s.", date->day, month_names[date->month - 1]);
    }

    print("%04d ", date->year);
  }

  print("\n");

  bool print_enable[journal.account_count];
  memset(print_enable, 0, sizeof(print_enable));
  for (int i = 0; i < journal.account_count; i++) {
    Account* account = &journal.accounts[i];
    if (!account->is_category && command->filter && apply_filter_account(command->filter, account) == 1) {
      // Mark all parents to be printed.
      while (account) {
        print_enable[account->index] = true;
        debug("Marking %d  to print\n", account->index);
        account = account->parent;
      }
    }
  }

  // Print account name and balance columns.
  for (int i = 1; i < journal.account_count; i++) {
    Account* account = &journal.accounts[i];
    if (command->filter && print_enable[i] == false) continue;
    if (account->is_category) {
      if (account->parent == journal.root_account) {
        // Split only the big categories.
        print_balance_splitter(name_width, number_width, column_count, false);
      }
      if (command->flat) continue;
    }

    start_line();

    // Print account name.
    int left_padding  = indentation * (account->level - 1);
    int right_padding = name_width - left_padding - (command->is_short ? account->name_length : account->path_length);
    
    assert(left_padding  >= 0);
    assert(right_padding >= 0);

    while (left_padding--) print(" ");
    print("%s", command->is_short ? account->name : account->path);
    while (right_padding--) print(" ");

    // Print balances.
    for (int j = 0; j < column_count; j++) {
      print("|");
      double tmp = periods[j].sum[i];
      if (command->budget) {
        tmp = (account->monthly_budget + account->yearly_budget / 12) - tmp;
      }
      print_number_in_field(command->print_zeros, tmp, number_width, command->budget && (account->monthly_budget != 0 || account->yearly_budget != 0));
    }

    print("\n");
  }
}

static void print_transaction_splitter(Command* command, int name_width, int number_width) {
  start_line();
  print("------------+-");
  for (int i = 0; i < name_width; i++) print("-");
  print("-+-");
  for (int i = 0; i < name_width; i++) print("-");
  print("-+-------------+");
  if (command->running) print("-------------+");
  if (command->print_ref) print("-----+");
  print("-------------");
  print("\n");
}

void print_transaction(Command* command, Transaction* t, bool swap) {
  start_line();
  print("%02d.%s.%4d | ", t->date.day, month_names[t->date.month - 1], t->date.year);

  int name_width = command->is_short ? get_max_account_name_length(0) : get_max_account_path_length(0);
  int padding;

  int from = swap ? t->to   : t->from;
  int to   = swap ? t->from : t->to;
  double amount = swap ? t->amount : -t->amount;

  print("%s", command->is_short ? journal.accounts[from].name : journal.accounts[from].path);
  padding = name_width - (command->is_short ? journal.accounts[from].name_length : journal.accounts[from].path_length);
  assert(padding >= 0);
  while (padding--) print(" ");
  print(" | ");

  print("%s", command->is_short ? journal.accounts[to].name : journal.accounts[to].path);
  padding = name_width - (command->is_short ? journal.accounts[to].name_length : journal.accounts[to].path_length);
  assert(padding >= 0);
  while (padding--) print(" ");
  print(" | ");


  print_number_in_field(command->print_zeros, amount, 11, false);
  print(" | ");

  if (command->running) {
    print_number_in_field(command->print_zeros, running_sum[from], 11, false);
    print(" | ");
  }

  if (command->print_ref) {
    if (t->reference >= 0) {
      print("%3d | ", t->reference);
    } else {
      print("    | ");
    }
  }

  if (t->description) {
    print("%s", t->description);
  }

  print("\n");
}


static void print_transactions(Command* command) {
  if (!command->unify) command->running = false; // Connot have a running total without unify since two accounts are involved.

  int name_width = command->is_short ? get_max_account_name_length(0) : get_max_account_path_length(0);

  Transaction* prev = 0;
  for (int i = 0; i < transaction_count; i++) {
    Transaction* current = transactions[i];

    if (is_new_period(command, prev, current)) {
      print_transaction_splitter(command, name_width, max_number_width + 1);
    }

    int tmp;
    bool status;

    running_sum[current->from] -= current->amount;
    running_sum[current->to]   += current->amount;

    if (command->unify) {
      tmp = current->to;
      current->to = -1;
      status = command->filter == 0 || apply_filter(command->filter, current);
      current->to = tmp;
      if (status) print_transaction(command, transactions[i], false);

      tmp = current->from;
      current->from = -1;
      status = command->filter == 0 || apply_filter(command->filter, current);
      current->from = tmp;
      if (status) print_transaction(command, transactions[i], true);

    } else {
      start_line();
      print_transaction(command, transactions[i], false);
    }

    prev = current;
  }
  flush();
}

void execute_command(Command* command) {
  if (command->type == COMMAND_CLEAR) {
    clear_all();
    set_cursor(0, 0);
    return;
  }

  // If the filter is changed, print the new one.
  if (command->filter && command->filter_modified) {
    start_line();
    print("Using filter: ");
    print_filter(command->filter);
    print("\n");
    flush();
  }

  // Sort by date.
  for (int i = 0; i < journal.raw_transaction_count; i++) sorted_transactions[i] = &journal.raw_transactions[i];
  journal_sort_transactions(sorted_transactions, journal.raw_transaction_count, sort_date_get_first, false);

  CLEAR_SUM(running_sum);

  // Apply the filter to get the source transactions while computing the running sum to the first transaction.
  transaction_count = 0;
  max_number_width = 0;
  Transaction* prev = 0;

  for (int i = 0; i < journal.raw_transaction_count; i++) {
    Transaction* t = sorted_transactions[i];

    if (prev && !transaction_count) {
      running_sum[t->to]   += t->amount;
      running_sum[t->from] -= t->amount;
    }

    if (command->date_present && (date_is_smaller(&t->date, &command->from.date) || date_is_bigger(&t->date, &command->to.date))) continue;
    if (command->type != COMMAND_BALANCE && command->filter && !apply_filter(command->filter, t)) continue; // Disable transaction filter when command is balance.

    transactions[transaction_count++] = t;
    int width = get_digit_count(t->amount);
    if (width > max_number_width) {
      max_number_width = width;
    }
    prev = t;
  }

  max_number_width += 1;

  if (command->type == COMMAND_BALANCE) {
    print_balance(command);
    return;
  }

  if (command->sort == SORT_DATE) {
    journal_sort_transactions(transactions, transaction_count, sort_date_get_first, command->sort_reverse);
  } else if (command->sort == SORT_FROM) {
    journal_sort_transactions(transactions, transaction_count, sort_from_get_first, command->sort_reverse);
  } else if (command->sort == SORT_TO) {
    journal_sort_transactions(transactions, transaction_count, sort_to_get_first, command->sort_reverse);
  } else if (command->sort == SORT_AMOUNT) {
    journal_sort_transactions(transactions, transaction_count, sort_amount_get_first, command->sort_reverse);
  }

  if (command->type == COMMAND_PRINT) {
    print_transactions(command);
  }
}
