#include <malamute.h>
#include <string>
#include <map>
#include "../streams.h"

#define USAGE "<mlm_endpoint>"
#define CLIENT_NAME "alert"


int main (int argc, char **argv) {
    if (argc < 2) {
        zsys_error ("Usage: %s %s", CLIENT_NAME, USAGE);
        return EXIT_FAILURE;
    }

    zsys_info ("client name: '%s'", CLIENT_NAME);
    zsys_info ("mlm_endpoint: '%s'", argv[1]);

    mlm_client_t *client = mlm_client_new ();
    assert (client);

    int rv = mlm_client_connect (client, argv[1], 1000, CLIENT_NAME);
    assert (rv != -1);

    rv = mlm_client_set_consumer (client, EVALS_STREAM, ".*");
    assert (rv != -1);
    rv = mlm_client_set_producer (client, ALERTS_STREAM);
    assert (rv != -1);

    // "<alert>@<device>" -> bool (not-used)
    std::map <std::string, bool> ack;

    while (!zsys_interrupted) {
        zmsg_t *msg = mlm_client_recv (client);
        if (!msg)
            break;
         
        if (streq (mlm_client_command (client), "STREAM DELIVER")) {
            char *alert = zmsg_popstr (msg);
            char *device = zmsg_popstr (msg);
            char *state = zmsg_popstr (msg);
            assert (alert); assert (device); assert (state);
            zmsg_destroy (&msg);
            auto search = ack.find (std::string (alert).append ("@").append (device));
            if (search == ack.end ()) {
                msg = zmsg_new ();
                zmsg_addstr (msg, alert);
                zmsg_addstr (msg, device);
                zmsg_addstr (msg, state);
                std::string send_subj (alert);
                send_subj.append ("@").append (device);
                if (mlm_client_send (client, send_subj.c_str (), &msg) != 0)
                    zsys_error ("mlm_client_send (subject = '%s') failed.", send_subj.c_str ());
                else
                    zsys_info ("TRIGGER subject='%s'", send_subj.c_str ());
            }
            else {
                zsys_info ("Alert '%s' is acknowledged.", alert);
            }
            free (alert); free (device); free (state);
        }
        else if (streq (mlm_client_command (client), "MAILBOX DELIVER")) {
            char *command = zmsg_popstr (msg);
            assert (command);
            if (streq (command, "LIST")) {
                zmsg_destroy (&msg);
                msg = zmsg_new ();
                zmsg_addstr (msg, "LIST");
                for (auto const& item : ack) {
                    zmsg_addstr (msg, item.first.c_str ());
                }
                if (mlm_client_sendto (client, "user", "LIST", NULL, 1000, &msg) != 0)
                    zsys_error ("mlm_client_sendto () failed.");
                else
                    zsys_info ("Message sent. Subject: '%s'", "LIST");
            }
            else if (streq (command, "ACK")) {
                char *alert = zmsg_popstr (msg);
                char *device = zmsg_popstr (msg);
                char *state = zmsg_popstr (msg);
                assert (alert); assert (device); assert (state);
                zmsg_destroy (&msg);
                msg = zmsg_new ();
                zmsg_addstr (msg, "ACK"); 
                if (streq (state, "ON")) {
                    ack.emplace (std::make_pair (std::string (alert).append ("@").append (device), true));
                    zmsg_addstr (msg, alert); 
                    zmsg_addstr (msg, device); 
                    zmsg_addstr (msg, "ON"); 
                }
                else if (streq (state, "OFF")) {
                    ack.erase (std::string (alert).append ("@").append (device));
                    zmsg_addstr (msg, alert); 
                    zmsg_addstr (msg, device); 
                    zmsg_addstr (msg, "OFF");
                }
                else {
                    zsys_error ("Unexpected status of alert: '%s'", state); 
                    zmsg_addstr (msg, alert); 
                    zmsg_addstr (msg, device); 
                    zmsg_addstr (msg, "ERROR");
                    zmsg_addstr (msg, std::string ("Unexpected status: ").append (state).c_str ());
                }
                if (mlm_client_sendto (client, "user", "ACK", NULL, 1000, &msg) != 0)
                    zsys_error ("mlm_client_sendto () failed.");
                else
                    zsys_info ("Message sent. Subject: '%s'", "ACK"); 
                free (alert); free (device); free (state);
            }
            else {
                zsys_error ("Unexpected message. First part (command): '%s', Sender: '%s'", command, mlm_client_sender (client));
                zmsg_destroy (&msg);
                continue;
            }
            free (command);
        }
        else {
            zsys_error ("%s does not offer any service. Sender: '%s', Subject: '%s'.",
                        argv[0], mlm_client_sender (client), mlm_client_subject (client));
            zmsg_destroy (&msg);
            continue;
        }
    }

    mlm_client_destroy (&client);
    return EXIT_SUCCESS;
}
