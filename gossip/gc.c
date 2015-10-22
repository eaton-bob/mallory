#include <czmq.h>

int main(int argc, char** argv) {

    if (argc != 2) {
        fprintf (stderr, "Usage: %s name\n", argv[0]);
        exit (EXIT_FAILURE);
    }

    zactor_t *gs = zactor_new (zgossip, argv[1]);
    zstr_send (zgossip, "VERBOSE");
    zstr_sendx (gs, "CONNECT", "ipc://@/bios-alerts", NULL);

    char buf[1024];
    snprintf(buf, 1024, "%d", random());
    zsys_debug ("PUBLISH: %s", buf);
    zstr_sendx (gs, "PUBLISH", "X-BIOS-PATH", buf, NULL);

    zclock_sleep (1000);

    zactor_destroy (&gs);

}
