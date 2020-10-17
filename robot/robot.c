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
//extern int sDown_cmd(int argc, char **argv);
//extern int sLeft_cmd(int argc, char **argv);
//extern int sRight_cmd(int argc, char **argv);
//extern int stop_cmd(int argc, char **argv);
extern int getSta_cmd(int argc, char **argv);
extern int getSta_cmd_remote(char* response);

//Declaring variables
uint8_t buf[16];
int moving_direction;
int energy;
int num_survivors;
struct Point position;
struct Point survivors_list[3];
struct Point survivors_found[3];
struct Point border;


static const shell_command_t shell_commands[] = {
	{"sUp", "Starts the robot moving in the direction UP", sUp_cmd},
	//{"sDown", "", sDown_cmd},
	//{"sLeft", "", sLeft_cmd},
	//{"sRight", "", sRight_cmd},
	//{"stop", "", stop_cmd},
	//{"getSta", "", getSta_cmd},
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

		//Setup communication thread


		//Setup robot logic loop


    puts("Shell running");
		char line_buf[SHELL_DEFAULT_BUFSIZE];
		shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);
    return 0;
}

int sUp_cmd(int argc, char **argv){
	if (argc != 1){
		printf("usage: %s\n", argv[0]);
		return 1;
	}

	printf("Moving UP\n");
	moving_direction = DIRECTION_UP;
	return 0;
}

int sUp_cmd_remote(char* response){
	moving_direction = DIRECTION_UP;
	getSta_cmd_remote(response);
	return 0;
}


int getSta_cmd_remote(char* response){
	char str[128];
	int i;
	sprintf(str, "STATUS robot ENERGY %d at (%d, %d) and direction %d, survivors: ", energy, position.x, position.y, moving_direction);
	strcat(response, str);
	for (i=0; i<num_survivors && survivors_list[i].x != NUM_LINES; i++){
		sprintf(str, "(%d, %d)", survivors_found[i].x, survivors_found[i].y);
		strcat(response, str);
	}
	return 0;
}
