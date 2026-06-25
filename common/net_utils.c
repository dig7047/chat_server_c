#include "net_utils.h"
#include "message.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <ctype.h>

int verbose_mode = 0;

/* ─── Invio affidabile ─── */

int send_all(int fd, const char *buf, int len){
	int tot_byte_sent = 0, bytes_left = len, n;
	while(tot_byte_sent < len){
		n = send(fd, buf+tot_byte_sent, bytes_left, 0); /* da dove arrivati, quelli mancanti */
		if(n == -1) return -1;
		tot_byte_sent += n;
		bytes_left -= n;
	}
	return 0;
}

int send_msg(int fd, const char *msg){
	if(msg == NULL) return -1;
	int len = strlen(msg);
	VLOG("Inviato su fd %d: %s", fd, msg);
	return send_all(fd, msg, len);
}

/* ─── Notifiche UDP ─── */

void send_udp_notify(const char *ip, uint16_t udp_port, FlowType flow_code, int unchecked){
	if(ip == NULL) return;

	int sockfd;
	struct sockaddr_storage dest_addr;
	socklen_t addr_len;
	memset(&dest_addr, 0, sizeof(dest_addr));

	if(strchr(ip, ':') != NULL){ 	/* Logica IPv6 */
		struct sockaddr_in6 *addr6 = (struct  sockaddr_in6 *)&dest_addr;
		addr6->sin6_family = AF_INET6;
		addr6->sin6_port = htons(udp_port);
		
		if(inet_pton(AF_INET6, ip, &addr6->sin6_addr) <= 0){
			VLOG("Errore formato IPv6 (%s)", ip);
			return;
		}
		addr_len = sizeof(struct sockaddr_in6);
		sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
	}else{							/* Logica IPv4 */
		struct sockaddr_in *addr4 = (struct  sockaddr_in *)&dest_addr;
		addr4->sin_family = AF_INET;
		addr4->sin_port = htons(udp_port);

		if(inet_pton(AF_INET, ip, &addr4->sin_addr) <= 0){
			VLOG("Errore formato IPv4 (%s)", ip);
			return;
		}
		addr_len = sizeof(struct sockaddr_in);
		sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	}

	if(sockfd < 0){
		VLOG("Errore creazione socket UDP temporaneo verso %s. Motivo %s", ip, strerror(errno));
		return;
	}
	char buf[4];
	build_udp_notify(buf, flow_code, unchecked);
	ssize_t bytes_sent = sendto(sockfd, buf, 3, 0, (struct sockaddr*)&dest_addr, addr_len);
	if(bytes_sent == 3){
		VLOG("Notifica UDP [%s] inviata a %s:%u", buf, ip, udp_port);
	}else{
		VLOG("Errore invio notifica UDP [%s] a %s:%u. Motivo %s", buf, ip, udp_port, strerror(errno));
	}
	close(sockfd);
}

/* ─── Apertura socket ─── */

int open_tcp_listen(uint16_t port){
	int sockfd;
	struct sockaddr_in6 server_addr;
	int opt = 1;

	sockfd = socket(AF_INET6, SOCK_STREAM, 0);
	if(sockfd < 0){
		VLOG("Errore creazione socket TCP per la porta %u. Motivo %s", port, strerror(errno));
		return -1;
	}

	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
		VLOG("Errore SO_REUSEADDR su fd %d (porta %u). Motivo %s", sockfd, port, strerror(errno));
		close(sockfd);
		return -1;
	}

	int no = 0, dual_stack_ok = 1;
	if(setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no)) < 0){
		VLOG("Avviso non abilitato Dual-Stack su fd %d (porta %u). Motivo %s", sockfd, port, strerror(errno));
		dual_stack_ok = 0;
	}

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin6_family = AF_INET6;
	server_addr.sin6_addr = in6addr_any;
	server_addr.sin6_port = htons(port);

	if(bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
		VLOG("Errore bind TCP su fd %d (porta %u). Motivo %s", sockfd, port, strerror(errno));
		close(sockfd);
		return -1;
	}

	if(listen(sockfd, SOMAXCONN) < 0){
		VLOG("Errore listen TCP su fd %d (porta %u). Motivo %s", sockfd, port, strerror(errno));
		close(sockfd);
		return -1;
	}

	VLOG("Socket TCP in ascolto su fd %d (porta %u) - Supporto %s", sockfd, port, dual_stack_ok ? "IPv4/IPv6" : "SOLO IPv6");
	return sockfd;
}

int open_udp_listen(uint16_t port){
	int sockfd;
	struct  sockaddr_in6 server_addr;
	
	sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
	if(sockfd < 0){
		VLOG("Errore creazione socket UDP per la porta %u. Motivo %s", port, strerror(errno));
		return -1;
	}

	int no = 0, dual_stack_ok = 1;
	if(setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no)) < 0){
		VLOG("Avviso non abilitato Dual-Stack su fd %d (porta %u). Motivo %s", sockfd, port, strerror(errno));
		dual_stack_ok = 0;
	}

	memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_addr = in6addr_any;
    server_addr.sin6_port = htons(port);

	if(bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
		VLOG("Errore bind UDP su fd %d (porta %u). Motivo %s", sockfd, port, strerror(errno));
		close(sockfd);
		return -1;
	}

	VLOG("Socket UDP in ascolto su fd %d (porta %u) - Supporto %s", sockfd, port, dual_stack_ok ? "IPv4/IPv6" : "SOLO IPv6");
	return sockfd;
}

/* ─── Utilita' ─── */

int is_valid_id(const char *s){
	if(s == NULL) return 0;
	if(strlen(s) != ID_LEN) return 0;
	for(int i = 0; i < ID_LEN; i++){
		if(!isalnum((unsigned char)s[i])) return 0;
	}

	return 1;
}

int is_valid_mess(const char *mess){
	if(mess == NULL) return 0;
	if(strlen(mess) > MESS_MAX) return 0;
	if(strstr(mess, "+++") != NULL) return 0;

	return 1;
}
