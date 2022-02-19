#pragma once

#define TOKEN_FROM_INODE(inode) (char const*)((inode)->i_sb->s_fs_info)

#define TOKEN_FROM_DENTRY(dentry) (char const*)((dentry)->d_sb->s_fs_info)

struct entry_info {
	unsigned char entry_type; // DT_DIR (4) or DT_REG (8)
	ino_t ino;
};

struct entry {
	unsigned char entry_type; // DT_DIR (4) or DT_REG (8)
	ino_t ino;
	char name[256];
};

struct entries {
	size_t entries_count;
	struct entry entries[16];
};

struct content {
	u64 content_length;
	char content[];
};

// fs type
struct dentry* networkfs_mount(struct file_system_type *fs_type, int flags, const char *token, void *data);

void networkfs_kill_sb(struct super_block *sb);

int networkfs_fill_super(struct super_block *sb, void *data, int silent);

struct inode* networkfs_get_inode(struct super_block *sb, const struct inode *dir, umode_t mode, int i_ino);

// inode operations
struct dentry* networkfs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flag);

int networkfs_create(struct inode* parent_inode, struct dentry* child_dentry, umode_t mode, bool b);

int networkfs_unlink(struct inode* parent_inode, struct dentry* child_dentry);

int networkfs_mkdir(struct inode* parent_inode, struct dentry* child_dentry, umode_t mode);

int networkfs_rmdir(struct inode* parent_inode, struct dentry* child_dentry);

int networkfs_link(struct dentry *old_dentry, struct inode *parent_dir, struct dentry *new_dentry);

// file operations
int networkfs_iterate(struct file *filp, struct dir_context *ctx);

ssize_t networkfs_read(struct file *filp, char *buffer, size_t len, loff_t *offset);

ssize_t networkfs_write(struct file *filp, const char *buffer, size_t len, loff_t *offset);
