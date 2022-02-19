#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include "networkfs.h"
#include "utils.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Volkov Egor");
MODULE_VERSION("0.01");

struct file_system_type networkfs_fs_type = {
		.name = "networkfs",
		.mount = networkfs_mount,
		.kill_sb = networkfs_kill_sb
};

struct inode_operations networkfs_inode_ops = {
		.lookup = networkfs_lookup,
		.create = networkfs_create,
		.unlink = networkfs_unlink,
		.mkdir = networkfs_mkdir,
		.rmdir = networkfs_rmdir,
		.link = networkfs_link,
};

struct file_operations networkfs_dir_ops = {
		.iterate = networkfs_iterate,
		.read = networkfs_read,
		.write = networkfs_write,
};

// ================================================

struct dentry *networkfs_mount(struct file_system_type *fs_type, int flags, const char *token, void *data) {
	struct dentry *ret = NULL;
	char* token_copy = NULL;

	ret = mount_nodev(fs_type, flags, data, networkfs_fill_super);
	ERR_WRAPPER(!ret, "networkfs: can't mount file system", exit)
	ret->d_sb->s_fs_info = NULL;

	token_copy = kmalloc(strlen(token), GFP_KERNEL);
	ERR_WRAPPER(!token_copy, "mount: memory error", exit);

	strcpy(token_copy, token);
	ret->d_sb->s_fs_info = (void*)token_copy;

	printk(KERN_INFO "networkfs: mounted successfully");

exit:
	return token_copy ? ret : 0;
}

int networkfs_fill_super(struct super_block *sb, void *data, int silent) {
	struct inode *inode;
	inode = networkfs_get_inode(sb, NULL, S_IFDIR | S_IRWXUGO, 1000);
	sb->s_root = d_make_root(inode);

	if (sb->s_root == NULL) {
		return -ENOMEM;
	}

	return 0;
}

struct inode* networkfs_get_inode(struct super_block *sb, const struct inode *dir, umode_t mode, int i_ino) {
	struct inode* inode;
	inode = new_inode(sb);

	if (inode) {
		inode->i_op = &networkfs_inode_ops;
		inode->i_fop = &networkfs_dir_ops;
		inode->i_ino = i_ino;
		inode_init_owner(inode, dir, mode);
	}

	return inode;
}

void networkfs_kill_sb(struct super_block *sb) {
	kfree(sb->s_fs_info);
	printk(KERN_INFO "networkfs super block is destroyed. Unmount successfully.\n");
}

int networkfs_iterate(struct file *filp, struct dir_context *ctx) {
	struct entries* entries = NULL;
	struct inode* inode;
	int ret;
	INT_ARG(ino)


	entries = kmalloc(sizeof(struct entries), GFP_KERNEL);
	ERR_WRAPPER(!entries, "iterate: memory error", exit)
	inode = filp->f_inode;

	sprintf(ino, "%s=%lu", "inode", inode->i_ino);
	char const* params[] = {ino};

	ret = connect_to_server("list", 1, params, TOKEN_FROM_INODE(inode), (char*)entries);
	ERR_WRAPPER(ret != 0, "iterate: server responded with error", exit)

	for ( ret = ctx->pos; ret < entries->entries_count; ++ret, ++ctx->pos) {
		struct entry entry = entries->entries[ret];
		dir_emit(ctx, entry.name, strlen(entry.name), entry.ino, entry.entry_type);
	}

exit:
	kfree(entries);
	return ret;
}

ssize_t networkfs_read(struct file *filp, char *buffer, size_t len, loff_t *offset) {
	struct inode* inode;
	struct content* content = NULL;
	ssize_t ret = -1;
	INT_ARG(ino)

	content = kmalloc(MAX_SIZE + sizeof(content->content_length), GFP_KERNEL);
	ERR_WRAPPER(!content, "read: memory error", exit)

	inode = filp->f_inode;

	sprintf(ino, "%s=%lu", "inode", inode->i_ino);
	char const* params[] = {ino};

	ret = connect_to_server("read", 1, params, TOKEN_FROM_INODE(inode), (char*)content);
	ERR_WRAPPER(ret != 0, "read: server responded with error", exit)

	if (*offset >= content->content_length) {
		goto exit; // ret is already 0
	}

	for ( ; *offset < content->content_length; ++ret, ++*offset) {
		put_user(content->content[*offset], buffer + *offset);
	}

exit:
	kfree(content);
	return ret;
}

ssize_t networkfs_write(struct file *filp, const char *buffer, size_t len, loff_t *offset) {
	struct inode* inode;
	char* content = NULL;
	char* escaped_content = NULL;
	int ret = -1;
	ssize_t cnt = 0;
	INT_ARG(ino)

	content = kmalloc(len, GFP_KERNEL);
	ERR_WRAPPER(!content, "write: memory error", exit)

	for ( ; *offset < len; ++cnt, ++*offset) {
		get_user(content[*offset], buffer + *offset);
	}

	inode = filp->f_inode;

	printk(KERN_ERR "%s", content);

	sprintf(ino, "%s=%lu", "inode", inode->i_ino);
	escaped_content = escape_url("content=", content, len);
	char const* params[] = {ino, escaped_content};

	printk(KERN_ERR "%s", escaped_content);

	ret = connect_to_server("write", 2, params, TOKEN_FROM_INODE(inode), NULL);
	printk(KERN_ERR "write: ret %d", ret);
	ERR_WRAPPER(ret != 0, "write: server responded with error", exit)

exit:
	kfree(content);
	kfree(escaped_content);
	return ret < 0 ? ret : cnt;
}

// =============================================

struct dentry* networkfs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flag) {
	struct entry_info* info = NULL;
	struct inode* inode;
	char* name = NULL;
	int ret;
	INT_ARG(parent)

	name = escape_url("name=", child_dentry->d_name.name, strlen(child_dentry->d_name.name));
	ERR_WRAPPER(!name, "lookup: escape error", exit)

	info = kmalloc(sizeof(struct entry_info), GFP_KERNEL);
	ERR_WRAPPER(!info, "lookup: memory error", exit)

	sprintf(parent, "%s=%lu", "parent", parent_inode->i_ino);
	char const* params[] = {parent, name};

	ret = connect_to_server("lookup", 2, params, TOKEN_FROM_INODE(parent_inode), (char*)info);
	printk(KERN_ERR "lookup: ret %d", ret);
	ERR_WRAPPER(ret != 0, "lookup: server responded with error", exit)

	inode = networkfs_get_inode(parent_inode->i_sb, NULL, (info->entry_type == DT_DIR ? S_IFDIR : S_IFREG) | S_IRWXUGO, info->ino);
	ERR_WRAPPER(!inode, "lookup: can't get inode", exit)

	d_add(child_dentry, inode);

exit:
	kfree(name);
	kfree(info);
	return NULL;
}

int networkfs_create(struct inode* parent_inode, struct dentry* child_dentry, umode_t mode, bool b) {
	char* name = NULL;
	struct inode* inode;
	int ret, i_no;
	INT_ARG(parent);

	name = escape_url("name=", child_dentry->d_name.name, strlen(child_dentry->d_name.name));
	ERR_WRAPPER(!name, "create: escape error", exit)

	sprintf(parent, "%s=%lu", "parent", parent_inode->i_ino);
	char const* params[] = {parent, name, "type=file"};

	ret = connect_to_server("create", 3, params, TOKEN_FROM_INODE(parent_inode), (char*)&i_no);
	ERR_WRAPPER(ret != 0, "create: server responded with error", exit)

	inode = networkfs_get_inode(parent_inode->i_sb, NULL, S_IFREG | S_IRWXUGO, i_no);
	ERR_WRAPPER(!inode, "create: can't get inode", exit)
	inode->i_op = &networkfs_inode_ops;
	inode->i_fop = &networkfs_dir_ops;

	d_add(child_dentry, inode);

exit:
	kfree(name);
	return ret;
}

int networkfs_unlink(struct inode* parent_inode, struct dentry* child_dentry) {
	char* name = NULL;
	int ret;
	INT_ARG(parent)

	name = escape_url("name=", child_dentry->d_name.name, strlen(child_dentry->d_name.name));
	ERR_WRAPPER(!name, "unlink: escape error", exit)

	sprintf(parent, "%s=%lu", "parent", parent_inode->i_ino);
	char const* params[] = {parent, name};

	ret = connect_to_server("unlink", 2, params, TOKEN_FROM_INODE(parent_inode), NULL);
	ERR_WRAPPER(ret != 0, "unlink: server responded with error", exit)

exit:
	kfree(name);
	return ret;
}

int networkfs_mkdir(struct inode* parent_inode, struct dentry* child_dentry, umode_t mode) {
	char* name = NULL;
	struct inode* inode;
	int ret, i_no;
	INT_ARG(parent)

	name = escape_url("name=", child_dentry->d_name.name, strlen(child_dentry->d_name.name));
	ERR_WRAPPER(!name, "mkdir: escape error", exit)

	sprintf(parent, "%s=%lu", "parent", parent_inode->i_ino);
	char const* params[] = {parent, name, "type=directory"};

	ret = connect_to_server("create", 3, params, TOKEN_FROM_INODE(parent_inode), (char*)&i_no);
	ERR_WRAPPER(ret != 0, "mkdir: server responded with error", exit)

	inode = networkfs_get_inode(parent_inode->i_sb, NULL, S_IFDIR | S_IRWXUGO, i_no);
	ERR_WRAPPER(!inode, "mkdir: can't get inode", exit)
	inode->i_op = &networkfs_inode_ops;
	inode->i_fop = &networkfs_dir_ops;

	d_add(child_dentry, inode);

exit:
	kfree(name);
	return ret;
}

int networkfs_rmdir(struct inode* parent_inode, struct dentry* child_dentry) {
	char* name = NULL;
	int ret;
	INT_ARG(parent)

	name = escape_url("name=", child_dentry->d_name.name, strlen(child_dentry->d_name.name));
	ERR_WRAPPER(!name, "rmdir: escape error", exit)

	sprintf(parent, "%s=%lu", "parent", parent_inode->i_ino);
	char const* params[] = {parent, name};

	ret = connect_to_server("rmdir", 2, params, TOKEN_FROM_INODE(parent_inode), NULL);
	ERR_WRAPPER(ret != 0, "rmdir: server responded with error", exit)

exit:
	kfree(name);
	return ret;
}

int networkfs_link(struct dentry *old_dentry, struct inode *parent_dir, struct dentry *new_dentry) {
	char* name = NULL;
	int ret;
	INT_ARG(parent)
	INT_ARG(source)

	name = escape_url("name=", new_dentry->d_name.name, strlen(new_dentry->d_name.name));
	printk(KERN_ERR "%s : %s", new_dentry->d_name.name, name);
	ERR_WRAPPER(!name, "link: escape error", exit)

	sprintf(parent, "%s=%lu", "parent", parent_dir->i_ino);
	sprintf(source, "%s=%lu", "source", old_dentry->d_inode->i_ino);
	char const* params[] = {source, parent, name};

	ret = connect_to_server("link", 3, params, TOKEN_FROM_INODE(parent_dir), NULL);
	printk(KERN_ERR "%d", ret);
	ERR_WRAPPER(ret != 0, "link: server responded with error", exit)


exit:
	kfree(name);
	return ret;
}


// ==============================================

int networkfs_init(void) {
	printk(KERN_INFO "networkfs_init\n");

	register_filesystem(&networkfs_fs_type);
	return 0;
}

void networkfs_exit(void) {
	printk(KERN_INFO "networkfs_exit\n");

	unregister_filesystem(&networkfs_fs_type);
}

module_init(networkfs_init);
module_exit(networkfs_exit);
