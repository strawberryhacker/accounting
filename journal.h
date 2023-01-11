#ifndef JOURNAL_H
#define JOURNAL_H

#include "date.h"
#include <stdbool.h>

#define MAX_ACCOUNT_LENGTH 64
#define MAX_ACCOUNTS       64
#define MAX_TRANSACTIONS   5000

typedef struct Account Account;
typedef struct Journal Journal;
typedef struct Transaction Transaction;

struct Account {
  char  path[MAX_ACCOUNT_LENGTH]; // Ex: Expenses.Trips.Abroad
  char* name;                     // Ex: Abroad

  int name_length; // Speed.
  int path_length; // Speed.

  bool is_category;
  int level; // 0 for root category, 1 for its children, etc.

  double monthly_budget;
  double yearly_budget;

  int index; // Index in jounal.accounts
  int count; // This account plus all subaccounts.

  Account* parent;
  Account* next;
  Account* childs;
};

struct Transaction {
  Date   date;
  char*  description;
  double amount;
  int    from;
  int    to;
  int    reference;
  double from_sum;
  double to_sum;
};

struct Journal {
  Account* root_account;
  Account  accounts[MAX_ACCOUNT_LENGTH];
  int      account_count;

  Transaction* sort_buffer[MAX_TRANSACTIONS];
  Transaction  raw_transactions[MAX_TRANSACTIONS];
  int          raw_transaction_count;
};

extern Journal journal;
typedef Transaction* (*GetFirstTransaction)(Transaction*, Transaction*);

void journal_parse();
void journal_sort_transactions(Transaction** transactions, int count, GetFirstTransaction get_first, bool reverse);
void journal_append_transaction(Transaction* transaction);
Account* get_account(char* name);

#endif
