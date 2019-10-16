
/** \file user_data.h
       \author Giuseppe Muntoni 544943 
       Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
       originale dell'autore  
     */  

#if !defined(USER_DATA_H_)
#define USER_DATA_H_

#include "boundedqueue.h"
#include "icl_hash.h"
#include "history_msg.h"
#include "op_res.h"

/**   Struttura dati contenente informazioni associate ad un utente
 */
typedef struct user_data {
   BQueue_t* history;				/**<	Coda contenente gli ultimi messaggi inviati all'utente		*/
   icl_hash_t* name_files_rcvd;	/**<	Tabella hash contenente i nomi dei file inviati all'utente	*/	
   int num_hist_msgs;				/**<	Dimensione della history												*/
   int fd;								/**<	Descrittore dell'utente se è connesso, -1 altrimenti			*/
   int id;								/**<	Id immutabile associato all'utente (non univoco)				*/
} user_data_t;

typedef BQueue_iterator_t* history_iterator_t;	//Iteratore history

/* FUNZIONI DI INTERFACCIA */

/**	Inizializza la struttura dati user_data_t
 * 	
 * 	\param nick:						nick dell'utente
 * 	\param fd:							descrittore dell'utente
 * 	\param history_dim:				dimensione della history dei messaggi
 * 	\param name_files_table_dim:	dimensione della tabella dei nomi dei file ricevuti
 * 	\return:								se ha successo puntatore a user_data_t altrimenti NULL
 */
user_data_t* user_data_init(char* nick, int fd, int history_dim, int name_files_table_dim);

/**	Dealloca la struttura dati user_data_t e chiude la connessione col client se è connesso
 * 	
 * 	\param user_data:	puntatore alla struttura dati da deallocare
 */
void user_data_destroy(void* user_data);

/**	Setta la variabile fd in user_data_t con il valore del parametro fd
 * 	Se il parametro fd == -1 allora viene anche chiusa la connessione con il client
 * 
 * 	\param user_data:	puntatore alla struttura dati user_data_t
 * 	\param fd: 			nuovo descrittore per l'utente
 * 	\return:				se user_data == NULL allora ILLEGAL_ARGUMENT
 * 							altrimenti REQUEST_OK 
 */
op_res_t set_fd(user_data_t* user_data, int fd);

/**	Restituisce il descrittore associato all'utente
 * 	
 * 	\param user_data:	puntatore alla struttura dati user_data_t
 * 	\param fd:			indirizzo della variabile in cui salvare il descrittore
 * 	\return:				se user_data == NULL || fd == NULL allora ILLEGAL_ARGUMENT
 * 							altrimenti REQUEST_OK
 */
op_res_t get_fd(user_data_t* user_data, int* fd);

/**	Resituisce l'id dell'utente
 * 
 *		\param user_data:	puntatore alla struttura dati user_data_t
 * 	\param id:			indirizzo della variabile in cui salvare l'id
 * 	\return:				se user_data == NULL || id == NULL allora ILLEGAL_ARGUMENT
 * 							altrimenti REQUEST_OK
 */
op_res_t get_id(user_data_t* user_data, int* id);

/**	Resituisce il numero di messaggi nella history dell'utente
 * 
 * 	\param user_data:			puntatore alla struttura dati user_data_t
 * 	\param num_hist_msgs:	indirizzo della variabile in cui salvare il descrittore
 * 	\return:						se user_data == NULL || num_hist_msgs == NULL allora ILLEGAL_ARGUMENT
 * 									altrimenti REQUEST_OK
 */
op_res_t get_num_hist_msgs(user_data_t* user_data, int* num_hist_msgs);

/**	Inserisce un messaggio nella history dell'utente
 * 	Se la history è piena per far spazio al nuovo messaggio verrà eliminato il messaggio inserito meno di recente
 * 
 * 	\param user_data:	puntatore alla struttura dati user_data_t
 * 	\param msg: 		puntatore al messaggio da inserire (history_msg_t definito in history_msg.h)
 * 	\return:				se user_data == NULL || msg == NULL allora ILLEGAL_ARGUMENT
 * 							altrimenti REQUEST_OK
 */
op_res_t insert_message(user_data_t* user_data, history_msg_t* msg);

/**	Apre un iteratore per la history dell'utente
 * 	
 * 	\param user_data:	puntatore alla struttura dati user_data_t
 * 	\param iterator:	indirizzo dell'iteratore da inizializzare
 * 	\return:				se user_data == NULL || iterator == NULL allora ILLEGAL_ARGUMENT
 * 							se c'è un errore nella gestione della memoria dinamica allora SYSTEM_ERROR
 * 							altrimenti REQUEST_OK
 */ 							
op_res_t history_iterator_open(user_data_t* user_data, history_iterator_t* iterator);

/**	Chiude l'iteratore per la history dell'utente
 * 
 * 	\param iterator:	iteratore da chiudere
 */					
void history_iterator_close(history_iterator_t iterator);

/**	Itera sulla history dell'utente
 * 	
 * 	\param iterator:	iteratore
 * 	\return:				puntatore al messaggio history_msg_t (history_msg_t definito in history_msg.h)
 */
history_msg_t* history_iterate(history_iterator_t iterator);

/**	Inserisce il nome di un file nella tabella name_files_rcvd
 * 
 * 	\param user_data:	puntatore alla struttura dati user_data_t
 * 	\param file_name: nome del file
 * 	\return:				se user_data == NULL || file_name == NULL allora ILLEGAL_ARGUMENT
 * 							se è già presente un match con la chiave file_name in name_files_rcvd allora ALREADY_INSERTED
 * 							se c'è un errore nella gestione della memoria dinamica allora SYSTEM_ERROR
 * 							altrimenti REQUEST_OK
 */
op_res_t insert_file_name(user_data_t* user_data, char* file_name);

/**	Controlla se il file file_name è stato inviato all'utente
 * 	
 * 	\param user_data:	puntatore alla struttura dati user_data_t
 * 	\param file_name:	nome del file
 * 	\return:				se user_data == NULL || file_name == NULL allora ILLEGAL_ARGUMENT
 * 							se non è presente un match con la chiave file_name in name_file_rcvd allora NOT_FOUND
 * 							altrimenti FOUND
 */
op_res_t search_file_name(user_data_t* user_data, char* file_name);

/**	Rimuove tutti i file dell'utente dalla directory dir_name
 * 
 * 	\param user_data:	puntatore alla struttura dati user_data_t
 * 	\param dir_name:	nome directory in cui sono salvati i file
 * 	\return:				se user_data == NULL || dir_name == NULL allora ILLEGAL_ARGUMENT
 * 							se c'è un errore di gestione della memoria dinamica allora SYSTEM_ERROR
 * 							altrimenti REQUEST_OK
 */
op_res_t remove_all_file(user_data_t* user_data, char* dir_name);

#endif /* USER_DATA_H_ */
