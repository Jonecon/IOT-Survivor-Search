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

#define DIRECTION_UP 1;
#define DIRECTION_DOWN 2;
#define DIRECTION_LEFT 3;
#define DIRECTION_RIGHT 4;
#define DIRECTION_POS 5;
#define DIRECTION_STOP 0;

// DECLARING FUNCTIONS
extern int _gnrc_netif_config(int argc, char **argv);
extern int controller_cmd(int argc, char **argv);
extern int getSta_cmd_remote(char* response);

struct Point {
	int x;
	int y;
};

typedef struct {
    int status;
	int energy;
	int survFoundCount;
	struct Point position;
	//Hard coded need to change.
	struct Point survFoundList[3];
	mutex_t lock;
} robot_data;

// DECLARING VARIABLES
char robot_addresses[MAX_ROBOTS][24];
char control_thread_stack[MAX_ROBOTS][THREAD_STACKSIZE_MAIN];
char auto_thread_stack[MAX_ROBOTS][THREAD_STACKSIZE_MAIN];
char listener_thread_stack[THREAD_STACKSIZE_MAIN];
char message[MAX_ROBOTS][20];

uint8_t buf[255];

int robotID = 0;
int numRobots = 0;

struct Point border;
//struct Point position;
static robot_data robots[MAX_ROBOTS];

/* DEFINING SETS OF COMMANDS FOR THE CONTROLLER */
static const shell_command_t shell_commands[] = {
	{"u", "Send move up instruction to a robot", controller_cmd},
	{"d", "Send move down instruction to a robot", controller_cmd},
	{"l", "Send move left instruction to a robot", controller_cmd},
	{"r", "Send move right instruction to a robot", controller_cmd},
	{"s", "Send stop instruction to a robot", controller_cmd},
	{"g", "Send get status instruction to a robot", controller_cmd},
	{"p", "Send pos instruction to a robot", controller_cmd},
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
			ssize_t res;

			// IF THERE'S AN ERROR CREATING UDP SOCK
			if (sock_udp_create(&sock, &local, NULL, 0) < 0) {
				puts("Error creating UDP sock");
				return NULL;
			}
			
			printf("\nSending: %s to robot id: %d", message[id], id);

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
					puts("\nTimed out, no response. Message may be lost or delayed.");
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

				/* Decide what to do with this information;
					-maybe update info on robot for the logic thread
					-Don't need to reply in this thread now
				*/


				//printf("Received from server (%s, %d): \"%s\"\n", ipv6_addr, remote.port, buf);
				strcpy(message[id], "");
			}
			sock_udp_close(&sock);
		}
	}

	return NULL;
}

void *listener_thread_handler(void* arg){

	//Here so we can display all messages recieved and are able to recieve spontaneous messages.
	/* if the port is the same as this threads robot
				Then we have a message to proccess and display
				if this message
					THEN
				etc
	*/
	(void) arg;
	sock_udp_ep_t local = SOCK_IPV6_EP_ANY;
	sock_udp_t sock;

	local.port = CONTROLLER_PORT;

	// IF THERE'S AN ERROR CREATING UDP SOCK
	if (sock_udp_create(&sock, &local, NULL, 0) < 0) {
		puts("Error creating UDP sock");
		return NULL;
	}

	while (1){

		ssize_t res;
		sock_udp_ep_t remote;
		uint8_t buf[255];
		res = sock_udp_recv(&sock, buf, sizeof(buf), 3 * US_PER_SEC, &remote);

		if (res < 0) {
			if (res == -ETIMEDOUT) {
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
			// printf("Received from robot (%s, %d): \"%s\"\n", ipv6_addr, remote.port, buf);
			printf("\nReceived from robot (%d): \"%s\"", remote.port, buf);

			int id, energy, x, y, status, sx, sy;

			if ((sscanf((char*) buf, "%d e %d p %d %d d %d", &id, &energy, &x, &y, &status)) == 5) {
				robots[id].status = status;
				robots[id].energy = energy;
				robots[id].position.x = x;
				robots[id].position.y = y;
			} else if ((sscanf((char*) buf, "%d d %d p %d %d", &id, &status, &x, &y)) == 4) {
				robots[id].status = status;
				robots[id].position.x = x;
				robots[id].position.y = y;
			} else if ((sscanf((char*) buf, "%d f %d %d d %d", &id, &sx, &sy, &status)) == 4) {
				robots[id].status = status;
				robots[id].position.x = sx;
				robots[id].position.y = sy;
				robots[id].survFoundList[robots[id].survFoundCount].x = sx;
				robots[id].survFoundList[robots[id].survFoundCount].y = sy;
			}
		}
	}
}

void *logic_thread_handler(void *arg) {
	(void)arg;

	// DECLARING VARIABLES
	border.x = NUM_LINES + 1;
	border.y = NUM_COLUMNS + 1;

	int x,y;
	
	// MOVING THE ROBOT TO ITS INITIAL POSITION
	x = 0;
	for (int i = 0; i < numRobots; ++i) {

		y = ((border.y/numRobots)*i);

		//SET MESSAGE[robot_id] WITH THE COMMAND TO BE SENT
		sprintf(message[i], "p %d %d", x, y);

		/* TO BE DELETED ? */
		printf("\nRobot %d position (%d, %d)",i, x, y);
	}

	xtimer_sleep(4);

	for (int i = 0; i < numRobots; ++i) {
		strcpy(message[i], "r");
		xtimer_sleep((border.y/numRobots) + 1);
	}

	while(1) {

		for (int i = 0; i < numRobots; ++i) {

			/*
			IF the robot status is 0
				THEN IF 

			*/



			// START MOVING RIGHT THEN WAIT FOR A MSG WHEN A ROBOT IS STOP
			
			if (robots[i].status == 0) {

				// CHECK IF ITS STOP BECAUSE IT REACHED Y BORDER 
				if ((robots[i].position.x == 0) || (robots[i].position.x == NUM_LINES)) {
					
					//SET MESSAGE[robot_id] WITH THE COMMAND TO BE SENT
					sprintf(message[i], "p %d %d", robots[i].position.x, (robots[i].position.y + 1));

					xtimer_sleep(4);

					if (robots[i].status == 0)
					{
						// IF IT REACHED MAX LEFT
						if (robots[i].position.x == 0) {

							//SET MESSAGE[robot_id] WITH THE COMMAND TO BE SENT
							strcpy(message[i], "r");
						}
						// IF IT REACHED MAX RIGHT
						if (robots[i].position.x == NUM_LINES) {

							//SET MESSAGE[robot_id] WITH THE COMMAND TO BE SENT
							strcpy(message[i], "l");
						}
					}
				}
			}
		}

		xtimer_sleep(1);
	}
}

int main(void) {

	// GETTING ALL ROBOT IPv6 ADDRESSES
	char str[24*MAX_ROBOTS];
	strcpy(str, ROBOT_ADDRESSES);
	char* token = strtok(str, ",");

	// SETTING UP COMMUNICATION THREADS FOR EACH ROBOTS
	while (token != NULL) {
		strcpy(robot_addresses[robotID], token);

		// CREATING THREAD PASSING ID AS PARAMETER
		thread_create(control_thread_stack[robotID], sizeof(control_thread_stack[robotID]), THREAD_PRIORITY_MAIN - 1,
			THREAD_CREATE_STACKTEST, controller_thread_handler, NULL, "Control Thread");

		robots[robotID].survFoundCount = 0;
		robots[robotID].status = -1;
		//robots[robotID].

		numRobots++;
		robotID++;
		token = strtok(NULL, ",");
	}

	thread_create(auto_thread_stack[robotID], sizeof(auto_thread_stack[robotID]), THREAD_PRIORITY_MAIN - 1,
			THREAD_CREATE_STACKTEST, logic_thread_handler, NULL, "Logic Thread");

	thread_create(listener_thread_stack, sizeof(listener_thread_stack), THREAD_PRIORITY_MAIN - 1,
		THREAD_CREATE_STACKTEST, listener_thread_handler, NULL, "Listener Thread");

	/*
	for(unsigned int i = 0; i < (sizeof(robot_addresses)/sizeof(robot_addresses[0])); i++){
		printf("robot %d address: %s\n", i, robot_addresses[i]);
	}*/

	// STARTING SHELL
	puts("\nShell running");
	char line_buf[SHELL_DEFAULT_BUFSIZE];
	shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

	// SHOULD NEVER BE REACHED
	return 0;
}

/* DECLARING FUNCTIONS */
int controller_cmd(int argc, char **argv) {
	
	//IF USER DID NOT PUT ROBOT ID
	if ((argc != 2) && (argc != 4)) {
		printf("usage: %s robot_id\n", argv[0]);
		return 1;
	}

	if (argc == 4){
		int robotid = atoi(argv[1]);
		if (robotid <= MAX_ROBOTS - 1) {
			//SET MESSAGE[robot_id] WITH THE COMMAND TO BE SENT
			sprintf(message[robotid], "%s %s %s", argv[0], argv[2], argv[3]);
			return 0;
		}
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
