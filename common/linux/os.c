/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Linux-specific initialization
 */

#define _GNU_SOURCE
#include <errno.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <dlfcn.h>
#include <time.h>
#include <pwd.h>
#include <pthread.h>
#include <sys/types.h>
#include <ufprog/log.h>
#include <ufprog/dirs.h>
#include <ufprog/misc.h>

#define PATH_INCREMENT			1024
#define UFPROG_APPDATA_DOT_NAME		"." UFPROG_APPDATA_NAME

static struct sigaction oldsa;
static ctrlc_handler ctrlc_cb;
static char *progname;

static ufprog_bool os_register_prog_root_dir(void)
{
	char *prog_path = NULL, *prog_path_new, *ptr;
	size_t buff_size = 0;
	ufprog_status ret;
	ssize_t retsize;
	int err;

	/* Get program's full path first */
	while (true) {
		buff_size += PATH_INCREMENT;

		if (!prog_path)
			prog_path_new = malloc(buff_size);
		else
			prog_path_new = realloc(prog_path, buff_size);

		if (!prog_path_new) {
			log_err("No memory for program's root directory\n");
			if (prog_path)
				free(prog_path);
			return false;
		}

		prog_path = prog_path_new;
		retsize = readlink("/proc/self/exe", prog_path, buff_size);
		if (retsize < 0) {
			err = errno;
			log_err("readlink() failed with %u: %s\n", err, strerror(err));
			free(prog_path);
			return false;
		}

		if (retsize < (ssize_t)buff_size) {
			prog_path[retsize] = 0;
			break;
		}
	}

	/* Split program's root directory */
	ptr = prog_path + strlen(prog_path) - 1;

	while (ptr >= prog_path) {
		if (*ptr == '/') {
			ptr++;
			break;
		}

		ptr--;
	}

	if (ptr <= prog_path) {
		log_err("Failed to parse program's root directory\n");
		free(prog_path);
		return false;
	}

	/* Store program's name */
	progname = strdup(ptr);
	if (!progname) {
		log_err("No memory to store program's name\n");
		free(prog_path);
		return false;
	}

	/* Register program's root directory */
	*ptr = 0;

	ret = set_root_dir(prog_path);
	free(prog_path);

	if (ret)
		return false;

	return true;
}

static ufprog_bool os_register_app_dirs(void)
{
	const char *homedir, *appname = UFPROG_APPDATA_NAME;
	char *config_home, *new_path;
	ufprog_bool bret = false;
	ufprog_status ret;
	size_t dirlen;

	homedir = getenv("XDG_CONFIG_HOME");
	if (!homedir || !*homedir) {
		appname = UFPROG_APPDATA_DOT_NAME;

		homedir = getenv("HOME");
		if (!homedir || !*homedir) {
			homedir = getpwuid(getuid())->pw_dir;
			if (!homedir || !*homedir) {
				log_err("Failed to get user's config data home directory\n");
				return false;
			}
		}
	}

	dirlen = strlen(homedir);
	if (homedir[dirlen - 1] == '/')
		config_home = path_concat(true, 0, homedir, appname, NULL);
	else
		config_home = path_concat(true, 0, homedir, "", appname, NULL);

	if (!config_home) {
		log_err("Failed to generate program's config base directory\n");
		return false;
	}

	/* Register program's config directory */
	bret = os_mkdir_p(config_home);
	if (!bret) {
		log_err("Failed to create program's config base directory\n");
		goto cleanup;
	}

	ret = add_dir(DIR_CONFIG, config_home);
	if (ret)
		goto cleanup;

	/* Register program's device directory */
	new_path = path_concat(true, 0, config_home, UFPROG_DEVICE_DIR_NAME, NULL);
	if (!new_path) {
		log_err("Failed to generate program's device directory\n");
		goto cleanup;
	}

	ret = add_dir(DIR_DEVICE, new_path);
	free(new_path);

	if (ret)
		goto cleanup;

	/* Register program's plugin directory */
	new_path = path_concat(true, 0, config_home, UFPROG_PLUGIN_DIR_NAME, NULL);
	if (!new_path) {
		log_err("Failed to generate program's plugin directory\n");
		goto cleanup;
	}

	ret = add_dir(DIR_PLUGIN, new_path);
	free(new_path);

	if (!ret)
		bret = true;

cleanup:
	free(config_home);
	return bret;
}

static ufprog_bool os_register_default_dirs(void)
{
	const char *prefix = INSTALL_PREFIX;
	char *data_root, *new_path;
	ufprog_bool bret = false;
	ufprog_status ret;
	size_t prefix_len;

	prefix_len = strlen(prefix);
	if (prefix[prefix_len - 1] == '/')
		data_root = path_concat(true, 0, prefix, "lib", UFPROG_APPDATA_NAME, NULL);
	else
		data_root = path_concat(true, 0, prefix, "", "lib", UFPROG_APPDATA_NAME, NULL);

	if (!data_root) {
		log_err("Failed to generate program's data base directory\n");
		return false;
	}

	/* Register program's data directory */
	ret = add_dir(DIR_DATA_ROOT, data_root);
	if (ret)
		goto cleanup;

	/* Register program's device directory */
	new_path = path_concat(true, 0, data_root, UFPROG_DEVICE_DIR_NAME, NULL);
	if (!new_path) {
		log_err("Failed to generate program's device directory\n");
		goto cleanup;
	}

	ret = add_dir(DIR_DEVICE, new_path);
	free(new_path);

	if (ret)
		goto cleanup;

	/* Register program's interface directory */
	new_path = path_concat(true, 0, data_root, UFPROG_PLUGIN_DIR_NAME, NULL);
	if (!new_path) {
		log_err("Failed to generate program's plugin directory\n");
		goto cleanup;
	}

	ret = add_dir(DIR_PLUGIN, new_path);
	free(new_path);

	if (!ret)
		bret = true;

cleanup:
	free(data_root);
	return bret;
}

static ufprog_bool os_register_default_portable_dirs(void)
{
	ufprog_status ret;
	char *new_path;

	/* Register program's data directory */
	ret = add_dir(DIR_DATA_ROOT, get_root_dir());
	if (ret)
		return false;

	/* Register program's config directory */
	ret = add_dir(DIR_CONFIG, get_root_dir());
	if (ret)
		return false;

	/* Register program's devices directory */
	new_path = path_concat(true, 0, get_root_dir(), UFPROG_DEVICE_DIR_NAME, NULL);
	if (!new_path) {
		log_err("Failed to generate program's device directory\n");
		return false;
	}

	ret = add_dir(DIR_DEVICE, new_path);
	free(new_path);

	if (ret)
		return false;

	/* Register program's plugin directory */
	new_path = path_concat(true, 0, get_root_dir(), UFPROG_PLUGIN_DIR_NAME, NULL);
	if (!new_path) {
		log_err("Failed to generate program's plugin directory\n");
		return false;
	}

	ret = add_dir(DIR_PLUGIN, new_path);
	free(new_path);

	if (ret)
		return false;

	return true;
}

ufprog_bool UFPROG_API os_init(void)
{
	os_register_prog_root_dir();

	if (uses_portable_dirs()) {
		os_register_default_portable_dirs();
	} else {
		os_register_app_dirs();
		os_register_default_dirs();
	}

	return true;
}

const char *UFPROG_API os_prog_name(void)
{
	return progname;
}

static void UFPROG_API linux_console_print(void *priv, log_level level, const char *text)
{
	int fd = level > LOG_WARN ? STDERR_FILENO : STDOUT_FILENO;
	ssize_t retlen;
	size_t len;

	len = strlen(text);

	while (len) {
		retlen = write(fd, text, len);
		if (retlen < 0)
			return;

		fsync(fd);

		len -= retlen;
		text += retlen;
	}
}

static void UFPROG_API linux_console_log_print(void *priv, const struct log_data *data)
{
	default_console_log(data, NULL, linux_console_print);
}

void UFPROG_API set_os_default_log_print(void)
{
	set_log_print_cb(NULL, linux_console_log_print);
}

char *UFPROG_API os_getline_alloc(FILE *f)
{
	char *line = NULL;
	size_t len = 0;

	if (getline(&line, &len, f) < 0)
		return NULL;

	return line;
}

ufprog_status UFPROG_API os_load_module(const char *module_path, module_handle *handle)
{
	void *module;

	if (!module_path || !handle)
		return UFP_INVALID_PARAMETER;

	if (access(module_path, F_OK) < 0)
		return UFP_FILE_NOT_EXIST;

	dlerror();

	module = dlopen(module_path, RTLD_NOW);
	if (!module) {
		log_err("Failed to load module '%s', error is %s\n", module_path, dlerror());
		return UFP_FAIL;
	}

	*handle = module;

	return UFP_OK;
}

void UFPROG_API os_unload_module(module_handle module)
{
	if (!module)
		return;

	dlclose(module);
}

void *UFPROG_API os_find_module_symbol(module_handle module, const char *name)
{
	if (!module || !name)
		return NULL;

	return dlsym(module, name);
}

ufprog_status UFPROG_API os_find_module_symbols(module_handle module, struct symbol_find_entry *list, size_t count,
						ufprog_bool full)
{
	ufprog_bool missing = false;
	void *proc;
	size_t i;

	if (!module || !list)
		return UFP_INVALID_PARAMETER;

	if (!count)
		return UFP_OK;

	for (i = 0; i < count; i++) {
		proc = dlsym(module, list[i].name);
		if (proc)
			list[i].found = true;
		else
			missing = true;

		if (list[i].psymbol)
			*list[i].psymbol = proc;
	}

	if (full && missing)
		return UFP_FAIL;

	return UFP_OK;
}

static void sigint_handler(int sig)
{
	ctrlc_cb();
}

ufprog_bool UFPROG_API os_register_ctrlc_handler(ctrlc_handler handler)
{
	struct sigaction sa;
	int err = 0;

	ctrlc_cb = handler;

	if (handler) {
		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = sigint_handler;

		err = sigaction(SIGINT, &sa, &oldsa);
	} else {
		if (oldsa.sa_handler)
			err = sigaction(SIGINT, &oldsa, NULL);
	}

	if (err < 0) {
		err = errno;
		log_err("sigaction() failed with %u: %s\n", err, strerror(err));
		return false;
	}

	return true;
}

ufprog_bool UFPROG_API os_create_mutex(mutex_handle *outmutex)
{
	pthread_mutex_t *mutex = NULL;
	pthread_mutexattr_t attr;
	int err;

	if (!outmutex)
		return false;

	err = pthread_mutexattr_init(&attr);
	if (err) {
		log_err("pthread_mutexattr_init() failed with %u: %s\n", err, strerror(err));
		return false;
	}

	err = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	if (err) {
		log_err("pthread_mutexattr_settype() failed with %u: %s\n", err, strerror(err));
		goto cleanup;
	}

	mutex = malloc(sizeof(*mutex));
	if (!mutex) {
		log_err("No memory for mutex object\n");
		goto cleanup;
	}

	err = pthread_mutex_init(mutex, &attr);
	if (err) {
		log_err("pthread_mutex_init() failed with %u: %s\n", err, strerror(err));
		goto cleanup;
	}

	pthread_mutexattr_destroy(&attr);

	*outmutex = (mutex_handle)mutex;
	return true;

cleanup:
	pthread_mutexattr_destroy(&attr);

	if (mutex)
		free(mutex);

	return false;
}

ufprog_bool UFPROG_API os_free_mutex(mutex_handle mutex)
{
	int err;

	if (!mutex)
		return false;

	err = pthread_mutex_destroy((pthread_mutex_t *)mutex);
	if (err)
		log_err("pthread_mutex_destroy() failed with %u: %s\n", err, strerror(err));

	free(mutex);

	return err ? false : true;
}

ufprog_bool UFPROG_API os_mutex_lock(mutex_handle mutex)
{
	int err;

	if (!mutex)
		return false;

	err = pthread_mutex_lock((pthread_mutex_t *)mutex);
	if (!err)
		return true;

	log_err("pthread_mutex_lock() failed with %u: %s\n", err, strerror(err));

	return false;
}

ufprog_bool UFPROG_API os_mutex_unlock(mutex_handle mutex)
{
	int err;

	if (!mutex)
		return false;

	err = pthread_mutex_unlock((pthread_mutex_t *)mutex);
	if (!err)
		return true;

	log_err("pthread_mutex_unlock() failed with %u: %s\n", err, strerror(err));

	return false;
}

static inline uint64_t get_timer_us(void)
{
	struct timespec t;

	if (clock_gettime(CLOCK_MONOTONIC, &t) < 0)
		return 0;

	return t.tv_sec * 1000000 + t.tv_nsec / 1000;
}

uint64_t UFPROG_API os_get_timer_us(void)
{
	return get_timer_us();
}

void UFPROG_API os_udelay(uint64_t us)
{
	uint64_t te;

	te = get_timer_us() + us;

	while (get_timer_us() <= te)
		;
}

ufprog_status UFPROG_API os_read_text_file(const char *filename, char **outdata, size_t *retlen)
{
	return read_file_contents(filename, (void **)outdata, retlen);
}
