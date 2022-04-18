#include <stdio.h>
#include "sqlite3.h"
#include <string.h>

int callback(void *NotUsed, int responses, char ** columns, char ** column_names) {
	for (int i=0;i<responses;i++) {
		printf("%s: %s\n", column_names[i], columns[i]);
	}
	return 0;
}

int inserir_tabela(sqlite3 * db, char *zErrMsg) {
	char barran;
	char titulo[255];
	char diretor[255];
	char ano[255];
	char genero[255];
	char queue[255];
	scanf("%c", &barran);
	printf("Digite o titulo do filme: ");
	scanf("%[^\n]", titulo);
	scanf("%c", &barran);
	printf("Digite o diretor do filme: ");
        scanf("%[^\n]", diretor);
	scanf("%c", &barran);
	printf("Digite o ano do filme: ");
        scanf("%s", ano);
	scanf("%c", &barran);
	printf("Digite o genero do filme: ");
        scanf("%[^\n]", genero);
	strcpy(queue,"INSERT INTO Movies VALUES (\"");
        strcat(queue,titulo);
       	strcat(queue,"\",\"");
	strcat(queue,diretor);
	strcat(queue,"\",");
        strcat(queue, ano);
        strcat(queue,",\"");
        strcat(queue,genero);
	strcat(queue,"\");");
	printf("%s\n",queue);
	return sqlite3_exec(db, queue,NULL,0, &zErrMsg);
}

int main() {

	sqlite3 *db;
	int rc;
	char *zErrMsg = 0;
	int choice = 0;
	rc = sqlite3_open("test.db", &db);
	while (choice != 3) {
		printf("Por favor selecione uma das opcoes\n");
		printf("(1) Imprimir conteudo do banco de dados\n");
		printf("(2) Inserir uma linha no banco de dados\n");
		printf("(3) Sair");
		scanf("%d", &choice);
		if (choice == 1) {
			rc = sqlite3_exec(db, "SELECT * FROM Movies;", callback, 0, &zErrMsg);
		}
		else if (choice == 2) {
			inserir_tabela(db,zErrMsg);
		}
	}
	sqlite3_close(db);
	return 0;
}
