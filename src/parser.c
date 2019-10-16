
/** \file parser.c  
      \author Giuseppe Muntoni 
      Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
      originale dell'autore  
     */  

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

#define N 1024

/* parametri del file di configurazione */
char UnixPath[UNIX_PATH_MAX];
char DirName[256];
char StatFileName[256];
long MaxConnections, ThreadsInPool, MaxMsgSize, MaxFileSize, MaxHistMsgs;

/**	Elimina spazi, tab e newline da una stringa e rende tutti i caratteri minuscoli
 */
void normalize_str (char* str, char* str2) {
	//Elimino spazi e tab
	for (int i=0,j=0; ; i++) {	
    	if ((str[i] != ' ') && (str[i] != '\t') && (str[i] != '\n'))
        	str2[j++] = str[i];
    	if (str[i] == '\0')
       		break;
	}
	//Rendo tutti i caratteri minuscoli
	for (int i = 0; str2[i]; i++) {
		if (((int)str2[i] >= 65) && ((int)str2[i] <= 90))
			str2[i] = (char)((int)str2[i] + 32);
	} 
}

int parse_config_file (char* path_file) {
	char buf[N];			//Contiene la riga letta (si suppone non sia più lunga di 1024 caratteri)
	char normal_str[N];	//Contiene la riga letta senza spazi, tab e con tutti i caratteri minuscoli
	char* token = NULL;	

	//Inizializzo stringhe
	memset(buf, '\0', N);
	memset(normal_str, '\0', N);
	memset(UnixPath, '\0', UNIX_PATH_MAX);
	memset(DirName, '\0', 256);
	memset(StatFileName, '\0', 256);
	//Inizializzo tutti i valori a -1
	MaxConnections = -1; ThreadsInPool = -1; MaxMsgSize = -1; MaxFileSize = -1; MaxHistMsgs = -1;

	//Apro il file di configurazione
	FILE *conf = fopen(path_file, "rb");
	if (conf == NULL) return -1;

	while (fgets(buf, N-1, conf) != NULL) {
		//Rendo tutti caratteri minuscoli e tolgo spazi, tab e newline dalla riga letta
		normalize_str(buf, normal_str); 	
		
		//I commenti vengono ignorati
		if (normal_str[0] == '#') 
			continue; 
		
		if (UnixPath[0] == '\0' && ((token = strstr(normal_str, "unixpath=")) != NULL || (token = strstr(normal_str, "unixpath:")) != NULL)) {
			token += strlen("unixpath=");
			strncpy(UnixPath, token, strlen(token));
		}
		else if (DirName[0] == '\0' && ((token = strstr(normal_str, "dirname=")) != NULL || (token = strstr(normal_str, "dirname:")) != NULL)) {
			token += strlen("dirname=");
			strncpy(DirName, token, strlen(token));
		}
		else if (StatFileName[0] == '\0' && ((token = strstr(normal_str, "statfilename=")) != NULL || (token = strstr(normal_str, "statfilename:")) != NULL)) {
			token += strlen("statfilename=");
			strncpy(StatFileName, token, strlen(token));
		}
		else if (MaxConnections == -1 && ((token = strstr(normal_str, "maxconnections=")) != NULL || (token = strstr(normal_str, "maxconnections:")) != NULL)) {
			token += strlen("maxconnections=");
			MaxConnections = strtol(token, NULL, 10);
		}
		else if (ThreadsInPool == -1 && ((token = strstr(normal_str, "threadsinpool=")) != NULL || (token = strstr(normal_str, "threadsinpool:")) != NULL)) {
			token += strlen("threadsinpool=");
			ThreadsInPool = strtol(token, NULL, 10);
		}
		else if (MaxMsgSize == -1 && ((token = strstr(normal_str, "maxmsgsize=")) != NULL || (token = strstr(normal_str, "maxmsgsize:")) != NULL)) {
			token += strlen("maxmsgsize=");
			MaxMsgSize = strtol(token, NULL, 10);
		}
		else if (MaxFileSize == -1 && ((token = strstr(normal_str, "maxfilesize=")) != NULL || (token = strstr(normal_str, "maxfilesize:")) != NULL)) {
			token += strlen("maxfilesize=");
			MaxFileSize = strtol(token, NULL, 10);
		}
		else if (MaxHistMsgs == -1 && ((token = strstr(normal_str, "maxhistmsgs=")) != NULL || (token = strstr(normal_str, "maxhistmsgs:")) != NULL)) {
			token += strlen("maxhistmsgs=");
			MaxHistMsgs = strtol(token, NULL, 10);
		}

		memset(buf, '\0', N);
		memset(normal_str, '\0', N);
	}

	fclose(conf);

	if (UnixPath[0] == '\0' || DirName[0] == '\0')
		return -1;
	
	if (MaxConnections == -1 || ThreadsInPool == -1 || MaxMsgSize == -1 || MaxFileSize == -1 || MaxHistMsgs == -1)
		return -1;

	return 0;
} 