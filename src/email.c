#include <malamute.h>

static const char *stream = "ALERTS";
static const char *endpoint = "ipc://@/malamute";

/*
 * msg1: REQ alert_subject/state
 *       REP alert_subject/ACK
 *
 * msg2: REQ LIST
 *       REP LIST/zhashx_t *alerts
 *
 * msg2 (on stream "ALERTS"):
 *      PUB alert_subject/state
 *
 */

static void
s_email (
        zsock_t *pipe,
        void *args) {

    const char *name = "EMAIL";

    mlm_client_t *cl = mlm_client_new ();
    mlm_client_connect (cl, endpoint, 5000, name);
    mlm_client_set_consumer (cl, stream, ".*");

    zsock_t *msgpipe = mlm_client_msgpipe (cl);

    zpoller_t *poller = zpoller_new (pipe, msgpipe, NULL);

    zhashx_t *email_sent = zhashx_new ();

    zsock_signal (pipe, 0);
    while (!zsys_interrupted) {
        zsock_t *which = zpoller_wait (poller, 1000);

        if (!which)
            continue;

        if (which == pipe)
            break;

        //which == msgpipe
        zmsg_t *msg = mlm_client_recv (cl);
        if (!streq (mlm_client_command (cl), "STREAM DELIVER"))
            goto msg_destroy;

        char *alert_subject = zmsg_popstr (msg);
        char *alert_state = zmsg_popstr (msg);

        if (!streq (alert_state, "NEW"))
            goto str_destroy;

        if (!zhashx_lookup (email_sent, alert_subject)) {
            zsys_info ("Email %s sent", alert_subject);
            zhashx_update (email_sent, alert_subject, "");
        }

str_destroy:
        zstr_free (&alert_state);
        zstr_free (&alert_subject);
msg_destroy:
        zmsg_destroy (&msg);
    }

    zhashx_destroy (&email_sent);
    zpoller_destroy (&poller);
    mlm_client_destroy (&cl);
}

int main() {
    zactor_t *actor = zactor_new (s_email, NULL);

    //XXX: this is UGLY
    while (!zsys_interrupted) {
        zclock_sleep(100);
    }

    zactor_destroy (&actor);
}

