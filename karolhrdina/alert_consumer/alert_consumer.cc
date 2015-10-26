#include <malamute.h>
#include <string>

#define USAGE "<mlm_endpoint> <function>"
#define STREAM_NAME "ALERTS"

int main (int argc, char **argv) {
    if (argc < 2) {
        zsys_error ("Usage: %s %s", argv[0], USAGE);
        return EXIT_FAILURE;
    }

    zsys_info ("mlm_endpoint: '%s'", argv[1]);
    zsys_info ("function: '%s'", argv[2]);

    const char *function = argv[2];

    mlm_client_t *client = mlm_client_new ();
    assert (client);

    std::string client_name (function);
    client_name.append ("-sender");
    int rv = mlm_client_connect (client, argv[1], 1000, client_name.c_str ());
    assert (rv != -1);

    rv = mlm_client_set_consumer (client, STREAM_NAME, "");
    assert (rv != -1);


    while (!zsys_interrupted) {
        zmsg_t *msg = mlm_client_recv (client);
        if (!msg)
            break;
        if (!streq (mlm_client_command (client), "STREAM DELIVER")) {
            zmsg_destroy (&msg);
            continue;
        }
        char *alert_name = zmsg_popstr (msg);
        char *device_name = zmsg_popstr (msg);
        if (!alert_name || !device_name) {
            zsys_error ("Wrong message format recevied. Subject: '%s'\tSender: '%s'",
                   mlm_client_subject (client), mlm_client_sender (client));
            zmsg_destroy (&msg);
            continue;
        }
        zsys_info ("Sending alert '%s' from device '%s' using '%s'.", alert_name, device_name, function);
        zmsg_destroy (&msg);
    }

    mlm_client_destroy (&client);
    return EXIT_SUCCESS;
}
