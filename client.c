#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <string.h>

#define PORT "5151"
#define MAXDATASIZE 300

void *get_in_addr(struct sockaddr * sa) {
	return &(((struct sockaddr_in*)sa)->sin_addr);
}

void logar() {

	int id = -1;
        while (scanf("%d",&id) && id<0) {
        	printf("Id invalido\n");
        }
        printf("Id: %d\n", id);
}

void cadastrar_filme(int sockfd) {

	int identifier;
        char name[100];
        char genre[100];
	char buf[MAXDATASIZE];
        int year;
        scanf(" %[^\n]", name);
        printf("Por favor digite o genero do filme que deseja cadastrar:\n");
        scanf(" %[^\n]", genre);
        printf("Por favor digite o ano do filme que deseja cadastrar:\n");
        scanf("%d", &year);
	printf("Por favor um identificador numerico para o filme que deseja cadastrar ou -1 para atribuir automaticamente.\n");
        scanf("%d", &identifier);
        sprintf(buf, "%d\n%s\n%s\n%d", identifier, name, genre, year);
        if (send(sockfd, buf, strlen(buf),0)==-1)
        	perror("send");
}

void editar_filme(int sockfd) {

	int identifier;
        char genre[100];
	char buf[MAXDATASIZE];
        scanf("%d", &identifier);
        printf("Por favor digite o genero que deseja acrescentar ao filme %d:\n", identifier);
        scanf(" %[^\n]", genre);
        sprintf(buf,"%d\n%s", identifier, genre);
        if (send(sockfd,buf,strlen(buf),0)==-1)
        	perror("send");
}

void deletar_filme(int sockfd) {

	int identifier;
        scanf("%d", &identifier);
        identifier = htonl(identifier);
        if (send(sockfd,&identifier,4,0)==-1)
        	perror("send");
}

void filtrar_filme(int sockfd) {

	int numbytes;
	char buf[MAXDATASIZE];
	short typefilter;
        char movie[MAXDATASIZE];
	int total_bytes;

        scanf("%hd", &typefilter);
        typefilter = htons(typefilter);
        if (send(sockfd, &typefilter, 2, 0) == -1)
        	perror("send:client");
        typefilter = ntohs(typefilter);
        if (typefilter == 2) {
        	char genre[MAXDATASIZE];
                if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1,0)) == -1) {
                	perror("recv");
                        exit(1);
                }
                buf[numbytes] = '\0';
                printf("%s", buf);
                scanf(" %[^\n]", genre);
                if (send(sockfd, genre, strlen(genre), 0) == -1)
                	perror("send:client");
        }
	else if (typefilter == 3) {
        	int identifier;
                if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1,0))==-1) {
                	perror("recv");
                        exit(1);
                }
                buf[numbytes] = '\0';
                printf("%s", buf);
                scanf("%d", &identifier);
                identifier = htonl(identifier);
                if (send(sockfd, &identifier, 4, 0)==-1)
                	perror("send");
        }
	if ((numbytes=recv(sockfd,&total_bytes,4,0))==-1) {
		perror("recv");
		exit(1);
	}
	total_bytes = ntohl(total_bytes);
        do {
        	if ((numbytes = recv(sockfd, movie, MAXDATASIZE-1,0)) == -1) {
                	perror("recv");
                        exit(1);
                }
                movie[numbytes] = '\0';
		total_bytes -= numbytes;
                printf("%s", movie);
        } while (total_bytes>0);
}

void listar_ids(int sockfd) {

	int numbytes;
	char movie[MAXDATASIZE];
	int total_bytes;

	if ((numbytes=recv(sockfd,&total_bytes,4,0))==-1) {
                perror("recv");
                exit(1);
        }
        total_bytes = ntohl(total_bytes);

        do {
        	if ((numbytes = recv(sockfd, movie, MAXDATASIZE-1,0)) == -1) {
                	perror("recv");
                        exit(1);
                }
                movie[numbytes] = '\0';
                total_bytes -= numbytes;
                printf("%s", movie);
        } while (total_bytes>0);
}

int main(int argc, char*argv[]) {

	short choice;
	
	int sockfd, numbytes;
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET_ADDRSTRLEN];

	if (argc != 2) {
		fprintf(stderr, "usage: client hostname\n");
		exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(argv[1],PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	
	}
	
	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
	printf("client: connecting to %s\n", s);
	
	freeaddrinfo(servinfo);

	/*Menu inicial*/
	if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
		perror("recv");
		exit(1);
	}

	buf[numbytes] = '\0';

	printf("%s", buf);

	while (scanf("%hd", &choice) && choice != 7) {
		choice = htons(choice);
		if (send(sockfd, &choice, 2, 0)==-1)
			perror("client: send");
		choice = ntohs(choice);

		/*Instrucoes*/
		if (choice != 6) {	
			if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1,0)) == -1) {
				perror("recv");
				exit(1);		
			}
			buf[numbytes] = '\0';
			printf("%s", buf);
		}

		if (choice == 1) {
			logar();
                }
                else if (choice == 2) {
			cadastrar_filme(sockfd);
                }
                else if (choice == 3) {
                        editar_filme(sockfd);
                }
                else if (choice == 4) {
			deletar_filme(sockfd);
                }
		else if (choice == 5) {
			filtrar_filme(sockfd);
                }
                else if (choice == 6) {
			listar_ids(sockfd);
                }
		printf("Por favor, selecione uma nova opcao:\n");
	}

	close(sockfd);

	return 0;
}
