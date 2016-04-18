#include <string>
#include <limits>

#include <malamute.h>

static const char* ENDPOINT = "ipc://@/mlm-test-2";

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
        zsys_debug ("result == '%s', final_result == '%s'",
                std::to_string (result).c_str (), std::to_string (final_result).c_str ());
    }
    final_result = final_result / repeat;
    zsys_debug ("final_result == '%s'", std::to_string (final_result).c_str ());
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

    // IMPORTANT: Here, just after the long calculation, let's check mlm_client_connected
    zsys_info ("Am i still connected? %s", mlm_client_connected (client) ? "YES" : "NO");

    rv = mlm_client_sendto (client, mlm_client_sender (client), "", NULL, 1000, &reply);
    if (rv != 0)
        zsys_error ("mlm_clien_sendto (address = '%s', timeout = '1000') failed.", mlm_client_sender (client));
}

int main () {
    zsys_debug ("mlm test #2 CONSUMER starting.");
    
    mlm_client_t *consumer = mlm_client_new ();
    assert (consumer);

    int rv = mlm_client_connect (consumer, ENDPOINT, 2000, "consumer");
    assert (rv >= 0);
    rv = mlm_client_set_consumer (consumer, "TESTSTREAM", ".*");

#ifdef WITH_POLLER    
    // This is just to simulate the agent setup as closely as possible
    // otherwise Alenka will argue with me to no end
    zsys_debug ("Creating poller!");
    zpoller_t *poller = zpoller_new (mlm_client_msgpipe (consumer), NULL);
    assert (poller);
#endif

    while (!zsys_interrupted) {

#ifdef WITH_POLLER
        void *which = zpoller_wait (poller, -1);
        if (!which) {
            if (!zsys_interrupted)
                zsys_error ("zpoller_wait () returned NULL while zsys_interrupted == false.");
            break;
        }
#endif        

        zmsg_t *msg = mlm_client_recv (consumer);
        if (!msg) {
            if (!zsys_interrupted)
                zsys_error ("mlm_client_recv () returned NULL while zsys_interrupted == false.");
            break;
        }
        if (!streq (mlm_client_command (consumer), "MAILBOX DELIVER")) {
            zsys_warning ("Received command '%s'. Ignoring.", mlm_client_command (consumer));
            continue;                
        }
        s_handle_mailbox (consumer, &msg);
         
    } 

#ifdef WITH_POLLER    
    zpoller_destroy (&poller);
#endif
    mlm_client_destroy (&consumer);
    zsys_debug ("mlm test #2 CONSUMER stopping.");
    return EXIT_SUCCESS;
}

