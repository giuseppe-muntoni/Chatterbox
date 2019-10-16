#ifndef QUEUE_H_
#define QUEUE_H_

/** Elemento della coda.
 *
 */
typedef struct Node {
    void        * data;
    struct Node * next;
} Node_t;

/** Struttura dati coda.
 *
 */
typedef struct Queue {
    Node_t        *head;
    Node_t        *tail;
    unsigned long  qlen;
} Queue_t;


/** Alloca ed inizializza una coda. Deve essere chiamata da un solo 
 *  thread (tipicamente il thread main).
 *
 *   \retval NULL se si sono verificati problemi nell'allocazione (errno settato)
 *   \retval q puntatore alla coda allocata
 */
Queue_t *initQueue();

/** Cancella una coda allocata con initQueue. Deve essere chiamata da
 *  da un solo thread (tipicamente il thread main).
 *  
 *   \param q puntatore alla coda da cancellare
 *   \param F funzione che effettua la cancellazione del dato
 */
void deleteQueue(Queue_t *q, void (*F)(void*));

/** Inserisce un dato nella coda.
 *   \param data puntatore al dato da inserire
 *  
 *   \retval 0 se successo
 *   \retval -1 se errore (errno settato opportunamente)
 */
int    pushQueue(Queue_t *q, void *data);

/** Estrae un dato dalla coda.
 *
 *  \retval data puntatore al dato estratto.
 */
void  *popQueue(Queue_t *q);


unsigned long length(Queue_t *q);

#endif /* QUEUE_H_ */
