/*
 * authswitch.h:
 * authentication drivers
 *
 * Copyright (c) 2001 Chris Lightfoot.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __AUTHSWITCH_H_ /* include guard */
#define __AUTHSWITCH_H_

#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include "mailbox.h"

typedef struct _authcontext {
    uid_t uid;
    gid_t gid;
    char *mboxdrv, *mailbox;    /* Name of mailbox driver and mailbox. */
    
    char *auth;                 /* Name of authentication driver, eg `pam'. */
    char *user, *home;          /* Name of user as supplied to POP server, and home directory if applicable. */
    char *local_part, *domain;  /* Local part and domain name. */
} *authcontext;

/* Authentication:
 * Authenticators are passed the username, which is what the client supplies;
 * and perhaps a local-part and domain, which are either the split-up version
 * of the username, if it contains a separating @, % or !, or the user and the
 * domain associated with the connection. */

struct authdrv {
    /* Initialise this authentication driver. Returns 1 on success or 0 on
     * failure. */
    int         (*auth_init)(void);
    
    /* Attempt to build authcontext from APOP; parameters are name, original
     * timestamp, supplied digest and the client host. */
    authcontext (*auth_new_apop)(const char *user, const char *local_part, const char *domain, const char *timestamp, const unsigned char *digest, const char *clienthost, const char *serverhost);
    
    /* Attempt to build authcontext from USER and PASS; parameters are name,
     * password and the client host. */
    authcontext (*auth_new_user_pass)(const char *user, const char *local_part, const char *domain, const char *password, const char *clienthost, const char *serverhost);

    /* Function to call after any successful authentication. */
    void        (*auth_onlogin)(const authcontext A, const char *clienthost, const char *serverhost);

    /* Clear up any resources associated with this driver prior to a fork. */
    void        (*auth_postfork)(void);

    /* Shut down this authentication driver, and free associated resources. */
    void        (*auth_close)(void);

    /* Name of the authentication driver (should be one word). */
    char *name;

    /* Description of the authentication driver. */
    char *description;
};

char *username_string(const char *user, const char *local_part, const char *domain);

void authswitch_describe(FILE *fp);

int authswitch_init(void);
authcontext authcontext_new_apop(const char *name, const char *local_part, const char *domain, const char *timestamp, const unsigned char *digest, const char *clienthost, const char *serverhost);
authcontext authcontext_new_user_pass(const char *user, const char *local_part, const char *domain, const char *pass, const char *clienthost, const char *serverhost);

void authswitch_onlogin(const authcontext A, const char *clienthost, const char *serverhost);
void authswitch_postfork(void);
void authswitch_close(void);

authcontext authcontext_new(const uid_t uid, const gid_t gid, const char *mboxdrv, const char *mailbox, const char *home);
void authcontext_delete(authcontext);

/* Function to find a mailbox according to the config file. */
mailbox find_mailbox(authcontext a);

/* Authentication cache. */
void authcache_init(void);
void authcache_close(void);
authcontext authcache_new_user_pass(const char *user, const char *local_part, const char *domain, const char *pass, const char *clienthost, const char *serverhost);
void authcache_save(authcontext A, const char *user, const char *local_part, const char *domain, const char *pass, const char *clienthost, const char *serverhost);

#endif /* __AUTHSWITCH_H_ */
