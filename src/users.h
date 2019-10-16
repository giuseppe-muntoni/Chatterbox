
/** \file users.h
      \author Giuseppe Muntoni 544943
      Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
      originale dell'autore  
     */  

#if !defined(USERS_H_)
#define USERS_H_

#include <pthread.h>
#include "icl_hash.h"
#include "op_res.h"
#include "user_data.h"

/**   Struttura dati per la gestione degli utenti   
 *    Le funzioni di interfaccia che agiscono sui campi della struttura non sono thread-safe,
 *    per far si che lo siano è necessario invocare le funzioni di lock e unlock sui vari campi che si intende leggere/modificare
 */
typedef struct users {
   icl_hash_t* reg_users;              /**<  Tabella hash che associa al nickname dell'utente la struttura dati definita in user_data.h   */
   pthread_mutex_t* mtx_reg_users;     /**<  Array di mutex per reg_users: 
                                        *    se non è richiesta la thread-safety di reg_users allora è uguale a null
                                        *    se si vuole un'unica mutex per tutta la tabella allora il vettore ha dimensione 1 
                                        *    se si vuole dividere la tabella in blocchi logici ad ognuno dei quali è associata una mutex distinta 
                                        *                                                                      allora il vettore ha dimensione > 1
                                        */
   icl_hash_t* fd_to_nick;             /**<  Tabella hash che associa al descrittore di un client il suo nickname                         */
   pthread_mutex_t* mtx_fd_to_nick;    /**<  Array di mutex per fd_to_nick: la sua dimensione è sempre uguale a quella di mtx_reg_users   */ 
   int num_logical_block;              /**<  Il numero di mutex associate a reg_users e fd_to_nick                                        */
	int max_conn;								/**<	Il numero massimo di connessioni contemporanee																*/
   int num_users_conn;                 /**<  Numero di utenti connessi                                                                    */
   int num_users_reg;                  /**<  Numero di utenti registrati                                                                  */
   pthread_mutex_t mtx_num_users;      /**<  Mutex associata a num_users_conn e num_users_reg                                             */ 
} users_t;

/**   Struttura dati iteratore su tabella hash
 */
typedef struct users_table_iterator {
   icl_hash_t* reg_users;              /**<  Tabella hash che si vuole iterare (vedere icl_hash.h per la definizione della tabella hash)  */
   icl_iterator_t* iterator;           /**<  Iteratore sulla tabella definito in icl_hash.h                                               */
} users_table_iterator_t;

/**   Elemento restituito dall'iterazione su reg_users (campo di users_t)
 */ 
typedef struct iterator_element {
   char* nick;                         /**<  Nickname dell'utente                                                                         */
   user_data_t* user_data;             /**<  Informazioni associate all'utente (user_data_t definita in user_data.h)                      */
} iterator_element_t;      

/* FUNZIONI DI INTERFACCIA */

/**   Inizializza la struttura dati users_t, deve essere chiamata da un solo thread (tipicamente il thread main)
 * 
 *    \param dim:                la dimensione delle tabelle hash
 *    \param num_logical_block:  numero di mutex da utilizzare per le tabelle hash
 * 	\param max_conn:				numero massimo di connessioni contemporanee
 *    \return:							puntatore alla struttura dati degli utenti allocata se successo, altrimenti NULL se c'è stato un fallimento
 */         
users_t* users_init(int dim, int num_logical_block, int max_conn);

/**   Dealloca la struttura dati users_t, deve essere chiamata da un solo thread (tipicamente il thread main)
 *    
 *    \param users:	puntatore alla struttura dati da deallocare
 */
void users_destroy(users_t* users);

/**   Acquisisce mutex su blocco logico di reg_users   
 *    
 *    \param users:  puntatore alla struttura dati users_t
 *    \param nick:   nickname dell'utente
 *    \return:			se users == NULL || nick == NULL allora ILLEGAL_ARGUMENT
 * 						altrimenti REQUEST_OK (enum definita in op_res.h) 
 */
op_res_t users_table_lock(users_t* users, char* nick);

/**   Rilascia mutex su blocco logico di reg_users
 *    
 *    \param users:  puntatore alla struttura dati users_t
 *    \param nick:   nickname dell'utente
 *    \return: 		se users == NULL || nick == NULL allora ILLEGAL_ARGUMENT
 * 						altrimenti REQUEST_OK (enum definita in op_res.h)
 */
op_res_t users_table_unlock(users_t* users, char* nick);

/**   Acquisisce mutex su tutta la tabella reg_users
 * 
 *    \param users:  puntatore alla struttura dati users_t
 *    \return: 		se users == NULL allora ILLEGAL_ARGUMENT
 * 						altrimenti REQUEST_OK (enum definita in op_res.h)
 */
op_res_t users_table_lock_all(users_t* users);

/**   Rilascia mutex su tutta la tabella reg_users
 * 
 *    \param users:  puntatore alla struttura dati users_t
 *    \return:			se users == NULL allora ILLEGAL_ARGUMENT
 * 						altrimenti REQUEST_OK (enum definita in op_res.h)
 */
op_res_t users_table_unlock_all(users_t* users);

/**   Acquisisce mutex su blocco logico di fd_to_nick
 * 
 *    \param users:  puntatore alla struttura dati users_t
 *    \param fd:  	descrittore dell'utente
 *    \return:			se users == NULL || fd < 0 allora ILLEGAL_ARGUMENT
 * 						altrimenti REQUEST_OK (enum definita in op_res.h)
 */
op_res_t fd_to_nick_table_lock(users_t* users, int fd);

/**   Rilascia mutex su blocco logico di fd_to_nick
 * 
 *    \param users:  puntatore alla struttura dati users_t
 *    \param fd:  	descrittore dell'utente
 *    \return: 		se users == NULL || fd < 0 allora ILLEGAL_ARGUMENT
 * 						altrimenti REQUEST_OK (enum definita in op_res.h)
 */
op_res_t fd_to_nick_table_unlock(users_t* users, int fd);

/**   Acquisisce mutex su num_users_conn e num_users_reg
 * 
 *    \param users:  puntatore alla struttura dati users_t
 *    \return: 		se users == NULL allora ILLEGAL_ARGUMENT
 * 						altrimenti REQUEST_OK (enum definita in op_res.h)
 */
op_res_t num_users_lock(users_t* users);

/**   Rilascia mutex su num_users_conn e num_users_reg
 * 
 *    \param users:  puntatore alla struttura dati users_t
 *    \return: 		se users == NULL allora ILLEGAL_ARGUMENT
 * 						altrimenti REQUEST_OK (enum definita in op_res.h)
 */
op_res_t num_users_unlock(users_t* users);

/**   Inserisce associazione nick -> user_data in reg_users
 * 
 *    \param users:  	puntatore alla struttura dati users_t
 *    \param nick:   	nickname utente
 *    \param user_data: puntatore alla struttura dati user_data_t
 *    \return: 			se users == NULL || nick == NULL || user_data == NULL allora ILLEGAL_ARGUMENT
 * 							se la chiave nick è già presente in reg_users allora ALREADY_INSERTED
 * 							se c'è un errore di allocazione della memoria allora SYSTEM_ERROR
 * 							altrimenti REQUEST_OK
 */
op_res_t users_table_insert(users_t* users, char* nick, user_data_t* user_data);

/**   Cancella la struttura dati user_data_t associata a nick in reg_users  
 * 
 *    \param users:  puntatore alla struttura dati users_t
 *    \param nick:   nickname utente
 *    \return: 		se users == NULL || nick == NULL allora ILLEGAL_ARGUMENT
 * 						se non c'è nessun match con la chiave nick in reg_users allora NOT_FOUND
 * 						altrimenti REQUEST_OK
 */
op_res_t users_table_delete(users_t* users, char* nick);

/**   Restituisce puntatore a struttura dati user_data_t associata a nick
 *    
 *    \param users:  puntatore alla struttura dati users_t
 *    \param nick:   nickname utente
 *    \return: 		se users == NULL || nick == NULL allora NULL
 * 						se non c'è un match con la chiave nick allora NULL
 * 						altrimenti puntatore a struttura user_data_t associata a nick
 */
user_data_t* get_user_data(users_t* users, char* nick);

/**   Inizializza un iteratore per reg_users
 *    
 *    \param users:		puntatore alla struttura dati users_t
 *    \param iteratore: indirizzo della struttura dati iteratore da inizializzare
 *    \return: 			se users == NULL || iterator == NULL allora ILLEGAL_ARGUMENT
 * 							se c'è un errore di gestione della memoria dinamica allora SYSTEM_ERROR
 * 							altrimenti REQUEST_OK  
 */
op_res_t users_table_iterator_init(users_t* users, users_table_iterator_t* iterator);

/**   Itera su reg_users
 * 
 *    \param users_table_iterator:  struttura dati iteratore
 *    \param element:					indirizzo della variabile in cui inserire l'elemento restituito dall'iteratore
 *    \return: 							se element == NULL allora ILLEGAL_ARGUMENT
 * 											se non ci sono più elementi su cui iterare allora NOT_FOUND
 * 											altrimenti REQUEST_OK
 */
op_res_t users_table_iterate(users_table_iterator_t users_table_iterator, iterator_element_t* element);

/**   Chiude l'iteratore su reg_users
 *    
 *    \param users_table_iterator:  indirizzo dell'iteratore da chiudere
 *    \return: 							se users_table_iterator == NULL allora ILLEGAL_ARGUMENT
 * 											altrimenti REQUEST_OK
 */
op_res_t users_table_iterator_close(users_table_iterator_t* users_table_iterator);

/**   Inserisce l'associazione fd -> nick in fd_to_nick
 * 
 *    \param users:  puntatore alla struttura dati users_t
 *    \param fd:     puntatore al descrittore      
 *    \param nick:   nickname dell'utente
 *    \return:       se users == NULL || fd == NULL || nick == NULL allora ILLEGAL_ARGUMENT
 * 						se è già presente un elemento con chiave fd allora ALREADY_INSERTED
 * 						se c'è un errore di gestione della memoria dinamica allora SYSTEM_ERROR
 * 						altrimenti REQUEST_OK 
 */
op_res_t fd_to_nick_insert(users_t* users, int* fd, char* nick);

/**   Elimina l'associazione fd -> nick in fd_to_nick
 * 
 *    \param users:  puntatore alla struttura dati users_t
 *    \param fd:     descrittore dell'utente  
 *    \return:       se users == NULL || fd < 0 allora ILLEGAL_ARGUMENT
 * 						se non c'è un match con la chiave fd in fd_to_nick allora NOT_FOUND
 * 						altrimenti REQUEST_OK
 */
op_res_t fd_to_nick_delete(users_t* users, int fd);

/**   Restituisce il nick associato al descrittore fd
 *    
 *    \param users:  puntatore alla struttura dati users_t
 *    \param fd:     descrittore dell'utente
 *    \return:       se users == NULL || fd < 0 allora NULL
 * 						se non c'è un nick associato a fd in fd_to_nick allora NULL
 * 						altrimenti puntatore a nick associato ad fd in fd_to_nick
 */
char* get_nick(users_t* users, int fd);

/**   Incrementa num_users_conn se e solo se non è stato raggiunto il massimo numero di utenti connessi contemporaneamente
 * 
 *    \param users:  puntatore alla struttura dati users_t
 *    \return:       se users == NULL allora ILLEGAL_ARGUMENT
 * 						se è stato raggiunto il massimo numero di connessioni allora CONN_LIMIT_REACHED
 * 						altrimenti REQUEST_OK
 */
op_res_t testAndInc_num_users_conn(users_t* users);

/**   Decrementa il numero di utenti connessi num_users_conn
 * 
 *    \param users:  puntatore alla struttura dati users_t   
 *    \return:       se users == NULL allora ILLEGAL_ARGUMENT
 * 						altrimenti REQUEST_OK
 */
op_res_t dec_num_users_conn(users_t* users);

/**   Restituisce il numero di utenti connessi num_users_conn
 * 
 *    \param users:  puntatore alla struttura dati users_t
 *    \param num:    indirizzo della variabile in cui salvare il numero di utenti connessi num_users_conn
 *    \return:       se users == NULL || num == NULL allora ILLEGAL_ARGUMENT
 * 						altrimenti REQUEST_OK
 */
op_res_t get_num_users_conn(users_t* users, int* num);

/**   Incrementa il numero di utenti registrati num_users_reg
 * 
 *    \param users:  puntatore alla struttura dati users_t
 *    \return:       se users == null allora ILLEGAL_ARGUMENT
 * 						altrimenti REQUEST_OK
 */
op_res_t inc_num_users_reg(users_t* users);

/**   Decrementa il numero di utenti registrati num_users_reg
 * 
 *    \param users:  puntatore alla struttura dati users_t
 *    \return:       se users == NULL allora ILLEGAL_ARGUMENT
 * 						altrimenti REQUEST_OK
 */
op_res_t dec_num_users_reg(users_t* users);

/**   Restituisce il numero di utenti registrati num_users_reg
 * 
 *    \param users:  puntatore alla struttura dati users_t
 *    \param num:    indirizzo della variabile in cui salvare il numero di utenti registrati num_users_reg
 *    \return:       se users == NULL || num == NULL allora ILLEGAL_ARGUMENT
 * 						altrimenti REQUEST_OK
 */
op_res_t get_num_users_reg(users_t*users, int* num);

#endif /* USERS_H_ */
