
/** \file user_data.c  
       \author Giuseppe Muntoni 544943
       Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
       originale dell'autore  
     */  

#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include "user_data.h"

#define BITS_IN_int     ( sizeof(int) * CHAR_BIT )
#define THREE_QUARTERS  ((int) ((BITS_IN_int * 3) / 4))
#define ONE_EIGHTH      ((int) (BITS_IN_int / 8))
#define HIGH_BITS       ( ~((unsigned int)(~0) >> ONE_EIGHTH ))

/* FUNZIONE HASH */
static unsigned int
hash_pjw(void* key) {
   char *datum = (char *)key;
   unsigned int hash_value, i;

   if(!datum) return 0;

   for (hash_value = 0; *datum; ++datum) {
      hash_value = (hash_value << ONE_EIGHTH) + *datum;
      if ((i = hash_value & HIGH_BITS) != 0)
         hash_value = (hash_value ^ (i >> THREE_QUARTERS)) & ~HIGH_BITS;
   }
   return (hash_value);
}

static void free_key(void* key) {
   if (key) free(key);
}

user_data_t* user_data_init(char* nick, int fd, int history_dim, int name_files_table_dim) {
   if (!nick || fd < 0 || history_dim <= 0 || name_files_table_dim <= 0) 
   	return NULL;

   user_data_t* user_data = (user_data_t*) malloc(sizeof(user_data_t));
   if (user_data == NULL)
      return NULL;

   user_data -> history = initBQueue(history_dim);
   if (!(user_data -> history)) {
      free(user_data);
      return NULL;
   }

   user_data -> name_files_rcvd = icl_hash_create(name_files_table_dim, NULL, NULL);
   if (!(user_data -> name_files_rcvd)) {
      destroyBQueue(user_data -> history, NULL);
      free(user_data);
      return NULL;
   }

   user_data -> fd = fd;
   user_data -> id = hash_pjw(nick);
   user_data -> num_hist_msgs = 0;

   return user_data;
}

void user_data_destroy(void* user_data) {
   if ((user_data_t*)user_data) {
      user_data_t* data = (user_data_t*)user_data;
      if (data -> history) destroyBQueue(data -> history, free_history_message);
      if (data -> name_files_rcvd) icl_hash_destroy(data -> name_files_rcvd, free_key, NULL);
      if (data -> fd != -1) close(data -> fd);
      free(data);
   }
}

op_res_t set_fd(user_data_t* user_data, int fd) {
   if (!user_data)
      return ILLEGAL_ARGUMENT;

   if (fd == -1) {
      if (user_data -> fd != -1) close(user_data -> fd);
   } 
   user_data -> fd = fd;
    
   return REQUEST_OK;
}

op_res_t get_fd(user_data_t* user_data, int* fd) {
   if (!user_data || !fd)
      return ILLEGAL_ARGUMENT;

   *fd = user_data -> fd;

   return REQUEST_OK;
}

op_res_t get_id(user_data_t* user_data, int* id) {
   if (!user_data || !id)
      return ILLEGAL_ARGUMENT;

   *id = user_data -> id;

   return REQUEST_OK;
}

op_res_t get_num_hist_msgs(user_data_t* user_data, int* num_hist_msgs) {
   if (!user_data || !num_hist_msgs)
      return ILLEGAL_ARGUMENT;
   
   *num_hist_msgs = user_data -> num_hist_msgs;

   return REQUEST_OK;
}

op_res_t insert_message(user_data_t* user_data, history_msg_t* msg) {
   if (!user_data || !(user_data -> history))
      return ILLEGAL_ARGUMENT;
    
   if (!msg) 
      return ILLEGAL_ARGUMENT;

   if (pushBQueue(user_data -> history, msg) == -1) {
      history_msg_t* hist_msg = popBQueue(user_data -> history);
      free_history_message(hist_msg);
      pushBQueue(user_data -> history, msg);
   } 
   else {
      (user_data -> num_hist_msgs)++;
   }

   return REQUEST_OK;
}

op_res_t history_iterator_open(user_data_t* user_data, history_iterator_t* iterator) {
   if (!user_data || !(user_data -> history))
      return ILLEGAL_ARGUMENT;
   
   BQueue_iterator_t* it = BQueue_iterator_init(user_data -> history);
   if (!it)
      return SYSTEM_ERROR;
   
   *iterator = it;

   return REQUEST_OK;
}

void history_iterator_close(history_iterator_t iterator) {
   BQueue_iterator_destroy(iterator);
}

history_msg_t* history_iterate(history_iterator_t iterator) {
   return BQueue_iterator_next(iterator);
}

op_res_t insert_file_name(user_data_t* user_data, char* file_name) {
   if (!user_data || !(user_data -> name_files_rcvd)) 
      return ILLEGAL_ARGUMENT;

   if (!file_name) 
      return ILLEGAL_ARGUMENT;

   if (icl_hash_find(user_data -> name_files_rcvd, file_name) != NULL)
      return ALREADY_INSERTED;

   if (icl_hash_insert(user_data -> name_files_rcvd, file_name, file_name) == NULL)
      return SYSTEM_ERROR;

   return REQUEST_OK;
}

op_res_t search_file_name(user_data_t* user_data, char* file_name) {
   if (!user_data || !(user_data -> name_files_rcvd))
      return ILLEGAL_ARGUMENT;

   if (!file_name) 
      return ILLEGAL_ARGUMENT;
    
   if (icl_hash_find(user_data -> name_files_rcvd, file_name) == NULL)
      return NOT_FOUND;
    
   return FOUND;
}

op_res_t remove_all_file(user_data_t* user_data, char* dir_name) {
   if (!user_data || !(user_data -> name_files_rcvd) || !dir_name)
      return ILLEGAL_ARGUMENT;

   icl_iterator_t* iterator = icl_iterator_create(user_data -> name_files_rcvd);
   if (!iterator)
      return SYSTEM_ERROR;

   icl_entry_t* entry;
   char* file_name;
   char file_path[2048];
   while((entry = icl_hash_iterate(iterator)) != NULL) {
      file_name = entry -> data;
      memset(file_path, '\0', 2048);
      strncpy(file_path, dir_name, strlen(dir_name));
      strncat(file_path, "/", 1);
      strncat(file_path, file_name, strlen(file_name));
      remove(file_path);
   }

   icl_iterator_destroy(iterator);

   return REQUEST_OK;
}