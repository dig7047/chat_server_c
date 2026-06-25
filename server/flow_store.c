#include "flow_store.h"
#include <string.h>

/* ─── Aggiunta ─── */

int fs_push(Client *c, FlowType type, const char *from_id, const char *message){
	if(c == NULL || from_id == NULL || c->flow_count >= MAX_FLOWS) return -1;
	if((type == FLOW_MESSAGE || type == FLOW_FLOOD) && message == NULL) return -1;

	Flow *new_f = &c->flows[c->flow_count];

	new_f->type=type;

	strncpy(new_f->from_id, from_id, ID_LEN);
	new_f->from_id[ID_LEN] = '\0';

	if(message != NULL){
		strncpy(new_f->message, message, MESS_MAX);
		new_f->message[MESS_MAX] = '\0';
	}else{
		new_f->message[0] = '\0';
	}
	
	c->flow_count++;
	return 0;
}

/* ─── Consumo ─── */

int fs_pop(Client *c, Flow *out){
	if(c == NULL || out == NULL) return -1;
	if(c->flow_count == 0) return 0;

	*out = c->flows[0];
	memmove(&c->flows[0], &c->flows[1], sizeof(Flow) * (c->flow_count - 1));
	c->flow_count--;
	return 1;
}

/* ─── Ispezione ─── */

int fs_is_empty(const Client *c){
	if(c == NULL) return -1;
	return c->flow_count <= 0;
}

