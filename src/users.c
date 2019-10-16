
/** \file users.c  
       \author Giuseppe Muntoni
       Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
       originale dell'autore  
     */  

#include <stdlib.h>
#include "users.h"
#include "parser.h"

static inline unsigned int fnv_hash_function( void *key, int len ) {
   unsigned char *p = (unsigned char*)key;
   unsigned int h = 2166136261u;
   int i;
   for ( i = 0; i < len; i++ )
      h = ( h * 16777619 ) ^ p[i];
   return h;
}

/* funzioni hash per per l'hashing di interi */
static inline unsigned int uint_hash_function( void *key ) {
   int len = sizeof(unsigned int);
   unsigned int hashval = fnv_hash_function( key, len );
   return hashval;
}
static inline int uint_key_compare( void *key1, void *key2  ) {
   return ( *(unsigned int*)key1 == *(unsigned int*)key2 );
}

static void free_key(void* key) {
	if (key) free(key);
}

/* FUNZIONI DI INTERFACCIA */
users_t* users_init(int dim, int num_logical_block, int max_conn) {
	//controllo parametri
	if (dim <= 0 || num_logical_block < 0) return NULL;

	int nmtx_reg_user_init;
	int nmtx_fd_to_nick_init;
	users_t* users;

	users = (users_t*) malloc(sizeof(users_t));
	if (users == NULL) return NULL;

	users -> reg_users = icl_hash_create(dim, NULL, NULL);
	if (users -> reg_users == NULL) goto error;

	users -> fd_to_nick = icl_hash_create(MaxConnections, uint_hash_function, uint_key_compare);
	if (users -> fd_to_nick == NULL) goto error;

	//num_logical_block != 0 se multiThreaded
	if (num_logical_block != 0) {
		users -> mtx_reg_users = (pthread_mutex_t*) malloc(num_logical_block*sizeof(pthread_mutex_t));
		if (users -> mtx_reg_users == NULL) goto error;
		
		nmtx_reg_user_init = 0;
		for (int i = 0; i < num_logical_block; i++) {
			if (pthread_mutex_init((users -> mtx_reg_users) + i, NULL) != 0) goto error;
			nmtx_reg_user_init++;
		}

		users -> mtx_fd_to_nick = (pthread_mutex_t*) malloc(num_logical_block*sizeof(pthread_mutex_t));
		if (users -> mtx_fd_to_nick == NULL) goto error;

		nmtx_fd_to_nick_init = 0;
		for (int i = 0; i < num_logical_block; i++) {
			if (pthread_mutex_init((users -> mtx_fd_to_nick) + i, NULL) != 0) goto error;
			nmtx_fd_to_nick_init++;
		}

		if (pthread_mutex_init(&(users -> mtx_num_users), NULL) != 0) goto error;
	}
	//num_logical_block = 0 se non c'è bisogno di mutex (versione singleThreaded)
	else {
		users -> mtx_reg_users = NULL;
		users -> mtx_fd_to_nick = NULL;
	}

	users -> num_logical_block = num_logical_block;
	users -> max_conn = max_conn;
	users -> num_users_conn = 0;
	users -> num_users_reg = 0;

	return users;

	error: 
	{
		if (users) {
			if (users -> reg_users) icl_hash_destroy(users -> reg_users, NULL, NULL);
			if (users -> fd_to_nick) icl_hash_destroy(users -> fd_to_nick, NULL, NULL);
			if (users -> mtx_reg_users) {
				for (int i = 0; i < nmtx_reg_user_init; i++)
					pthread_mutex_destroy((users -> mtx_reg_users) + i);
				free(users -> mtx_reg_users);
			}
			if (users -> mtx_fd_to_nick) {
				for (int i = 0; i < nmtx_fd_to_nick_init; i++)
					pthread_mutex_destroy((users -> mtx_fd_to_nick) + i);
				free(users -> mtx_fd_to_nick);
			}
			free(users);
		}	
		return NULL;
	}	
}

void users_destroy(users_t* users) {
	if (!users) return;

	if (users -> reg_users) icl_hash_destroy(users -> reg_users, free_key, user_data_destroy);
	
	if (users -> fd_to_nick) icl_hash_destroy(users -> fd_to_nick, free_key, free_key);
	
	if (users -> num_logical_block != 0) {
		if (users -> mtx_reg_users) {
			for (int i = 0; i < users -> num_logical_block; i++)
				pthread_mutex_destroy((users -> mtx_reg_users) + i);
			free(users -> mtx_reg_users);
		}
	
		if (users -> mtx_fd_to_nick) {
			for (int i = 0; i < users -> num_logical_block; i++)
				pthread_mutex_destroy((users -> mtx_fd_to_nick) + i); 
			free(users -> mtx_fd_to_nick);
		}

		pthread_mutex_destroy(&(users -> mtx_num_users));
	}

	free(users);
}

op_res_t users_table_lock(users_t* users, char* nick) {
	if (!users || !nick) 
		return ILLEGAL_ARGUMENT;
	
	if (users -> num_logical_block != 0) {
		int hash_val = (* users -> reg_users -> hash_function)(nick) % (users -> reg_users -> nbuckets);
		pthread_mutex_lock((users -> mtx_reg_users) + (hash_val/(users -> num_logical_block)));
	}

	return REQUEST_OK;
}

op_res_t users_table_unlock(users_t* users, char* nick) {
	if (!users || !nick) 
		return ILLEGAL_ARGUMENT;
	
	if (users -> num_logical_block != 0) {
		int hash_val = (* users -> reg_users -> hash_function)(nick) % (users -> reg_users -> nbuckets);
		pthread_mutex_unlock((users -> mtx_reg_users) + (hash_val/(users -> num_logical_block)));
	}

	return REQUEST_OK;
}

op_res_t users_table_lock_all(users_t* users) {
   if (!users)
      return ILLEGAL_ARGUMENT;

   if (users -> num_logical_block != 0) {
      for (int i = 0; i < (users -> num_logical_block); i++)
         pthread_mutex_lock((users -> mtx_reg_users) + i);
   }

   return REQUEST_OK;
}

op_res_t users_table_unlock_all(users_t* users) {
   if (!users)
      return ILLEGAL_ARGUMENT;

   if (users -> num_logical_block != 0) {
      for (int i = (users -> num_logical_block)-1; i >= 0; i--)
         pthread_mutex_unlock((users -> mtx_reg_users) + i);
   }

   return REQUEST_OK;
}

op_res_t fd_to_nick_table_lock(users_t* users, int fd) {
	if (!users)
		return ILLEGAL_ARGUMENT;

	if (fd < 0) 
		return ILLEGAL_ARGUMENT;
	
	if (users -> num_logical_block != 0) {
		int hash_val = (* users -> fd_to_nick -> hash_function)(&fd) % (users -> fd_to_nick -> nbuckets);
		pthread_mutex_lock((users -> mtx_fd_to_nick) + (hash_val/(users -> num_logical_block)));
	}

	return REQUEST_OK;
}

op_res_t fd_to_nick_table_unlock(users_t* users, int fd) {
	if (!users)
		return ILLEGAL_ARGUMENT;

	if (fd < 0) 
		return ILLEGAL_ARGUMENT;
	
	if (users -> num_logical_block != 0) {
		int hash_val = (* users -> fd_to_nick -> hash_function)(&fd) % (users -> fd_to_nick -> nbuckets);
		pthread_mutex_unlock((users -> mtx_fd_to_nick) + (hash_val/(users -> num_logical_block)));
	}

	return REQUEST_OK;
}

op_res_t num_users_lock(users_t* users) {
	if (!users) 
		return ILLEGAL_ARGUMENT;

	if (users -> num_logical_block != 0)
		pthread_mutex_lock(&(users -> mtx_num_users));

	return REQUEST_OK;
}

op_res_t num_users_unlock(users_t* users) {
	if (!users) 
		return ILLEGAL_ARGUMENT;

	if (users -> num_logical_block != 0)
		pthread_mutex_unlock(&(users -> mtx_num_users));

	return REQUEST_OK;
}

op_res_t users_table_insert(users_t* users, char* nick, user_data_t* user_data) {
	if (!users || !(users -> reg_users) || !nick || !user_data) 
		return ILLEGAL_ARGUMENT;

	if (icl_hash_find(users -> reg_users, nick) != NULL) 
		return ALREADY_INSERTED;

	if (icl_hash_insert(users -> reg_users, nick, user_data) == NULL)
		return SYSTEM_ERROR;

	return REQUEST_OK;
}

op_res_t users_table_delete(users_t* users, char* nick) {
	if (!users || !(users -> reg_users) || !nick) 
		return ILLEGAL_ARGUMENT;

	if (icl_hash_delete(users -> reg_users, nick, free_key, user_data_destroy) == -1) 
		return NOT_FOUND;
	
	return REQUEST_OK;
}

user_data_t* get_user_data(users_t* users, char* nick) {
	if (!users || !(users -> reg_users) || !nick)
		return NULL;
	
	user_data_t* user_data = icl_hash_find(users -> reg_users, nick);

	return user_data;
}

op_res_t users_table_iterator_init(users_t* users, users_table_iterator_t* iterator) {
   if (!users || !(users -> reg_users) || !iterator)
      return ILLEGAL_ARGUMENT;

   iterator -> reg_users = users -> reg_users;
   iterator -> iterator = icl_iterator_create(users -> reg_users);

   if (iterator -> iterator == NULL)
      return SYSTEM_ERROR;

   return REQUEST_OK;
}

op_res_t users_table_iterate(users_table_iterator_t users_table_iterator, iterator_element_t* element) {
   if (!element)
      return ILLEGAL_ARGUMENT;

   icl_entry_t* icl_entry = icl_hash_iterate(users_table_iterator.iterator);
   if (icl_entry == NULL)
      return NOT_FOUND;

   element -> nick = icl_entry -> key;
   element -> user_data = icl_entry -> data;

   return REQUEST_OK;
}

op_res_t users_table_iterator_close(users_table_iterator_t* users_table_iterator) {
   if (!users_table_iterator)
      return ILLEGAL_ARGUMENT;

   icl_iterator_destroy(users_table_iterator -> iterator);

   return REQUEST_OK;
}

op_res_t fd_to_nick_insert(users_t* users, int* fd, char* nick) {
	if (!users || !(users -> fd_to_nick) || !fd || !nick)
		return ILLEGAL_ARGUMENT;

	if (icl_hash_find(users -> fd_to_nick, fd) != NULL)
		return ALREADY_INSERTED;

	if (icl_hash_insert(users -> fd_to_nick, fd, nick) == NULL)
		return SYSTEM_ERROR;

	return REQUEST_OK;
}

op_res_t fd_to_nick_delete(users_t* users, int fd) {
	if (!users || !(users -> fd_to_nick))
		return ILLEGAL_ARGUMENT;
	
	if (fd < 0) 
		return ILLEGAL_ARGUMENT;

	if (icl_hash_delete(users -> fd_to_nick, &fd, free_key, free_key) == -1)
		return NOT_FOUND;
	
	return REQUEST_OK;
}

char* get_nick(users_t* users, int fd) {
	if (!users || !(users -> fd_to_nick)) 
		return NULL;
	
	if (fd < 0)
		return NULL;
	
	char* nick = icl_hash_find(users -> fd_to_nick, &fd);

	return nick;
}

op_res_t testAndInc_num_users_conn(users_t* users) {
	if (!users)
		return ILLEGAL_ARGUMENT;
	
	if (users -> num_users_conn == users -> max_conn) 
		return CONN_LIMIT_REACHED;

	(users -> num_users_conn)++;

	return REQUEST_OK;
}

op_res_t dec_num_users_conn(users_t* users) {
	if (!users)
		return ILLEGAL_ARGUMENT;
	
	(users -> num_users_conn)--;

   return REQUEST_OK;
}

op_res_t get_num_users_conn(users_t* users, int* num) {
   if (!users || !num)
      return ILLEGAL_ARGUMENT;

   *num = users -> num_users_conn;

   return REQUEST_OK;
}

op_res_t inc_num_users_reg(users_t* users) {
	if (!users)
		return ILLEGAL_ARGUMENT;
	
	(users -> num_users_reg)++;

	return REQUEST_OK;
}

op_res_t dec_num_users_reg(users_t* users) {
	if (!users)
		return ILLEGAL_ARGUMENT;
	
	(users -> num_users_reg)--;

	return REQUEST_OK;
}

op_res_t get_num_users_reg(users_t* users, int* num) {
   if (!users || !num)
      return ILLEGAL_ARGUMENT;

   *num = users -> num_users_reg;

   return REQUEST_OK;
}
