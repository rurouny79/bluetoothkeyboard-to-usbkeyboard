#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <fcntl.h>

#define le16_to_cpu(val) (val)

#define HCI_CHANNEL_MONITOR	2
#define BTSNOOP_MAX_PACKET_SIZE		(1486 + 4)
#define BTSNOOP_OPCODE_ACL_RX_PKT	5
#define acl_flags(h)		(h >> 12)
#define BTPROTO_HCI	1
#define HCI_DEV_NONE	0xffff
#define MAX_EPOLL_EVENTS 10

typedef void (*mainloop_event_func)(void *user_data);
typedef void (*mainloop_destroy_func)(void *user_data);

struct mainloop_data {
	int fd;
	uint32_t events;
	mainloop_event_func callback;
	mainloop_destroy_func destroy;
	void *user_data;
};

struct control_data {
	uint16_t channel;
	int fd;
	unsigned char buf[BTSNOOP_MAX_PACKET_SIZE];
	uint16_t offset;
};

struct mgmt_hdr {
	uint16_t opcode;
	uint16_t index;
	uint16_t len;
} __packed;
#define MGMT_HDR_SIZE	6

struct bt_hci_acl_hdr {
	uint16_t handle;
	uint16_t dlen;
} __attribute__ ((packed));
#define HCI_ACL_HDR_SIZE	4

struct bt_l2cap_hdr {
	uint16_t len;
	uint16_t cid;
} __attribute__ ((packed));

struct sockaddr_hci {
	sa_family_t hci_family;
	unsigned short hci_dev;
	unsigned short hci_channel;
};

int fd_hidg0;

void packet_hexdump(const unsigned char *buf, uint16_t len) {
	if (!len)
		return;

	// (Left Control + A)
	// HHKB   : 1b 18 00 01 00 04 00 00 00 00 00
        if (buf[0] == 0x1b && buf[1] == 0x18) {                                 
                char cmd[8];                                                    
                cmd[0] = buf[3];                                                
                cmd[1] = 0x00;                                                  
                cmd[2] = buf[5];                                                
                cmd[3] = buf[6];                                                
                cmd[4] = buf[7];                                                
                cmd[5] = buf[8];                                                
                cmd[6] = buf[9];                                                
                cmd[7] = buf[10];                                               
                                                                                
                write(fd_hidg0, cmd, sizeof(cmd));                              
        }
	
	// (Left Control + A)
	// FC660R : a1 01 01 00 04 00 00 00 00 00
	else if (buf[0] == 0xa1 && buf[1] == 0x01) {
		char cmd[8];
		cmd[0] = buf[2];
		cmd[1] = 0x00;
		cmd[2] = buf[4];
		cmd[3] = buf[5];
		cmd[4] = buf[6];
		cmd[5] = buf[7];
		cmd[6] = buf[8];
		cmd[7] = buf[9];

		write(fd_hidg0, cmd, sizeof(cmd));
	}

#ifdef DBG
	static const char hexdigits[] = "0123456789abcdef";
	char str[68] = { '\0', };
	uint16_t i;
	
	for (i = 0; i < len; i++) {
		str[(i * 3) + 0] = hexdigits[buf[i] >> 4];
		str[(i * 3) + 1] = hexdigits[buf[i] & 0xf];
		str[(i * 3) + 2] = ' ';
	}

	printf("%s\n", str);
#endif
}

void packet_hci_acldata(const void *data, uint16_t size) {
	const struct bt_hci_acl_hdr *hdr = data;
	uint16_t handle = le16_to_cpu(hdr->handle);
	uint8_t flags = acl_flags(handle);

	data += HCI_ACL_HDR_SIZE;
	size -= HCI_ACL_HDR_SIZE;

	const struct bt_l2cap_hdr *hdr_l2cap = data;
	uint16_t len;

	switch (flags) {
	case 0x02: /* start of an automatically-flushable PDU */
		len = le16_to_cpu(hdr_l2cap->len);

		data += sizeof(*hdr_l2cap);
		size -= sizeof(*hdr_l2cap);

		if (len == size) {
			/* complete frame */
			packet_hexdump(data, len);
			return;
		}

		break;
	}
}

static void free_data(void *user_data) {
	struct control_data *data = user_data;

	close(data->fd);
	free(data);
}

static void data_callback(void *user_data) {
	struct control_data *data = user_data;
	unsigned char control[64];
	struct mgmt_hdr hdr;
	struct msghdr msg;
	struct iovec iov[2];

	iov[0].iov_base = &hdr;
	iov[0].iov_len = MGMT_HDR_SIZE;
	iov[1].iov_base = data->buf;
	iov[1].iov_len = sizeof(data->buf);

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = iov;
	msg.msg_iovlen = 2;
	msg.msg_control = control;
	msg.msg_controllen = sizeof(control);

	while (1) {
		uint16_t opcode, pktlen;
		ssize_t len;

		len = recvmsg(data->fd, &msg, MSG_DONTWAIT);
		if (len < 0)
			break;

		if (len < MGMT_HDR_SIZE)
			break;

		opcode = le16_to_cpu(hdr.opcode);
		pktlen = le16_to_cpu(hdr.len);

		if (opcode == BTSNOOP_OPCODE_ACL_RX_PKT)
			packet_hci_acldata(data->buf, pktlen);
	}
}

static int open_socket(void) {
	struct sockaddr_hci addr;
	int fd;

	fd = socket(AF_BLUETOOTH, SOCK_RAW | SOCK_CLOEXEC, BTPROTO_HCI);

	memset(&addr, 0, sizeof(addr));
	addr.hci_family = AF_BLUETOOTH;
	addr.hci_dev = HCI_DEV_NONE;
	addr.hci_channel = HCI_CHANNEL_MONITOR;

	bind(fd, (struct sockaddr*) &addr, sizeof(addr));

	return fd;
}

int main(void) {
	fd_hidg0 = open("/dev/hidg0", O_WRONLY);

	struct control_data *data;
	struct mainloop_data *ev_data;
	struct epoll_event ev;
	int epoll_fd;
	int epoll_terminate = 0;

	epoll_fd = epoll_create1(EPOLL_CLOEXEC);

	data = malloc(sizeof(*data));

	memset(data, 0, sizeof(*data));
	data->channel = HCI_CHANNEL_MONITOR;
	data->fd = open_socket();

	ev_data = malloc(sizeof(*ev_data));

	memset(ev_data, 0, sizeof(*ev_data));
	ev_data->fd = data->fd;
	ev_data->events = EPOLLIN;
	ev_data->callback = data_callback;
	ev_data->destroy = free_data;
	ev_data->user_data = data;

	memset(&ev, 0, sizeof(ev));
	ev.events = EPOLLIN;
	ev.data.ptr = ev_data;

	epoll_ctl(epoll_fd, EPOLL_CTL_ADD, ev_data->fd, &ev);

	while (!epoll_terminate) {
		struct epoll_event events[MAX_EPOLL_EVENTS];
		int n, nfds;

		nfds = epoll_wait(epoll_fd, events, MAX_EPOLL_EVENTS, -1);
		if (nfds < 0)
			continue;

		for (n = 0; n < nfds; n++) {
			struct mainloop_data *data = events[n].data.ptr;

			data->callback(data->user_data);
		}
	}

	return 0;
}
