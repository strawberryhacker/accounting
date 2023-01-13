#include "command.h"
#include "basic.h"
#include "terminal.h"
#include "journal.h"
#include "date.h"
#include <string.h>
#include <assert.h>

#define MAX_PERIODS        24
#define LEFT_INDENTATION   3
#define INTEGRAL_WIDTH     8
#define NUMBER_WIDTH       (INTEGRAL_WIDTH + 3)
#define REF_WIDTH          3

typedef struct {
  Date date;
  double sum[MAX_ACCOUNTS];
} Period;

static Transaction* transactions[MAX_TRANSACTIONS];
static int transaction_count;

static Period periods[MAX_PERIODS];
static int period_count;

static double running_sums[MAX_ACCOUNTS];
static double initial_sums[MAX_ACCOUNTS];

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

static double compute_category_sums(Account* account, double* data) {
  double sum = 0;

  if (!account->is_category) return data[account->index];

  Account* child = account->childs;
  while (child) {
    sum += compute_category_sums(child, data);
    child = child->next;
  }

  data[account->index] = sum;
  return sum;
}

static void compute_budget_sum(Account* account, double* y, double* m) {
  double y_sum = 0;
  double m_sum = 0;

  if (!account->is_category) {
    *y = account->yearly_budget;
    *m = account->monthly_budget;
    return;
  }

  Account* child = account->childs;
  while (child) {
    double y, m;
    compute_budget_sum(child, &y, &m);
    y_sum += y;
    m_sum += m;

    child = child->next;
  }

  account->yearly_budget = y_sum;
  account->monthly_budget = m_sum;
}

static void save_period_info(Transaction* last_transaction) {
  Period* period = &periods[period_count++];
  compute_category_sums(journal.root_account, initial_sums);
  memcpy(period->sum, initial_sums, sizeof(initial_sums));
  period->date = last_transaction->date;
}

static void get_periods(Command* command) {
  period_count = 0;

  if (!command->running)
    memset(initial_sums, 0, sizeof(initial_sums));

  Transaction* prev_trans = 0;

  for (int i = 0; i < transaction_count; i++) {
    Transaction* trans = transactions[i];

    if (is_new_period(command, prev_trans, trans)) {
      save_period_info(prev_trans);

      if (!command->running)
        memset(initial_sums, 0, sizeof(initial_sums));

      if (period_count == MAX_PERIODS)
        return;
    }

    initial_sums[trans->to]   += trans->amount;
    initial_sums[trans->from] -= trans->amount;

    prev_trans = trans;
  }

  save_period_info(prev_trans);
}

void print_chars(int count, char c) {
  assert(count >= 0);
  while (count--) print("%c", c);
}

void print_balance_splitter(int account_width, int number_width, int column_count, bool ignore_first_column) {
  start_line();
  if (ignore_first_column) {
    set_this_cursor(account_width);
  } else {
    print_chars(account_width, '-');
  }

  while (column_count--) {
    print("+");
    print_chars(NUMBER_WIDTH, '-');
  }

  print("\n");
}

void print_number_in_field(bool print_zero, double number, int width, bool positive_color) {
  if (print_zero || number) {
    print_chars(NUMBER_WIDTH - get_digit_count(number) - 3, ' ');
    if (number < 0)
      print("\033[31m");
    print("%.2lf", number);
    format_off();
  } else {
    print_chars(NUMBER_WIDTH, ' ');
  }
}

void print_balance(Command* command) {
  if (!command->flat)
    command->is_short = true;

  if (command->quarterly)
    command->budget = false;

  if (command->running)
    command->budget = false;

  if (command->no_grid)
    command->print_zeros = true;

  get_periods(command);
  compute_budget_sum(journal.root_account, 0, 0);

  int width, height;
  get_size(&width, &height);

  int indentation      = command->flat ? 0 : 4;
  int name_width       = command->is_short ? get_max_account_name_length(indentation) : get_max_account_path_length(indentation);
  int name_field_width = name_width + 1;
  int number_width     = 0;
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

  for (int i = 0; i < column_count; i++) {
    print_chars(diff(NUMBER_WIDTH, date_width), ' ');

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

    if (command->budget && account->monthly_budget == 0 && account->yearly_budget == 0) {
      debug("Skipping %s\n", account->path);
      continue;
    }

    if (command->filter && !apply_filter_account(command->filter, account))
      continue;


    while (account) {
      print_enable[account->index] = true;
      debug("Marking %d  to print\n", account->index);
      account = account->parent;
    }
  }

  // Print account name and balance columns.
  for (int i = 1; i < journal.account_count; i++) {
    Account* account = &journal.accounts[i];
    if (print_enable[i] == false) continue;

    if (account->is_category) {
      if (account->parent == journal.root_account && !command->no_grid) {
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
      print("%c", command->no_grid ? ' ' : '|');
      if (command->percent) {
        assert(account->parent); // Iterate from 1.
        double parent_sum = periods[j].sum[account->parent->index];
        double this_sum   = periods[j].sum[i];
        double percent = (double)(100.0 * (this_sum / parent_sum));

        if (parent_sum == 0 || percent == 0) {
          print_chars(NUMBER_WIDTH, ' ');
        } else {
          print("%*.2lf%%", NUMBER_WIDTH - 1, percent);
        }
      } else {
        double tmp = periods[j].sum[i];

        if (command->budget) {
          if (command->yearly) {
            tmp = (12 * account->monthly_budget + account->yearly_budget) - tmp;
          } else {
            tmp = (account->monthly_budget + account->yearly_budget / 12) - tmp;
          }
        }

        print_number_in_field(command->print_zeros, tmp, NUMBER_WIDTH, command->budget && (account->monthly_budget != 0 || account->yearly_budget != 0));
      }
    }

    print("\n");
  }
}

static void print_minus(int count) {
  for (int i = 0; i < count; i++)
    print("-");
}

static void wall(char c) {
  print("-%c-", c);
}

static void print_transaction_splitter(Command* command, int name_width) {
  start_line();
  char splitter = command->no_grid ? '-' : '+';

  print("-----------");

  wall(splitter);
  print_minus(name_width);

  wall(splitter);
  print_minus(name_width);

  wall(splitter);
  print_minus(NUMBER_WIDTH);

  if (command->running) {
    wall(splitter);
    print_minus(NUMBER_WIDTH);
  }

  if (command->sum) {
    wall(splitter);
    print_minus(NUMBER_WIDTH);
  }

  if (command->print_ref) {
    wall(splitter);
    print_minus(REF_WIDTH);
  }

  wall(splitter);
  print("-------------\n");
}

void print_transaction(Command* command, Transaction* t, double* sums) {
  start_line();
  print("%02d.%s.%4d", t->date.day, month_names[t->date.month - 1], t->date.year);

  int name_width = command->is_short ? get_max_account_name_length(0) : get_max_account_path_length(0);
  int padding;

  int from = t->from;
  int to   = t->to;
  char splitter = command->no_grid ? ' ' : '|';

  print(" %c ", splitter);
  print("%s", command->is_short ? journal.accounts[from].name : journal.accounts[from].path);
  padding = name_width - (command->is_short ? journal.accounts[from].name_length : journal.accounts[from].path_length);
  assert(padding >= 0);
  while (padding--) print(" ");

  print(" %c ", splitter);
  print("%s", command->is_short ? journal.accounts[to].name : journal.accounts[to].path);
  padding = name_width - (command->is_short ? journal.accounts[to].name_length : journal.accounts[to].path_length);
  assert(padding >= 0);
  while (padding--) print(" ");


  print(" %c ", splitter);
  print_number_in_field(command->print_zeros, t->amount, NUMBER_WIDTH, false);

  if (command->running) {
    print(" %c ", splitter);
    print_number_in_field(command->print_zeros, t->from_sum, NUMBER_WIDTH, false);
  }

  if (command->sum) {
    print(" %c ", splitter);
    print_number_in_field(command->print_zeros, sums[t->from], NUMBER_WIDTH, false);
  }

  if (command->print_ref) {
    print(" %c ", splitter);
    if (t->reference >= 0) {
      print("%3d", t->reference);
    } else {
      print("   ");
    }
  }

  print(" %c ", splitter);
  if (t->description) {
    print("%s", t->description);
  }

  print("\n");
}

static void print_transactions(Command* command) {
  print("\n");

  if (command->running || command->sum)
    command->unify = true;

  int name_width = command->is_short ? get_max_account_name_length(0) : get_max_account_path_length(0);

  double sums[MAX_ACCOUNTS];
  memset(sums, 0, sizeof(sums));

  int count = 0;

  Transaction* prev_trans = 0;
  for (int i = 0; i < transaction_count; i++) {
    Transaction* trans = transactions[i];

    if (is_new_period(command, prev_trans, trans)) {
      if (command->no_grid)
        print("\n");
      else 
        print_transaction_splitter(command, name_width);

      memset(sums, 0, sizeof(sums));
    }

    sums[trans->from] -= trans->amount;
    sums[trans->to]   += trans->amount;

    if (command->unify) {
      trans->amount *= -1;

      if (trans->unify_print_from)
        print_transaction(command, transactions[i], sums);

      trans->amount *= -1;

      int tmp = trans->from;
      trans->from = trans->to;
      trans->to = tmp;

      if (trans->unify_print_to)
        print_transaction(command, transactions[i], sums);

      tmp = trans->from;
      trans->from = trans->to;
      trans->to = tmp;
    } else {
      print_transaction(command, transactions[i], sums);
    }

    prev_trans = trans;
  }
}

void execute_command(Command* command) {
  // Handle commands that does not need transactions.
  if (command->type == COMMAND_CLEAR) {
    clear_all();
    set_cursor(0, 0);
    return;
  }

  // If the filter is changed, print the modified filter.
  if (command->filter && command->filter_modified) {
    start_line();
    print("Using filter: ");
    print_filter(command->filter);
    print("\n");
  }

  // Store pointers to all transactions and sort the list by date.
  for (int i = 0; i < journal.raw_transaction_count; i++)
    transactions[i] = &journal.raw_transactions[i];

  journal_sort_transactions(transactions, journal.raw_transaction_count, sort_date_get_first, false);

  memset(running_sums, 0, sizeof(running_sums));
  memset(initial_sums, 0, sizeof(initial_sums));

  transaction_count = 0;

  for (int i = 0; i < journal.raw_transaction_count; i++) {
    Transaction* trans = transactions[i];

    bool date_keep_transaction = !command->date_present || (!date_is_smaller(&trans->date, &command->from.date) && !date_is_bigger(&trans->date, &command->to.date));
    bool filter_keep;

    if (command->unify) {
      trans->unify_print_from = false;
      trans->unify_print_to = false;

      trans->amount *= -1;
      
      if (command->filter == 0 || apply_filter(command->filter, trans))
        trans->unify_print_from = true;
      
      trans->amount *= -1;

      int tmp = trans->from;
      trans->from = trans->to;
      trans->to = tmp;

      if (command->filter == 0 || apply_filter(command->filter, trans))
        trans->unify_print_to = true;

      tmp = trans->from;
      trans->from = trans->to;
      trans->to = tmp;

      filter_keep = trans->unify_print_from || trans->unify_print_to;
    } else {
      filter_keep = command->type == COMMAND_BALANCE || command->filter == 0 || apply_filter(command->filter, trans);
    }

    bool keep = filter_keep && date_keep_transaction;

    if (keep && transaction_count == 0)
      memcpy(initial_sums, running_sums, sizeof(running_sums));

    running_sums[trans->from] -= trans->amount;
    running_sums[trans->to]   += trans->amount;

    trans->from_sum = running_sums[trans->from];
    trans->to_sum   = running_sums[trans->to];

    if (keep)
      transactions[transaction_count++] = trans;
  }

  if (!transaction_count) {
    start_line();
    print("\033[31mNo transactions");
    format_off();
    return;
  }

  // Update search filters.
  if (command->type == COMMAND_BALANCE)
    command->sort = SORT_DATE;

  if (command->sort == SORT_FROM) {
    journal_sort_transactions(transactions, transaction_count, sort_from_get_first, command->sort_reverse);
  } else if (command->sort == SORT_TO) {
    journal_sort_transactions(transactions, transaction_count, sort_to_get_first, command->sort_reverse);
  } else if (command->sort == SORT_AMOUNT) {
    journal_sort_transactions(transactions, transaction_count, sort_amount_get_first, command->sort_reverse);
  }

  if (command->type == COMMAND_PRINT) {
    print_transactions(command);
  } else if (command->type == COMMAND_BALANCE) {
    print_balance(command);
  }

  flush();
}
