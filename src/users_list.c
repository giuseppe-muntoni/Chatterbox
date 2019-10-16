
/** \file users_list.c  
       \author Giuseppe Muntoni
       Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
       originale dell'autore  
     */  

#include <stdlib.h>
#include <string.h>
#include "users_list.h"
#include "config.h"

/** Cerca la prima occorrenza della stringa to_search nella stringa str_nick che ha dimensione dim 
*/
static char* search_substring(char *str_nick, char *to_search, int dim) {
	char *curr = str_nick;
	
	for (int bytes_read = 0; bytes_read < dim; bytes_read += (MAX_NAME_LENGTH + 1)) {
		if (strcmp(to_search, curr) == 0) return curr;
		curr += (MAX_NAME_LENGTH + 1);
	}

	return NULL;
}

/* FUNZIONI DI INTERFACCIA */
users_list_t* users_list_init() {
	users_list_t* users_list = (users_list_t*) malloc(sizeof(users_list_t));
	if (!users_list)
		return NULL;

	if (pthread_mutex_init(&(users_list -> mtx), NULL) != 0) {
		free(users_list);
		return NULL;
	}

	users_list -> str = NULL;
	users_list -> dim = 0;

	return users_list;
}

void users_list_destroy(users_list_t* users_list) {
	if (users_list) {
		if (users_list -> str) free(users_list -> str);
		pthread_mutex_destroy(&(users_list -> mtx));
		free(users_list);
	}
}

op_res_t users_list_lock(users_list_t* users_list) {
	if (!users_list) 
		return ILLEGAL_ARGUMENT;

	pthread_mutex_lock(&(users_list -> mtx));

	return REQUEST_OK;
}

op_res_t users_list_unlock(users_list_t* users_list) {
	if (!users_list) 
		return ILLEGAL_ARGUMENT;

	pthread_mutex_unlock(&(users_list -> mtx));

	return REQUEST_OK;
}

op_res_t users_list_insert(users_list_t* users_list, char* nick) {
	if (!users_list)
		return ILLEGAL_ARGUMENT;

	if (!nick || strlen(nick) > MAX_NAME_LENGTH)
		return ILLEGAL_ARGUMENT;
	
	int dim_copy = users_list -> dim;

	users_list -> dim += (MAX_NAME_LENGTH + 1);

	char *str = realloc(users_list -> str, users_list -> dim);
	if (str == NULL) 
		return SYSTEM_ERROR;
	
	users_list -> str = str;

	char* curr = (users_list -> str) + dim_copy;
	memset(curr, '\0', MAX_NAME_LENGTH + 1);
	strncpy(curr, nick, strlen(nick));

	return REQUEST_OK;
}

op_res_t users_list_remove(users_list_t* users_list, char* nick) {
	if (!users_list || !(users_list -> str))
		return ILLEGAL_ARGUMENT;
	
	if (!nick || strlen(nick) > MAX_NAME_LENGTH)
		return ILLEGAL_ARGUMENT;

	char *to_cancel = search_substring(users_list -> str, nick, users_list -> dim);
	if (to_cancel == NULL) return REQUEST_OK;
	
	memset(to_cancel, '\0', MAX_NAME_LENGTH + 1);

	char *last_nick = (users_list -> str) + (users_list -> dim) - (MAX_NAME_LENGTH + 1);

	char *ptr_last_nick = last_nick;
	while ((*ptr_last_nick) != '\0') {
		*to_cancel = *ptr_last_nick;
		to_cancel++;
		ptr_last_nick++;
	}
	memset(last_nick, '\0', MAX_NAME_LENGTH + 1);

	users_list -> dim -= (MAX_NAME_LENGTH + 1);
	if (users_list -> dim == 0) {
		free(users_list -> str);
		users_list -> str = NULL;
	}
	else {
		char *str = realloc(users_list -> str, users_list -> dim);
		if (str == NULL) return SYSTEM_ERROR;
		users_list -> str = str;
	}

	return REQUEST_OK;
}

char* get_string(users_list_t* users_list) {
	if (!users_list)
		return NULL;
	
	return (users_list -> str);
}

int get_dim(users_list_t* users_list) {
	if (!users_list)
		return -1;
	
	return (users_list -> dim);
}
