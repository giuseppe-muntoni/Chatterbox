
/** \file users_list.h  
       \author Giuseppe Muntoni
       Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
       originale dell'autore  
     */  

#if !defined(USERS_LIST_H_)
#define USERS_LIST_H_

#include <pthread.h>
#include "op_res.h"

/** Struttura di supporto alla gestione della stringa degli utenti connessi
 */
typedef struct users_list {
   char* str;                 /**<  Stringa degli utenti connessi       */   
   int dim;                   /**<  Numero di caratteri della stringa   */
   pthread_mutex_t mtx;       /**<  Mutex per la struttura dati         */
} users_list_t;

/**   Alloca la struttra dati users_list ed inizializza il campo str a NULL e dim a 0
 *    
 *    \return:  puntatore alla struttura dati allocata
 */
users_list_t* users_list_init();

/**   Dealloca la struttura dati users_list
 * 
 *    \param users_list:   puntatore alla struttura dati da deallocare   
 */
void users_list_destroy(users_list_t* users_list);

/**   Acquisisce la mutex sulla struttura dati users_list
 * 
 *    \param users_list:   puntatore alla struttura dati users_list da bloccare
 *    \return:             se user_list è uguale a NULL allora ILLEGAL_ARGUMENT
 *                         altrimenti REQUEST_OK 
 */
op_res_t users_list_lock(users_list_t* users_list);

/**   Rilascia la mutex sulla struttura dati users_list
 * 
 *    \param users_list:   puntatore alla struttura dati users_list da sbloccare
 *    \return:             se users_list è uguale a NULL allora ILLEGAL_ARGUMENT
 *                         altrimenti REQUEST_OK
 */
op_res_t users_list_unlock(users_list_t* users_list);

/**   Inserisce un nuovo nick nella stringa degli utenti connessi
 * 
 *    \param users_list:   puntatore alla struttura dati users_list
 *    \param nick:         nickname da inserire
 *    \return:             se users_list == NULL || nick == NULL allora ILLEGAL_ARGUMENT
 *                         se il numero di caratteri > MAX_NAME_LENGTH allora ILLEGAL_ARGUMENT (macro definita in config.h)
 *                         se c'è un errore di gestione della memoria dinamica allora SYSTEM_ERROR
 *                         altrimenti REQUEST_OK
 */
op_res_t users_list_insert(users_list_t* users_list, char* nick);

/**   Rimuove il nick dalla struttura dati users_list
 *    
 *    \param users_list:   puntatore alla struttura dati users_list
 *    \param nick:         nickname da rimuovere
 *    \return:             se users_list == NULL || nick == NULL allora ILLEGAL_ARGUMENT
 *                         se il numero di caratteri > MAX_NAME_LENGTH allora ILLEGAL_ARGUMENT (macro definita in config.h)
 *                         se c'è un errore di gestione della memoria dinamica allora SYSTEM_ERROR
 *                         altrimenti REQUEST_OK          
 */
op_res_t users_list_remove(users_list_t* users_list, char* nick);

/**   Ritorna il campo str di users_list
 * 
 *    \param users_list:   puntatore alla struttura dati users_list
 *    \return:             se users_list è uguale a NULL allora NULL
 *                         altrimenti la stringa degli utenti connessi
 */
char* get_string(users_list_t* users_list);

/**   Ritorna il campo dim di users_list
 *    
 *    \param users_list:   puntatore alla struttura dati users_list
 *    \return:             se users_list è uguale a NULL allora -1
 *                         altrimenti il numero di caratteri della stringa
 */    
int get_dim(users_list_t* users_list);

#endif /* USERS_LIST_H_ */


