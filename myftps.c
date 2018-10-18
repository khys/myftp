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
#include <errno.h>
#include <fcntl.h>

#include "myftp.h"

struct type_table {
	uint8_t type;
	void (*func)(int);
} typ_tbl[] = {
	{0x01, quit_exec},
	{0x02, pwd_exec},
	{0x03, cwd_exec},
	{0x04, list_exec},
	{0x05, retr_exec},
	{0x06, stor_exec},
	{0, NULL}
};

struct myftph msg;
struct myftph_data msg_data;
int sd0;

int main(int argc, char *argv[])
{
	struct addrinfo hints, *res;
	struct sockaddr_storage sin;
	int sd1, err;
	socklen_t sktlen, len;
	// char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

	struct type_table *p;
	
	char serv[] = SRVPORT;

	if (argc > 2) {
		fprintf(stderr, "Usage: ./myftps [current directory]\n");
		exit(1);
	} else if (argc == 2) {
		if (chdir(argv[1]) < 0) {
			fprintf(stderr, "chdir error\n");
			exit(1);
		}
	}
	
	memset(&hints, 0, sizeof hints);
	hints.ai_flags = AI_PASSIVE;
	hints.ai_socktype = SOCK_STREAM;
	if ((err = getaddrinfo(NULL, serv, &hints, &res)) < 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
		exit(1);
	}
	if ((sd0 = socket(res->ai_family, res->ai_socktype,
					  res->ai_protocol)) < 0) {
		perror("socket");
		exit(1);
	}
	if (bind(sd0, res->ai_addr, res->ai_addrlen) < 0) {
		perror("bind");
		exit(1);
	}
	freeaddrinfo(res);
	if (listen(sd0, 5) < 0) {
		perror("lisen");
		exit(1);
	}
	sktlen = sizeof (struct sockaddr_storage);
	if ((sd1 = accept(sd0, (struct sockaddr *)&sin, &sktlen)) < 0) {
		perror("accept");
		exit(1);
	}
	/*
	  if ((err = getnameinfo((struct sockaddr *)&sin, len,
	  hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), 0)) < 0) {
	  fprintf(stderr, "getnameinfo: %s\n", gai_strerror(err));
	  exit(1);
	  }
	*/
	
	for (;;) {
		msg_data_recv(sd1);
		for (p = typ_tbl; p->type; p++) {
			if (msg_data.type == p->type) {
				(*p->func)(sd1);
				break;
			}
		}
		if (p->type == 0) {
			fprintf(stderr, "unknown type\n");
		}		
	}

	return 0;
}

void quit_exec(int sd)
{
	msg.type = 0x10;
	msg.code = 0x00;
	msg.length = 0;
	msg_send(sd);	
	
	close(sd0);
	close(sd);
	exit(0);
}

void pwd_exec(int sd)
{
	FILE *fp;
	char buf[DATASIZE];
	char *cmd = "pwd";
	
	if ((fp = popen(cmd, "r")) == NULL) {
		perror("popen");
		exit(1);
	}
	fgets(buf, sizeof buf, fp);
	pclose(fp);
	
	msg_data.type = 0x10;
	msg_data.code = 0x00;
	msg.length = strlen(buf);
	strcpy(msg_data.data, buf);
	msg_data_send(sd);
}

void cwd_exec(int sd)
{	
	if (chdir(msg_data.data) < 0) {
		fprintf(stderr, "chdir error\n");
		msg.type = 0x12;
		msg.code = 0x00;
	} else {		
		msg.type = 0x10;
		msg.code = 0x00;
	}
	msg.length = 0;
	msg_send(sd);
}

void list_exec(int sd)
{
	int pid, stat;
	FILE *fp;
	char buf[DATASIZE], lbuf[80];
	char *cmd1 = "ls -l";
	char cmd2[80] = "ls -l ";

	*buf = '\0';
	memset(lbuf, '\0', sizeof lbuf);
	strcat(cmd2, msg_data.data);

	if (msg_data.length == 0) {
		if ((fp = popen(cmd1, "r")) == NULL) {
			msg.type = 0x12;
			msg.code = 0x00;
			msg.length = 0;
			msg_send(sd);
			perror("popen");
			return;
		}
		while (fgets(lbuf, sizeof lbuf, fp) != NULL) {
			lbuf[strlen(lbuf) + 1] = '\n';
			strcat(buf, lbuf);
		}
		pclose(fp);
	} else {
		if ((fp = popen(cmd2, "r")) == NULL) {
			msg.type = 0x12;
			msg.code = 0x00;
			msg.length = 0;
			msg_send(sd);
			perror("popen");
			return;
		}
		while (fgets(lbuf, sizeof lbuf, fp) != NULL) {
			lbuf[strlen(lbuf) + 1] = '\n';
			strcat(buf, lbuf);
		}
		pclose(fp);
	}
	msg.type = 0x10;
	msg.code = 0x01;
	msg.length = 0;
	msg_send(sd);

	msg_data.type = 0x20;
	msg_data.code = 0x00;
	msg_data.length = strlen(buf);
	strcpy(msg_data.data, buf);
	msg_data_send(sd);
}

void retr_exec(int sd)
{
	int fd, cnt;

	if ((fd = open(msg_data.data, O_RDONLY)) < 0) {
		msg.type = 0x12;
		msg.code = 0x00;
		msg.length = 0;
		msg_send(sd);
		perror("open");
		return;
	} else {
		msg.type = 0x10;
		msg.code = 0x01;
		msg.length = 0;
		msg_send(sd);
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

void stor_exec(int sd)
{
	int fd;
	
	if ((fd = open(msg_data.data, O_WRONLY | O_CREAT | O_EXCL, 0644)) < 0) {
		msg.type = 0x12;
		msg.code = 0x01;
		msg.length = 0;
		msg_send(sd);
		perror("open");
		return;
	} else {
		msg.type = 0x10;
		msg.code = 0x02;
		msg.length = 0;
		msg_send(sd);
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
