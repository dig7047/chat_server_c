#ifndef SERVER_H
#define SERVER_H

#include <sys/select.h>
#include "../common/protocol.h"

/*
 * server.h / server_main.c
 *
 * Ciclo vita server:
 *   - inizializzazione
 *   - loop select()
 *   - dispatch
 *   - shutdown
 *
 * Struttura definita in protocol.h.
 * Contesto per loop (fd_set, fdmax).
 */

/* ─── Contesto loop select() ─── */

typedef struct {
    Server  *srv;           /* puntatore server */
    fd_set   master_fds;    /* fd monitorati */
    fd_set   read_fds;      /* copia per select() */
    int      fdmax;         /* fd piu' alto */
} SelectCtx;

/* ─── Ciclo di vita ─── */

/*
 * Inizializza Server:
 *   - azzera client e contatore
 *   - apre socket TCP ascolto su port
 *   - inizializza SelectCtx
 * Ritorna 0 successo, -1 errore.
 */
int server_init(Server *srv, SelectCtx *ctx, uint16_t port);

/*
 * Loop principale: select() e smista eventi.
 * Non ritorna (exit() su errore fatale).
 */
void server_run(Server *srv, SelectCtx *ctx);

/*
 * Chiude socket aperti e libera risorse.
 * Chiamato su SIGINT o errore fatale.
 */
void server_shutdown(Server *srv, SelectCtx *ctx);

/* ─── Gestione connessioni ─── */

/*
 * Accetta nuova connessione TCP da srv->listen_fd.
 * Aggiunge fd a SelectCtx e salva IP in srv->pending_ip[fd].
 * Ritorna fd, -1 errore.
 */
int server_accept(Server *srv, SelectCtx *ctx);

/*
 * Gestisce chiusura della connessione su fd.
 * Rimuove fd da SelectCtx, chiude socket, marca client tcp_fd = -1.
 */
void server_disconnect(Server *srv, SelectCtx *ctx, int fd);

/* ─── Dispatch ─── */

/*
 * Legge messaggio da fd e handler.
 * fd non autenticato (REGIS/CONNE)
 * client autenticato (tutti)
 * Per messaggio non riconosciuto o malformato risponde GOBYE+++
 * e chiama server_disconnect().
 */
void server_dispatch(Server *srv, SelectCtx *ctx, int fd);

#endif /* SERVER_H */