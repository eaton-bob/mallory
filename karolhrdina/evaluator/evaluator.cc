#include <malamute.h>
#include <string>
#include "../streams.h"

// TODO: <mlm_endpoint> <zconfing>
//       we have simple hardcoded rules now
#define USAGE "<mlm_endpoint>"

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

    rv = mlm_client_set_consumer (client, METRICS_STREAM, ".*");
    assert (rv != -1);
    rv = mlm_client_set_producer(client, EVALS_STREAM);
    assert (rv != -1);

    while (!zsys_interrupted) {
        zmsg_t *msg = mlm_client_recv (client);
        if (!msg)
            break;
        if (!streq (mlm_client_command (client), "STREAM DELIVER")) {
            zmsg_destroy (&msg);
            continue;
        }
        const char *subject = mlm_client_subject (client);
        char *metric_name = zmsg_popstr (msg);
        char *metric_value = zmsg_popstr (msg);
        assert (metric_name); assert (metric_value);
        zmsg_destroy (&msg); 
        // TODO: checks (stoi, char * nullity ...)
        uint64_t value = std::stoi (metric_value);
        // hardcoded rule for temp > 10
        if (streq (metric_name, "temp") && value > 70) {
            msg = zmsg_new ();
            zmsg_addstr (msg, "ONFIRE");
            zmsg_addstr (msg, subject);
            std::string send_subj ("ONFIRE");
            send_subj.append ("@").append (subject);
            if (mlm_client_send (client, send_subj.c_str (), &msg) != 0)
                zsys_error ("mlm_client_send (subject = '%s') failed.", send_subj.c_str ());
            else
                zsys_info ("TRIGGER subject='%s'", send_subj.c_str ());
        }
        else if (streq (metric_name, "hum") && value > 50) {
            // temporary copy-paste
            msg = zmsg_new ();
            zmsg_addstr (msg, "CORROSION");
            zmsg_addstr (msg, subject);
            std::string send_subj ("CORROSION");
            send_subj.append ("@").append (subject);
            if (mlm_client_send (client, send_subj.c_str (), &msg) != 0)
                zsys_error ("mlm_client_send (subject = '%s') failed.", send_subj.c_str ());
            else
                zsys_info ("TRIGGER subject='%s'", send_subj.c_str ());
        }
        free (metric_name); free (metric_value);
    }

    mlm_client_destroy (&client);
    return EXIT_SUCCESS;
}
