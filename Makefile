SRC_TOKEN=token.c
SRC_AVS=alexa_request_simple_demo.c cJSON.c check_event.c

OBJ_TOKEN=$(SRC_TOKEN:.c=.o)
OBJ_AVS=$(SRC_AVS:.c=.o)

CFLAGS += -Werror -I.
LDLIBS_AVS += -lpthread -lcurl -ljson-c -lm
LDLIBS_TOKEN += -lcurl -ljson-c


all: clean avs token


avs: $(OBJ_AVS)
	$(CC) $^ $(CFLAGS) -o $@ $(LDLIBS_AVS)

token: $(OBJ_TOKEN)
	$(CC) $^ $(CFLAGS) -o $@ $(LDLIBS_TOKEN)


clean:
	-rm -f avs token *.o
