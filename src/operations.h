
/** \file operations.h  
       \author Giuseppe Muntoni 544943
       Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
       originale dell'autore  
     */  

#if !defined(OPERATIONS_H_)
#define OPERATIONS_H_

#include "message.h"
#include "op_res.h"

/** Effettua la registrazione di un utente alla chat
 * 
 *  \param fd:  descrittore del client
 *  \param msg: richiesta del client
 *  \return:    se l'operazione ha avuto successo allora REQUEST_OK
 *              se l'operazione ha fallito causa richiesta malformata dal client allora CLIENT_ERROR
 *              se l'operazione ha fallito durante la gestione della memoria dinamica o in qualche chiamata di sistema allora SYSTEM_ERROR
 */
op_res_t register_op(unsigned int fd, message_t msg);

/** Connette un utente alla chat
 *  
 *  \param fd:  descrittore del client
 *  \param msg: richiesta del client
 *  \return:    se l'operazione ha avuto successo allora REQUEST_OK
 *              se l'operazione ha fallito causa richiesta malformata dal client allora CLIENT_ERROR
 *              se l'operazione ha fallito durante la gestione della memoria dinamica o in qualche chiamata di sistema allora SYSTEM_ERROR 
 */
op_res_t connect_op(unsigned int fd, message_t msg);

/** Invia un messaggio ad un utente registrato alla chat
 * 
 *  \param fd:  descrittore del client
 *  \param msg: richiesta del client
 *  \return:    se l'operazione ha avuto successo allora REQUEST_OK
 *              se l'operazione ha fallito causa richiesta malformata dal client allora CLIENT_ERROR
 *              se l'operazione ha fallito durante la gestione della memoria dinamica o in qualche chiamata di sistema allora SYSTEM_ERROR 
 */ 
op_res_t posttxt_op(unsigned int fd, message_t msg);

/** Invia un messaggio a tutti gli utenti registrati alla chat
 * 
 *  \param fd:  descrittore del client
 *  \param msg: richiesta del client
 *  \return:    se l'operazione ha avuto successo allora REQUEST_OK
 *              se l'operazione ha fallito causa richiesta malformata dal client allora CLIENT_ERROR
 *              se l'operazione ha fallito durante la gestione della memoria dinamica o in qualche chiamata di sistema allora SYSTEM_ERROR
 */ 
op_res_t posttxtall_op(unsigned int fd, message_t msg);

/** Salva un file nella directory DirName e lo notifica, se connesso, al client ricevente
 * 
 *  \param fd:              descrittore del client
 *  \param msg:             richiesta del client
 *  \param file_content:    contenuto del file da postare  
 *  \return:                se l'operazione ha avuto successo allora REQUEST_OK
 *                          se l'operazione ha fallito causa richiesta malformata dal client allora CLIENT_ERROR
 *                          se l'operazione ha fallito durante la gestione della memoria dinamica o in qualche chiamata di sistema allora SYSTEM_ERROR
 */
op_res_t postfile_op(unsigned int fd, message_t msg, message_data_t file_content);

/** Invia il contenuto di un file
 * 
 *  \param fd:  descrittore del client
 *  \param msg: richiesta del client
 *  \return:    se l'operazione ha avuto successo allora REQUEST_OK
 *              se l'operazione ha fallito causa richiesta malformata dal client allora CLIENT_ERROR
 *              se l'operazione ha fallito durante la gestione della memoria dinamica o in qualche chiamata di sistema allora SYSTEM_ERROR
 */
op_res_t getfile_op(unsigned int fd, message_t msg);

/** Invia tutti i messaggi salvati nella history dell'utente
 * 
 *  \param fd:  descrittore del client
 *  \param msg: richiesta del client
 *  \return:    se l'operazione ha avuto successo allora REQUEST_OK
 *              se l'operazione ha fallito causa richiesta malformata dal client allora CLIENT_ERROR
 *              se l'operazione ha fallito durante la gestione della memoria dinamica o in qualche chiamata di sistema allora SYSTEM_ERROR
 */
op_res_t getprevmsgs_op(unsigned int fd, message_t msg);

/** Invia la lista degli utenti connessi
 * 
 *  \param fd:  descrittore del client
 *  \param msg: richiesta del client
 *  \return:    se l'operazione ha avuto successo allora REQUEST_OK
 *              se l'operazione ha fallito causa richiesta malformata dal client allora CLIENT_ERROR
 *              se l'operazione ha fallito durante la gestione della memoria dinamica o in qualche chiamata di sistema allora SYSTEM_ERROR 
 */
op_res_t usrlist_op(unsigned int fd, message_t msg);

/** Deregistra un utente dalla chat
 * 
 *  \param fd:  descrittore del client
 *  \param msg: richiesta del client
 *  \return:    se l'operazione ha avuto successo allora REQUEST_OK
 *              se l'operazione ha fallito causa richiesta malformata dal client allora CLIENT_ERROR
 *              se l'operazione ha fallito durante la gestione della memoria dinamica o in qualche chiamata di sistema allora SYSTEM_ERROR
 */
op_res_t unregister_op(unsigned int fd, message_t msg);

/** Disconnette un utente dalla chat
 * 
 *  \param fd:  descrittore del client
 *  \return:    se l'operazione ha avuto successo allora REQUEST_OK
 *              se l'operazione ha fallito causa richiesta malformata dal client allora CLIENT_ERROR
 *              se l'operazione ha fallito durante la gestione della memoria dinamica o in qualche chiamata di sistema allora SYSTEM_ERROR
 */
op_res_t disconnect_op(unsigned int fd);

#endif /* OPERATIONS_H_ */
