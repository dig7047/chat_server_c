#ifndef CLIENT_H
#define CLIENT_H

#include <stdint.h>
#include "../common/protocol.h"

/*
 * client.h
 *
 * Struttura e funzioni del client.
 *
 * Due socket aperti contemporaneamente:
 *   - tcp_fd  : comunicazione server, bidirezionale
 *   - udp_fd  : in ascolto su porta UDP, notifiche [YXX]
 *
 * Il loop principale select() con tre fd monitorati:
 *   - tcp_fd        : risposte del server
 *   - udp_fd        : notifiche dal server
 *   - STDIN_FILENO  : input da tastiera
 */

/* ─── Stato del client ─── */

typedef struct {
    char     id[ID_LEN + 1];       /* id (8 char + '\0')        */
    uint16_t password;             /* password le            */
    uint16_t udp_port;             /* porta UDP notifiche   */
    int      tcp_fd;               /* socket TCP server            */
    int      udp_fd;               /* socket UDP ascolto        */
    bool     authenticated;        /* true se WELCO o HELLO  */
} ClientState;

/* ─── Ciclo di vita ─── */

/*
 * Inizializza ClientState:
 *   - copia id, password, udp_port
 *   - apre socket UDP ascolto su udp_port con open_udp_listen()
 *   - imposta tcp_fd = -1 e authenticated = false
 * Ritorna 0 successo, -1 errore.
 */
int client_init(ClientState *cs, uint16_t udp_port);

/*
 * Connette client a server TCP sull'ip e porta.
 * Imposta cs->tcp_fd con il socket connesso.
 * Ritorna 0 successo, -1 errore.
 */
int client_connect(ClientState *cs, const char *server_ip, uint16_t server_port);

/*
 * Chiude tcp_fd e udp_fd, azzera authenticated.
 * Chiamata su SIGINT o uscita volontaria.
 */
void client_shutdown(ClientState *cs);

/* ─── Loop principale ─── */

/*
 * Loop select() con tre fd.
 *
 * - STDIN_FILENO pronto : legge il comando, lo invia al server
 * - tcp_fd pronto       : legge la risposta server, la mostra all'utente
 * - udp_fd pronto       : legge la notifica [YXX], mostra un avviso
 *
 * esce su IQUIT o errore fatale.
 */
void client_run(ClientState *cs);

/* ─── Comandi ─── */

/*
 * Legge riga da stdin, costruisce il messaggio TCP
 * con build_*() di message.c, invia con send_msg().
 * Ritorna 0 successo, -1 socket chiuso o input invalido.
 *
 * Comandi supportati:
 *   regis <id> <port> <password>
 *   conne <id> <password>
 *   frie  <id>
 *   mess  <id> <testo>
 *   floo  <testo>
 *   list
 *   consu
 *   quit
 */
int client_handle_input(ClientState *cs, const char *line);

/* ─── Risposte TCP ─── */

/*
 * Legge con recv_message(), parse_type() e stampa la risposta.
 * Gestisce tutti i tipi:
 *   WELCO, HELLO, GOBYE, FRIE>, FRIE<, MESS>, MESS<, FLOO>,
 *   RLIST, LINUM, SSEM>, OOLF>, EIRF>, ACKRF, FRIEN, NOFRI, NOCON.
 *
 * Per EIRF> chiede all'utente e invia OKIRF o NOKRF.
 * Ritorna 0 successo, -1 connessione chiusa.
 */
int client_handle_tcp(ClientState *cs);

/* ─── Notifiche UDP ─── */

/*
 * Legge una notifica UDP [YXX].
 * Decodifica Y e XX e stampa un avviso all'utente.
 * Non ritorna codici di errore — notifica UDP persa accettabile.
 */
void client_handle_udp(ClientState *cs);

#endif /* CLIENT_H */