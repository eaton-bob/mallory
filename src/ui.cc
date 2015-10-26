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
    if ( mlm_client_connect (client, endpoint, 5000, client_name) == -1 )
    {
        zsys_error ("ERROR");
        mlm_client_destroy(&client);
        return EXIT_FAILURE;
    }
    zsys_info ("connected succesfully to malamute");

    char *statuses[3];
    statuses[0] = strdup("new");
    statuses[1] = strdup("ack");
    statuses[2] = strdup("resolved");

    // 1)  need to get list of UUID of alerts

    //     form message
    zmsg_t *pubmsg = zmsg_new ();
    zmsg_addstr(pubmsg, "notresolved");
    //     set subject
    char *subject = strdup("LIST");
    //     send request (mailbox)
    mlm_client_sendto(client, "ALERT", subject, NULL, 5000, &pubmsg);
    zsys_info ("LIST message sent");
    //     wait for the reply
    zmsg_t *reply = mlm_client_recv (client);
    zsys_info ("LIST REPLY message received");
    if ( reply == NULL )
    {
        zsys_error ("Cannot get the list of alerts");
        exit(1);
    }
    free(subject);
    //     Assumption: only 2 alerts can be in the system
    char *uuid[2];
    uuid[0] = zmsg_popstr(reply);
    uuid[1] = zmsg_popstr(reply);
    zsys_info ("uuid1 =%s", uuid[0]);
    zsys_info ("uuid2 =%s", uuid[1]);

    // we would play with status changing
    while ( !zsys_interrupted )
    {
        // randomly select what alert would be changed
        int order = random() % 2;
        // select subject
        subject = uuid[order];
        // randomly select new status
        int status_number = random() % 3;
        // create message
        pubmsg = zmsg_new();
        zmsg_addstr(pubmsg, statuses[status_number]);
        mlm_client_sendto(client, "ALERT", subject, NULL, 5000, &pubmsg);
        // we are not going to spam :)
        sleep(2);
    }
    free(uuid[0]);
    free(uuid[1]);
    zmsg_destroy (&reply);

client_destroy:
    free(statuses[0]);
    free(statuses[1]);
    free(statuses[2]);
    mlm_client_destroy(&client);
    return 0;
}
