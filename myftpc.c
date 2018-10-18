#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>

#include "myftp.h"

struct command_table {
	char *cmd;
	void (*func)(int, int, char *[]);
} cmd_tbl[] = {
	{"quit", quit_proc},
	{"pwd", pwd_proc},
	{"cd", cd_proc},
	{"dir", dir_proc},
	{"lpwd", lpwd_proc},
	{"lcd", lcd_proc},
	{"ldir", ldir_proc},
	{"get", get_proc},
	{"put", put_proc},
	{"help", help_proc},
	{NULL, NULL}
};

struct myftph msg;
struct myftph_data msg_data;

int main(int argc, char *argv[])
{
	struct addrinfo hints, *res;
	char *node, buf[80];
	int sd, err;

	struct command_table *p;
   	char *av[NARGS];
	int ac;
	
	char serv[] = SRVPORT;
	node = argv[1];
	
	if (argc != 2) {
		fprintf(stderr, "Usage: ./myftpc <server hostname>\n");
		exit(1);
	}	
	memset(&hints, 0, sizeof hints);
	hints.ai_socktype = SOCK_STREAM;
	if ((err = getaddrinfo(node, serv, &hints, &res)) < 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
		exit(1);
	}
	if ((sd = socket(res->ai_family, res->ai_socktype,
					 res->ai_protocol)) < 0) {
		perror("socket");
		exit(1);
	}	
	if (connect(sd, res->ai_addr, res->ai_addrlen) < 0) {
		perror("connect");
		exit(1);
	}
	freeaddrinfo(res);

	for (;;) {
		fprintf(stderr, "myFTP%% ");
		fgets(buf, sizeof buf, stdin);
		buf[strlen(buf) - 1] = '\0';
		if (*buf == '\0') {
			continue;
		}
		getargs(&ac, av, buf);
		for (p = cmd_tbl; p->cmd; p++) {
			if (!strcmp(av[0], p->cmd)) {
				(*p->func)(sd, ac, av);
				break;
			}
		}
		if (p->cmd == NULL) {
			fprintf(stderr, "unknown command\n");
		}
	}
	
	return 0;
}

void getargs(int *ac, char *av[], char *p)
{
	*ac = 0;

	for (;;) {
		while (isblank(*p)) {
			p++;
		}
		if (*p == '\0') {
			return;
		}
		av[(*ac)++] = p;
		while (*p && !isblank(*p)) {
			p++;
		}
		if (*p == '\0') {
			return;
		}
		*p++ = '\0';
	}
}

void quit_proc(int sd, int ac, char *av[])
{
	msg.type = 0x01;
	msg.length = 0;
	msg_send(sd);
	msg_recv(sd);

	close(sd);
	exit(0);
}

void pwd_proc(int sd, int ac, char *av[])
{
	msg.type = 0x02;
	msg.length = 0;
	msg_send(sd);
	msg_data_recv(sd);
	if (msg_data.type != 0x10) {		
		fprintf(stderr, "error\n");
		return;
	}
	printf("%s", msg_data.data);
}

void cd_proc(int sd, int ac, char *av[])
{
	if (ac != 2) {
		fprintf(stderr, "Syntax: cd <directory>\n");
		return;
	}
	msg_data.type = 0x03;
	msg_data.length = strlen(av[1]);
	strcpy(msg_data.data, av[1]);
	msg_data_send(sd);
	msg_recv(sd);	
	if (msg.type != 0x10) {		
		fprintf(stderr, "Directory not found\n");
	}
}

void dir_proc(int sd, int ac, char *av[])
{
	if (ac > 3) {
		fprintf(stderr, "Syntax: dir [file or directory]\n");
		return;
	} else if (ac == 1) {
		msg_data.type = 0x04;
		msg_data.length = 0;
		memset(msg_data.data, '\0', sizeof msg_data.data);
		msg_data_send(sd);
	} else {
		msg_data.type = 0x04;
		msg_data.length = strlen(av[1]);
		strcpy(msg_data.data, av[1]);
		msg_data_send(sd);
	}
	msg_recv(sd);
	if (msg.type != 0x10) {
		fprintf(stderr, "error\n");
		return;
	}
	msg_data_recv(sd);
	if (msg_data.type != 0x20) {
		fprintf(stderr, "no data\n");
		return;
	}
	printf("%s", msg_data.data);
}

void lpwd_proc(int sd, int ac, char *av[])
{
	int pid, stat;
	char *ex[] = {"pwd", NULL};

	if ((pid = fork()) < 0) {
		perror("fork");
		exit(1);
	}
	if (pid == 0) {
		if (execvp(ex[0], ex) < 0) {
			perror("execvp");
			exit(1);
		}
	}
	if (wait(&stat) < 0) {
		perror("wait");
		exit(1);
	}
}

void lcd_proc(int sd, int ac, char *av[])
{
	if (ac != 2) {
		fprintf(stderr, "Syntax: lcd <directory>\n");
	} else if (chdir(av[1]) < 0) {
		fprintf(stderr, "chdir error\n");
		exit(1);
	}
}

void ldir_proc(int sd, int ac, char *av[])
{
	int pid, stat;
	char *ex1[] = {"ls", "-l", NULL};
	char *ex2[] = {"ls", "-l", av[1], NULL};

	if ((pid = fork()) < 0) {
		perror("fork");
		exit(1);
	}
	if (pid == 0) {
		if (ac == 1) {
			if (execvp(ex1[0], ex1) < 0) {
				perror("execvp");
				exit(1);
			}
		} else {
			if (execvp(ex2[0], ex2) < 0) {
				perror("execvp");
				exit(1);
			}
		}
	}
	if (wait(&stat) < 0) {
		perror("wait");
		exit(1);
	}
}

void get_proc(int sd, int ac, char *av[])
{
	int fd;
	char file[80], lbuf[10];

	if (ac > 3 || ac < 2) {
		fprintf(stderr, "Syntax: get <filename> [filename]\n");
		return;
	} else if (ac == 2) {
		strcpy(file, av[1]);
	} else {
		strcpy(file, av[2]);
	}
	if ((fd = open(file, O_WRONLY | O_CREAT | O_EXCL, 0644)) < 0) {
		if (errno != EEXIST) {
			perror("open");
			exit(1);
		}
		fprintf(stderr, "overwrite ok (yes/no): ");
		if (fgets(lbuf, sizeof lbuf, stdin) == NULL) {
			if (ferror(stdin)) {
				perror("fgets");
				exit(1);
			}
			if (feof(stdin)) {
				fprintf(stderr, "stdin EOF\n");
				exit(1);
			}
		}
		if (*lbuf != 'y') {
			return;
		}
		if ((fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
			perror("open");
			exit(1);
		}
	}
	
	msg_data.type = 0x05;
	msg_data.length = strlen(av[1]);
	strcpy(msg_data.data, av[1]);
	msg_data_send(sd);
	msg_recv(sd);
	if (msg.type != 0x10) {
		fprintf(stderr, "error\n");
		return;
	}
	if (msg.code != 0x01) {
		fprintf(stderr, "no data\n");
		return;
	}
	for (;;) {
		msg_data_recv(sd);			
		if (msg_data.type != 0x20) {
			fprintf(stderr, "error\n");
			return;
		} else {
			if (write(fd, msg_data.data, msg_data.length) < 0) {
				perror("write");
				close(fd);
				exit(1);
			}
			if (msg_data.code == 0x01) {
				break;
			}
		}
	}
	close(fd);
}

void put_proc(int sd, int ac, char *av[])
{
	int fd, cnt;

	if (ac > 3 || ac < 2) {
		fprintf(stderr, "Syntax: put <filename> [filename]\n");
		return;
	} else if (ac == 2) {
		strcpy(msg_data.data, av[1]);
	} else {
		strcpy(msg_data.data, av[2]);
	}
	msg_data.type = 0x06;
	msg_data.length = strlen(msg_data.data);
	msg_data_send(sd);
	msg_recv(sd);
	if (msg.type != 0x10) {
		fprintf(stderr, "error\n");
		return;
	}
	if (msg.code != 0x02) {
		fprintf(stderr, "no data\n");
		return;
	}
	if ((fd = open(av[1], O_RDONLY)) < 0) {
		perror("open");
		return;
	}
	while ((cnt = read(fd, msg_data.data, sizeof msg_data.data))) {
		if (cnt < 0) {
			msg.type = 0x12;
			msg.code = 0x01;
			msg.length = 0;
			msg_send(sd);
			perror("read");
			close(fd);
			return;
		} else if (cnt == sizeof msg_data.data) {
			msg_data.type = 0x20;
			msg_data.code = 0x00;
			msg_data.length = cnt;
			msg_data_send(sd);
		} else {
			break;
		}
	}
	msg_data.type = 0x20;
	msg_data.code = 0x01;
	msg_data.length = cnt;
	msg_data_send(sd);
	
	close(fd);
}

void help_proc(int sd, int ac, char *av[]){}

int msg_send(int sd)
{
	int cnt;
	
	if ((cnt = send(sd, &msg, sizeof msg, 0)) < 0) {
		perror("send");
		close(sd);
		exit(1);
	}
	/*
	  printf("send | Type: 0x%02x, Code: 0x%02x, Length: %d\n",
	  msg.type, msg.code, msg.length);
	*/
	
	return cnt;
}

int msg_data_send(int sd)
{
	int cnt;

	if ((cnt = send(sd, &msg_data, sizeof msg_data, 0)) < 0) {
		perror("send");
		close(sd);
		exit(1);
	}
	/*
	  printf("send | Type: 0x%02x, Code: 0x%02x, Length: %d, Data: %s\n",
	  msg_data.type, msg_data.code, msg_data.length, msg_data.data);
	*/
	
	return cnt;
}

int msg_recv(int sd)
{
	int cnt;
	
	if ((cnt = recv(sd, &msg, sizeof msg, 0)) < 0) {
		perror("recv");
		close(sd);
		exit(1);
	}
	/*
	  printf("recv | Type: 0x%02x, Code: 0x%02x, Length: %d\n",
	  msg.type, msg.code, msg.length);
	*/
	
	return cnt;
}

int msg_data_recv(int sd)
{
	int cnt;

	if ((cnt = recv(sd, &msg_data, sizeof msg_data, 0)) < 0) {
		perror("recv");
		close(sd);
		exit(1);
	}
	/*
	  printf("recv | Type: 0x%02x, Code: 0x%02x, Length: %d, Data: %s\n",
	  msg_data.type, msg_data.code, msg_data.length, msg_data.data);
	*/
	
	return cnt;
}
