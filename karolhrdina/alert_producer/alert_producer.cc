#include <malamute.h>
#include <string>
#include <map>
#include "../streams.h"

#define USAGE "<mlm_endpoint>"
#define CLIENT_NAME "alert"

/*
// TODO: To be replaced by zconfig
int load (const char *file) {
    assert (file);
    zfile_t *file = zfile_new ("./", file);
}
int save (const char *file) {
}
*/

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

    std::map <std::string, bool> ack;

    while (!zsys_interrupted) {
        zmsg_t *msg = mlm_client_recv (client);
        if (!msg)
            break;
         
        if (streq (mlm_client_command (client), "STREAM DELIVER")) {
            char *alert_name = zmsg_popstr (msg);
            char *device_name = zmsg_popstr (msg);
            char *state = zmsg_popstr (msg);
            assert (alert_name); assert (device_name); assert (state);
            zmsg_destroy (&msg);
            auto search = ack.find (alert_name);
            if (search == ack.end ()) {
                msg = zmsg_new ();
                zmsg_addstr (msg, alert_name);
                zmsg_addstr (msg, device_name);
                zmsg_addstr (msg, state);
                std::string send_subj (alert_name);
                send_subj.append ("@").append (device_name);
                if (mlm_client_send (client, send_subj.c_str (), &msg) != 0)
                    zsys_error ("mlm_client_send (subject = '%s') failed.", send_subj.c_str ());
                else
                    zsys_info ("TRIGGER subject='%s'", send_subj.c_str ());
            }
            else {
                zsys_info ("Alert '%s' is acknowledged.", alert_name);
            }
            free (alert_name); free (device_name); free (state);
        }
        else if (streq (mlm_client_command (client), "MAILBOX DELIVER")) {
            const char *subject = mlm_client_subject (client);
            assert (subject);
            if (streq (subject, "LIST")) {
                zmsg_destroy (&msg);
                msg = zmsg_new ();
                for (auto const& item : ack) {
                    zmsg_addstr (msg, item.first.c_str ());
                }
                if (mlm_client_sendto (client, "user", "LIST", NULL, 1000, &msg) != 0)
                    zsys_error ("mlm_client_sendto () failed.");
                else
                    zsys_info ("Message sent. Subject: '%s'", "LIST");
            }
            else if (streq (subject, "ACK")) {
                char *alert_name = zmsg_popstr (msg);
                char *alert_status = zmsg_popstr (msg);
                assert (alert_name); assert (alert_status);
                zmsg_destroy (&msg);
                msg = zmsg_new ();
                if (streq (alert_status, "ON")) {
                    ack.emplace (std::make_pair (alert_name, true));
                    zmsg_addstr (msg, alert_name); 
                    zmsg_addstr (msg, "ACK"); 
                    zmsg_addstr (msg, "ON"); 
                }
                else if (streq (alert_status, "OFF")) {
                    ack.erase (alert_name);
                    zmsg_addstr (msg, alert_name); 
                    zmsg_addstr (msg, "ACK"); 
                    zmsg_addstr (msg, "OFF");
                }
                else {
                    zsys_error ("Unexpected status of alert: '%s'", alert_status); 
                    zmsg_addstr (msg, alert_name); 
                    zmsg_addstr (msg, "ACK"); 
                    zmsg_addstr (msg, "ERROR");
                }
                if (mlm_client_sendto (client, "user", "ACK", NULL, 1000, &msg) != 0)
                    zsys_error ("mlm_client_sendto () failed.");
                else
                    zsys_info ("Message sent. Subject: '%s'", "ACK"); 
                free (alert_name); free (alert_status);
            }
            else {
                zsys_error ("Unexpected message. Subject: '%s', Sender: '%s'", subject, mlm_client_sender (client));
                zmsg_destroy (&msg);
                continue;
            }
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
