#define _POSIX_C_SOURCE 200809L

#include "server.h"
#include "handlers.h"
#include "client_store.h"

#include "../common/net_utils.h"
#include "../common/message.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <signal.h>

/* ─── Variabili globale e sigint ─── */
static Server *g_srv = NULL;
static SelectCtx *g_ctx = NULL;

static void handle_sigint(int sig){
	printf("\n[!] Segnale di terminazione (%d). Spegnimento...\n", sig);
	if(g_srv != NULL && g_ctx != NULL) server_shutdown(g_srv, g_ctx);
	exit(EXIT_SUCCESS);
}

/* ─── Ciclo di vita ─── */

int server_init(Server *srv, SelectCtx *ctx, uint16_t port){
	if(srv == NULL || ctx == NULL)return -1;
	memset(srv, 0, sizeof(Server));

	for(int i = 0; i < MAX_CLIENTS; i++) srv->clients[i].tcp_fd = -1;
	srv->listen_fd = open_tcp_listen(port);
	if(srv->listen_fd < 0) return -1;

	ctx->srv = srv;
	FD_ZERO(&ctx->master_fds);
	FD_ZERO(&ctx->read_fds);
	FD_SET(srv->listen_fd, &ctx->master_fds);
	ctx->fdmax = srv->listen_fd;
	return 0;
}

void server_run(Server *srv, SelectCtx *ctx){
	VLOG("AVVIO");

	while(true){
		ctx->read_fds = ctx->master_fds;
		if(select(ctx->fdmax + 1, &ctx->read_fds, NULL, NULL, NULL) < 0){
			if (errno == EINTR) continue;
            perror("Errore select");
            break;
		}
		for(int i = 0; i <= ctx->fdmax; i++){
			if(FD_ISSET(i, &ctx->read_fds)){
				if(i == srv->listen_fd){
					server_accept(srv, ctx);
				}else{
					server_dispatch(srv, ctx, i);
				}
			}
		}
	}
}

void server_shutdown(Server *srv, SelectCtx *ctx){
	VLOG("SPEGNIMENTO");
	for(int i = 0; i <=ctx->fdmax; i++){
		if(FD_ISSET(i, &ctx->master_fds)) close(i);
	}
	FD_ZERO(&ctx->master_fds);
	VLOG("Chiusura completata.");
}

/* ─── Connessioni ─── */

int server_accept(Server *srv, SelectCtx *ctx){
	struct sockaddr_in6 client_addr;
	socklen_t addr_len = sizeof(client_addr);

	int new_fd = accept(srv->listen_fd, (struct sockaddr*)&client_addr, &addr_len);
	if (new_fd < 0) {
        VLOG("Errore accept: %s", strerror(errno));
        return -1;
    }

	if (new_fd >= FD_SETSIZE) {
        VLOG("Raggiunto limite massimo di file descriptor (FD_SETSIZE). Connessione rifiutata.");
        close(new_fd);
        return -1;
    }

	inet_ntop(AF_INET6, &client_addr.sin6_addr, srv->pending_ip[new_fd], INET6_ADDRSTRLEN);
	FD_SET(new_fd, &ctx->master_fds);
	if(new_fd > ctx->fdmax) ctx->fdmax = new_fd;

	VLOG("Nuova connessione su fd %d da IP %s", new_fd, srv->pending_ip[new_fd]);
    return new_fd;
}

void server_disconnect(Server *srv, SelectCtx *ctx, int fd){
	if(fd < 0) return;

	Client *c = cs_find_by_fd(srv, fd);
	if(c != NULL){
		cs_set_offline(c);
		VLOG("Client '%s' (fd %d) disconnesso", c->id, fd);
	}else{
		VLOG("Connessione pre auth fd %d chiusa", fd);
	}

	close(fd);
	FD_CLR(fd, &ctx->master_fds);
	srv->pending_ip[fd][0] = '\0';
}

/* ─── Dispatch ─── */

void server_dispatch(Server *srv, SelectCtx *ctx, int fd){
	char buf[MSG_BUF_SIZE + 1];
	int nbytes = recv_message(fd, buf, MSG_BUF_SIZE + 1);
	if (nbytes <= 0) {
        server_disconnect(srv, ctx, fd);
        return;
    }

	char type[TYPE_LEN + 1];
	parse_type(buf, type);

	Client *c = cs_find_by_fd(srv, fd);
	if(c == NULL){
		if(strncmp(type, "REGIS", 5) == 0){
			handle_regis(srv, ctx, fd, buf);
		}else if(strncmp(type, "CONNE", 5) == 0){
			handle_conne(srv, ctx, fd, buf);
		}else{
			VLOG("Comando pre auth errato su fd %d: '%s'", fd, type);
			char reply[MSG_BUF_SIZE];
			build_gobye(reply);
			send_msg(fd, reply);
			server_disconnect(srv, ctx, fd);
		}
	}else{
		if(c->waiting_friend_reply){
			handle_friend_reply(srv, ctx, c, type);
			return;
		}

		if (strncmp(type, "FRIE?", 5) == 0) {
            handle_frie(srv, ctx, c, buf);
        } else if (strncmp(type, "MESS?", 5) == 0) {
            handle_mess(srv, ctx, c, buf);
        } else if (strncmp(type, "FLOO?", 5) == 0) {
            handle_floo(srv, ctx, c, buf);
        } else if (strncmp(type, "LIST?", 5) == 0) {
            handle_list(srv, ctx, c);
        } else if (strncmp(type, "CONSU", 5) == 0) {
            handle_consu(srv, ctx, c);
        } else if (strncmp(type, "IQUIT", 5) == 0) {
            handle_quit(srv, ctx, c);
        } else {
			VLOG("Comando sconosciuto da '%s': '%s'", c->id, type);
			char reply[MSG_BUF_SIZE];
			build_gobye(reply);
			send_msg(fd, reply);
			server_disconnect(srv, ctx, fd);
		}
	}
}

/* ─── Main ─── */

int main(int argc, char *argv[]){
	if(argc < 2 || argc > 3){
		fprintf(stderr, "Uso: %s <porta> [-v]\n", argv[0]);
		return EXIT_FAILURE;
	}
	if(argc == 3 && strncmp(argv[2], "-v", 2) == 0) verbose_mode = 1;

	char *endptr;
	long p = strtol(argv[1], &endptr, 10);
	if(*endptr != '\0' || p <= 0 || p>= 9999){
		fprintf(stderr, "Errore: Porta non valida (1-9999)\n");
        return EXIT_FAILURE;
	}
	uint16_t port = (uint16_t)p;
	Server srv;
	SelectCtx ctx;
	g_srv = &srv;
	g_ctx = &ctx;

	signal(SIGINT, handle_sigint);
 	signal(SIGTERM, handle_sigint);
	signal(SIGPIPE, SIG_IGN);

	if(server_init(&srv, &ctx, port) < 0){
		fprintf(stderr, "Errore avvio server.\n");
        return EXIT_FAILURE;
	}

	server_run(&srv, &ctx);
	server_shutdown(&srv, &ctx);
	return EXIT_SUCCESS;
}
