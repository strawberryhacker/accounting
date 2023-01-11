#include "add.h"
#include "journal.h"
#include "suggestions.h"
#include "input.h"
#include <time.h>
#include <string.h>
#include "terminal.h"
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>

#define INPUT_WIDTH 40

enum {
  STATE_DATE,
  STATE_AMOUNT,
  STATE_FROM,
  STATE_TO,
  STATE_DESCRIPTION,
  STATE_CONFIRM,
  STATE_DONE,
  STATE_COUNT,
};

static Transaction transaction;
static int state;
static bool use_reference;
static char description_buffer[1024];
static char reference_buffer[1024];
static bool got_reference;

int get_next_index() {
  int max = 0;

  DIR* dir = opendir(REFS_PATH);
  assert(dir);

  while (true) {
    struct dirent* entry = readdir(dir);
    if (!entry) break;

    if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
    if (entry->d_type != DT_REG) continue;

    int number, n;
    assert(sscanf(entry->d_name, "%d%n", &number, &n) == 1);

    if (number > max) max = number;
  }

  closedir(dir);
  return max + 1;
}

int copy_reference() {
  int ref_size = strlen(reference_buffer);
  char* ext = reference_buffer + ref_size - 1;
  while (ext != reference_buffer && *ext != '.') ext--;
  int index = get_next_index();

  char dest_path[2000];
  snprintf(dest_path, 2000, "%s/%d%s", REFS_PATH, index, ext);

  FILE* source_file = fopen(reference_buffer, "rb");
  FILE* dest_file = fopen(dest_path, "wb");

  assert(source_file);
  assert(dest_file);

  char buffer[1024];

  size_t read_count;
  do {
    read_count = fread(buffer, 1, 1024, source_file);
    assert(fwrite(buffer, 1, read_count, dest_file) == read_count);
  } while (read_count);

  assert(fclose(source_file) == 0);
  assert(fclose(dest_file) == 0);

  return index;
}

void add_transaction_init() {
    state = STATE_DATE;
    input_update_width(INPUT_WIDTH, 2);
    set_suggestions_minimum_index(0);
}

static void date_update(bool enter) {
  Date input_date;
  int input_date_count;
  Date current_date;

  time_t t = time(NULL);
  struct tm tm  = *localtime(&t);
  current_date.day   = tm.tm_mday;
  current_date.month = tm.tm_mon  + 1;
  current_date.year  = tm.tm_year + 1900;

  int n;
  input_date_count = sscanf(input.data, "%d.%d.%d%n", &input_date.day, &input_date.month, &input_date.year, &n);
  if (input_date_count <= 0) return;

  if (input_date_count >= 1 && (input_date.day   < 1    ||   31 < input_date.day))   return;
  if (input_date_count >= 2 && (input_date.month < 1    ||   12 < input_date.month)) return;
  if (input_date_count == 3 && (input_date.year  < 1000 || 3000 < input_date.year))  return;

  if (input_date_count == 1) {
    input_date.month = current_date.month;
    input_date.year  = current_date.year;

    if (input_date.day > current_date.day) {
      input_date.month--;
      if (input_date.month == 0) {
        input_date.month = 12;
        input_date.year--;
      }
    }
  } else if (input_date_count == 2) {
    input_date.year = current_date.year;

    if (input_date.month > current_date.month || (input_date.month == current_date.month && input_date.day > current_date.day)) {
      input_date.year--;
    }
  }


  if (enter) {
    transaction.date.day   = input_date.day;
    transaction.date.month = input_date.month;
    transaction.date.year  = input_date.year;
    state = STATE_AMOUNT;
  } else {
    add_suggestion("%d.%d.%d", input_date.day, input_date.month, input_date.year);
  }
}

static void amount_update(bool enter) {
  int n;
  if ((sscanf(input.data, "%lf%n", &transaction.amount, &n) != 1) || (n != input.size)) return;
  if (enter) {
    state = STATE_FROM;
    set_suggestions_minimum_index(0);
  }
}

bool substring_in_account(char* data, int size, char* substring) {
    char* start = substring;
    while (true) {
        bool match = true;
        for (int i = 0; i < size; i++) {
            if (!substring[i]) return false;

            char a = data[i]      | 0x20;
            char b = substring[i] | 0x20;

            if (a != b) {
                match = false;
                break;
            }
        }

        if (match) return true;
        substring++;
    }
}

static void account_update(bool enter) {
  if (enter) {
    char* suggestion = get_suggestion();
    if (!suggestion) return;

    Account* account = get_account(suggestion);
    debug("Searching for account : %s\n", suggestion);
    debug("found account: %d\n", account->index);
    if (account && account->is_category == false) {
      if (state == STATE_FROM) {
        transaction.from = account->index;
      } else {
        transaction.to = account->index;
        set_suggestions_minimum_index(-1);
      }
      state++;
      return;
    }
  }
  
  suggest_account(input.data, false);
}

static void description_update(bool enter) {
  if (enter) {
    char* suggestion = get_suggestion();
    if (suggestion) {
      input_replace(suggestion, input.size, 0);
    } else {
      strcpy(description_buffer, input.data);
      transaction.description = description_buffer;
      state = STATE_CONFIRM;
    }
    return;
  } else {
    suggest_description(input.data);
  }
}

static void confirm_update(bool enter) {
  if (enter) {
    if (got_reference) transaction.reference = copy_reference();
    journal_append_transaction(&transaction);

    print("\n");
    set_x_cursor(0);
    clear_all_right();
    set_x_cursor(3);
    flush();

    state = STATE_DONE;
  } else {
    input_clear();
  }
}

bool add_transaction_update(int keycode) {
  bool enter = (keycode == KEYCODE_ENTER);
  int prev_state = state;

  if (keycode == KEYCODE_DRAG_AND_DROP_PATH) {
    strcpy(reference_buffer, get_drag_and_drop_buffer());
    got_reference = true;
    debug("Got path: \033[31m%s\033[0m\n", reference_buffer);
    return false;
  }

  if (keycode == KEYCODE_UP || keycode == KEYCODE_DOWN) return false;

  if (keycode == KEYCODE_ESCAPE) {
    state--;
    if (state < 0) return true;
  } switch (state) {
    case STATE_DATE:        date_update(enter);        break;
    case STATE_AMOUNT:      amount_update(enter);      break;
    case STATE_FROM:        account_update(enter);     break;
    case STATE_TO:          account_update(enter);     break;
    case STATE_DESCRIPTION: description_update(enter); break;
    case STATE_CONFIRM:     confirm_update(enter);     break;
  }

  if (prev_state != state) {
    input_clear();
    add_transaction_update(KEYCODE_NONE);
  }

  return state == STATE_DONE;
}

#define print_faint(format, ...) faint(); print(format, ##__VA_ARGS__); format_off()

int add_transaction_render(int x) {
  int cursor = x;
  set_x_cursor(x);
  cursor += print("  ");

  if (got_reference) {
    int max = strlen(reference_buffer);
    int size = min(20, max);
    if (size != max) size -= 3;
    print("\033[33m");
    cursor += print("(ref %s%.*s)   ", (size != max) ? "..." : "", size, reference_buffer + max - size);
    format_off();
  }

  if (state >= STATE_DATE) {
    set_suggestions_print_cursor(cursor);

    if (state == STATE_DATE) {
      if (input.size) {
        print_input();
      } else {
        print_faint("date");
      }
    } else {
      cursor += print("%d.%d.%d", transaction.date.day, transaction.date.month, transaction.date.year);
    }
  }

  if (state >= STATE_AMOUNT) {
    cursor += print("   NOK ");

    if (state == STATE_AMOUNT) {
      if (input.size) {
        print_input();
      } else {
        print_faint("amount");
      }
    } else {
      cursor += print("%.2lf", transaction.amount);
    }
  }

  if (state >= STATE_FROM) {
    cursor += print("   ");
    set_suggestions_print_cursor(cursor);

    if (state == STATE_FROM) {
      if (input.size) {
        print_input();
      } else {
        print_faint("from account");
      }
    } else {
      cursor += print("%s", journal.accounts[transaction.from].path);
    }
  }

  if (state >= STATE_TO) {
    cursor += print("  >  ");
    set_suggestions_print_cursor(cursor);


    if (state == STATE_TO) {
      if (input.size) {
        print_input();
      } else {
        print_faint("to account");
      }
    } else {
      cursor += print("%s", journal.accounts[transaction.to].path);
    }
  }

  if (state >= STATE_DESCRIPTION) {
    cursor += print("   \"");
    set_suggestions_print_cursor(cursor);

    if (state == STATE_DESCRIPTION) {
      print_input();
      print("\"");
    } else {
      cursor += print("%s", description_buffer);
      cursor += print("\"");
    }
  }

  if (state >= STATE_CONFIRM) {
    cursor += print(" (confirm)");
  }


  return cursor;
}