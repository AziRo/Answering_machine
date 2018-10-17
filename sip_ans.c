#include <pjsua-lib/pjsua.h>

#define THIS_APP "SIP_ANS"

#define SIP_DOMAIN "192.168.23.5"
#define SIP_USER "111"
#define PORT 5083

#define NUMBER1 "1111"
#define NUMBER2 "2222"
#define NUMBER3 "3333"
#define NUMBER4 "4444"


#define MAX_CLIENT 20

#define MAX_COUNT 1
#define DELAY 2

#define LOOP_WAV_FILE "./trentemoller-miss_you.wav"
#define WAV_FILE "./Sound.wav"


typedef struct tonegen_i {
    pj_pool_t *pool;
    pjmedia_port *m_port;
    pjsua_conf_port_id ring_slot;
    pjmedia_tone_desc tones;
} tonegen_i;

typedef struct player_i {
    pj_str_t wav_file;
    pjsua_player_id player_id;
    pjsua_conf_port_id wav_slot;
    pjmedia_port *m_port;
} player_i;

typedef struct calls_info {
    pjsua_call_id call_id;
    pjsua_call_info call_info;
    player_i player;
    pj_str_t to;
    unsigned int ind;
    int lock;
} calls_info;


calls_info c_info[MAX_CLIENT];

pj_str_t c_num1, c_num2, c_num3, c_num4;

tonegen_i tonegen[2];
player_i loop_player;


pj_status_t player_callback(pjmedia_port *m_port, void *user_data)
{
    calls_info c_i = *((calls_info*)user_data);
    c_info[c_i.ind].lock = 0;
    return pjsua_call_hangup(c_i.call_id, 406, NULL, NULL);
}


void update_info(){
    for(int i = 0; i < MAX_CLIENT; ++i) {
        if (c_info[i].lock) {
            if(!pjsua_call_is_active(c_info[i].call_id)) {
                if(c_info[i].player.m_port != NULL) {
                    pjsua_player_destroy(c_info[i].player.player_id);
                    c_info[i].player.m_port = NULL;
                }
                c_info[i].lock = 0;
            }
        }
    }
}

int find_unlock() {
    for (int i = 0; i < MAX_CLIENT; ++i) {
        update_info();
        if(!c_info[i].lock) return i;
    }
    return -1;
}


static void timer_callback(pj_timer_heap_t *ht, pj_timer_entry *e)
{
    PJ_LOG(4,(THIS_APP, "----------Timer callback started!---------"));
    PJ_UNUSED_ARG(ht);
    pj_status_t status;

    int ind = e->id;

    /* 200 OK */
    pjsua_call_answer(c_info[ind].call_id, 200, NULL, NULL);

    pjsua_call_info call_info;
    pjsua_call_get_info(c_info[ind].call_id, &call_info);

    if (call_info.media_status == PJSUA_CALL_MEDIA_ACTIVE) {

        if(!pj_strcmp(&c_info[ind].to, &c_num1)) {
            pjsua_conf_connect(loop_player.wav_slot, call_info.conf_slot);
        }
        else if(!pj_strcmp(&c_info[ind].to, &c_num2)) {
            pjsua_conf_connect(tonegen[0].ring_slot, call_info.conf_slot);
        }
        else if(!pj_strcmp(&c_info[ind].to, &c_num3)) {
            pjsua_conf_connect(tonegen[1].ring_slot, call_info.conf_slot);
        }
        else {
            pj_cstr(&c_info[ind].player.wav_file, WAV_FILE);
            status = pjsua_player_create(&c_info[ind].player.wav_file,
                                         PJMEDIA_FILE_NO_LOOP,
                                         &c_info[ind].player.player_id);

            if (status != PJ_SUCCESS) {
                pjsua_perror(THIS_APP, "Unable to create WAV player", status);
            }

            c_info[ind].player.wav_slot = pjsua_player_get_conf_port(
                                                  c_info[ind].player.player_id);

            pjsua_player_get_port(c_info[ind].player.player_id,
                                  &c_info[ind].player.m_port);

            pjmedia_wav_player_set_eof_cb(c_info[ind].player.m_port,
                                          (void*)&c_info[ind], player_callback);

            pjsua_conf_connect(c_info[ind].player.wav_slot,call_info.conf_slot);
        }

    }
}


void timer(int sec, int msec, int ind)
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

    pj_timer_entry_init(entry, ind, NULL, &timer_callback);

    pj_time_val delay;
    delay.sec = sec;
    delay.msec = msec;

    status = pjsua_schedule_timer(entry, &delay);
    if (status != 0) {
        char errmsg[PJ_ERR_MSG_SIZE];
        pj_strerror(status, errmsg, sizeof(errmsg));
        PJ_LOG(3,(THIS_APP, "Error: unable to schedule timer: %s", errmsg));
        return;
    }

}


static void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
                             pjsip_rx_data *rdata)
{
    pjsua_call_info call_info;

    pjsua_call_get_info(call_id, &call_info);
    int client_id = find_unlock();
    if(client_id == -1) {
      /* 503 Service Unavailable */
      pjsua_call_answer(call_id, 503, NULL, NULL);
      return;
    };
    pj_pool_t *pool;
    pool = pjsua_pool_create(NULL, 512, 0);
    c_info[client_id].to.ptr = (char*) pj_pool_alloc(pool, 256);

    pj_str_t str = call_info.local_info;
    pj_strcpy(&c_info[client_id].to, &str);

    PJ_LOG(3, (THIS_APP, "Incoming call to: %.*s",
             (int)c_info[client_id].to.slen, c_info[client_id].to.ptr));
    PJ_LOG(3,(THIS_APP, "Incoming call from %.*s",
             (int)call_info.remote_info.slen, call_info.remote_info.ptr));

    /* 180 Ringing */
    pjsua_call_answer(call_id, 180, NULL, NULL);

    c_info[client_id].call_id = call_id;
    c_info[client_id].call_info = call_info;
    c_info[client_id].lock = 1;

    timer(2, 500, client_id);
}


static void on_call_media_state(pjsua_call_id call_id)
{

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

    c_num1 = pj_str("<sip:" NUMBER1 "@" SIP_DOMAIN ">");
    c_num2 = pj_str("<sip:" NUMBER2 "@" SIP_DOMAIN ">");
    c_num3 = pj_str("<sip:" NUMBER3 "@" SIP_DOMAIN ">");
    c_num4 = pj_str("<sip:" NUMBER4 "@" SIP_DOMAIN ">");

    {
        pjsua_config cfg;
        pjsua_logging_config log_cfg;

        pjsua_config_default(&cfg);
        cfg.cb.on_incoming_call = &on_incoming_call;
        cfg.cb.on_call_media_state = &on_call_media_state;
        cfg.cb.on_call_state = NULL;
        cfg.max_calls = MAX_CLIENT;


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

    pjsua_set_null_snd_dev();

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

    /* Tonegen  0 */
    tonegen[0].pool = pjsua_pool_create(THIS_APP, 1000, 1000);
    status = pjmedia_tonegen_create(tonegen[0].pool, 8000, 1, 160, 16,
                                    PJMEDIA_TONEGEN_LOOP, &tonegen[0].m_port);
    if (status != PJ_SUCCESS) {
        pjsua_perror(THIS_APP, "Unable to create tonegen",status);
    }

    tonegen[0].tones.freq1 = 425;
    tonegen[0].tones.on_msec = 1000;
    tonegen[0].tones.off_msec = 4000;
    tonegen[0].tones.volume = PJMEDIA_TONEGEN_VOLUME;
    tonegen[0].tones.flags = 0;

    status = pjsua_conf_add_port(tonegen[0].pool, tonegen[0].m_port,
                                 &tonegen[0].ring_slot);
    pj_assert(status == PJ_SUCCESS);
    status = pjmedia_tonegen_play(tonegen[0].m_port, 1, &tonegen[0].tones,
                                  PJMEDIA_TONEGEN_LOOP);
    pj_assert(status == PJ_SUCCESS);

    /* Tonegen  1 */
    tonegen[1].pool = pjsua_pool_create(THIS_APP, 1000, 1000);
    status = pjmedia_tonegen_create(tonegen[1].pool, 8000, 1, 160, 16,
                                    PJMEDIA_TONEGEN_LOOP, &tonegen[1].m_port);
    if (status != PJ_SUCCESS) {
        pjsua_perror(THIS_APP, "Unable to create tonegen",status);
    }

    tonegen[1].tones.freq1 = 425;
    tonegen[1].tones.on_msec = 4000;
    tonegen[1].tones.off_msec = 0;
    tonegen[1].tones.volume = PJMEDIA_TONEGEN_VOLUME;
    tonegen[1].tones.flags = 0;

    status = pjsua_conf_add_port(tonegen[1].pool, tonegen[1].m_port,
                                 &tonegen[1].ring_slot);
    pj_assert(status == PJ_SUCCESS);
    status = pjmedia_tonegen_play(tonegen[1].m_port, 1, &tonegen[1].tones,
                                  PJMEDIA_TONEGEN_LOOP);
    pj_assert(status == PJ_SUCCESS);

    /* WAV player */
    pj_cstr(&loop_player.wav_file, LOOP_WAV_FILE);
    status=pjsua_player_create(&loop_player.wav_file,
                               !PJMEDIA_FILE_NO_LOOP,
                               &loop_player.player_id);
    if (status != PJ_SUCCESS) {
        pjsua_perror(THIS_APP, "Unable to create WAV player", status);
    }

    loop_player.wav_slot = pjsua_player_get_conf_port(loop_player.player_id);
    pjsua_player_get_port(loop_player.player_id, &loop_player.m_port);
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
