#include "vfs.h"
#include "console.h"
#include "fs.h"
#include "kstring.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define VFS_MAX_NODES 80
#define VFS_NAME_SIZE 64
#define VFS_CHILDREN_MAX 16
#define VFS_DATA_STORE_SIZE 16384

struct vfs_node {
	char name[VFS_NAME_SIZE];
	bool is_dir;
	bool read_only;
	const char *data;
	size_t size;
	char *own_data;
	size_t own_size;
	int parent;
	int child_count;
	int children[VFS_CHILDREN_MAX];
};

typedef struct vfs_node vfs_node_t;

static vfs_node_t nodes[VFS_MAX_NODES];
static char data_store[VFS_DATA_STORE_SIZE];
static size_t data_used;
static int root_node = -1;
static int cwd_node = -1;

static int vfs_new_node(void)
{
	for (int i = 0; i < VFS_MAX_NODES; ++i) {
		if (nodes[i].name[0] == '\0' && i != root_node)
			return i;
	}
	return -1;
}

static void vfs_clear_node(int index)
{
	nodes[index].name[0] = '\0';
	nodes[index].is_dir = false;
	nodes[index].read_only = false;
	nodes[index].data = 0;
	nodes[index].size = 0;
	nodes[index].own_data = 0;
	nodes[index].own_size = 0;
	nodes[index].parent = -1;
	nodes[index].child_count = 0;
}

static char *vfs_alloc_data(const char *data, size_t size)
{
	if (data_used + size >= VFS_DATA_STORE_SIZE)
		return 0;
	char *dest = data_store + data_used;
	for (size_t i = 0; i < size; ++i)
		dest[i] = data[i];
	data_used += size;
	return dest;
}

static int vfs_find_child(int dir, const char *name)
{
	if (dir < 0 || dir >= VFS_MAX_NODES || !nodes[dir].is_dir)
		return -1;
	for (int i = 0; i < nodes[dir].child_count; ++i) {
		int child = nodes[dir].children[i];
		if (kstrcmp(nodes[child].name, name) == 0)
			return child;
	}
	return -1;
}

static int vfs_add_child(int parent, int child)
{
	if (parent < 0 || parent >= VFS_MAX_NODES)
		return -1;
	if (nodes[parent].child_count >= VFS_CHILDREN_MAX)
		return -1;
	nodes[parent].children[nodes[parent].child_count++] = child;
	nodes[child].parent = parent;
	return 0;
}

static int vfs_resolve(const char *path)
{
	int current = cwd_node;
	if (!path || path[0] == '\0')
		return current;
	if (path[0] == '/') {
		current = root_node;
		++path;
	}
	if (current < 0)
		return -1;
	char component[VFS_NAME_SIZE];
	while (*path) {
		while (*path == '/')
			++path;
		if (!*path)
			break;
		size_t pos = 0;
		while (*path && *path != '/' && pos + 1 < VFS_NAME_SIZE)
			component[pos++] = *path++;
		component[pos] = '\0';
		if (kstrcmp(component, ".") == 0)
			continue;
		if (kstrcmp(component, "..") == 0) {
			if (nodes[current].parent >= 0)
				current = nodes[current].parent;
			continue;
		}
		int child = vfs_find_child(current, component);
		if (child < 0)
			return -1;
		current = child;
	}
	return current;
}

static int vfs_resolve_parent(const char *path, char *basename)
{
	if (!path || path[0] == '\0')
		return -1;
	const char *slash = 0;
	const char *p = path;
	while (*p) {
		if (*p == '/')
			slash = p;
		++p;
	}
	if (!slash) {
		kstrcpy(basename, path, VFS_NAME_SIZE);
		return cwd_node;
	}
	size_t parent_len = (size_t)(slash - path);
	if (parent_len == 0) {
		kstrcpy(basename, slash + 1, VFS_NAME_SIZE);
		return root_node;
	}
	const char *name_start = slash + 1;
	while (*name_start == '/')
		++name_start;
	kstrcpy(basename, name_start, VFS_NAME_SIZE);
	char parent_path[128];
	size_t len = 0;
	const char *q = path;
	while (q < slash && len + 1 < sizeof(parent_path))
		parent_path[len++] = *q++;
	if (len > 1 && parent_path[len - 1] == '/')
		--len;
	parent_path[len] = '\0';
	if (len == 0)
		return root_node;
	return vfs_resolve(parent_path);
}

static int vfs_create_node(const char *path, bool dir)
{
	char name[VFS_NAME_SIZE];
	int parent = vfs_resolve_parent(path, name);
	if (parent < 0)
		return -1;
	if (!nodes[parent].is_dir)
		return -1;
	if (name[0] == '\0')
		return -1;
	int existing = vfs_find_child(parent, name);
	if (existing >= 0)
		return -1;
	int node = -1;
	for (int i = 0; i < VFS_MAX_NODES; ++i) {
		if (nodes[i].name[0] == '\0' && (i != root_node || root_node < 0)) {
			node = i;
			break;
		}
	}
	if (node < 0)
		return -1;
	vfs_clear_node(node);
	kstrcpy(nodes[node].name, name, VFS_NAME_SIZE);
	nodes[node].is_dir = dir;
	nodes[node].read_only = false;
	nodes[node].parent = parent;
	nodes[node].child_count = 0;
	if (vfs_add_child(parent, node) < 0)
		return -1;
	return node;
}

static void vfs_build_path(int node, char *out, size_t max_len)
{
	if (node < 0) {
		out[0] = '\0';
		return;
	}
	if (nodes[node].parent < 0) {
		if (max_len > 1) {
			out[0] = '/';
			out[1] = '\0';
		} else if (max_len > 0) {
			out[0] = '\0';
		}
		return;
	}
	char tmp[256];
	vfs_build_path(nodes[node].parent, tmp, sizeof(tmp));
	size_t len = kstrlen(tmp);
	if (len > 0 && tmp[len - 1] != '/' && len + 1 < max_len) {
		if (len + 1 >= sizeof(tmp))
			return;
		tmp[len++] = '/';
		tmp[len] = '\0';
	}
	size_t i = 0;
	while (i < len && i + 1 < max_len) {
		out[i] = tmp[i];
		++i;
	}
	const char *name = nodes[node].name;
	while (*name && i + 1 < max_len)
		out[i++] = *name++;
	out[i] = '\0';
}

bool vfs_init(void)
{
	for (int i = 0; i < VFS_MAX_NODES; ++i) {
		nodes[i].name[0] = '\0';
		nodes[i].parent = -1;
		nodes[i].child_count = 0;
		nodes[i].own_data = 0;
	}
	data_used = 0;
	root_node = 0;
	vfs_clear_node(root_node);
	nodes[root_node].is_dir = true;
	nodes[root_node].read_only = true;
	nodes[root_node].parent = -1;
	cwd_node = root_node;

	size_t total = fs_file_count();
	for (size_t i = 0; i < total; ++i) {
		const struct fs_file *file = fs_file_at(i);
		if (!file || !file->name)
			continue;
		const char *path = file->name;
		int current = root_node;
		char buffer[VFS_NAME_SIZE];
		const char *p = path;
		while (*p) {
			while (*p == '/')
				++p;
			if (!*p)
				break;
			size_t pos = 0;
			while (*p && *p != '/' && pos + 1 < VFS_NAME_SIZE)
				buffer[pos++] = *p++;
			buffer[pos] = '\0';
			int child = vfs_find_child(current, buffer);
			if (child < 0) {
				int node = -1;
				if (*p != '\0') {
					node = vfs_new_node();
					if (node >= 0) {
						vfs_clear_node(node);
						kstrcpy(nodes[node].name, buffer, VFS_NAME_SIZE);
						nodes[node].is_dir = true;
						nodes[node].read_only = true;
						nodes[node].parent = current;
						nodes[node].child_count = 0;
						if (vfs_add_child(current, node) < 0)
							continue;
					}
				} else {
					node = vfs_new_node();
					if (node >= 0) {
						vfs_clear_node(node);
						kstrcpy(nodes[node].name, buffer, VFS_NAME_SIZE);
						nodes[node].is_dir = false;
						nodes[node].read_only = true;
						nodes[node].data = (const char *)file->data;
						nodes[node].size = file->size;
						nodes[node].parent = current;
						nodes[node].child_count = 0;
						if (vfs_add_child(current, node) < 0)
							continue;
					}
				}
				if (node < 0)
					break;
				child = node;
			}
			current = child;
		}
	}
	return true;
}

const char *vfs_pwd(void)
{
	static char buffer[256];
	vfs_build_path(cwd_node, buffer, sizeof(buffer));
	return buffer;
}

bool vfs_cd(const char *path)
{
	int node = vfs_resolve(path);
	if (node < 0 || !nodes[node].is_dir)
		return false;
	cwd_node = node;
	return true;
}

bool vfs_list(const char *path)
{
	int node = path && path[0] ? vfs_resolve(path) : cwd_node;
	if (node < 0 || !nodes[node].is_dir)
		return false;
	for (int i = 0; i < nodes[node].child_count; ++i) {
		int child = nodes[node].children[i];
		if (nodes[child].is_dir)
			console_print("/ ");
		else
			console_print("  ");
		console_print(nodes[child].name);
		console_print("\n");
	}
	return true;
}

bool vfs_cat(const char *path)
{
	int node = vfs_resolve(path);
	if (node < 0 || nodes[node].is_dir)
		return false;
	const char *data = nodes[node].data;
	size_t size = nodes[node].size;
	for (size_t i = 0; i < size; ++i) {
		char c = data[i];
		console_putchar(c ? c : '\n');
	}
	console_print("\n");
	return true;
}

bool vfs_mkdir(const char *path)
{
	return vfs_create_node(path, true) >= 0;
}

bool vfs_touch(const char *path)
{
	int existing = vfs_resolve(path);
	if (existing >= 0)
		return true;
	int node = vfs_create_node(path, false);
	if (node < 0)
		return false;
	nodes[node].data = "";
	nodes[node].size = 0;
	return true;
}

static bool vfs_remove_node(int node)
{
	if (node < 0 || nodes[node].read_only)
		return false;
	if (nodes[node].is_dir && nodes[node].child_count > 0)
		return false;
	int parent = nodes[node].parent;
	if (parent >= 0) {
		int write = 0;
		for (int i = 0; i < nodes[parent].child_count; ++i) {
			if (nodes[parent].children[i] == node)
				continue;
			nodes[parent].children[write++] = nodes[parent].children[i];
		}
		nodes[parent].child_count = write;
	}
	vfs_clear_node(node);
	return true;
}

bool vfs_remove(const char *path)
{
	int node = vfs_resolve(path);
	if (node < 0 || nodes[node].is_dir)
		return false;
	return vfs_remove_node(node);
}

bool vfs_rmdir(const char *path)
{
	int node = vfs_resolve(path);
	if (node < 0 || !nodes[node].is_dir)
		return false;
	return vfs_remove_node(node);
}

bool vfs_cp(const char *src, const char *dst)
{
	int node = vfs_resolve(src);
	if (node < 0 || nodes[node].is_dir)
		return false;
	int target = vfs_resolve(dst);
	if (target >= 0)
		return false;
	char name[VFS_NAME_SIZE];
	int parent = vfs_resolve_parent(dst, name);
	if (parent < 0 || !nodes[parent].is_dir)
		return false;
	int child = vfs_create_node(dst, false);
	if (child < 0)
		return false;
	char *storage = vfs_alloc_data(nodes[node].data, nodes[node].size);
	if (!storage)
		return false;
	nodes[child].data = storage;
	nodes[child].size = nodes[node].size;
	nodes[child].own_data = storage;
	nodes[child].own_size = nodes[node].size;
	return true;
}

bool vfs_mv(const char *src, const char *dst)
{
	int node = vfs_resolve(src);
	if (node < 0 || nodes[node].read_only)
		return false;
	char name[VFS_NAME_SIZE];
	int parent = vfs_resolve_parent(dst, name);
	if (parent < 0 || !nodes[parent].is_dir)
		return false;
	int existing = vfs_find_child(parent, name);
	if (existing >= 0)
		return false;
	int old_parent = nodes[node].parent;
	if (old_parent >= 0) {
		int write = 0;
		for (int i = 0; i < nodes[old_parent].child_count; ++i) {
			if (nodes[old_parent].children[i] == node)
				continue;
			nodes[old_parent].children[write++] = nodes[old_parent].children[i];
		}
		nodes[old_parent].child_count = write;
	}
	kstrcpy(nodes[node].name, name, VFS_NAME_SIZE);
	vfs_add_child(parent, node);
	return true;
}

bool vfs_write(const char *path, const char *content, size_t length)
{
	int node = vfs_resolve(path);
	if (node < 0) {
		node = vfs_create_node(path, false);
		if (node < 0)
			return false;
	}
	if (nodes[node].is_dir)
		return false;
	char *storage = vfs_alloc_data(content, length);
	if (!storage)
		return false;
	nodes[node].data = storage;
	nodes[node].size = length;
	nodes[node].own_data = storage;
	nodes[node].own_size = length;
	return true;
}

const void *vfs_read(const char *path)
{
	int node = vfs_resolve(path);
	if (node < 0 || nodes[node].is_dir)
		return 0;
	return nodes[node].data;
}

size_t vfs_get_size(const char *path)
{
	int node = vfs_resolve(path);
	if (node < 0 || nodes[node].is_dir)
		return 0;
	return nodes[node].size;
}

bool vfs_exists(const char *path)
{
	return vfs_resolve(path) >= 0;
}
