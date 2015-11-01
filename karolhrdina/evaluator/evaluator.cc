#include <malamute.h>
#include <map>
#include <string>
#include "../streams.h"

// TODO: <mlm_endpoint> <zconfing>
//       we have simple hardcoded rules now
#define USAGE "<mlm_endpoint>"

// rules/logic is hardcoded - fine for PoC
void
eval (mlm_client_t *client, std::map <std::string, bool>& active_alerts, const char *metric, int64_t value, const char *device) {
    assert (client); assert (metric); assert (device);

    if (streq (metric, "temp")) {
        if (value > 70) {
            zmsg_t *msg = zmsg_new ();
            zmsg_addstr (msg, "ONFIRE");
            zmsg_addstr (msg, device);
            zmsg_addstr (msg, "ACTIVE");
            std::string send_subj ("ONFIRE");
            send_subj.append ("@").append (device);
            if (mlm_client_send (client, send_subj.c_str (), &msg) != 0) {
                zsys_error ("mlm_client_send (subject = '%s') failed.", send_subj.c_str ());
            }
            else {
                active_alerts.emplace (std::make_pair (std::string ("ONFIRE@").append (device), false));
                zsys_info ("TRIGGER subject='%s' state ='%s'", send_subj.c_str (), "ACTIVE"); 
            }
            return;
        }
        else {
            auto it = active_alerts.find (std::string ("ONFIRE@").append (device));
            if (it != active_alerts.end ()) {
                zmsg_t *msg = zmsg_new ();
                zmsg_addstr (msg, "ONFIRE");
                zmsg_addstr (msg, device);
                zmsg_addstr (msg, "RESOLVED");
                std::string send_subj ("ONFIRE");
                send_subj.append ("@").append (device);
                if (mlm_client_send (client, send_subj.c_str (), &msg) != 0) {
                    zsys_error ("mlm_client_send (subject = '%s') failed.", send_subj.c_str ());
                }
                else {
                    active_alerts.erase (it);
                    zsys_info ("TRIGGER subject='%s' state ='%s'", send_subj.c_str (), "RESOLVED");           
                }
            }
            return;
        }
    }       
    else if (streq (metric, "hum")) {
        if (value > 50) {
            zmsg_t *msg = zmsg_new ();
            zmsg_addstr (msg, "CORROSION");
            zmsg_addstr (msg, device);
            zmsg_addstr (msg, "ACTIVE");
            std::string send_subj ("CORROSION");
            send_subj.append ("@").append (device);
            if (mlm_client_send (client, send_subj.c_str (), &msg) != 0) {
                zsys_error ("mlm_client_send (subject = '%s') failed.", send_subj.c_str ());
            }
            else { 
                active_alerts.emplace (std::make_pair (std::string ("CORROSION@").append (device), false));
                zsys_info ("TRIGGER subject='%s' state ='%s'", send_subj.c_str (), "ACTIVE");
            }
            return; 
        }
        else {
            auto it = active_alerts.find (std::string ("CORROSION@").append (device));
            if (it != active_alerts.end ()) {
                zmsg_t *msg = zmsg_new ();
                zmsg_addstr (msg, "CORROSION");
                zmsg_addstr (msg, device);
                zmsg_addstr (msg, "RESOLVED");
                std::string send_subj ("ONFIRE");
                send_subj.append ("@").append (device);
                if (mlm_client_send (client, send_subj.c_str (), &msg) != 0) {
                    zsys_error ("mlm_client_send (subject = '%s') failed.", send_subj.c_str ());
                }
                else {
                    active_alerts.erase (it);
                    zsys_info ("TRIGGER subject='%s' state ='%s'", send_subj.c_str (), "RESOLVED");           
                }
            }
            return;           
        }
    }
    return;
}

int main (int argc, char **argv) {
    if (argc < 2) {
        zsys_error ("Usage: %s %s", argv[0], USAGE);
        return EXIT_FAILURE;
    }

    zsys_info ("mlm_endpoint: '%s'", argv[1]);

    mlm_client_t *client = mlm_client_new ();
    assert (client);

    int rv = mlm_client_connect (client, argv[1], 1000, argv[0]);
    assert (rv != -1);

    rv = mlm_client_set_consumer (client, METRICS_STREAM, ".*");
    assert (rv != -1);
    rv = mlm_client_set_producer(client, EVALS_STREAM);
    assert (rv != -1);

    //       "<alert>@<device>" -> bool (not-used) 
    std::map <std::string, bool> active_alerts;

    while (!zsys_interrupted) {
        zmsg_t *msg = mlm_client_recv (client);
        if (!msg)
            break;
        if (!streq (mlm_client_command (client), "STREAM DELIVER")) {
            zmsg_destroy (&msg);
            continue;
        }
        const char *subject = mlm_client_subject (client);
        char *metric_name = zmsg_popstr (msg);
        char *metric_value = zmsg_popstr (msg);
        assert (metric_name); assert (metric_value);
        int64_t value = std::stoi (metric_value); // TODO
        eval (client, active_alerts, metric_name, value, subject);
        free (metric_name); free (metric_value);
        zmsg_destroy (&msg); 

    }

    mlm_client_destroy (&client);
    return EXIT_SUCCESS;
}
