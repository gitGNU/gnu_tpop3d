/*
 * mailbox.h:
 * Mailbox object support in tpop3d.
 *
 * Terminology: a `mailbox' is a store of messages, which could be in any
 * format (though note that the contents of the mailbox object are rather
 * influenced by its origins). A `mailspool' means specifically a mailbox file
 * in BSD ("From ") format, and is an instance of a `mailbox'. A `maildir' is
 * a collection of messages organised in individual files in the style of
 * Qmail. The last implementation is `emptymbox', which can be used if no
 * actual mailbox is available.
 *
 * Each mailbox driver should be listed with its constructor, a short
 * description and a key word in the mbox_drivers array in mailbox.c. The key
 * word is used in the specification of user mailspools.
 *
 * Locating user mailspools is the job of authentication drivers; however,
 * there is a function find_mailbox which assists with this; it interprets
 * configuration parameters of the form `/var/spool/$(user[0])/$user' or
 * `maildir:$(home)/Maildir' and maps them to actual mailspool names. This is
 * intended for use with actual user mailspools, rather than virtual-domain
 * ones.
 *
 * (Historically, tpop3d supported only BSD mailspools, so the terminology is
 * a little confused....)
 *
 * $Id$
 * 
 * Copyright (c) 2001 Chris Lightfoot, Paul Makepeace (realprogrammers.com).
 * All rights reserved.
 *
 */
 
#ifndef __MAILBOX_H_ /* include guard */
#define __MAILBOX_H_

#ifdef HAVE_CONFIG_H
#include "configuration.h"
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>    /* for struct stat */
#include <sys/stat.h>

#include <stdio.h>

#include "vector.h"

/* mailbox:
 * Generic object representing a store of messages.
 */
typedef struct _mailbox *mailbox;

struct _mailbox {
    char *name;                /* Spool filename or maildir directory name */
    int fd;                    /* File descriptor for open mailspool. */
    char isempty;              /* Boolean for mailspool. */
    struct stat st;
    vector index;              /* `array' of information for messages */
    int numdeleted;

    /* function pointers for pseudo OO-ness */
    void    (*delete)(mailbox m);
    int     (*send_message)(mailbox m, int sck, const int i, int n);
    int     (*apply_changes)(mailbox m);
};

/* Return code from a mailbox constructor used to indicate non-presence of the
 * mailbox.
 */
#define MBOX_NOENT      ((mailbox)-1)

/* indexpoint:
 * Individual messages which appear as files on FS for maildir or sections of
 * a file in a BSD mailspool.
 */
typedef struct _indexpoint {
    char *filename;
    size_t offset, length, msglength; /* offsets and length for mailboxes */
    time_t mtime;                     /* modification time used for maildirs */
    char deleted;
    unsigned char hash[16];
} *indexpoint;

/* struct mboxdrv:
 * Structure for describing alternate mailspool drivers.
 */
struct mboxdrv {
    char *name;
    char *description;
    mailbox (*m_new)(const char*);
};

/* mailspool, maildir common functions */
void       mailbox_describe(FILE *fp);
mailbox    mailbox_new(const char *filename, const char *type);
void       mailbox_delete(mailbox m);

/* Empty mailbox implementation. */
mailbox emptymbox_new(const char *filename);
int emptymbox_apply_changes(mailbox m);

#ifdef MBOX_BSD
/* BSD mailspool implementation. */
mailbox    mailspool_new_from_file(const char *filename);
void       mailspool_delete(mailbox m);
vector     mailspool_build_index(mailbox m);
int        mailspool_send_message(mailbox m, int sck, const int i, int n);
int        mailspool_apply_changes(mailbox m);

/* How long we wait between trying to lock the mailspool */
#define MAILSPOOL_LOCK_WAIT           2
/* How many times we try */
#define MAILSPOOL_LOCK_TRIES          4

#endif /* MBOX_BSD */

/* Maildir implementation. */
#ifdef MBOX_MAILDIR
mailbox    maildir_new(const char *filename);
void       maildir_delete(mailbox m);
int        maildir_send_message(mailbox m, int sck, const int i, int n);
int        maildir_apply_changes(mailbox m);
#endif /* MBOX_MAILDIR */

#endif /* __MAILBOX_H_ */