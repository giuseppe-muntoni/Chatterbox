
/** \file parser.h  
       \author Giuseppe Muntoni
       Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
       originale dell'autore  
     */  

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 64
#endif

extern char UnixPath[UNIX_PATH_MAX];
extern char DirName[256];
extern char StatFileName[256];
extern long MaxConnections, ThreadsInPool, MaxMsgSize, MaxFileSize, MaxHistMsgs;

/** Effettua il parsing del file di configurazione
 * 
 *  \param pathFile:  path del file di configurazione
 *  \return:          se successo allora 0
 *                    se errore allora -1
 */
int parse_config_file (char* path_file);
