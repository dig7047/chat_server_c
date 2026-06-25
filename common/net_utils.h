#ifndef NET_UTILS_H
#define NET_UTILS_H

#include <stdint.h>
#include <stdio.h>
#include "protocol.h"

/*
 * net_utils.h / net_utils.c
 *
 * Utility comunicazione rete:
 *   - invio garantito (send() parziali)
 *   - invio notifiche UDP
 *   - apertura socket TCP e UDP
 *   - log verboso
 */

/* ─── Modalita' verbose ─── */

/*
 * verbose_mode != 0,
 * Impostato dal main() flag -v.
 */
extern int verbose_mode;

#define VLOG(fmt, ...) \
    do { if (verbose_mode) fprintf(stderr, "[VERBOSE] " fmt "\n", ##__VA_ARGS__); } while(0)

/* ─── Invio affidabile ─── */

/*
 * Invia len byte da buf su socket fd.
 * Gestisce send() parziali.
 */
int send_all(int fd, const char *buf, int len);

/*
 * Costruisce msg e invia su fd con send_all().
 * Stampa su stderr se verbose_mode attivo.
 * msg null-terminato.
 *
 * Nota: usa strlen(msg) per la lunghezza — msg NON deve contenere '\0'
 * interni (la password binaria gestita con send_all diretto).
 */
int send_msg(int fd, const char *msg);

/* ─── Notifiche UDP ──── */

/*
 * Invia notifica UDP [YXX] a porta udp_port dell'indirizzo ip.
 * flow_code : codice flusso (0-4)
 * unchecked : flussi non consultati
 *
 * Socket UDP temporaneo
 */
void send_udp_notify(const char *ip, uint16_t udp_port, FlowType flow_code, int unchecked);

/* ─── Apertura socket ─── */

/*
 * Apre socket TCP in ascolto sulla porta.
 * Imposta SO_REUSEADDR, bind e listen.
 * Ritorna fd se successo, -1 se errore.
 */
int open_tcp_listen(uint16_t port);

/*
 * Apre socket UDP in ascolto sulla porta(lato client).
 * Ritorna fd se successo, -1 se errore.
 */
int open_udp_listen(uint16_t port);

/* ─── Utilita' ─── */

/*
 * Controlla s identificativo valido:
 *   - ID_LEN caratteri 
 *   - no '+++'
 * Ritorna 1 se valido, 0 altrimenti.
 */
int is_valid_id(const char *s);

/*
 * Controlla mess no "+++" e che strlen(mess) <= MESS_MAX.
 * Ritorna 1 se valido, 0 altrimenti.
 */
int is_valid_mess(const char *mess);

#endif /* NET_UTILS_H */