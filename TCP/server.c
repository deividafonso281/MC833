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

#include "sqlite3.h"

#define PORT "5151"

#define BACKLOG 5

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
	int socket = *((int*)NotUsed);
	char movie[200];
	strcpy(movie, "\0");
        for (int i=0;i<responses;i++) {
		strcat(movie,column_names[i]);
		strcat(movie,": ");
		strcat(movie,columns[i]);
		strcat(movie,"\n");
        }
	if (send(socket, movie,strlen(movie),0)==-1)
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
        int socket = *((int*)NotUsed);
        char movie[200];
        sprintf(movie, "%s %s\n", columns[0], columns[1]);
        if (send(socket, movie,strlen(movie),0)==-1)
                perror("show_ids");

        return 0;
}

/*Funcao que executa a operacao de adicionar um filme*/
void add_movie(int socket, sqlite3* db, int rc, char *zErrMsg) {

	int identifier;
        int year;
        char name[100];
        char genre[100];
        char queryfilter[300];
        char buffer[300];
        int numbytes;
        if (send(socket, "Por favor digite o nome do filme que deseja cadastrar\n", 54, 0) == -1)
        	perror("add_movie requisicao 1");
        if ((numbytes=recv(socket,buffer,299,0))==-1) {
        	perror("add_movie resposta 1");
                exit(1);
        }
        buffer[numbytes] = '\0';
        sscanf(buffer, "%d %[^\n] %[^\n]%d", &identifier, name, genre, &year); // extrai os valores dos campos do filme que sera adicionado da mensagem do cliente
        if (identifier==-1) { // constroi a query que adiciona o filme
        	sprintf(queryfilter, "INSERT INTO Movies (Id, Title, Genre, Year) VALUES (NULL, \'%s\', \'%s\', %d);", name, genre, year);
        }
        else { // constroi a query que adiciona o filme
        	sprintf(queryfilter, "INSERT INTO Movies (Id, Title, Genre, Year) VALUES (%d, \'%s\', \'%s\', %d);", identifier, name, genre, year);
                                        }

        /*sqlite3*/
        rc = sqlite3_exec(db, queryfilter, NULL, 0, &zErrMsg); // executa a query
}


/*Funcao que executa a operacao de editar um filme*/
void edit_movie(int socket, sqlite3* db, int rc, char *zErrMsg) {

	int identifier;
        char new_genre[100];
        char old_genre[100];
        char queryfilter[300];
        int numbytes;
        char buffer[300];
        if (send(socket, "Por favor digite o ideintificador do filme que deseja editar\n", 61, 0) == -1)
        	perror("edit_movie requisicao 1");
        if ((numbytes=recv(socket, buffer, 299, 0))==-1) {
        	perror("edit_movie resposta 1");
                exit(1);
        }
        buffer[numbytes] ='\0';
        sscanf(buffer,"%d %[^\n]", &identifier, new_genre); // extrai os campos do filme que sera editado da mensagem do cliente
        sprintf(queryfilter, "SELECT Id, Genre FROM Movies WHERE Id = %d;", identifier); // constroi a query que encontra o filme no banco de dados

        /*sqlite3*/
        rc = sqlite3_exec(db,queryfilter, edit,(void*)old_genre,&zErrMsg); //executa a query que encontra o filme no banco de dados
        strcat(old_genre, ", ");
        strcat(old_genre, new_genre);
        sprintf(queryfilter, "UPDATE Movies SET Genre = \'%s\' WHERE Id = %d;", old_genre, identifier); // constroi a query de atualizacao
        rc = sqlite3_exec(db, queryfilter, NULL, NULL, &zErrMsg); // executa a query de atualizacao
}

/* Funcao que executa a operacao de deletar um filme do banco de dados */
void delete_movie(int socket, sqlite3* db, int rc, char *zErrMsg) {

	int identifier;
        char queryfilter[300];
        int numbytes;
        if (send(socket, "Por favor digite o identificador do filme que deseja deletar\n", 62, 0) == -1)
        	perror("delete_movie requisicao 1");
        if ((numbytes=recv(socket, &identifier, 4, 0))==-1) {
        	perror("delete_movie resposta 1");
                exit(1);
        }
        identifier = ntohl(identifier);
        sprintf(queryfilter, "DELETE FROM Movies WHERE Id = %d;", identifier); // constroi a query de deletar o filme

        /*sqlite3*/
        rc = sqlite3_exec(db, queryfilter, NULL, 0, &zErrMsg); // executa a query
}

/* Funcao que realiza a operacao de filtrar filmes */
void filter_movies(int socket, sqlite3* db, int rc, char *zErrMsg) {

	char queryfilter[300];
	char countfilter[500];
        int numbytes;
        short typefilter;
	int total_bytes = 28;
	void* addrs_total_bytes = (void*) &total_bytes;
        if (send(socket, "Por favor selecione um dos filtros abaixo:\n(1)Todos os filmes\n(2)Filtrar por genero\n(3)Filtrar por identificador\n", 113, 0) == -1)
        	perror("filte_movies requisicao 1");
        if ((numbytes=recv(socket, &typefilter, 2, 0))==-1) {
        	perror("filter_movies resposta 1");
                exit(1);
        }
        typefilter = ntohs(typefilter);
        if (typefilter == 1) // usuario quer filtrar todos os filmes
        	sprintf(queryfilter, "SELECT * FROM Movies;"); // constroi query que filtra todos os filmes
        else if (typefilter == 2) { // usuario quer filtrar um genero
        	char genre[100];
                if (send(socket, "Por favor selecione um genero para filtrar:\n",44,0)==-1)
                	perror("filter_movies requisicao 2");
                if ((numbytes=recv(socket, genre,99,0))==-1) {
                	perror("filter_movies resposta 2");
                        exit(1);
                }
                genre[numbytes] = '\0';
                sprintf(queryfilter, "SELECT * FROM Movies WHERE Genre LIKE \'%%%s%%\';", genre); // constroi query que filtra genero
        }
	else if (typefilter == 3) { // usuario quer filtrar um identificador
        	int identifier;
                if (send(socket, "Por favor selecione um identificador para filtrar:\n", 51, 0)==-1)
                	perror("filter_movies requisicao 3");
                if ((numbytes=recv(socket,&identifier,4,0))==-1) {
                	perror("filter_movies requisicao 3");
                        exit(1);
                }
                identifier = ntohl(identifier);
                sprintf(queryfilter, "SELECT * FROM Movies WHERE Id = \"%d\";", identifier); // constroi a query que filtra um identificador
        }

	sprintf(countfilter,"SELECT COUNT(*), SUM(LENGTH(Id)), SUM(LENGTH(Title)), SUM(LENGTH(Genre)), SUM(LENGTH(YEAR)) FROM (%s);", queryfilter); // constroi a query que conta o numero de bytes que serao enviados ao cliente
	countfilter[strlen(countfilter)-3] = ' ';
	rc = sqlite3_exec(db, countfilter, count_bytes, addrs_total_bytes, &zErrMsg); // executa a query que conta os bytes
	total_bytes = htonl(total_bytes);
	if (send(socket, &total_bytes, 4, 0)==-1)
		perror("filter_movies tamanho");

        /*sqlite3*/
        void* param = &socket;
        rc = sqlite3_exec(db, queryfilter, filter, param, &zErrMsg); // executa a query de filtro

}

void show_all_ids(int socket, sqlite3* db, int rc, char *zErrMsg) {

	char queryfilter[300];
	char countfilter[500];
        int numbytes;
	int total_bytes = 2;
	void* addrs_total_bytes = (void*) &total_bytes;
        sprintf(queryfilter, "SELECT Id, Title FROM Movies;"); // constroi a query que seleciona os ids e nomes de todos os filmes
	sprintf(countfilter,"SELECT COUNT(*), SUM(LENGTH(Id)), SUM(LENGTH(Title)) FROM (%s);", queryfilter); // constroi a query que conta o numero de bytes que sera enviado para o cliente
        countfilter[strlen(countfilter)-3] = ' ';

	rc = sqlite3_exec(db, countfilter, count_bytes, addrs_total_bytes, &zErrMsg); // executa a query que conta os bytes
        total_bytes = htonl(total_bytes);
        if (send(socket, &total_bytes, 4, 0)==-1)
                perror("show_all_ids tamanho");

        /*sqlite3*/
        void* param = &socket;
        rc = sqlite3_exec(db,queryfilter, show_ids, param, &zErrMsg); // executa a query que seleciona os ids e nomes de todos os filmes
}

void sigchld_handler(int s) {
	int saved_errno = errno;
	while(waitpid(-1,NULL, WNOHANG) > 0);
	errno = saved_errno;
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

	short choice = 0; // escolha do usuario
	int numbytes; //numero de bytes recebidos
	char firstmenu[300];
	sprintf(firstmenu,"Bem vindo, por favor escolha uma das op????es abaixo:\n(1)Logar\n(2)Cadastrar filmes\n(3)Editar um filme\n(4)Deletar um filme\n(5)Filtrar\n(6)Listar identificadores\n(7)Sair\n");

	if (send(socket, firstmenu, strlen(firstmenu), 0) == -1)
                                perror("process_requests requisicao 1");
                        while (choice != 7) {
                                if ((numbytes=recv(socket, &choice, 2, 0))==-1) {
                                        perror("process_requests resposta 1");
                                        exit(1);
                                }
                                choice = ntohs(choice);
                                if (choice == 1) {
                                        if (send(socket, "Por favor digite seu id de usuario\n", 35, 0) == -1)
                                                perror("user_id");
                                }
                                else if (choice == 2) {
					add_movie(socket,db,rc,zErrMsg);
                                }
                                else if (choice == 3) {
					edit_movie(socket,db,rc,zErrMsg);
                                }
				else if (choice == 4) {
					delete_movie(socket,db,rc,zErrMsg);
                                }
                                else if (choice == 5) {
					filter_movies(socket,db,rc,zErrMsg);
                                }
                                else if (choice == 6) {
					show_all_ids(socket,db,rc,zErrMsg);
                                }
                        }
			sqlite3_close(db);
}

int main () {

	int work;
	work = gethostname(ip, sizeof(ip));
	printf("O ip desse computador ?? %s\n", ip);

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
			process_requests(new_fd);
			close(new_fd);
			exit(0);
		}
		close(new_fd);
	}
}
