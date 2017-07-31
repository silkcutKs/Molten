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

#ifndef MOLTEN_STATUS_H
#define MOLTEN_STATUS_H

#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <errno.h>

#include "php.h"
#include "SAPI.h"

#include "molten_util.h"
#include "molten_ctrl.h"
#include "php7_wrapper.h"
#include "molten_struct.h"

/* status uri */
#define STATUS_URI      "/molten/status"

/* status */
#define STATUS_FAIL     -1
#define STATUS_SUCCESS   0

/* send status info */
#define STATUS_REQUEST_SHUTDOWN  0
#define STATUS_REQUEST_CONTINUE  1

/* molten status */
typedef struct {
    char                *notify_uri;    /* notify service up */
} mo_status_t;

void mo_status_ctor(mo_status_t *mst);
void mo_request_handle(mo_status_t *mst, mo_ctrl_t *mrt TSRMLS_DC);
void mo_status_dtor(mo_status_t *mst);
#endif
