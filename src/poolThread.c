
/** \file poolThread.c  
       \author Giuseppe Muntoni
       Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
       originale dell'autore  
     */  

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "conn.h"
#include "queue.h"
#include "message.h"
#include "ops.h"
#include "parser.h"
#include "operations.h"
#include "connections.h"

/**	Valore speciale che indica che un thread del pool deve terminare
 */
#ifndef POOL_TERM
#define POOL_TERM (void*)-1
#endif

extern Queue_t *codaFd;										//Coda dei descrittori condivisa con il thread listener
extern int fdpipe[2];										//Pipe per la comunicazione dei descrittori dei client per i quali ho finito di gestire la richiesta
extern int countActiveThreads;							//Numero di threads del pool attualmente attivi
extern pthread_mutex_t mtx_countActiveThreads;		//Mutex su countActiveThreads

/**	Decrementa di uno il numero di threads del pool attivi (thread_safe)
 */
static void update_countActiveThreads() {
	pthread_mutex_lock(&mtx_countActiveThreads);
	countActiveThreads--;
	pthread_mutex_unlock(&mtx_countActiveThreads);
}

/**	Notifica al thread listener, tramite la pipe, il descrittore del client per il quale ha concluso la gestione della richiesta 
 */
static int communicate_request_completed(int fd) {
	if (writen(fdpipe[1], &fd, sizeof(int)) == -1) {
		disconnect_op(fd);
		return -1;
	}
	return 0;
}

void* pool_func() {
	message_t request;	//Richiesta dal client
	int fd;					//Descrittore del client che ha effettuato la richiesta
	int read_res;			//Risultato lettura richiesta	
	op_res_t op_res;		//Risultato gestione operazione
	void *data;		

	request.data.buf  = NULL;
	
	while(1) {
		//Estraggo il descrittore dalla coda
		data = popQueue(codaFd);
		
		//Verifico se devo terminare
		if (data == POOL_TERM)
		   break;

		fd = *(int*)data;
		free(data);
		
		//Leggo la richiesta
		read_res = readMsg(fd, &request);
		//Controlle esito
		if (read_res == -1) {
		   close(fd);
			update_countActiveThreads();
			return (void*)1;
		}
		else if (read_res == 0) {
			if (disconnect_op(fd) == SYSTEM_ERROR) {
				printf("Errore di sistema nella disconessione\n");
				update_countActiveThreads();
				return (void*)1;
			}
		}
		else if (read_res > 0) {
			switch(request.hdr.op) {
				case REGISTER_OP: {
					op_res = register_op(fd, request);
					if (op_res == REQUEST_OK) {
						if (communicate_request_completed(fd) == -1) {
							update_countActiveThreads();
							return (void*)1;
						}
					}
					else if (op_res == SYSTEM_ERROR) {
						printf("Errore di sistema nella registrazione\n");
						update_countActiveThreads();
						return (void*)1;
					}
					break;
				}
				case CONNECT_OP: {
					op_res = connect_op(fd, request);
				   if (op_res == REQUEST_OK) {
						if (communicate_request_completed(fd) == -1) {
							update_countActiveThreads();
							return (void*)1;
						}
					}
					else if (op_res == SYSTEM_ERROR) {
						printf("Errore di sistema nella connessione\n");
						update_countActiveThreads();
						return (void*)1;
					}
					break;
				}
				case POSTTXT_OP: {
					op_res = posttxt_op(fd, request);
					if (op_res == REQUEST_OK) {
						if (communicate_request_completed(fd) == -1) {
							update_countActiveThreads();
							return (void*)1;
						}
					}
					else if (op_res == SYSTEM_ERROR) {
						printf("Errore di sistema nell'invio di un messaggio\n");
						disconnect_op(fd);
						update_countActiveThreads();
						return (void*)1;
					}
					else if (op_res == CLIENT_ERROR) {
						disconnect_op(fd);
					}
					break;
				}  
				case POSTTXTALL_OP: {
					op_res = posttxtall_op(fd, request);
					if (op_res == REQUEST_OK) {
						if (communicate_request_completed(fd) == -1) {
							update_countActiveThreads();
							return (void*)1;
						}
					}
					else if (op_res == SYSTEM_ERROR) {
						printf("Errore di sistema nell'invio di un messaggio a tutti\n");
						disconnect_op(fd);
						update_countActiveThreads();
						return (void*)1;
					}
					else if (op_res == CLIENT_ERROR) {
						disconnect_op(fd);
					}
					break;
				}
				case POSTFILE_OP: {
               //Leggo il contenuto del file
               message_data_t file_content;
               if (readData(fd, &file_content) == -1) {
                  if (errno == EPIPE) {
                     disconnect_op(fd);
                  }
                  else {
							disconnect_op(fd);
                     update_countActiveThreads();
                     return (void*)1;
                  }
               }
               else {
                  op_res = postfile_op(fd, request, file_content);
                  if (op_res == REQUEST_OK) {
                     if (communicate_request_completed(fd) == -1) {
                        update_countActiveThreads();
                        return (void*)1;
                     }
                  }
                  else if (op_res == SYSTEM_ERROR) {
							printf("Errore di sistema nell'invio di un file da %s a %s\n", request.hdr.sender, request.data.hdr.receiver);
							disconnect_op(fd);
                     update_countActiveThreads();
                     return (void*)1;
                  }
                  else if (op_res == CLIENT_ERROR) {
						   disconnect_op(fd);
					   }
               }
               if (file_content.buf) free(file_content.buf);
               break;
            }
				case GETFILE_OP: {
					op_res = getfile_op(fd, request);
					if (op_res == REQUEST_OK) {
						if (communicate_request_completed(fd) == -1) {
							update_countActiveThreads();
							return (void*)1;
						}
					}
					else if (op_res == SYSTEM_ERROR) {
						printf("Errore di sistema nel download di un file\n");
						disconnect_op(fd);
						update_countActiveThreads();
						return (void*)1;
					}
               else if (op_res == CLIENT_ERROR) {
						disconnect_op(fd);
					}
               break;
				}
				case GETPREVMSGS_OP: {
					op_res = getprevmsgs_op(fd, request);
					if (op_res == REQUEST_OK) {
						if (communicate_request_completed(fd) == -1) {
							update_countActiveThreads();
							return (void*)1;
						}
					}
					else if (op_res == SYSTEM_ERROR) {
						printf("Errore di sistema nell'invio dei messaggi della history\n");
						disconnect_op(fd);
						update_countActiveThreads();
						return (void*)1;
					}
               else if (op_res == CLIENT_ERROR) {
						disconnect_op(fd);
					}
				   break;
				}
				case USRLIST_OP: {
					op_res = usrlist_op(fd, request);
					if (op_res == REQUEST_OK) {
						if (communicate_request_completed(fd) == -1) {
							update_countActiveThreads();
							return (void*)1;
						}
					}
					else if (op_res == SYSTEM_ERROR) {
							printf("Errore di sistema nell'invio della lista degli utenti connessi\n");
							disconnect_op(fd);
							update_countActiveThreads();
							return (void*)1;
					}
               else if (op_res == CLIENT_ERROR) {
						disconnect_op(fd);
					}
					break;
				}
				case UNREGISTER_OP: {
					op_res = unregister_op(fd, request);
					if (op_res == SYSTEM_ERROR){
						printf("Errore di sistema nella deregistrazione\n");
						disconnect_op(fd);
						update_countActiveThreads();
						return (void*)1;
					}
					break;
				}
				default: {
					disconnect_op(fd);
					break;
				}
			}
		}
		if (request.data.buf) {
			free(request.data.buf);
			request.data.buf = NULL;
		}
	}

	return (void*)0;
}
