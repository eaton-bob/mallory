#include <string>
#include <cstdlib>

#include <malamute.h>

static const char* ENDPOINT = "ipc://@/mlm-test-2";
static const char* NAME_PREFIX = "producer2.";

/* TODO: write the two required actor commands
static void
s_producer (zsock_t *pipe, void *args)
{
    assert (args);
    char *name = (char*) args;
    
    mlm_client_t *client = mlm_client_new ();
    assert (client);

    int rv = mlm_client_connect (client, ENDPOINT, 2000, name);
    assert (rv >= 0);

    zpoller_t *poller = zpoller_new (pipe, mlm_client_msgpipe (client), NULL);
    assert (poller);

    zsock_signal (pipe, 0);


    while (!zsys_interrupted) {
        void *which = zpoller_wait (poller, -1);
        if (which == NULL) {
            zsys_debug ("which == NULL; zpoller_terminated == '%s'; zsys_interrupted == '%s'",
                zpoller_terminated (poller) ? "true" : "false",
                zsys_interrupted ? "true" : "false");
            break;
        }

        if (which == pipe) {
            char *str = zmsg_recv (pipe);
            if (!str) {
                log_error ("Bad actor command.");
                continue;
            }
            if (streq (cmd, "$TERM")) {
                break;
            }
            else
            if (streq (cmd, "VERBOSE")) {
                verbose = true;
            }
            continue;
        }


    }

    mlm_client_destroy (&client);
}
*/

int main (int argc, char **argv) {
    srandom (time (NULL));
    std::string name (NAME_PREFIX);
    name.append (std::to_string (random () % 1000));

    mlm_client_t *producer = mlm_client_new ();
    assert (producer);

    int rv = mlm_client_connect (producer, ENDPOINT, 2000, name.c_str ());
    if (rv == -1) {
        zsys_error ("%s: mlm_client_connect () failed.", name.c_str ());
        return EXIT_FAILURE;
    }
    rv = mlm_client_set_producer (producer, "TESTSTREAM");
    if (rv == -1) {
        zsys_error ("%s: mlm_client_set_producer () failed.", name.c_str ());
        return EXIT_FAILURE;
    }
    while (!zsys_interrupted) {

        zmsg_t *message = zmsg_new ();
        assert (message);

        zmsg_addstr (message, "aaaa");

        rv = mlm_client_send (producer, "ASDF", &message);
        if (rv != 0)
            zsys_error ("%s: mlm_client_sendto (address = 'consumer', timeout = '1000') failed.", name.c_str ());
    }
    mlm_client_destroy (&producer);
    return EXIT_SUCCESS;
}
