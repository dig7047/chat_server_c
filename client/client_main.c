#define _POSIX_C_SOURCE 200809L

#include "client.h"
#include "../common/protocol.h"
#include "../common/message.h"
#include "../common/net_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>

/* ─── Variabili globale e sigint ─── */

static ClientState *g_cs = NULL;
static char g_server_ip[INET6_ADDRSTRLEN];
static uint16_t g_server_port;

static void handle_sigint(int sig){
    printf("\n[!] Uscita in corso (segnale %d)...\n", sig);
   	if(g_cs != NULL) client_shutdown(g_cs);
    exit(EXIT_SUCCESS);
}

/* ─── Menu ─── */

static void print_help(){
    printf("\n==========================================================\n");
    printf("                  IPortbook - Client CLI                    \n");
    printf("==========================================================\n");
    printf(" COMANDI PRE-AUTENTICAZIONE:\n");
    printf("  regis <id> <password>\n");
    printf("  conne <id> <password>\n");
    printf("  quit  (chiude il client)\n");
    printf("\n COMANDI POST-AUTENTICAZIONE:\n");
    printf("  frie <id_amico>\n");
    printf("  mess <id_amico> <testo_messaggio>\n");
    printf("  floo <testo_messaggio>\n");
    printf("  list\n");
    printf("  consu\n");
    printf("  quit  (disconnette, ma mantiene aperto il client)\n");
    printf("==========================================================\n");
    printf("\nRegole formali:\n");
    printf(" - ID: Esattamente 8 caratteri alfanumerici.\n");
    printf(" - Password: Da 0 a 65535.\n");
    printf(" - Messaggio: massimo 200 caratteri.\n");
    printf("==========================================================\n\n> ");
    fflush(stdout);
}

/* ─── Ciclo di vita ─── */

int client_init(ClientState *cs, uint16_t udp_port){
    if(cs == NULL) return -1;

	memset(cs, 0, sizeof(ClientState));
	cs->udp_port = udp_port;
	cs->tcp_fd = -1;
	cs->authenticated = 0;

	cs->udp_fd = open_udp_listen(udp_port);
	if(cs->udp_fd < 0){
		fprintf(stderr, "Errore apertura porta UDP %u.\n", udp_port);
        return -1;
	}
    VLOG("Client in ascolto su UDP %u", udp_port);
    return 0;
}

int client_connect(ClientState *cs, const char *server_ip, uint16_t server_port){
    if (cs == NULL || server_ip == NULL) return -1;

    int sockfd;
    struct sockaddr_storage dest_addr;
    socklen_t addr_len;
    memset(&dest_addr, 0, sizeof(dest_addr));

    if(strchr(server_ip, ':') != NULL){                           /* IPv6 */
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&dest_addr;
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = htons(server_port);
        
        if(inet_pton(AF_INET6, server_ip, &addr6->sin6_addr) <= 0){
            fprintf(stderr, "Errore: Indirizzo IPv6 del server non valido (%s)\n", server_ip);
            return -1;
        }
        addr_len = sizeof(struct sockaddr_in6);
        sockfd = socket(AF_INET6, SOCK_STREAM, 0);
    }else{                                                        /* IPv4 */
        struct sockaddr_in *addr4 = (struct sockaddr_in *)&dest_addr;
        addr4->sin_family = AF_INET;
        addr4->sin_port = htons(server_port);

        if(inet_pton(AF_INET, server_ip, &addr4->sin_addr) <= 0){
            fprintf(stderr, "Errore: Indirizzo IPv4 del server non valido (%s)\n", server_ip);
            return -1;
        }
        addr_len = sizeof(struct sockaddr_in);
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
    }

    if(sockfd < 0){
        perror("Errore apertura socket TCP");
        return -1;
    }

    VLOG("Connessione TCP a %s:%u...", server_ip, server_port);
    if(connect(sockfd, (struct sockaddr *)&dest_addr, addr_len) < 0){
        perror("Errore connessione TCP");
        close(sockfd);
        return -1;
    }

    cs->tcp_fd = sockfd;
    VLOG("Connesso con successo al server TCP.");
    return 0;
}

void client_shutdown(ClientState *cs){
    if(cs == NULL) return;

	if(cs->tcp_fd >= 0){
		close(cs->tcp_fd);
		cs->tcp_fd = -1;
		VLOG("Socket TCP chiuso.");
	}
	if(cs->udp_fd >= 0){
		close(cs->udp_fd);
		cs->udp_fd = -1;
		VLOG("Socket UDP chiuso.");
	}
	cs->authenticated = 0;
	VLOG("Shutdown completato!");
}

/* ─── Loop principale ─── */

void client_run(ClientState *cs){
    fd_set read_fds;
    int fdmax;

    print_help();

    while (1){
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        fdmax = STDERR_FILENO;

        if(cs->tcp_fd >= 0){
            FD_SET(cs->tcp_fd, &read_fds);
            if (cs->tcp_fd > fdmax) fdmax = cs->tcp_fd;
        }
        if(cs->udp_fd >= 0){
            FD_SET(cs->udp_fd, &read_fds);
            if (cs->udp_fd > fdmax) fdmax = cs->udp_fd;
        }

        if(select(fdmax + 1, &read_fds, NULL, NULL, NULL) < 0){
            if(errno == EINTR) continue;
            perror("Errore select");
            break;
        }

        // 1. Input tastiera
        if(FD_ISSET(STDIN_FILENO, &read_fds)){
            char line[512];
            if(fgets(line, sizeof(line), stdin) == NULL) break;
            line[strcspn(line, "\n")] = 0;

            if(strlen(line) > 0){
                if(client_handle_input(cs, line) < 0) break;
            }

            printf("> ");
            fflush(stdout);
        }

        // 2. Risposte server(TCP)
        if(cs->tcp_fd >= 0 && FD_ISSET(cs->tcp_fd, &read_fds)){
            if (client_handle_tcp(cs) < 0) break;
            printf("> ");
            fflush(stdout);
        }

        // 3. Notifiche (UDP)
        if(cs->udp_fd >= 0 && FD_ISSET(cs->udp_fd, &read_fds)){
            client_handle_udp(cs);
        }
    }
}

/* ─── Comandi ─── */

int client_handle_input(ClientState *cs, const char *line){
    char cmd[7] = {0}; 
    if(sscanf(line, "%6s", cmd) != 1) return 0; // legge massimo 6, tutti i comandi massimo 5

    if(strlen(cmd) > 5){
        printf("[!] Comando sconosciuto.\n");
        print_help();
        return 0;
    }

    if(!cs->authenticated){
        if(strcmp(cmd, "regis") != 0 && strcmp(cmd, "conne") != 0 && strcmp(cmd, "quit") != 0){
            printf("[!] Errore: non sei ancora autenticato.\n");
            return 0; 
        }
    }else{
        if(strcmp(cmd, "regis") == 0 || strcmp(cmd, "conne") == 0){
            printf("[!] Errore: sei gia' autenticato.\n");
            return 0; 
        }
    }

    char msg[MSG_BUF_SIZE];

    if(strcmp(cmd, "regis") == 0 || strcmp(cmd, "conne") == 0){
        char arg_id[10] = {0};
        char arg_pwd_str[7] = {0};
        if(sscanf(line, "%*s %9s %6s", arg_id, arg_pwd_str) != 2){
            printf("Uso corretto: %s <id> <password>\n", cmd);
            return 0;
        }
        if(!is_valid_id(arg_id)){
            printf("[!] Errore: ID malformato, %d caratteri alfanumerici.\n", ID_LEN);
            return 0;
        }
        char *endptr;
        long p = strtol(arg_pwd_str, &endptr, 10);
        if(*endptr != '\0' || p < 0 || p > 65535){
            printf("[!] Errore: password fuori range (deve essere un numero 0-65535).\n");
            return 0;
        }
        if(cs->tcp_fd < 0){
            if (client_connect(cs, g_server_ip, g_server_port) < 0) {
                printf("[!] Errore connessione server TCP.\n");
                return 0;
            }
        }

        strncpy(cs->id, arg_id, ID_LEN);
        cs->id[ID_LEN] = '\0';
        cs->password = (uint16_t)p;
        if(strcmp(cmd, "regis") == 0){
            build_regis(msg, cs->id, cs->udp_port, cs->password);
            if (send_all(cs->tcp_fd, msg, 25) < 0) return -1;
        }else{
            build_conne(msg, cs->id, cs->password);
            if (send_all(cs->tcp_fd, msg, 20) < 0) return -1;
        }
    } 
    else if(strcmp(cmd, "frie") == 0){
        char arg_id[10] = {0};
        if(sscanf(line, "%*s %9s", arg_id) != 1){
            printf("Uso corretto: frie <id_amico>\n");
            return 0;
        }
        if(!is_valid_id(arg_id)){
            printf("[!] Errore: ID malformato, %d caratteri alfanumerici.\n", ID_LEN);
            return 0;
        }
        build_frie_req(msg, arg_id);
        send_msg(cs->tcp_fd, msg);
    } 
    else if(strcmp(cmd, "mess") == 0){
        char arg_id[10] = {0};
        if(sscanf(line, "%*s %9s", arg_id) != 1){
            printf("Uso corretto: mess <id_amico> <testo>\n");
            return 0;
        }
        if(!is_valid_id(arg_id)){
            printf("[!] Errore: ID malformato, %d caratteri alfanumerici.\n", ID_LEN);
            return 0;
        }

        char *text_start = strstr(line, arg_id) + strlen(arg_id);     //id_ptr + id_len
        while (*text_start == ' ') text_start++;                      //toglie " "
        if(strlen(text_start) == 0 || !is_valid_mess(text_start)){
            printf("[!] Errore: messaggio invalido (1-%d caratteri, no '+++').\n", MESS_MAX);
            return 0;
        }
        build_mess_req(msg, arg_id, text_start);
        send_msg(cs->tcp_fd, msg);
    } 
    else if(strcmp(cmd, "floo") == 0){
        char *text_start = strstr(line, cmd) + strlen(cmd);
        while (*text_start == ' ') text_start++;
        if (strlen(text_start) == 0 || !is_valid_mess(text_start)) {
            printf("[!] Errore: messaggio invalido (1-%d caratteri, no '+++').\n", MESS_MAX);
            return 0;
        }
        build_floo_req(msg, text_start);
        send_msg(cs->tcp_fd, msg);
    } 
    else if(strcmp(cmd, "list") == 0){
        build_list_req(msg);
        send_msg(cs->tcp_fd, msg);
    } 
    else if(strcmp(cmd, "consu") == 0){
        build_consu_req(msg);
        send_msg(cs->tcp_fd, msg);
    } 
    else if(strcmp(cmd, "quit") == 0){
        if (cs->authenticated && cs->tcp_fd >= 0) {   //se auth IQUIT
            build_iquit(msg);
            send_msg(cs->tcp_fd, msg);
        } else {                                      //se !auth chiude client
            return -1; 
        }
    } 
    else{
        printf("Comando sconosciuto.\n");
    }
    return 0;
}

/* ─── Risposte TCP ─── */

int client_handle_tcp(ClientState *cs){
    char buf[MSG_BUF_SIZE + 1];
    
    int nbytes = recv_message(cs->tcp_fd, buf, MSG_BUF_SIZE + 1);
    if(nbytes <= 0){
        printf("\n[Errore] Connessione TCP interrotta!\n");
        close(cs->tcp_fd);
        cs->tcp_fd = -1;
        cs->authenticated = 0;
        return 0; 
    }
    char type[TYPE_LEN + 1];
    parse_type(buf, type);

    if(strncmp(type, "WELCO", 5) == 0){
        printf("[Server] Registrazione effettuata! Benvenuto %s.\n", cs->id);
        cs->authenticated = 1;
    } 
    else if(strncmp(type, "HELLO", 5) == 0){
        printf("[Server] Login effettuato! Bentornato %s.\n", cs->id);
        cs->authenticated = 1;
    } 
    else if(strncmp(type, "GOBYE", 5) == 0){
        printf("[Server] Disconnessione confermata.\n");
        close(cs->tcp_fd);
        cs->tcp_fd = -1;
        cs->authenticated = 0;
        return 0;
    } 
    else if(strncmp(type, "FRIE>", 5) == 0){
        printf("[Server] Richiesta di amicizia inoltrata con successo.\n");
    } 
    else if(strncmp(type, "FRIE<", 5) == 0){
        printf("\n[Errore] Impossibile inviare la richiesta (utente inesistente o siete gia' amici).\n");
    } 
    else if(strncmp(type, "MESS>", 5) == 0){
        printf("[Server] Messaggio inviato con successo.\n");
    } 
    else if(strncmp(type, "MESS<", 5) == 0){
        printf("\n[Errore] Impossibile inviare il messaggio.\n");
    } 
    else if(strncmp(type, "FLOO>", 5) == 0){
        printf("[Server] Messaggio di flooding inviato con successo.\n");
    } 
    else if(strncmp(type, "RLIST", 5) == 0){
        char num_buf[4];
        extract(buf, 6, 3, "+++", num_buf);
        int num_items = (int)strtol(num_buf, NULL, 10);
        printf("\n--- Utenti Registrati (%d) ---\n", num_items);
        
        for(int i = 0; i < num_items; i++){
            char linum_buf[MSG_BUF_SIZE + 1];
            int n = recv_message(cs->tcp_fd, linum_buf, MSG_BUF_SIZE + 1);
            if(n <= 0){
                printf("[!] Connessione interrotta durante il download della lista.\n");
                close(cs->tcp_fd);
                cs->tcp_fd = -1;
                cs->authenticated = 0;
                return 0;
            }
            char t_type[TYPE_LEN + 1];
            parse_type(linum_buf, t_type);
            if(strncmp(t_type, "LINUM", 5) == 0){
                char id_buf[ID_LEN + 1];
                parse_id(linum_buf, 6, id_buf);
                printf("- %s\n", id_buf);
            }else{
                printf("[!] Errore di protocollo: atteso LINUM, ricevuto: %s\n", linum_buf);
            }
        }
        printf("------------------------------\n");
    } 
    else if(strncmp(type, "SSEM>", 5) == 0){
        char id_buf[ID_LEN + 1];
        char mess_buf[MESS_MAX + 1];
        parse_id(buf, 6, id_buf);
        parse_mess(buf, 15, mess_buf); 
        printf("[Messaggio - da %s]: %s\n", id_buf, mess_buf);
    } 
    else if(strncmp(type, "OOLF>", 5) == 0){
        char id_buf[ID_LEN + 1];
        char mess_buf[MESS_MAX + 1];
        parse_id(buf, 6, id_buf);
        parse_mess(buf, 15, mess_buf); 
        printf("[Flooding - da %s]: %s\n", id_buf, mess_buf);
    } 
    else if(strncmp(type, "EIRF>", 5) == 0){
        char id_buf[ID_LEN + 1];
        parse_id(buf, 6, id_buf);
        printf("[Amicizia] L'utente '%s' ha richiesto l'amicizia.\n", id_buf);
        printf("Vuoi accettare? [s/n]: ");
        fflush(stdout);
        char ans[16];
        if (fgets(ans, sizeof(ans), stdin) != NULL) {
            char reply[MSG_BUF_SIZE];
            if (ans[0] == 's' || ans[0] == 'S') {
                build_okirf(reply); 
                printf("Amicizia di %s accettata.\n", id_buf);
            } else {
                build_nokrf(reply);
                printf("Amicizia di %s rifiutata.\n", id_buf);
            }
            send_msg(cs->tcp_fd, reply);
        }
    } 
    else if(strncmp(type, "ACKRF", 5) == 0){
        printf("[Server] Risposta alla richiesta di amicizia consegnata.\n");
    } 
    else if(strncmp(type, "FRIEN", 5) == 0){
        char id_buf[ID_LEN + 1];
        parse_id(buf, 6, id_buf);
        printf("[Server] %s ha accettato la tua richiesta di amicizia!\n", id_buf);
    } 
    else if(strncmp(type, "NOFRI", 5) == 0){
        char id_buf[ID_LEN + 1];
        parse_id(buf, 6, id_buf);
        printf("[Errore] %s ha rifiutato la richiesta o impossibile completare l'operazione.\n", id_buf);
    } 
    else if(strncmp(type, "NOCON", 5) == 0){
        printf("[Server] Non ci sono piu' flussi da consultare.\n");
    } 
    else{
        printf("[Errore] Messaggio non riconosciuto o malformato: %s\n", buf);
    }
    return 0;
}

/* ─── Notifiche UDP ─── */

void client_handle_udp(ClientState *cs){
    char buf[4];
	struct sockaddr_in6 server_addr;
	socklen_t addr_len = sizeof(server_addr);

	ssize_t n = recvfrom(cs->udp_fd, buf, 3, 0, (struct sockaddr*)&server_addr, &addr_len);
	if(n <= 0) return;
	buf[n] = '\0';

	if(n == 3){
		char notif_type = buf[0];
		char hex_str[3];
        hex_str[0] = buf[2];
        hex_str[1] = buf[1];
        hex_str[2] = '\0';
		int count = (int)strtol(hex_str, NULL, 16);

		printf("\n[NOTIFICA UDP] ");
        switch (notif_type) {
            case '0':
                printf("Richiesta di amicizia in attesa! ");
                break;
            case '1':
                printf("Richiesta di amicizia accettata! ");
                break;
            case '2':
                printf("Richiesta di amicizia rifiutata. ");
                break;
            case '3':
                printf("Nuovo messaggio! ");
                break;
            case '4':
                printf("Nuovo messaggio di flooding! ");
                break;
            default:
                printf("Notifica sconosciuta. ");
                break;
        }
        printf("\n(Totale notifiche da leggere: %d. Usa 'consu')", count);
        printf("\n> ");
        fflush(stdout);
	}else{
        VLOG("Errore pacchetto UDP malformato: %s", buf);
    }
}

/* ─── Main ─── */

int main(int argc, char *argv[]){
    if(argc < 4 || argc > 5) {
        fprintf(stderr, "USO: %s <server_ip> <server_port> <udp_port> [-v]\n", argv[0]);
        fprintf(stderr, "ESEMPIO: %s 127.0.0.1 8080 7878 -v\n", argv[0]);
        return EXIT_FAILURE;
    }

    if(argc == 5 && strncmp(argv[4], "-v", 2) == 0) verbose_mode = 1;

    strncpy(g_server_ip, argv[1], sizeof(g_server_ip) - 1);
    g_server_ip[sizeof(g_server_ip) - 1] = '\0';

    char *endptr;
    long p = strtol(argv[2], &endptr, 10);
    if(*endptr != '\0' || p <= 0 || p > 9999){
        fprintf(stderr, "Porta server TCP non valida (1-9999)!\n");
        return EXIT_FAILURE;
    }
    g_server_port = (uint16_t)p;

    long u = strtol(argv[3], &endptr, 10);
    if(*endptr != '\0' || u <= 0 || u > 9999){
        fprintf(stderr, "Porta locale UDP non valida (1-9999)!\n");
        return EXIT_FAILURE;
    }
    uint16_t udp_port = (uint16_t)u;

    ClientState cs;
    g_cs = &cs;
    signal(SIGINT,  handle_sigint);
    signal(SIGTERM, handle_sigint);
    signal(SIGPIPE, SIG_IGN); 

    if(client_init(&cs, udp_port) < 0){
        fprintf(stderr, "Errore inizializzazione client!\n");
        return EXIT_FAILURE;
    }

    if(client_connect(&cs, g_server_ip, g_server_port) < 0){
        fprintf(stderr, "Errore connessione a %s:%u\n", g_server_ip, g_server_port);
        client_shutdown(&cs);
        return EXIT_FAILURE;
    }

    client_run(&cs);
    client_shutdown(&cs);
    return EXIT_SUCCESS;
}