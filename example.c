#include <signal.h>
#include <stdio.h>
#include <ev.h>
#include "sev.h"

#define PORT 5555

void open_cb(struct sev_client *client)
{
    printf("open %s:%d\n", client->ip, client->port);
}

void read_cb(struct sev_client *client, const char *data, size_t len)
{
    printf("read %s: %s\n", client->ip, data);

    sev_send(client, "hello\n", 6);

    if (data[0] == 'q') {
        sev_close(client);
    }
}

void close_cb(struct sev_client *client)
{
    printf("close %s\n", client->ip);
}

int main(int argc, char *argv[])
{
    signal(SIGPIPE, SIG_IGN);

    struct sev_server server;

    if (sev_server_init(&server, PORT)) {
        perror("sev_server_init");
        return -1;
    }

    server.open_cb = open_cb;
    server.read_cb = read_cb;
    server.close_cb = close_cb;

    ev_loop(EV_DEFAULT_ 0);

    return 0;
}
