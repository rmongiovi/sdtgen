SRCS:=$(wildcard *.c)

DEPDIR:=.deps
DEPFLAGS=-MT $@ -MMD -MP -MF $(DEPDIR)/$*.d

COMPILE.c=$(CC) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c

../packtables: packtables.o intset.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

../tableformat: tableformat.o intset.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

.PHONY: clean
clean:
	/usr/bin/rm -rf $(DEPDIR) *.o

%.o: %.c
%.o: %.c $(DEPDIR)/%.d | $(DEPDIR)
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(DEPDIR):
	@/usr/bin/mkdir -p $@

DEPFILES:=$(SRCS:%.c=$(DEPDIR)/%.d)
$(DEPFILES):

include $(wildcard $(DEPFILES))
