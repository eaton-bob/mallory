#include <malamute.h>

enum state {
    NEW,
    ACK,
    RESOLVED
};

int main(int argc, char** argv)
{
    mlm_client_t *client = mlm_client_new();

    const char *endpoint = "ipc://@/malamute";
    const char *client_name = "UI";
    mlm_client_connect (client, endpoint, 5000, client_name);

    // 1)  need to get list of UUID of alerts

    //     form message
    zmsg_t *pubmsg = zmsg_new ();
    zmsg_addstr(pubmsg, "notresolved");
    //     set subject
    char *subject = strdup("LIST");
    //     send request (mailbox)
    zsys_debug ("send smhng on ALERT");
    mlm_client_sendto(client, "ALERT", subject, NULL, 5000, &pubmsg);
    //     wait for the reply
    zmsg_t *reply = mlm_client_recv (client);
    if ( reply == NULL )
    {
        zsys_error ("Cannot get the list of alerts");
        exit(1);
    }

    zframe_t *frame = zmsg_pop (pubmsg);
    zhashx_t *alerts = zhashx_unpack (frame);

    for (void* it = zhashx_first (alerts); it != NULL; it = zhashx_next (alerts)) {
        printf ("%s/%s\n", (char*) it, (char*) zhashx_cursor (alerts));
    }

    zhashx_destroy (&alerts);
    zframe_destroy (&frame);
    zmsg_destroy (&reply);

    mlm_client_destroy(&client);
    return 0;
}
