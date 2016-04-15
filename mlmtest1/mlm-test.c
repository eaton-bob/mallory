#include <malamute.h>

static const char* ENDPOINT = "inproc://@/mlm-test-addf432ebc66f4";

static void
s_producer (zsock_t *pipe, void *args)
{
    char *name = strdup ((char*) args);
    mlm_client_t *client = mlm_client_new ();
    mlm_client_connect (client, ENDPOINT, 2000, name);
    mlm_client_set_producer (client, "STREAM");

    zsock_signal (pipe, 0);
    while (!zsys_interrupted)
    {
        mlm_client_sendx (client, "SUBJECT@DEVICE", "42", NULL);
        zclock_sleep (100);

        // hack to end the actor
        char *msg = zstr_recv_nowait (pipe);
        if (!msg)
            continue;
        else {
            zstr_free (&msg);
            break;
        }
    }

    mlm_client_destroy (&client);
    zstr_free (&name);
}

static uint64_t
s_cpu_intensive_processing (zmsg_t **msg_p)
{
    zmsg_t *msg = *msg_p;
    //TODO: intensive processing here
    zmsg_print (msg);
    zmsg_destroy (&msg);
    return 1;
}

static void
s_consumer (zsock_t *pipe, void *args)
{
    char *name = strdup ((char*) args);
    mlm_client_t *client = mlm_client_new ();
    mlm_client_connect (client, ENDPOINT, 2000, name);
    mlm_client_set_consumer (client, "STREAM", ".*");

    zpoller_t *poller = zpoller_new (pipe, mlm_client_msgpipe (client), NULL);

    uint64_t acc = 0;

    zsock_signal (pipe, 0);
    while (!zsys_interrupted)
    {
        void *which = zpoller_wait (poller, -1);

        if (which == pipe)
            break;

        zmsg_t *msg = mlm_client_recv (client);
        acc += s_cpu_intensive_processing (&msg);
    }

    // this is a trick to really "use" the acc, so any clever compiler won't optimize the s_cpu_intensive_processing
    zsys_info ("acc=%"PRIu64, acc);

    mlm_client_destroy (&client);
    zstr_free (&name);
}

int main ()
{
    zactor_t *server = zactor_new (mlm_server, "Malamute");
    zstr_sendx (server, "BIND", ENDPOINT, NULL);

    zactor_t *producer = zactor_new (s_producer, "producer-1");
    zactor_t *consumer = zactor_new (s_consumer, "consumer-1");

    zclock_sleep (10000);

    zactor_destroy (&producer);
    zactor_destroy (&consumer);
    zactor_destroy (&server);
}
