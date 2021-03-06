/*
 * auth_ldap.c:
 * Authenticate users against a LDAP server.
 *
 * designed for tpop3d by Sebastien THOMAS (prune@lecentre.net) - Mad Cow tribe
 * Copyright (c) 2002 Sebastien Thomas, Chris Lightfoot.
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
 * 
 */

#ifdef HAVE_CONFIG_H
#include "configuration.h"
#endif /* HAVE_CONFIG_H */

#ifdef AUTH_LDAP
static const char rcsid[] = "$Id$";

#include <sys/types.h> /* BSD needs this here, apparently. */

/* Some of the LDAP APIs we're using are deprecated, sadly. Without this they
 * don't get defined in the header files. */
#define LDAP_DEPRECATED 1

#include <lber.h>
#include <ldap.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "auth_ldap.h"
#include "authswitch.h"
#include "config.h"
#include "stringmap.h"
#include "util.h"

/* ldapinfo:
 * Information relating to the LDAP server and queries against same. */
static struct {
    char *hostname;
    int port;
    char *dn, *searchdn, *password;
    uid_t uid;
    gid_t gid;
    int tls;
    char *filter_spec;
    int scope;
    struct {
        char *mailbox, *mboxtype, *user, *group;
    } attr;
    LDAP *ldap;
} ldapinfo = {
        NULL,               /* no default host */
        LDAP_PORT,          /* default port */
        NULL,               /* dn */
        NULL,               /* no default search dn */
        NULL,               /* or password */
        -1, -1,             /* no default user/group */
        0,                  /* don't use TLS */
        "(mail=$(local_part)@$(domain))",
                            /* default filter matches complete email address
                             * to mail attribute */
        LDAP_SCOPE_SUBTREE, /* search subtree by default. */
        {
            NULL,           /* attribute from which to obtain mailbox location */
            NULL,           /*    by default, guess mailbox type. */
            NULL,           /*    user id */
            NULL,           /*    group id */
        },
        NULL
    };

static char *substitute_filter_params(const char *template, const char *user, const char *local_part, const char *domain);

/* auth_ldap_connect:
 * Try to connect to the LDAP server. */
static int auth_ldap_connect(void) {
    int r = 1;

    if (!(ldapinfo.ldap = ldap_open(ldapinfo.hostname, ldapinfo.port))) {
        log_print(LOG_ERR, "auth_ldap_connect: ldap_open: %m");
        return 0;
    }
    
    if (ldapinfo.tls) {
        int vers, ret;

        vers = LDAP_VERSION3;
        if ((ret = ldap_set_option(ldapinfo.ldap, LDAP_OPT_PROTOCOL_VERSION, &vers)) != LDAP_OPT_SUCCESS) {
            log_print(LOG_ERR, "auth_ldap_connect: ldap_set_option(LDAP_VERSION3): %s", ldap_err2string(ret));
            r = 0;
        } else if ((ret = ldap_start_tls_s(ldapinfo.ldap, NULL, NULL)) != LDAP_SUCCESS) {
            log_print(LOG_ERR, "auth_ldap_connect: ldap_start_tls_s: %s", ldap_err2string(ret));
            r = 0;
        }
    }

    if (!r) {
        ldap_unbind(ldapinfo.ldap);
        ldapinfo.ldap = NULL;
    }
    
    return r;
}

/* auth_ldap_init:
 * Read configuration directives relating to LDAP and save them in the
 * ldapinfo structure. */
extern int verbose; /* in main.c */

int auth_ldap_init(void) {
    char *ldap_url = NULL, *s, *t;
    int ret = 0, r = 0;
    LDAPURLDesc *urldesc = NULL;

    /* get the data from an ldap_url string */
    if (!(ldap_url = config_get_string("auth-ldap-url"))) {
        log_print(LOG_ERR, _("auth_ldap_init: no auth-ldap-url directive in config"));
        goto fail;
    }

    /* Find hostname and port from ldap url */
    if ((ret = ldap_url_parse(ldap_url, &urldesc)) != LDAP_URL_SUCCESS) {
        log_print(LOG_ERR, _("auth_ldap_init: %s: URL error %d"), ldap_url, ret);
        goto fail;
    }
    
    ldapinfo.hostname = xstrdup(urldesc->lud_host);
    
    /* If no port is specified, use the default. */
    if (urldesc->lud_port)
        ldapinfo.port = urldesc->lud_port;

    if (!(ldapinfo.port = urldesc->lud_port))
        ldapinfo.port = LDAP_PORT;

    if (urldesc->lud_dn)
        ldapinfo.dn = xstrdup(urldesc->lud_dn);

    ldap_free_urldesc(urldesc);

    if (verbose)
        log_print(LOG_DEBUG, _("auth_ldap_init: using DN %s on %s:%d"), ldapinfo.dn ? ldapinfo.dn : "n/a", ldapinfo.hostname, ldapinfo.port);

    /* Obtain search DN and password used to connect to the server. */
    if (!(ldapinfo.searchdn = config_get_string("auth-ldap-searchdn"))) {
        log_print(LOG_ERR, _("auth_ldap_init: no auth-ldap-searchdn directive in config"));
        goto fail;
    } else if (!(ldapinfo.password = config_get_string("auth-ldap-password"))) {
        log_print(LOG_ERR, _("auth_ldap_init: no auth-ldap-password directive in config; anonymous bind is not permitted"));
        goto fail;
    }
    
    /* Filter substitution string. */
    if ((s = config_get_string("auth-ldap-filter")))
        ldapinfo.filter_spec = xstrdup(s);
    else
        log_print(LOG_WARNING, _("auth_ldap_init: using default auth-ldap-filter `%s'"), ldapinfo.filter_spec);

    if ((s = config_get_string("auth-ldap-scope"))) {
        if (strcasecmp(s, "subtree") == 0)
            ldapinfo.scope = LDAP_SCOPE_SUBTREE;
        else if (strcasecmp(s, "base") == 0)
            ldapinfo.scope = LDAP_SCOPE_BASE;
        else if (strcasecmp(s, "onelevel") == 0)
            ldapinfo.scope = LDAP_SCOPE_ONELEVEL;
        else
            log_print(LOG_WARNING, _("auth_ldap_init: unknown scope specification `%s'; using default, `subtree'"), s);
    }

    /* Mailbox locations, or attribute which specifies it. */
    s = config_get_string("auth-ldap-mailbox");
    t = config_get_string("auth-ldap-mailbox-attr");
    if (!s && t) {
        ldapinfo.attr.mailbox = xstrdup(t);
        if ((s = config_get_string("auth-ldap-mboxtype-attr")))
            ldapinfo.attr.mboxtype = xstrdup(s);
        else
            log_print(LOG_WARNING, _("auth_ldap_init: will guess mailbox types based upon filename"), ldapinfo.attr.mailbox);
    } else if (s && t) {
        log_print(LOG_ERR, _("auth_ldap_init: both an auth-ldap-mailbox and an auth-ldap-mailbox-attr directive were specified"));
        goto fail;
    } 
    
    
    /* The UID and GID used to access the mailbox may be specified in the
     * configuration file or in the directory. */
    s = config_get_string("auth-ldap-mail-user");
    t = config_get_string("auth-ldap-mail-user-attr");
    if (s && !t) {
        if (!parse_uid(s, &ldapinfo.uid)) {
            log_print(LOG_ERR, _("auth_ldap_init: auth-ldap-mail-user directive `%s' does not make sense"), s);
            goto fail;
        }
    } else if (!s && t)
        ldapinfo.attr.user = xstrdup(t);
    else if (s && t) {
        log_print(LOG_ERR, _("auth_ldap_init: both an auth-ldap-mail-user and an auth-ldap-mail-user-attr directive were specified"));
        goto fail;
    } else {
        log_print(LOG_ERR, _("auth_ldap_init: neither an auth-ldap-mail-user nor an auth-ldap-mail-user-attr directive was specified"));
        goto fail;
    }

    s = config_get_string("auth-ldap-mail-group");
    t = config_get_string("auth-ldap-mail-group-attr");
    if (s && !t) {
        if (!parse_gid(s, &ldapinfo.gid)) {
            log_print(LOG_ERR, _("auth_ldap_init: auth-ldap-mail-group directive `%s' does not make sense"), s);
            goto fail;
        }
    } else if (!s && t)
        ldapinfo.attr.group = xstrdup(t);
    else if (s && t) {
        log_print(LOG_ERR, _("auth_ldap_init: both an auth-ldap-mail-group and an auth-ldap-mail-group-attr directive were specified"));
        goto fail;
    } else {
        log_print(LOG_ERR, _("auth_ldap_init: neither an auth-ldap-mail-group nor an auth-ldap-mail-group-attr directive was specified"));
        goto fail;
    }

    /* Do we use TLS to connect to the server? */
    if (config_get_bool("auth-ldap-use-tls"))
        ldapinfo.tls = 1;

    r = 1;

fail:
    return r;
}

extern int verbose; /* in main.c */

/* ldap_strerror:
 * Return the current error string from the LDAP library. */
static char *ldap_strerror(void) {
    int ld_errno;
    ldap_get_option(ldapinfo.ldap, LDAP_OPT_ERROR_NUMBER, &ld_errno);
    return ldap_err2string(ld_errno);
}

/* try_ldap_connect_bind:
 * Try to connect to the LDAP server and bind. */
static int try_ldap_connect_bind(const char *who, const char *passwd) {
    int ret = LDAP_OTHER, i;    /* XXX */
    for (i = 0; i < 3; ++i) {
        if (ldapinfo.ldap || auth_ldap_connect()) {
            ret = ldap_simple_bind_s(ldapinfo.ldap, who, passwd);
            if (ret == LDAP_SUCCESS)
                return LDAP_SUCCESS;
            else {
                log_print(LOG_ERR, "try_ldap_connect_bind: ldap_simple_bind_s: %s", ldap_err2string(ret));
                ldap_unbind(ldapinfo.ldap);
                ldapinfo.ldap = NULL;
            }
        } else
            ldapinfo.ldap = NULL;
    }

    /* OK, didn't succeed. */
    return ret;
}

/* try_ldap_bind:
 * Try a bind against the LDAP server. */
static int try_ldap_bind(LDAP *ld, const char *who, const char *passwd) {
    int ret, i;
    for (i = 0; i < 3; ++i) {
        ret = ldap_simple_bind_s(ld, who, passwd);
        if (ret == LDAP_SUCCESS)
            return LDAP_SUCCESS;
    }
    return ret;
}

/* auth_ldap_new_user_pass:
 * Attempt to authenticate user against the directory, using a two-step
 * search/bind process. */
authcontext auth_ldap_new_user_pass(const char *username, const char *local_part, const char *domain, const char *pass, const char *clienthost /* unused */, const char *serverhost /* unused */) {
    authcontext a = NULL;
    char *filter = NULL, *base = NULL, *who;
    LDAPMessage *ldapres = NULL, *user_attr = NULL;
    char *user_dn = NULL;
    int nentries, ret;

    who = username_string(username, local_part, domain);

    /* Connect to the server. */
    if (try_ldap_connect_bind(ldapinfo.searchdn, ldapinfo.password) != LDAP_SUCCESS) {
        log_print(LOG_ERR, _("auth_ldap_new_user_pass: unable to connect and bind to LDAP server"));
        goto fail;
    }

    /* Obtain search filter. */
    if (!(filter = substitute_filter_params(ldapinfo.filter_spec, username, local_part, domain)))
        goto fail;
    
    if (verbose)
        log_print(LOG_DEBUG, _("auth_ldap_new_user_pass: LDAP search filter: %s"), filter);

    /* Obtain search base. */
    if (!(base = substitute_filter_params(ldapinfo.dn, username, local_part, domain)))
        goto fail;

    /* Look for DN of user in the directory. */
    if ((ret = ldap_search_s(ldapinfo.ldap, base, LDAP_SCOPE_SUBTREE, filter, NULL, 0, &ldapres)) != LDAP_SUCCESS) {
        log_print(LOG_ERR, "auth_ldap_new_user_pass: ldap_search_s: %s", ldap_err2string(ret));
        goto fail;
    }

    /* There must be only one result. */
    switch (nentries = ldap_count_entries(ldapinfo.ldap, ldapres)) {
        case 1:
            break;

        default:
            log_print(LOG_ERR, _("auth_ldap_new_user_pass: search returned %d entries, should be 0 or 1"), nentries);
            /* fall through */

        case 0:
            goto fail;
    }

    if (!(user_attr = ldap_first_entry(ldapinfo.ldap, ldapres))) {
        log_print(LOG_ERR, "auth_ldap_new_user_pass: ldap_first_entry: %s", ldap_strerror());
        goto fail;
    }

    /* Get the dn string from the current entry */
    if (!(user_dn = ldap_get_dn(ldapinfo.ldap, user_attr))) {
        log_print(LOG_ERR, "auth_ldap_new_user_pass: ldap_get_dn: %s", ldap_strerror());
        goto fail;
    }

    /* Now attempt authentication by binding with the user's credentials. */
    if ((ret = try_ldap_bind(ldapinfo.ldap, user_dn, pass)) != LDAP_SUCCESS) {
        /* Bind failed; user has failed to log in. */
        if (ret == LDAP_INVALID_CREDENTIALS)
            log_print(LOG_ERR, _("auth_ldap_new_user_pass: failed login for %s"), who);
        else
            log_print(LOG_ERR, "auth_ldap_new_user_pass: try_ldap_bind: %s", ldap_err2string(ret));
        goto fail;
    } else {
        /* Bind OK; accumulate information about this user and generate an
         * authcontext. Collect attributes and off we go. */
        uid_t uid = -1;
        gid_t gid = -1;
        char *mailbox = NULL, *mboxtype = NULL, *user = NULL, *group = NULL;
        char *attr;
        BerElement *ber;

        for (attr = ldap_first_attribute(ldapinfo.ldap, user_attr, &ber); attr; attr = ldap_next_attribute(ldapinfo.ldap, user_attr, ber)) {
            char **vals;

            if (!(vals = ldap_get_values(ldapinfo.ldap, user_attr, attr))) {
                log_print(LOG_WARNING, "auth_ldap_new_user_pass: ldap_get_values(`%s', `%s'): %s", user_attr, attr, ldap_strerror());
                continue;
            }

            /* XXX case? */
            if (ldapinfo.attr.mailbox && strcasecmp(attr, ldapinfo.attr.mailbox) == 0)
                mailbox = xstrdup(*vals);
            else if (ldapinfo.attr.mboxtype && strcasecmp(attr, ldapinfo.attr.mboxtype) == 0)
                mboxtype = xstrdup(*vals);
            else if (ldapinfo.attr.user && strcasecmp(attr, ldapinfo.attr.user) == 0)
                user = xstrdup(*vals);
            else if (ldapinfo.attr.group && strcasecmp(attr, ldapinfo.attr.group) == 0)
                group = xstrdup(*vals);

            ldap_value_free(vals);
            ldap_memfree(attr);
        }

        ber_free(ber, 0);

        /* Check that we've retrieved all the attributes we need. */
#define GOT_ATTR(a)     if (ldapinfo.attr.a && !a) { \
                            log_print(LOG_ERR, _("auth_ldap_new_user_pass: did not find required attribute `%s' for %s"), \
                                      ldapinfo.attr.a, who); \
                            goto fail; \
                        }
        GOT_ATTR(mailbox);
        GOT_ATTR(mboxtype);
        GOT_ATTR(user);
        GOT_ATTR(group);
#undef GOT_ATTR

        /* Test user/group. XXX values specified in LDAP override those in config. */
        uid = ldapinfo.uid;
        gid = ldapinfo.gid;
        if (user && !parse_uid(user, &uid))
            log_print(LOG_ERR, _("auth_ldap_new_user_pass: unix user `%s' for %s does not make sense"), user, who);
        else if (group && !parse_gid(group, &gid))
            log_print(LOG_ERR, _("auth_ldap_new_user_pass: unix group `%s' for %s does not make sense"), group, who);
        else {
            struct passwd *pw;
            char *home = NULL;
            pw = getpwuid(uid);
            if (pw) home = pw->pw_dir;
            /* OK, looks like we can actually do the authentication. */
            if (mailbox && !mboxtype) {
                /* Guess mailbox type based upon name of mailbox. */
                if (mailbox[strlen(mailbox) - 1] == '/')
                    a = authcontext_new(uid, gid, "maildir", mailbox, home);
                else
                    a = authcontext_new(uid, gid, "bsd", mailbox, home);
            } else if (mailbox)
                /* Fully specified. */
                a = authcontext_new(uid, gid, mboxtype, mailbox, home);
            else
                /* Let the mailbox sort itself out.... */
                a = authcontext_new(uid, gid, NULL, NULL, home);
        }

        xfree(mailbox);
        xfree(mboxtype);
        xfree(user);
        xfree(group);
    }

fail:
    /* Ugly: force the LDAP library to free user_addr. */
    if (user_attr) while (ldap_next_entry(ldapinfo.ldap, ldapres));
    
    if (ldapres) ldap_msgfree(ldapres);
    if (user_dn) ldap_memfree(user_dn);

    xfree(filter);
    xfree(base);

/*    auth_ldap_close();*/

    return a;
}

/* auth_ldap_close:
 * Close the ldap connection. */
void auth_ldap_close() {
    if (ldapinfo.ldap) {
        ldap_unbind(ldapinfo.ldap);
        ldapinfo.ldap = NULL;
    }
}

/* auth_ldap_postfork:
 * Post-fork cleanup. */
void auth_ldap_postfork() {
    ldapinfo.ldap = NULL; /* XXX */
}

/* ldap_escape:
 * Form an escaped version of a string for use in an LDAP filter. */
static char *ldap_escape(const char *s) {
    static char *t;
    static size_t tlen;
    size_t l;
    char *q;
    const char *p;
    
    if (tlen < (l = strlen(s) * 3 + 1)) {
        tlen = l;
        t = xrealloc(t, tlen);
    }

    for (p = s, q = t; *p; ++p)
        if (strchr("*()\\", *p)) {
            sprintf(q, "\\%02x", (unsigned int)*p);
            q += 3;
        } else
            *q++ = *p;
    *q = 0;

    return t;
}

/* substitute_filter_params:
 * Given a filter template, local part and domain, construct a real filter
 * string. */
static char *substitute_filter_params(const char *template, const char *user, const char *local_part, const char *domain) {
    char *filter = NULL, *u = NULL, *l = NULL, *d = NULL;
    struct sverr err;

    u = xstrdup(ldap_escape(user));
    if (local_part)
        l = xstrdup(ldap_escape(local_part));
    if (domain)
        d = xstrdup(ldap_escape(domain));

    filter = substitute_variables(template, &err, 3, "user", u, "local_part", l, "domain", d);

    if (!filter && err.code != sv_nullvalue)
        log_print(LOG_ERR, _("substitute_filter_params: %s near `%.16s'"), err.msg, template + err.offset);

    xfree(u);
    xfree(l);
    xfree(d);
    return filter;
}

#endif /* AUTH_LDAP */
