#include <pjsua-lib/pjsua.h>

#define THIS_APP "SIP_ANS"

#define SIP_DOMAIN "192.168.0.106"
#define SIP_USER "111"


static void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
                             pjsip_rx_data *rdata)
{
    pjsua_call_info call_info;

    PJ_UNUSED_ARG(acc_id);
    PJ_UNUSED_ARG(rdata);

    pjsua_call_get_info(call_id, &call_info);

    PJ_LOG(3,(THIS_APP, "Incoming call from %.*s",
             (int)call_info.remote_info.slen, call_info.remote_info.ptr));

    /* 180 Ringing */
    pjsua_call_answer(call_id, 180, NULL, NULL);

    /* 200 OK */
    pjsua_call_answer(call_id, 200, NULL, NULL);
}


static void on_call_media_state(pjsua_call_id call_id)
{
    pjsua_call_info call_info;

    pjsua_call_get_info(call_id, &call_info);

    if (call_info.media_status == PJSUA_CALL_MEDIA_ACTIVE) {
        pjsua_conf_connect(call_info.conf_slot, 0);
        pjsua_conf_connect(0, call_info.conf_slot);
    }
}


int main(int argc, char *argv[])
{
    pjsua_acc_id acc_id;
    pj_status_t status;

    status = pjsua_create();
    if (status != PJ_SUCCESS) {
        pjsua_perror(THIS_APP, "Error in pjsua_create()", status);
        pjsua_destroy();
        return -1;
    }

    {
        pjsua_config cfg;
        pjsua_logging_config log_cfg;

        pjsua_config_default(&cfg);
        cfg.cb.on_incoming_call = &on_incoming_call;
        cfg.cb.on_call_media_state = &on_call_media_state;

        pjsua_logging_config_default(&log_cfg);
        log_cfg.console_level = 4;

        status = pjsua_init(&cfg, &log_cfg, NULL);
        if (status != PJ_SUCCESS) {
            pjsua_perror(THIS_APP, "Error in pjsua_init()", status);
            pjsua_destroy();
            return -1;
        }
    }

    {
        pjsua_transport_config cfg;
        pjsua_transport_config_default(&cfg);
        cfg.port = 5083;
        status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &cfg, NULL);
        if (status != PJ_SUCCESS) {
            pjsua_perror(THIS_APP, "Error creating transport", status);
            pjsua_destroy();
            return -1;
        }
    }

    status = pjsua_start();
    if (status != PJ_SUCCESS) {
        pjsua_perror(THIS_APP, "Error starting pjsua", status);
    }

    {
        pjsua_acc_config cfg;

        pjsua_acc_config_default(&cfg);
        cfg.id = pj_str("sip:" SIP_USER);
        cfg.reg_uri = pj_str("sip:" SIP_USER "@" SIP_DOMAIN);
        cfg.cred_count = 1;
        cfg.register_on_acc_add = PJ_FALSE;

        status = pjsua_acc_add(&cfg, PJ_TRUE, &acc_id);
        if (status != PJ_SUCCESS) {
            pjsua_perror(THIS_APP, "Error adding account", status);
        }
    }


    while(1) {
        char option[10];

        puts("Press 'h' to hangup all calls, 'q' to quit");

        if (fgets(option, sizeof(option), stdin) == NULL) {
            pjsua_destroy();
            return 0;
        }

        switch (option[0]) {
            case 'q':
                pjsua_destroy();
                return 0;
            case 'h':
                pjsua_call_hangup_all();
                break;
            default:
                puts("Unknown option.");
        }
    }

    return 0;
}
