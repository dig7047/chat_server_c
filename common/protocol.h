#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <sys/select.h>

/* ─── Limiti ─── */

#define MAX_CLIENTS     100
#define TYPE_LEN        5       
#define ID_LEN          8       
#define MESS_MAX        200     
#define MAX_FLOWS       255     
#define PORT_STR_LEN    4       
#define MSG_BUF_SIZE    500     
#define MSG_TERMINATOR  "+++"   

/* ─── Tipi di flusso (Y in [YXX]) ─── */

typedef enum {
    FLOW_FRIEND_REQUEST = 0,
    FLOW_FRIEND_ACCEPT  = 1,
    FLOW_FRIEND_REJECT  = 2, 
    FLOW_MESSAGE        = 3, 
    FLOW_FLOOD          = 4 
} FlowType;

/* ─── Flusso ─── */

typedef struct {
    FlowType type;
    char     from_id[ID_LEN + 1];   /* mittente */
    char     message[MESS_MAX + 1]; /* contenuto */
} Flow;

/* ─── Cliente ─── */

typedef struct {
    char     id[ID_LEN + 1];          /* id (8 + '\0') */
    uint16_t password;                 /* password l-e */
    char     ip[INET6_ADDRSTRLEN];     /* IP sorgente (IPv4 o IPv6) */
    uint16_t udp_port;                 /* porta UDP */
    int      tcp_fd;                   /* fd, -1 discon */

    int      friends[MAX_CLIENTS];    /* indici nell'array Server.clients */
    int      friend_count;

    bool     waiting_friend_reply;     /* 1 se aspetta OKIRF/NOKRF */
    char     pending_friend_id[ID_LEN + 1]; /* ID richiedente */
    
    bool     has_pending_flow;         /* 1 se flusso non confermato */
    Flow     pending_flow;             /* Copia flusso */

    Flow     flows[MAX_FLOWS];         /* coda FIFO flussi */
    int      flow_count;
} Client;

/* ─── Server ─── */

typedef struct {
    Client   clients[MAX_CLIENTS];
    int      count;                    /* slot occupati */
    int      listen_fd;                /* socket TCP ascolto */

    char     pending_ip[FD_SETSIZE][INET6_ADDRSTRLEN]; /* Indice = file descriptor */
} Server;

#endif /* PROTOCOL_H */