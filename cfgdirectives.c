/*
 * cfgdirectives.c:
 * List of valid config directives.
 *
 * This is ugly; it would be nice to pick this information automatically
 * somehow from the source, but that's a matter for another day.
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

static const char rcsid[] = "$Id$";

#ifdef HAVE_CONFIG_H
#include "configuration.h"
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>
#include <string.h>

char *cfgdirectives[] = {
    /* global directives */
    "listen-address",
    "max-children",
    "append-domain",
    "strip-domain",
    "timeout-seconds",
    "tcp-send-buffer",
    "log-facility",
    "log-stderr",
    "log-level",
    "apop-only",
    "mailbox",
    "no-detach",
    "permit-empty-passwords",
    "onlogin-child-wait",
    "log-bad-passwords",
    "no-commit-on-early-close",
    "lowercase-user",
    "lowercase-mailbox",
    "uidl-style",
    "domain-separators",
 
#if defined(MBOX_BSD) && defined(MBOX_BSD_SAVE_INDICES)
    "mailspool-index",
#endif

#ifdef MBOX_BSD
    "mailspool-no-dotfile-locking",
#endif
    
#ifdef USE_TCP_WRAPPERS
    "tcp-wrappers-name",
#endif

#ifdef USE_DRAC
    "drac-server",
#endif

#ifdef USE_WHOSON
    "whoson-enable",
#endif
    
#ifdef MBOX_MAILDIR
    "maildir-exclusive-lock",
    "maildir-recursion",
    "maildir-ignore-folders",
    "maildir-evaluate-filename",
    "maildir-size-string",
#endif

#ifdef USE_TLS
    "tls-no-bug-workarounds",
#endif

    "authcache-enable",
    "authcache-use-client-host",
    "authcache-entry-lifetime",

#ifdef AUTH_PAM
    /* auth-pam options */
    "auth-pam-enable",
    "auth-pam-facility",
    "auth-pam-mailbox",
    "auth-pam-mail-user",
    "auth-pam-mail-group",
#endif

#ifdef AUTH_PASSWD
    /* auth-passwd options */
    "auth-passwd-enable",
    "auth-passwd-mailbox",
    "auth-passwd-mail-group",
#endif

#ifdef AUTH_MYSQL
    /* auth-mysql options */
    "auth-mysql-enable",
    "auth-mysql-username",
    "auth-mysql-password",
    "auth-mysql-database",
    "auth-mysql-hostname",
    "auth-mysql-mail-group",
    "auth-mysql-pass-query",
    "auth-mysql-apop-query",
    "auth-mysql-onlogin-query",
#endif /* AUTH_MYSQL */

#ifdef AUTH_PGSQL
    /* auth-pgsql options */
    "auth-pgsql-enable",
    "auth-pgsql-username",
    "auth-pgsql-password",
    "auth-pgsql-database",
    "auth-pgsql-hostname",
    "auth-pgsql-mail-group",
    "auth-pgsql-pass-query",
    "auth-pgsql-apop-query",
    "auth-pgsql-onlogin-query",
#endif /* AUTH_PGSQL */

#ifdef AUTH_LDAP
    "auth-ldap-enable",
    "auth-ldap-url",
    "auth-ldap-searchdn",
    "auth-ldap-password",
    "auth-ldap-use-tls",
    "auth-ldap-filter",
    "auth-ldap-scope",
    "auth-ldap-mailbox",
    "auth-ldap-mailbox-attr",
    "auth-ldap-mboxtype-attr",
    "auth-ldap-mail-user",
    "auth-ldap-mail-user-attr",
    "auth-ldap-mail-group",
    "auth-ldap-mail-group-attr",
#endif /* AUTH_LDAP */
    
#ifdef AUTH_OTHER
    "auth-other-enable",
    "auth-other-program",
    "auth-other-user",
    "auth-other-group",
    "auth-other-timeout",
#endif /* AUTH_OTHER */
 
#ifdef AUTH_PERL
    "auth-perl-enable",
    "auth-perl-start",
    "auth-perl-finish",
    "auth-perl-apop",
    "auth-perl-pass",
    "auth-perl-onlogin",
#endif /* AUTH_PERL */

#ifdef AUTH_FLATFILE
    "auth-flatfile-enable",
    "auth-flatfile-mail-user",
    "auth-flatfile-mail-group",
    "auth-flatfile-passwd-file",
#endif /* AUTH_FLATFILE */

#ifdef AUTH_GDBM
    "auth-gdbm-enable",
    "auth-gdbm-mail-user",
    "auth-gdbm-mail-group",
    "auth-gdbm-passwd-file",
    "auth-gdbm-persistent",
#endif

    /* final entry must be NULL */
    NULL};

int is_cfgdirective_valid(const char *s) {
    char **t;
    for (t = cfgdirectives; *t; ++t)
        if (strcmp(s, *t) == 0) return 1;
    return 0;
}
