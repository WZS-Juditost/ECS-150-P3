# Target library
targets := libfs.a
objs := fs.o disk.o

CFLAGS := -Wall -Werror -Wextra -MMD
CC := gcc
PANDOC := pandoc

ifneq ($(V),1)
Q=@
endif

all: $(targets)

deps := $(patsubst %.o,%.d,$(objs))
-include $(deps)

%.o: %.c
	@echo "CC $@"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

$(targets): $(objs)
	@echo "ar $@"
	$(Q)ar rcs $(targets) $(objs) 

clean:
	@echo "CLEAN"
	@rm -f $(targets) $(objs) $(deps)
## TODO: Phase 1
