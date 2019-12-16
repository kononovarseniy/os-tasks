CC=gcc
FLAGS=-Wall
SOURCES=round_buffer.c pump.c main.c manager.c controller.c command_queue.c
OBJECTS=$(addprefix $(OBJDIR)/, $(SOURCES:.c=.o))
OBJDIR=obj
BINDIR=bin

all: directories portfwd
portfwd: $(OBJECTS)
	$(CC) -g -o $(BINDIR)/portfwd $(OBJECTS)
$(OBJDIR)/%.o: %.c
	$(CC) -g $(FLAGS) -c $< -o $@
directories: $(OBJDIR) $(BINDIR)
$(OBJDIR):
	mkdir $(OBJDIR)
$(BINDIR):
	mkdir $(BINDIR)
