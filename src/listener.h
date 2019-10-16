
/** \file listener.h 
       \author Giuseppe Muntoni 544943 
       Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
       originale dell'autore  
     */  

#if !defined(LISTENER_H)
#define LISTENER_H

/** Funzione che effettua la select per gestire le connessioni in arrivo e le nuove richieste dei client
 * 
 *  \param fdsig: descrittore dei segnali
 *  \return:      se ha successo ritorna 0
 *                se c'è stato un errore ritorna 1
 */
void* listener (void* fdsig);

#endif /* LISTENER_H */
