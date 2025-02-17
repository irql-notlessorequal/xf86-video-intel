/***************************************************************************

 Copyright 2013 Intel Corporation.  All Rights Reserved.

 Permission is hereby granted, free of charge, to any person obtaining a
 copy of this software and associated documentation files (the
 "Software"), to deal in the Software without restriction, including
 without limitation the rights to use, copy, modify, merge, publish,
 distribute, sub license, and/or sell copies of the Software, and to
 permit persons to whom the Software is furnished to do so, subject to
 the following conditions:

 The above copyright notice and this permission notice (including the
 next paragraph) shall be included in all copies or substantial portions
 of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 IN NO EVENT SHALL INTEL, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
 THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 **************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>

#if MAJOR_IN_MKDEV
#include <sys/mkdev.h>
#elif MAJOR_IN_SYSMACROS
#include <sys/sysmacros.h>
#endif

#include <pciaccess.h>

#include <xorg-server.h>
#include <xf86.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <xf86_OSproc.h>
#include <i915_drm.h>

#ifdef XSERVER_PLATFORM_BUS
#include <xf86platformBus.h>
#endif

#ifdef HAVE_VALGRIND
#include <valgrind.h>
#include <memcheck.h>
#define VG(x) x
#else
#define VG(x)
#endif

#define VG_CLEAR(s) VG(memset(&s, 0, sizeof(s)))

#include "intel_driver.h"
#include "fd.h"

struct intel_device {
	int idx;
	char *master_node;
	char *render_node;
	int fd;
	int device_id;
	int open_count;
	int master_count;
} __attribute__((packed));

static int intel_device_key = -1;

static int dump_file(ScrnInfoPtr scrn, const char *path)
{
	FILE *file;
	size_t len = 0;
	char *line = NULL;

	file = fopen(path, "r");
	if (file == NULL)
		return 0;

	xf86DrvMsg(scrn->scrnIndex, X_INFO, "[drm] Contents of '%s':\n", path);
	while (getline(&line, &len, file) != -1)
		xf86DrvMsg(scrn->scrnIndex, X_INFO, "[drm] %s", line);

	free(line);
	fclose(file);
	return 1;
}

static int __find_debugfs(void)
{
	int i;

	for (i = 0; i < DRM_MAX_MINOR; i++) {
		char path[80];

		sprintf(path, "/sys/kernel/debug/dri/%d/i915_wedged", i);
		if (access(path, R_OK) == 0)
			return i;

		sprintf(path, "/debug/dri/%d/i915_wedged", i);
		if (access(path, R_OK) == 0)
			return i;
	}

	return -1;
}

static int drm_get_minor(int fd)
{
	struct stat st;

	if (fstat(fd, &st))
		return __find_debugfs();

	if (!S_ISCHR(st.st_mode))
		return __find_debugfs();

	return st.st_rdev & 0x63;
}

#if __linux__
#include <sys/mount.h>

static void dump_debugfs(ScrnInfoPtr scrn, int fd, const char *name)
{
	char path[80];
	int minor;

	minor = drm_get_minor(fd);
	if (minor < 0)
		return;

	sprintf(path, "/sys/kernel/debug/dri/%d/%s", minor, name);
	if (dump_file(scrn, path))
		return;

	sprintf(path, "/debug/dri/%d/%s", minor, name);
	if (dump_file(scrn, path))
		return;

	if (mount("X-debug", "/sys/kernel/debug", "debugfs", 0, 0) == 0) {
		sprintf(path, "/sys/kernel/debug/dri/%d/%s", minor, name);
		dump_file(scrn, path);
		umount("X-debug");
		return;
	}
}
#else
static void dump_debugfs(ScrnInfoPtr scrn, int fd, const char *name) { }
#endif

static void dump_clients_info(ScrnInfoPtr scrn, int fd)
{
	dump_debugfs(scrn, fd, "clients");
}

static int __intel_get_device_id(int fd)
{
	struct drm_i915_getparam gp;
	int devid = 0;

	VG_CLEAR(gp);
	gp.param = I915_PARAM_CHIPSET_ID;
	gp.value = &devid;

	if (drmIoctl(fd, DRM_IOCTL_I915_GETPARAM, &gp))
		return 0;

	return devid;
}

int intel_entity_get_devid(int idx)
{
	struct intel_device *dev;

	dev = xf86GetEntityPrivate(idx, intel_device_key)->ptr;
	if (dev == NULL)
		return 0;

	return dev->device_id;
}

static inline struct intel_device *intel_device(ScrnInfoPtr scrn)
{
	if (scrn->entityList == NULL)
		return NULL;

	return xf86GetEntityPrivate(scrn->entityList[0], intel_device_key)->ptr;
}

static const char *kernel_module_names[] ={
	"i915",
	NULL,
};

static int is_i915_device(int fd)
{
	drm_version_t version;
	const char **kn;
	char name[5] = "";

	memset(&version, 0, sizeof(version));
	version.name_len = 4;
	version.name = name;

	if (drmIoctl(fd, DRM_IOCTL_VERSION, &version))
		return 0;

	for (kn = kernel_module_names; *kn; kn++)
		if (strcmp(*kn, name) == 0)
			return 1;

	return 0;
}

static int load_i915_kernel_module(void)
{
	const char **kn;

	for (kn = kernel_module_names; *kn; kn++)
		if (xf86LoadKernelModule(*kn))
			return 0;

	return -1;
}

static int is_i915_gem(int fd)
{
	int ret = is_i915_device(fd);

	if (ret) {
		struct drm_i915_getparam gp;

		VG_CLEAR(gp);
		gp.param = I915_PARAM_HAS_GEM;
		gp.value = &ret;

		if (drmIoctl(fd, DRM_IOCTL_I915_GETPARAM, &gp))
			ret = 0;
	}

	return ret;
}

static int __intel_check_device(int fd)
{
	int ret;

	/* Confirm that this is a i915.ko device with GEM/KMS enabled */
	ret = is_i915_gem(fd);
	if (ret && !hosted()) {
		struct drm_mode_card_res res;

		memset(&res, 0, sizeof(res));
		if (drmIoctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res))
			ret = 0;
	}

	return ret;
}

static int open_cloexec(const char *path)
{
	struct stat st;
	int loop = 1000;
	int fd;

	/* No file? Assume the driver is loading slowly */
	while (stat(path, &st) == -1 && errno == ENOENT && --loop)
		usleep(50000);

	if (loop != 1000)
		ErrorF("intel: waited %d ms for '%s' to appear\n",
		       (1000 - loop) * 50, path);

	fd = -1;
#ifdef O_CLOEXEC
	fd = open(path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
#endif
	if (fd == -1)
		fd = fd_set_cloexec(open(path, O_RDWR | O_NONBLOCK));

	return fd;
}

#ifdef __linux__
static int __intel_open_device__major_minor(int _major, int _minor)
{
	DIR *dir;
	struct dirent *de;
	char path[9+sizeof(de->d_name)];
	int base, fd = -1;

	base = sprintf(path, "/dev/dri/");

	dir = opendir(path);
	if (dir == NULL)
		return -1;

	while ((de = readdir(dir)) != NULL) {
		struct stat st;

		if (*de->d_name == '.')
			continue;

		sprintf(path + base, "%s", de->d_name);
		if (stat(path, &st) == 0 &&
		    major(st.st_rdev) == _major &&
		    minor(st.st_rdev) == _minor) {
			fd = open_cloexec(path);
			break;
		}
	}

	closedir(dir);

	return fd;
}

static int __intel_open_device__pci(const struct pci_device *pci)
{
	struct stat st;
	struct dirent *de;
	char path[64+sizeof(de->d_name)];
	DIR *dir;
	int base;
	int fd;

	/* Look up the major:minor for the drm device through sysfs.
	 * First we need to check that sysfs is available, then
	 * check that we have loaded our driver. When we are happy
	 * that our KMS module is loaded, we can then search for our
	 * device node. We make the assumption that it uses the same
	 * name, but after that we read the major:minor assigned to us
	 * and search for a matching entry in /dev.
	 */

	base = sprintf(path,
		       "/sys/bus/pci/devices/%04x:%02x:%02x.%d/",
		       pci->domain, pci->bus, pci->dev, pci->func);
	if (stat(path, &st))
		return -1;

	sprintf(path + base, "drm");
	dir = opendir(path);
	if (dir == NULL) {
		int loop = 0;

		sprintf(path + base, "driver");
		if (stat(path, &st)) {
			if (load_i915_kernel_module())
				return -1;
			(void)xf86LoadKernelModule("fbcon");
		}

		sprintf(path + base, "drm");
		while ((dir = opendir(path)) == NULL && loop++ < 100)
			usleep(20000);

		ErrorF("intel: waited %d ms for i915.ko driver to load\n", loop * 20000 / 1000);

		if (dir == NULL)
			return -1;
	}

	fd = -1;
	while ((de = readdir(dir)) != NULL) {
		if (*de->d_name == '.')
			continue;

		if (strncmp(de->d_name, "card", 4) == 0) {
			sprintf(path + base + 4, "/dev/dri/%s", de->d_name);
			fd = open_cloexec(path + base + 4);
			if (fd != -1)
				break;

			sprintf(path + base + 3, "/%s/dev", de->d_name);
			fd = open(path, O_RDONLY);
			if (fd == -1)
				break;

			base = read(fd, path, sizeof(path) - 1);
			close(fd);

			fd = -1;
			if (base > 0) {
				int major, minor;
				path[base] = '\0';
				if (sscanf(path, "%d:%d", &major, &minor) == 2)
					fd = __intel_open_device__major_minor(major, minor);
			}
			break;
		}
	}
	closedir(dir);

	return fd;
}
#else
static int __intel_open_device__pci(const struct pci_device *pci) { return -1; }
#endif

static int __intel_open_device__legacy(const struct pci_device *pci)
{
	char id[20];
	int ret;

	snprintf(id, sizeof(id),
		 "pci:%04x:%02x:%02x.%d",
		 pci->domain, pci->bus, pci->dev, pci->func);

	ret = drmCheckModesettingSupported(id);
	if (ret) {
		if (load_i915_kernel_module() == 0)
			ret = drmCheckModesettingSupported(id);
		if (ret)
			return -1;
		/* Be nice to the user and load fbcon too */
		(void)xf86LoadKernelModule("fbcon");
	}

	return fd_set_nonblock(drmOpen(NULL, id));
}

static int __intel_open_device(const struct pci_device *pci, const char *path)
{
	int fd;

	if (path == NULL) {
		if (pci == NULL)
			return -1;

		fd = __intel_open_device__pci(pci);
		if (fd == -1)
			fd = __intel_open_device__legacy(pci);
	} else
		fd = open_cloexec(path);

	return fd;
}

static char *find_master_node(int fd)
{
	struct stat st, master;
	char buf[128];

	if (fstat(fd, &st))
		return NULL;

	if (!S_ISCHR(st.st_mode))
		return NULL;

	sprintf(buf, "/dev/dri/card%d", (int)(st.st_rdev & 0x7f));
	if (stat(buf, &master) == 0 &&
	    S_ISCHR(master.st_mode) &&
	    (st.st_rdev & 0x7f) == master.st_rdev)
		return strdup(buf);

	/* Fallback to iterating over the usual suspects */
	return drmGetDeviceNameFromFd(fd);
}

static int is_render_node(int fd, struct stat *st)
{
	if (fstat(fd, st))
		return -1;

	if (!S_ISCHR(st->st_mode))
		return -1;

	return st->st_rdev & 0x80;
}

static char *find_render_node(int fd)
{
	struct stat master, render;
	char buf[128];
	int i;

	/* Are we a render-node ourselves? */
	if (is_render_node(fd, &master))
		return NULL;

	sprintf(buf, "/dev/dri/renderD%d", (int)((master.st_rdev | 0x80) & 0xbf));
	if (stat(buf, &render) == 0 &&
	    S_ISCHR(render.st_mode) &&
	    render.st_rdev == (master.st_rdev | 0x80))
		return strdup(buf);

	/* Misaligned card <-> renderD, do a full search */
	for (i = 0; i < 16; i++) {
		sprintf(buf, "/dev/dri/renderD%d", i + 128);
		if (stat(buf, &render) == 0 &&
		    S_ISCHR(render.st_mode) &&
		    render.st_rdev == (master.st_rdev | 0x80))
			return strdup(buf);
	}

	return NULL;
}

#if defined(ODEV_ATTRIB_PATH)
static char *get_path(struct xf86_platform_device *dev)
{
	const char *path;

	if (dev == NULL)
		return NULL;

	path = xf86_get_platform_device_attrib(dev, ODEV_ATTRIB_PATH);
	if (path == NULL)
		return NULL;

	return strdup(path);
}

#else

static char *get_path(struct xf86_platform_device *dev)
{
	return NULL;
}
#endif


#if defined(ODEV_ATTRIB_FD)
static int get_fd(struct xf86_platform_device *dev)
{
	if (dev == NULL)
		return -1;

	return xf86_get_platform_device_int_attrib(dev, ODEV_ATTRIB_FD, -1);
}

#else

static int get_fd(struct xf86_platform_device *dev)
{
	return -1;
}
#endif

static int is_master(int fd)
{
	drmSetVersion sv;

	sv.drm_di_major = 1;
	sv.drm_di_minor = 1;
	sv.drm_dd_major = -1;
	sv.drm_dd_minor = -1;
	
	return drmIoctl(fd, DRM_IOCTL_SET_VERSION, &sv) == 0;
}

int intel_open_device(int entity_num,
		      const struct pci_device *pci,
		      struct xf86_platform_device *platform)
{
	struct intel_device *dev;
	char *path;
	int fd, master_count;

	if (intel_device_key == -1)
		intel_device_key = xf86AllocateEntityPrivateIndex();
	if (intel_device_key == -1)
		return -1;

	dev = xf86GetEntityPrivate(entity_num, intel_device_key)->ptr;
	if (dev)
		return dev->fd;

	path = get_path(platform);

	master_count = 1; /* DRM_MASTER is managed by Xserver */
	fd = get_fd(platform);
	if (fd == -1) {
		fd = __intel_open_device(pci, path);
		if (fd == -1)
			goto err_path;

		master_count = 0;
	}

	if (path == NULL) {
		path = find_master_node(fd);
		if (path == NULL)
			goto err_close;
	}

	if (!__intel_check_device(fd))
		goto err_close;

	dev = malloc(sizeof(*dev));
	if (dev == NULL)
		goto err_close;

	/* If hosted under a system compositor, just pretend to be master */
	if (hosted())
	{
		master_count++;
	}

	/* Non-root user holding MASTER, don't let go */
	if (geteuid() && is_master(fd))
	{
		master_count++;
	}

	if (pci)
	{
		dev->device_id = pci->device_id;
	}
	else
	{
		dev->device_id = __intel_get_device_id(fd);
	}

	dev->idx = entity_num;
	dev->fd = fd;
	dev->open_count = master_count;
	dev->master_count = master_count;
	dev->master_node = path;
	dev->render_node = find_render_node(fd);
	if (dev->render_node == NULL)
		dev->render_node = dev->master_node;

	/**
	 * Check if we have nvidia-drm loaded. find_render_node() is broken and will pick up the wrong node
	 * if the NVIDIA driver acts as the PRIMARY DRM node.
	 */
	if (__requires_nvidia_drm_workaround(dev)) {
		xf86Msg(X_WARNING, "[intel::intel_open_device] NVIDIA driver detected, applying workaround.\n");

		if (!__get_correct_render_node(dev))
		{
#ifdef __linux__
			xf86Msg(X_ERROR, "[intel::intel_open_device] Workaround failed, making a bold assumption instead.\n");
#else
			xf86Msg(X_ERROR, "[intel::intel_open_device] Workaround unavailable, making a bold assumption instead.\n");
#endif
			dev->render_node = "/dev/dri/renderD128";
		}

#if HAS_DEBUG_FULL
		xf86Msg(X_DEBUG, "[intel::intel_open_device] idx=%i,fd=%i,master_count=%i,master_node=%s,render_node=%s\n", dev->idx,dev->fd, dev->master_count, dev->master_node, dev->render_node);
#endif
	}

	xf86GetEntityPrivate(entity_num, intel_device_key)->ptr = dev;

	return fd;
err_close:
	if (master_count == 0) /* Don't close server-fds */
		close(fd);
err_path:
	free(path);
	return -1;
}

void intel_close_device(int entity_num)
{
	struct intel_device *dev;

	if (intel_device_key == -1)
		return;

	dev = xf86GetEntityPrivate(entity_num, intel_device_key)->ptr;
	xf86GetEntityPrivate(entity_num, intel_device_key)->ptr = NULL;
	if (!dev)
		return;

	if (dev->master_count == 0) /* Don't close server-fds */
		close(dev->fd);

	if (dev->render_node != dev->master_node)
		free(dev->render_node);
	free(dev->master_node);
	free(dev);
}

int __intel_peek_fd(ScrnInfoPtr scrn)
{
	struct intel_device *dev;

	dev = intel_device(scrn);
	assert(dev && dev->fd != -1);

	return dev->fd;
}

int intel_has_render_node(struct intel_device *dev)
{
	struct stat st;

	assert(dev && dev->fd != -1);
	return is_render_node(dev->fd, &st);
}

struct intel_device *intel_get_device(ScrnInfoPtr scrn, int *fd)
{
	struct intel_device *dev;
	int ret;

	dev = intel_device(scrn);
	if (dev == NULL)
		return NULL;

	assert(dev->fd != -1);

	if (dev->open_count++ == 0) {
		drmSetVersion sv;
		int retry = 2000;

		assert(!hosted());

		/* Check that what we opened was a master or a
		 * master-capable FD, by setting the version of the
		 * interface we'll use to talk to it.
		 */
		do {
			sv.drm_di_major = 1;
			sv.drm_di_minor = 1;
			sv.drm_dd_major = -1;
			sv.drm_dd_minor = -1;
			ret = drmIoctl(dev->fd, DRM_IOCTL_SET_VERSION, &sv);
			if (ret == 0)
				break;

			usleep(1000);
		} while (--retry);
		if (ret != 0) {
			xf86DrvMsg(scrn->scrnIndex, X_ERROR,
				   "[drm] failed to set drm interface version: %s [%d].\n",
				   strerror(errno), errno);
			dump_clients_info(scrn, dev->fd);
			dev->open_count--;
			return NULL;
		}
	}

	*fd = dev->fd;
	return dev;
}

const char *intel_get_master_name(struct intel_device *dev)
{
	assert(dev && dev->master_node);
	return dev->master_node;
}

const char *intel_get_client_name(struct intel_device *dev)
{
	assert(dev && dev->render_node);
	return dev->render_node;
}

static int authorise(struct intel_device *dev, int fd)
{
	struct stat st;
	drm_magic_t magic;

	if (is_render_node(fd, &st)) /* restricted authority, do not elevate */
		return 1;

	return drmGetMagic(fd, &magic) == 0 && drmAuthMagic(dev->fd, magic) == 0;
}

int intel_get_client_fd(struct intel_device *dev)
{
	int fd = -1;

	assert(dev && dev->fd != -1);
	assert(dev->render_node);

#ifdef O_CLOEXEC
	fd = open(dev->render_node, O_RDWR | O_CLOEXEC);
#endif
	if (fd < 0)
		fd = fd_set_cloexec(open(dev->render_node, O_RDWR));
	if (fd < 0)
		return -BadAlloc;

	if (!authorise(dev, fd)) {
		close(fd);
		return -BadMatch;
	}

	assert(is_i915_gem(fd));

	return fd;
}

int intel_get_device_id(struct intel_device *dev)
{
	assert(dev && dev->fd != -1);
	return dev->device_id;
}

int intel_get_master(struct intel_device *dev)
{
	int ret;

	assert(dev && dev->fd != -1);

	ret = 0;
	if (dev->master_count++ == 0) {
		int retry = 2000;

		assert(!hosted());
		do {
			ret = drmSetMaster(dev->fd);
			if (ret == 0)
				break;
			usleep(1000);
		} while (--retry);
	}

	return ret;
}

int intel_put_master(struct intel_device *dev)
{
	int ret;

	assert(dev && dev->fd != -1);

	ret = 0;
	assert(dev->master_count);
	if (--dev->master_count == 0) {
		assert(!hosted());
		assert(drmSetMaster(dev->fd) == 0);
		ret = drmDropMaster(dev->fd);
	}

	return ret;
}

void intel_put_device(struct intel_device *dev)
{
	assert(dev && dev->fd != -1);

	assert(dev->open_count);
	if (--dev->open_count)
		return;

	assert(!hosted());
	xf86GetEntityPrivate(dev->idx, intel_device_key)->ptr = NULL;

	drmClose(dev->fd);
	if (dev->render_node != dev->master_node)
		free(dev->render_node);
	free(dev->master_node);
	free(dev);
}

int intel_is_same_file(int fd1, int fd2) {
	struct stat stat1, stat2;
	if (fstat(fd1, &stat1) < 0) return -1;
	if (fstat(fd2, &stat2) < 0) return -1;
	return (stat1.st_dev == stat2.st_dev) && (stat1.st_ino == stat2.st_ino);
}

char *intel_str_replace(char *orig, char *rep, char *with) {
	char *result;
	char *ins;
	char *tmp;
	int len_rep;
	int len_with;
	int len_front;
	int count;

	if (!orig || !rep)
		return NULL;

	len_rep = strlen(rep);
	if (len_rep == 0)
		return NULL;

	if (!with)
		with = "";
	len_with = strlen(with);

	ins = orig;
	for (count = 0; (tmp = strstr(ins, rep)); ++count) {
		ins = tmp + len_rep;
	}

	tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

	if (!result)
		return NULL;

	while (count--) {
		ins = strstr(orig, rep);
		len_front = ins - orig;
		tmp = strncpy(tmp, orig, len_front) + len_front;
		tmp = strcpy(tmp, with) + len_with;
		orig += len_front + len_rep; // move to next "end of rep"
	}

	strcpy(tmp, orig);
	return result;
}

#ifdef __linux__
int __get_render_node_count(void)
{
	DIR *d;
	struct dirent *dir;
	d = opendir("/dev/dri/by-path/");

	if (!d) {
		xf86Msg(X_ERROR, "[intel::__get_render_node_count] Failed to open /dev/dri/by-path!\n");
		return 0;
	}

	int count = 0;
	while ((dir = readdir(d)) != NULL)
	{
		/* We need card nodes. Only allow them. */
		if (strstr(dir->d_name, "-card") == NULL)
			continue;

		char curr_path[PATH_MAX] = {0};
		strcat(curr_path, "/dev/dri/by-path/");
		strcat(curr_path, dir->d_name);

		char true_path[PATH_MAX];
		if (!realpath(curr_path, true_path))
		{
			xf86Msg(X_ERROR, "[intel::__get_render_node_count] realpath(%s) returned an error.\n", curr_path);
			continue;
		}

		/* Increment the counter. */
		count++;
	}

	closedir(d);
	return count;
}

int __get_correct_render_node(struct intel_device *dev)
{
	DIR *d;
	struct dirent *dir;
	d = opendir("/dev/dri/by-path/");

	if (!d) {
		xf86Msg(X_ERROR, "[intel::__get_correct_render_node] Failed to open /dev/dri/by-path!\n");
		return 0;
	}

	while ((dir = readdir(d)) != NULL)
	{
		/* We need card nodes. Only allow them. */
		if (strstr(dir->d_name, "-card") == NULL)
			continue;

		char curr_path[PATH_MAX] = {0};
		strcat(curr_path, "/dev/dri/by-path/");
		strcat(curr_path, dir->d_name);

		char true_path[PATH_MAX];
		if (!realpath(curr_path, true_path))
		{
			xf86Msg(X_ERROR, "[intel::__get_correct_render_node:0] realpath(%s) returned an error.\n", curr_path);
			continue;
		}

		/* We have the truepath, check if card nodes match our assumed card node. */
		if (strcmp(dev->master_node, true_path) != 0)
		{
#if HAS_DEBUG_FULL
			xf86Msg(X_DEBUG, "[intel::__get_correct_render_node] strcmp(%s,%s) mismatch.\n", dev->master_node, true_path);
#endif
			continue;
		}

		/* Get the render node and find the realpath, and return that. */
		char* render_node_candidate = intel_str_replace(curr_path, "-card", "-render");

		int test_fd = open(render_node_candidate, O_RDONLY);
		if (!test_fd)
		{
#if HAS_DEBUG_FULL
			xf86Msg(X_DEBUG, "[intel::__get_correct_render_node] render node candidate test failed.\n");
#endif
			free(render_node_candidate);
			continue;
		}

		/* We don't need the FD anymore. */
		close(test_fd);

		/* Our node probably exists, we're good. */
		char correct_render_node[PATH_MAX] = {0};
		if (!realpath(render_node_candidate, correct_render_node))
		{
			xf86Msg(X_ERROR, "[intel::__get_correct_render_node:1] realpath(%s) returned an error.\n", render_node_candidate);

			free(render_node_candidate);
			continue;
		}

		/* Clean up before returning. */
		free(render_node_candidate);

		strcpy(dev->render_node, correct_render_node);
		break;
	}

	closedir(d);
	return 1;
}
#else
int __get_render_node_count(void)
{
	return 1;
}

int __get_correct_render_node(struct intel_device *dev)
{
	/* Unsupported environment, surely nothing will go wrong... */
	return 0;
}
#endif

int __requires_nvidia_drm_workaround(struct intel_device *dev)
{
	/* Check if we're root or DRM master, if so then workaround not required. */
	if (strcmp(dev->master_node, "/dev/dri/card0") == 0)
	{
#if HAS_DEBUG_FULL
		xf86Msg(X_DEBUG, "[intel::__requires_nvidia_drm_workaround] STRCMP OK.\n");
#endif
		return 0;
	}

	/* Must be RD_WR as we will be sending an IOCTL to it. */
	int fd_root = open("/dev/dri/card0", O_RDWR);
	if (!fd_root)
	{
		xf86Msg(X_ERROR, "[intel::__requires_nvidia_drm_workaround] FD_ROOT returned an error.\n");
		return 0;
	}

	int fd_curr = open(dev->master_node, O_RDONLY);
	/* Same card instance, don't worry about it. */
	if (intel_is_same_file(fd_root, fd_curr))
	{
#if HAS_DEBUG_FULL
		xf86Msg(X_NONE, "[intel::__requires_nvidia_drm_workaround] FD_ROOT is same as FD_CURR.\n");
#endif
		close(fd_curr);
		close(fd_root);
		return 0;
	}

	/**
	 * Previously we checked if 'fd_root' was nvidia-drm, while this tends to work,
	 * if we insert a third GPU, everything goes wrong. Oops.
	 * 
	 * Check how many GPUs we have present and then make a dumb choice.
	 */
	int gpu_count = __get_render_node_count();

	if (gpu_count <= 1)
	{
		/* Something went wrong, bail out. */
		return false;
	}
	else if (gpu_count == 2)
	{
		drmVersionPtr version = drmGetVersion(fd_root);

		close(fd_curr);
		close(fd_root);
		return strcmp(version->name, "nvidia-drm") == 0;
	}
	else
	{
		/* We likely need to use the workaround, no I can't bothered to iterate and check. */

		close(fd_curr);
		close(fd_root);
		return true;
	}
}
