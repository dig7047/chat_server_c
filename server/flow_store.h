#ifndef FLOW_STORE_H
#define FLOW_STORE_H

#include "../common/protocol.h"

/*
 * flow_store.h / flow_store.c
 *
 * Gestione coda FIFO flussi.
 * Esclusivamente in memoria.
 *
 * La coda implementata come array con shift
 * sufficiente per MAX_FLOWS elementi di dimensione ridotta.
 */

/* ─── Aggiunta ─── */

/*
 * Aggiunge flusso in c->flows.
 *
 * type    : tipo di flusso 
 * from_id : identificativo mittente (8 char), mai NULL
 * message : contenuto testuale, puo' essere NULL per i flussi di amicizia
 *
 * Ritorna  0 successo.
 * Ritorna -1 se la coda e' piena: flusso scartato 
 */
int fs_push(Client *c, FlowType type,
            const char *from_id, const char *message);

/* ─── Consumo ─── */

/*
 * Estrae il primo flusso e lo copia in *out.
 * Il flusso rimosso da c->flows e flow_count decrementato.
 *
 * Ritorna  1 successo
 * Ritorna  0 coda vuota
 * Ritorna -1 errore
 *
 * Nota: elementi shiftati di 1 con memmove.
 */
int fs_pop(Client *c, Flow *out);

/* ─── Ispezione ─── */

/*
 * Ritorna 1 coda vuota, -1 se errore, 0 altrimenti.
 */
int fs_is_empty(const Client *c);

#endif /* FLOW_STORE_H */