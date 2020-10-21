/* make all PORT=tap10 ROBOT_ID=1 ROBOT_PORT=10001 term */

#include <stdio.h>
#include "net/sock/udp.h"
#include "shell.h"
#include "thread.h"
#include "xtimer.h"
#include "msg.h"
#include "net/gnrc/netreg.h"
#include "net/gnrc/pktdump.h"
#include "mutex.h"

#define DIRECTION_STOP 0;
#define DIRECTION_UP 1;
#define DIRECTION_DOWN 2;
#define DIRECTION_LEFT 3;
#define DIRECTION_RIGHT 4;
#define DIRECTION_POS 5;

struct Point {
	int x;
	int y;
};

typedef struct {
    int direction;
		int energy;
		struct Point position;
		struct Point destination;
		//Hard coded need to change.
		struct Point survivors_list[3];
		struct Point survivors_found[3];
		struct Point mines_list[MAX_MINES];
		mutex_t lock;
} data_t;

static data_t data;

//Declaring functions
extern int _gnrc_netif_config(int argc, char **argv);
extern int sUp_cmd(int argc, char **argv);
extern int sDown_cmd(int argc, char **argv);
extern int sLeft_cmd(int argc, char **argv);
extern int sRight_cmd(int argc, char **argv);
extern int stop_cmd(int argc, char **argv);
extern int getSta_cmd(int argc, char **argv);
extern int setPosition_cmd(int argc, char **argv);
extern int getSta_cmd_remote(char* response);
extern int sUp_cmd_remote(char* response);
extern int sDown_cmd_remote(char* response);
extern int sRight_cmd_remote(char* response);
extern int sLeft_cmd_remote(char* response);
extern int stop_cmd_remote(char* response);
extern int setPosition_cmd_remote(char* response);
void *robot_communications_thread_handler(void *arg);
void *robot_logic_thread_handler(void *arg);

//Declaring variables
uint8_t buf[16];
int num_survivors;
char com_thread_stack[THREAD_STACKSIZE_MAIN];
char logic_thread_stack[THREAD_STACKSIZE_MAIN];
struct Point border;
char message[20];
int id;
int sentCount;

static const shell_command_t shell_commands[] = {
	{"sUp", "Starts the robot moving in the direction UP", sUp_cmd},
	{"sDown", "Starts the robot moving in the direction DOWN", sDown_cmd},
	{"sLeft", "Starts the robot moving in the direction LEFT", sLeft_cmd},
	{"sRight", "Starts the robot moving in the direction RIGHT", sRight_cmd},
	{"stop", "Stops the robot moving", stop_cmd},
	{"getSta", "Gets the status of the robot", getSta_cmd},
	{"pos", "Moves the robot to a specific coordinate", setPosition_cmd},
	{NULL, NULL, NULL}
};

int main(void){
		//Setup variables to map
		data.energy = ENERGY;
		printf("%d\n", data.energy);
		data.position.x = 0;
		data.position.y = 0;
		data.destination.x = 0;
		data.destination.y = 0;
		border.x = NUM_LINES;
		border.y = NUM_COLUMNS;
		id = ROBOT_ID;
		sentCount = 0;

		//Setting up survivors pos variable.
		char str[NUM_LINES * NUM_COLUMNS];
		strcpy(str, SURVIVOR_LIST);
		char* token = strtok(str, ",");
		int id = 0;
		while (token != NULL){
			data.survivors_list[id].x = atoi(token);
			token = strtok(NULL, ",");
			data.survivors_list[id].y = atoi(token);
			id++;
			token = strtok(NULL, ",");
		}
		num_survivors = id;

		//Setting up mines pos variable
		strcpy(str, MINES_LIST);
		token = strtok(str, ",");
		id = 0;
		while (token != NULL){
			data.mines_list[id].x = atoi(token);
			token = strtok(NULL, ",");
			data.mines_list[id].y = atoi(token);
			id++;
			token = strtok(NULL, ",");
		}

		//Setup communication thread
		thread_create(com_thread_stack, sizeof(com_thread_stack), THREAD_PRIORITY_MAIN - 3,
			THREAD_CREATE_STACKTEST, robot_communications_thread_handler,
			NULL, "communication thread");

		//Setup robot logic loop thread
		thread_create(logic_thread_stack, sizeof(logic_thread_stack), THREAD_PRIORITY_MAIN - 1,
			THREAD_CREATE_STACKTEST, robot_logic_thread_handler,
			NULL, "logic thread");

    puts("Shell running");
		char line_buf[SHELL_DEFAULT_BUFSIZE];
		shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);
    return 0;
}

void *robot_communications_thread_handler(void *arg){
	(void)arg;

	//Wait for a message to be recieved and then send a response
	// print network addresses. this is useful to confirm the right addresses are being used
	puts("Configured network interfaces:");
	_gnrc_netif_config(0, NULL);

	// configure a local IPv6 address and set port to be used by this server
	sock_udp_ep_t local = SOCK_IPV6_EP_ANY;
	local.port = ROBOT_PORT;
	// create and bind sock to local address
	sock_udp_t sock;

	if (sock_udp_create(&sock, &local, NULL, 0) < 0) {
		puts("Error creating UDP sock");
		return NULL;
	}

	// the server is good to go
	printf("server waiting on port %d\n", local.port);

	ssize_t res;
	//Remote address which will be filled in by the recv function.
	sock_udp_ep_t remote;
	while (1) {
		// wait for a message from the client, no more than 3s
		// to wait forever, use SOCK_NO_TIMEOUT
		res = sock_udp_recv(&sock, buf, sizeof(buf), 3 * US_PER_SEC, &remote);
		if (res >= 0) {
			sentCount = 0;
			// convert IPv6 address from client to string to display on console
			char ipv6_addr_str[IPV6_ADDR_MAX_STR_LEN];

			if (ipv6_addr_to_str(ipv6_addr_str, (ipv6_addr_t *)&remote.addr.ipv6, IPV6_ADDR_MAX_STR_LEN ) == NULL) {
				/* Cannot convert client address */
				strcpy(ipv6_addr_str, "???");
			}

			buf[res] = 0; // ensure null-terminated string
			printf("\nReceived from (%s, %d): \"%s\"\t\n", ipv6_addr_str, remote.port, (char*) buf);


			//WILL BE CHANGING TO TO SMALLER MESSAGE SIZE INDICATORS
			if (strcmp((char*) buf, "u") == 0) {

				sUp_cmd_remote((char*) buf);
				printf("Sending back %s\n", (char*) buf);
				if (sock_udp_send(&sock, buf, strlen((char*)buf), &remote) < 0) {
					puts("\nError sending reply to client");
				}
				strcpy(message, "");

			} else if (strcmp((char*) buf, "d") == 0) {

				sDown_cmd_remote((char*) buf);
				printf("Sending back %s\n", (char*) buf);
				if (sock_udp_send(&sock, buf, strlen((char*)buf), &remote) < 0) {
					puts("\nError sending reply to client");
				}
				strcpy(message, "");

			} else if (strcmp((char*) buf, "l") == 0) {

				sLeft_cmd_remote((char*) buf);
				printf("Sending back %s\n", (char*) buf);
				if (sock_udp_send(&sock, buf, strlen((char*)buf), &remote) < 0) {
					puts("\nError sending reply to client");
				}
				strcpy(message, "");

			} else if (strcmp((char*) buf, "r") == 0) {

				sRight_cmd_remote((char*) buf);
				printf("Sending back %s\n", (char*) buf);
				if (sock_udp_send(&sock, buf, strlen((char*)buf), &remote) < 0) {
					puts("\nError sending reply to client");
				}
				strcpy(message, "");

			} else if (strcmp((char*) buf, "s") == 0) {

				stop_cmd_remote((char*) buf);
				printf("Sending back %s\n", (char*) buf);
				if (sock_udp_send(&sock, buf, strlen((char*)buf), &remote) < 0) {
					puts("\nError sending reply to client");
				}

			} else if (strcmp((char*) buf, "g") == 0) {

				getSta_cmd_remote((char*) buf);
				printf("Sending back %s\n", (char*) buf);
				if (sock_udp_send(&sock, buf, strlen((char*)buf), &remote) < 0) {
					puts("\nError sending reply to client");
				}
			} else {
			 	int x, y;
				if ((sscanf((char*) buf, "p %d %d", &x, &y)) != -1){
					setPosition_cmd_remote((char*) buf);
					printf("Sending back %s\n", buf);
					if (sock_udp_send(&sock, buf, strlen((char*) buf), &remote) < 0){
						puts("\nError sending reply to client");
					}
				}
				strcpy(message, "");
			}


			res = -1;
		}
		else {

			if (res == -ETIMEDOUT) {
				// this is the case when the receive "failed" because there was no message to be received
				// within the time interval given
				//puts("timeout, no incoming PING available, just wait");
			}
			else {
				puts("\nError receiving message");
			}
		}

		if (strlen(message) != 0 && sentCount < 1){
			printf("\nSending: %s", message);
			// TRANSMITTING THE MESSAGE TO THE ROBOT[id] WHILE CHECKING
			if (sock_udp_send(&sock, message, strlen(message) + 1, &remote) < 0) {
				puts("Error sending message");
				sentCount += 1;
			}else{
				strcpy(message, "");
			}
		}
	}
	return NULL;
}

void *robot_logic_thread_handler(void *arg){
	(void)arg;
	while(1){
		//Wait 1 second and then move.
		xtimer_sleep(1);
		mutex_lock(&data.lock);
		if (data.energy <= 0 && data.direction != 0){
			printf("%s\n", "BATTERY DEAD");
			while(1){}
		}
		switch (data.direction) {
			case 1:
				if (data.position.y == 0){
					data.direction = DIRECTION_STOP;
					break;
				}
				data.position.y -= 1;
				printf("(%d, %d)", data.position.x, data.position.y);
				break;

			case 2:
				if (data.position.y == border.y){
					data.direction = DIRECTION_STOP;
					break;
				}
				data.position.y += 1;
				printf("(%d, %d)", data.position.x, data.position.y);
				break;

			case 3:
				if (data.position.x == 0){
					data.direction = DIRECTION_STOP;
					break;
				}
				data.position.x -= 1;
				printf("(%d, %d)", data.position.x, data.position.y);
				break;

			case 4:
				if (data.position.x == border.x){
					data.direction = DIRECTION_STOP;
					break;
				}
				data.position.x += 1;
				printf("(%d, %d)", data.position.x, data.position.y);
				break;

			case 0:
				if (!(data.position.x == 0 && data.position.y == 0)){
					sprintf(message, "%d d 0 p %d %d", id, data.position.x, data.position.y);
				}
				printf("(%d, %d)", data.position.x, data.position.y);
				//printf("stationary with message: %s\n", message);
				break;

			case 5:
				if (data.destination.x < 0 || data.destination.x > border.x || data.destination.y > border.y || data.destination.y < 0){
					printf("The coords (%d, %d) are out of bounds\n", data.destination.x, data.destination.y);
					data.direction = DIRECTION_STOP;
					//Reply to the server these are out of bounds.
					break;
				}
				if (data.position.x != data.destination.x){
					if (data.position.x > data.destination.x){
						data.position.x -= 1;
					}else{
						data.position.x += 1;
					}
				}else if (data.position.y != data.destination.y){
					if (data.position.y > data.destination.y){
						data.position.y -= 1;
					}else{
						data.position.y += 1;
					}
				}else{
					data.direction = DIRECTION_STOP;
					//Tell server we have reached desination.
				}
				printf("(%d, %d)", data.position.x, data.position.y);
				break;
		}
		//If we are on a survivor ALSO HARD CODED NEEDS TO CHANGE
		for (int i = 0; i < 3; i++){
			if (data.position.x == data.survivors_list[i].x && data.position.y == data.survivors_list[i].y){
				printf("\n%s\n", "FOUND SURVIVOR");
				data.survivors_found[i].x = data.position.x;
				data.survivors_found[i].y = data.position.y;
				sprintf(message, "%d f %d %d d %d", id, data.position.x, data.position.y, data.direction);
			}
		}

		//If we landed on a bomb
		for (int i = 0; i < MAX_MINES; i++){
			if (data.mines_list[i].x == 0 && data.mines_list[i].y == 0){
				break;
			}
			if (data.position.x == data.mines_list[i].x && data.position.y == data.mines_list[i].y){
				printf("\n%s\n", "DEAD");
				while (1){}
			}
		}
		if (data.direction != 0){
			data.energy -= 1;
		}
		mutex_unlock(&data.lock);
	}
	return NULL;
}

int sUp_cmd(int argc, char **argv){
	if (argc != 1){
		printf("usage: %s\n", argv[0]);
		return 1;
	}

	data.direction = DIRECTION_UP;
	return 0;
}


int sDown_cmd(int argc, char **argv){
	if (argc != 1){
		printf("usage: %s\n", argv[0]);
		return 1;
	}

	data.direction = DIRECTION_DOWN;
	return 0;
}


int sLeft_cmd(int argc, char **argv){
	if (argc != 1){
		printf("usage: %s\n", argv[0]);
		return 1;
	}

	data.direction = DIRECTION_LEFT;
	return 0;
}

int sRight_cmd(int argc, char **argv){
	if (argc != 1){
		printf("usage: %s\n", argv[0]);
		return 1;
	}

	data.direction = DIRECTION_RIGHT;
	return 0;
}


int stop_cmd(int argc, char **argv){
	if (argc != 1){
		printf("usage: %s\n", argv[0]);
		return 1;
	}

	data.direction = DIRECTION_STOP;
	return 0;
}

int getSta_cmd(int argc, char **argv){
	if (argc != 1){
		printf("usage: %s\n", argv[0]);
		return 1;
	}

	printf("energy: %d, Position: (%d, %d)\n", data.energy, data.position.x, data.position.y);
	return 0;
}

int setPosition_cmd(int argc, char **argv){
	if (argc != 3){
		printf("usage: pos X Y\n");
		return 1;
	}
	data.direction = DIRECTION_POS;
	data.destination.x = atoi(argv[1]);
	data.destination.y = atoi(argv[2]);
	return 0;
}

int getSta_cmd_remote(char* response){
	mutex_lock(&data.lock);
	sprintf(response, "%d e %d p %d %d d %d", id, data.energy, data.position.x, data.position.y, data.direction);
	mutex_unlock(&data.lock);
	return 0;
}

int sUp_cmd_remote(char* response){
	data.direction = DIRECTION_UP;
	getSta_cmd_remote(response);
	return 0;
}

int sDown_cmd_remote(char* response){
	data.direction = DIRECTION_DOWN;
	getSta_cmd_remote(response);
	return 0;
}

int sLeft_cmd_remote(char* response){
	data.direction = DIRECTION_LEFT;
	getSta_cmd_remote(response);
	return 0;
}

int sRight_cmd_remote(char* response){
	data.direction = DIRECTION_RIGHT;
	getSta_cmd_remote(response);
	return 0;
}

int stop_cmd_remote(char* response){
	data.direction = DIRECTION_STOP;
	getSta_cmd_remote(response);
	return 0;
}

int setPosition_cmd_remote(char* response){
	data.direction = DIRECTION_POS;
	sscanf(response, "p %d %d\n", &data.destination.x, &data.destination.y);
	getSta_cmd_remote(response);
	return 0;
}
