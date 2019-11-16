CC = gcc

LIB_SRC = \
	src/FileMonitor.c

HEADERS = \
	src/FileMonitor.h \
	test/conveniences.h \
	test/common.h \

APP_SRC = \
	test/fmon.c \
	test/mon_until_changed.c \
	test/common.c \

TEST_SRC = \
	test/test_FileMonitor.c \
	test/test_FileMonitor_main.c \

CMOCKERY_SRC = \
	test/cmockery/cmockery.c \

CFLAGS = -g -Wall -std=gnu99 -Isrc

LFLAGS =

LIB_OBJS = $(subst .c,.o,$(LIB_SRC))
TEST_OBJS = $(subst .c,.o,$(TEST_SRC))
APP_OBJS = $(subst .c,.o,$(APP_SRC))
CMOCKERY_OBJS = $(subst .c,.o,$(CMOCKERY_SRC))

OBJS = $(LIB_OBJS) $(TEST_OBJS) $(APP_OBJS) $(CMOCKERY_OBJS)

all : test fmon mon_until_changed

loc:
	cloc --by-file-by-lang .


fmon: test/common.o test/fmon.o $(LIB_OBJS)
	@echo linking $@
	@$(CC) $(CFLAGS) $(LINKER_FLAGS) $^ -o $@

mon_until_changed: test/common.o test/mon_until_changed.o $(LIB_OBJS)
	@echo linking $@
	@$(CC) $(CFLAGS) $(LINKER_FLAGS) $^ -o $@

test: test_file_monitor
	@echo =================== test starting ==================
	@echo
	./test_file_monitor

test_file_monitor: $(LIB_OBJS) $(TEST_OBJS) $(CMOCKERY_OBJS)
	@echo linking $@
	@$(CC) $(CFLAGS) $(LINKER_FLAGS) $^ -o $@


$(CMOCKERY_OBJS) : %.o : %.c
	@echo "compiling $@"
	@$(CC) -c \
	  $(CFLAGS) \
	  -Wno-implicit-function-declaration \
	  -Wno-pointer-to-int-cast \
	  -Wno-int-to-pointer-cast \
	   $< -o $@

$(TEST_OBJS) : %.o : %.c $(HEADERS)
	@echo "compiling $@"
	@$(CC) -c \
	  $(CFLAGS) \
	  -Itest/cmockery/ \
	  -Wno-unused-result \
	  $< -o $@

$(APP_OBJS) $(LIB_OBJS) : %.o: %.c $(HEADERS)
	@echo "compiling $@"
	@$(CC) -c $(CFLAGS) -Werror $< -o $@

clean:
	@echo cleaning
	@rm -f test_file_monitor
	@rm -f fmon
	@rm -f mon_until_changed
	@rm -f $(OBJS)

.phony: clean all test loc
