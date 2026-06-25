#ifndef CLIENT_STORE_H
#define CLIENT_STORE_H

#include <stdint.h>
#include "../common/protocol.h"

/*
 * client_store.h / client_store.c
 *
 * Gestione Server.clients: aggiunta, ricerca, rimozione e amicizie.
 * Opera esclusivamente sulla memoria.
 */

/* ─── Aggiunta / rimozione ─── */

/*
 * Aggiunge nuovo client.
 * Inizializza i campi: id, password, ip, udp_port,tcp_fd, 
 * azzera friend_count e flow_count.
 *
 * Ritorna puntatore al Client aggiunto,
 * NULL se array pieno o id gia' presente.
 */
Client *cs_add(Server *srv, const char *id, uint16_t password, const char *ip, uint16_t udp_port, int fd);

/*
 * Marca client disconnesso, tcp_fd = -1.
 * NON rimuove il client, puo riconnettersi con CONNE.
 * Nulla se c == NULL.
 */
void cs_set_offline(Client *c);

/*
 * Aggiorna tcp_fd e ip
 * quando riconnessione (CONNE).
 * Nulla se c == NULL.
 */
void cs_set_online(Client *c, int fd, const char *ip);

/* ─── Ricerca ─── */

/*
 * Cerca client per identificativo.
 * Ritorna puntatore, NULL se non trovato.
 */
Client *cs_find_by_id(Server *srv, const char *id);

/*
 * Cerca client per fd TCP attivo.
 * Ritorna puntatore, NULL se non trovato
 */
Client *cs_find_by_fd(Server *srv, int fd);

/*
 * Ritorna indice in srv->clients del client con l'id indicato,
 * -1 se non trovato.
 */
int cs_index_of(Server *srv, const char *id);

/* ─── Amicizie ─── */

/*
 * Aggiunge amicizia tra a e b.
 * Inserisce indice di b in a->friends e viceversa.
 *
 * Ritorna:
 * 0 successo
 * -1 array friends pieno
 * 1 se gia' amici.
 * 2 errore
 */
int cs_add_friendship(Server *srv, int idx_a, int idx_b);

/*
 * Controlla se a e b sono amici.
 * Ritorna:
 * 1 si 
 * 0 no.
 * 2 errore
 */
int cs_are_friends(Server *srv, int idx_a, int idx_b);

/*
 * Restituisce in out_indices indici (in srv->clients) degli
 * amici del client c. Scrive al massimo max_out valori.
 * Ritorna il numero di amici effettivamente restituiti.
 * Ritorna  -1 in caso di errore
 */
int cs_get_friend_indices(const Client *c, int *out_indices, int max_out);

/* ─── Utilita' ─── */

/*
 * Verifica se id e'registrato sul server.
 */
int cs_id_exists(Server *srv, const char *id);

/*
 * Verifica se server è pieno 1 o se ha posto 0.
 */
int cs_is_full(const Server *srv);

#endif /* CLIENT_STORE_H */