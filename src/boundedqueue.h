
/** \file chatty.c 
 *     \author  Giuseppe Muntoni 544943
 *     \brief   Tale struttura dati è stata trovata in una delle soluzioni degli assegnamenti dati durante il corso 
 *              ed è stata da me modificata in maniera sostanziale per adattarla agli usi in tale progetto
 */

#if !defined(BOUNDED_QUEUE_H)
#define BOUNDED_QUEUE_H

#include <stdlib.h>

/** Struttura dati coda. 
 *  (Non thread-safe)
 */
typedef struct BQueue {
    void   **buf;
    size_t   head;
    size_t   tail;
    size_t   qsize;
    size_t   qlen;
} BQueue_t;

/** Struttura dati iteratore coda
*/
typedef struct BQueue_iterator {
    BQueue_t* queue;
    size_t next;
} BQueue_iterator_t;


/** Alloca ed inizializza una coda di dimensione \param n. 
 *
 *   \retval NULL se si sono verificati problemi nell'allocazione (errno settato)
 *   \retval q puntatore alla coda allocata
 */
BQueue_t *initBQueue(size_t n);

/** Cancella una coda allocata con initQueue.
 *  
 *  \param q puntatore alla coda da cancellare
 *  \param F puntatore alla funzione che effettua la deallocazione dei singoli elementi della coda    
 */
void destroyBQueue(BQueue_t *q, void (*F)(void*));

/** Inserisce un dato nella coda.
 *  
 *  \param q puntatore alla coda 
 *  \param data puntatore al dato da inserire
 *  
 *   \retval 0 se successo
 *   \retval -1 se la coda è piena o se q == NULL o data == NULL (errno settato)
 */
int pushBQueue(BQueue_t *q, void *data);

/** Estrae un elemento dalla coda
 * 
 *  \retval dato se successo
 *  \retval NULL se la coda è vuota
 */  
void* popBQueue(BQueue_t *q);

/** Restituisce il numero di elementi nella coda
 *  
 *  \param  puntatore alla coda
 *  \retval numero di elementi nella coda
 */
size_t getBQueueLen(BQueue_t *q);

/** Inizializza un iteratore per la coda q
 * 
 *  \param  puntatore alla coda
 *  \return puntatore all'iteratore  
 */
BQueue_iterator_t* BQueue_iterator_init(BQueue_t* q);

/** Restituisce il prossimo elemento nella coda
 *  
 *  \param  puntatore all'iteratore
 *  \return elemento iterato
 */
void* BQueue_iterator_next(BQueue_iterator_t* iterator);

/** Dealloca l'iteratore sulla coda
 * 
 *  \param  puntatore all'iteratore da deallocare
 */
void BQueue_iterator_destroy(BQueue_iterator_t* iterator);

#endif /* BOUNDED_QUEUE_H */