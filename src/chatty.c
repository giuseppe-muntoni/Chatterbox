/*
 * membox Progetto del corso di LSO 2017/2018
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */
/**
 * @file chatty.c
 * @brief File principale del server chatterbox
 */
/** \file chatty.c 
       \author Giuseppe Muntoni
       Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
       originale dell'autore  
     */  

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <sys/signalfd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "parser.h"
#include "stats.h"
#include "queue.h"
#include "listener.h"
#include "poolThread.h"
#include "users.h"
#include "users_list.h"
#include "message.h"

#define DIM_HASH 1024
#define NUM_MTX_HASH DIM_HASH/32

/**	Se result è uguale a value allora stampa str sullo stderr e se doCleanup > 0 libera la memoria, infine termina
 */
#define CHECK_EQ(result, value, str, doCleanup)	\
	if (result == value)	{							\
		fprintf(stderr, "%s\n", str);				\
		if (doCleanup) cleanup();					\
		exit(EXIT_FAILURE);							\
	}

/**	Se result è diverso da value allora stampa str sullo stderr e se doCleanup > 0 libera la memoria, infine termina
 */
#define CHECK_NEQ(result, value, str, doCleanup)	\
	if (result != value) {							\
		fprintf(stderr, "%s\n", str);				\
		if (doCleanup) cleanup();					\
		exit(EXIT_FAILURE);							\
	}

/* struttura che memorizza le statistiche del server, struct statistics 
 * e' definita in stats.h.
 */
struct statistics chattyStats = { 0,0,0,0,0,0,0 };

/* MUTEX PER CHATTYSTATS */
pthread_mutex_t chattyStatsMtx = PTHREAD_MUTEX_INITIALIZER;

/* CODA DESCRITTORI DEI CLIENT CONDIVISA TRA THREAD LISTENER E THREADS DEL POOL */
Queue_t *codaFd;	

/* STRUTTURA CHE MEMORIZZA GLI UTENTI, DEFINITA IN users.h */
users_t* users;

/* STRUTTURA CHE MEMORIZZA LA STRINGA DEGLI UTENTI CONNESSI, DEFINITA IN users_list.h */
users_list_t* users_list;

/* ARRAY DI MUTEX UTILIZZATO PER LE WRITE SUI DESCRITTORI */
pthread_mutex_t* fd_mtx;

/* PIPE UTILIZZATA PER COMUNICAZIONE TRA THREADS DEL POOL E THREAD LISTENER */
int fdpipe[2];

/* DESCRITTORE PER LA GESTIONE DEI SEGNALI */
int fd_sig;

/* CONTATORE DEL NUMERO DI THREADS DEL POOL ATTIVI */
int countActiveThreads;

/* MUTEX ASSOCIATA A countActiveThreads */
pthread_mutex_t mtx_countActiveThreads = PTHREAD_MUTEX_INITIALIZER;

/* MUTEX PER LA GESTIONE DELLA DIRECTORY */
pthread_mutex_t dir_mtx = PTHREAD_MUTEX_INITIALIZER;

/**	Chiude il descrittore puntato da fd e libera la memoria 
 */
static void closeFd(void* fd) {
	if(fd) {
		if (*(int*)fd != -1) close(*(int*)fd);
		free(fd);
	}
}

/**	Dealloca tutte le strutture dati allocate nello heap, chiude descrittori e pipe, distrugge le mutex
 */
static void cleanup() {
	if (users) users_destroy(users);
	if (users_list) users_list_destroy(users_list);
	if (codaFd) deleteQueue(codaFd, closeFd);
	if (fd_sig != -1) close(fd_sig);
	if (fdpipe[0] != -1) close(fdpipe[0]);
	if (fdpipe[1] != -1) close(fdpipe[1]);
	if (fd_mtx) {
		for (int i = 0; i < MaxConnections; i++) {
			pthread_mutex_destroy(fd_mtx + i);
		}
		free(fd_mtx);
	}
}

/**	Configurazione gestione segnali
 */
static void signal_handle(sigset_t *s) {
	sigset_t set;
	struct sigaction sa;
	memset(&sa, '\0', sizeof(sa));
	sa.sa_handler = SIG_IGN;
	//	Ignoro SIGPIPE
	CHECK_EQ(sigaction(SIGPIPE, &sa, NULL), -1, "errore sigaction", 0)
	//	Se StatFileName non è settato ignoro anche SIGUSR1
	if (StatFileName[0] == '\0') {
		CHECK_EQ(sigaction(SIGUSR1, &sa, NULL), -1, "errore sigaction", 0)
	}
	//	Maschero SIGINT, SIGTERM e SIGQUIT e se StatFileName è settato anche SIGUSR1
	//	Per ascoltarli in seguito con la signalfd
	CHECK_EQ(sigemptyset(&set), -1, "errore sigemptyset", 0)
	CHECK_EQ(sigaddset(&set, SIGINT), -1, "errore sigaddset", 0)
	CHECK_EQ(sigaddset(&set, SIGTERM), -1, "errore sigaddset", 0)
	CHECK_EQ(sigaddset(&set, SIGQUIT), -1, "errore sigaddset", 0)
	if (StatFileName[0] != '\0') {
		CHECK_EQ(sigaddset(&set, SIGUSR1), -1, "errore sigaddset", 0)
	}
	CHECK_EQ(pthread_sigmask(SIG_SETMASK, &set, NULL), -1, "errore mascheramento segnali", 0)
	*s = set;
}

static void usage(const char *progname) {
   fprintf(stderr, "Il server va lanciato con il seguente comando:\n");
   fprintf(stderr, "  %s -f conffile\n", progname);
}

int main(int argc, char *argv[]) {
	sigset_t set;
	int opt;
	if((opt = getopt(argc, argv, "f:")) == -1) {
		usage((const char*)argv[0]);
		exit(EXIT_FAILURE);
	}

	CHECK_EQ(parse_config_file(optarg), -1, "Errore parsing file di configurazione", 0)

	signal_handle(&set);

	pthread_t listenerT, pool[ThreadsInPool];

	fdpipe[0] = fdpipe[1] = fd_sig = -1;

	countActiveThreads = ThreadsInPool;

	codaFd = initQueue();
	CHECK_EQ(codaFd, NULL, "Errore inizializzazione coda descrittori", 1)

	users = users_init(DIM_HASH, NUM_MTX_HASH, MaxConnections);
	CHECK_EQ(users, NULL, "Errore inizializzazione struttura utenti", 1)

	users_list = users_list_init();
	CHECK_EQ(users_list, NULL, "Errore inizializzazione lista utenti connessi", 1)

	fd_mtx = (pthread_mutex_t*) malloc(MaxConnections*sizeof(pthread_mutex_t));
	CHECK_EQ(fd_mtx, NULL, "Errore inizializzazione array mutex", 1)
	for (int i = 0; i < MaxConnections; i++) {
		CHECK_NEQ(pthread_mutex_init(fd_mtx + i, NULL), 0, "Errore inizializzazione mutex", 1)
	}

	fd_sig = signalfd(-1, &set, 0);
	CHECK_EQ(fd_sig, -1, "Errore signalfd", 1)

	/* Creazione pipe per la comunicazione inversa */
	CHECK_EQ(pipe(fdpipe), -1, "Errore creazione pipe", 1)

	struct stat st = {0};
	if (stat(DirName, &st) == -1) {
		mkdir(DirName, 0700);
	}
	
	/* Creazione threads */
	for (int i = 0; i < ThreadsInPool; i++) {
		CHECK_NEQ(pthread_create(pool + i, NULL, pool_func, NULL), 0, "Errore creazione thread del pool", 1)
		printf("Ho creato il thread del pool n %ld\n", pool[i]);
	}

   CHECK_NEQ(pthread_create(&listenerT, NULL, listener, &fd_sig), 0, "Errore creazione thread listener", 1)
   printf("Ho creato il listener\n");
	
	/* Attesa threads */
	int poolError = 0, res_join = 0;
   int retValuePool[ThreadsInPool];
	for (int i = 0; i < ThreadsInPool; i++) {
	   res_join = pthread_join(pool[i], (void**)&retValuePool[i]);
	   CHECK_NEQ(res_join, 0, "Errore join thread", 1)
	   if (retValuePool[i] == 0)
	      printf("il thread del pool n %ld ha terminato con successo\n", pool[i]);
	   else if (retValuePool[i] != 0) {
	      printf("il thread del pool n %ld ha terminato con fallimento\n", pool[i]);
	      poolError = 1;
	   }
	}
	
	int listenerError;
	CHECK_NEQ(pthread_join(listenerT, (void**)&listenerError), 0, "Errore join listener", 1)

	cleanup();

	if (poolError == 1 || listenerError == 1) return 1;
	return 0;
}
