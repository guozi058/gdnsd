/* Copyright © 2012 Brandon L Black <blblack@gmail.com>
 *
 * This file is part of gdnsd.
 *
 * gdnsd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * gdnsd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gdnsd.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include "gdnsd/mon.h"
#include "gdnsd/log.h"

mon_state_uint_t gdnsd_mon_get_min_state(const mon_state_t* states, const unsigned num_states) {
    dmn_assert(states);
    mon_state_uint_t lowest = MON_STATE_UP;
    for(unsigned i = 0; i < num_states; i++) {
       mon_state_uint_t st = stats_get(&states[i]);
       if(st < lowest)
           lowest = st;
    }

    return lowest;
}

/*
=== State Monitoring ===

state: enum(UNINIT, UP, DANGER, DOWN), init to UNINIT
n_failure: uint, init to 0
n_success: uint, init to 0

state_updater {
    latest_is_OK:
        state == UP      => NO-OP;
        state == DANGER  => if(++n_success == ok_thresh) state = UP;
        state == DOWN    => if(++n_success == up_thresh) state = UP;
        state == UNINIT  => state = UP;
    latest_is_BAD:
        n_success = 0;
        state == UP      => n_failure = 1; state = DANGER;
        state == DANGER  => if(++n_failure == down_thresh) state = DOWN;
        state == DOWN    => NO-OP;
        state == UNINIT  => state = DOWN;
}
*/

void gdnsd_mon_state_updater(mon_smgr_t* smgr, const bool latest) {
    dmn_assert(smgr);

    const mon_state_uint_t now_state = stats_own_get(smgr->mon_state_ptrs[0]);
    mon_state_uint_t new_state = now_state;

    // a bit spammy to leave in all debug builds, but handy at times...
    //log_debug("'%s' new monitor result: %s", smgr->desc, latest ? "OK" : "FAIL");

    if(latest) { // New Success
        switch(now_state) {
            case MON_STATE_UP:
                break;
            case MON_STATE_DANGER:
                if(++(smgr->n_success) == smgr->ok_thresh) {
                    log_info("'%s' transitioned to the UP state", smgr->desc);
                    new_state = MON_STATE_UP;
                }
                break;
            case MON_STATE_DOWN:
                if(++(smgr->n_success) == smgr->up_thresh) {
                    log_info("'%s' transitioned to the UP state", smgr->desc);
                    new_state = MON_STATE_UP;
                }
                break;
            case MON_STATE_UNINIT:
                log_info("'%s' initialized to the UP state", smgr->desc);
                new_state = MON_STATE_UP;
                break;
        }
    }
    else { // New Failure
        smgr->n_success = 0;
        switch(now_state) {
            case MON_STATE_UP:
                smgr->n_failure = 1;
                log_info("'%s' transitioned to the DANGER state", smgr->desc);
                new_state = MON_STATE_DANGER;
                break;
            case MON_STATE_DANGER:
                if(++(smgr->n_failure) == smgr->down_thresh) {
                    log_info("'%s' transitioned to the DOWN state", smgr->desc);
                    new_state = MON_STATE_DOWN;
                }
                break;
            case MON_STATE_DOWN:
                break;
            case MON_STATE_UNINIT:
                log_info("'%s' initialized to the DOWN state", smgr->desc);
                new_state = MON_STATE_DOWN;
                break;
        }
    }

    if(new_state != now_state) {
        for(unsigned i = 0; i < smgr->num_state_ptrs; i++)
            stats_own_set(smgr->mon_state_ptrs[i], new_state);
    }
}

