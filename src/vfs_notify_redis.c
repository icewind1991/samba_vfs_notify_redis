/*
 * Auditing VFS module for samba.  Log selected file operations to syslog
 * facility.
 *
 * Copyright (C) Tim Potter			1999-2000
 * Copyright (C) Alexander Bokovoy		2002
 * Copyright (C) Stefan (metze) Metzmacher	2002
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */


#include "includes.h"
#include "system/filesys.h"
#include "system/syslog.h"
#include "smbd/smbd.h"
#include "lib/param/loadparm.h"
#include "hiredis.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_VFS

/* Implementation of vfs_ops.  Pass everything on to the default
   operation but log event first. */

struct notify_redis_context {
    redisContext *redis;
    const char *list;

};

static void close_redis_connection(void **data)
{
    struct notify_redis_context **p_redis_context = (struct notify_redis_context **)data;
    redisFree((*p_redis_context)->redis);
    TALLOC_FREE(*p_redis_context);
}

static int notify_connect(vfs_handle_struct *handle, const char *svc, const char *user)
{
    int result;

    result = SMB_VFS_NEXT_CONNECT(handle, svc, user);
    if (result < 0) {
        return result;
    }

    struct notify_redis_context *config;
    const char *redis_hostname = lp_parm_const_string(SNUM(handle->conn), "notify_redis", "hostname", "localhost");
    const char *redis_list = lp_parm_const_string(SNUM(handle->conn), "notify_redis", "list", "notify");
    int redis_port = lp_parm_int(SNUM(handle->conn), "notify_redis", "port", 6379);
    redisContext *c = redisConnect(redis_hostname, redis_port);
    config = talloc_zero(handle->conn, struct notify_redis_context);
    if (!config) {
        SMB_VFS_NEXT_DISCONNECT(handle);
        DEBUG(0, ("talloc_zero() failed\n")); return -1;
    }
    config->redis = c;
    config->list = redis_list;
    SMB_VFS_HANDLE_SET_DATA(handle, config,
                            close_redis_connection, struct notify_redis_context,
                            return -1);
    return 0;
}

static int notify_redis_simple(vfs_handle_struct *handle, const char *type, const struct smb_filename *smb_fname)
{
    struct notify_redis_context *context = NULL;
    SMB_VFS_HANDLE_GET_DATA(handle, context, struct notify_redis_context, NULL);
    if (context == NULL) {
        return -1;
    }

    redisCommand(context->redis, "LPUSH %s %s|%s", context->list, type, smb_fname->base_name);

    return 0;
}

static int notify_redis_rename(vfs_handle_struct *handle, const struct smb_filename *from, const struct smb_filename *to)
{
    struct notify_redis_context *context = NULL;
    SMB_VFS_HANDLE_GET_DATA(handle, context, struct notify_redis_context, NULL);
    if (context == NULL) {
        return -1;
    }

    redisCommand(context->redis, "LPUSH %s rename|%s|%s", context->list, from->base_name, to->base_name);

    return 0;
}

static int notify_mkdir(vfs_handle_struct *handle,
                       const struct smb_filename *smb_fname,
                       mode_t mode)
{
    int result;

    result = SMB_VFS_NEXT_MKDIR(handle, smb_fname, mode);

    notify_redis_simple(handle, "write", smb_fname);

    return result;
}

static int notify_rmdir(vfs_handle_struct *handle,
                       const struct smb_filename *smb_fname)
{
    int result;

    result = SMB_VFS_NEXT_RMDIR(handle, smb_fname);

    notify_redis_simple(handle, "remove", smb_fname);

    return result;
}

struct open_file {
    struct smb_filename *smb_fname;
    int flags;
};

static int notify_open(vfs_handle_struct *handle,
                      struct smb_filename *smb_fname, files_struct *fsp,
                      int flags, mode_t mode)
{
    int result;

    result = SMB_VFS_NEXT_OPEN(handle, smb_fname, fsp, flags, mode);

    struct open_file *p_file_info;
    p_file_info = VFS_ADD_FSP_EXTENSION(handle, fsp, struct open_file, NULL);
    p_file_info->smb_fname = smb_fname;
    p_file_info->flags = flags;

    return result;
}

static int notify_close(vfs_handle_struct *handle, files_struct *fsp)
{
    int result;

    result = SMB_VFS_NEXT_CLOSE(handle, fsp);

    struct open_file *p_file_info;
    p_file_info = (struct open_file *)VFS_FETCH_FSP_EXTENSION(handle, fsp);

    if ((p_file_info->flags & O_WRONLY) || (p_file_info->flags & O_RDWR)) {
        notify_redis_simple(handle, "write", p_file_info->smb_fname);
    }

    return result;
}

static int notify_rename(vfs_handle_struct *handle,
                        const struct smb_filename *smb_fname_src,
                        const struct smb_filename *smb_fname_dst)
{
    int result;

    result = SMB_VFS_NEXT_RENAME(handle, smb_fname_src, smb_fname_dst);

    notify_redis_rename(handle, smb_fname_src, smb_fname_dst);

    return result;
}

static int notify_unlink(vfs_handle_struct *handle,
                        const struct smb_filename *smb_fname)
{
    int result;

    result = SMB_VFS_NEXT_UNLINK(handle, smb_fname);

    notify_redis_simple(handle, "remove", smb_fname);

    return result;
}

static int notify_chmod(vfs_handle_struct *handle,
                       const struct smb_filename *smb_fname,
                       mode_t mode)
{
    int result;

    result = SMB_VFS_NEXT_CHMOD(handle, smb_fname, mode);

    // notify_redis_simple(handle, "chmod write", smb_fname);

    return result;
}

static int notify_chmod_acl(vfs_handle_struct *handle,
                           const struct smb_filename *smb_fname,
                           mode_t mode)
{
    int result;

    result = SMB_VFS_NEXT_CHMOD_ACL(handle, smb_fname, mode);

    // notify_redis_simple(handle, "acl write", smb_fname);

    return result;
}

static struct vfs_fn_pointers vfs_notify_redis_fns = {
        .connect_fn = notify_connect,
        .mkdir_fn = notify_mkdir,
        .rmdir_fn = notify_rmdir,
        .open_fn = notify_open,
        .close_fn = notify_close,
        .rename_fn = notify_rename,
        .unlink_fn = notify_unlink,
        .chmod_fn = notify_chmod,
        .chmod_acl_fn = notify_chmod_acl,
};

static_decl_vfs;
NTSTATUS vfs_notify_redis_init(TALLOC_CTX *ctx)
{
    return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "notify_redis",
                            &vfs_notify_redis_fns);
}
