#ifndef HANDLERS_H
#define HANDLERS_H

#include "../common/protocol.h"
#include "server.h"

/*
 * handlers.h / handlers.c
 *
 * Handler messaggi TCP server.
 * Ogni funzione:
 *   1. parsing campi necessari
 *   2. aggiorna memoria (cs_*, fs_*)
 *   3. invia risposte TCP
 *   4. invia notifiche UDP
 *   5. chiama server_disconnect() per errore di rete
 *
 * dispatch in server_dispatch() in server_main.c
 */

/* ─── Pre-autenticazione ─── */

/*
 * REGIS id port mdp+++
 *
 * Registra client su server.
 * Parsing: id offset 6, port offset 15, mdp offset 20.
 *
 * Risponde WELCO+++ e cs_add() se:
 *   - server non pieno
 *   - id non registrato
 *   - id valido
 * Risponde GOBYE+++.
 */
void handle_regis(Server *srv, SelectCtx *ctx, int fd, const char *msg);

/*
 * CONNE id mdp+++
 *
 * Riconnette client.
 * Parsing: id offset 6, mdp offset 15.
 *
 * Risponde HELLO+++ e cs_set_online() se:
 *   - id esiste
 *   - password corrisponde
 *   - il client non connesso
 * Risponde GOBYE+++.
 */
void handle_conne(Server *srv, SelectCtx *ctx, int fd, const char *msg);

/* ─── Post-autenticazione ─── */

/*
 * FRIE? id+++
 *
 * Richiesta di amicizia client id.
 * Parsing: id offset 6.
 *
 * Risponde FRIE>+++ se client id esiste:
 *   - aggiunge FLOW_FRIEND_REQUEST a dest, fs_push()
 *   - notifica UDP a dest, send_udp_notify()
 * Risponde FRIE<+++ se id non esiste.
 */
void handle_frie(Server *srv, SelectCtx *ctx, Client *c, const char *msg);

/*
 * MESS? id mess+++
 *
 * Invio messaggio privato all'amico id.
 * Parsing: id dest offset 6, testo mess offset 15.
 *
 * Risponde MESS>+++ se:
 *   - id esiste
 *   - c e id sono amici
 *   - messaggio valido
 *   - aggiunge FLOW_MESSAGE a dest, fs_push()
 *   - notifica UDP a dest, send_udp_notify()
 * Risponde MESS<+++ altrimenti.
 */
void handle_mess(Server *srv, SelectCtx *ctx, Client *c, const char *msg);

/*
 * FLOO? mess+++
 *
 * Flood: invia mess a tutti gli amici, amici degli amici, ecc.
 * Parsing: mess a offset 6.
 * Delega la BFS a flood_send() in flood.c.
 *
 * Risponde FLOO>+++ sempre.
 */
void handle_floo(Server *srv, SelectCtx *ctx, Client *c, const char *msg);

/*
 * LIST?+++
 *
 * Richiesta elenco di tutti i client.
 *
 * Risponde con:
 *   RLIST NNN+++          (NNN = numero di client 3 cifre)
 *   LINUM id+++           (NNN volte)
 *
 * tutti i client registrati.
 */
void handle_list(Server *srv, SelectCtx *ctx, Client *c);

/*
 * CONSU+++
 *
 * Consultazione flusso in coda.
 *
 * Se has_pending_flow == 1 (flusso in sospeso)
 * rimanda stesso flusso.
 * Se coda vuota risponde NOCON+++.
 *
 * Estrae flusso con fs_pop(), salva in pending_flow,
 * imposta has_pending_flow = 1 e risponde:
 *
 *   FLOW_MESSAGE        → SSEM> from_id mess+++
 *                         se send_msg ok: has_pending_flow = 0
 *   FLOW_FLOOD          → OOLF> from_id mess+++
 *                         se send_msg ok: has_pending_flow = 0
 *   FLOW_FRIEND_REQUEST → EIRF> from_id+++
 *                         se send_msg ok: imposta waiting_friend_reply = 1
 *                         e pending_friend_id = from_id
 *                         (has_pending_flow = 1 fino ACKRF)
 *   FLOW_FRIEND_ACCEPT  → FRIEN from_id+++
 *                         se send_msg ok: has_pending_flow = 0
 *   FLOW_FRIEND_REJECT  → NOFRI from_id+++
 *                         se send_msg ok: has_pending_flow = 0
 *
 * errore su send_msg chiama server_disconnect().
 * has_pending_flow rimane 1 cosi' flusso viene riproposto.
 */
void handle_consu(Server *srv, SelectCtx *ctx, Client *c);

/*
 * Risposta OKIRF+++ o NOKRF+++ dopo EIRF> e waiting_friend_reply = 1.
 *
 * Se OKIRF:
 *   - cs_add_friendship() con c e req
 *   - fs_push(FLOW_FRIEND_ACCEPT) a req
 *   - send_udp_notify a req (se fs_push successo)
 * Se NOKRF (o altro messaggio):
 *   - fs_push(FLOW_FRIEND_REJECT) a req
 *   - send_udp_notify a req (se fs_push successo)
 * entrambi:
 *   - waiting_friend_reply = 0, pending_friend_id = "", has_pending_flow = 0
 *   - send_msg(ACKRF+++) a c
 * errore su send_msg chiama server_disconnect().
 */
void handle_friend_reply(Server *srv, SelectCtx *ctx, Client *c, const char *type);

/*
 * IQUIT+++
 *
 * Disconnessione client.
 *
 * Risponde GOBYE+++ e server_disconnect()
 */
void handle_quit(Server *srv, SelectCtx *ctx, Client *c);

#endif /* HANDLERS_H */