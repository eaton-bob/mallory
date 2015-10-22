#include <czmq.h>

int main(int argc, char** argv) {

    if (argc != 2) {
        fprintf (stderr, "Usage: %s name\n", argv[0]);
        exit (EXIT_FAILURE);
    }

    zactor_t *gs = zactor_new (zgossip, argv[1]);
    zstr_send (zgossip, "VERBOSE");
    zstr_sendx (gs, "BIND", "ipc://@/bios-alerts");

    /*
    char buf[1024];
    snprintf(buf, 1024, "%d", random());
    zsys_debug ("PUBLISH: %s", buf);
    zstr_sendx (gs, "PUBLISH", "X-BIOS-PATH", buf, NULL);
    */

    while (!zsys_interrupted) {
        char *method, *key, *value;
        zmsg_t *msg = zactor_recv (gs);

        if (!msg)
            continue;

        zmsg_print (msg);

        method = zmsg_popstr (msg);
        key = zmsg_popstr (msg);
        value = zmsg_popstr (msg);

        printf ("method: '%s', key: '%s', value: '%s'\n", method, key, value);
        zstr_free (&key);
        zstr_free (&value);
        zstr_free (&method);
        zmsg_destroy (&msg);
    }

    zactor_destroy (&gs);

}
