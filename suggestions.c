#define _GNU_SOURCE

#include "suggestions.h"
#include "stdio.h"
#include "basic.h"
#include "journal.h"
#include "terminal.h"
#include "assert.h"
#include <string.h>

#define MAX_VIEWED_SUGGESTIONS 3
#define MAX_SUGGESTIONS        32
#define MAX_SUGGESTION_LENGTH  256

static char buffer[MAX_SUGGESTIONS][MAX_SUGGESTION_LENGTH];
static int position;
static int offset;
static int count;
static int cursor;
static int minimum_position;
static char* last_selected_suggestion;

static void update_offset() {
  offset = compute_offset(position, offset, MAX_VIEWED_SUGGESTIONS, 0, 1);
}

static void update_pointer() {
  last_selected_suggestion = (position >= 0) ? buffer[position] : 0;
}

void suggestion_up() {
  position = max(position - 1, minimum_position);
  update_offset();
  update_pointer();
}

void suggestion_down() {
  position = min(position + 1, count - 1);
  update_offset();
  update_pointer();
}

bool suggestion_active() {
  return count > 0;
}

void set_suggestions_print_cursor(int x) {
  cursor = x;
}

void set_suggestions_minimum_index(int index) {
  minimum_position = index;
  position = minimum_position;
}

bool add_suggestion(char* data, ...) {
  if (count == MAX_SUGGESTIONS) return false;
  char* dest = buffer[count++];
  va_list arguments;
  va_start(arguments, data);
  int size = vsnprintf(dest, MAX_SUGGESTION_LENGTH - 1, data, arguments);
  dest[size] = 0;
  return true;
}

char* get_suggestion() {
  // Makes it easier. No need to think about when to clear, when to get current suggestion, etc. Clear first, get at any time.
  return last_selected_suggestion;
}

void clear_suggestions() {
  if (count) {
    last_selected_suggestion = (position >= 0) ? buffer[position] : 0;
    position = minimum_position;
    count = 0;
    offset = 0;

    // Erase previously displayed suggestions.
    print("\r\n");
    clear_all_right();
    cursor_up(1);
  } else {
    last_selected_suggestion = 0;
  }
}

void print_suggestions() {
  int print_count = min(MAX_VIEWED_SUGGESTIONS, count - offset);
  if (cursor < 2) print_count = 0;

  // Make sure there is enough spaces for all suggestions. Prevent the terminal from suddenly scrolling.
  for (int i = 0; i < MAX_VIEWED_SUGGESTIONS; i++) print("\n");
  cursor_up(MAX_VIEWED_SUGGESTIONS);

  for (int i = offset; i < offset + print_count; i++) {
    print("\n");
    clear_line();
    set_x_cursor(cursor - 1);
    if (i == position) invert();
    print(" %s \033[0m", buffer[i]);
  }

  if (print_count) cursor_up(print_count);
}

void suggest_account(char* name, bool allow_cathegories) {
  for (int i = 0; i < journal.account_count; i++) {
    Account* account = &journal.accounts[i];
    if (allow_cathegories == false && account->is_category) continue;
    if (strcasestr(account->path, name)) {
      if (!add_suggestion("%s", account->path)) return;
    }
  }
}

void suggest_description(char* name) {
  for (int i = 0; i < journal.raw_transaction_count; i++) {
    char* desc = journal.raw_transactions[i].description;
    if (!desc) continue;

    if (strcasestr(desc, name)) {
      bool unique = true;
      for (int j = 0; j < count; j++) {
        if (!strcmp(buffer[j], desc)) {
          unique = false;
          break;
        }
      }

      if (unique && !add_suggestion(desc)) break;
    }
  }
}
