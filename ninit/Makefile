CC      := gcc
CFLAGS  := -Os -s -static -I.

stage2/ninit: stage2/ninit.c stage2/runner.c utils/parser.c
	$(CC) $(CFLAGS) -o $@ $^

nsv/nsv: nsv/nsv.c
	$(CC) $(CFLAGS) -o $@ $^
