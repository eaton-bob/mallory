#include <malamute.h>

enum state {
    NEW,
    ACK,
    RESOLVED
};

int main(int argc, char** argv)
{
    mlm_client_t *client = mlm_client_new();

    const char *endpoint = "";
    const char *client_name = "UI";
    mlm_client_connect (client, endpoint, 5000, client_name);

    // 1)  need to get list of UUID of alerts

    //     form message
    zmsg_t *pubmsg = zmsg_new ();
    zmsg_addstr(pubmsg, "notresolved");
    //     set subject
    char *subject = strdup("LIST");
    //     send request (mailbox)
    mlm_client_sendto(client, "ALERT", subject, NULL, 5000, &pubmsg);
    //     wait for the reply
    zmsg_t *reply = mlm_client_recv (client);
    if ( reply == NULL )
    {
        zsys_error ("Cannot get the list of alerts");
        exit(1);
    }
    //     Assumption: only 2 alerts can be in the system
    char *uuid1 = zmsg_popstr(reply);
    char *uuid2 = zmsg_popstr(reply);
    zsys_info ("uuid1 =%s", uuid1);
    zsys_info ("uuid2 =%s", uuid2);

    mlm_client_destroy(&client);
    free(subject);
    free(uuid1);
    free(uuid2);
    zmsg_destroy (&reply);
    return 0;
}
