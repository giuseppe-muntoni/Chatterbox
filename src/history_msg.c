
/** \file history_msg.c 
       \author Giuseppe Muntoni
       Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
       originale dell'autore  
     */  

#include <stdlib.h>
#include "history_msg.h"
#include "config.h"

history_msg_t* init_history_message(message_t msg, boolean_t sended) {
   if (msg.hdr.op != TXT_MESSAGE && msg.hdr.op != FILE_MESSAGE)
      return NULL;

   if (msg.data.hdr.len == 0)
      return NULL;
    
   history_msg_t* history_msg = (history_msg_t*) malloc(sizeof(history_msg_t));
   if (!history_msg)
      return NULL;

   /* EFFETTUO UNA DEEP COPY DEL MESSAGGIO */
   message_t* msg_copy = (message_t*) malloc(sizeof(message_t));
   if (!msg_copy) {
      free(history_msg);
      return NULL; 
   }
   memset(msg_copy, '\0', sizeof(message_t));

   setHeader(&(msg_copy -> hdr), msg.hdr.op, msg.hdr.sender);

   (msg_copy -> data).hdr.len = msg.data.hdr.len;

   (msg_copy -> data).buf = (char*) malloc(((msg.data.hdr.len)+1)*sizeof(char));
   if (!((msg_copy -> data).buf)) {
      free(history_msg);
      free(msg_copy);
      return NULL;
   }
   memset((msg_copy -> data).buf, '\0', msg.data.hdr.len+1);
   strncpy((msg_copy -> data).buf, msg.data.buf, msg.data.hdr.len);

   memset((msg_copy -> data).hdr.receiver, '\0', MAX_NAME_LENGTH+1); 
   strncpy((msg_copy -> data).hdr.receiver, msg.data.hdr.receiver, MAX_NAME_LENGTH);

   history_msg -> msg = msg_copy;

   history_msg -> sended = sended;

   return history_msg;
}

void free_history_message(void* history_msg) {
   if ((history_msg_t*)history_msg) {
      history_msg_t* hist_msg = history_msg;
      if (hist_msg -> msg) freeMessage(hist_msg -> msg);
      free(hist_msg);
   }
}

op_res_t set_sended(history_msg_t* history_msg, boolean_t sended) {
   if (!history_msg)
      return ILLEGAL_ARGUMENT;

   history_msg -> sended = sended;

   return REQUEST_OK; 
}

op_res_t get_sended(history_msg_t* history_msg, boolean_t* sended) {
   if (!history_msg)
      return ILLEGAL_ARGUMENT;
    
   *sended = history_msg -> sended;

   return REQUEST_OK;
}


