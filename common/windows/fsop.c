/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Platform-specific filesystem operations
 */

#include <ctype.h>
#include <ufprog/osdef.h>
#include <ufprog/dirs.h>
#include <ufprog/log.h>
#include "win32.h"

struct os_file_handle {
	HANDLE hFile;
	char *path;
};

struct os_file_mapping {
	struct os_file_handle file;
	HANDLE hFileMapping;
	uint64_t nMaxSize;
	size_t nMappingGranularity;
	size_t nMappingSize;
	uint64_t nCurrentFileOffset;
	size_t nCurrentMappingSize;
	LPVOID pCurrentMapping;
	bool bWritable;
};

ufprog_bool UFPROG_API os_is_valid_filename(const char *filename)
{
	size_t i, len;
	int j;

	static const char invchars[] = { '<', '>', ':', '"', '/', '\\', '|', '?', '*' };

	if (!filename)
		return false;

	len = strlen(filename);
	if (!len)
		return false;

	for (i = 0; i < len; i++) {
		for (j = 0; j < ARRAYSIZE(invchars); j++) {
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
	LPWSTR pathbuf = utf8_to_wcs(path), p;
	DWORD dwErrorCode = 0;

	if (!pathbuf) {
		log_err("No memory for path buffer\n");
		return false;
	}

	p = pathbuf;
	while (*p) {
		if (*p == '/')
			*p = '\\';

		p++;
	}

	p = pathbuf;

	do {
		p = wcschr(p, '\\');
		if (p)
			*p = 0;

		if (*pathbuf && !(isalpha(pathbuf[0]) && pathbuf[1] == ':' && !pathbuf[2])) {
			if (!CreateDirectoryW(pathbuf, NULL)) {
				dwErrorCode = GetLastError();
				if (dwErrorCode != ERROR_ALREADY_EXISTS) {
					log_sys_error_utf8(dwErrorCode, "CreateDirectory failed with %u", dwErrorCode);
					break;
				}

				dwErrorCode = 0;
			}
		}

		if (p) {
			*p = '\\';
			p++;
		}
	} while (p);

	free(pathbuf);

	return dwErrorCode ? false : true;
}

static int __os_enum_file(const char *dir, const char *base, ufprog_bool recursive, void *priv, enum_file_cb cb)
{
	WIN32_FIND_DATAW wfd;
	char *dirpat, *name;
	LPWSTR lpwsDirPat;
	DWORD dwErrorCode;
	size_t dirlen;
	HANDLE hFind;
	int ret = 0;

	if (!dir || !cb)
		return false;

	if (!base)
		base = ".";

	/* Build enumration pattern */
	dirlen = strlen(dir);

	if (dir[dirlen - 1] == '\\')
		dirpat = path_concat(false, 0, dir, "*", NULL);
	else
		dirpat = path_concat(false, 0, dir, "", "*", NULL);

	if (!dirpat) {
		log_err("Unable to build enumration pattern\n");
		return -1;
	}

	/* Convert enumration pattern from UTF-8 to UCS-2 */
	lpwsDirPat = utf8_to_wcs(dirpat);
	free(dirpat);

	if (!lpwsDirPat) {
		log_err("Unable to convert enumration pattern to UTF-16\n");
		return -1;
	}

	hFind = FindFirstFileW(lpwsDirPat, &wfd);
	if (hFind == INVALID_HANDLE_VALUE) {
		dwErrorCode = GetLastError();

		if (dwErrorCode == ERROR_FILE_NOT_FOUND) {
			goto cleanup;
		}

		log_sys_error_utf8(dwErrorCode, "FindFirstFile failed with %u", dwErrorCode);
		ret = -1;
		goto cleanup;
	}

	do {
		if (!wcscmp(wfd.cFileName, L".") || !wcscmp(wfd.cFileName, L".."))
			continue;

		/* Convert file name to UTF-8 */
		name = wcs_to_utf8(wfd.cFileName);
		if (!name) {
			log_err("Unable to convert file name to UTF-16\n");
			goto cleanup;
		}

		if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			if (recursive) {
				char *subdir, *base_dir;

				if (dir[dirlen - 1] == '\\')
					subdir = path_concat(true, 0, dir, name, NULL);
				else
					subdir = path_concat(true, 0, dir, "", name, NULL);

				if (!subdir) {
					log_err("Unable to build subdirectory\n");
					ret = -1;
					goto out;
				}

				if (!strcmp(base, ""))
					base_dir = path_concat(false, 0, name, NULL);
				else
					base_dir = path_concat(false, 0, base, "", name, NULL);

				if (!base_dir) {
					log_err("Unable to build subdir enumration pattern\n");
					free(subdir);
					ret = -1;
					goto out;
				}

				ret = __os_enum_file(subdir, base_dir, recursive, priv, cb);
				free(base_dir);
				free(subdir);
			}
		} else {
			ret = cb(priv, base, name);
		}

	out:
		free(name);
	} while (!ret && FindNextFileW(hFind, &wfd));

cleanup:
	FindClose(hFind);
	free(lpwsDirPat);

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
	DWORD dwLastError, dwDesiredAccess, dwCreationDisposition;
	LPWSTR lpwsFileName;
	const char *opname;

	if (!read && !write) {
		log_err("Neither read nor write is specified for opening '%s'\n", file);
		return UFP_INVALID_PARAMETER;
	}

	/* Convert filepath from UTF-8 to UCS-2 */
	lpwsFileName = utf8_to_wcs(file);
	if (!lpwsFileName) {
		log_err("Unable to convert file name to UTF-16\n");
		return UFP_NOMEM;
	}

	if (read && !write) {
		opname = "read";
		rethandle->hFile = CreateFileW(lpwsFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0,
					       NULL);
	} else {
		if (!read && write) {
			opname = "write";
			dwDesiredAccess = GENERIC_WRITE;
		} else {
			opname = "read/write";
			dwDesiredAccess = GENERIC_READ | GENERIC_WRITE;
		}

		if (create) {
			if (trunc)
				dwCreationDisposition = CREATE_ALWAYS;
			else
				dwCreationDisposition = OPEN_ALWAYS;
		} else {
			if (trunc)
				dwCreationDisposition = TRUNCATE_EXISTING;
			else
				dwCreationDisposition = OPEN_EXISTING;
		}

		rethandle->hFile = CreateFileW(lpwsFileName, dwDesiredAccess, FILE_SHARE_READ, NULL,
					       dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);
	}

	free(lpwsFileName);

	if (rethandle->hFile == INVALID_HANDLE_VALUE) {
		dwLastError = GetLastError();
		if (dwLastError == ERROR_FILE_NOT_FOUND)
			return UFP_FILE_NOT_EXIST;
		else if (dwLastError == ERROR_INVALID_NAME)
			return UFP_FILE_NAME_INVALID;

		log_sys_error_utf8(dwLastError, "Failed to open file '%s' for %s", file, opname);
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

	handle->path = (char *)((uintptr_t)handle + sizeof(*handle));
	strlcpy(handle->path, file, pathlen + 1);

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

	CloseHandle(handle->hFile);

	free(handle);

	return true;
}

ufprog_bool UFPROG_API os_get_file_size(file_handle handle, uint64_t *retval)
{
	LARGE_INTEGER fsz;

	if (!handle || !retval)
		return false;

	if (!GetFileSizeEx(handle->hFile, &fsz)) {
		log_sys_error_utf8(GetLastError(), "Failed to get size of file '%s'", handle->path);
		return false;
	}

	*retval = fsz.QuadPart;

	return true;
}

ufprog_bool UFPROG_API os_set_file_pointer(file_handle handle, enum os_file_seek_method method, uint64_t distance,
					   uint64_t *retpointer)
{
	LARGE_INTEGER nfp, fp = { .QuadPart = distance };
	DWORD dwMoveMethod;

	if (!handle)
		return false;

	switch (method) {
	case FILE_SEEK_BEGIN:
		dwMoveMethod = FILE_BEGIN;
		break;

	case FILE_SEEK_CURR:
		dwMoveMethod = FILE_CURRENT;
		break;

	case FILE_SEEK_END:
		dwMoveMethod = FILE_END;
		break;

	default:
		log_err("Invalid file seek method %u\n", method);
		return false;
	}

	if (!SetFilePointerEx(handle->hFile, fp, &nfp, dwMoveMethod)) {
		log_sys_error_utf8(GetLastError(), "Failed to set file pointer for '%s'", handle->path);
		return false;
	}

	if (retpointer)
		*retpointer = nfp.QuadPart;

	return true;
}

ufprog_bool UFPROG_API os_set_end_of_file(file_handle handle)
{
	if (!handle)
		return false;

	return SetEndOfFile(handle->hFile);
}

ufprog_bool UFPROG_API os_read_file(file_handle handle, size_t len, void *buf, size_t *retlen)
{
	DWORD nBytesToRead, dwBytesRead;
	ufprog_bool ret = true;
	size_t nBytesRead = 0;
	uint8_t *p = buf;

	if (!handle)
		return false;

	if (!len) {
		if (retlen)
			*retlen = 0;
		return true;
	}

	if (!buf)
		return false;

	while (nBytesRead < len) {
		if (len - nBytesRead > MAXDWORD)
			nBytesToRead = MAXDWORD;
		else
			nBytesToRead = (DWORD)(len - nBytesRead);

		if (!ReadFile(handle->hFile, p + nBytesRead, nBytesToRead, &dwBytesRead, NULL)) {
			log_sys_error_utf8(GetLastError(), "Failed to read from file '%s'", handle->path);
			ret = false;
			break;
		}

		nBytesRead += dwBytesRead;
	}

	if (retlen)
		*retlen = nBytesRead;

	return ret;
}

ufprog_bool UFPROG_API os_write_file(file_handle handle, size_t len, const void *buf, size_t *retlen)
{
	DWORD nBytesToWrite, dwBytesWritten;
	size_t nBytesWritten = 0;
	const uint8_t *p = buf;
	ufprog_bool ret = true;

	if (!handle)
		return false;

	if (!len) {
		if (retlen)
			*retlen = 0;
		return true;
	}

	if (!buf)
		return false;

	while (nBytesWritten < len) {
		if (len - nBytesWritten > MAXDWORD)
			nBytesToWrite = MAXDWORD;
		else
			nBytesToWrite = (DWORD)(len - nBytesWritten);

		if (!WriteFile(handle->hFile, p + nBytesWritten, nBytesToWrite, &dwBytesWritten, NULL)) {
			log_sys_error_utf8(GetLastError(), "Failed to write to file '%s'", handle->path);
			ret = false;
			break;
		}

		nBytesWritten += dwBytesWritten;
	}

	if (retlen)
		*retlen = nBytesWritten;

	return ret;
}

ufprog_status UFPROG_API os_open_file_mapping(const char *file, uint64_t size, size_t mapsize, ufprog_bool write,
					      ufprog_bool trunc, file_mapping *outmapping)
{
	LARGE_INTEGER fsz = { .QuadPart = size };
	file_mapping mapping;
	ufprog_status ret;
	SYSTEM_INFO si;
	size_t pathlen;

	if (!file || !outmapping)
		return UFP_INVALID_PARAMETER;

	pathlen = strlen(file);
	mapping = calloc(1, sizeof(*mapping) + pathlen + 1);
	if (!mapping) {
		log_err("No memory for new file mapping\n");
		return UFP_NOMEM;
	}

	mapping->file.path = (char *)((uintptr_t)mapping + sizeof(*mapping));
	strlcpy(mapping->file.path, file, pathlen + 1);

	ret = __os_open_file(file, true, write, trunc, write, &mapping->file);
	if (ret) {
		free(mapping);
		return ret;
	}

	if (write) {
		if (!SetFilePointerEx(mapping->file.hFile, fsz, NULL, FILE_BEGIN)) {
			log_sys_error_utf8(GetLastError(), "Failed to set file size to %llu for '%s'", size, file);
			CloseHandle(mapping->file.hFile);
			free(mapping);
			return UFP_FAIL;
		}

		if (!SetEndOfFile(mapping->file.hFile)) {
			log_sys_error_utf8(GetLastError(), "Failed to set end of file for '%s'", file);
			CloseHandle(mapping->file.hFile);
			free(mapping);
			return UFP_FAIL;
		}
	} else {
		if (!GetFileSizeEx(mapping->file.hFile, &fsz)) {
			log_sys_error_utf8(GetLastError(), "Failed to get size of file '%s'", file);
			CloseHandle(mapping->file.hFile);
			free(mapping);
			return UFP_FAIL;
		}

		if (!size || size > (uint64_t)fsz.QuadPart)
			size = fsz.QuadPart;

		if (!mapsize)
			mapsize = size;
	}

	if (mapsize > size)
		mapsize = size;

	GetSystemInfo(&si);

	mapping->nMappingGranularity = si.dwAllocationGranularity;
	mapping->nMappingSize = mapsize;
	mapping->nMaxSize = size;
	mapping->bWritable = write;

	mapping->hFileMapping = CreateFileMappingW(mapping->file.hFile, NULL, write ? PAGE_READWRITE : PAGE_READONLY,
						   (DWORD)(size >> 32), (DWORD)(size & 0xffffffff), NULL);
	if (!mapping->hFileMapping) {
		log_sys_error_utf8(GetLastError(), "Failed to create mapping of file '%s'", file);
		CloseHandle(mapping->file.hFile);
		free(mapping);
		return UFP_FAIL;
	}

	*outmapping = mapping;

	return UFP_OK;
}

ufprog_bool UFPROG_API os_close_file_mapping(file_mapping mapping)
{
	if (!mapping)
		return false;

	if (mapping->pCurrentMapping) {
		if (mapping->bWritable) {
			if (!FlushViewOfFile(mapping->pCurrentMapping, mapping->nCurrentMappingSize)) {
				log_sys_error_utf8(GetLastError(), "Failed to commit mapping of file '%s'",
						   mapping->file.path);
			}
		}

		UnmapViewOfFile(mapping->pCurrentMapping);
	}

	CloseHandle(mapping->hFileMapping);
	CloseHandle(mapping->file.hFile);

	return true;
}

ufprog_bool UFPROG_API os_set_file_mapping_offset(file_mapping mapping, uint64_t offset, void **memory)
{
	size_t nMappingSize;

	if (!mapping)
		return false;

	if (offset % mapping->nMappingGranularity)
		offset -= offset % mapping->nMappingGranularity;

	if (offset >= mapping->nMaxSize)
		return false;

	nMappingSize = mapping->nMaxSize - offset;
	if (nMappingSize > mapping->nMappingSize)
		nMappingSize = mapping->nMappingSize;

	if (mapping->pCurrentMapping) {
		if (mapping->nCurrentFileOffset == offset && mapping->nCurrentMappingSize >= nMappingSize)
			goto out;

		if (mapping->bWritable) {
			if (!FlushViewOfFile(mapping->pCurrentMapping, mapping->nCurrentMappingSize)) {
				log_sys_error_utf8(GetLastError(), "Failed to commit mapping of file '%s'",
						   mapping->file.path);
			}
		}

		UnmapViewOfFile(mapping->pCurrentMapping);
	}

	mapping->nCurrentMappingSize = 0;
	mapping->nCurrentFileOffset = 0;

	mapping->pCurrentMapping = MapViewOfFile(mapping->hFileMapping,
						 mapping->bWritable ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ,
						 (DWORD)(offset >> 32), (DWORD)(offset & 0xffffffff), nMappingSize);
	if (!mapping->pCurrentMapping) {
		log_sys_error_utf8(GetLastError(), "Failed to map file '%s', offset 0x%llx, size 0x%lx\n",
				   mapping->file.path, offset, nMappingSize);
		return false;
	}

	mapping->nCurrentMappingSize = nMappingSize;
	mapping->nCurrentFileOffset = offset;

out:
	if (memory)
		*memory = mapping->pCurrentMapping;

	return true;
}

size_t UFPROG_API os_get_file_mapping_granularity(file_mapping mapping)
{
	if (!mapping)
		return 0;

	return mapping->nMappingGranularity;
}

size_t UFPROG_API os_get_file_max_mapping_size(file_mapping mapping)
{
	if (!mapping)
		return 0;

	return mapping->nMaxSize;
}

void *UFPROG_API os_get_file_mapping_memory(file_mapping mapping)
{
	if (!mapping)
		return 0;

	return mapping->pCurrentMapping;
}

uint64_t UFPROG_API os_get_file_mapping_offset(file_mapping mapping)
{
	if (!mapping)
		return 0;

	return mapping->nCurrentFileOffset;
}

size_t UFPROG_API os_get_file_mapping_size(file_mapping mapping)
{
	if (!mapping)
		return 0;

	return mapping->nCurrentMappingSize;
}

file_handle UFPROG_API os_get_file_mapping_file_handle(file_mapping mapping)
{
	if (!mapping)
		return NULL;

	return &mapping->file;
}
