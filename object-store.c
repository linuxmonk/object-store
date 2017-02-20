/*
 * Author: Sai Kiran Gummaraj
 * Date: 6 September 2016
 * 
 * Sample plugin to demonstrate storing email messages to a remote
 * object store. See COPYING file in Dovecot directory for license
 * terms.
 */

#include "lib.h"
#include "array.h"
#include "llist.h"
#include "str.h"
#include "str-sanitize.h"
#include "mail-user.h"
#include "mail-storage-private.h"
#include "mail-log-plugin.h"

#include "object-store.h"

/* 
 * Define a few convenience macros to access the plugin contexts.
 * These macros return the pointers to the plugin specific object contexts
 */
#define OBJECT_STORE_STORAGE_CONTEXT(obj) MODULE_CONTEXT(obj, object_store_storage_module)
#define OBJECT_STORE_MAIL_CONTEXT(obj) MODULE_CONTEXT(obj, object_store_mail_module)
#define OBJECT_STORE_USER_CONTEXT(obj) MODULE_CONTEXT(obj, object_store_user_module)

const char *object_store_plugin_version = DOVECOT_ABI_VERSION;

/* 
 * Object store user
 */
typedef struct s3_user {

    union mail_user_module_context module_ctx;
    const char *s3_hostname;
    const char *s3_access_keyid;
    const char *s3_access_secret;
} s3_user_t;

/*
 * Based on the objects we are interested to manipulate/look at in this plugin.
 * In this case mail storage and mail. This defines corresponding context types 
 * for the plugin.
 */
static MODULE_CONTEXT_DEFINE_INIT(object_store_storage_module, &mail_storage_module_register);
static MODULE_CONTEXT_DEFINE_INIT(object_store_mail_module, &mail_module_register);
static MODULE_CONTEXT_DEFINE_INIT(object_store_user_module, &mail_user_module_register);

static int object_store_istream_opened(struct mail *m, struct istream **stream) 
{
    struct mail_private *mail = (struct mail_private *)m;
    struct mail_user *user = m->box->storage->user;
    s3_user_t *s3user = OBJECT_STORE_USER_CONTEXT(user);
    union mail_module_context *mmctx = OBJECT_STORE_MAIL_CONTEXT(mail);
    struct istream *input;

    /* 
     * Much of the logic of getting email from object store
     * would go here. Probably call to caching as well.
     * TODO: For that to work, istream object to the
     * object store must be created.
     * For now, just call the super method which would default
     * to the mailbox type being used.
     */
    int result = mmctx->super.istream_opened(m, stream);

    return result;
}

static void object_store_mail_allocated(struct mail *m) 
{
    struct mail_private *mail = (struct mail_private *)m;
    struct mail_vfuncs *v = mail->vlast;
    union mail_module_context *mmctx;

    i_debug("object_store_mail_allocated called");
    mmctx = p_new(mail->pool, union mail_module_context, 1);
    mmctx->super = *v;
    mail->vlast = &mmctx->super;

    v->istream_opened = object_store_istream_opened;

    MODULE_CONTEXT_SET_SELF(mail, object_store_mail_module, mmctx);
}

static int object_store_mail_save_begin(struct mail_save_context *context, struct istream *input) {
    struct mailbox *box = context->transaction->box;
    // s3_user_t *s3user = OBJECT_STORE_USER_CONTEXT(box->storage->user);
    union mailbox_module_context *mbox = OBJECT_STORE_STORAGE_CONTEXT(box);

	if (mbox->super.save_begin(context, input) < 0)
				return -1;

    i_debug("object_store_mail_save_begin called");
    i_debug("Mail save to S3 started...");
    return 0;
}

static void object_store_mailbox_allocated(struct mailbox *box)
{
    struct mailbox_vfuncs *v = box->vlast; 
    union mailbox_module_context *mbox;
    enum mail_storage_class_flags class_flags = box->storage->class_flags;
    
    /*
     * Create module specific context for this plugin on the mailbox 
     * memory pool and assign our callbacks to that list.
     */
    i_debug("object_store_mailbox_allocated called");
    mbox = p_new(box->pool, union mailbox_module_context, 1);
    mbox->super = *v;
    box->vlast = &mbox->super;

    MODULE_CONTEXT_SET_SELF(box, object_store_storage_module, mbox);

    if ((class_flags & MAIL_STORAGE_CLASS_FLAG_OPEN_STREAMS) == 0)
        v->save_begin = object_store_mail_save_begin;
}

static void object_store_user_created(struct mail_user *user) {
    struct mail_user_vfuncs *v = user->vlast;
    s3_user_t *s3user;

    i_debug("object_store_user_created called");
    s3user = p_new(user->pool, s3_user_t, 1);
    s3user->module_ctx.super = *v;
    user->vlast = &s3user->module_ctx.super;

    s3user->s3_hostname = mail_user_plugin_getenv(user, "s3_hostname");
    s3user->s3_access_keyid = mail_user_plugin_getenv(user, "s3_access_keyid");
    s3user->s3_access_secret = mail_user_plugin_getenv(user, "s3_access_secret");

    i_debug("s3_hostname = %s", s3user->s3_hostname);
    i_debug("s3_access_keyid = %s", s3user->s3_access_keyid);
    i_debug("s3_access_secret = %s", s3user->s3_access_secret);

    MODULE_CONTEXT_SET(user, object_store_user_module, s3user);
}

static struct mail_storage_hooks object_store_storage_hooks = {
    .mail_user_created = object_store_user_created,
    .mailbox_allocated = object_store_mailbox_allocated,
    .mail_allocated = object_store_mail_allocated
}; 

void object_store_plugin_init(struct module *module)
{
    i_debug("object_store_plugin_init called");
    mail_storage_hooks_add(module, &object_store_storage_hooks);
}

void object_store_plugin_deinit(void)
{
    i_debug("object_store_plugin_deinit called");
    mail_storage_hooks_remove(&object_store_storage_hooks);
}

const char *object_store_plugin_dependencies[] = { NULL };

