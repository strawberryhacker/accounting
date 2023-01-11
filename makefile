REDIRECT = /dev/tty3

FLAGS = -O1 \
				-g \
				-I. \
				-DREDIRECT=\"$(REDIRECT)\" \
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
