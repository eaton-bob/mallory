#include <malamute.h>
#include <string>
#include <map>

#define USAGE "<mlm_endpoint>"
#define ALERTS_STREAM "METRICS"
#define EVALS_STREAM "EVALS"

/*
// TODO: To be replaced by zconfig
int load (const char *file) {
    assert (file);
    zfile_t *file = zfile_new ("./", file);
}
int save (const char *file) {
}
*/

int main (int argc, char **argv) {
    if (argc < 2) {
        zsys_error ("Usage: %s %s", argv[0], USAGE);
        return EXIT_FAILURE;
    }

    zsys_info ("mlm_endpoint: '%s'", argv[1]);

    mlm_client_t *client = mlm_client_new ();
    assert (client);

    int rv = mlm_client_connect (client, argv[1], 1000, argv[0]);
    assert (rv != -1);

    rv = mlm_client_set_consumer (client, EVALS_STREAM, "");
    assert (rv != -1);
    rv = mlm_client_set_producer (client, ALERTS_STREAM);
    assert (rv != -1);

    while (!zsys_interrupted) {
        zmsg_t *msg = mlm_client_recv (client);
        if (!msg)
            break;
         
        if (streq (mlm_client_command (client), "STREAM DELIVER")) {
            // TODO: Acknowleding not implemented yet. Now we are simply forwarding the message.
            //       Need to connect the pieces asap.
            if (mlm_client_send (client, mlm_client_subject (client), &msg) != 0)
                zsys_error ("mlm_client_send (subject = '%s') failed.", mlm_client_subject (client));
            else
                zsys_info ("TRIGGER subject='%s'", mlm_client_subject (client));
            zmsg_destroy (&msg);
        }
        else if (streq (mlm_client_command (client), "MAILBOX DELIVER")) {
            zsys_debug ("Not implemented yet.");
            zmsg_destroy (&msg);
        }
        else {
            zsys_error ("%s does not offer any service. Sender: '%s', Subject: '%s'.",
                        argv[0], mlm_client_sender (client), mlm_client_subject (client));
            zmsg_destroy (&msg);
            continue;
        }
    }

    mlm_client_destroy (&client);
    return EXIT_SUCCESS;
}
