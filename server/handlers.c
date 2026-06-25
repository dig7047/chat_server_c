#include "handlers.h"      
#include "client_store.h"  
#include "flow_store.h" 
#include "flood.h"   

#include "../common/message.h"   
#include "../common/net_utils.h" 

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ─── Pre-autenticazione ─── */

void handle_regis(Server *srv, SelectCtx *ctx, int fd, const char *msg) {
    if(srv == NULL || ctx == NULL || msg == NULL || fd < 0) return;

	char id_buf[ID_LEN + 1];
	char port_buf[PORT_STR_LEN +1];
	char reply[MSG_BUF_SIZE];

	parse_id(msg, 6, id_buf);
	parse_port(msg, 15, port_buf);

	uint16_t udp_port = (uint16_t)atoi(port_buf);
    uint16_t password = (unsigned char)msg[20] | ((unsigned char)msg[21] << 8);

	if(cs_is_full(srv)){
        VLOG("REGIS Fallita per fd %d: server pieno", fd);
        goto regis_failed;
    }

    if(cs_id_exists(srv, id_buf)){
        VLOG("REGIS Fallita per fd %d: id '%s' esiste", fd, id_buf);
        goto regis_failed;
    }

    if(!is_valid_id(id_buf)){
        VLOG("REGIS Fallita per fd %d: id '%s' non valido", fd, id_buf);
        goto regis_failed;
    }

    const char *client_ip = srv->pending_ip[fd];
    Client *new_cl = cs_add(srv, id_buf, password, client_ip, udp_port, fd);

    if(new_cl == NULL){
        VLOG("REGIS Fallita per fd %d: cs_add fallita", fd);
        goto regis_failed;
    }

    VLOG("REGIS Successo per fd %d registrato come client '%s'", fd, id_buf);
    build_welco(reply);
    if(send_msg(fd, reply) < 0){
        VLOG("REGIS Fallita, rete caduta inviando WELCO a '%s'", id_buf);
        server_disconnect(srv, ctx, fd);
    }  
    return;

regis_failed:
    build_gobye(reply);
    send_msg(fd, reply);
    server_disconnect(srv, ctx, fd);
}

void handle_conne(Server *srv, SelectCtx *ctx, int fd, const char *msg) {
    if(srv == NULL || ctx == NULL || msg == NULL || fd < 0) return;

	char id_buf[ID_LEN + 1];
	char reply[MSG_BUF_SIZE];

    parse_id(msg, 6, id_buf);
    uint16_t password = (unsigned char)msg[15] | ((unsigned char)msg[16] << 8);

    Client *c = cs_find_by_id(srv, id_buf);

    if(c == NULL){
        VLOG("CONNE Fallita per fd %d: id '%s' non esiste", fd, id_buf);
        goto conne_failed;
    }

    if(c->password != password){
        VLOG("CONNE Fallita per fd %d: password errata per id '%s'", fd, id_buf);
        goto conne_failed;
    }

    if(c->tcp_fd != -1){
        VLOG("CONNE Fallita per fd %d: id '%s' gia connesso su fd '%d", fd, id_buf, c->tcp_fd);
        goto conne_failed;
    }

    const char *new_ip = srv->pending_ip[fd];
    cs_set_online(c, fd, new_ip);

    VLOG("CONNE Successo per id '%s', ONLINE con fd %d da [%s]", id_buf, fd, new_ip);
    build_hello(reply);
    if(send_msg(fd, reply) < 0){
        VLOG("CONNE Fallita, rete caduta inviando HELLO a '%s'", id_buf);
        server_disconnect(srv, ctx, fd);
    }  
    return;

conne_failed:
    build_gobye(reply);
    send_msg(fd, reply);
    server_disconnect(srv, ctx, fd);
}

/* ─── Post-autenticazione ─── */

void handle_frie(Server *srv, SelectCtx *ctx, Client *c, const char *msg) {
    if(srv == NULL || ctx == NULL || c == NULL || msg == NULL) return;

    char id_buf[ID_LEN + 1];
    char reply[MSG_BUF_SIZE];

    parse_id(msg, 6, id_buf);

    if(strcmp(c->id, id_buf) == 0){
        VLOG("FRIE Fallita per id '%s': ha cercato di aggiungere se stesso", c->id);
        goto frie_failed;
    }

    Client *dest = cs_find_by_id(srv, id_buf);
    if(dest == NULL){
        VLOG("FRIE Fallita per id '%s': il destinatario '%s' non esiste", c->id, id_buf);
        goto frie_failed;
    }

    int idx_send = cs_index_of(srv, c->id);
    int idx_dest = cs_index_of(srv, dest->id);
    if(cs_are_friends(srv, idx_send, idx_dest) == 1){
        VLOG("FRIE Fallita per id '%s': e' gia' amico di '%s'", c->id, id_buf);
        goto frie_failed;
    }

    if(fs_push(dest, FLOW_FRIEND_REQUEST, c->id, NULL) < 0){
        VLOG("FRIE Fallita per id '%s': coda di '%s' piena, notifica scartata", c->id, id_buf);
        goto frie_failed;
    }

    send_udp_notify(dest->ip, dest->udp_port, FLOW_FRIEND_REQUEST, dest->flow_count);
    VLOG("FRIE Successo: Richiesta da '%s' a '%s' inoltrata in coda", c->id, id_buf);

    build_frie_ok(reply);
    if(send_msg(c->tcp_fd, reply) < 0) server_disconnect(srv, ctx, c->tcp_fd);
    return;

frie_failed:
    build_frie_err(reply);
    if(send_msg(c->tcp_fd, reply) < 0) server_disconnect(srv, ctx, c->tcp_fd);
    return;
}

void handle_mess(Server *srv, SelectCtx *ctx, Client *c, const char *msg) {
    if(srv == NULL || ctx == NULL || c == NULL || msg == NULL) return;

    char id_dest[ID_LEN + 1];
    char mess[MESS_MAX + 1];
    char reply[MSG_BUF_SIZE];
    
    parse_id(msg, 6, id_dest);
    parse_mess(msg, 15, mess);

    if(!is_valid_mess(mess)){
        VLOG("MESS Fallita per id '%s': messaggio non valido", c->id);
        goto mess_failed;
    }

    if(strcmp(c->id, id_dest) == 0){
        VLOG("MESS Fallita per id '%s': ha cercato di scrivere a se stesso", c->id);
        goto mess_failed;
    }

    Client *dest = cs_find_by_id(srv, id_dest);
    if(dest == NULL){
        VLOG("MESS Fallita per id '%s': il destinatario '%s' non esiste", c->id, id_dest);
        goto mess_failed;
    }

    int idx_send = cs_index_of(srv, c->id);
    int idx_dest = cs_index_of(srv, dest->id);
    if(cs_are_friends(srv, idx_send, idx_dest) == 0){
        VLOG("MESS Fallita per id '%s': non e' amico di '%s'", c->id, id_dest);
        goto mess_failed;
    }

    if(fs_push(dest, FLOW_MESSAGE, c->id, mess) < 0){
        VLOG("MESS Fallita per id '%s': coda di '%s' piena, notifica scartata", c->id, id_dest);
        goto mess_failed;
    }

    send_udp_notify(dest->ip, dest->udp_port, FLOW_MESSAGE, dest->flow_count);
    VLOG("MESS Successo: Messaggio da '%s' a '%s' inoltrata in coda", c->id, id_dest);

    build_mess_ok(reply);
    if(send_msg(c->tcp_fd, reply) < 0) server_disconnect(srv, ctx, c->tcp_fd);
    return;

mess_failed:
    build_mess_err(reply);
    if(send_msg(c->tcp_fd, reply) < 0) server_disconnect(srv, ctx, c->tcp_fd);
    return;
}

void handle_floo(Server *srv, SelectCtx *ctx, Client *c, const char *msg) {
    if (srv == NULL || ctx == NULL || c == NULL || msg == NULL) return;

    char mess_buf[MESS_MAX + 1];
    char reply[MSG_BUF_SIZE];

    parse_mess(msg, 6, mess_buf);
    if(!is_valid_mess(mess_buf)){
        VLOG("FLOO Fallita per id '%s', messaggio non valido", c->id);
        return; 
    }

    int reached = flood_send(srv, c, mess_buf);
    VLOG("FLOO Successo, BFS delegata a flood_send per '%s', raggiunti %d client", c->id, reached);
    build_floo_ok(reply);
    if (send_msg(c->tcp_fd, reply) < 0) {
        VLOG("FLOO Avviso, rete caduta inviando OK a '%s'", c->id);
        server_disconnect(srv, ctx, c->tcp_fd);
    }
}

void handle_list(Server *srv, SelectCtx *ctx, Client *c) {
    if(srv == NULL || ctx == NULL || c == NULL) return;

    char reply[MSG_BUF_SIZE];
    int num_item = srv->count;

    build_rlist(reply, num_item);
    if(send_msg(c->tcp_fd, reply) < 0){
        VLOG("LIST Fallita, rete caduta inviando l'intestazione RLIST a '%s'", c->id);
        server_disconnect(srv, ctx, c->tcp_fd);
        return;
    }

    for(int i = 0; i < num_item; i++){
        build_linum(reply, srv->clients[i].id);
        if(send_msg(c->tcp_fd, reply) < 0){
            VLOG("LIST Fallita rete caduta inviando LINUM a '%s' (inviati %d/%d)", c->id, i, num_item);
            server_disconnect(srv, ctx, c->tcp_fd);
            return;
        } 
    }
    VLOG("LIST Successo: Inviata lista completa di %d client a '%s'", num_item, c->id);
}

void handle_consu(Server *srv, SelectCtx *ctx, Client *c) {
    if(srv == NULL || ctx == NULL || c == NULL) return;

    char reply[MSG_BUF_SIZE];

    if(!c->has_pending_flow){
        if(fs_is_empty(c)){
            build_nocon(reply);
            if(send_msg(c->tcp_fd, reply) < 0){
                VLOG("CONSU Fallita, rete caduta inviando NOCON a '%s'", c->id);
                server_disconnect(srv, ctx, c->tcp_fd);
            }
            return;
        }
        fs_pop(c, &c->pending_flow);
        c->has_pending_flow = true;
    }

    switch(c->pending_flow.type){
        case FLOW_MESSAGE:
            build_ssem(reply, c->pending_flow.from_id, c->pending_flow.message);
            if(send_msg(c->tcp_fd, reply) < 0){
                VLOG("CONSU Fallita, rete caduta inviando SSEM> a '%s'", c->id);
                server_disconnect(srv, ctx, c->tcp_fd);
                return;
            }
            c->has_pending_flow = false;
            break;

        case FLOW_FLOOD:
            build_oolf(reply, c->pending_flow.from_id, c->pending_flow.message);
            if(send_msg(c->tcp_fd, reply) < 0){
                VLOG("CONSU Fallita, rete caduta inviando OOLF> a '%s'", c->id);
                server_disconnect(srv, ctx, c->tcp_fd);
                return;
            }
            c->has_pending_flow = false;
            break;

        case FLOW_FRIEND_REQUEST:
            build_eirf(reply, c->pending_flow.from_id);
            if(send_msg(c->tcp_fd, reply) < 0){
                VLOG("CONSU Fallita, rete caduta inviando EIRF> a '%s'", c->id);
                server_disconnect(srv, ctx, c->tcp_fd);
                return;
            }
            // NON azzera has_pending_flow, sarà handle_friend_reply dopo ACKRF.
            c->waiting_friend_reply = true;
            strncpy(c->pending_friend_id, c->pending_flow.from_id, ID_LEN);
            c->pending_friend_id[ID_LEN]='\0';
            break;

        case FLOW_FRIEND_ACCEPT:
            build_frien(reply, c->pending_flow.from_id);
            if (send_msg(c->tcp_fd, reply) < 0) {
                VLOG("CONSU Fallita, rete caduta inviando FRIEN a '%s'", c->id);
                server_disconnect(srv, ctx, c->tcp_fd);
                return;
            }
            c->has_pending_flow = false;
            break;

        case FLOW_FRIEND_REJECT:
            build_nofri(reply, c->pending_flow.from_id);
            if (send_msg(c->tcp_fd, reply) < 0) {
                VLOG("CONSU Fallita, rete caduta inviando NOFRI a '%s'", c->id);
                server_disconnect(srv, ctx, c->tcp_fd);
                return;
            }
            c->has_pending_flow = false;
            break;

        default:
            VLOG("CONSU Fallita, tipo flusso sconosciuto %d per '%s'", c->pending_flow.type, c->id);
            c->has_pending_flow = false;
            break;
    }
    VLOG("CONSU Successo, flusso tipo %d elaborato per '%s'", c->pending_flow.type, c->id);
}

void handle_friend_reply(Server *srv, SelectCtx *ctx, Client *c, const char *type){
    if (srv == NULL || ctx == NULL || c == NULL || type == NULL) return;

    char reply[MSG_BUF_SIZE];

    Client *requester = cs_find_by_id(srv, c->pending_friend_id);

    if(strcmp(type, "OKIRF") == 0){
        if (requester != NULL) {
            int idx_c = cs_index_of(srv, c->id);
            int idx_req = cs_index_of(srv, c->pending_friend_id);
            cs_add_friendship(srv, idx_c, idx_req);
        
            if (fs_push(requester, FLOW_FRIEND_ACCEPT, c->id, NULL) == 0) {
                send_udp_notify(requester->ip, requester->udp_port, FLOW_FRIEND_ACCEPT, requester->flow_count);
            }else{
                VLOG("FRIEND_REPLY Avviso: coda di '%s' piena, notifica ACCETTAZIONE scartata", requester->id);
            }
            VLOG("FRIEND_REPLY: '%s' ha ACCETTATO l'amicizia di '%s'", c->id, c->pending_friend_id);
        }else{
            VLOG("FRIEND_REPLY Errore: Il richiedente '%s' non è stato trovato", c->pending_friend_id);
        }
    }else{
        if (requester != NULL) {
            if (fs_push(requester, FLOW_FRIEND_REJECT, c->id, NULL) == 0) {
                send_udp_notify(requester->ip, requester->udp_port, FLOW_FRIEND_REJECT, requester->flow_count);
            }else{
                VLOG("FRIEND_REPLY Avviso: coda di '%s' piena, notifica RIFIUTO scartata", requester->id);
            }
        }
        VLOG("FRIEND_REPLY: '%s' ha RIFIUTATO l'amicizia di '%s'", c->id, c->pending_friend_id);
    }
    c->waiting_friend_reply = false;
    c->pending_friend_id[0] = '\0';
    c->has_pending_flow = false;

    build_ackrf(reply);
    if (send_msg(c->tcp_fd, reply) < 0) {
        VLOG("FRIEND_REPLY fallita, rete caduta inviando ACKRF a '%s'", c->id);
        server_disconnect(srv, ctx, c->tcp_fd);
    }
}

void handle_quit(Server *srv, SelectCtx *ctx, Client *c) {
    if(srv == NULL || ctx == NULL || c == NULL) return;

    char reply[MSG_BUF_SIZE];
    VLOG("QUIT Richiesta da client '%s'", c->id);
    build_gobye(reply);
    send_msg(c->tcp_fd, reply);
    server_disconnect(srv, ctx, c->tcp_fd);
    VLOG("QUIT Successo, client '%s' disconnesso", c->id);
}

