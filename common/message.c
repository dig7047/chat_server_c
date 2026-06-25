#include "message.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>


/* ─── Ricezione ─── */

int recv_message(int fd, char *buf, int bufsize){
	if(buf == NULL || bufsize <=0) return -1;

	int bytes_r = 0;
	int consec_plus = 0;
	char c;

	while(bytes_r < bufsize-1){
		ssize_t n = recv(fd, &c, 1, 0);
		if(n<=0) return -1; 
		buf[bytes_r++] = c;

		if (c == '+'){
			consec_plus++;
			if(consec_plus == 3) break;
		}else{
			consec_plus = 0;
		}
	}
	buf[bytes_r] = '\0';
	return bytes_r;
}

/* utility */
void extract(const char *src, int offset, int max_len, const char *delim, char *dest){
	if(src == NULL || dest == NULL || max_len <= 0) return;

	int src_len = strlen(src);
    if (offset >= src_len) { 
        dest[0] = '\0'; 
        return;
    }

	const char *start = src + offset;
	int available = src_len - offset;
	int copy_len = (max_len < available) ? max_len : available;
	
	if(delim != NULL){
		const char *pos_delim = strstr(start, delim);
		if(pos_delim != NULL){                             /* se delim non trovato copy_len rimane max_len */
			copy_len = pos_delim - start;
			if(copy_len > max_len) copy_len = max_len;
		}
	}
	strncpy(dest, start, copy_len);
	dest[copy_len]='\0';
}

void parse_type(const char *msg, char *type){
	extract(msg, 0, TYPE_LEN, NULL, type);
}

void parse_id(const char *msg, int offset, char *id_out){
	extract(msg, offset, ID_LEN, NULL, id_out);
}

void parse_port(const char *msg, int offset, char *port_out){
    extract(msg, offset, PORT_STR_LEN, NULL, port_out);
}

void parse_mess(const char *msg, int offset, char *mess_out){
	extract(msg, offset, MESS_MAX, MSG_TERMINATOR, mess_out);
}

/* ─── TCP (server → client) ─── */

void build_welco(char *buf){ strcpy(buf, "WELCO+++"); }     
void build_hello(char *buf){ strcpy(buf, "HELLO+++"); }     
void build_gobye(char *buf){ strcpy(buf, "GOBYE+++"); }     
void build_ackrf(char *buf){ strcpy(buf, "ACKRF+++"); }     
void build_nocon(char *buf){ strcpy(buf, "NOCON+++"); }     
void build_frie_ok(char *buf){ strcpy(buf, "FRIE>+++"); }   
void build_frie_err(char *buf){ strcpy(buf, "FRIE<+++"); }  
void build_mess_ok(char *buf){ strcpy(buf, "MESS>+++"); }   
void build_mess_err(char *buf){ strcpy(buf, "MESS<+++"); }  
void build_floo_ok(char *buf){ strcpy(buf, "FLOO>+++"); }   

void build_rlist(char *buf, int num_items){ sprintf(buf, "RLIST %03d+++", num_items); }          			
void build_linum(char *buf, const char *id){ sprintf(buf, "LINUM %s+++", id); }           		
void build_eirf(char *buf, const char *id){ sprintf(buf, "EIRF> %s+++", id); }            		
void build_frien(char *buf, const char *id){ sprintf(buf, "FRIEN %s+++", id); }           		
void build_nofri(char *buf, const char *id){ sprintf(buf, "NOFRI %s+++", id); }           	
void build_ssem(char *buf, const char *id,const char *mess){ sprintf(buf, "SSEM> %s %s+++", id, mess); }  
void build_oolf(char *buf, const char *id,const char *mess){ sprintf(buf, "OOLF> %s %s+++", id, mess); } 

/* ─── TCP (client → server) ─── */

void build_regis(char *buf, const char *id, uint16_t port, uint16_t mdp){
	int offset = sprintf(buf, "REGIS %s %04u ", id, port);
	buf[offset] = mdp & 0xFF;           /* isolo il byte destro */
	buf[offset+1] = (mdp>>8) & 0xFF;	/* shift byte sinistro a destra e isolo */
	offset += 2;
	memcpy(buf+offset, "+++", 4);
}

void build_conne(char *buf, const char *id, uint16_t mdp){
	int offset = sprintf(buf, "CONNE %s ", id);
	buf[offset] = mdp & 0xFF;           
	buf[offset+1] = (mdp>>8) & 0xFF;	
	offset += 2;
	memcpy(buf+offset, "+++", 4);
}

void build_frie_req(char *buf, const char *id){ sprintf(buf, "FRIE? %s+++", id); }      							
void build_mess_req(char *buf, const char *id,const char *mess){ sprintf(buf, "MESS? %s %s+++", id, mess); }       
void build_floo_req(char *buf, const char *mess){ sprintf(buf, "FLOO? %s+++", mess); }     							
void build_list_req(char *buf){ strcpy(buf, "LIST?+++"); }                      							
void build_consu_req(char *buf){ strcpy(buf, "CONSU+++"); }                     							
void build_iquit(char *buf){ strcpy(buf, "IQUIT+++"); }                         							
void build_okirf(char *buf){ strcpy(buf, "OKIRF+++"); }                   								
void build_nokrf(char *buf){ strcpy(buf, "NOKRF+++"); }                     							

/* ─── Notifiche UDP ─── */

void build_udp_notify(char *buf, FlowType flow_code, int unchecked){ 
	int val = unchecked & 0xFF;
	int low = val & 0x0F;
	int high = (val >> 4) & 0x0F;
	sprintf(buf, "%d%X%X", flow_code, low, high); 
}
