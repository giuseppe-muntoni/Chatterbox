
/** \file listener.c  
       \author Giuseppe Muntoni 544943
       Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
       originale dell'autore  
 */  

#if !defined(OP_RES_H_)
#define OP_RES_H_

/** Enumerazione che codifica i risultati delle operazioni eseguite dal server
 */
typedef enum {
    REQUEST_OK = 0,
    ILLEGAL_ARGUMENT = 1,
    SYSTEM_ERROR = 2,
    CLIENT_ERROR = 3,
    ALREADY_INSERTED = 4,
    FOUND = 5,
    NOT_FOUND = 6,
    CONN_LIMIT_REACHED = 7
} op_res_t;

#endif
