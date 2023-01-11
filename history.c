#include "history.h"
#include "input.h"
#include "terminal.h"
#include "suggestions.h"
#include "basic.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define HISTORY_COUNT 64

// 4 5 6 _ 1 2 3
//   ^   ^
//   |   index = 3
//   undo = 2

static char data[HISTORY_COUNT][INPUT_SIZE];
static int count;
static int position;
static int undo;

void load_history_from_file() {
  FILE* file = fopen(HISTORY_PATH, "r");
  assert(file);
  size_t n = 0;
  char* line;

  while (true) {
    int size = getline(&line, &n, file);
    if (size == -1) break;

    // Strip CR and LF.
    for (int i = 0; i < 2; i++) {
      if (size && (line[size - 1] == '\n' || line[size - 1] == '\r')) size--;
    }

    if (size) {
      line[size] = 0;

      assert(count < HISTORY_COUNT - 1);
      strcpy(data[position], line);
      debug("Added history item '%s'\n", line);

      position++;
      count++;
    }
  }

  if (line) free(line);
}

static void move(int relative) {
  // Save current input.
  if (undo == 0) strcpy(data[position], input.data);

  undo = limit(undo + relative, 0, count);

  int select_index = position - undo;
  if (select_index < 0) select_index += HISTORY_COUNT;

  input_replace(data[select_index], input.size, 0);
}

void history_up() {
  move(1);
}

void history_down() {
  move(-1);
}

void history_enter() {
  undo = 0;

  if (input.size) {
    strcpy(data[position++], input.data);

    if (count <  (HISTORY_COUNT - 1)) count++;
    if (position == HISTORY_COUNT) position = 0;

    // Save all items.
    FILE* file = fopen(HISTORY_PATH, "w");

    int save_index = position - count;
    if (save_index < 0) save_index += HISTORY_COUNT;
    
    for (int i = 0;  i < count; i++) {
      fprintf(file, "%s\n", data[save_index++]);
      if (save_index == HISTORY_COUNT) save_index = 0;
    }

    fclose(file);
  }
}

void history_other() {
  if (undo) {
    undo = 0;
    input_replace(data[position], input.size, 0);
  }
}
