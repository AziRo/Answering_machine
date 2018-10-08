#include <pjsua-lib/pjsua.h>
//#include <pjlib.h>

#define THIS_APP "SIP_ANS"

#define SIP_DOMAIN "192.168.0.106"
#define SIP_USER "111"
#define PORT 5083

#define MAX_COUNT 1
#define DELAY 2


pj_pool_t *pool;
pjmedia_port *m_port;
pjsua_conf_port_id ring_slot;
pjmedia_tone_desc tones;


static void timer_callback(pj_timer_heap_t *ht, pj_timer_entry *e)
{
    PJ_UNUSED_ARG(ht);
    PJ_UNUSED_ARG(e);

    PJ_LOG(4,(THIS_APP, "----------Timer callback started!---------"));
}


void timer(int sec, int msec)
{
    pj_status_t status;

    pj_size_t size = pj_timer_heap_mem_size(MAX_COUNT);

    pj_pool_t* pool = pjsua_pool_create(NULL, size, 1024);

    if (!pool) {
        PJ_LOG(3,(THIS_APP, "Error: unable to create pool of %u bytes",
                  size));
        return;
    }

    pj_timer_entry *entry;
    entry = (pj_timer_entry*)pj_pool_calloc(pool, MAX_COUNT, sizeof(*entry));

    if (!entry) {
        PJ_LOG(3,(THIS_APP, "pj_pool_calloc failed!"));
        return;
    }

    pj_timer_entry_init(entry, 144, NULL, &timer_callback);

    pj_timer_heap_t* timer;
    status = pj_timer_heap_create(pool, MAX_COUNT, &timer);

    if(status != PJ_SUCCESS) {
        char errmsg[PJ_ERR_MSG_SIZE];
        pj_strerror(status, errmsg, sizeof(errmsg));
        PJ_LOG(3,(THIS_APP, "Error: unable to create timer heap: %s", errmsg));
        return;
    }

    pj_time_val delay;
    delay.sec = sec;
    delay.msec = msec;

    status = pj_timer_heap_schedule(timer, entry, &delay);
    if (status != 0) {
        char errmsg[PJ_ERR_MSG_SIZE];
        pj_strerror(status, errmsg, sizeof(errmsg));
        PJ_LOG(3,(THIS_APP, "Error: unable to schedule timer: %s", errmsg));
        return;
    }
    pj_time_val next_delay;
    do {
        pj_thread_sleep(500);
        pj_timer_heap_poll(timer, &next_delay);
    } while (pj_timer_heap_count(timer) > 0);

    pj_pool_release(pool);
}


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

    pj_thread_sleep(2500);
    //timer(2, 500);

    /* 200 OK */
    pjsua_call_answer(call_id, 200, NULL, NULL);
}


static void on_call_media_state(pjsua_call_id call_id)
{
    pjsua_call_info call_info;

    pjsua_call_get_info(call_id, &call_info);

    if (call_info.media_status == PJSUA_CALL_MEDIA_ACTIVE) {
        pjsua_conf_connect(ring_slot, call_info.conf_slot);
        pjsua_conf_connect(call_info.conf_slot, 0);
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
        cfg.port = PORT;
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

    if(status != 0) {
        char errmsg[PJ_ERR_MSG_SIZE];
        pj_strerror(status, errmsg, sizeof(errmsg));
        PJ_LOG(3,(THIS_APP,"Error: unable to schedule timer heap: %s", errmsg));
        return -1;
    }

    /* Tonegen */
    pool = pjsua_pool_create(THIS_APP, 1000, 1000);
    status = pjmedia_tonegen_create(pool, 8000, 1, 160, 16,
                                    PJMEDIA_TONEGEN_LOOP, &m_port);
    if (status != PJ_SUCCESS) {
        pjsua_perror(THIS_APP, "Unable to create tonegen",status);
    }

    tones.freq1 = 425;
    tones.on_msec = 1000;
    tones.off_msec = 4000;
    tones.volume = PJMEDIA_TONEGEN_VOLUME;
    tones.flags = 0;

    status = pjsua_conf_add_port(pool, m_port, &ring_slot);
    pj_assert(status == PJ_SUCCESS);
    status = pjmedia_tonegen_play(m_port, 1, &tones, PJMEDIA_TONEGEN_LOOP);
    pj_assert(status == PJ_SUCCESS);

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
