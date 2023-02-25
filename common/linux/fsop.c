/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Platform-specific filesystem operations
 */

#define _DEFAULT_SOURCE
#define _FILE_OFFSET_BITS	64
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <malloc.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <ufprog/osdef.h>
#include <ufprog/dirs.h>
#include <ufprog/log.h>

struct os_file_handle {
	int fd;
	char *path;
};

struct os_file_mapping {
	struct os_file_handle file;
	uint64_t max_size;
	size_t page_size;
	size_t mapping_size;
	uint64_t curr_file_offset;
	size_t curr_mapping_size;
	void *curr_mapping;
	bool writable;
};

ufprog_bool UFPROG_API os_is_valid_filename(const char *filename)
{
	size_t i, len;
	int j;

	static const char invchars[] = { '/' };

	if (!filename)
		return false;

	len = strlen(filename);
	if (!len)
		return false;

	for (i = 0; i < len; i++) {
		for (j = 0; j < ARRAY_SIZE(invchars); j++) {
			if (filename[i] == invchars[j])
				return false;
		}
	}

	if (filename[len - 1] == ' ' || filename[len - 1] == '\t')
		return false;

	return true;
}

ufprog_bool UFPROG_API os_mkdir_p(const char *path)
{
	char *pathbuf = strdup(path), *p;
	int err = 0;

	if (!pathbuf) {
		log_err("No memory for path buffer\n");
		return false;
	}

	p = pathbuf;

	do {
		p = strchr(p, '/');
		if (p)
			*p = 0;

		if (*pathbuf) {
			if (mkdir(pathbuf, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0) {
				err = errno;
				if (err != EEXIST) {
					log_err("opendir() failed with %u: %s\n", err, strerror(err));
					break;
				}

				err = 0;
			}
		}

		if (p) {
			*p = '/';
			p++;
		}
	} while (p);

	free(pathbuf);

	return err ? false : true;
}

static int __os_enum_file(const char *dir, const char *base, ufprog_bool recursive, void *priv, enum_file_cb cb)
{
	struct dirent *dent;
	int err, ret = 0;
	size_t dirlen;
	DIR *d;

	if (!dir || !cb)
		return false;

	if (!base)
		base = ".";

	dirlen = strlen(dir);

	d = opendir(dir);
	if (!d) {
		err = errno;
		if (err == ENOENT)
			return 0;

		log_err("opendir() failed with %u: %s\n", err, strerror(err));
		return -1;
	}

	errno = 0;
	dent = readdir(d);

	if (!dent) {
		err = errno;
		if (!err)
			goto cleanup;

		log_err("Initial readdir() failed with %u: %s\n", err, strerror(err));
		ret = -1;
		goto cleanup;
	}

	do {
		if (!strcmp(dent->d_name, ".") || !strcmp(dent->d_name, ".."))
			continue;

		if (dent->d_type == DT_DIR) {
			if (recursive) {
				char *subdir, *base_dir;

				if (dir[dirlen - 1] == '/')
					subdir = path_concat(true, 0, dir, dent->d_name, NULL);
				else
					subdir = path_concat(true, 0, dir, "", dent->d_name, NULL);

				if (!subdir) {
					log_err("Unable to build subdirectory\n");
					ret = -1;
					goto cleanup;
				}

				if (!strcmp(base, ""))
					base_dir = path_concat(false, 0, dent->d_name, NULL);
				else
					base_dir = path_concat(false, 0, base, "", dent->d_name, NULL);

				if (!base_dir) {
					log_err("Unable to build subdir enumration pattern\n");
					free(subdir);
					ret = -1;
					goto cleanup;
				}

				ret = __os_enum_file(subdir, base_dir, recursive, priv, cb);
				free(base_dir);
				free(subdir);
			}
		} else if (dent->d_type == DT_REG || dent->d_type == DT_LNK) {
			ret = cb(priv, base, dent->d_name);
		}

		errno = 0;
		dent = readdir(d);
	} while (!ret && dent);

	if (!dent) {
		err = errno;
		if (err) {
			log_err("readdir() failed with %u: %s\n", err, strerror(err));
			ret = -1;
		}
	}

cleanup:
	closedir(d);

	return ret;
}

ufprog_bool UFPROG_API os_enum_file(const char *dir, ufprog_bool recursive, void *priv, enum_file_cb cb)
{
	int ret;

	ret = __os_enum_file(dir, "", recursive, priv, cb);
	if (ret >= 0)
		return true;

	return false;
}

static ufprog_status __os_open_file(const char *file, ufprog_bool read, ufprog_bool write, ufprog_bool trunc,
				    ufprog_bool create, file_handle rethandle)
{
	int flags, mode, err;
	const char *opname;

	if (!read && !write) {
		log_err("Neither read nor write is specified for opening '%s'\n", file);
		return UFP_INVALID_PARAMETER;
	}

	if (read && !write) {
		opname = "read";
		flags = O_RDONLY;
		mode = 0;
	} else {
		mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

		if (!read && write) {
			opname = "write";
			flags = O_WRONLY;
		} else {
			opname = "read/write";
			flags = O_RDWR;
		}

		if (create)
			flags |= O_CREAT;

		if (trunc)
			flags |= O_TRUNC;
	}

	rethandle->fd = open(file, flags, mode);
	if (rethandle->fd < 0) {
		err = errno;
		if (err == ENOENT && !(flags & O_CREAT))
			return UFP_FILE_NOT_EXIST;
		else if (err == EINVAL)
			return UFP_FILE_NAME_INVALID;

		log_err("open() for %s to '%s' failed with %u: %s\n", opname, file, err, strerror(err));
		return UFP_FAIL;
	}

	return UFP_OK;
}

ufprog_status UFPROG_API os_open_file(const char *file, ufprog_bool read, ufprog_bool write, ufprog_bool trunc,
				      ufprog_bool create, file_handle *outhandle)
{
	file_handle handle;
	ufprog_status ret;
	size_t pathlen;

	if (!file || !outhandle)
		return UFP_INVALID_PARAMETER;

	pathlen = strlen(file);
	handle = malloc(sizeof(*handle) + pathlen + 1);
	if (!handle) {
		log_err("No memory for new file handle\n");
		return UFP_NOMEM;
	}

	handle->path = (char *)handle + sizeof(*handle);
	memcpy(handle->path, file, pathlen + 1);

	ret = __os_open_file(file, read, write, trunc, create, handle);
	if (ret) {
		free(handle);
		return ret;
	}

	*outhandle = handle;

	return UFP_OK;
}

ufprog_bool UFPROG_API os_close_file(file_handle handle)
{
	if (!handle)
		return false;

	close(handle->fd);

	free(handle);

	return true;
}

ufprog_bool UFPROG_API os_get_file_size(file_handle handle, uint64_t *retval)
{
	struct stat st;
	int err;

	if (!handle || !retval)
		return false;

	if (fstat(handle->fd, &st) < 0) {
		err = errno;
		log_err("fstat() for '%s' failed with %u: %s\n", handle->path, err, strerror(err));
		return false;
	}

	*retval = st.st_size;

	return true;
}

ufprog_bool UFPROG_API os_set_file_pointer(file_handle handle, enum os_file_seek_method method, uint64_t distance,
					   uint64_t *retpointer)
{
	int pos, err;
	off_t ret;

	if (!handle)
		return false;

	switch (method) {
	case FILE_SEEK_BEGIN:
		pos = SEEK_SET;
		break;

	case FILE_SEEK_CURR:
		pos = SEEK_CUR;
		break;

	case FILE_SEEK_END:
		pos = SEEK_END;
		break;

	default:
		log_err("Invalid file seek method %u\n", method);
		return false;
	}

	ret = lseek(handle->fd, distance, pos);
	if (ret < 0) {
		err = errno;
		log_err("lseek() for '%s' failed with %u: %s\n", handle->path, err, strerror(err));
		return false;
	}

	if (retpointer)
		*retpointer = ret;

	return true;
}

ufprog_bool UFPROG_API os_set_end_of_file(file_handle handle)
{
	off_t size;
	int err;

	if (!handle)
		return false;

	size = lseek(handle->fd, 0, SEEK_CUR);
	if (size < 0) {
		err = errno;
		log_err("lseek() for '%s' failed with %u: %s\n", handle->path, err, strerror(err));
		return false;
	}

	if (ftruncate(handle->fd, size) < 0) {
		err = errno;
		log_err("ftruncate() for '%s' failed with %u: %s\n", handle->path, err, strerror(err));
		return false;
	}

	return true;
}

ufprog_bool UFPROG_API os_read_file(file_handle handle, size_t len, void *buf, size_t *retlen)
{
	size_t chksz, num_read = 0;
	ufprog_bool ret = true;
	ssize_t len_read;
	int err;

	if (!handle)
		return false;

	if (!len) {
		if (retlen)
			*retlen = 0;
		return true;
	}

	if (!buf)
		return false;

	while (num_read < len) {
		if (len - num_read > SSIZE_MAX)
			chksz = SSIZE_MAX;
		else
			chksz = len - num_read;

		len_read = read(handle->fd, buf + num_read, chksz);
		if (len_read < 0) {
			err = errno;
			log_err("read() for '%s' failed with %u: %s\n", handle->path, err, strerror(err));
			ret = false;
			break;
		}

		num_read += len_read;
	}

	if (retlen)
		*retlen = num_read;

	return ret;
}

ufprog_bool UFPROG_API os_write_file(file_handle handle, size_t len, const void *buf, size_t *retlen)
{
	size_t chksz, num_written = 0;
	ufprog_bool ret = true;
	ssize_t len_write;
	int err;

	if (!handle)
		return false;

	if (!len) {
		if (retlen)
			*retlen = 0;
		return true;
	}

	if (!buf)
		return false;

	while (num_written < len) {
		if (len - num_written > SSIZE_MAX)
			chksz = SSIZE_MAX;
		else
			chksz = len - num_written;

		len_write = write(handle->fd, buf + num_written, chksz);
		if (len_write < 0) {
			err = errno;
			log_err("write() for '%s' failed with %u: %s\n", handle->path, err, strerror(err));
			ret = false;
			break;
		}

		num_written += len_write;
	}

	if (retlen)
		*retlen = num_written;

	return ret;
}

ufprog_status UFPROG_API os_open_file_mapping(const char *file, uint64_t size, size_t mapsize, ufprog_bool write,
					      ufprog_bool trunc, file_mapping *outmapping)
{
	file_mapping mapping;
	ufprog_status ret;
	size_t pathlen;
	struct stat st;
	int err;

	if (!file || !outmapping)
		return UFP_INVALID_PARAMETER;

	pathlen = strlen(file);
	mapping = calloc(1, sizeof(*mapping) + pathlen + 1);
	if (!mapping) {
		log_err("No memory for new file mapping\n");
		return UFP_NOMEM;
	}

	mapping->file.path = (char *)mapping + sizeof(*mapping);
	memcpy(mapping->file.path, file, pathlen + 1);

	ret = __os_open_file(file, true, write, trunc, write, &mapping->file);
	if (ret) {
		free(mapping);
		return ret;
	}

	if (write) {
		if (ftruncate(mapping->file.fd, size) < 0) {
			err = errno;
			log_err("ftruncate() for '%s' failed with %u: %s\n", mapping->file.path, err, strerror(err));
			close(mapping->file.fd);
			free(mapping);
			return false;
		}
	} else {
		if (fstat(mapping->file.fd, &st) < 0) {
			err = errno;
			log_err("fstat() for '%s' failed with %u: %s\n", mapping->file.path, err, strerror(err));
			close(mapping->file.fd);
			free(mapping);
			return false;
		}

		if (!size || size > st.st_size)
			size = st.st_size;

		if (!mapsize)
			mapsize = size;
	}

	if (mapsize > size)
		mapsize = size;

	mapping->page_size = sysconf(_SC_PAGE_SIZE);
	mapping->mapping_size = mapsize;
	mapping->max_size = size;
	mapping->writable = write;
	mapping->curr_mapping = MAP_FAILED;

	*outmapping = mapping;

	return UFP_OK;
}

ufprog_bool UFPROG_API os_close_file_mapping(file_mapping mapping)
{
	int err;

	if (!mapping)
		return false;

	if (mapping->curr_mapping != MAP_FAILED) {
		if (mapping->writable) {
			if (msync(mapping->curr_mapping, mapping->curr_mapping_size, MS_SYNC) < 0) {
				err = errno;
				log_err("msync() for '%s' failed with %u: %s\n", mapping->file.path, err, strerror(err));
			}
		}

		munmap(mapping->curr_mapping, mapping->curr_mapping_size);
	}

	close(mapping->file.fd);
	free(mapping);

	return true;
}

ufprog_bool UFPROG_API os_set_file_mapping_offset(file_mapping mapping, uint64_t offset, void **memory)
{
	int err, prot = PROT_READ;
	size_t mapping_size;

	if (!mapping)
		return false;

	if (offset % mapping->page_size)
		offset -= offset % mapping->page_size;

	if (offset >= mapping->max_size)
		return false;

	mapping_size = mapping->max_size - offset;
	if (mapping_size > mapping->mapping_size)
		mapping_size = mapping->mapping_size;

	if (mapping->curr_mapping != MAP_FAILED) {
		if (mapping->curr_file_offset == offset && mapping->curr_mapping_size >= mapping_size)
			goto out;

		if (mapping->writable) {
			if (msync(mapping->curr_mapping, mapping->curr_mapping_size, MS_SYNC) < 0) {
				err = errno;
				log_err("msync() for '%s' failed with %u: %s\n", mapping->file.path, err,
					strerror(err));
			}
		}

		munmap(mapping->curr_mapping, mapping->curr_mapping_size);
	}

	if (mapping->writable)
		prot |= PROT_WRITE;

	mapping->curr_mapping_size = 0;
	mapping->curr_file_offset = 0;

	mapping->curr_mapping = mmap(NULL, mapping_size, prot, MAP_SHARED, mapping->file.fd, offset);
	if (mapping->curr_mapping == MAP_FAILED) {
		err = errno;
		log_err("mmap() for '%s' failed with %u: %s\n", mapping->file.path, err, strerror(err));
		return false;
	}

	mapping->curr_mapping_size = mapping_size;
	mapping->curr_file_offset = offset;

out:
	if (memory)
		*memory = mapping->curr_mapping;

	return true;
}

size_t UFPROG_API os_get_file_mapping_granularity(file_mapping mapping)
{
	if (!mapping)
		return 0;

	return mapping->page_size;
}

size_t UFPROG_API os_get_file_max_mapping_size(file_mapping mapping)
{
	if (!mapping)
		return 0;

	return mapping->max_size;
}

void *UFPROG_API os_get_file_mapping_memory(file_mapping mapping)
{
	if (!mapping)
		return 0;

	return mapping->curr_mapping;
}

uint64_t UFPROG_API os_get_file_mapping_offset(file_mapping mapping)
{
	if (!mapping)
		return 0;

	return mapping->curr_file_offset;
}

size_t UFPROG_API os_get_file_mapping_size(file_mapping mapping)
{
	if (!mapping)
		return 0;

	return mapping->curr_mapping_size;
}
