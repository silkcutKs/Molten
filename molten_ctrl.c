
/**
 * Copyright 2017 chuan-yun silkcutKs <silkcutbeta@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "molten_ctrl.h"

/* clear sock */
//static inline void clear_sock(mo_ctrl_t *prt)
//{
//    if (prt->sock > 0) {
//        close(prt->sock);
//    }
//    prt->sock = -1;
//}

/* connect sock */
//static inline int connect_sock(mo_ctrl_t *prt)
//{
//    /* init unix sock */  
//    struct sockaddr_un server;
//    struct timeval timeout;
//    int len;
//    prt->sock = socket(AF_UNIX, SOCK_STREAM, 0);
//    if (prt->sock < 0) {
//        clear_sock(prt);
//        return -1;
//    }
//
//    server.sun_family = AF_UNIX;
//    strcpy(server.sun_path, prt->domain_path);
//    len = strlen(server.sun_path) + sizeof(server.sun_family);
//
//    // set read and write timeout 
//    timeout.tv_sec = 0;
//    timeout.tv_usec = 10000;    /* set to 10 ms */
//    if (setsockopt(prt->sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
//        clear_sock(prt);
//        return -1;
//    }
//
//    if (setsockopt(prt->sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
//        clear_sock(prt);
//        return -1;
//    }
//    
//    if (connect(prt->sock, (struct sockaddr *)&server, len) < 0) {
//        clear_sock(prt);
//        return -1;
//    }
//
//    return 0;
//}

/* dispose network error */
//static inline void dispose_error(mo_ctrl_t *prt)
//{
//    /* for after error , havn`t to reconnect */
//    /* if timeout errno = EAGAIN or EWOULDBLOCK */
//    //if (errno != EAGAIN || errno != EWOULDBLOCK || errno != EINTR) {
//    if (errno != EINTR) {
//        clear_sock(prt);
//        connect_sock(prt);
//    }
//}

/* {{{ mo_ctrl_ctor */
/* use tcp(stream) to keep data stable */
/* if use udp server need use funciton recvfrom and sendto */
/* current use tcp , so we use read and write to set timeout (: */
int mo_ctrl_ctor(mo_ctrl_t *prt, mo_shm_t *msm, char *domain_path, int req_interval, long sampling_type, long sampling_rate, long sampling_request)
{
    //memset(prt->domain_path, 0x00, sizeof(prt->domain_path));
    //strcpy(prt->domain_path, domain_path);
    
    mo_ctrm_t mcm = {0, 1, sampling_type, sampling_rate, sampling_request};
    mo_repi_t mri = {0, 0};

    prt->last_req_time = 0;
    prt->req_interval = req_interval;

    /* init */
    prt->msr = pemalloc(sizeof(mo_sr_t), 1);

    /* init repi */
    prt->mcm = (mo_ctrm_t *)mo_create_slot(msm, MO_CTRL_SHM, (unsigned char *)&mcm, sizeof(mo_ctrm_t));
    prt->mri = (mo_repi_t *)mo_create_slot(msm, MO_STATUS_SHM, (unsigned char *)&mri, sizeof(mo_repi_t));

    return 0;
}
/* }}} */

/* {{{ mo_ctrl_dtor */
void mo_ctrl_dtor(mo_ctrl_t *prt)
{
    //array_free_persist(&prt->mri->capture_host);
    pefree(prt->msr, 1);
}
/* }}} */

/* {{{ check req interval already */
static int inline check_interval(mo_ctrl_t *mrt)
{
    long sec = mo_time_sec();
    if ((sec - mrt->last_req_time) > mrt->req_interval) {
        mrt->last_req_time = sec;
        return 1;
    } else {
        return 0;
    }
}
/* }}} */

/* {{{ serrialize all message */
void mo_ctrl_serialize_msg(mo_ctrl_t *mrt, char **buf)
{
    /* prometheus metrics */
    spprintf(buf, 0, 
        "# HELP molten_request_all Number of all request.\n"
        "# TYPE molten_request_all counter\n"
        "molten_request_all %ld\n"
        "# HELP molten_request_capture Number of request be capture.\n"
        "# TYPE molten_request_capture counter\n"
        "molten_request_capture %ld\n"
        "# HELP molten_sampling_type the type of sampling.\n"
        "# TYPE molten_sampling_type gauge\n"
        "molten_sampling_type %ld\n"
        "# HELP molten_sampling_rate the rate of sampling.\n"
        "# TYPE molten_sampling_rate gauge\n"
        "molten_sampling_rate %ld\n"
        "# HELP molten_sampling_request the request be capture one min.\n"
        "# TYPE molten_sampling_request gauge\n"
        "molten_sampling_request %ld\n",                                  \
        mrt->mri->request_all, mrt->mri->request_capture,               \
        mrt->mcm->sampling_type, mrt->mcm->sampling_rate,               \
        mrt->mcm->sampling_request
    );
}
/* }}} */

/* unpack simple json */
/* 
 *  {"change_time":1121212121, "language":"php", "enable":1, "sampling_rate":11, "sampling_type":1, "sampling_request":100}
 */
int mo_ctrl_update_sampling(char *rec, mo_ctrm_t *mcm)
{
    if (rec == NULL) {
        return -1;
    }

    zval ret;
    php_json_decode_ex(&ret, rec, strlen(rec), PHP_JSON_OBJECT_AS_ARRAY, 256);
    
    if (MO_Z_TYPE_P(&ret) != IS_ARRAY) {
        return -1;
    }

    HashTable *ht = Z_ARRVAL(ret);
    /* retrive date from json */
    zval *tmp;
    
    /* if here is change, we will do after */
    if (mo_zend_hash_zval_find(ht, "enable", sizeof("enable"), (void **)&tmp) == SUCCESS) {
        convert_to_long(tmp);
        if (Z_LVAL_P(tmp) == 0) {
            mcm->enable = 0;
            return 0;
        } else {
            mcm->enable = 1;
        }
    }
    
    /* determine witch sampling type */
    if (mo_zend_hash_zval_find(ht, "samplingType", sizeof("samplingType"), (void **)&tmp) == SUCCESS) {
       convert_to_long(tmp);
       mcm->sampling_type = Z_LVAL_P(tmp);
    }

    if (mcm->sampling_type == SAMPLING_RATE) {
        if (mo_zend_hash_zval_find(ht, "samplingRate", sizeof("samplingRate"), (void **)&tmp) == SUCCESS) {
            convert_to_long(tmp);
            mcm->sampling_rate = Z_LVAL_P(tmp);
        }
    } else {
        if (mo_zend_hash_zval_find(ht, "samplingRequest", sizeof("samplingRequest"), (void **)&tmp) == SUCCESS) {
            convert_to_long(tmp);
            mcm->sampling_request = Z_LVAL_P(tmp);
        }
    }

    zval_dtor(&ret);

    return 0;
}

/* {{{ record message */
void mo_ctrl_record(mo_ctrl_t *mrt, int is_sampled)
{
    /* every request will do this */
    __sync_add_and_fetch(&mrt->mri->request_all, 1);
    
    /* todo something error 
    zval *http_host;
    if (find_server_var("HTTP_HOST", sizeof("HTTP_HOST"), (void **)&http_host) == SUCCESS) {
        add_assoc_bool(&prt->mri->capture_host, (Z_STRVAL_P(http_host)), 1);
    }
    */
    

    if (is_sampled == 1) {
        __sync_add_and_fetch(&mrt->mri->request_capture, 1);
    }
}
/* }}} */

/* {{{ report and recive date */
/* define protocol linke http , clent send report info, server send control info */
/* we must avoid dead lock */
void mo_ctrl_sr_data(mo_ctrl_t *mrt)
{
    /* check interval */
    if (check_interval(mrt)) {

        /*
        int             ret;
        smart_string    rec = {0};
        smart_string    s = {0};
        char            buf[REC_DATA_SIZE] = {0};

        if (prt->sock < 0) {
            if (connect_sock(prt) < 0) {
                return;
            }
        }
        
        pack_message(&s, prt->mri);

        ret = send(prt->sock, smart_string_str(s), smart_string_len(s), 0);
        
        smart_string_free(&s);

        if (ret <= 0) {
            dispose_error(prt);
            MOLTEN_ERROR("molten report data error, errno:[%d], str:[%s]", errno, strerror(errno)); 
            return;
        } else {
            ZVAL_LONG(&prt->mri->request_capture,   0);
            ZVAL_LONG(&prt->mri->request_all,       0);
        }

        do{
            ret = recv(prt->sock, buf, REC_DATA_SIZE, 0);
            if (ret > 0) {
                smart_string_appendl(&rec, buf, ret);
                if (ret < REC_DATA_SIZE) {
                    smart_string_0(&rec);
                    break;
                }
            }
        }while(ret > 0);


        if (ret <= 0) {
            dispose_error(prt);
            MOLTEN_ERROR("molten read data error, errno:[%d], str:[%s]", errno, strerror(errno)); 
            return;
        }

        unpack_message(&rec, prt->mcm);
        smart_string_free(&rec);
    */
    }
}
/* }}} */

/* {{{ control sampling module,  must after is_cli judege */
void mo_ctrl_sampling(mo_ctrl_t *prt, mo_chain_t *pct)
{
    /* determine sample or not */
    /* if not sampled this function will return direct */
    zval *sampled = NULL;
    int sampled_ret = FAILURE;
    if (pct->is_cli != 1) {
        sampled_ret = find_server_var(MOLTEN_REC_SAMPLED, sizeof(MOLTEN_REC_SAMPLED), (void **)&sampled);
    }

    /* default is 0 */
    pct->pch.is_sampled = 0;
     
    if ((sampled_ret == SUCCESS) && (sampled != NULL) && (Z_TYPE_P(sampled) == IS_STRING) ) {
        if (strncmp(Z_STRVAL_P(sampled), "1", 1) == 0) {
            pct->pch.is_sampled = 1;
        } else {
            pct->pch.is_sampled = 0;
        }
    } else {

        /* sampling rate */
        if (prt->mcm->sampling_type == SAMPLING_RATE) {
            if (check_hit_ratio(prt->mcm->sampling_rate)) {
                pct->pch.is_sampled = 1;
            } else {
                pct->pch.is_sampled = 0;
            }
        } else {

            /* sampling by r/m */
            /* todo here can use share memory for actural data, but use memory lock will effect performance */
            long min = mo_time_m();
            if (min == prt->msr->last_min) {
                prt->msr->request_num++; 
            } else {
                prt->msr->request_num = 0;
                prt->msr->last_min = min;
            }

            if (prt->msr->request_num < prt->mcm->sampling_request) {
                pct->pch.is_sampled = 1;
            } else {
                pct->pch.is_sampled = 0;
            }
        }  
    }
}
/* }}} */
