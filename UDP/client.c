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
#include <poll.h>

#include <string.h>

#define PORT "5151"
#define MAXDATASIZE 300

#define TIMEOUT 500

void *get_in_addr(struct sockaddr * sa) {
	return &(((struct sockaddr_in*)sa)->sin_addr);
}

/* Funcao que solicita ao servidor que realize o login do usuario */
void logar() {

	int id = -1;
        while (scanf("%d",&id) && id<0) {
        	printf("Id invalido\n");
        }
        printf("Id: %d\n", id);
}

/* Funcao que solicita ao servidor que cadastre um filme no banco de dados*/
void cadastrar_filme(int sockfd, struct addrinfo * p) {

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
        sprintf(buf, "%d\n%s\n%s\n%d", identifier, name, genre, year); // constroi a mensagem que sera enviada para o servidor
        if (sendto(sockfd, buf, strlen(buf),0,p->ai_addr,p->ai_addrlen)==-1)
        	perror("cadastrar_filme resposta 1");
}

/* Funcao que solicita ao servidor que edite um filme do banco de dados */
void editar_filme(int sockfd, struct addrinfo * p) {

	int identifier;
        char genre[100];
	char buf[MAXDATASIZE];
        scanf("%d", &identifier);
        printf("Por favor digite o genero que deseja acrescentar ao filme %d:\n", identifier);
        scanf(" %[^\n]", genre);
        sprintf(buf,"%d\n%s", identifier, genre);
        if (sendto(sockfd,buf,strlen(buf),0,p->ai_addr,p->ai_addrlen)==-1)
        	perror("editar_filme resposta 1");
}

/* Funcao que solicita ao servidor que delete filmes do banco de dados*/
void deletar_filme(int sockfd, struct addrinfo * p) {

	int identifier;
        scanf("%d", &identifier);
        identifier = htonl(identifier);
        if (sendto(sockfd,&identifier,4,0,p->ai_addr,p->ai_addrlen)==-1)
        	perror("deletar_filme resposta 1");
}

/* Funcao que solicita ao servidor que filtre filmes do banco de dados */
void filtrar_filme(int sockfd, struct addrinfo * p) {

	int numbytes;
	char buf[MAXDATASIZE];
	short typefilter;
        char movie[MAXDATASIZE];
	int total_bytes;

	// Variaveis para verificar se a mensagem foi enviada pelo servidor
        socklen_t len;
        struct sockaddr*preply_addr;
        preply_addr = malloc(p->ai_addrlen);
        len = p->ai_addrlen;
        // Variaveis para verificar se a mensagem foi enviada pelo servidor
	
	// Variaveis para usar no time out
        int fd_count = 1;
        struct pollfd *pfds = malloc(sizeof *pfds);
        pfds[0].fd = sockfd;
        pfds[0].events = POLLIN;
        int n_events;
        // Variaveis para usar no time out

        scanf("%hd", &typefilter);
        typefilter = htons(typefilter);
        if (sendto(sockfd, &typefilter, 2, 0,p->ai_addr,p->ai_addrlen) == -1)
        	perror("filtrar_filme resposta 1");
        typefilter = ntohs(typefilter);
        if (typefilter == 2) { // filtrar um genero
        	char genre[MAXDATASIZE];
		do {
			n_events = poll(pfds,fd_count,TIMEOUT);
                        if (n_events==0) {
                                printf("Dificuldade em conectar com o servidor\n");
                        	return;
                        }
                        else {
				numbytes = recvfrom(sockfd, buf, MAXDATASIZE-1,0,preply_addr, &len);
				if (numbytes == -1) {
                        		perror("filtrar_filme requisicao 1");
                        		exit(1);
                		}       
                        }

		} while (len != p->ai_addrlen || memcmp(p->ai_addr,preply_addr,len)!=0);
                buf[numbytes] = '\0';
                printf("%s", buf);
                scanf(" %[^\n]", genre); // recebe um genero do usuario
                if (sendto(sockfd, genre, strlen(genre), 0,p->ai_addr,p->ai_addrlen) == -1)
                	perror("filtrar_filme resposta 2");
        }
	else if (typefilter == 3) { // filtrar um identificador
        	int identifier;
		do {
			n_events = poll(pfds,fd_count,TIMEOUT);
                        if (n_events==0) {
                                printf("Dificuldade em conectar com o servidor\n");
                        	return;
                        }
                        else {
				numbytes = recvfrom(sockfd, buf, MAXDATASIZE-1,0,preply_addr,&len);
				if (numbytes ==-1) {
                        		perror("filtrar_filme requisicao 2");
                        		exit(1);
                		}
                        }
		} while (len != p->ai_addrlen || memcmp(p->ai_addr,preply_addr,len)!=0);
                buf[numbytes] = '\0';
                printf("%s", buf);
                scanf("%d", &identifier); // recebe um identificador do usuario
                identifier = htonl(identifier);
                if (sendto(sockfd, &identifier, 4, 0,p->ai_addr,p->ai_addrlen)==-1)
                	perror("filtrar_filme resposta 3");
        }

	do {
		n_events = poll(pfds,fd_count,TIMEOUT);
                if (n_events==0) {
                	printf("Dificuldade em conectar com o servidor\n");
                	return;
                }
                else {
			numbytes=recvfrom(sockfd,&total_bytes,4,0,preply_addr,&len);
			if (numbytes ==-1) {
                	perror("filtrar_filme requisicao 3");
                	exit(1);
        		}
                }
	} while(len != p->ai_addrlen || memcmp(p->ai_addr,preply_addr,len)!=0);
	total_bytes = ntohl(total_bytes);
        do { // enquanto nao tiver recebido todos os bytes recebe mais 299
		do {
			n_events = poll(pfds,fd_count,TIMEOUT);
                        if (n_events==0) {
                                printf("Dificuldade em conectar com o servidor\n");
                        	return;
                        }
                        else {
				numbytes = recvfrom(sockfd, movie, MAXDATASIZE-1,0,preply_addr,&len);
				if (numbytes == -1) {
                        		perror("filtrar_filme requisicao 4");
                        		exit(1);
                		}
                        }
		} while (len != p->ai_addrlen || memcmp(p->ai_addr,preply_addr,len)!=0);
                movie[numbytes] = '\0';
		total_bytes -= numbytes;
                printf("%s", movie); // imprime os bytes recebidos
        } while (total_bytes>0);
}

/* Funcao que solicita ao servidor os ids e nomes de todos os filmes */
void listar_ids(int sockfd, struct addrinfo * p) {

	int numbytes;
	char movie[MAXDATASIZE];
	int total_bytes;
	
	// Variaveis para verificar se a mensagem foi enviada pelo servidor
	socklen_t len;
	struct sockaddr*preply_addr;
	preply_addr = malloc(p->ai_addrlen);
	len = p->ai_addrlen;
	// Variaveis para verificar se a mensagem foi enviada pelo servidor
	
	// Variaveis para usar no time out
        int fd_count = 1;
        struct pollfd *pfds = malloc(sizeof *pfds);
        pfds[0].fd = sockfd;
        pfds[0].events = POLLIN;
        int n_events;
        // Variaveis para usar no time out

	do {
		n_events = poll(pfds,fd_count,TIMEOUT);
                if (n_events==0) {
                	printf("Dificuldade em conectar com o servidor\n");
                	return;
                }
                else {
			numbytes=recvfrom(sockfd,&total_bytes,4,0,preply_addr,&len);
			if (numbytes == -1) {
                		perror("listar_ids requisicao 1");
                		exit(1);
        		}
                }
	} while (len != p->ai_addrlen || memcmp(p->ai_addr,preply_addr,len)!=0);
        total_bytes = ntohl(total_bytes);

        do { // enquanto nao tiver recebido total_bytes recebe mais 299
        	do {
			n_events = poll(pfds,fd_count,TIMEOUT);
                        if (n_events==0) {
                        	printf("Dificuldade em conectar com o servidor\n");
                        	return;
                        }
                        else {
				numbytes = recvfrom(sockfd, movie, MAXDATASIZE-1,0,preply_addr, &len);
				if (numbytes == -1) {
                        		perror("listar_ids resposta 1");
                        		exit(1);
                		}
                        }
		} while (len != p->ai_addrlen || memcmp(p->ai_addr,preply_addr,len)!=0);
                movie[numbytes] = '\0';
                total_bytes -= numbytes;
                printf("%s", movie); // imprime os bytes recebidos
        } while (total_bytes>0);
}

int main(int argc, char*argv[]) {

	short choice = 0;
	
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
	hints.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo(argv[1],PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}


	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		break;
	
	}
	
	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	freeaddrinfo(servinfo);

	// Variaveis para verificar se a mensagem foi enviada pelo servidor
        socklen_t len;
        struct sockaddr*preply_addr;
        len = p->ai_addrlen;
	preply_addr = malloc(len);
        // Variaveis para verificar se a mensagem foi enviada pelo servidor
	//
	// Variaveis para usar no time out
	int fd_count = 1;
	struct pollfd *pfds = malloc(sizeof *pfds);
	pfds[0].fd = sockfd;
	pfds[0].events = POLLIN;
	int n_events;
	// Variaveis para usar no time out

	choice = htons(choice);
	
	do {
		if ((numbytes = sendto(sockfd, &choice, 2, 0,p->ai_addr, p->ai_addrlen))== -1) {
			perror("talker: sendto");
			exit(1);
		}

		/*Menu inicial*/
		do {
			n_events = poll(pfds,fd_count,TIMEOUT);
			if (n_events==0) {
				printf("Dificuldade em conectar com o servidor, por favor espere um pouco\n");
				break;
			}
			else {
				numbytes = recvfrom(sockfd, buf, MAXDATASIZE-1, 0, preply_addr, &len);
				if (numbytes ==-1) {
                		        perror("Menu inicial resposta");
                		        exit(1);
                		}
			}
		} while (len != p->ai_addrlen || memcmp(p->ai_addr,preply_addr,len)!=0);
	}while (n_events==0);

	buf[numbytes] = '\0';

	printf("%s", buf);

	while (scanf("%hd", &choice) && choice != 7) {
		choice = htons(choice);
		if ((numbytes = sendto(sockfd, &choice, 2, 0,p->ai_addr, p->ai_addrlen))== -1)  // envia ao servidor escolha do usuario
			perror("escolha resposta");
		choice = ntohs(choice);

		/*Instrucoes*/
		if (choice != 6) {
                	do {
                	        n_events = poll(pfds,fd_count,TIMEOUT);
                       		if (n_events==0) {
                                	printf("Dificuldade em conectar com o servidor\n");
                        		break;
				}
                        	else {
					numbytes = recvfrom(sockfd, buf, MAXDATASIZE-1, 0, preply_addr, &len);
					if (numbytes ==-1) {
						perror("segunda instrucao requisicao");
						exit(1);
					}
                        	}
                	} while (len != p->ai_addrlen || memcmp(p->ai_addr,preply_addr,len)!=0);
			buf[numbytes] = '\0';
			printf("%s", buf);
		}

		if (choice == 1) {
			logar();
                }
                else if (choice == 2) {
			cadastrar_filme(sockfd,p);
                }
                else if (choice == 3) {
                        editar_filme(sockfd,p);
                }
                else if (choice == 4) {
			deletar_filme(sockfd,p);
                }
		else if (choice == 5) {
			filtrar_filme(sockfd,p);
                }
                else if (choice == 6) {
			listar_ids(sockfd,p);
                }
		printf("Por favor, selecione uma nova opcao:\n");
	}

	close(sockfd);

	return 0;
}
