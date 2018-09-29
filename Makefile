CC=gcc

all: sip_ans sip_call

sip_ans: sip_ans.c
	$(CC) -o $@ $< `pkg-config --cflags --libs libpjproject`

sip_call: sip_call.c
	$(CC) -o $@ $< `pkg-config --cflags --libs libpjproject`

clean:
	rm -f sip_ans.o sip_ans sip_call.o sip_call
