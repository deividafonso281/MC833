#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define PORT "5151"

#define BACKLOG 5

char ip[100];
char ipstr[100];

void sigchld_handler(int s) {
	int saved_errno = errno;
	while(waitpid(-1,NULL, WNOHANG) > 0);
	errno = saved_errno;
}

void *get_in_addr(struct sockaddr *sa) {
	return &(((struct sockaddr_in*)sa)->sin_addr);
}

int main () {

	int numbytes;
	short choice = 0;
	int work;
	work = gethostname(ip, sizeof(ip));
	printf("O ip desse computador é %s\n", ip);//inet_ntop(AF_INET,&ip, ipstr, sizeof(ipstr)));
	
	int sockfd, new_fd;
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr;
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET_ADDRSTRLEN];
	int rv;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
	}

	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}
		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}
	freeaddrinfo(servinfo);

	if (p == NULL) {
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}
	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connections...\n");

	while(1) {
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
		printf("server: got connection from %s\n", s);

		if (!fork()) {
			close(sockfd);
			if (send(new_fd, "Bem vindo, por favor escolha uma das opções abaixo:\n(1)Logar\n(2)Cadastrar filmes\n(3)Editar um filme\n(4)Deletar um filme\n(5)Filtrar\n(6)Listar identificadores\n(7)Sair\n", 166, 0) == -1)
				perror("send");
			while (choice != 7) {
				if ((numbytes=recv(new_fd, &choice, 2, 0))==-1) {
					perror("recv");
					exit(1);
				}
				choice = ntohs(choice);
				printf("Escolha do usuário: %d\n", choice);
				if (choice == 1) {
					if (send(new_fd, "Por favor digite seu id de usuario\n", 35, 0) == -1)
						perror("send");
				}
				else if (choice == 2) {
					if (send(new_fd, "Por favor digite o nome do filme que deseja cadastrar\n", 54, 0) == -1)
						perror("send");
				}
				else if (choice == 3) {
					if (send(new_fd, "Por favor digite o ideintificador do filme que deseja editar\n", 61, 0) == -1)
						perror("send");
				}
				else if (choice == 4) {
					if (send(new_fd, "Por favor digite o identificador do filme que deseja deletar\n", 62, 0) == -1)
						perror("send");
				}
				else if (choice == 5) {
					if (send(new_fd, "Por favor selecione um dos filtros abaixo:\n(1)Todos os filmes\n(2)Filtrar por genero\n(3)Filtrar por identificador\n", 113, 0) == -1)
						perror("send");
				}
				else if (choice == 6) {
					printf("choice = 6\n");
				}
			}
			close(new_fd);
			exit(0);
		}
		close(new_fd);
	}
}
