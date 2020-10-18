/* CLIENT = CONTROLLER */
/* make all PORT=tap3 ROBOT_ADDRESSES=fe80::b4e1:7aff:fe26:3b28 term */

#include <stdio.h>
#include "net/sock/udp.h"
#include "shell.h"
#include "thread.h"
#include "xtimer.h"
#include "msg.h"

#include "net/af.h"
#include "net/protnum.h"
#include "net/ipv6/addr.h"

// DECLARING FUNCTIONS
extern int _gnrc_netif_config(int argc, char **argv);
extern int sUp_cmd(int argc, char **argv);
// extern int sDown_cmd(int argc, char **argv);
// extern int sLeft_cmd(int argc, char **argv);
// extern int sRight_cmd(int argc, char **argv);
// extern int stop_cmd(int argc, char **argv);
// extern int getSta_cmd(int argc, char **argv);
// extern int getList_cmd(int argc, char **argv);
extern int getSta_cmd_remote(char* response);

// DECLARING VARIABLES
int numRobots = 0;
char robot_addresses[MAX_ROBOTS][24];
char stack[MAX_ROBOTS][THREAD_STACKSIZE_MAIN];
char message[MAX_ROBOTS][20];
uint8_t buf[16];

/* DEFINING SETS OF COMMANDS FOR THE CONTROLLER */
static const shell_command_t shell_commands[] = {
	{"sUp", "Send move up instruction to a robot", sUp_cmd},
	// {"sDown", "Send move down instruction to a robot", sDown_cmd},
	// {"sLeft", "Send move left instruction to a robot", sLeft_cmd},
	// {"sRight", "Send move right instruction to a robot", sRight_cmd},
	// {"stop", "Send stop instruction to a robot", stop_cmd},
	// {"getSta", "Send get status instruction to a robot", getSta_cmd},
	// {"getList", "List robot status", getList_cmd},
	{NULL, NULL, NULL}
};

/* THREAD HANDLER */

void *controller_thread_handler(void *arg) {

	int id = atoi(arg);
	printf("\nid: %d\n", id);

	// CONFIGURING local.port TO WAIT FOR MESSAGES TRANSMITED TOWARDS THE CONTROLLER_PORT
	// THEN CREATING UDP SOCK BOUND TO THE LOCAL ADDRESS
	sock_udp_ep_t local = SOCK_IPV6_EP_ANY;
	sock_udp_t sock;

	local.port = CONTROLLER_PORT;
	
	// IF THERE'S AN ERROR CREATING UDP SOCK
	if (sock_udp_create(&sock, &local, NULL, 0) < 0) {
		puts("Error creating UDP sock");
		return NULL;
	}

	// PREPARE A REMOTE ADDRESS WHICH CORRESPOND TO THE ROBOT
	sock_udp_ep_t remote = {.family = AF_INET6};
	if (ipv6_addr_from_str((ipv6_addr_t *)&remote.addr.ipv6, robot_addresses[id]) == NULL) {
		puts("Cannot convert server address");
		sock_udp_close(&sock);
		return NULL;
	}
	// then the remote port
	remote.port = CONTROLLER_PORT;

	//configure the underlying network such that all packets transmitted will reach a server
	ipv6_addr_set_all_nodes_multicast((ipv6_addr_t *)&remote.addr.ipv6, IPV6_ADDR_MCAST_SCP_LINK_LOCAL);

	ssize_t res;
	while (1) {
		if (strlen(message[id]) == 0) {
			xtimer_sleep(1);
			continue;
		}
		else
		{
			printf("sending: %s\n", message[id] );

			if (sock_udp_send(&sock, message[id], strlen(message[id]), &remote) < 0)
			{
				puts("Error sending message");
				sock_udp_close(&sock);
				return NULL;
			}

			// wait for msg
			sock_udp_ep_t remote;
			buf[0] = 0;
			res = sock_udp_recv(&sock, buf, sizeof(buf), 1 * US_PER_SEC, &remote);
			if (res < 0) {
				if (res == -ETIMEDOUT) {
					puts("Timed out, no response. Message may be lost or delayed.");
				} else {
					puts("Error receiving message. This should not happen.");
				}
			}
			else {
				// convert server (i.e. remote) address from ipv6_addr_t to string
				char ipv6_addr[IPV6_ADDR_MAX_STR_LEN];
				if (ipv6_addr_to_str(ipv6_addr, (ipv6_addr_t *)&remote.addr.ipv6, IPV6_ADDR_MAX_STR_LEN ) == NULL) {
					printf("\nCannot convert server address\n");
					strcpy(ipv6_addr, "???");
				}

				// ensure a null-terminated string
				buf[res] = 0;
				printf("Received from server (%s, %d): \"%s\"\n", ipv6_addr, remote.port, buf);
				// take parameter from the message
			}

			// DELETING COMMAND FROM THE MESSAGE
			strcpy(message[id], "");
			// printf("Sending %s instruction to robot\n", arg[0]);
		}
	}

	return NULL;
}

int main(void) {

	// SETTING UP VARIABLES

	// GETTING ALL ROBOT IPv6 ADDRESSES
	char str[24*MAX_ROBOTS];
	strcpy(str, ROBOT_ADDRESSES);
	char* token = strtok(str, ",");

	int robotID = 0;

	// SETTING UP COMMUNICATION THREADS FOR EACH ROBOTS
	while (token != NULL) {
		strcpy(robot_addresses[robotID], token);

		// CREATING THREAD PASSING ID AS PARAMETER
		thread_create(stack[robotID], sizeof(stack[robotID]), THREAD_PRIORITY_MAIN - 1,
			THREAD_CREATE_STACKTEST, controller_thread_handler, (void *)&robotID, "control thread");

		numRobots++;
		robotID++;
		token = strtok(NULL, ",");
	}

	// STARTING SHELL
	puts("Shell running");
	char line_buf[SHELL_DEFAULT_BUFSIZE];
	shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

	// SHOULD NEVER BE REACHED
	return 0;
}

/* DECLARING FUNCTIONS */
int sUp_cmd(int argc, char **argv) {

	//IF USER DID NOT PUT ROBOT ID
	if (argc != 2) {
		printf("usage: %s robot_id\n", argv[0]);
		return 1;
	}

	int robotid = atoi(argv[1]);
	if (robotid <= MAX_ROBOTS - 1) {
		//SET MESSAGE[robot_id] WITH THE COMMAND TO BE SENT
		strcpy(message[robotid], argv[0]);
		printf("Sending %s instruction to robot\n", argv[0]);

	} else {
		printf("No robot with this ID\n");
		return 1;
	}

	return 0;
}
