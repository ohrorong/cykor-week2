#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/wait.h>

#define MAX_CMD_LEN 1024
void my_cd(char *path);  // cd
void my_pwd();           // pwd
void pipeline(char *cmdline); //pipeline
int single_command(char *cmd); //single command

int main() {
    char cmd[MAX_CMD_LEN];
    char cwd[PATH_MAX];

    while (1) {
        getcwd(cwd, sizeof(cwd));
        printf("%s$ ", cwd);

        if (!fgets(cmd, sizeof(cmd), stdin)) {
            perror("fgets");
            continue;
        }


        cmd[strcspn(cmd, "\n")] = 0;


	// exit
     	if (strcmp(cmd, "exit") == 0) break;

        char *rest = cmd;
        char *current;
        int last_status = 0;

        while ((current = strsep(&rest, ";")) != NULL) {

            if (strstr(current, "&&")) {
		char *op = strstr(current, "&&");
		if (op) {
    		*op = '\0';
  		char *left = current;
    		char *right = op + 2;

    		while (*left == ' ') left++;
    		while (*right == ' ') right++;

    		last_status = single_command(left);
    		if (last_status == 0)
        	last_status = single_command(right);
		}
	}

            else if (strstr(current, "||")) {
		char *op = strstr(current, "||");
		if (op) {
    		*op = '\0';
    		char *left = current;
    		char *right = op + 2;

    		while (*left == ' ') left++;
    		while (*right == ' ') right++;

    		last_status = single_command(left);
    		if (last_status != 0 && right)
        		last_status = single_command(right);
		}
	}  else {
                while (*current == ' ') current++;
                last_status = single_command(current);
            }
        }
    }

    return 0;
}

int single_command(char *cmd){

    int background = 0;

    while (*cmd == ' ') cmd++;

    int len = strlen(cmd);
    if (len > 0 && cmd[len - 1] == '&') {
        background = 1;
        cmd[len - 1] = '\0';
        while (len > 1 && cmd[len - 2] == ' ') {
            cmd[len - 2] = '\0';
            len--;
        }
    }

    if (strcmp(cmd, "pwd") == 0) {
        my_pwd();
        return 0;
    }

    if (strncmp(cmd, "cd ", 3) == 0) {
        my_cd(cmd + 3);
        return 0;
    }

    if (strchr(cmd, '|')) {
        pipeline(cmd);
        return 0;
    }

    pid_t pid = fork();
    if (pid == 0) {
        char *argv[] = {"/bin/sh", "-c", cmd, NULL};
        execvp(argv[0], argv);
        perror("exec");
        exit(1);
    } else {
        if (!background) {
            int status;
            waitpid(pid, &status, 0);
            return WEXITSTATUS(status);
        } else {
            printf("[background pid: %d]\n", pid);
            return 0;
        }
    }
}

void my_pwd() {
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));
    printf("%s\n", cwd);
}

void my_cd(char *path) {
    if (chdir(path) != 0) {
        perror("cd error");
    }
}

void pipeline(char *cmdline) {
    char *cmds[16];
    int num_cmds = 0;

    char *token = strtok(cmdline, "|");
    while (token != NULL && num_cmds < 16) {
        while (*token == ' ') token++;
        cmds[num_cmds++] = token;
        token = strtok(NULL, "|");
    }

    int pipefd[2], prev_fd = -1;

    for (int i = 0; i < num_cmds; ++i) {
        pipe(pipefd);
        pid_t pid = fork();

        if (pid == 0) {
            if (i != 0) {
                dup2(prev_fd, STDIN_FILENO);
                close(prev_fd);
            }
            if (i != num_cmds - 1) {
                dup2(pipefd[1], STDOUT_FILENO);
            }
            close(pipefd[0]);
            close(pipefd[1]);

            char *args[] = {"/bin/sh", "-c", cmds[i], NULL};
            execvp(args[0], args);
            perror("exec error");
            exit(1);
        } else {
            int status;
	    waitpid(pid, &status, 0);
            close(pipefd[1]);
            if (prev_fd != -1) close(prev_fd);
            prev_fd = pipefd[0];
        }
    }
}
