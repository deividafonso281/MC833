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
#include <poll.h>

#include "sqlite3.h"

#define PORT "5151"

#define BACKLOG 5


/* Estrutura para armazenar os parametros que serao usados no sqlite3_exec para enviar mensagens para o cliente*/
typedef struct params {
	int socket;
	struct sockaddr_storage their_addr;
	socklen_t addr_len;
} params;

char ip[100];
char ipstr[100];

/*Funcao callback da query que edita o banco de dados*/
int edit(void *NotUsed, int responses, char ** columns, char ** column_names) {
	for (int i=0;i<responses;i++) {
		if (strcmp(column_names[i],"Genre")==0) {
			strcpy((char*)NotUsed, columns[i]);
		}
	}
}

/*Funcao callback da query que filtra o banco de dados*/
int filter(void *NotUsed, int responses, char ** columns, char ** column_names) {
	params* para = (params*) NotUsed;
	int socket = para->socket;
	struct sockaddr_storage their_addr = para->their_addr;
	socklen_t addr_len = para->addr_len;
	char movie[200];
	strcpy(movie, "\0");
        for (int i=0;i<responses;i++) {
		strcat(movie,column_names[i]);
		strcat(movie,": ");
		strcat(movie,columns[i]);
		strcat(movie,"\n");
        }
	if (sendto(socket, movie,strlen(movie),0,(struct sockaddr *)&their_addr, addr_len)==-1)
        	perror("filter");

        return 0;
}

/*Funcao callback da query que conta o numero de bytes que sera enviado para o cliente*/
int count_bytes(void *NotUsed, int responses, char **columns, char **column_names) {
	int *total = (int*) NotUsed; // Numero total de bytes, comeca com a quantidade de bytes em um header 
	int sumcolumn; // numero de bytes na coluna selecionada
	int headers; // conta o numero de filmes que correspondem ao valor buscado
	sscanf(columns[0],"%d", &headers);
	headers *= (*total); // multiplica o numero de filmes pelo numero de bytes em um header
	*total = 0; // reseta o valor de total
	for (int i=1;i<responses;i++) { // soma o numero de bytes em todas as colunas
		sscanf(columns[i],"%d", &sumcolumn);
		*total += sumcolumn;
	}
	*total += headers; // soma os bytes dos headers

	return 0;
}


/*Funcao callback da query que seleciona o nome e o id de todos os filmes*/
int show_ids(void *NotUsed, int responses, char ** columns, char ** column_names) {
	params* para = (params*) NotUsed;
        int socket = para->socket;
        struct sockaddr_storage their_addr = para->their_addr;
        socklen_t addr_len = para->addr_len;
        char movie[200];
        sprintf(movie, "%s %s\n", columns[0], columns[1]);
        if (sendto(socket, movie,strlen(movie),0,(struct sockaddr *)&their_addr, addr_len)==-1)
                perror("show_ids");

        return 0;
}

/*Funcao que executa a operacao de adicionar um filme*/
void add_movie(int socket, sqlite3* db, int rc, char *zErrMsg,struct sockaddr_storage their_addr, socklen_t addr_len,char* request) {

	int identifier;
        int year;
        char name[100];
        char genre[100];
	char director[100];
        char queryfilter[450];
        int numbytes;

	sscanf(request, "%*d %d %[^\n] %[^\n] %[^\n] %d", &identifier, name, genre, director, &year); // extrai os valores dos campos do filme que sera adicionado da mensagem do cliente

        if (identifier==-1) { // constroi a query que adiciona o filme
        	sprintf(queryfilter, "INSERT INTO Movies (Id, Title, Genre, Director, Year) VALUES (NULL, \'%s\', \'%s\', \'%s\',%d);", name, genre, director, year);
        }
        else { // constroi a query que adiciona o filme
        	sprintf(queryfilter, "INSERT INTO Movies (Id, Title, Genre, Director, Year) VALUES (%d, \'%s\', \'%s\', \'%s\', %d);", identifier, name, genre, director, year);
        }

        /*sqlite3*/
        rc = sqlite3_exec(db, queryfilter, NULL, 0, &zErrMsg); // executa a query
}


/*Funcao que executa a operacao de editar um filme*/
void edit_movie(int socket, sqlite3* db, int rc, char *zErrMsg,struct sockaddr_storage their_addr, socklen_t addr_len, char* request) {

	int identifier;
        char new_genre[100];
        char old_genre[100];
        char queryfilter[300];
        int numbytes;

	sscanf(request, "%*d %d %[^\n]", &identifier, new_genre);

        sprintf(queryfilter, "SELECT Id, Genre FROM Movies WHERE Id = %d;", identifier); // constroi a query que encontra o filme no banco de dados

        /*sqlite3*/
        rc = sqlite3_exec(db,queryfilter, edit,(void*)old_genre,&zErrMsg); //executa a query que encontra o filme no banco de dados
        strcat(old_genre, ", ");
        strcat(old_genre, new_genre);
        sprintf(queryfilter, "UPDATE Movies SET Genre = \'%s\' WHERE Id = %d;", old_genre, identifier); // constroi a query de atualizacao
        rc = sqlite3_exec(db, queryfilter, NULL, NULL, &zErrMsg); // executa a query de atualizacao
}

/* Funcao que executa a operacao de deletar um filme do banco de dados */
void delete_movie(int socket, sqlite3* db, int rc, char *zErrMsg,struct sockaddr_storage their_addr, socklen_t addr_len, char* request) {

	int identifier;
        char queryfilter[300];
        int numbytes;

	sscanf(request,"%*d %d", &identifier);
        sprintf(queryfilter, "DELETE FROM Movies WHERE Id = %d;", identifier); // constroi a query de deletar o filme

        /*sqlite3*/
        rc = sqlite3_exec(db, queryfilter, NULL, 0, &zErrMsg); // executa a query
}

/* Funcao que realiza a operacao de filtrar filmes */
void filter_movies(int socket, sqlite3* db, int rc, char *zErrMsg,struct sockaddr_storage their_addr, socklen_t addr_len, char* request) {

	char queryfilter[300];
	char countfilter[500];
        int numbytes;
        short typefilter;
	int total_bytes = 39;
	void* addrs_total_bytes = (void*) &total_bytes;

	params* par = malloc(sizeof(params));
	par->socket = socket;
	par->their_addr = their_addr;
	par->addr_len = addr_len;

        sscanf(request,"%*d %hd", &typefilter);

        if (typefilter == 1) // usuario quer filtrar todos os filmes
        	sprintf(queryfilter, "SELECT * FROM Movies;"); // constroi query que filtra todos os filmes
        else if (typefilter == 2) { // usuario quer filtrar um genero
        	char genre[100];
		sscanf(request, "%*d %*d %[^\n]", genre);
                sprintf(queryfilter, "SELECT * FROM Movies WHERE Genre LIKE \'%%%s%%\';", genre); // constroi query que filtra genero
        }
	else if (typefilter == 3) { // usuario quer filtrar um identificador 
        	int identifier;
		sscanf(request, "%*d %*d %d", &identifier);
                sprintf(queryfilter, "SELECT * FROM Movies WHERE Id = \"%d\";", identifier); // constroi a query que filtra um identificador
        }

	sprintf(countfilter,"SELECT COUNT(*), SUM(LENGTH(Id)), SUM(LENGTH(Title)), SUM(LENGTH(Genre)), SUM(LENGTH(Director)),SUM(LENGTH(YEAR)) FROM (%s);", queryfilter); // constroi a query que conta o numero de bytes que serao enviados ao cliente
	countfilter[strlen(countfilter)-3] = ' ';
	rc = sqlite3_exec(db, countfilter, count_bytes, addrs_total_bytes, &zErrMsg); // executa a query que conta os bytes
	total_bytes = htonl(total_bytes);
	if ((numbytes = sendto(socket, &total_bytes, 4, 0, (struct sockaddr *)&their_addr, addr_len))==-1)
		perror("filter_movies tamanho");

        /*sqlite3*/
        rc = sqlite3_exec(db, queryfilter, filter, par, &zErrMsg); // executa a query de filtro
	free(par);
}

void show_all_ids(int socket, sqlite3* db, int rc, char *zErrMsg,struct sockaddr_storage their_addr, socklen_t addr_len) {

	char queryfilter[300];
	char countfilter[500];
        int numbytes;
	int total_bytes = 2;
	void* addrs_total_bytes = (void*) &total_bytes;

	params* par = malloc(sizeof(params));
	par->socket = socket;
	par->their_addr = their_addr;
	par->addr_len = addr_len;

        sprintf(queryfilter, "SELECT Id, Title FROM Movies;"); // constroi a query que seleciona os ids e nomes de todos os filmes
	sprintf(countfilter,"SELECT COUNT(*), SUM(LENGTH(Id)), SUM(LENGTH(Title)) FROM (%s);", queryfilter); // constroi a query que conta o numero de bytes que sera enviado para o cliente
        countfilter[strlen(countfilter)-3] = ' ';

	rc = sqlite3_exec(db, countfilter, count_bytes, addrs_total_bytes, &zErrMsg); // executa a query que conta os bytes
        total_bytes = htonl(total_bytes);
        if ((numbytes = sendto(socket, &total_bytes, 4, 0,(struct sockaddr *)&their_addr, addr_len))==-1)
                perror("show_all_ids tamanho");

        /*sqlite3*/
        void* param = &socket;
        rc = sqlite3_exec(db,queryfilter, show_ids, par, &zErrMsg); // executa a query que seleciona os ids e nomes de todos os filmes
	free(par);
}

void *get_in_addr(struct sockaddr *sa) {
	return &(((struct sockaddr_in*)sa)->sin_addr);
}

/* Funcao principal para a funcionalidade do servidor */
void process_requests(int socket) {

	/*sqlite3*/
        sqlite3 *db;
        int rc;
        char *zErrMsg = 0;
        rc = sqlite3_open("movies.db", &db); // abre o banco de dados

	// Estruturas para armazenar o endereco do cliente e que serao usadas para enviar e receber mensagens do cliente
	struct sockaddr_storage their_addr;
        socklen_t addr_len;
	addr_len = sizeof their_addr;
	// Estruturas para armazenar o endereco do cliente e que serao usadas para enviar e receber mensagens do cliente

	short choice = 0; // escolha do usuario
	int numbytes; //numero de bytes recebidos
	char request[450];

        while (1) {
        	if ((numbytes = recvfrom(socket, request, 449, 0, (struct sockaddr *)&their_addr, &addr_len))==-1) {
                	perror("process_requests resposta 1");
                        exit(1);
                }
		request[numbytes] = '\0';
		sscanf(request,"%hd", &choice);
                if (choice == 2) {
			add_movie(socket,db,rc,zErrMsg,their_addr,addr_len,request);
                }
                else if (choice == 3) {
			edit_movie(socket,db,rc,zErrMsg,their_addr,addr_len,request);
                }
		else if (choice == 4) {
			delete_movie(socket,db,rc,zErrMsg,their_addr,addr_len,request);
                }
                else if (choice == 5) {
			filter_movies(socket,db,rc,zErrMsg,their_addr,addr_len,request);
                }
                else if (choice == 6) {
			show_all_ids(socket,db,rc,zErrMsg,their_addr,addr_len);
                }
	}
	sqlite3_close(db);
}

int main () {

	int work;
	work = gethostname(ip, sizeof(ip));
	printf("O ip desse computador é %s\n", ip);

	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	char s[INET_ADDRSTRLEN];
	int rv;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
	}

	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
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

	printf("server: waiting for connections...\n");

	process_requests(sockfd);
	close(sockfd);

	return 0;
}
