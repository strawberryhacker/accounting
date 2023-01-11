#include "journal.h"
#include "text.h"
#include "stdio.h"
#include "stdlib.h"
#include "assert.h"
#include "basic.h"
#include "terminal.h"
#include <string.h>

#define JOURNAL_FILE "data/journal.txt"
Journal journal;

static char* read_entire_file(const char* path) {
  FILE* file = fopen(path, "rb");
  assert(file);
  assert(fseek(file, 0, SEEK_END) == 0);
  long int size = ftell(file);
  assert(size >= 0);
  assert(fseek(file, 0, SEEK_SET) == 0);
  char* data = malloc(size + 1);
  assert(fread(data, 1, size, file) == (size_t)size);
  data[size] = 0;
  fclose(file);
  return data;
}

void journal_append_transaction(Transaction* t) {
  FILE* file = fopen(JOURNAL_FILE, "a");
  assert(file);
  fprintf(file, "$ %02d.%02d.%d %s %s %.2lf '", t->date.day, t->date.month, t->date.year, journal.accounts[t->from].path, journal.accounts[t->to].path, t->amount);
  if (t->description) fprintf(file, "%s", t->description);
  fprintf(file, "'");
  if (t->reference >= 0) fprintf(file, " %d", t->reference);
  fprintf(file, "\n");
  fclose(file);
}

static void build_account_path(char* dest, Account* account) {
  if (!account || !account->parent) return;
  build_account_path(dest, account->parent);
  strcat(dest, account->name);
  strcat(dest, ".");
}

static void insert_account(Account** next_pointer, Account* node) {
  while (*next_pointer) next_pointer = &(*next_pointer)->next;
  *next_pointer = node;
}

static Account* parse_account_group(Account* parent, char** cursor, char* name, int level) {
  int index = journal.account_count++;

  Account* account = &journal.accounts[index];
  account->parent = parent;
  account->index = index;
  account->level = level;

  build_account_path(account->path, parent);
  account->name = account->path + strlen(account->path);
  strcat(account->path, name);

  account->name_length = strlen(account->name);
  account->path_length = strlen(account->path);

  if (skip_char(cursor, '{')) {
    account->is_category = true;
    while (!skip_char(cursor, '}')) {
      Account* subaccount = parse_account_group(account, cursor, get_string(cursor), level + 1);
      insert_account(&account->childs, subaccount);
    }
  }

  account->count = journal.account_count - account->index;
  return account;
}

static void parse_accounts(char** cursor) {
  journal.root_account = parse_account_group(0, cursor, "Accounts", 0);
}

static Date parse_date(char** cursor) {
  Date date;
  for (int i = 0; i < 3; i++) {
    assert(!i || skip_char(cursor, '.'));
    ((int*)&date)[i] = get_number(cursor);
  }

  return date;
}

static int parse_account_reference(char** cursor) {
  char* string = get_string(cursor);

  for (int i = 0; i < journal.account_count; i++) {
    Account* account = &journal.accounts[i];
    if (account->is_category) continue;
    if (!strcmp(string, account->path)) return i;
  }

  assert(0);
}

static char* parse_description(char** cursor) {
  char* data = get_quoted_string(cursor);

  if (*data) {
    data = string_save(data);
    return data;
  } else {
    return 0;
  }
}

static int parse_reference(char** cursor) {
  int result = -1;
  skip_blank(cursor);

  if (is_number(**cursor)) {
    int n;
    sscanf(*cursor, "%d%n", &result, &n);
    *cursor += n;
  }

  return result;
}

static void parse_transaction(char** cursor) {
  Transaction* transaction = &journal.raw_transactions[journal.raw_transaction_count++];

  transaction->date        = parse_date(cursor);
  transaction->from        = parse_account_reference(cursor);
  transaction->to          = parse_account_reference(cursor);
  transaction->amount      = get_double(cursor);
  transaction->description = parse_description(cursor);
  transaction->reference   = parse_reference(cursor);
}

static void parse_budget(char** cursor) {
  if (skip_char(cursor, 'm')) {
    int account = parse_account_reference(cursor);
    journal.accounts[account].monthly_budget += get_double(cursor);
  } else if (skip_char(cursor, 'y')) {
    int account = parse_account_reference(cursor);
    journal.accounts[account].yearly_budget += get_double(cursor);
  } else {
    assert(0 && "missing m or y budget specifier");
  }
}

void journal_parse() {
  char* content = read_entire_file(JOURNAL_FILE);
  char* cursor = content;

  memset(&journal, 0, sizeof(journal));

  while (*cursor) {
    if (skip_char(&cursor, '@')) {
      parse_accounts(&cursor);
    } else if (skip_char(&cursor, '$')) {
      parse_transaction(&cursor);
    } else if (skip_char(&cursor, '?')) {
      parse_budget(&cursor);
    } else {
      skip_line(&cursor);
    }
  }

  free(content);
}

static void merge(Transaction** transactions, int start, int middle, int end, GetFirstTransaction get_first, bool reverse) {
  int left_size  = middle - start;
  int right_size = end - middle;

  Transaction** left_data  = &journal.sort_buffer[0];
  Transaction** right_data = &journal.sort_buffer[left_size];

  for (int i = 0; i < left_size;  i++) left_data [i] = transactions[start  + i];
  for (int i = 0; i < right_size; i++) right_data[i] = transactions[middle + i];

  int left_index  = 0;
  int right_index = 0;
  int dest_index  = start;

  while (left_index < left_size && right_index < right_size) {
    Transaction* left  =  left_data[left_index ];
    Transaction* right = right_data[right_index];

    Transaction* first = get_first(left, right);
    bool add_left = (!reverse) ? first == left : first == right;

    if (add_left) {
      transactions[dest_index++] = left;
      left_index++;
    } else {
      transactions[dest_index++] = right;
      right_index++;
    }
  }

  while (left_index  < left_size)  transactions[dest_index++] = left_data [left_index++ ];
  while (right_index < right_size) transactions[dest_index++] = right_data[right_index++];
}

static void merge_sort(Transaction** transactions, int start, int end, GetFirstTransaction get_first, bool reverse) {
  if ((start + 1) >= end) return;
  int middle = start + (end - start) / 2;

  merge_sort(transactions, start,  middle, get_first, reverse);
  merge_sort(transactions, middle, end,    get_first, reverse);

  merge(transactions, start, middle, end, get_first, reverse);
}

void journal_sort_transactions(Transaction** transactions, int count, GetFirstTransaction get_first, bool reverse) {
  merge_sort(transactions, 0, count, get_first, reverse);
}

Account* get_account(char* name) {
  for (int i = 0; i < journal.account_count; i++) {
    Account* node = &journal.accounts[i];
    if (!strcmp(name, node->path)) {
      return node;
    }
  }
  return 0;
}
