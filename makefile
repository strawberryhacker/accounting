REDIRECT     = /dev/tty2

JOURNAL_PATH = example_data/data
REFS_PATH    = example_data/refs
HISTORY_PATH = example_data/history

#JOURNAL_PATH = ../private-accounting/data
#REFS_PATH    = ../private-accounting/refs
#HISTORY_PATH = ../private-accounting/history

FLAGS = -O1 \
				-g \
				-I. \
				-DREDIRECT=\"$(REDIRECT)\" \
				-DJOURNAL_PATH=\"$(JOURNAL_PATH)\" \
				-DREFS_PATH=\"$(REFS_PATH)\" \
				-DHISTORY_PATH=\"$(HISTORY_PATH)\" \
				-Wall \
				-Wextra \
				-Wno-unused-function \
				-Wno-unused-variable \
				-Wno-unused-but-set-variable \
				-Wno-unused-parameter \

FILES = main.c \
				journal.c \
				text.c \
				command_line.c \
				suggestions.c \
				terminal.c \
				input.c \
				add.c \
				history.c \
				command.c \
				date.c \

BINARY = binary

.PHONY: build


build:
	@echo -e "\033\0143" > $(REDIRECT)
	@gcc $(FLAGS) $(FILES) -o $(BINARY) 2> $(REDIRECT)
	@gdb ./$(BINARY) -ex 'start' -ex 'c'

run:
	@echo -e "\033\0143" > $(REDIRECT)
	@gcc $(FLAGS) $(FILES) -o $(BINARY) 2> $(REDIRECT)
	@./$(BINARY)
