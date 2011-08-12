/* -*- mode: C; c-basic-offset: 4  -*- */
/* ex: set shiftwidth=4 expandtab: */ 
/*
 * Copyright (c) 2010, Georgia Tech Research Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *     * Neither the name of the Georgia Tech Research Corporation nor
 *       the names of its contributors may be used to endorse or
 *       promote products derived from this software without specific
 *       prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GEORGIA TECH RESEARCH CORPORATION ''AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GEORGIA
 * TECH RESEARCH CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/** Author: jscholz, Neil Dantam
 */

#include <argp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <syslog.h>

#include <somatic.h>
#include <somatic/daemon.h>
#include <ach.h>

#include <somatic/util.h>
#include <somatic.h>

#include "js.h"
#include "jachd.h"



typedef struct {
    somatic_d_t d;
    somatic_d_opts_t d_opts;
    ach_channel_t chan;
    timer_t timer;
    const char *opt_chan_name;
    uint8_t opt_jsdev;
    int opt_verbosity;
    size_t opt_axis_cnt;
    size_t opt_button_cnt;
    struct timespec opt_period; // provide message at least every period
} cx_t;


/* ---------- */
/* ARGP Junk  */
/* ---------- */
static struct argp_option options[] = {
    {
        .name = "jsdev",
        .key = 'j',
        .arg = "device-num",
        .flags = 0,
        .doc = "specifies which device to use (default 0)"
    },
    {
        .name = "verbose",
        .key = 'v',
        .arg = NULL,
        .flags = 0,
        .doc = "Causes verbose output"
    },
    {
        .name = "channel",
        .key = 'c',
        .arg = "channel",
        .flags = 0,
        .doc = "ach channel to use (default \"joystick-data\")"
    },
    {
        .name = "axes",
        .key = 'a',
        .arg = "axis-count",
        .flags = 0,
        .doc = "number of joystick axes (default to 6)"
    },
    {
        .name = "daemonize",
        .key = 'd',
        .arg = NULL,
        .flags = 0,
        .doc = "fork off daemon process"
    },
    {
        .name = "ident",
        .key = 'I',
        .arg = "IDENT",
        .flags = 0,
        .doc = "identifier for this daemon"
    },
    {
        .name = NULL,
        .key = 0,
        .arg = NULL,
        .flags = 0,
        .doc = NULL
    }
};


/// argp parsing function
static int parse_opt( int key, char *arg, struct argp_state *state);
/// argp program version
const char *argp_program_version = "jachd-0.0.1";
/// argp program arguments documentation
static char args_doc[] = "";
/// argp program doc line
static char doc[] = "reads from linux joystick and pushes out ach messages";
/// argp object
static struct argp argp = {options, parse_opt, args_doc, doc, NULL, NULL, NULL };


static int parse_opt( int key, char *arg, struct argp_state *state) {
    cx_t *cx = (cx_t*)state->input;
    switch(key) {
    case 'j':
        cx->opt_jsdev = (uint8_t)atoi(arg);
        break;
    case 'v':
        cx->opt_verbosity++;
        break;
    case 'c':
        cx->opt_chan_name = strdup( arg );
        break;
    case 'a':
        cx->opt_axis_cnt = (size_t)atoi(arg);
        break;
    case 'I':
        cx->d_opts.ident = strdup(arg);
        break;
    case 'd':
        cx->d_opts.daemonize = 1;
        break;
    case 0:
        break;
    }
    return 0;
}

/* ------------- */
/* Function Defs */
/* ------------- */

/**
 * Block, waiting for a mouse event
 */
int jach_read_to_msg( Somatic__Joystick *msg, js_t *js )
{
    int status = js_poll_state( js );
    if( !status ) {
        size_t i;
        for( i = 0; i < msg->axes->n_data; i++ )
            msg->axes->data[i] = js->state.axes[i];
        
        for( i = 0; i < msg->buttons->n_data; i++ )
            msg->buttons->data[i] = (int64_t)js->state.buttons[i];
    }
    return status;
}



static void timer_handler(int sig) {
    // do nothing, the read will get EINTR
    (void)sig;
}

static int create_timer(cx_t *cx) {
    struct sigevent sev;
    struct itimerspec its;
    int r;
    struct sigaction sa;
    
    // setup sighandler
    memset(&sa,0,sizeof(sa));
    sa.sa_handler = timer_handler;
    sigemptyset(&sa.sa_mask);
    if( 0 != (r = sigaction(SIGUSR1, &sa, NULL)) )
        return r;

    // create  timer
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGUSR1;
    sev.sigev_value.sival_ptr = &cx->timer;
    if( 0 != (r = timer_create(CLOCK_MONOTONIC, &sev, &cx->timer)) )
        return r; 

    // start
    its.it_value = cx->opt_period;
    its.it_interval = cx->opt_period;
    if( 0 != (r = timer_settime(&cx->timer, 0, &its, NULL)) )
        return r;
   
    return 0;
}

/* ---- */
/* MAIN */
/* ---- */
int main( int argc, char **argv ) {
    static cx_t cx;
    memset(&cx, 0, sizeof(cx));

    // default options
    cx.opt_chan_name = "joystick";
    cx.d_opts.ident = "joystick";
    cx.opt_jsdev = 0;
    cx.opt_verbosity = 0;
    cx.opt_axis_cnt = 6;
    cx.opt_button_cnt = 10;
    cx.opt_period = aa_tm_sec2timespec(1.0 / 30.0);

    argp_parse (&argp, argc, argv, 0, NULL, &cx);

    //initialize
    somatic_d_init(&cx.d, &cx.d_opts);

    // Open joystick device
    js_t *js = js_open( cx.opt_jsdev );
    somatic_hard_assert( js != NULL, "Failed to open joystick device\n");

    // open channel
    somatic_d_channel_open( &cx.d, &cx.chan, 
                            cx.opt_chan_name, NULL );

    Somatic__Joystick *msg = somatic_joystick_alloc(cx.opt_axis_cnt, cx.opt_button_cnt);

    if( cx.opt_verbosity ) {
        fprintf(stderr, "\n* JSD *\n");
        fprintf(stderr, "Verbosity:    %d\n", cx.opt_verbosity);
        fprintf(stderr, "jsdev:        %d\n", cx.opt_jsdev);
        fprintf(stderr, "channel:      %s\n", cx.opt_chan_name);
        fprintf(stderr, "period:       %fs\n", aa_tm_timespec2sec(cx.opt_period));
        fprintf(stderr, "message size: %"PRIuPTR"\n", somatic__joystick__get_packed_size(msg) );
        fprintf(stderr,"-------\n");
   }

    // This gives read an EINTR when the timer expires,
    // so we can republish the message instead of blocking on the joystick read.
    // Note that blocks on ach channels take an expiration time directly, so
    // no timer would be needed for waits there.
    if( create_timer(&cx) ) {
        syslog(LOG_WARNING, "Couldn't create timer, jachd will block: %s", strerror(errno));
    }

    while (!somatic_sig_received) {
        int status = jach_read_to_msg( msg, js );
        if( !status || (EINTR == errno) ) {
            somatic_metadata_set_time_timespec( msg->meta, aa_tm_now() );
            SOMATIC_PACK_SEND( &cx.chan, somatic__joystick, msg );
        }// else check status, errno or something
    }

    // Cleanup:
    somatic_joystick_free(msg);
    somatic_d_channel_close( &cx.d, &cx.chan );
    somatic_d_destroy(&cx.d);

    js_close(js);

    return 0;
}


