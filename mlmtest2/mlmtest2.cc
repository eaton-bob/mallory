#include <malamute.h>
#include <limits>

#include <string>

// g++ -std=c++11 mlmtest2.cc -lczmq -lmlm -o mlmtest2

static const uint64_t FROM=1;
static const uint64_t TO=100000000;
static const uint64_t REPEAT=2;
static const char* ENDPOINT = "ipc://@/mlm-test-2";

static void
s_producer (zsock_t *pipe, void *args)
{

    static const char* NAME_PREFIX = "producer.";
    srandom (time (NULL));
    std::string name (NAME_PREFIX);
    name.append (std::to_string (random () % 1000));

    mlm_client_t *producer = mlm_client_new ();
    assert (producer);

    int rv = mlm_client_connect (producer, ENDPOINT, 2000, name.c_str ());
    if (rv == -1) {
        zsys_error ("%s: mlm_client_connect () failed.", name.c_str ());
        return;
    }

    zsock_signal (pipe, 0);
    while (!zsys_interrupted)
    {
        zmsg_t *message = zmsg_new ();
        assert (message);

        zmsg_addstr (message, "COMPUTE");
        zmsg_addstr (message, std::to_string (FROM).c_str ());
        zmsg_addstr (message, std::to_string (TO).c_str ());
        zmsg_addstr (message, std::to_string (REPEAT).c_str ());

        rv = mlm_client_sendto (producer, "consumer", "", NULL, 1000, &message);
        if (rv != 0)
            zsys_error ("%s: mlm_client_sendto (address = 'consumer', timeout = '1000') failed.", name.c_str ());

        zmsg_t *reply = mlm_client_recv (producer);
        assert (reply);

        char *first = zmsg_popstr (reply);
        assert (first);
        char *second = zmsg_popstr (reply);
        assert (second);

        zsys_info ("%s: %s/%s", name.c_str (), first, second);
        zstr_free (&first);
        zstr_free (&second);
        zmsg_destroy (&reply);
    }

    mlm_client_destroy (&producer);
}


static void
s_producer_stream (zsock_t *pipe, void *args)
{
    static const char* NAME_PREFIX = "producer_stream.";
    srandom (time (NULL));
    std::string name (NAME_PREFIX);
    name.append (std::to_string (random () % 1000));

    mlm_client_t *producer = mlm_client_new ();
    assert (producer);

    int rv = mlm_client_connect (producer, ENDPOINT, 2000, name.c_str ());
    if (rv == -1) {
        zsys_error ("%s: mlm_client_connect () failed.", name.c_str ());
        return;
    }
    rv = mlm_client_set_producer (producer, "TESTSTREAM");
    if (rv == -1) {
        zsys_error ("%s: mlm_client_set_producer () failed.", name.c_str ());
        return;
    }

    zsock_signal (pipe, 0);

    while (!zsys_interrupted) {

        zmsg_t *message = zmsg_new ();
        assert (message);

        zmsg_addstr (message, "aaaa");

        rv = mlm_client_send (producer, "ASDF", &message);
        if (rv != 0)
            zsys_error ("%s: mlm_client_sendto (address = 'consumer', timeout = '1000') failed.", name.c_str ());
    }
    mlm_client_destroy (&producer);
}

// convert string to uint64_t safely
// 0 success, -1 error
static int
s_str_to_uint64 (const std::string& string, uint64_t& uint64) {

    size_t pos = 0;
    try {
        unsigned long long u = std::stoull (string, &pos);
        if (pos != string.size ()) {
            return -1;
        }
        if (u >  std::numeric_limits<uint64_t>::max ()) {
            return -1;
        }
        uint64 = static_cast<uint64_t> (u);
    }
    catch (...) {
        return -1;
    }
    return 0;
}

// send error message to the sender of last received message
static void
s_error_reply (mlm_client_t *client, const char *reason) {
    assert (client);
    assert (reason);

    zmsg_t *message = zmsg_new ();
    assert (message);

    zmsg_addstr (message, "ERROR");
    zmsg_addstr (message, reason);

    int rv = mlm_client_sendto (client, mlm_client_sender (client), "", NULL, 1000, &message);
    if (rv != 0)
        zsys_error ("mlm_client_sendto (address = '%s', timeout = '1000') failed.", mlm_client_sender (client));
    zmsg_destroy (&message);
}

// simulation of cpu intensive task
static double
s_compute (uint64_t from, uint64_t to, uint64_t repeat) {
    double final_result = 0;
    for (uint64_t times = 1; times <= repeat; times++) {
        double result = 0;
        for (uint64_t i = from; i < to; i++) {
            result += i;
            if (i % 2 == 0)
                result = result / i;
        }
        final_result += result;
        zsys_debug ("s_consumer: result == '%s', final_result == '%s'",
                std::to_string (result).c_str (), std::to_string (final_result).c_str ());
    }
    final_result = final_result / repeat;
    zsys_debug ("s_consumer: final_result == '%s'", std::to_string (final_result).c_str ());
    return final_result;
}

static void
s_handle_mailbox (mlm_client_t *client, zmsg_t **message_p) {
    assert (client);
    assert (message_p && *message_p);

    zmsg_t *message = *message_p;

    char *str = zmsg_popstr (message);
    if (!str || !streq (str, "COMPUTE")) {
        s_error_reply (client, "Bad command");
        zstr_free (&str);
        return;
    }
    zstr_free (&str);

    str = zmsg_popstr (message);
    if (!str) {
        s_error_reply (client, "Field 'from' missing");
        return;
    }
    uint64_t from = 0;
    int rv = s_str_to_uint64 (str, from);
    zstr_free (&str);
    if (rv == -1) {
        s_error_reply (client, "Field 'from' not and unsigned integer.");
        return;
    }

    str = zmsg_popstr (message);
    if (!str) {
        s_error_reply (client, "Field 'to' missing");
        return;
    }
    uint64_t to = 0;
    rv = s_str_to_uint64 (str, to);
    zstr_free (&str);
    if (rv == -1) {
        s_error_reply (client, "Field 'to' not and unsigned integer.");
        return;
    }

    uint64_t repeat = 1;
    str = zmsg_popstr (message);
    if (str) {
        rv = s_str_to_uint64 (str, repeat);
        if (rv == -1)
            zsys_warning ("Field 'repeat' == '%s' is not an unsigned integer. Using 1 repeat instead.", str);
        zstr_free (&str);
    }
    zmsg_destroy (message_p);

    double result = s_compute (from, to, repeat);
    zmsg_t *reply = zmsg_new ();
    assert (reply);

    zmsg_addstr (reply, "RESULT");
    zmsg_addstr (reply, std::to_string (result).c_str ());

    /*
    // IMPORTANT: Here, just after the long calculation, let's check mlm_client_connected
    zsys_info ("Am i still connected? %s", mlm_client_connected (client) ? "YES" : "NO");
    */

    rv = mlm_client_sendto (client, mlm_client_sender (client), "", NULL, 1000, &reply);
    if (rv != 0)
        zsys_error ("mlm_clien_sendto (address = '%s', timeout = '1000') failed.", mlm_client_sender (client));
}

static void
s_consumer (zsock_t *pipe, void *args)
{
    mlm_client_t *consumer = mlm_client_new ();
    assert (consumer);

    int rv = mlm_client_connect (consumer, ENDPOINT, 2000, "consumer");
    assert (rv >= 0);
    rv = mlm_client_set_consumer (consumer, "TESTSTREAM", ".*");
    mlm_client_set_verbose (consumer, true);

    zpoller_t *poller = zpoller_new (pipe, mlm_client_msgpipe (consumer), NULL);
    assert (poller);

    while (!zsys_interrupted) {

        void *which = zpoller_wait (poller, -1);

        if (which == pipe || zsys_interrupted)
            break;

        if (!which) {
            if (!zsys_interrupted)
                zsys_error ("s_consumer: zpoller_wait () returned NULL while zsys_interrupted == false.");
            break;
        }

        zmsg_t *msg = mlm_client_recv (consumer);
        zsys_error ("s_consumer: receiver message, command: %s", mlm_client_command (consumer));
        if (!msg) {
            if (!zsys_interrupted)
                zsys_error ("s_consumer: mlm_client_recv () returned NULL while zsys_interrupted == false.");
            break;
        }
        s_handle_mailbox (consumer, &msg);

    }

    zpoller_destroy (&poller);
    mlm_client_destroy (&consumer);
    zsys_debug ("s_consumer: mlm test #2 CONSUMER stopping.");
}

int main ()
{
    zactor_t *server = zactor_new (mlm_server, (void*) "Malamute");
    zstr_sendx (server, "BIND", ENDPOINT, NULL);
    zstr_sendx (server, "VERBOSE", NULL);

    zactor_t *producer = zactor_new (s_producer, NULL);
    zactor_t *producer_stream = zactor_new (s_producer_stream, NULL);
    zactor_t *consumer = zactor_new (s_consumer, NULL);

    // copy from src/malamute.c - under MPL license
    //  Accept and print any message back from server
    while (true) {
        char *message = zstr_recv (server);
        if (message) {
            puts (message);
            free (message);
        }
        else {
            puts ("interrupted");
            break;
        }
    }

    zactor_destroy (&consumer);
    zactor_destroy (&producer_stream);
    zactor_destroy (&producer);

    zactor_destroy (&server);
}
