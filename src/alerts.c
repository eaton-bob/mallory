#include <malamute.h>
#include <stdlib.h>

static const char *stream = "ALERTS";
static const char *endpoint = "ipc://@/malamute";

static void
s_pub_alert (
        const char* name,
        mlm_client_t *cl,
        zhashx_t *alerts) {

    int idx = random() % zhashx_size (alerts);
    char *alert_state = zhashx_first (alerts);

    for (int i = 0; i != idx; i++) {
        alert_state = zhashx_next (alerts);
    }

    const char *alert_subject = zhashx_cursor (alerts);

    zsys_debug ("(%s): PUB ALERT: %s/%s", name, alert_subject, alert_state);
    mlm_client_sendx (cl, alert_subject, alert_subject, alert_state, NULL);
}

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
s_alerts (
        zsock_t *pipe,
        void *args) {

    const char *name = "ALERT";

    mlm_client_t *cl = mlm_client_new ();
    mlm_client_connect (cl, endpoint, 5000, name);
    mlm_client_set_producer (cl, stream);

    zsock_t *msgpipe = mlm_client_msgpipe (cl);

    zpoller_t *poller = zpoller_new (pipe, msgpipe, NULL);

    zhashx_t *alerts = zhashx_new ();
    zhashx_set_destructor (alerts, (zhashx_destructor_fn *) zstr_free);

    //bootstrap the alerts
    zhashx_insert (alerts, "upsonbattery@UPS1", "NEW");
    zhashx_insert (alerts, "upsonbypass@UPS2", "ACK");
    zhashx_insert (alerts, "toomuchalcohol@MVY", "RESOLVED");

    zsock_signal (pipe, 0);
    while (!zsys_interrupted) {
        zsock_t *which = zpoller_wait (poller, 1000);

        if (!which) {
            s_pub_alert (name, cl, alerts);
            continue;
        }

        if (which == pipe)
            break;

        //which == msgpipe
        zmsg_t *msg = mlm_client_recv (cl);
        zsys_debug ("received smthng on malamute");
        if (!streq (mlm_client_command (cl), "MAILBOX DELIVER"))
            goto msg_destroy;

        //LIST
        if (streq (mlm_client_subject (cl), "LIST")) {
            zsys_debug ("(%s): got command LIST", name);

            zmsg_t *msg = zmsg_new ();
            zframe_t *frame = zhashx_pack (alerts);
            zmsg_append (msg, &frame);
            mlm_client_sendto (cl, mlm_client_sender (cl), "LIST", NULL, 5000, &msg);
            goto msg_destroy;
        }

        char *alert_subject = zmsg_popstr (msg);
        zsys_debug ("alert_subject: %s", alert_subject);

        // others
        char *alert_state = zmsg_popstr (msg);
        zsys_debug ("(%s): Alert '%s' new state is '%s'", name, alert_subject, alert_state);

        zhashx_update (alerts, alert_subject, alert_state);

        //ACK
        mlm_client_sendtox (cl, mlm_client_sender (cl), alert_subject, alert_subject, "ACK");
        zstr_free (&alert_state);
        zstr_free (&alert_subject);
msg_destroy:
        zmsg_destroy (&msg);
    }

    zhashx_destroy (&alerts);
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
