#include "flood.h"
#include "client_store.h"
#include "flow_store.h"

#include "../common/net_utils.h"

#include <stdio.h>
#include <string.h>

/* ─── Algoritmo di Flood ─── */

int flood_send(Server *srv, Client *sender, const char *mess){
    if (srv == NULL || sender == NULL || mess == NULL) return 0;

    bool visited[MAX_CLIENTS] = {false};
    int sender_idx = cs_index_of(srv, sender->id);
    if(sender_idx < 0) return 0;

    flood_bfs(srv, sender_idx, visited);
    visited[sender_idx] = false;
    int count = 0;

    for(int i = 0; i < srv->count; i++){
        if(visited[i]){
            Client *dest = &srv->clients[i];

            if(fs_push(dest, FLOW_FLOOD, sender->id, mess) == 0){
                send_udp_notify(dest->ip, dest->udp_port, FLOW_FLOOD, dest->flow_count);
                count++;
            }else{
                VLOG("FLOO Avviso: coda di '%s' piena, messaggio flood scartato", dest->id);
            }
        }
    }
    return count; 
}

void flood_bfs(Server *srv, int start_index, bool *visited){
    if (srv == NULL || visited == NULL || start_index < 0 || start_index >= srv->count) return;
    
    int queue[MAX_CLIENTS];
    int head = 0, tail = 0;
    int friend_buf[MAX_CLIENTS];

    visited[start_index] = true;
    queue[tail] = start_index;
    tail++;

    while(head < tail){
        int current = queue[head];
        head++;
        Client *curr_cl = &srv->clients[current];
        int num_friend = cs_get_friend_indices(curr_cl, friend_buf, MAX_CLIENTS);
        if(num_friend <= 0) continue;

        for(int i = 0; i < num_friend; i++){
            if(!visited[friend_buf[i]]){
                visited[friend_buf[i]] = true;
                queue[tail] = friend_buf[i];
                tail++;
            }
        }
    }
}