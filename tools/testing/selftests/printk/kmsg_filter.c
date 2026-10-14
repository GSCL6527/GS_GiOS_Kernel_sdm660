// SPDX-License-Identifier: GPL-2.0
/* Verify that formerly filtered diagnostic messages reach /dev/kmsg. */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
	const char *terms[] = {
		"healthd", "logd", "DM_DEV_STATUS", "Untracked pid", "usb_gadget"
	};
	char message[256], record[8192], marker[128];
	int reader, writer, failed = 0;
	unsigned int i;

	reader = open("/dev/kmsg", O_RDONLY | O_NONBLOCK);
	writer = open("/dev/kmsg", O_WRONLY);
	if (reader < 0 || writer < 0) {
		printf("1..0 # SKIP /dev/kmsg requires a privileged target kernel\n");
		if (reader >= 0)
			close(reader);
		if (writer >= 0)
			close(writer);
		return 4;
	}
	if (lseek(reader, 0, SEEK_END) < 0) {
		perror("lseek /dev/kmsg");
		close(writer);
		close(reader);
		return 1;
	}
	printf("TAP version 13\n1..5\n");
	for (i = 0; i < sizeof(terms) / sizeof(terms[0]); i++) {
		struct pollfd pfd = { .fd = reader, .events = POLLIN };
		int found = 0, attempts;
		ssize_t n;

		snprintf(marker, sizeof(marker), "kmsg-filter-%ld-%u: %s",
			 (long)getpid(), i, terms[i]);
		snprintf(message, sizeof(message), "<3>%s\n", marker);
		n = write(writer, message, strlen(message));
		if (n != (ssize_t)strlen(message)) {
			printf("not ok %u - write %s\n", i + 1, terms[i]);
			failed = 1;
			continue;
		}
		/* Bound the wait even if unrelated messages keep arriving. */
		for (attempts = 0; attempts < 100 && !found; attempts++) {
			if (poll(&pfd, 1, 20) <= 0)
				continue;
			n = read(reader, record, sizeof(record) - 1);
			if (n < 0) {
				if (errno == EPIPE || errno == EAGAIN || errno == EINTR)
					continue;
				break;
			}
			record[n] = '\0';
			found = strstr(record, marker) != NULL;
		}
		printf("%s %u - retain %s\n", found ? "ok" : "not ok",
		       i + 1, terms[i]);
		failed |= !found;
	}
	close(writer);
	close(reader);
	return failed;
}
