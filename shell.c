#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <readline/readline.h>

#define MAXCOM 80 // max 80 chars in line
#define MAXLIST 100 // max num of commands to be supported
#define USER getenv("USER")
#define RED "\033[0;31m"
#define CYAN "\033[0;36m"
#define GREEN "\033[0;32m"
#define BLUE "\033[0;34m"
#define BLUE_BOLD "\033[1;34m"
#define RESET "\033[0m"

// Function for signal handling
void sigint_handler(int sig){
	write(1, "\nTerminating through signal handler\n", 37);
	exit(0);
}


// Shell startup
void minishell_start(){
	printf("%s******WELCOME TO MINI-SHELL******\n%s", RED, RESET);
	// get current user and machine name
	char hostname[25];
	gethostname(hostname, 26);
	printf("%sUSER: %s%s@%s\n", CYAN, RESET, USER, hostname);
}

// Function to take commands
int minishell_takeInput(char* line){
	// buffer string
	char* buf;
	// combine for prompt
	char prompt[80] = "\033[1;32m";
	char prompt2[] = "@mini-shell>\033[0m";
	strcat(prompt, USER);
	strcat(prompt, prompt2);
	// read commands into buffer
	buf = readline(prompt);
	if (strlen(buf) != 0){
		strcpy(line, buf);
		return 0;
	} else {
		return 1;
	}
}

// Function to execute arguments
void minishell_execArgs(char** parsed){
	// execute basic UNIX commands
	// fork child
	pid_t pid = fork();

	// check if fork worked
	if (pid == -1){
		printf("\nForking failed");
		return;
	} else if (pid == 0){
		if (execvp(parsed[0], parsed) < 0){
			printf("\nFailed to execute command\n");
		}
		exit(0);
	} else {
		// wait for child to finish
		wait(NULL);
		return;
	}
}

// Function to handle piped commands
void minishell_execPiped(char** parsed, char** parsedPipe){
	// 0 is the read, 1 is write
	int pipefd[2];
	// create two process ID's
	pid_t p1, p2;
	// child statuses
	int p1_status, p2_status;
	
	if (pipe(pipefd) < 0){
		printf("\nPipe was unsuccessful\n");
		return;
	}

	// fork first child (run first command)
	p1 = fork();
	// check for success
	if (p1 < 0){
		printf("\nCouldn't fork child\n");
		return;
	}

	// if child is forked succesfully
	if (p1 == 0){
		// child proc 1 exec
		// close read
		close(pipefd[0]);
		// duplicate the output to STDOUT
		dup2(pipefd[1], STDOUT_FILENO);
		// close write
		close(pipefd[1]);

		// check for failed first command + exit
		if (execvp(parsed[0], parsed) < 0){
			printf("\nCommand 1 failed to execute\n");
			exit(0);
		}
	} else {
		// parent executing here
		p2 = fork();

		if (p2 < 0){
			printf("\nCouldn't fork child\n");
			return;
		}

		// child proc 2 exec
		if (p2 == 0){
			// close the write end
			close(pipefd[1]);
			// dup the read
			dup2(pipefd[0], STDIN_FILENO);
			// close the read
			close(pipefd[0]);

			if (execvp(parsedPipe[0], parsedPipe) < 0){
				printf("\nCommand 2 failed to execute\n");
				exit(0);
			}
		} else {
			// parent executing, wait for child 1 2
			// close parent read and write
			close(pipefd[0]);
			close(pipefd[1]);
			waitpid(p1, &p1_status, NULL);
			waitpid(p2, &p2_status, NULL);
		}
	}
}

// Help command builtin
void minishell_openHelp(){
	printf("\n%s***MINI-SHELL HELP***%s"
		 "\n%sSupported Commands:%s"
		 "\n%sAll basic UNIX commands: use 'man' for more help on these"
		 "\n%sBuilt-in Commands:"
		 "\n%sexit: exits the shell"
		 "\nhelp: brings up this prompt"
		 "\ncd: change directory like UNIX"
		 "\nsys: output system information\n%s"
		 , RED, RESET, CYAN, RESET, GREEN, BLUE, BLUE_BOLD, RESET);
	return;
}


// Function to execute builtin commands
int minishell_commandHandler(char** parsed){
	int commands_count = 4;
	int i;
	int switchArg = 0;
	char* list_of_builtins[commands_count];
	char* username;

	list_of_builtins[0] = "exit";
	list_of_builtins[1] = "help";
	list_of_builtins[2] = "cd";
	list_of_builtins[3] = "sys";

	for (i=0; i< commands_count; i++){
		if (strcmp(parsed[0], list_of_builtins[i]) == 0){
			switchArg = i + 1;	
			break;
		}
	}

	// switch statement to handle builtins
	switch (switchArg){
	case 1:
		printf("%sSuccessfully exited mini-shell\n%s", RED, RESET);
		exit(0);
	case 2:
		minishell_openHelp();
		return 1;
	case 3:
		chdir(parsed[1]);
		return 1;
	case 4:
		username = getenv("USER");
		char machine[25];
		char* path = getenv("PATH");
		char* home = getenv("HOME");
		char* term = getenv("TERM");	
		char* desktop = getenv("XDG_CURRENT_DESKTOP");
		gethostname(machine, 26);
		printf("%sCURRENT SYSTEM INFORMATION:\n", RED);
		printf("%sUser: %s%s@%s\n", CYAN, RESET, username, machine);
		printf("%sHome: %s%s\n", CYAN, RESET, home);
		printf("%sDesktop: %s%s\n", CYAN, RESET, desktop);
		printf("%sPath: %s%s\n", CYAN, RESET, path);
		printf("%sTerminal: %s%s\n", CYAN, RESET, term);
		return 1;
	default:
		break;
	}

	return 0;
}

// Function to parse for pipe
int minishell_parsePipe(char* line, char** linePiped){
	int i;
	for (i=0; i<2; i++){
		linePiped[i] = strsep(&line, "|");
		if (linePiped[i] == NULL){
			break;
		}
	}

	if (linePiped[1] == NULL){
		return 0; // no pipe found
	} else {
		return 1; // pipe found
	}
}

// Function to parse commands
void minishell_parseSpace(char* line, char** parsed){
	int i;

	for (i=0; i< MAXLIST; i++){
		parsed[i] = strsep(&line, " ");

		if (parsed[i] == NULL){
			break;
		}
		if (strlen(parsed[i]) == 0){
			i--;
		}
	}
}

// Function to process the commands
int minishell_processLine(char* line, char** parsed, char** parsedPipe){
	char* strPiped[2];
	int piped = 0;

	// pipe will be 0 or 1
	piped = minishell_parsePipe(line, strPiped);

	if (piped){
		// if piped == 1
		minishell_parseSpace(strPiped[0], parsed);
		minishell_parseSpace(strPiped[1], parsedPipe);
	} else{
		// piped == 0
		minishell_parseSpace(line, parsed);
	}

	if (minishell_commandHandler(parsed)){
		return 0;
	} else {
		return 1 + piped;
	}	
}


int main(){
	// install signal handler
	signal(SIGINT, sigint_handler);

	char readLine[MAXCOM];
	char* parsedArgs[MAXLIST];
	char* parsedArgsPiped[MAXLIST];
	int flag = 0;
	minishell_start();
	
	// loop for commands
	while (1){
		if (minishell_takeInput(readLine)){
			continue;
		}

		flag = minishell_processLine(readLine, parsedArgs, parsedArgsPiped);
		// execute function for no pipe
		if (flag == 1){
			minishell_execArgs(parsedArgs);
		}
		// execute function with a pipe
		if (flag == 2){
			minishell_execPiped(parsedArgs, parsedArgsPiped);
		}
	}

	return 0;
}
