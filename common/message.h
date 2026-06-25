#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdint.h>
#include "protocol.h"

/*
 * message.h / message.c
 *
 * Forma messaggi:
 *   - lettura messaggi + estrazione/parsing
 *   - costruzione messaggi TCP + notifiche UDP
 *
 * NO logica server/client.
 */

/* ─── Ricezione ─── */
/*
 * Legge da fd fino "+++" o buff pieno. buf (null-terminato).
 * Ritorna byte letti, -1 err/chius.
*/
int recv_message(int fd, char *buf, int bufsize);

/*
 * Estrae da src + offset fino a delim o max_len, salva in dest.
 */
void extract(const char *src, int offset, int max_len, const char *delim, char *dest);

/*
 * Estrae type da msg.
 * type almeno TYPE_LEN+1 byte.
 */
void parse_type(const char *msg, char *type);

/*
 * Estrae id da msg partendo da offset.
 * id_out almeno ID_LEN+1 byte.
 */
void parse_id(const char *msg, int offset, char *id_out);

/*
 * Estrae port da msg partendo da offset.
 * port_out almeno PORT_STR_LEN+1 byte.
 */
void parse_port(const char *msg, int offset, char *port_out);

/*
 * Estrae mess (fino a "+++") partendo da offset.
 * mess_out almeno MESS_MAX+1 byte.
 */
void parse_mess(const char *msg, int offset, char *mess_out);

/* ─── TCP (server → client) ─── */

/* funzioni di REGIS e CONNE */
void build_welco(char *buf);    
void build_hello(char *buf);    
void build_gobye(char *buf); 
/* funzioni di FRIE?(frie_reply) */ 
void build_ackrf(char *buf);    
/* funzioni di FRIE? */   
void build_frie_ok(char *buf);  
void build_frie_err(char *buf); 
/* funzioni di MESS? */
void build_mess_ok(char *buf);  
void build_mess_err(char *buf); 
/* funzioni di FLOO? */
void build_floo_ok(char *buf);  
/* funzioni di LIST */
void build_rlist(char *buf, int num_items);         
void build_linum(char *buf, const char *id);  
/* funzioni di CONSU */      
void build_nocon(char *buf);
void build_eirf(char *buf, const char *id);   /* friend req  */      
void build_frien(char *buf, const char *id);  /* friend acc */      
void build_nofri(char *buf, const char *id);  /* friend rej */      
void build_ssem(char *buf, const char *id, const char *mess);                  
void build_oolf(char *buf, const char *id, const char *mess);                  

/* ─── TCP (client → server) ─── */

/* buffer ALMENO 6 + 8 + 1 + 4 + 1 + 2 + 3 + 1 = 26, 64 ottimo */
void build_regis(char *buf, const char *id, uint16_t port, uint16_t mdp);

/* buffer ALMENO 6 + 8 + 1 + 2 + 3 + 1 = 21, 64 ottimo */
void build_conne(char *buf, const char *id, uint16_t mdp);

void build_frie_req(char *buf, const char *id);      
void build_mess_req(char *buf, const char *id, const char *mess);               
void build_floo_req(char *buf, const char *mess);    
void build_list_req(char *buf);                      
void build_consu_req(char *buf);                     
void build_iquit(char *buf);                         
void build_okirf(char *buf);                     
void build_nokrf(char *buf);                     

/* ─── Notifiche UDP ─── */

/*
 * Notifica UDP 3 byte: YXX
 *   flow_code : 0-4 (FlowType)
 *   unchecked : numero di flussi non consultati
 *
 * buf almeno 4 byte (3 + '\0').
 */
void build_udp_notify(char *buf, FlowType flow_code, int unchecked);

#endif 