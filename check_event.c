#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "check_event.h"

struct input_event {
	struct timeval time;
	uint16_t type;
	uint16_t code;
	int32_t value;
};

int check_event(void)
{
	int button_fd = 0, fifo_fd = 0;
	int ret = 0, rc, i;
	int event_id;
	struct input_event event;
	char input[8];

	fd_set read_fd_set;

	struct timeval timeout;
	timeout.tv_sec = 300;
	timeout.tv_usec = 0;

	fifo_fd = open("/root/alexa/wakeup", 2);
	if (fifo_fd < 0) {
		printf("fifo open error\n");
		return -1;
	}

	button_fd = open("/dev/input/event2", 2);
	if (button_fd < 0) {
		printf("button open error\n");
		return -1;
	}

	FD_ZERO(&read_fd_set);
	FD_SET(fifo_fd, &read_fd_set);
	FD_SET(button_fd, &read_fd_set);

	rc = select(button_fd+1, &read_fd_set, NULL, NULL, &timeout);

	if (rc == 0) {
		return EVENT_TIMEOUT;
	}

	for (i = 0; i < button_fd+1; i++) {
		if (FD_ISSET (i, &read_fd_set)) {
			if (i == fifo_fd) {
				ret = read(fifo_fd, &input, sizeof(input));
				printf("Wake up is triggered %s\n", input);
				event_id = EVENT_ROKID;
			}
			if (i == button_fd) {
				ret = read(button_fd, &event, sizeof(struct input_event));
				printf("***type:%#x code:%#x val:%#x*****\n", event.type, event.code, event.value);
				event_id = EVENT_BUTTON;
			}
			// Add more events here...
		}
	}

	close(fifo_fd);
	close(button_fd);

	return event_id;
}


