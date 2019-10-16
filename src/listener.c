
/** \file listener.c  
       \author Giuseppe Muntoni
       Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
       originale dell'autore  
     */  

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/signalfd.h>
#include <errno.h>
#include "queue.h"
#include "conn.h"
#include "users.h"
#include "stats.h"
#include "parser.h"

/**	Valore speciale utilizzato per far terminare i threads del pool
 */
#ifndef POOL_TERM
#define POOL_TERM (void*)-1
#endif

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 64
#endif

extern char UnixPath[UNIX_PATH_MAX];
extern Queue_t *codaFd;										//Coda dei descrittori condivisa con i threads del pool
extern int fdpipe[2];										//Pipe per la comunicazione dei descrittori da thread del pool a thread listener
extern int countActiveThreads;							//Numero di threads del pool ancora attivi	
extern pthread_mutex_t mtx_countActiveThreads;		//Mutex su countActiveThreads
extern struct statistics chattyStats;					//Statistiche chatty
extern pthread_mutex_t chattyStatsMtx;					//Mutex sulle statistiche
extern users_t* users;										//Struttura dati utenti (definita in users.h)
int fds;															//Descrittore della socket

/**	Funzione thread-safe che ritorna il numero di thread del pool attivi
 */
static int get_countActiveThreads() {
	pthread_mutex_lock(&mtx_countActiveThreads);

	int n = countActiveThreads;

	pthread_mutex_unlock(&mtx_countActiveThreads);

	return n;
}

/**	Inserisce nella coda il messaggio di terminazione per i thread del pool ancora attivi
 * 	Chiude il descrittore della socket
 */
static void safeTermination() {
	//TUTTI I THREAD DEL POOL ANCORA ATTIVI DEVONO TERMINARE
	int n = get_countActiveThreads();
	for(int i = 0; i < n ; i++) {
		pushQueue(codaFd, POOL_TERM);
	}
	//CHIUDO IL DESCRITTORE DELLA SOCKET SE GIÀ APERTO
	if (fds != -1)
		close(fds);
}

void cleanup() {
   unlink(UnixPath);
}

/**	Aggiorna l'indice max tra i descrittori nel set
 */
int updatemax(fd_set set, int fdmax) {
   for(int i = fdmax; i >= 0; --i)
	   if (FD_ISSET(i, &set)) return i;
   return -1;
}

void* listener(void* fdsig) {
   cleanup();
	int res; 							//Per il risultato delle chiamate di sistema
	int fdc, fdp, fd_num = -1;
	fd_set set;				
	fd_set tmpset;

	//Creo la socket
	fds = -1;
	fds = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fds == -1) {
		safeTermination();
		return (void*)1;
	}
	struct sockaddr_un sa;
	strncpy(sa.sun_path, UnixPath, UNIX_PATH_MAX);
	sa.sun_family = AF_UNIX;
	if (bind(fds, (const struct sockaddr*)&sa, sizeof(sa)) == -1) {
		safeTermination();
		return (void*)1;
	}

	//Mi metto in ascolto
	if (listen(fds, SOMAXCONN) == -1) {
		safeTermination();
		return (void*)1;
	}

	FD_ZERO(&set);
	//Setto il descrittore della socket
	FD_SET(fds, &set);
	if (fds > fd_num) 
		fd_num = fds;
	//Setto il descrittore della pipe
	FD_SET(fdpipe[0], &set);
	if (fdpipe[0] > fd_num)
		fd_num = fdpipe[0];
	//Setto il descrittore dei segnali
	FD_SET(*(int*)fdsig, &set);
	if (*(int*)fdsig > fd_num)
	   fd_num = *(int*)fdsig;

	while(1) {
		tmpset = set;
		
		//Faccio la select
		if (select(fd_num+1, &tmpset, NULL, NULL, NULL) == -1) {
			safeTermination();
			return (void*)1;
		}

		for(int fd = 0; fd <= fd_num; fd++) {
			if(FD_ISSET(fd, &tmpset)) {
				//Richiesta di connessione da parte di un nuovo client
				if(fd == fds) {
					//Se non ci sono più thread del pool attivi termino
					if (get_countActiveThreads() == 0) {
						safeTermination();
						return (void*)0;
					}
					//Effettuo la accept
					fdc = accept(fds, NULL, 0);
					if (fdc == -1) {
						safeTermination();
						return (void*)1;
					}
					//Inserisco il descrittore del nuovo client nel set della select
					FD_SET(fdc, &set);
					if (fdc > fd_num) 
						fd_num = fdc;
				}
				//Arriva un descrittore da un thread del pool ==> l'operazione richiesta dal client ha avuto successo e lo riascolto nella select
				else if(fd == fdpipe[0]) {
               fdp = -1;
					res = readn(fdpipe[0], &fdp, sizeof(int));
					if (res == -1) {
						safeTermination();
						return (void*)1;
					}
					//Riascolto le richieste in arrivo dal client
					FD_SET(fdp, &set);
					if (fdp > fd_num) fd_num = fdp;
				}
				//Il descrittore dei segnali è pronto in lettura ==> controllo la tipologia di segnale e lo gestisco opportunamente
				else if(fd == *(int*)fdsig) {
               struct signalfd_siginfo infosig;
	            memset(&infosig, '\0', sizeof(infosig));
	            int k = read(*(int*)fdsig, &infosig, sizeof(struct signalfd_siginfo));
	            if (k == -1) {
                  safeTermination();
                  return (void*)1;
               }
					//Se è arrivato SIGUSR1 stampo le statistiche sul file StatFileName altrimenti termino
               if (infosig.ssi_signo == SIGUSR1) {
						printf("Arrivato segnale SIGUSR1\n");
						int nreg = 0, nonline = 0;
						//Recupero il numero di utenti registrati e connessi 
                  num_users_lock(users);
						get_num_users_reg(users, &(nreg));
                  get_num_users_conn(users, &(nonline));
						num_users_unlock(users);
						//Aggiorno le statistiche
                  pthread_mutex_lock(&chattyStatsMtx);
                  chattyStats.nusers = nreg;
						chattyStats.nonline = nonline;
                  pthread_mutex_unlock(&chattyStatsMtx);
						//Apro il file per stampare le statistiche
		            FILE *f = fopen(StatFileName, "ab");
                  pthread_mutex_lock(&chattyStatsMtx);
		            if (f == NULL || printStats(f) == -1) {
                     pthread_mutex_unlock(&chattyStatsMtx);
                     safeTermination();
                     return (void*)1;
                  }
                  pthread_mutex_unlock(&chattyStatsMtx);
		            if (f) fclose(f);
	            }
               else {
					   safeTermination();
					   return (void*)0;
               }
				}
				//È arrivata una richiesta da un client ==> inserisco il descrittore del client nella coda in modo che un thread del pool possa gestirla
				else {
					//Se non ci sono più thread del pool attivi allora termino
					if (get_countActiveThreads() == 0) {
						close(fd);
						safeTermination();
						return (void*)0;
					}
					//Non ascolto il client finchè il thread del pool non mi notifica la terminazione della gestione
					FD_CLR(fd, &set);
					fd_num = updatemax(set, fd_num);
					//Alloco il descrittore e lo inserisco nella coda
					int *data = (int*) malloc(sizeof(int));
					if (data == NULL) {
						safeTermination();
						return (void*)1;
					}
					*data = fd;
					pushQueue(codaFd, data);
				}
			}
		}
	}
}
