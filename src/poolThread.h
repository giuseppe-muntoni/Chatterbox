
/** \file poolThread.h  
       \author Giuseppe Muntoni
       Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
       originale dell'autore  
     */  

#if !defined(POOLTHREAD_H_)
#define POOLTHREAD_H_

/** Task eseguito dai threads del pool
 * 
 *  \return:  se successo allora 0
 *            se c'è stato un fallimento allora -1
 */
void* pool_func();

#endif /* POOLTHREAD_H_ */
