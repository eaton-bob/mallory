#include <malamute.h>
#include <string>
#include <map>
#include "../streams.h"

#define USAGE "<mlm_endpoint> [LIST | ACK <alert> [ON | OFF]]"
#define CLIENT_NAME "user"

int main (int argc, char **argv) {
    if (argc < 3 ||
        (!streq (argv[2], "LIST") && !streq (argv[2], "ACK")) ||
        (streq (argv[2], "ACK") && argc < 5)) {
        zsys_error ("Usage: %s %s", CLIENT_NAME, USAGE);
        return EXIT_FAILURE;
    }
    zsys_info ("client name: '%s'", CLIENT_NAME);
    zsys_info ("mlm_endpoint: '%s'", argv[1]);
    zsys_info ("command: '%s'", argv[2]);  

    mlm_client_t *client = mlm_client_new ();
    assert (client);

    int rv = mlm_client_connect (client, argv[1], 1000, CLIENT_NAME);
    assert (rv != -1);

    zmsg_t *msg = zmsg_new ();
    if (streq (argv[2], "ACK")) {
       zsys_info ("alarm: '%s'", argv[3]);
       zsys_info ("status: '%s'", argv[4]);
       zmsg_addstr (msg, argv[3]);
       zmsg_addstr (msg, argv[4]);
    }

    if (mlm_client_sendto (client, "alert", argv[2], NULL, 1000, &msg) != 0)
        zsys_error ("mlm_client_sendto () failed.");
    else
        zsys_info ("Message sent. Subject: '%s'", argv[2]);

    msg = mlm_client_recv (client);
    if (!msg) {
        zsys_info ("Interrupt received");
        mlm_client_destroy (&client);
        return EXIT_SUCCESS;
    }
    assert (streq (mlm_client_command (client), "MAILBOX DELIVER"));
    char *message = zmsg_popstr (msg);
    while (message) {
        zsys_debug ("%s", message);
        message = zmsg_popstr (msg);
    }
    zmsg_destroy (&msg);
    mlm_client_destroy (&client);
    return EXIT_SUCCESS;

}

