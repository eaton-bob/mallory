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
    statuses[0] = strdup("NEW");
    statuses[1] = strdup("ACK");
    statuses[2] = strdup("RESOLVED");

    // 1)  need to get list of UUID of alerts

    //     form message
    zmsg_t *pubmsg = zmsg_new ();
    zmsg_addstr(pubmsg, "notresolved");
    //     set subject
    char *subject = strdup("LIST");
    //     send request (mailbox)
    zsys_debug ("send smhng on ALERT");
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

    zmsg_print (reply);

    zframe_t *frame = zmsg_pop (reply);
    zhashx_t *alerts = zhashx_unpack (frame);

    // key - name
    // value - status
    for (void* it = zhashx_first (alerts); it != NULL; it = zhashx_next (alerts)) {
        printf ("%s/%s\n", (char*) zhashx_cursor (alerts), (char*) it);
    }

    // we would play with status changing only for first alert
    char *alert_status = (char *) zhashx_first (alerts);
    char *alert_name = (char *) zhashx_cursor (alerts);
    while ( !zsys_interrupted )
    {
        // randomly select new status
        int status_number = random() % 3;
        // create message
        pubmsg = zmsg_new();
        zmsg_addstr(pubmsg, alert_name);
        zmsg_addstr(pubmsg, statuses[status_number]);
        mlm_client_sendto(client, "ALERT", alert_name, NULL, 5000, &pubmsg);
        // we are not going to spam :)
        sleep(2);
    }
    zhashx_destroy (&alerts);
    zframe_destroy (&frame);
    zmsg_destroy (&reply);
    free(statuses[0]);
    free(statuses[1]);
    free(statuses[2]);
    zmsg_destroy (&reply);
    mlm_client_destroy(&client);
    return 0;
}
