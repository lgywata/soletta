/*
 * This file is part of the Soletta Project
 *
 * Copyright (C) 2015 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>

#include "sol-worker-thread.h"
#include "sol-worker-thread-impl.h"

SOL_LOG_INTERNAL_DECLARE(_sol_worker_thread_log_domain, "worker-thread");

SOL_API struct sol_worker_thread *
sol_worker_thread_new(bool (*setup)(void *data),
    void (*cleanup)(void *data),
    bool (*iterate)(void *data),
    void (*cancel)(void *data),
    void (*finished)(void *data),
    void (*feedback)(void *data),
    const void *data)
{
    SOL_NULL_CHECK(iterate, NULL);
    return sol_worker_thread_impl_new(setup, cleanup, iterate,
        cancel, finished, feedback, data);
}

SOL_API void
sol_worker_thread_cancel(struct sol_worker_thread *thread)
{
    SOL_NULL_CHECK(thread);
    sol_worker_thread_impl_cancel(thread);
}

SOL_API void
sol_worker_thread_feedback(struct sol_worker_thread *thread)
{
    SOL_NULL_CHECK(thread);
    sol_worker_thread_impl_feedback(thread);
}
