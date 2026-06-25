#include "client_store.h"
#include <string.h>


/* ─── Aggiunta / rimozione ─── */

Client *cs_add(Server *srv, const char *id, uint16_t password, const char *ip, uint16_t udp_port, int fd){

	if(srv == NULL || id == NULL || ip==NULL || srv->count >= MAX_CLIENTS) return NULL;
	
	for(int i = 0; i < srv->count; i++){
		if(strncmp(srv->clients[i].id, id, ID_LEN) == 0) return NULL;
	}
	
	Client *new_cl = &srv->clients[srv->count];

	strncpy(new_cl->id, id, ID_LEN);
	new_cl->id[ID_LEN] = '\0';

	strncpy(new_cl->ip, ip, INET6_ADDRSTRLEN - 1);
	new_cl->ip[INET6_ADDRSTRLEN - 1] = '\0';

	new_cl->password = password;
	new_cl->udp_port = udp_port;
	new_cl->tcp_fd = fd;
	new_cl->friend_count = 0;
	new_cl->flow_count = 0;
	srv->count++;

	return new_cl;

}

void cs_set_offline(Client *c){
	if (c == NULL) return;
	c->tcp_fd = -1;
}

void cs_set_online(Client *c, int fd, const char *ip){
	if (c == NULL) return;
	c->tcp_fd = fd;
	
	strncpy(c->ip, ip, INET6_ADDRSTRLEN - 1);
	c->ip[INET6_ADDRSTRLEN - 1] = '\0';

	// c->has_pending_flow non toccato. Rimane 1 se flusso interrotto
	c->waiting_friend_reply = false;
	c->pending_friend_id[0] = '\0';
}

/* ─── Ricerca ─── */

Client *cs_find_by_id(Server *srv, const char *id){
	if(srv == NULL || id == NULL) return NULL;

	for(int i = 0; i < srv->count; i++){
		if(strncmp(srv->clients[i].id, id, ID_LEN) == 0) return &srv->clients[i];
	}
	return NULL;
}

Client *cs_find_by_fd(Server *srv, int fd){
	if(srv == NULL || fd < 0) return NULL;

	for(int i = 0; i < srv->count; i++){
		if(srv->clients[i].tcp_fd == fd) return &srv->clients[i];
	}
	return NULL;
}

int cs_index_of(Server *srv, const char *id){
	if(srv == NULL || id == NULL) return -1;

	for(int i = 0; i < srv->count; i++){
		if(strncmp(srv->clients[i].id, id, ID_LEN) == 0) return i;
	}
	return -1;
}

/* ─── Amicizie ─── */

int cs_add_friendship(Server *srv, int idx_a, int idx_b){
	if(srv == NULL || idx_a < 0 || idx_b < 0 || idx_a >= srv->count || idx_b >= srv->count) return 2;

	Client *a=&srv->clients[idx_a];
	Client *b=&srv->clients[idx_b];

	if(a->friend_count == MAX_CLIENTS || b->friend_count == MAX_CLIENTS) return -1;

	for(int i = 0; i < a->friend_count; i++){
		if(a->friends[i] == idx_b) return 1;
	}

	a->friends[a->friend_count++] = idx_b;
	b->friends[b->friend_count++] = idx_a;
	return 0;
}

int cs_are_friends(Server *srv, int idx_a, int idx_b){
	if(srv == NULL || idx_a < 0 || idx_b < 0 || idx_a >= srv->count || idx_b >= srv->count) return 2;

	Client *a=&srv->clients[idx_a];

	for(int i = 0; i < a->friend_count; i++){
		if(a->friends[i] == idx_b) return 1;
	}
	return 0;
}

int cs_get_friend_indices(const Client *c, int *out_indices, int max_out){
	if(c == NULL || out_indices == NULL || max_out < 1) return -1;

	int lim = (c->friend_count < max_out) ? c->friend_count : max_out;

	for(int i = 0; i < lim; i++){
		out_indices[i] = c->friends[i];
	}
	return lim;
}

/* ─── Utilita' ─── */

int cs_id_exists(Server *srv, const char *id){
	if(srv == NULL || id == NULL) return -1;
	return (cs_find_by_id(srv, id) != NULL);
}

int cs_is_full(const Server *srv){
	if(srv == NULL) return -1;
	return srv->count >= MAX_CLIENTS;
}
