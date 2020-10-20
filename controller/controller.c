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
extern int controller_cmd(int argc, char **argv);
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
char control_thread_stack[MAX_ROBOTS][THREAD_STACKSIZE_MAIN];
char auto_thread_stack[MAX_ROBOTS][THREAD_STACKSIZE_MAIN];
char message[MAX_ROBOTS][20];
uint8_t buf[255];

int robotID = 0;

/* DEFINING SETS OF COMMANDS FOR THE CONTROLLER */
static const shell_command_t shell_commands[] = {
	{"sUp", "Send move up instruction to a robot", controller_cmd},
	{"sDown", "Send move down instruction to a robot", controller_cmd},
	{"sLeft", "Send move left instruction to a robot", controller_cmd},
	{"sRight", "Send move right instruction to a robot", controller_cmd},
	{"stop", "Send stop instruction to a robot", controller_cmd},
	{"getSta", "Send get status instruction to a robot", controller_cmd},
	// {"getList", "List robot status", getList_cmd},
	{NULL, NULL, NULL}
};

/* THREAD HANDLER */

void *controller_thread_handler(void *arg) {
	(void)arg;

	int id = robotID;

	// CONFIGURING local.port TO WAIT FOR MESSAGES TRANSMITED TOWARDS THE CONTROLLER_PORT
	// THEN CREATING UDP SOCK BOUND TO THE LOCAL ADDRESS
	sock_udp_ep_t local = SOCK_IPV6_EP_ANY;
	sock_udp_t sock;

	local.port = CONTROLLER_PORT;

	/* IF THERE'S AN ERROR CREATING UDP SOCK
	if (sock_udp_create(&sock, &local, NULL, 0) < 0) {
		puts("Error creating UDP sock");
		return NULL;
	}*/

	// PREPARE A REMOTE ADDRESS WHICH CORRESPOND TO THE ROBOT
	sock_udp_ep_t remote = {.family = AF_INET6};
	if (ipv6_addr_from_str((ipv6_addr_t *)&remote.addr.ipv6, robot_addresses[id]) == NULL) {
		puts("Cannot convert server address");
		sock_udp_close(&sock);
		return NULL;
	}

	// SETTING THE PORT OF THE CONTROLLER
	remote.port = CONTROLLER_PORT + id;

	// configure the underlying network such that all packets transmitted will reach a server
	ipv6_addr_set_all_nodes_multicast((ipv6_addr_t *)&remote.addr.ipv6, IPV6_ADDR_MCAST_SCP_LINK_LOCAL);

	while (1) {
		// IF THERE IS NO COMMAND
		if (strlen(message[id]) == 0) {
			xtimer_sleep(1);
			continue;
		} else {
			// IF THERE'S AN ERROR CREATING UDP SOCK
			if (sock_udp_create(&sock, &local, NULL, 0) < 0) {
				puts("Error creating UDP sock");
				return NULL;
			}
			ssize_t res;
			/* TO BE DELETED ? */
			char ipv6_addr_send[IPV6_ADDR_MAX_STR_LEN];
			ipv6_addr_to_str(ipv6_addr_send, (ipv6_addr_t *)&remote.addr.ipv6, IPV6_ADDR_MAX_STR_LEN);
			printf("sending: %s to robot id: %d with address of: %s and port of: %d\n", message[id], id, ipv6_addr_send, remote.port);

			// TRANSMITTING THE MESSAGE TO THE ROBOT[id] WHILE CHECKING
			if (sock_udp_send(&sock, message[id], strlen(message[id]) + 1, &remote) < 0) {
				puts("Error sending message");
				sock_udp_close(&sock);
				return NULL;
			}

			// WAITING FOR A RESPONSE
			sock_udp_ep_t remote;
			uint8_t buf[255];
			res = sock_udp_recv(&sock, buf, sizeof(buf), 3 * US_PER_SEC, &remote);
			if (res < 0) {
				if (res == -ETIMEDOUT) {
					puts("Timed out, no response. Message may be lost or delayed.");
				} else {
					puts("Error receiving message. This should not happen.");
				}
			}
			else {
				// CONVERT ROBOT ADDRESS FROM ipv6_addr_t TO STRING
				char ipv6_addr[IPV6_ADDR_MAX_STR_LEN];
				if (ipv6_addr_to_str(ipv6_addr, (ipv6_addr_t *)&remote.addr.ipv6, IPV6_ADDR_MAX_STR_LEN ) == NULL) {
					printf("\nCannot convert server address\n");
					strcpy(ipv6_addr, "???");
				}

				// ensure a null-terminated string
				buf[res] = 0;

				/* TO BE DELETED ? */
				printf("Received from server (%s, %d): \"%s\"\n", ipv6_addr, remote.port, buf);
			}

			// DELETING COMMAND FROM THE MESSAGE
			strcpy(message[id], "");
			res = 0;
			sock_udp_close(&sock);
		}
		//Check to see if we have recieved a spontaneous message.
		/* if the port is the same as this threads robot
					Then we have a message to proccess and display
					if this message
						THEN
					etc

		*/
	}

	return NULL;
}

int main(void) {

	// SETTING UP VARIABLES

	// GETTING ALL ROBOT IPv6 ADDRESSES
	char str[24*MAX_ROBOTS];
	strcpy(str, ROBOT_ADDRESSES);
	char* token = strtok(str, ",");

	// int robotID = 0;

	// SETTING UP COMMUNICATION THREADS FOR EACH ROBOTS
	while (token != NULL) {
		strcpy(robot_addresses[robotID], token);
		//printf("This is the token: %s\n", token);

		// CREATING THREAD PASSING ID AS PARAMETER
		thread_create(control_thread_stack[robotID], sizeof(control_thread_stack[robotID]), THREAD_PRIORITY_MAIN - 1 - robotID,
			THREAD_CREATE_STACKTEST, controller_thread_handler, NULL, "control thread");

		// thread_create(auto_thread_stack[robotID], sizeof(control_thread_stack[robotID]), THREAD_PRIORITY_MAIN - 1,
		// 	THREAD_CREATE_STACKTEST, controller_thread_handler, (void *)&robotID, "control thread");

		numRobots++;
		robotID++;
		token = strtok(NULL, ",");
	}
	/*
	for(unsigned int i = 0; i < (sizeof(robot_addresses)/sizeof(robot_addresses[0])); i++){
		printf("robot %d address: %s\n", i, robot_addresses[i]);
	}*/

	// STARTING SHELL
	puts("Shell running");
	char line_buf[SHELL_DEFAULT_BUFSIZE];
	shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

	// SHOULD NEVER BE REACHED
	return 0;
}

/* DECLARING FUNCTIONS */
int controller_cmd(int argc, char **argv) {

	//IF USER DID NOT PUT ROBOT ID
	if (argc != 2) {
		printf("usage: %s robot_id\n", argv[0]);
		return 1;
	}

	int robotid = atoi(argv[1]);
	if (robotid <= MAX_ROBOTS - 1) {
		//SET MESSAGE[robot_id] WITH THE COMMAND TO BE SENT
		strcpy(message[robotid], argv[0]);
	} else {
		printf("No robot with this ID\n");
		return 1;
	}

	return 0;
}
