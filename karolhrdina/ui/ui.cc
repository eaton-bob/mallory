#include <malamute.h>
#include <string>
#include <map>
#include <iostream>
#include "../streams.h"

#define USAGE "<mlm_endpoint> [LIST | ACK <alert> <device> [ON | OFF]]"
#define CLIENT_NAME "user"

int main (int argc, char **argv) {
    if (argc < 3 ||
        (!streq (argv[2], "LIST") && !streq (argv[2], "ACK")) ||
        (streq (argv[2], "ACK") && argc < 6)) {
        zsys_error ("Usage: %s %s", CLIENT_NAME, USAGE);
        return EXIT_FAILURE;
    }
    zsys_info ("client name: '%s'", CLIENT_NAME);
    zsys_info ("mlm_endpoint: '%s'", argv[1]);
    zsys_info ("command: '%s'", argv[2]);  

    mlm_client_t *client = mlm_client_new ();
    assert (client);

    int rv = mlm_client_connect (client, argv[1], 1000, CLIENT_NAME);
    assert (rv != -1);

    zmsg_t *msg = zmsg_new ();
    zmsg_addstr (msg, argv[2]); // add command
    if (streq (argv[2], "ACK")) {
       zsys_info ("alert: '%s'", argv[3]);
       zsys_info ("device: '%s'", argv[4]);
       zsys_info ("status: '%s'", argv[5]);
       zmsg_addstr (msg, argv[3]);
       zmsg_addstr (msg, argv[4]);
       zmsg_addstr (msg, argv[5]);
    }

    if (mlm_client_sendto (client, "alert", argv[2], NULL, 1000, &msg) != 0) {
        zsys_error ("mlm_client_sendto () failed.");
        zmsg_destroy (&msg);
        mlm_client_destroy (&client);
        return EXIT_FAILURE;
    }
    zsys_info ("Message sent. Subject: '%s'", argv[2]);
    msg = mlm_client_recv (client);
    if (!msg) {
        zsys_info ("Interrupt received");
        mlm_client_destroy (&client);
        return EXIT_FAILURE;
    }
    assert (streq (mlm_client_command (client), "MAILBOX DELIVER"));
    assert (streq (mlm_client_sender (client), "alert"));
    char *command = zmsg_popstr (msg);
    assert (command);
    int return_value = EXIT_SUCCESS;
    if (streq (command, "LIST")) {
        std::cout << "List of acknowledged alerts:" << std::endl;           
        char *message = zmsg_popstr (msg);
        while (message) {
            std::cout << message << std::endl;
            free (message);
            message = zmsg_popstr (msg);
        }
    }
    else if (streq (command, "ACK")) {
        char *alert = zmsg_popstr (msg);
        char *device = zmsg_popstr (msg);
        char *state = zmsg_popstr (msg);
        assert (alert); assert (device); assert (state);
        if (!streq (alert, argv[3]) || !streq (device, argv[4])) {
            std::cerr << "Received response for different alert/device:" << alert << "/" << device << std::endl;
            return_value = EXIT_FAILURE;
        }
        else if (streq (state, argv[5])) {
            std::cout << "Alert '" << alert << "' successfully set to ACKNOWLEDGE " << state << " for device '" << device << "'." << std::endl;
        }
        else if (streq (state, "ERROR")) {
            char *reason = zmsg_popstr (msg);
            std::cerr << "Error setting alert '" << alert << "' to ACKNOWLEDGE " << state << " for device '" << device << "'";
            if (reason) {
                std::cerr << ": " << reason << std::endl;
                free (reason);
            }
            else { 
               std::cerr << "." << std::endl;
            }
            return_value = EXIT_FAILURE;
        }
        else {
            std::cerr << "Wrong response." << std::endl;
            return_value = EXIT_FAILURE;
        }
        free (alert); free (device); free (state);
    }
    else {
        std::cerr << "Wrong response." << std::endl;
        return_value = EXIT_FAILURE;
    }
    free (command);
    zmsg_destroy (&msg);
    mlm_client_destroy (&client);
    return return_value;
}

