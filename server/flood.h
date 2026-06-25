#ifndef FLOOD_H
#define FLOOD_H

#include "../common/protocol.h"
#include "server.h"

#include <stdbool.h>

/*
 * flood.h / flood.c
 *
 * Implementazione flood (FLOO?): propagazione di un messaggio tramite BFS.
 *
 * Il grafo amicizie e' non orientato, nodi sono gli indici in srv->clients.
 * BFS usa visited[] per non visitare due volte lo stesso nodo.
 *
 * Il mittente NON riceve il messaggio flood.
 */

/*
 * Propaga mess a client raggiungibili da sender attraverso amicizie.
 *
 * Per ogni client raggiunto:
 *   - aggiunge FLOW_FLOOD a coda con fs_push()
 *   - notifica UDP [4XX] con send_udp_notify()
 *
 * sender: client che ha inviato FLOO?
 * mess: testo messaggio
 *
 * Ritorna numero client raggiunti.
 * Ritorna 0 se sender 0 amici o se tutti coda piena.
 */
int flood_send(Server *srv, Client *sender, const char *mess);

/*
 * Visita grafo amicizie a partire da start_index e marca visited
 *
 * visited: array bool dove, client raggiunti = true.
 */
void flood_bfs(Server *srv, int start_index, bool *visited);

#endif /* FLOOD_H */