
/** \file history_msg.h  
      \author Giuseppe Muntoni
      Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
      originale dell'autore  
*/  

#if !defined(HISTORY_MSG_H_)
#define HISTORY_MSG_H_

#include "message.h"
#include "op_res.h"

/**  Tipo di dato booleano
 */
typedef enum boolean {
	FALSE = 0,
	TRUE = 1
} boolean_t;

/**   Struttura dati che rappresenta un messaggio della history
 */
typedef struct history_msg {
   message_t* msg;               /**<  Puntatore al messaggio                                   */
   boolean_t sended;             /**<  Booleano TRUE se e solo se il messaggio è stato inviato  */ 
} history_msg_t;


/**   Inizializza un messaggio della history
 * 
 *    \param msg:       messaggio da inserire
 *    \param sended:    booleano TRUE se il messaggio è stato inviato, FALSE altrimenti
 *    \return:          se c'è stato un errore nella deep-copy (allocazione memoria dinamica) del messaggio allora NULL 
 *                      se l'op del messaggio è diversa da TXT_MESSAGE e FILE_MESSAGE allora NULL (vedi message.h)
 *                      altrimenti ritorna un puntatore alla struttura dati history_msg_t         
 */
history_msg_t* init_history_message(message_t msg, boolean_t sended);

/**   Dealloca un messaggio della history
 * 
 *    \param history_msg:  puntatore alla struttura dati history_msg_t da deallocare
 */
void free_history_message(void* history_msg);

/**   Setta il booleano sended in history_msg
 *    
 *    \param history_msg:  puntatore alla struttra dati history_msg_t
 *    \param sended:       booleano che indica se il messaggio è stato inviato oppure no
 *    \return:             se history_msg è NULL allora ILLEGAL_ARGUMENT
 *                         altrimenti REQUEST_OK
 */
op_res_t set_sended(history_msg_t* history_msg, boolean_t sended);

/**   Setta il parametro sended con il valore del campo sended in history_msg
 *    
 *    \param history_msg:  puntatore alla struttura dati history_msg_t
 *    \param sended:       puntatore alla variabile booleana in cui inserire il valore del campo sended di history_msg
 */
op_res_t get_sended(history_msg_t* history_msg, boolean_t* sended);

#endif /* HISTORY_MSG_H_ */
