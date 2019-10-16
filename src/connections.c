
/** \file connections.c 
      \author Giuseppe Muntoni
      Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
      originale dell'autore  
     */  

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <conn.h>
#include <errno.h>
#include <string.h>
#include <connections.h>

int openConnection(char* path, unsigned int ntimes, unsigned int secs) {
	if (ntimes > MAX_RETRIES || secs > MAX_SLEEPING) 
		return -1;
	
	struct sockaddr_un sa;
	strncpy(sa.sun_path, path, UNIX_PATH_MAX);
	sa.sun_family = AF_UNIX;
	
	//Creo la socket
	int fd_sock = socket(AF_UNIX, SOCK_STREAM, 0);
	
	if (fd_sock == -1) 
		return -1;
	
	while(ntimes > 0) {
		if (connect(fd_sock, (const struct sockaddr*)&sa, sizeof(sa)) == 0) {
		   printf("connessione stabilita : %d\n", fd_sock);
         return fd_sock;
      }
		ntimes--;
		sleep(secs);
	}
	
	return -1;
}

int readHeader(long connfd, message_hdr_t* hdr) {
	errno = 0;
	int k = readn(connfd, hdr, sizeof(message_hdr_t));
	//Controllo esito
	if (k == 0)
		return 0;
	else if (k == -1) {
		if (errno == ECONNRESET)
			return 0;
		perror("Errore read header");
		return -1;
	}
	return k;
}

int readData(long fd, message_data_t* data) {
	errno = 0;
	int k = readn(fd, &(data -> hdr), sizeof(message_data_hdr_t));
	//controllo esito
	if (k == 0)
		return 0;
	else if (k == -1) {
		if (errno == ECONNRESET)
			return 0;
		perror("Errore read data");
		return -1;
	}

	int len = (data -> hdr).len;
	if (len != 0) {
		data -> buf =  (char*) malloc(len);
		if (data -> buf == NULL) 
			return -1;
		memset((data -> buf), '\0', len);
		
		k = readn(fd, data -> buf, len);
		if (k == 0) 
			return 0;
		else if (k == -1) {
			if (errno == ECONNRESET)
				return 0;
			perror("Errore read buffer");
			return -1;
		}
	}

	return k;
}

int readMsg(long fd, message_t* msg) {
	int byteHeader, byteData;
	byteHeader = readHeader(fd, &(msg->hdr));
	if (byteHeader == 0) 
		return 0;
	else if (byteHeader == -1) 
		return -1;

	byteData = readData(fd, &(msg->data));
	if (byteData == 0) 
		return 0;
	else if (byteData == -1) 
		return -1;
	
	return (byteHeader + byteData);
}

int sendRequest(long fd, message_t* msg) {
	int k = writen(fd, &(msg -> hdr), sizeof(message_hdr_t));
	if (k == -1) {
		perror("Errore invio header richiesta");
		return -1;
	}
	k = writen(fd, &((msg -> data).hdr), sizeof(message_data_hdr_t));
	if (k == -1) {
		perror("Errore invio dati richiesta");
		return -1;
	}
	k = writen(fd, ((msg -> data).buf), (msg -> data).hdr.len);
	if (k == -1) {
		perror("Errore invio buffer richiesta");
		return -1;
	}
	return 1;
}

int sendData(long fd, message_data_t* msg) {
	int k = writen(fd, &(msg -> hdr), sizeof(message_data_hdr_t));
	if (k == -1) 
		return -1;
	
	k = writen(fd, msg -> buf, (msg -> hdr).len);
	if (k == -1) 
		return -1;
	
	return 1;
}

/* LATO SERVER */

int sendHeader(long fd, message_hdr_t* msg) {
	int k = writen(fd, msg, sizeof(message_hdr_t));
	if (k == -1) 
		return -1; 
	
	return 1;
}



