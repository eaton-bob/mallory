#include <malamute.h>
#include <string>

#define USAGE "<mlm_endpoint> <gen_name> <metric_name> <range>"
#define STREAM_NAME "METRIC"
#define FREQ 5

//  Provide random number from 0..(num-1)
#define randof(num)  (int) ((float) (num) * random () / (RAND_MAX + 1.0))

int main (int argc, char **argv) {        
    if (argc < 4) {
        zsys_error ("Usage: %s %s", argv[0], USAGE);
        return EXIT_FAILURE;
    }

    zsys_info ("mlm_endpoint: '%s'", argv[1]);
    zsys_info("gen_name: '%s'", argv[2]);
    zsys_info ("metric_name: '%s'", argv[3]);
    zsys_info ("range: '%s'", argv[4]);

    int range = std::stoi (argv[4]);

    srandom ((unsigned) time (NULL));

    mlm_client_t *client = mlm_client_new ();
    assert (client);
    int rv = mlm_client_connect (client, argv[1], 1000, argv[2]);
    assert (rv != -1);
    rv = mlm_client_set_producer(client, STREAM_NAME);
    assert (rv != -1);

    while (!zsys_interrupted) {
        zmsg_t *msg = zmsg_new ();
        int value = randof (range);
        zmsg_addstr (msg, argv[3]);
        zmsg_addstr (msg, std::to_string (value).c_str ());
        if (mlm_client_send (client, argv[2], &msg) != 0)
            zsys_error ("mlm_client_send () failed.");
        else
            zsys_info ("\t%d", value);
        
        zclock_sleep (randof (FREQ) * 1000);
    }

    mlm_client_destroy (&client);        
    return EXIT_SUCCESS;
}
