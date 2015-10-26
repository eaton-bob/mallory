#include <malamute.h>

static const char *stream = "ALERTS";
static const char *endpoint = "ipc://@/alerts";

/*
 * msg1: REQ alert_subject/state
 *       REP alert_subject/ACK
 *
 * msg2 (on stream "ALERTS"):
 *      PUB alert_subject/state
 *
 */

static void
s_alerts (
        zsock_t *pipe,
        void *args) {

    const char *name = "ALERT";

    mlm_client_t *cl = mlm_client_new ();
    mlm_client_connect (cl, endpoint, 5000, __PRETTY_FUNCTION__);
    mlm_client_set_producer (cl, stream);

    zsock_t *msgpipe = mlm_client_msgpipe (cl);

    zpoller_t *poller = zpoller_new (pipe, msgpipe, NULL);

    char *alert_state = strdup ("NEW");

    zsock_signal (pipe, 0);
    while (!zsys_interrupted) {
        zsock_t *which = zpoller_wait (poller, 1000);

        if (!which) {
            mlm_client_sendx (cl, "alert://upsonbattery@ups1", alert_state, NULL);
            continue;
        }

        if (which == pipe)
            break;

        //which == msgpipe
        zmsg_t *msg = mlm_client_recv (cl);
        if (!streq (mlm_client_command (cl), "MAILBOX DELIVER"))
            goto msg_destroy;

        char *alert_subject = zmsg_popstr (msg);

        zstr_free (&alert_state);
        alert_state = zmsg_popstr (msg);

        zsys_info ("%s: Alert '%s' new state is '%s'", name, alert_subject, alert_state);

        //ACK
        mlm_client_sendtox (cl, mlm_client_sender (cl), alert_subject, alert_subject, "ACK");
        zstr_free (&alert_subject);
msg_destroy:
        zmsg_destroy (&msg);
    }

    zstr_free (&alert_state);
    zpoller_destroy (&poller);
    mlm_client_destroy (&cl);
}

int main() {
    zactor_t *actor = zactor_new (s_alerts, NULL);

    //XXX: this is UGLY
    while (!zsys_interrupted) {
        zclock_sleep(100);
    }

    zactor_destroy (&actor);
}
