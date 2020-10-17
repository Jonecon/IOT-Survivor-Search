// make PORT=tap1 all term
// make PORT=tap2 all term

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
	printf("\n id: %d", id);

	while (1) {
		if (strlen(message[id]) == 0) {
			xtimer_sleep(1);
			continue;
		}

		// printf("sending: %s\n", message[id] );
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


