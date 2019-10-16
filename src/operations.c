
/** \file operations.c  
       \author Giuseppe Muntoni
       Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
       originale dell'autore  
     */  

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "operations.h"
#include "connections.h"
#include "stats.h"
#include "boundedqueue.h"
#include "message.h"
#include "users.h"
#include "users_list.h"
#include "conn.h"
#include "parser.h"

/**	Dimensione della tabella hash
 */
#ifndef DIM_HASH
#define DIM_HASH 1024
#endif

/**	Numero di blocchi logici tabella hash
 */
#ifndef NUM_MTX_HASH
#define NUM_MTX_HASH DIM_HASH/32
#endif

/**	Dimesione tabella hash dei nomi dei file
 */
#ifndef DIM_FILE_TABLE
#define DIM_FILE_TABLE 32
#endif

extern struct statistics chattyStats;		//Struttura dati che mantiene le statistiche (definita in stats.h)
extern pthread_mutex_t chattyStatsMtx;		//Mutex sulle statistiche
extern users_t* users;							//Struttura dati per la gestione degli utenti
extern users_list_t* users_list;				//Struttura deti per la gestione della stringa degli utenti connessi
extern pthread_mutex_t* fd_mtx;				//Array di mutex sui descrittori
extern pthread_mutex_t dir_mtx;				//Mutex per la gestione della directory dei file

/* 
	Funzione thread_safe che effettua l'aggiornamento delle statistiche 
*/
static void update_stats(long nusers, long nonline, long ndeliv, long nnotdeliv, long nfiledeliv, long nfilenotdeliv, long nerr) {

	pthread_mutex_lock(&chattyStatsMtx);
	chattyStats.nusers += nusers;
	chattyStats.nonline += nonline;
	chattyStats.ndelivered += ndeliv;
	chattyStats.nnotdelivered += nnotdeliv;
	chattyStats.nfiledelivered += nfiledeliv;
	chattyStats.nfilenotdelivered += nfilenotdeliv;
	chattyStats.nerrors += nerr;
	pthread_mutex_unlock(&chattyStatsMtx);

}

/*
	Funzione thread_safe che invia messaggio ad un client
	Ritorna -1 in caso di errore di scrittura, 0 in caso di successo
*/
static int send_reply(int user_id, int fd, message_hdr_t *hdr, message_data_t *data) {
	int result = 0;		//VALORE DI RITORNO DELLA FUNZIONE
	
	/*	SE user_id != -1 ALLORA ACQUISICO LA MUTEX SUL DESCRITTORE ALTRIMENTI NO */
	if (user_id !=-1) pthread_mutex_lock(&(fd_mtx[user_id%MaxConnections]));

	/* INVIO L'HEADER SE != NULL, IGNORO EPIPE ED EBADF */
	if (hdr != NULL) {
		if (sendHeader(fd, hdr) == -1) {
			if (errno == EPIPE || errno == EBADF) ;
			else result = -1;
		}
	}

	/* INVIO I DATI DATI SE != NULL, IGNORO EPIPE ED EBADF */
	if (data != NULL && result != -1) {
		if (sendData(fd, data) == -1) {
			if (errno == EPIPE || errno == EBADF) ;
			else result = -1;
		}
	}

	/* RILASCIO LA MUTEX SE E SOLO SE PRIMA ERA STATA ACQUISITA */
	if (user_id != -1) pthread_mutex_unlock(&(fd_mtx[user_id%MaxConnections]));
	
	return result;
}

op_res_t register_op(unsigned int fd, message_t msg) {
	op_res_t result = REQUEST_OK;		//VALORE DI RITORNO DELLA FUNZIONE
	op_res_t func_res;					//RISULTATO DELLE FUNZIONI CHIAMATE
	int max_conn_reached = 0;			//UGUALE A 1 SE E SOLO SE È STATO RAGGIUNTO IL NUMERO MAX DI CONNESSIONI
	int invalid_param = 0;				//UGUALE A 1 SE E SOLO SE UNO DEI PARAMETRI È ILLEGALE
	int inserted_to_users_table = 0;	//UGUALE A 1 SE E SOLO SE USER_DATA É STATO INSERITO NELLA TABELLA DEGLI UTENTI 
	int fd_to_nick_added = 0;			//UGUALE A 1 SE E SOLO SE È STATA AGGIUNTA L'ASSOCIAZIONE FD -> NICK
	int users_list_updated = 0;		//UGUALE A 1 SE E SOLO SE È STATO AGGIUNTO IL NICK ALLA STRINGA DEGLI UTENTI CONNESSI
	int user_id = -1;						//L'ID DELL'UTENTE CHE HA EFFETTUATO LA RICHIESTA
	message_hdr_t header_reply;		//L'HEADER DELLA RISPOSTA
	message_data_t data_reply;			//I DATI DELLA RISPOSTA
	int* new_user_fd = NULL;			//DESCRITTORE DELL'UTENTE DA REGISTRARE
	char* nick = NULL;					//NICKNAME DELL'UTENTE DA REGISTRARE
	char* nick2 = NULL;
	user_data_t* user_data = NULL;	//STRUTTURA DATI DELL'UTENTE DA REGISTRARE

	/* INIZIO CONTROLLO PARAMETRI */
	if (fd < 0) return ILLEGAL_ARGUMENT;

	if (msg.hdr.sender[0] == '\0' || strlen(msg.hdr.sender) > MAX_NAME_LENGTH) {
		invalid_param = 1;
		setHeader(&header_reply, OP_FAIL, "");
		if (send_reply(user_id, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		goto error_register;
	}
	/* FINE CONTROLLO PARAMETRI */

	/* VERIFICO DI NON AVER RAGGIUNTO IL NUMERO MASSIMO DI CONNESSIONI */
	num_users_lock(users);
	func_res = testAndInc_num_users_conn(users);
	num_users_unlock(users);

	if (func_res == CONN_LIMIT_REACHED) {
		max_conn_reached = 1;
		setHeader(&header_reply, OP_FAIL, "");
		if (send_reply(user_id, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		goto error_register;
	}

	/* ALLOCO IL NICK PER LA TABELLA DEGLI UTENTI */
	nick = (char*) malloc((strlen(msg.hdr.sender)+1)*sizeof(char));
	if (!nick) {
		setHeader(&header_reply, OP_FAIL, "");
		send_reply(user_id, fd, &header_reply, NULL);
		result = SYSTEM_ERROR;
		goto error_register;
	}
	memset(nick, '\0', strlen(msg.hdr.sender)+1);
	strncpy(nick, msg.hdr.sender, strlen(msg.hdr.sender));

	/* ALLOCO IL NICK PER L'ASSOCIAZIONE FD -> NICK */
	nick2 = (char*) malloc((strlen(msg.hdr.sender)+1)*sizeof(char));
	if (!nick2) {
		setHeader(&header_reply, OP_FAIL, "");
		send_reply(user_id, fd, &header_reply, NULL);
		result = SYSTEM_ERROR;
		goto error_register;
	}
	memset(nick2, '\0', strlen(msg.hdr.sender)+1);
	strncpy(nick2, msg.hdr.sender, strlen(msg.hdr.sender));

	/* INIZIALIZZO USER_DATA */
	user_data = user_data_init(msg.hdr.sender, fd, MaxHistMsgs, DIM_FILE_TABLE);
	if (!user_data) {
		setHeader(&header_reply, OP_FAIL, "");
		send_reply(user_id, fd, &header_reply, NULL);
		result = SYSTEM_ERROR;
		goto error_register;
	}
	/* SALVO L'ID DELL'UTENTE */
	get_id(user_data, &user_id);
	
	/* INSERISCO I DATI DELL'UTENTE IN USERS */ 
	users_table_lock(users, msg.hdr.sender);
	func_res = users_table_insert(users, nick, user_data);
	users_table_unlock(users, msg.hdr.sender);

	/* CONTROLLO ERRORI INSERIMENTO */
	if (func_res == ALREADY_INSERTED) {
		setHeader(&header_reply, OP_NICK_ALREADY, "");
		/* USER_DATA NON ANCORA PRESENTE NELLA TABELLA DEGLI UTENTI ==> MUTUA ESCLUSIONE NON NECESSARIA */
		if (send_reply(-1, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		goto error_register;
	}
	if (func_res == SYSTEM_ERROR) {
		setHeader(&header_reply, OP_FAIL, "");
		/* USER_DATA NON ANCORA PRESENTE NELLA TABELLA DEGLI UTENTI ==> MUTUA ESCLUSIONE NON NECESSARIA */
		send_reply(-1, fd, &header_reply, NULL);
		result = SYSTEM_ERROR;
		goto error_register;
	}

	/* L'INSERIMENTO NELLA TABELLA DEGLI UTENTI HA AVUTO ESITO POSITIVO */
	inserted_to_users_table = 1;

	/* ALLOCO IL DESCRITTORE */
	new_user_fd = (int*) malloc(sizeof(int));
	if (!new_user_fd) {
		setHeader(&header_reply, OP_FAIL, "");
		send_reply(user_id, fd, &header_reply, NULL);
		result = SYSTEM_ERROR;
		goto error_register;
	}
	*new_user_fd = fd;

	/* AGGIUNGO L'ASSOCIAZIONE DESCRITTORE -> NICK */
	fd_to_nick_table_lock(users, fd);
	func_res = fd_to_nick_insert(users, new_user_fd, nick2); 
	fd_to_nick_table_unlock(users, fd);

	/* CONTROLLO ERRORI */
	if (func_res == SYSTEM_ERROR) {
		setHeader(&header_reply, OP_FAIL, "");
		send_reply(user_id, fd, &header_reply, NULL);
		result = SYSTEM_ERROR;
		goto error_register;
	}
	//ALREADY_INSERTED impossibile: se stessi richiedendo la registrazione due volte fallirei prima

	/* L'AGGIUNTA DELL'ASSOCIAZIONE HA AVUTO ESITO POSITIVO */
	fd_to_nick_added = 1;

	/* AGGIORNO LA STRINGA DEGLI UTENTI CONNESSI */
	users_list_lock(users_list);
	func_res = users_list_insert(users_list, msg.hdr.sender);
	users_list_unlock(users_list);

	/* CONTROLLO ERRORI */
	if (func_res == SYSTEM_ERROR) {
		setHeader(&header_reply, OP_FAIL, "");
		send_reply(user_id, fd, &header_reply, NULL);
		result = SYSTEM_ERROR;
		goto error_register;
	}

	/* L'AGGIORNAMENTO DELLA STRINGA HA AVUTO ESITO POSITIVO */
	users_list_updated = 1;
	
	/* INVIO IL MESSAGGIO DI RISPOSTA */
	setHeader(&header_reply, OP_OK, "");
	
	users_list_lock(users_list);
	setData(&data_reply, "", get_string(users_list), get_dim(users_list));
	if (send_reply(user_id, fd, &header_reply, &data_reply) == -1) {
		users_list_unlock(users_list);
		result = SYSTEM_ERROR;
		goto error_register;
	}	
 	users_list_unlock(users_list);
	
	/* INCREMENTO IL NUMERO DI UTENTI REGISTRATI */
	num_users_lock(users);
	inc_num_users_reg(users);
	num_users_unlock(users);

	printf("Registrazione di %s terminata\n", msg.hdr.sender);

	return result;

	error_register:
	{	
		if (max_conn_reached || invalid_param) {
			close(fd);
		}
		else {
			/** 
		 	 * SE É PRESENTE NELLA TABELLA DEGLI UTENTI LO ELIMINO;
		 	 * IN OGNI CASO VIENE EFFETTUATA LA FREE DI NICK E LA DESTROY DI USER_DATA (VIENE CHIUSO IL DESCRITTORE)
			**/
			if (inserted_to_users_table) {
				/* ACQUISISCO LA MUTEX SUL BLOCCO LOGICO DELLA TABELLA HASH */
				users_table_lock(users, msg.hdr.sender);
				/* ACQUISISCO LA MUTEX SUL DESCRITTORE POICHÈ NON È CONSENTITA LA CLOSE MENTRE SONO IN CORSO DELLE WRITE SUL DESCRITTORE STESSO */
				pthread_mutex_lock(&(fd_mtx[user_id%MaxConnections]));
				func_res = users_table_delete(users, msg.hdr.sender);
				pthread_mutex_unlock(&(fd_mtx[user_id%MaxConnections]));
				users_table_unlock(users, msg.hdr.sender);
			}
			else {
				if (nick) free(nick);
				if (user_data) user_data_destroy(user_data);
				else close(fd);
			}

			/**	
		 	 * SE É PRESENTE L'ASSOCIAZIONE FD -> NICK LA ELIMINO;
		  	 * IN OGNI CASO VIENE EFFETTUATA LA FREE DEL DESCRITTORE
			**/	
			if (fd_to_nick_added) {
				fd_to_nick_table_lock(users, fd);
				func_res = fd_to_nick_delete(users, fd);
				fd_to_nick_table_unlock(users, fd);
			}
			else {
				if (new_user_fd) free(new_user_fd);
            if (nick2) free(nick2);
			}

			/* ELIMINO IL NICK DALLA STRINGA SE E SOLO SE È STATO INSERITO */
			if (users_list_updated) {
				users_list_lock(users_list);
				func_res = users_list_remove(users_list, msg.hdr.sender);
				users_list_unlock(users_list);
			}
			/* CONTROLLO ERRORE */
			if (func_res == SYSTEM_ERROR) result = SYSTEM_ERROR;

			/* DECREMENTO IL NUMERO DI UTENTI CONNESSI */
			num_users_lock(users);
			dec_num_users_conn(users);
			num_users_unlock(users);
		}

		/* AGGIORNAMENTO STATISTICHE */
		update_stats(0,0,0,0,0,0,1);
		
		return result;
	}
}

op_res_t connect_op(unsigned int fd, message_t msg) {
	op_res_t result = REQUEST_OK;			//RISULTATO OPERAZIONE
	op_res_t func_res;						//RISULTATO CHIAMATE DI FUNZIONE
	user_data_t* user_data = NULL;		//DATI DELL'UTENTE
	message_hdr_t header_reply;			//HEADER MESSAGGIO DI RISPOSTA
	message_data_t data_reply;				//DATI MESSAGGIO DI RISPOSTA
	int* fd_key = NULL;						//DESCRITTORE UTENTE
	char* nick_data = NULL;					//NICK UTENTE
	int user_id = -1;							//ID DELL'UTENTE
	int max_conn_reached = 0;				//UGUALE A 1 SE E SOLO SE È STATO RAGGIUNTO IL MASSIMO NUMERO DI CONNESSIONI
	int connected = 0;						//UGUALE A 1 SE E SOLO SE L'UTENTE È STATO CONNESSO
	int fd_to_nick_added = 0;				//UGUALE A 1 SE E SOLO SE È STATA AGGIUNTA L'ASSOCIAZIONE DESCRITTORE -> NICK IN USERS_T	
	int users_list_updated = 0;			//UGUALE A 1 SE E SOLO SE È STATA AGGIORNATA LA STRINGA DEGLI UTENTI CONNESSI
	int invalid_param = 0;					//UGUALE A 1 SE E SOLO SE UNO DEI PARAMETRI NON È VALIDO

	/* INIZIO CONTROLLO PARAMETRI */
	if (fd < 0) return ILLEGAL_ARGUMENT;

	if (msg.hdr.sender[0] == '\0' || strlen(msg.hdr.sender) > MAX_NAME_LENGTH) {
		invalid_param = 1;
		setHeader(&header_reply, OP_FAIL, "");
		if (send_reply(user_id, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		goto error_connect;
	}
	/* FINE CONTROLLO PARAMETRI */

	/* VERIFICO DI NON AVER RAGGIUNTO IL NUMERO MASSIMO DI CONNESSIONI */
	num_users_lock(users);
	func_res = testAndInc_num_users_conn(users);
	num_users_unlock(users);

	if (func_res == CONN_LIMIT_REACHED) {
		max_conn_reached = 1;
		setHeader(&header_reply, OP_FAIL, "");
		if (send_reply(user_id, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		goto error_connect;
	}

	users_table_lock(users, msg.hdr.sender);
	/* CONTROLLO SE L'UTENTE È REGISTRATO */
	user_data = get_user_data(users, msg.hdr.sender);
	if (user_data == NULL) {
		users_table_unlock(users, msg.hdr.sender);
		setHeader(&header_reply, OP_NICK_UNKNOWN, "");
		if (send_reply(user_id, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		goto error_connect;
	}
	/* OTTENGO L'ID DELL'UTENTE */
	get_id(user_data, &user_id);
	/* VERIFICO CHE L'UTENTE NON SIA GIÀ CONNESSO */
	int current_fd;
	get_fd(user_data, &current_fd);
	if (current_fd != -1) {
		users_table_unlock(users, msg.hdr.sender);
		setHeader(&header_reply, OP_NICK_ALREADY, "");
		if (send_reply(user_id, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		goto error_connect;
	}
	/* SETTO IL DESCRITTORE */
	pthread_mutex_lock(fd_mtx + (user_id % MaxConnections));
	set_fd(user_data, fd);
	pthread_mutex_unlock(fd_mtx + (user_id % MaxConnections));
	users_table_unlock(users, msg.hdr.sender);

	connected = 1;

	/* ALLOCO IL DESCRITTORE */
	fd_key = (int*) malloc(sizeof(int));
	if (fd_key == NULL) {
		setHeader(&header_reply, OP_FAIL, "");
		send_reply(user_id, fd, &header_reply, NULL);
		result = SYSTEM_ERROR;
		goto error_connect;
	}
	*fd_key = fd;

	/* ALLOCO IL NICK */
	nick_data = (char*) malloc((strlen(msg.hdr.sender)+1)*sizeof(char));
	if (nick_data == NULL) {
		setHeader(&header_reply, OP_FAIL, "");
		send_reply(user_id, fd, &header_reply, NULL);
		result = SYSTEM_ERROR;
		goto error_connect;
	}
	memset(nick_data, '\0', strlen(msg.hdr.sender)+1);
	strncpy(nick_data, msg.hdr.sender, strlen(msg.hdr.sender));

	/* AGGIUNGO L'ASSOCIAZIONE DESCRITTORE -> NICK */
	fd_to_nick_table_lock(users, fd);
	func_res = fd_to_nick_insert(users, fd_key, nick_data);
	fd_to_nick_table_unlock(users, fd);

	/* CONTROLLO ERRORI */
	if (func_res != REQUEST_OK) {
		setHeader(&header_reply, OP_FAIL, "");
		send_reply(user_id, fd, &header_reply, NULL);
		result = SYSTEM_ERROR;
		goto error_connect;
	}

	fd_to_nick_added = 1;

	/* AGGIORNO LA STRINGA DEGLI UTENTI CONNESSI */
	users_list_lock(users_list);
	func_res = users_list_insert(users_list, msg.hdr.sender);
	users_list_unlock(users_list);

	/* CONTROLLO ERRORI */
	if (func_res == SYSTEM_ERROR) {
		setHeader(&header_reply, OP_FAIL, "");
		send_reply(user_id, fd, &header_reply, NULL);
		result = SYSTEM_ERROR;
		goto error_connect;
	}

	users_list_updated = 1;

	/* INVIO IL MESSAGGIO DI RISPOSTA */
	setHeader(&header_reply, OP_OK, "");

	users_list_lock(users_list);
	setData(&data_reply, "", get_string(users_list), get_dim(users_list));
	if (send_reply(user_id, fd, &header_reply, &data_reply) == -1) {
		users_list_unlock(users_list);
		result = SYSTEM_ERROR;
		goto error_connect;
	}
	users_list_unlock(users_list);

	printf("Connessione di %s avvenuta con successo\n", msg.hdr.sender);

	return result;

	error_connect: 
	{
		if (max_conn_reached || invalid_param) {
			close(fd);
		}
		else {
			if (connected) {
				users_table_lock(users, msg.hdr.sender);
				pthread_mutex_lock(fd_mtx + (user_id % MaxConnections));
				set_fd(user_data, -1);
				pthread_mutex_unlock(fd_mtx + (user_id % MaxConnections));
				users_table_unlock(users, msg.hdr.sender);
			}
			else {
				close(fd);
			}
			if (fd_to_nick_added) {
				fd_to_nick_table_lock(users, fd);
				fd_to_nick_delete(users, fd);
				fd_to_nick_table_unlock(users, fd);
			}
			else {
				if (fd_key) free(fd_key);
				if (nick_data) free(nick_data);
			}
			if (users_list_updated) {
				users_list_lock(users_list);
				users_list_remove(users_list, msg.hdr.sender);
				users_list_unlock(users_list);
			}

			num_users_lock(users);
			dec_num_users_conn(users);
			num_users_unlock(users);
		}

		/* AGGIORNAMENTO STATISTICHE */
		update_stats(0,0,0,0,0,0,1);

		return result;
	}
}

op_res_t posttxt_op(unsigned int fd, message_t msg) {
	op_res_t result = REQUEST_OK;						//RISULTATO DELL'OPERAZIONE
	message_hdr_t header_reply;						//HEADER MESSAGGIO RISPOSTA
	message_t message_to_send;							//MESSAGGIO DA INVIARE
	history_msg_t* history_msg;						//MESSAGGIO DA INSERIRE NELLA HISTORY
	boolean_t sended = FALSE;							//TRUE SE E SOLO SE IL MESSAGGIO È STATO INVIATO ALL'UTENTE
	user_data_t* user_data_sender = NULL;			//DATI E INFO DEL SENDER
	user_data_t* user_data_receiver = NULL;		//DATI E INFO DEL RECEIVER
	int user_id_sender = -1;							//ID DEL SENDER
	int user_id_receiver = -1;							//ID DEL RECEIVER

	/* INIZIO CONTROLLO PARAMETRI */
	if (fd < 0) return ILLEGAL_ARGUMENT;

	if (msg.hdr.sender[0] == '\0' || strlen(msg.hdr.sender) > MAX_NAME_LENGTH) {
		setHeader(&header_reply, OP_FAIL, "");
		if (send_reply(user_id_sender, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		update_stats(0,0,0,0,0,0,1);
		return result;
	}

	if (msg.data.hdr.receiver[0] == '\0' || strlen(msg.data.hdr.receiver) > MAX_NAME_LENGTH) {
		setHeader(&header_reply, OP_FAIL, "");
		if (send_reply(user_id_sender, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		update_stats(0,0,0,0,0,0,1);
		return result;
	}

   if (msg.data.hdr.len > MaxMsgSize) {
      setHeader(&header_reply, OP_MSG_TOOLONG, "");
		if (send_reply(user_id_sender, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		update_stats(0,0,0,0,0,0,1);
		return result;
   }
	/* FINE CONTROLLO PARAMETRI */

	users_table_lock(users, msg.hdr.sender);
	/* CONTROLLO SE IL MITTENTE È REGISTRATO */
	user_data_sender = get_user_data(users, msg.hdr.sender);
	if (user_data_sender == NULL) {
		users_table_unlock(users, msg.hdr.sender);
		setHeader(&header_reply, OP_NICK_UNKNOWN, "");
		if (send_reply(user_id_sender, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		update_stats(0,0,0,0,0,0,1);
		return result;
	}
	/* RECUPERO L'ID DEL MITTENTE */
	get_id(user_data_sender, &user_id_sender);
	/* CONTROLLO SE IL MITTENTE È CONNESSO */
	int current_fd;
	get_fd(user_data_sender, &current_fd);
	if (current_fd == -1) {
		users_table_unlock(users, msg.hdr.sender);
		setHeader(&header_reply, OP_FAIL, "");
		if (send_reply(user_id_sender, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		update_stats(0,0,0,0,0,0,1);
		return result;
	}
	users_table_unlock(users, msg.hdr.sender);

	users_table_lock(users, msg.data.hdr.receiver);
	/* CONTROLLO SE IL DESTINATARIO È REGISTRATO */
	user_data_receiver = get_user_data(users, msg.data.hdr.receiver);
	if (user_data_receiver == NULL) {
		users_table_unlock(users, msg.data.hdr.receiver);
		setHeader(&header_reply, OP_NICK_UNKNOWN, "");
		if (send_reply(user_id_sender, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		update_stats(0,0,0,0,0,0,1);
		return result;
	}
	/* RECUPERO L'ID DEL DESTINATARIO */
	get_id(user_data_receiver, &user_id_receiver);
	users_table_unlock(users, msg.data.hdr.receiver);

	setHeader(&message_to_send.hdr, TXT_MESSAGE, msg.hdr.sender);
	setData(&message_to_send.data, msg.data.hdr.receiver, msg.data.buf, strlen(msg.data.buf)+1);

	/* INVIO IL MESSAGGIO SE E SOLO SE IL DESTINATARIO È CONNESSO */
	pthread_mutex_lock(fd_mtx + (user_id_receiver % MaxConnections));
	int fd_receiver = -1;
	get_fd(user_data_receiver, &fd_receiver);
	/* SE IL DESTINATARIO SI È DEREGISTRATO O SI È DISCONNESSO ALLORA fd_receiver = -1 */
	if (fd_receiver != -1) {
		/* PASSO PARAMETRO USER_ID = -1 PERCHÈ LA MUTUA ESCLUSIONE SUL DESCRITTORE È STATA GIÀ ACQUISITA */
		if (send_reply(-1, fd_receiver, &message_to_send.hdr, &message_to_send.data) == -1) {
			pthread_mutex_unlock(fd_mtx + (user_id_receiver % MaxConnections));
			update_stats(0,0,0,0,0,0,1);
			return SYSTEM_ERROR;
		}
		sended = TRUE;
	}
	else {
		sended = FALSE;
	}
	pthread_mutex_unlock(fd_mtx + (user_id_receiver % MaxConnections));


	/* INIZIALIZZO IL MESSAGGIO DA INSERIRE NELLA HISTORY */
	history_msg = init_history_message(message_to_send, sended);
	if (history_msg == NULL) {
		setHeader(&header_reply, OP_FAIL, "");
		send_reply(user_id_sender, fd, &header_reply, NULL); 
		update_stats(0,0,0,0,0,0,1);
		return SYSTEM_ERROR;
	}

	/* INSERISCO IL MESSAGGIO NELLA HISTORY DEL DESTINATARIO */
	users_table_lock(users, msg.data.hdr.receiver);
	/* user_data_receiver POTREBBE ESSERE DIVENTATO NULL PERCHÈ POTREBBE ESSERSI DEREGISTRATO IL DESTINATARIO */
	if (user_data_receiver == NULL) {
		users_table_unlock(users, msg.data.hdr.receiver);
		free_history_message(history_msg);
		setHeader(&header_reply, OP_NICK_UNKNOWN, "");
		if (send_reply(user_id_sender, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		update_stats(0,0,0,0,0,0,1);
		return result;
	}
	insert_message(user_data_receiver, history_msg);
	users_table_unlock(users, msg.data.hdr.receiver);

	/* INVIO IL MESSAGGIO DI RISPOSTA AL MITTENTE */
	setHeader(&header_reply, OP_OK, "");
	if (send_reply(user_id_sender, fd, &header_reply, NULL) == -1) {
		update_stats(0,0,0,0,0,0,1);
		return SYSTEM_ERROR;
	}

	/* AGGIORNAMENTO STATISTICHE */
	if (sended == TRUE) update_stats(0,0,1,0,0,0,0);
	else if (sended == FALSE) update_stats(0,0,0,1,0,0,0);

	printf("Messaggio [%s] inviato da %s a %s con successo\n", msg.data.buf, msg.hdr.sender, msg.data.hdr.receiver);

	return result;
}

op_res_t posttxtall_op(unsigned int fd, message_t msg) {
	op_res_t result = REQUEST_OK;				//RISULTATO OPERAZIONE	
	op_res_t func_res;							//RISULTATO FUNZIONI CHIAMATE
	message_hdr_t header_reply;				//HEADER DELLA RISPOSTA
	message_t message_to_sent;					//MESSAGGIO DA INVIARE
	user_data_t* user_data_sender;			//DATI E INFO DEL SENDER
	history_msg_t* history_msg;				//MESSAGGIO DA INSERIRE NELLA HISTORY
	int user_id_sender = -1;					//ID DEL SENDER
	int user_id_receiver = -1;					//ID DEL RECEIVER
	int fd_receiver;								//DESCRITTORE DEL RECEIVER
	int num_messages_sended = 0;				//NUMERO DI MESSAGGIO INVIATI
	int num_messages_not_sended = 0;			//NUMERO DI MESSAGGI NON INVIATI
   users_table_iterator_t iterator;			//ITERATORE PER LA TABELLA HASH DEGLI UTENTI REGISTRATI
	iterator_element_t iterator_element;	//ELEMENTO RESTITUITO DALL'ITERATORE
	boolean_t sended;								//TRUE SE E SOLO SE IL MESSAGGIO È STATO INVIATO

   /* INIZIO CONTROLLO PARAMETRI */
   if (fd < 0) return ILLEGAL_ARGUMENT;

	if (!msg.hdr.sender || strlen(msg.hdr.sender) > MAX_NAME_LENGTH) {
		setHeader(&header_reply, OP_FAIL, "");
		if (send_reply(user_id_sender, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		update_stats(0,0,0,0,0,0,1);
		return result;
	}
	/* FINE CONTROLLO PARAMETRI */

   users_table_lock(users, msg.hdr.sender);
	/* CONTROLLO CHE L'UTENTE SIA REGISTRATO */
   user_data_sender = get_user_data(users, msg.hdr.sender);
   if (user_data_sender == NULL) {
		users_table_unlock(users, msg.hdr.sender);
		setHeader(&header_reply, OP_NICK_UNKNOWN, "");
		if (send_reply(user_id_sender, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		update_stats(0,0,0,0,0,0,1);
		return result;
   }
	/* RECUPERO L'ID DELL'UTENTE */
	get_id(user_data_sender, &user_id_sender);
	/* CONTROLLO CHE L'UTENTE SIA CONNESSO */
	int current_fd;
	get_fd(user_data_sender, &current_fd);
	if (current_fd == -1) {
		users_table_unlock(users, msg.hdr.sender);
		setHeader(&header_reply, OP_FAIL, "");
		if (send_reply(user_id_sender, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		update_stats(0,0,0,0,0,0,1);
		return result;
	}
	users_table_unlock(users, msg.hdr.sender);

	/* CREO UN ITERATORE PER LA TABELLA DEGLI UTENTI */
	func_res = users_table_iterator_init(users, &iterator);
	/* CONTROLLO ERRORI */
	if (func_res == SYSTEM_ERROR) {
		setHeader(&header_reply, OP_FAIL, "");
		send_reply(user_id_sender, fd, &header_reply, NULL);
		update_stats(0,0,0,0,0,0,1);
		return SYSTEM_ERROR;
	}

	setHeader(&message_to_sent.hdr, TXT_MESSAGE, msg.hdr.sender);
	setData(&message_to_sent.data, "", msg.data.buf, strlen(msg.data.buf)+1);

	users_table_lock_all(users);
	func_res = users_table_iterate(iterator, &iterator_element);
	while(func_res != NOT_FOUND) {
		if (strcmp(msg.hdr.sender, iterator_element.nick) != 0) {	//non invio il messaggio all'utente che ha effettuato la richiesta
			get_id(iterator_element.user_data, &user_id_receiver);
			users_table_unlock_all(users);
			fd_receiver = -1;
			pthread_mutex_lock(fd_mtx + (user_id_receiver % MaxConnections));
			get_fd(iterator_element.user_data, &fd_receiver);
			if (fd_receiver != -1) {
				if (send_reply(-1, fd_receiver, &message_to_sent.hdr, &message_to_sent.data) == -1) {
					pthread_mutex_unlock(fd_mtx + (user_id_receiver % MaxConnections));
					setHeader(&header_reply, OP_FAIL, "");
					send_reply(user_id_sender, fd, &header_reply, NULL);
					update_stats(0,0,0,0,0,0,1);
					return SYSTEM_ERROR;
				}
				sended = TRUE;
				num_messages_sended++;
			}
			else {
				sended = FALSE;
				num_messages_not_sended++;
			}
			pthread_mutex_unlock(fd_mtx + (user_id_receiver % MaxConnections));

			history_msg = init_history_message(message_to_sent, sended);
			if (history_msg == NULL) {
				setHeader(&header_reply, OP_FAIL, "");
				send_reply(user_id_sender, fd, &header_reply, NULL);
				update_stats(0,0,0,0,0,0,1);
				return SYSTEM_ERROR;
			}

			users_table_lock(users, iterator_element.nick);
			if (iterator_element.user_data != NULL)
				insert_message(iterator_element.user_data, history_msg);
			else
				free_history_message(history_msg);
			users_table_unlock(users, iterator_element.nick);
			users_table_lock_all(users);
		}

		func_res = users_table_iterate(iterator, &iterator_element);
	}
	users_table_unlock_all(users);

	users_table_iterator_close(&iterator);

	/* INVIO IL MESSAGGIO DI RISPOSTA AL MITTENTE */
	setHeader(&header_reply, OP_OK, "");
	if (send_reply(user_id_sender, fd, &header_reply, NULL) == -1) { 
		update_stats(0,0,0,0,0,0,1);
      return SYSTEM_ERROR;
   }

	/* AGGIORNAMENTO STATISTICHE */
	update_stats(0, 0, num_messages_sended, num_messages_not_sended, 0, 0, 0);
	
	printf("Messaggio [%s] inviato a tutti con successo\n", msg.data.buf);

	return result;
}

op_res_t postfile_op(unsigned int fd, message_t msg, message_data_t file_content) {
   op_res_t result = REQUEST_OK;			//RISULTATO DELL'OPERAZIONE
   message_hdr_t header_reply;			//HEADER DELLA RISPOSTA
   message_t message_to_send;				//MESSAGGIO DA INVIARE
   user_data_t* user_data_sender;		//INFO E DATI DEL SENDER
   user_data_t* user_data_receiver;		//INFO E DATI DEL RECEIVER
   history_msg_t* history_msg;			//MESSAGGIO DA INSERIRE NELLA HISTORY
   char file_path[2048];					//PATH DEL FILE		
   char file_name[1024];					//NOME DEL FILE
   FILE* file = NULL;						//PUNTATORE ALLA STRUTTURA DATI FILE DEL FILE file_name
   int user_id_sender = -1;				//ID DEL SENDER
   int user_id_receiver = -1;				//ID DEL RECEIVER
   boolean_t sended = FALSE;				//TRUE SE E SOLO SE IL MESSAGGIO È STATO INVIATO

   /* INIZIO CONTROLLO PARAMETRI */
	if (fd < 0) return ILLEGAL_ARGUMENT;

	if (!msg.hdr.sender || strlen(msg.hdr.sender) > MAX_NAME_LENGTH) {
		setHeader(&header_reply, OP_FAIL, "");
		if (send_reply(user_id_sender, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		update_stats(0,0,0,0,0,0,1);
		return result;
	}
   
   if (!msg.data.hdr.receiver || strlen(msg.data.hdr.receiver) > MAX_NAME_LENGTH) {
		setHeader(&header_reply, OP_FAIL, "");
		if (send_reply(user_id_sender, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		update_stats(0,0,0,0,0,0,1);
		return result;
	}

   if (!file_content.buf) {
      setHeader(&header_reply, OP_FAIL, "");
		if (send_reply(user_id_sender, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		update_stats(0,0,0,0,0,0,1);
		return result;
   }

   if (file_content.hdr.len > (MaxFileSize*1000)) {
      setHeader(&header_reply, OP_MSG_TOOLONG, "");
		if (send_reply(user_id_sender, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		update_stats(0,0,0,0,0,0,1);
		return result;
   }
	/* FINE CONTROLLO PARAMETRI */

   users_table_lock(users, msg.hdr.sender);
	/* CONTROLLO CHE IL MITTENTE SIA REGISTRATO */
   user_data_sender = get_user_data(users, msg.hdr.sender);
   if (user_data_sender == NULL) {
		users_table_unlock(users, msg.hdr.sender);
		setHeader(&header_reply, OP_NICK_UNKNOWN, "");
		if (send_reply(user_id_sender, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		update_stats(0,0,0,0,0,0,1);
		return result;
   }
	/* RECUPERO L'ID DEL MITTENTE */
	get_id(user_data_sender, &user_id_sender);
	/* CONTROLLO CHE IL MITTENTE SIA CONNESSO */
	int current_fd;
	get_fd(user_data_sender, &current_fd);
	if (current_fd == -1) {
		users_table_unlock(users, msg.hdr.sender);
		setHeader(&header_reply, OP_FAIL, "");
		if (send_reply(user_id_sender, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		update_stats(0,0,0,0,0,0,1);
		return result;
	}
	users_table_unlock(users, msg.hdr.sender);

   users_table_lock(users, msg.data.hdr.receiver);
   /* CONTROLLO CHE IL DESTINATARIO SIA REGISTRATO */
   user_data_receiver = get_user_data(users, msg.data.hdr.receiver);
   if (user_data_receiver == NULL) {
      users_table_unlock(users, msg.data.hdr.receiver);
      setHeader(&header_reply, OP_NICK_UNKNOWN, "");
      if (send_reply(user_id_sender, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
      else result = CLIENT_ERROR;
      update_stats(0,0,0,0,0,0,1);
		return result;
   }
   /* RECUPERO L'ID DEL DESTINATARIO */
   get_id(user_data_receiver, &user_id_receiver);
   users_table_unlock(users, msg.data.hdr.receiver);

   /* PREPARO IL PATH DEL FILE */
   memset(file_path, '\0', 1024);
   strncpy(file_path, DirName, strlen(DirName));
   strncat(file_path, "/", 1);
   strncat(file_path, msg.data.buf, strlen(msg.data.buf));

   char suffix[16];
   memset(suffix, '\0', 16);
   int i, open_error = 0;
   pthread_mutex_lock(&dir_mtx);
   struct stat st;
   for (i = 1; i <= 128; i++) {
      if (stat(file_path, &st) != -1) {
         sprintf(suffix, "(%d)", i);
         memset(file_path, '\0', 1024);
         strncpy(file_path, DirName, strlen(DirName));
         strncat(file_path, "/", 1);
         strncat(file_path, msg.data.buf, strlen(msg.data.buf));
         strncat(file_path, suffix, strlen(suffix));
      }
      else {
         errno = 0;
         file = fopen(file_path, "wb");
         if (file == NULL && errno != ENOENT) open_error = 1;
         break;
      }
   }
   pthread_mutex_unlock(&dir_mtx);

   memset(file_name, '\0', 1024);
   strncpy(file_name, msg.data.buf, msg.data.hdr.len);
   strncat(file_name, suffix, strlen(suffix));

   if (open_error) {
      setHeader(&header_reply, OP_FAIL, "");
      send_reply(user_id_sender, fd, &header_reply, NULL);
      update_stats(0,0,0,0,0,0,1);
		return SYSTEM_ERROR;
   }

   //se non c'è stato errore nell'apertura ma file è comunque null allora ci sono più di 128 file con lo stesso nome ==> fallisco
   if (file == NULL) {
      setHeader(&header_reply, OP_FAIL, "");
      if (send_reply(user_id_sender, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
      else result = CLIENT_ERROR;
      update_stats(0,0,0,0,0,0,1);
		return result;
   }

   if (fwrite(file_content.buf, sizeof(char), file_content.hdr.len, file) < file_content.hdr.len) {
      fclose(file);
      remove(file_path);
      setHeader(&header_reply, OP_FAIL, "");
      send_reply(user_id_sender, fd, &header_reply, NULL);
      update_stats(0,0,0,0,0,0,1);
		return SYSTEM_ERROR;
   }

   fclose(file);
   
   setHeader(&message_to_send.hdr, FILE_MESSAGE, msg.hdr.sender);
   setData(&message_to_send.data, msg.data.hdr.receiver, file_name, strlen(file_name)+1); 

   /* INVIO IL MESSAGGIO SE E SOLO SE IL DESTINATARIO È CONNESSO */
	pthread_mutex_lock(fd_mtx + (user_id_receiver % MaxConnections));
	int fd_receiver = -1;
	get_fd(user_data_receiver, &fd_receiver);
	/* SE IL DESTINATARIO SI È DEREGISTRATO O SI È DISCONNESSO ALLORA fd_receiver = -1 */
	if (fd_receiver != -1) {
		if (send_reply(-1, fd_receiver, &message_to_send.hdr, &message_to_send.data) == -1) {
         pthread_mutex_unlock(fd_mtx + (user_id_receiver % MaxConnections));
         remove(file_path);
         update_stats(0,0,0,0,0,0,1);
		   return SYSTEM_ERROR;
      }
		else sended = TRUE;
	}
	else {
		sended = FALSE;
	}
	pthread_mutex_unlock(fd_mtx + (user_id_receiver % MaxConnections));

   /* INIZIALIZZO IL MESSAGGIO DA INSERIRE NELLA HISTORY */
	history_msg = init_history_message(message_to_send, sended);
   if (history_msg == NULL) {
      remove(file_path);
      setHeader(&header_reply, OP_FAIL, "");
      send_reply(user_id_sender, fd, &header_reply, NULL);
      update_stats(0,0,0,0,0,0,1);
		return SYSTEM_ERROR;
   }

   /* ALLOCO IL NOME DEL FILE */
   char* str = (char*) malloc((strlen(file_name)+1)*sizeof(char));
   if (str == NULL) {
      remove(file_path);
      setHeader(&header_reply, OP_FAIL, "");
		send_reply(user_id_sender, fd, &header_reply, NULL);
		update_stats(0,0,0,0,0,0,1);
      return SYSTEM_ERROR;
   }
   memset(str, '\0', strlen(file_name)+1);
   strncpy(str, file_name, strlen(file_name));

	/* INSERISCO IL MESSAGGIO NELLA HISTORY DEL DESTINATARIO */
	users_table_lock(users, msg.data.hdr.receiver);
	/* user_data_receiver POTREBBE ESSERE DIVENTATO NULL SE IL DESTINATARIO SI È DEREGISTRATO */
	if (user_data_receiver == NULL) {
		users_table_unlock(users, msg.data.hdr.receiver);
      remove(file_path);
		free_history_message(history_msg);
      free(str);
		setHeader(&header_reply, OP_NICK_UNKNOWN, "");
		if (send_reply(user_id_sender, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		update_stats(0,0,0,0,0,0,1);
      return result;
	}
	insert_message(user_data_receiver, history_msg);
   if (insert_file_name(user_data_receiver, str) == SYSTEM_ERROR) {
      users_table_unlock(users, msg.data.hdr.receiver);
      remove(file_path);
      free(str);
		setHeader(&header_reply, OP_FAIL, "");
		send_reply(user_id_sender, fd, &header_reply, NULL);
      update_stats(0,0,0,0,0,0,1);
		return SYSTEM_ERROR;
   }
	users_table_unlock(users, msg.data.hdr.receiver);

   /* INVIO IL MESSAGGIO DI RISPOSTA AL MITTENTE */
	setHeader(&header_reply, OP_OK, "");
	if (send_reply(user_id_sender, fd, &header_reply, NULL) == -1) {
      update_stats(0,0,0,0,0,0,1);
      return SYSTEM_ERROR;
   }

	/* AGGIORNAMENTO STATISTICHE */
	if (sended == TRUE) update_stats(0,0,0,0,1,0,0);
	else if (sended == FALSE) update_stats(0,0,0,0,0,1,0);

	printf("File [%s] postato da %s per %s con successo\n", msg.data.buf, msg.hdr.sender, msg.data.hdr.receiver);

	return result;
}

op_res_t getfile_op(unsigned int fd, message_t msg) {
   op_res_t result = REQUEST_OK;		//RISULTATO OPERAZIONE
   message_hdr_t header_reply;		//HEADER DELLA RISPOSTA
   message_data_t data_reply;			//DATI DELLA RISPOSTA
   user_data_t* user_data;				//INFO E DATI DELL'UTENTE
   int user_id = -1;						//ID DELL'UTENTE
   char* buf = NULL;						//CONTENUTO FILE
   char file_path[1024];				//PATH DEL FILE

   /* INIZIO CONTROLLO PARAMETRI */
   if (fd < 0) return ILLEGAL_ARGUMENT;

   if (!msg.hdr.sender || strlen(msg.hdr.sender) > MAX_NAME_LENGTH) {
      setHeader(&header_reply, OP_FAIL, "");
      if (send_reply(user_id, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
      else result = CLIENT_ERROR;
      update_stats(0,0,0,0,0,0,1);
      return result;
   }
   /* FINE CONTROLLO PARAMETRI */

   users_table_lock(users, msg.hdr.sender);
	/* CONTROLLO SE L'UTENTE È REGISTRATO */
	user_data = get_user_data(users, msg.hdr.sender);
	if (user_data == NULL) {
		users_table_unlock(users, msg.hdr.sender);
		setHeader(&header_reply, OP_NICK_UNKNOWN, "");
		if (send_reply(user_id, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		update_stats(0,0,0,0,0,0,1);
      return result;
	}
	/* RECUPERO L'ID DELL'UTENTE */
	get_id(user_data, &user_id);
	/* CONTROLLO SE L'UTENTE È CONNESSO */
	int current_fd;
	get_fd(user_data, &current_fd);
	if (current_fd == -1) {
		users_table_unlock(users, msg.hdr.sender);
		setHeader(&header_reply, OP_FAIL, "");
		if (send_reply(user_id, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		update_stats(0,0,0,0,0,0,1);
      return result;
	}
   /* VERIFICO CHE IL FILE SIA DESTINATO ALL'UTENTE */
   if (search_file_name(user_data, msg.data.buf) == NOT_FOUND) {
      users_table_unlock(users, msg.hdr.sender);
      setHeader(&header_reply, OP_FAIL, "");
      if (send_reply(user_id, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
      else result = CLIENT_ERROR;
      update_stats(0,0,0,0,0,0,1);
      return result;
   }
	users_table_unlock(users, msg.hdr.sender);
	
   /* PREPARO IL PATH DEL FILE */
   memset(file_path, '\0', 1024);
   strncpy(file_path, DirName, strlen(DirName));
   strncat(file_path, "/", 1);
   strncat(file_path, msg.data.buf, msg.data.hdr.len);
   
	//MUTEX QUI PROBABILMENTE NON NECESSARIA
   pthread_mutex_lock(&dir_mtx);
   FILE* file = fopen(file_path, "rb");
   pthread_mutex_unlock(&dir_mtx);

   if (file == NULL) {
      setHeader(&header_reply, OP_FAIL, "");
      send_reply(user_id, fd, &header_reply, NULL);
      update_stats(0,0,0,0,0,0,1);
      return SYSTEM_ERROR;
   }
	
   if (fseek(file, SEEK_END, 0) == -1) {
      fclose(file);
      setHeader(&header_reply, OP_FAIL, "");
      send_reply(user_id, fd, &header_reply, NULL);
      update_stats(0,0,0,0,0,0,1);
      return SYSTEM_ERROR;
   }
	
   long file_size = ftell(file);
   
   rewind(file);
   
   buf = (char*) malloc((file_size+1)*sizeof(char));
   if (!buf) {
      fclose(file);
      setHeader(&header_reply, OP_FAIL, "");
      send_reply(user_id, fd, &header_reply, NULL);
      update_stats(0,0,0,0,0,0,1);
      return SYSTEM_ERROR;
   }
   memset(buf, '\0', file_size + 1);

   int n = fread(buf, sizeof(char), file_size, file);

   if (n < file_size && ferror(file) != 0) {
      fclose(file);
      free(buf);
      setHeader(&header_reply, OP_FAIL, "");
      send_reply(user_id, fd, &header_reply, NULL);
      update_stats(0,0,0,0,0,0,1);
      return SYSTEM_ERROR;
   }

   fclose(file);

   setHeader(&header_reply, OP_OK, "");
   setData(&data_reply, "", buf, strlen(buf)+1);
   if (send_reply(user_id, fd, &header_reply, &data_reply) == -1) {
      free(buf);
      update_stats(0,0,0,0,0,0,1);
      return SYSTEM_ERROR;   
   }

   free(buf);

   printf("File inviato a %s con successo\n", msg.hdr.sender);

   return result;
}

op_res_t getprevmsgs_op(unsigned int fd, message_t msg) {
	op_res_t result = REQUEST_OK;		//RISULTATO OPERAZIONE
	op_res_t func_res;					//RISULTATO CHIAMATE DI FUNZIONE
	message_hdr_t header_reply;		//HEADER DELLA RISPOSTA
	message_data_t data_reply;			//DATI DELLA RISPOSTA
	user_data_t* user_data;				//INFO E DATI DELL'UTENTE
	history_msg_t* history_msg;		//MESSAGGIO DELLA HISTORY
	int user_id = -1;						//ID DELL'UTENTE
	size_t* num_hist_msgs;				//NUMERO DI MESSAGGI NELLA HISTORY DELL'UTENTE
	int num_messages_sended = 0;		//NUMERO DI MESSAGGI INVIATI
	int num_files_sended = 0;			//NUMERO DI FILES INVIATI

	/* INIZIO CONTROLLO PARAMETRI */
	if (fd < 0) return ILLEGAL_ARGUMENT;

	if (!msg.hdr.sender || strlen(msg.hdr.sender) > MAX_NAME_LENGTH) {
		setHeader(&header_reply, OP_FAIL, "");
		if (send_reply(user_id, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		update_stats(0,0,0,0,0,0,1);
      return result;
	}
	/* FINE CONTROLLO PARAMETRI */

	users_table_lock(users, msg.hdr.sender);
	/* CONTROLLO SE L'UTENTE È REGISTRATO */
	user_data = get_user_data(users, msg.hdr.sender);
	if (user_data == NULL) {
		users_table_unlock(users, msg.hdr.sender);
		setHeader(&header_reply, OP_NICK_UNKNOWN, "");
		if (send_reply(user_id, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		update_stats(0,0,0,0,0,0,1);
      return result;
	}
	/* RECUPERO L'ID DELL'UTENTE */
	get_id(user_data, &user_id);
	/* CONTROLLO SE L'UTENTE È CONNESSO */
	int current_fd = -1;
	get_fd(user_data, &current_fd);
	if (current_fd == -1) {
		users_table_unlock(users, msg.hdr.sender);
		setHeader(&header_reply, OP_FAIL, "");
		if (send_reply(user_id, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		update_stats(0,0,0,0,0,0,1);
      return result;
	}
	int n;
	get_num_hist_msgs(user_data, &n);
	users_table_unlock(users, msg.hdr.sender);

	/* APRO UN ITERATORE PER LA HISTORY */
	history_iterator_t iterator;
	func_res = history_iterator_open(user_data, &iterator);
	if (func_res == SYSTEM_ERROR) {
		setHeader(&header_reply, OP_FAIL, "");
		send_reply(user_id, fd, &header_reply, NULL);
		update_stats(0,0,0,0,0,0,1);
      return SYSTEM_ERROR;
	}
	
	num_hist_msgs = (size_t*) malloc(sizeof(size_t));
   if (num_hist_msgs == NULL) {
      setHeader(&header_reply, OP_FAIL, "");
		send_reply(user_id, fd, &header_reply, NULL);
		update_stats(0,0,0,0,0,0,1);
      return SYSTEM_ERROR;
   }
	*num_hist_msgs = n;
	/* INVIO IL NUMERO DI MESSGGI NELLA HISTORY */
	setHeader(&header_reply, OP_OK, "");
	setData(&data_reply, "", (char*)num_hist_msgs, sizeof(size_t*));
	if (send_reply(user_id, fd, &header_reply, &data_reply) == -1) {
      free(num_hist_msgs);
		update_stats(0,0,0,0,0,0,1);
      return SYSTEM_ERROR;
   }

	/* INVIO I MESSAGGI */
   boolean_t sended;
	users_table_lock(users, msg.hdr.sender);
	for (size_t i = 0; i < (*num_hist_msgs); i++) {
		history_msg = history_iterate(iterator);
		if (history_msg == NULL) break;
		if (send_reply(user_id, fd, &(history_msg -> msg -> hdr), &(history_msg -> msg -> data)) == -1) {
			users_table_unlock(users, msg.hdr.sender);
			history_iterator_close(iterator);
         free(num_hist_msgs);
		   update_stats(0,0,0,0,0,0,1);
         return SYSTEM_ERROR;
		}
		get_sended(history_msg, &sended);
		if (sended == FALSE) {
			set_sended(history_msg, TRUE);
			if ((history_msg -> msg -> hdr).op == TXT_MESSAGE) num_messages_sended++;
			else if ((history_msg -> msg -> hdr).op == FILE_MESSAGE) num_files_sended++;
		}
	}
	users_table_unlock(users, msg.hdr.sender);

	history_iterator_close(iterator);

	/* AGGIORNAMENTO STATISTICHE */
	update_stats(0, 0, num_messages_sended, (-num_messages_sended), num_files_sended, (-num_files_sended), 0);

	free(num_hist_msgs);	
	
	printf("Messaggi della history di %s inviati con successo\n", msg.hdr.sender);

	return result;
}

op_res_t usrlist_op(unsigned int fd, message_t msg) {
	op_res_t result = REQUEST_OK;		//RISULTATO DELL'OPERAZIONE
	message_hdr_t header_reply;		//HEADER DELLA RISPOSTA
	message_data_t data_reply;			//DATI DELLA RISPOSTA	
	user_data_t* user_data;				//INFO E DATI DELL'UTENTE
	int user_id = -1;						//ID DELL'UTENTE

	/* INIZIO CONTROLLO PARAMETRI */
	if (fd < 0) return ILLEGAL_ARGUMENT;

	if (!msg.hdr.sender || strlen(msg.hdr.sender) > MAX_NAME_LENGTH) {
		setHeader(&header_reply, OP_FAIL, "");
		if (send_reply(user_id, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		update_stats(0,0,0,0,0,0,1);
      return result;
	}
	/* FINE CONTROLLO PARAMETRI */

	users_table_lock(users, msg.hdr.sender);
	/* CONTROLLO SE L'UTENTE È REGISTRATO */
	user_data = get_user_data(users, msg.hdr.sender);
	if (user_data == NULL) {
		users_table_unlock(users, msg.hdr.sender);
		setHeader(&header_reply, OP_NICK_UNKNOWN, "");
		if (send_reply(user_id, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		update_stats(0,0,0,0,0,0,1);
      return result;
	}
	/* RECUPERO L'ID DELL'UTENTE */
	get_id(user_data, &user_id);
	/* CONTROLLO SE L'UTENTE È CONNESSO */
	int current_fd;
	get_fd(user_data, &current_fd);
	if (current_fd == -1) {
		users_table_unlock(users, msg.hdr.sender);
		setHeader(&header_reply, OP_FAIL, "");
		if (send_reply(user_id, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		update_stats(0,0,0,0,0,0,1);
      return result;
	}
	users_table_unlock(users, msg.hdr.sender);

	/* INVIO MESSAGGIO DI RISPOSTA */
	setHeader(&header_reply, OP_OK, "");

	users_list_lock(users_list);
	setData(&data_reply, "", get_string(users_list), get_dim(users_list));
	if (send_reply(user_id, fd, &header_reply, &data_reply) == -1) {
		users_list_unlock(users_list);
		update_stats(0,0,0,0,0,0,1);
      return SYSTEM_ERROR;
	}
	users_list_unlock(users_list);

   printf("Lista degli utenti connessi inviata a %s con successo\n", msg.hdr.sender);

	return result;
}

op_res_t unregister_op(unsigned int fd, message_t msg) {
	op_res_t result = REQUEST_OK;		//RISULTATO DELL'OPERAZIONE
	op_res_t func_res;					//RISULTATO DELLE CHIAMATE DI FUNZIONE
	message_hdr_t header_reply;		//HEADER DELLA RISPOSTA
	user_data_t* user_data;				//INFO E DATI DELL'UTENTE
	int user_id = -1;						//ID DELL'UTENTE

	/* INIZIO CONTROLLO PARAMETRI */
	if (fd < 0) return ILLEGAL_ARGUMENT;

	if (!msg.hdr.sender || strlen(msg.hdr.sender) > MAX_NAME_LENGTH) {
		setHeader(&header_reply, OP_FAIL, "");
		if (send_reply(user_id, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		update_stats(0,0,0,0,0,0,1);
      return result;
	}

	if (!msg.data.hdr.receiver || strlen(msg.data.hdr.receiver) > MAX_NAME_LENGTH) {
		setHeader(&header_reply, OP_FAIL, "");
		if (send_reply(user_id, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		update_stats(0,0,0,0,0,0,1);
      return result;
	}

	if (strcmp(msg.hdr.sender, msg.data.hdr.receiver) != 0) {
		setHeader(&header_reply, OP_FAIL, "");
		if (send_reply(user_id, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		update_stats(0,0,0,0,0,0,1);
      return result;
	}
	/* FINE CONTROLLO PARAMETRI */

	users_table_lock(users, msg.hdr.sender);
	/* CONTROLLO SE L'UTENTE È REGISTRATO */
	user_data = get_user_data(users, msg.hdr.sender);
	if (user_data == NULL) {
		users_table_unlock(users, msg.hdr.sender);
		setHeader(&header_reply, OP_NICK_UNKNOWN, "");
		if (send_reply(user_id, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		update_stats(0,0,0,0,0,0,1);
      return result;
	}
	/* RECUPERO L'ID DELL'UTENTE */
	get_id(user_data, &user_id);
	/* CONTROLLO SE L'UTENTE È CONNESSO */
	int current_fd;
	get_fd(user_data, &current_fd);
	if (current_fd == -1) {
		users_table_unlock(users, msg.hdr.sender);
		setHeader(&header_reply, OP_FAIL, "");
		if (send_reply(user_id, fd, &header_reply, NULL) == -1) result = SYSTEM_ERROR;
		else result = CLIENT_ERROR;
		update_stats(0,0,0,0,0,0,1);
      return result;
	}
	users_table_unlock(users, msg.hdr.sender);

	/* INVIO MESSAGGIO DI RISPOSTA */
	setHeader(&header_reply, OP_OK, "");
	if (send_reply(user_id, fd, &header_reply, NULL) == -1) {
		update_stats(0,0,0,0,0,0,1);
      return SYSTEM_ERROR;
	}

	/* ELIMINO I FILE DESTINATI ALL'UTENTE ED ELIMINO L'UTENTE DALLA TABELLA DEGLI UTENTI */
	users_table_lock(users, msg.hdr.sender);
   remove_all_file(user_data, DirName);
	pthread_mutex_lock(fd_mtx + (user_id % MaxConnections));
	users_table_delete(users, msg.hdr.sender);
	pthread_mutex_unlock(fd_mtx + (user_id % MaxConnections));
	users_table_unlock(users, msg.hdr.sender);

	/* ELIMINO L'ASSOCIAZIONE DESCRITTORE NICK */
	fd_to_nick_table_lock(users, fd);
	fd_to_nick_delete(users, fd);
	fd_to_nick_table_unlock(users, fd);

	/* ELIMINO IL NICK DALLA STRINGA DEGLI UTENTI CONNESSI */
	users_list_lock(users_list);
	func_res = users_list_remove(users_list, msg.hdr.sender);
	users_list_unlock(users_list);

	/* CONTROLLO ERRORI */
	if (func_res == SYSTEM_ERROR) {
      update_stats(0,0,0,0,0,0,1);
      result = SYSTEM_ERROR;
   }

	/* AGGIORNO IL NUMERO DEGLI UTENTI CONNESSI E REGISTRATI */
	num_users_lock(users);
	dec_num_users_reg(users);
	dec_num_users_conn(users);
	num_users_unlock(users);

   printf("%s deregistrato con successo\n", msg.hdr.sender);

	return result;
}

op_res_t disconnect_op(unsigned int fd) {
	char nick[MAX_NAME_LENGTH+1];		//NICKNAME DELL'UTENTE
	user_data_t* user_data = NULL;	//INFO E DATI DELL'UTENTE
	int id;								   //ID DELL'UTENTE

	if (fd < 0) return ILLEGAL_ARGUMENT;
	
	memset(nick, '\0', MAX_NAME_LENGTH + 1);

	/* RECUPERO IL NICKNAME DELL'UTENTE */
	fd_to_nick_table_lock(users, fd);
	char* p = get_nick(users, fd);
	if (p != NULL) {
		strncpy(nick, p, strlen(p));
		fd_to_nick_delete(users, fd);
	}
	else
		close(fd);
	fd_to_nick_table_unlock(users, fd);

	/* RECUPERO I DATI DELL'UTENTE E CHIUDO E SETTO IL DESCRITTORE A -1 */
	if (nick[0] != '\0') {
		users_table_lock(users, nick);
		user_data = get_user_data(users, nick);
		if (user_data) {
			get_id(user_data, &id);
			pthread_mutex_lock(&(fd_mtx[id%MaxConnections]));
			set_fd(user_data, -1);
			pthread_mutex_unlock(&(fd_mtx[id%MaxConnections]));
		}
		else close(fd);
		users_table_unlock(users, nick);
		
		/* RIMUOVO IL NICKNAME DALLA STRINGA DEGLI UTENTI CONNESSI */
		users_list_lock(users_list);
		users_list_remove(users_list, nick);
		users_list_unlock(users_list);

		/* DECREMENTO IL NUMERO DI UTENTI CONNESSI */
      num_users_lock(users);
	   dec_num_users_conn(users);
	   num_users_unlock(users);
	}

   printf("Disconnessione di %s avvenuta con successo\n", nick);

	return REQUEST_OK;
}
