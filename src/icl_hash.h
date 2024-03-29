/**
 * @file
 *
 * Header file for icl_hash routines.
 *
 */
/* $Id$ */
/* $UTK_Copyright: $ */

/** Sono state effettuate delle aggiunte
 */

#ifndef icl_hash_h
#define icl_hash_h

#include <stdio.h>

#if defined(c_plusplus) || defined(__cplusplus)
extern "C" {
#endif

typedef struct icl_entry_s {
    void* key;
    void* data;
    struct icl_entry_s* next;
} icl_entry_t;

typedef struct icl_hash_s {
    int nbuckets;
    int nentries;
    icl_entry_t **buckets;
    unsigned int (*hash_function)(void*);
    int (*hash_key_compare)(void*, void*);
} icl_hash_t;

/** Iteratore sulla tabella hash
 */
typedef struct icl_iterator_s {
   icl_hash_t* ht;
   icl_entry_t* next;
   int idx_bucket;
} icl_iterator_t;

icl_hash_t *
icl_hash_create( int nbuckets, unsigned int (*hash_function)(void*), int (*hash_key_compare)(void*, void*) );

void
* icl_hash_find(icl_hash_t *, void* );

icl_entry_t
* icl_hash_insert(icl_hash_t *, void*, void *);

int
icl_hash_destroy(icl_hash_t *, void (*)(void*), void (*)(void*)),
    icl_hash_dump(FILE *, icl_hash_t *);

int icl_hash_delete( icl_hash_t *ht, void* key, void (*free_key)(void*), void (*free_data)(void*) );

icl_iterator_t* icl_iterator_create(icl_hash_t* ht);

icl_entry_t* icl_hash_iterate(icl_iterator_t* icl_iterator);

void icl_iterator_destroy(icl_iterator_t* icl_iterator);

#if defined(c_plusplus) || defined(__cplusplus)
}
#endif

#endif /* icl_hash_h */
