// make all term PORT=tap0

#include <stdio.h>
#include "net/sock/udp.h"
#include "shell.h"
#include "thread.h"
#include "xtimer.h"
#include "msg.h"
#include "net/gnrc/netreg.h"
#include "net/gnrc/pktdump.h"

#define DIRECTION_UP 1;
#define DIRECTION_DOWN 2;
#define DIRECTION_LEFT 3;
#define DIRECTION_RIGHT 4;
#define DIRECTION_STOP 0;

struct Point {
	int x;
	int y;
};

//Declaring functions
extern int _gnrc_netif_config(int argc, char **argv);
extern int sUp_cmd(int argc, char **argv);
extern int sDown_cmd(int argc, char **argv);
extern int sLeft_cmd(int argc, char **argv);
extern int sRight_cmd(int argc, char **argv);
extern int stop_cmd(int argc, char **argv);
extern int getSta_cmd(int argc, char **argv);
extern int getSta_cmd_remote(char* response);
extern int sUp_cmd_remote(char* response);
void *robot_communications_thread_handler(void *arg);
void *robot_logic_thread_handler(void *arg);

//Declaring variables
uint8_t buf[16];
int moving_direction;
int energy;
int num_survivors;
char com_thread_stack[THREAD_STACKSIZE_MAIN];
char logic_thread_stack[THREAD_STACKSIZE_MAIN];
struct Point position;
struct Point survivors_list[3];
struct Point survivors_found[3];
struct Point mines_list[MAX_MINES];
struct Point border;


static const shell_command_t shell_commands[] = {
	{"sUp", "Starts the robot moving in the direction UP", sUp_cmd},
	{"sDown", "", sDown_cmd},
	{"sLeft", "", sLeft_cmd},
	{"sRight", "", sRight_cmd},
	{"stop", "", stop_cmd},
	{"getSta", "", getSta_cmd},
	{NULL, NULL, NULL}
};



int main(void)
{
		//Setup variables to map
		energy = ENERGY;
		position.x = 10;
		position.y = 10;
		border.x = NUM_LINES;
		border.y = NUM_COLUMNS;

		//Setting up survivors pos variable.
		char str[NUM_LINES * NUM_COLUMNS];
		strcpy(str, SURVIVOR_LIST);
		char* token = strtok(str, ",");
		int id = 0;
		while (token != NULL){
			survivors_list[id].x = atoi(token);
			token = strtok(NULL, ",");
			survivors_list[id].y = atoi(token);
			id++;
			token = strtok(NULL, ",");
		}
		num_survivors = id;

		//Setting up mines pos variable
		strcpy(str, MINES_LIST);
		token = strtok(str, ",");
		id = 0;
		while (token != NULL){
			mines_list[id].x = atoi(token);
			token = strtok(NULL, ",");
			mines_list[id].y = atoi(token);
			id++;
			token = strtok(NULL, ",");
		}

		//Setup communication thread
		thread_create(com_thread_stack, sizeof(com_thread_stack), THREAD_PRIORITY_MAIN - 1,
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
  while (1) {
    // configure a remote address, which will be filled in by recv from the arriving message
    sock_udp_ep_t remote;

    // wait for a message from the client, no more than 3s
    // to wait forever, use SOCK_NO_TIMEOUT
    res = sock_udp_recv(&sock, buf, sizeof(buf), 3 * US_PER_SEC, &remote);
    if (res >= 0) {
    	// convert IPv6 address from client to string to display on console
      char ipv6_addr_str[IPV6_ADDR_MAX_STR_LEN];
      if (ipv6_addr_to_str(ipv6_addr_str, (ipv6_addr_t *)&remote.addr.ipv6, IPV6_ADDR_MAX_STR_LEN ) == NULL) {
        /* Cannot convert client address */
        strcpy(ipv6_addr_str, "???");
      }
      buf[res] = 0; // ensure null-terminated string
      printf("Received from (%s, %d): \"%s\"\t", ipv6_addr_str, remote.port, (char*) buf);

			if (strcmp((char*) buf, "sUp") == 0){
				xtimer_sleep(1);
				sUp_cmd_remote((char*) buf);
				xtimer_sleep(1);
	      printf("sending back %s\n", (char*) buf);
				xtimer_sleep(1);
	      if (sock_udp_send(&sock, buf, strlen((char*)buf)+1, &remote) < 0) {
	        puts("\nError sending reply to client");
	      }
			}
	  }
	  else {
	      if (res == -ETIMEDOUT) {
          // this is the case when the receive "failed" because there was no message to be received
          // within the time interval given
          puts("timeout, no incoming PING available, just wait");
	      }
	      else {
	        puts("\nError receiving message");
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
		switch (moving_direction) {
			case 1:
				if (position.y == 0){
					moving_direction = DIRECTION_STOP;
					break;
				}
				position.y -= 1;
				printf("(%d, %d)", position.x, position.y);
				break;

			case 2:
				if (position.y == border.y){
					moving_direction = DIRECTION_STOP;
					break;
				}
				position.y += 1;
				printf("(%d, %d)", position.x, position.y);
				break;

			case 3:
				if (position.x == 0){
					moving_direction = DIRECTION_STOP;
					break;
				}
				position.x -= 1;
				printf("(%d, %d)", position.x, position.y);
				break;

			case 4:
				if (position.x == border.x){
					moving_direction = DIRECTION_STOP;
					break;
				}
				position.x += 1;
				printf("(%d, %d)", position.x, position.y);
				break;

			case 0:
				printf("(%d, %d (%s))", position.x, position.y, "stationary");
				break;
		}
		printf("Direction: %d\n", moving_direction);
	}
	return NULL;
}

int sUp_cmd(int argc, char **argv){
	if (argc != 1){
		printf("usage: %s\n", argv[0]);
		return 1;
	}

	moving_direction = DIRECTION_UP;
	return 0;
}


int sDown_cmd(int argc, char **argv){
	if (argc != 1){
		printf("usage: %s\n", argv[0]);
		return 1;
	}

	moving_direction = DIRECTION_DOWN;
	return 0;
}


int sLeft_cmd(int argc, char **argv){
	if (argc != 1){
		printf("usage: %s\n", argv[0]);
		return 1;
	}

	moving_direction = DIRECTION_LEFT;
	return 0;
}

int sRight_cmd(int argc, char **argv){
	if (argc != 1){
		printf("usage: %s\n", argv[0]);
		return 1;
	}

	moving_direction = DIRECTION_RIGHT;
	return 0;
}


int stop_cmd(int argc, char **argv){
	if (argc != 1){
		printf("usage: %s\n", argv[0]);
		return 1;
	}

	moving_direction = DIRECTION_STOP;
	return 0;
}

int getSta_cmd(int argc, char **argv){
	if (argc != 1){
		printf("usage: %s\n", argv[0]);
		return 1;
	}

	printf("Energy: %d, Position: (%d, %d)\n", energy, position.x, position.y);
	return 0;
}

int getSta_cmd_remote(char* response){
	//char str[128];
	//int i;
	sprintf(response, "STATUS asdfsadfsadfasdfsadfsafdsafd");
	//strcat(response, str);
	//for (i=0; i<num_survivors && survivors_list[i].x != NUM_LINES; i++){
	//	sprintf(str, "(%d, %d)", survivors_found[i].x, survivors_found[i].y);
	//	strcat(response, str);
	//}
	return 0;
}

int sUp_cmd_remote(char* response){
	moving_direction = DIRECTION_UP;
	getSta_cmd_remote(response);
	return 0;
}

int sDown_cmd_remote(char* response){
	moving_direction = DIRECTION_DOWN;
	getSta_cmd_remote(response);
	return 0;
}

int sLeft_cmd_remote(char* response){
	moving_direction = DIRECTION_LEFT;
	getSta_cmd_remote(response);
	return 0;
}

int sRight_cmd_remote(char* response){
	moving_direction = DIRECTION_RIGHT;
	getSta_cmd_remote(response);
	return 0;
}

int stop_cmd_remote(char* response){
	moving_direction = DIRECTION_STOP;
	getSta_cmd_remote(response);
	return 0;
}
