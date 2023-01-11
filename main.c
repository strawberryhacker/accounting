#include "stdio.h"
#include "journal.h"
#include "suggestions.h"
#include "basic.h"
#include "terminal.h"
#include "command_line.h"
#include "input.h"
#include "add.h"
#include <stdio.h>
#include "history.h"

int main() {
  journal_parse();

  terminal_init();
  load_history_from_file();
  command_line_handle(KEYCODE_NONE);

  while (1) {
    int keycode = get_input_keycode();
    if (keycode == KEYCODE_CTRL_C) break;
    if (keycode == KEYCODE_NONE) continue;
    
    command_line_handle(keycode);
  }

  return 0;
}
