#pragma once

#define ESOCKNOCREATE  -1
#define ESOCKNOCONNECT -2
#define ESOCKNOMSGSEND -3
#define ESOCKNOMSGRECV -4
#define ENOOKRESPONSE  -5
#define ENOSPACE       -10

#define MAX_LEN 256
#define MAX_SIZE 512

// 7 for "parent=" (maximal), 20 for number
#define INT_ARG(NAME) char NAME[27];


#define ERR_WRAPPER(predicate, msg, label)        \
	if (predicate) {                        \
		printk(KERN_ERR "%s\n", msg);       \
		goto label;                         \
	}

char* escape_url(char const* pref, char const* str, size_t len);

int send_msg_to_server(struct socket *sock, char *send_buf);

int recv_msg_from_server(struct socket *sock, char *recv_buf, int recv_buf_size);

int connect_to_server_atoi(const char *c);

int connect_to_server(const char *command, int params_count, const char *params[], const char *token, char *output_buf);
