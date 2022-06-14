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
#define MAXDATASIZE 450

#define TIMEOUT 500

void *get_in_addr(struct sockaddr * sa) {
	return &(((struct sockaddr_in*)sa)->sin_addr);
}

/* Funcao que solicita ao servidor que realize o login do usuario */
void logar() {

	int id = -1;
	printf("Por favor digite seu id de usuario\n");
        while (scanf("%d",&id) && id<0) {
        	printf("Id invalido\n");
        }
        printf("Id: %d\n", id);
}

/* Funcao que solicita ao servidor que cadastre um filme no banco de dados*/
void cadastrar_filme(short choice, int sockfd, struct addrinfo * p) {

	int identifier;
        char name[100];
        char genre[100];
	char director[100];
	char buf[MAXDATASIZE];
        int year;
	printf("Por favor digite o nome do filme que deseja cadastrar\n");
        scanf(" %[^\n]", name);
        printf("Por favor digite o genero do filme que deseja cadastrar:\n");
        scanf(" %[^\n]", genre);
	printf("Por favor digite o nome do diretor do filme que deseja cadastrar:\n");
	scanf(" %[^\n]", director);
        printf("Por favor digite o ano do filme que deseja cadastrar:\n");
        scanf("%d", &year);
	printf("Por favor um identificador numerico para o filme que deseja cadastrar ou -1 para atribuir automaticamente.\n");
        scanf("%d", &identifier);
        sprintf(buf, "%hd\n%d\n%s\n%s\n%s\n%d", choice, identifier, name, genre, director, year); // constroi a mensagem que sera enviada para o servidor
        if (sendto(sockfd, buf, strlen(buf),0,p->ai_addr,p->ai_addrlen)==-1)
        	perror("cadastrar_filme resposta 1");
}

/* Funcao que solicita ao servidor que edite um filme do banco de dados */
void editar_filme(short choice, int sockfd, struct addrinfo * p) {

	int identifier;
        char genre[100];
	char buf[MAXDATASIZE];
	printf("Por favor digite o identificador do filme que deseja editar\n");
        scanf("%d", &identifier);
        printf("Por favor digite o genero que deseja acrescentar ao filme %d:\n", identifier);
        scanf(" %[^\n]", genre);
        sprintf(buf,"%hd\n%d\n%s", choice, identifier, genre);
        if (sendto(sockfd,buf,strlen(buf),0,p->ai_addr,p->ai_addrlen)==-1)
        	perror("editar_filme resposta 1");
}

/* Funcao que solicita ao servidor que delete filmes do banco de dados*/
void deletar_filme(short choice, int sockfd, struct addrinfo * p) {

	int identifier;
	char buf[MAXDATASIZE];
	printf("Por favor digite o identificador do filme que deseja deletar\n");
        scanf("%d", &identifier);
        sprintf(buf,"%hd\n%d", choice, identifier);
        if (sendto(sockfd,buf,strlen(buf),0,p->ai_addr,p->ai_addrlen)==-1)
        	perror("deletar_filme resposta 1");
}

/* Funcao que solicita ao servidor que filtre filmes do banco de dados */
void filtrar_filme(short choice, int sockfd, struct addrinfo * p) {

	int numbytes;
	char buf[MAXDATASIZE+3];
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

	printf("Por favor selecione um dos filtros abaixo:\n(1)Todos os filmes\n(2)Filtrar por genero\n(3)Filtrar por identificador\n");
        scanf("%hd", &typefilter);
	if (typefilter == 1) {
		sprintf(buf,"%hd\n%hd",choice, typefilter);
	}
	else if (typefilter == 2) { // filtrar um genero
		printf("Por favor selecione um genero para filtrar:\n");
        	char genre[100];
                scanf(" %[^\n]", genre); // recebe um genero do usuario
		sprintf(buf,"%hd\n%hd\n%s",choice, typefilter,genre);
        }
	else if (typefilter == 3) { // filtrar um identificador
		printf("Por favor selecione um identificador para filtrar:\n");
        	int identifier;
                scanf("%d", &identifier); // recebe um identificador do usuario
		sprintf(buf,"%hd\n%hd\n%d",choice,typefilter,identifier);
        }

	if (sendto(sockfd,buf,strlen(buf),0,p->ai_addr,p->ai_addrlen)==-1)
                perror("filtrar_filme query");

	do { // recebe o numero de bytes do servidor
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
void listar_ids(short choice, int sockfd, struct addrinfo * p) {

	int numbytes;
	char movie[MAXDATASIZE];
	int total_bytes;
	char buf[MAXDATASIZE];
	
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

	sprintf(buf,"%hd", choice);
	if (sendto(sockfd,buf,strlen(buf),0,p->ai_addr,p->ai_addrlen)==-1)
                perror("listar ids");
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

	/*Menu inicial*/
	printf("Bem vindo, por favor escolha uma das opções abaixo:\n(1)Logar\n(2)Cadastrar filmes\n(3)Editar um filme\n(4)Deletar um filme\n(5)Filtrar\n(6)Listar identificadores\n(7)Sair\n");

	while (scanf("%hd", &choice) && choice != 7) {

		if (choice == 1) {
			logar();
                }
                else if (choice == 2) {
			cadastrar_filme(choice,sockfd,p);
                }
                else if (choice == 3) {
                        editar_filme(choice,sockfd,p);
                }
                else if (choice == 4) {
			deletar_filme(choice,sockfd,p);
                }
		else if (choice == 5) {
			filtrar_filme(choice,sockfd,p);
                }
                else if (choice == 6) {
			listar_ids(choice,sockfd,p);
                }
		printf("Por favor, selecione uma nova opcao:\n");
	}

	close(sockfd);

	return 0;
}
