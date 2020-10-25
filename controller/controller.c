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
	int missedPings;
    int status;
	int energy;
	int survFoundCount;
	int helpRobotID;
	/* alive - done - helping id - unreachable - kill*/
	char flag[15];
	struct Point position;
	struct Point startPos;
	struct Point endPos;
	struct Point survFoundList[NUM_LINES * NUM_COLUMNS];
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
		int count;
		// IF THERE IS NO COMMAND
		if (strlen(message[id]) == 0) {
			xtimer_sleep(1);
			count = 0;
			continue;

		} else {
			ssize_t res;
			count++;
			if (count > 3){
				if (strcmp(robots[id].flag,"a") == 0) {
					sprintf(robots[id].flag, "u");
				} else if (strcmp(robots[id].flag,"h") == 0) {
					sprintf(robots[id].flag, "k");
				}
				
				return NULL;
			}
			// IF THERE'S AN ERROR CREATING UDP SOCK
			if (sock_udp_create(&sock, &local, NULL, 0) < 0) {
				puts("Error creating UDP sock");
				return NULL;
			}
			
			printf("Sending: %s to robot id: %d\n", message[id], id);

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
					printf("Cannot convert server address\n");
					strcpy(ipv6_addr, "???");
				}

				// ensure a null-terminated string
				buf[res] = 0;

				strcpy(message[id], "");
			}
			sock_udp_close(&sock);
		}
	}

	return NULL;
}

void *listener_thread_handler(void* arg){

	//Here so we can display all messages recieved and are able to recieve spontaneous messages.
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
			if (res == -ETIMEDOUT){
				for (int i = 0; i < numRobots; i++){
					robots[i].missedPings += 1;
					if (robots[i].missedPings > (NUM_COLUMNS / 3) + 3){
						strcpy(message[i], "g");
					}
				}

			} else {
				puts("Error receiving message. This should not happen.");
			}
		}
		else {
			// CONVERT ROBOT ADDRESS FROM ipv6_addr_t TO STRING
			char ipv6_addr[IPV6_ADDR_MAX_STR_LEN];
			if (ipv6_addr_to_str(ipv6_addr, (ipv6_addr_t *)&remote.addr.ipv6, IPV6_ADDR_MAX_STR_LEN ) == NULL) {
				printf("Cannot convert server address\n");
				strcpy(ipv6_addr, "???");
			}

			// ensure a null-terminated string
			buf[res] = 0;
			printf("Received from robot (%d): \"%s\"\n", remote.port, buf);

			int id, energy, x, y, status, sx, sy;

			if ((sscanf((char*) buf, "%d e %d p %d %d d %d", &id, &energy, &x, &y, &status)) == 5) {
				robots[id].status = status;
				robots[id].energy = energy;
				robots[id].position.x = x;
				robots[id].position.y = y;
				robots[id].missedPings = 0;
			} else if ((sscanf((char*) buf, "%d d %d p %d %d", &id, &status, &x, &y)) == 4) {
				robots[id].status = status;
				robots[id].position.x = x;
				robots[id].position.y = y;
				robots[id].missedPings = 0;
			} else if ((sscanf((char*) buf, "%d f %d %d d %d", &id, &sx, &sy, &status)) == 4) {
				robots[id].status = status;
				robots[id].position.x = sx;
				robots[id].position.y = sy;
				robots[id].survFoundList[robots[id].survFoundCount].x = sx;
				robots[id].survFoundList[robots[id].survFoundCount].y = sy;
				robots[id].missedPings = 0;
			}
		}
	}
}

void *logic_thread_handler(void *arg) {
	(void)arg;

	// DECLARING VARIABLES
	bool notFinish = true;
	border.x = NUM_COLUMNS + 1;
	border.y = NUM_LINES + 1;

	int x,y;
	
	// MOVING THE ROBOT TO ITS INITIAL POSITION
	x = 0;
	for (int i = 0; i < numRobots; ++i) {

		y = ((border.y/numRobots)*i);

		// (TELL ROBOT TO GO ITS INITIAL POS) - SET MESSAGE[robot_id] WITH THE COMMAND TO BE SENT
		sprintf(message[i], "p %d %d", x, y);
		robots[i].startPos.x = x;
		robots[i].startPos.y = y;

		if (i != numRobots - 1) {
			robots[i].endPos.x = NUM_COLUMNS;
			robots[i].endPos.y = ((border.y/numRobots)*(i+1))-1;
		} else {
			robots[i].endPos.x = NUM_COLUMNS;
			robots[i].endPos.y = NUM_LINES;
		}

		/* TO BE DELETED ? */
		printf("Robot %d Start.Pos (%d, %d) End.Pos (%d, %d)\n",i, x, y, robots[i].endPos.x, robots[i].endPos.y);
	}

	xtimer_sleep(4);

	// (TELL ALL ROBOT TO GO RIGHT) - SET MESSAGE[robot_id] WITH THE COMMAND TO BE SENT
	for (int i = 0; i < numRobots; ++i) {
		strcpy(message[i], "r");
		xtimer_sleep((border.y/numRobots) + 1);
	}

	while(notFinish) {

		/* alive(a) - done(d) - helping id(d %d) - unreachable(u) - kill(k)*/
		for (int i = 0; i < numRobots; ++i) {

			if (strcmp(robots[i].flag,"a") == 0) {

				// WAIT FOR A MSG WHEN A ROBOT IS STOP
				if (robots[i].status == 0) {

					// CHECK IF ROBOT REACHED X BORDER 
					if ((robots[i].position.x == 0) || (robots[i].position.x == NUM_LINES)) {
						
						// IF IT HASNT REACHED ROBOT'S END SECTION
						if (robots[i].position.y != robots[i].endPos.y) {

							// (TELL ROBOT TO GO DOWN) - SET MESSAGE[robot_id] WITH THE COMMAND TO BE SENT
							sprintf(message[i], "p %d %d", robots[i].position.x, (robots[i].position.y + 1));

							xtimer_sleep(4);

							if (robots[i].status == 0)
							{
								// IF IT REACHED MAX LEFT
								if (robots[i].position.x == 0) {

									// (TELL ROBOT TO GO RIGHT) - IF IT REACHED MAX LEFT
									strcpy(message[i], "r");
								}
								// IF IT REACHED MAX RIGHT
								if (robots[i].position.x == NUM_LINES) {

									// (TELL ROBOT TO GO LEFT) - IF IT REACHED MAX LEFT
									strcpy(message[i], "l");
								}
							}
						} else {
							sprintf(robots[i].flag, "d");
						}
					}
				}
			} else if (strcmp(robots[i].flag,"d") == 0) {

				// FOR EVERY ROBOTS
				for (int j = 0; j < numRobots; ++j) {
					// IF ITS NOT ITSELF
					if (i != j) {
						// IF ROBOT IS STILL NOT HELPING ANOTHER ROBOT
						if (strcmp(robots[i].flag,"d") == 0) {
							// IF THE J ROBOT IS UNREACHABLE - HELP
							if (strcmp(robots[j].flag,"u") == 0) {

								// MAKE U TO K AND D TO S
								sprintf(robots[j].flag, "k");
								sprintf(robots[i].flag, "s");
								robots[i].helpRobotID = j;

								robots[i].endPos.x = robots[j].position.x;
								robots[i].endPos.y = robots[j].position.y;
							}
						}
					}
				}
			} else if (strcmp(robots[i].flag, "s") == 0){

				// MOVING THE ROBOT TO UNREACHABLE'S ROBOT'S END POINT
				x = robots[robots[i].helpRobotID].endPos.x;
				y = robots[robots[i].helpRobotID].endPos.y;
				sprintf(message[i], "p %d %d", x, y);
				strcpy(robots[i].flag, "h");
				xtimer_sleep(4);

			} else if (strcmp(robots[i].flag,"h") == 0) {
				
				// WAIT FOR A MSG WHEN A ROBOT IS STOP
				if (robots[i].status == 0) {

					// CHECK IF ROBOT REACHED X BORDER 
					if ((robots[i].position.x == 0) || (robots[i].position.x == NUM_LINES)) {

						// IF IT HASNT REACHED ROBOT'S END SECTION
						if (robots[i].position.y != robots[i].endPos.y) {

							// (TELL ROBOT TO GO DOWN) - SET MESSAGE[robot_id] WITH THE COMMAND TO BE SENT
							sprintf(message[i], "p %d %d", robots[i].position.x, (robots[i].position.y - 1));
							xtimer_sleep(4);

							if (robots[i].status == 0) {

								// IF IT REACHED MAX LEFT
								if (robots[i].position.x == 0) {

									// (TELL ROBOT TO GO RIGHT) - IF IT REACHED MAX LEFT
									strcpy(message[i], "r");
								}
								// IF IT REACHED MAX RIGHT
								if (robots[i].position.x == NUM_LINES) {

									// (TELL ROBOT TO GO LEFT) - IF IT REACHED MAX LEFT
									strcpy(message[i], "l");
								}
							}
						} else {
							sprintf(robots[i].flag, "d");
						}
					}
				}
			}
		}

		notFinish = false;
		for (int i = 0; i < numRobots; ++i) {

			// IF THERES STILL ROBOT
			if ((strcmp(robots[i].flag,"h") == 0) || (strcmp(robots[i].flag,"a") == 0)) {
				notFinish = true;
			}
		}
		xtimer_sleep(1);
	}

	// IF EVERY ROBOT IS DEAD
	printf("Found Survivors\n");
	for (int i = 0; i < numRobots; ++i) {

		// PRINT SURVIVOR'S COORD
		printf("Robot %d found:\n", i);

		for (unsigned int j = 0; j < (sizeof(robots[i].survFoundList)/sizeof(robots[i].survFoundList[0])); ++j)
		{
			if (robots[i].survFoundList[j].x == 0 && robots[i].survFoundList[j].y == 0){
				//printf("Count: %d\n", i);
				break;
			}

			// PRINT SURVIVOR'S COORD
			printf("\t Survivor %d: (%d, %d)\n", j, robots[i].survFoundList[j].x, robots[i].survFoundList[j].y);
		}
	}
	return NULL;
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
		sprintf(robots[robotID].flag, "a");

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
	puts("\nShell running\n");
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
