/*
 * auth_pam.c:
 * authenticate using Pluggable Authentication Modules
 *
 * Copyright (c) 2001 Chris Lightfoot. All rights reserved.
 *
 */

#ifdef HAVE_CONFIG_H
#include "configuration.h"
#endif /* HAVE_CONFIG_H */

#ifdef AUTH_PAM
static const char rcsid[] = "$Id$";

#include <sys/types.h> /* BSD needs this here, it seems. */

#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include <security/pam_appl.h>

#include "auth_pam.h"
#include "authswitch.h"
#include "config.h"
#include "util.h"

/* auth_pam_conversation:
 * PAM conversation function, used to transmit the password supplied by the
 * user to the PAM modules for authentication. */
int auth_pam_conversation(int num_msg, const struct pam_message **msg, struct pam_response **resp, void *appdata_ptr) {
    const struct pam_message **m;
    struct pam_response *r;

    if (!num_msg || !msg || !appdata_ptr) return PAM_CONV_ERR;
    
    *resp = (struct pam_response*)xcalloc(num_msg, sizeof(struct pam_response));
    if (!*resp) return PAM_CONV_ERR;

    /* Assume that any prompt is asking for a password */
    for (m = msg, r = *resp; m < msg + num_msg; ++m, ++r) {
        if ((*m)->msg_style == PAM_PROMPT_ECHO_OFF) {
            r->resp = xstrdup((char*)appdata_ptr);
            r->resp_retcode = 0;
        }
    }

    return PAM_SUCCESS;
}

/* auth_pam_new_user_pass:
 * Attempt to authenticate user and pass using PAM. This is not a
 * virtual-domains authenticator, so it only looks at user. */
authcontext auth_pam_new_user_pass(const char *user, const char *local_part, const char *domain, const char *pass, const char *clienthost /* unused */, const char *serverhost) {
    pam_handle_t *pamh = NULL;
    struct passwd pw, *pw2;
    int r, n = PAM_SUCCESS;
    authcontext a = NULL;
    struct pam_conv conv;
    char *facility;
    char *s;
    int use_gid = 0;
    gid_t gid = 99;

    /* Check the this isn't a virtual-domain user. */
    if (local_part) return NULL;


    /* It is possible to use PAM to authenticate users who do not exist as
     * system users. We support this by defining an auth-pam-mail-user
     * configuration option which is used to obtain the user information
     * for a non-system user to be authenticated against PAM. */
    if (!(pw2 = getpwnam(user))) {
        char *s;
        if ((s = config_get_string("auth-pam-mail-user"))) {
            uid_t u;
            if (parse_uid(s, &u)) {
                if (!(pw2 = getpwuid(u)))
                    log_print(LOG_ERR, _("auth_pam_new_user_pass: auth-pam-mail-user directive `%s' does not correspond to a real user"), s);
            } else
                log_print(LOG_ERR, _("auth_pam_new_user_pass: auth-pam-mail-user directive `%s' does not make sense"), s);
        }

        if (!pw2)
            return NULL;
    }

    /* Copy the password structure, since it is in static storage and may
     * get overwritten by calls in the PAM code. */
    if (!(pw2 = getpwnam(user))) return NULL;
    pw = *pw2;

    /* pw now contains either the data for the real UNIX user named or the UNIX
     * user given by the auth-pam-mail-user config option. */


    /* Obtain facility name. */
    if (!(facility = config_get_string("auth-pam-facility")))
        facility = AUTH_PAM_FACILITY;

    /* Obtain gid to use */
    if ((s = config_get_string("auth-pam-mail-group"))) {
        if (!parse_gid(s, &gid)) {
            log_print(LOG_ERR, _("auth_pam_new_user_pass: auth-pam-mail-group directive `%s' does not make sense"), s);
            return NULL;
        }
        use_gid = 1;
    }

    /* This will generate a warning on Solaris; I can't see an easy fix. */
    conv.conv = auth_pam_conversation;
    conv.appdata_ptr = (void*)pass;
    
    r = pam_start(facility, user, &conv, &pamh);

    if (r != PAM_SUCCESS) {
        log_print(LOG_ERR, "auth_pam_new_user_pass: pam_start: %s", pam_strerror(pamh, r));
        return NULL;
    }
    
    /* We want to be able to test against the client IP; make the remote host
     * information available to the PAM stack. */
    r = pam_set_item(pamh, PAM_RHOST, clienthost);
    
    if (r != PAM_SUCCESS) {
        log_print(LOG_ERR, "auth_pam_new_user_pass: pam_start: %s", pam_strerror(pamh, r));
        return NULL;
    }

    /* Authenticate user. */
    r = pam_authenticate(pamh, 0);

    if (r == PAM_SUCCESS) {
        /* OK, is the account presently allowed to log in? */
        r = pam_acct_mgmt(pamh, PAM_SILENT);
        if (r == PAM_SUCCESS) {
            /* Succeeded; figure out the mailbox name later. */
            a = authcontext_new(pw.pw_uid, use_gid ? gid : pw.pw_gid, NULL, NULL, pw.pw_dir);
        } else log_print(LOG_ERR, "auth_pam_new_user_pass: pam_acct_mgmt(%s): %s", user, pam_strerror(pamh, r));
    } else log_print(LOG_ERR, "auth_pam_new_user_pass: pam_authenticate(%s): %s", user, pam_strerror(pamh, r));

    r = pam_end(pamh, n);

    if (r != PAM_SUCCESS) log_print(LOG_ERR, "auth_pam_new_user_pass: pam_end: %s", pam_strerror(pamh, r));

    return a;
}

#endif /* AUTH_PAM */
