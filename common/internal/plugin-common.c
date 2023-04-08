// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Generic plugin management
 */

#include <malloc.h>
#include <string.h>
#include <ufprog/dirs.h>
#include <ufprog/log.h>
#include "plugin-common.h"

struct plugin_mgmt {
	char *name;
	char *dir_name;
	struct ufprog_lookup_table *plugin_list;
	plugin_api_init_fn api_init;
	plugin_post_init_fn post_init;

	size_t extra_struct_size;
	uint32_t required_api_version_major;
};

struct plugin_load_priv {
	struct plugin_mgmt *pluginmgmt;
	struct plugin *plugin;
};

struct plugin_config_load_priv {
	struct plugin_mgmt *pluginmgmt;
	struct json_object *config;
	const char *name;
};

ufprog_status plugin_mgmt_create(const char *name, const char *dirname, size_t total_struct_size,
				 uint32_t required_api_version_major, plugin_api_init_fn api_init_fn,
				 plugin_post_init_fn post_init_fn, struct plugin_mgmt **out_pluginmgmt)
{
	struct plugin_mgmt *pluginmgmt;
	size_t namelen, dir_name_len;

	if (!name || !dirname)
		return UFP_INVALID_PARAMETER;

	if (total_struct_size < sizeof(struct plugin))
		return UFP_INVALID_PARAMETER;

	namelen = strlen(name);
	dir_name_len = strlen(dirname);

	pluginmgmt = calloc(1, sizeof(*pluginmgmt) + namelen + 1 + dir_name_len + 1);
	if (!pluginmgmt)
		return UFP_NOMEM;

	pluginmgmt->name = (char *)pluginmgmt + sizeof(*pluginmgmt);
	memcpy(pluginmgmt->name, name, namelen + 1);

	pluginmgmt->dir_name = pluginmgmt->name + namelen + 1;
	memcpy(pluginmgmt->dir_name, dirname, dir_name_len + 1);

	if (lookup_table_create(&pluginmgmt->plugin_list, 0)) {
		free(pluginmgmt);
		return UFP_NOMEM;
	}

	pluginmgmt->api_init = api_init_fn;
	pluginmgmt->post_init = post_init_fn;
	pluginmgmt->extra_struct_size = total_struct_size - sizeof(struct plugin);
	pluginmgmt->required_api_version_major = required_api_version_major;

	*out_pluginmgmt = pluginmgmt;

	return UFP_OK;
}

ufprog_status plugin_mgmt_destroy(struct plugin_mgmt *pluginmgmt)
{
	if (!pluginmgmt)
		return UFP_INVALID_PARAMETER;

	if (lookup_table_length(pluginmgmt->plugin_list))
		return UFP_FAIL;

	lookup_table_destroy(pluginmgmt->plugin_list);
	free(pluginmgmt);

	return UFP_OK;
}

const char *plugin_mgmt_name(struct plugin_mgmt *pluginmgmt)
{
	if (!pluginmgmt)
		return NULL;

	return pluginmgmt->name;
}

const char *plugin_dir_name(struct plugin_mgmt *pluginmgmt)
{
	if (!pluginmgmt)
		return NULL;

	return pluginmgmt->dir_name;
}

size_t plugin_struct_size(struct plugin_mgmt *pluginmgmt)
{
	if (!pluginmgmt)
		return 0;

	return sizeof(struct plugin) + pluginmgmt->extra_struct_size;
}

uint32_t plugin_mgmt_loaded_count(struct plugin_mgmt *pluginmgmt)
{
	if (!pluginmgmt)
		return 0;

	return lookup_table_length(pluginmgmt->plugin_list);
}

static ufprog_status plugin_check(struct plugin_mgmt *pluginmgmt, struct plugin *plugin, const char *module_path)
{
	api_plugin_api_version fn_plugin_api_version;
	api_plugin_version fn_plugin_version;
	api_plugin_desc fn_plugin_desc;
	ufprog_status ret;

	struct symbol_find_entry basic_symbols[] = {
		FIND_MODULE(API_NAME_PLUGIN_API_VERSION, fn_plugin_api_version),
		FIND_MODULE(API_NAME_PLUGIN_DESC, fn_plugin_desc),
	};

	struct symbol_find_entry optional_symbols[] = {
		FIND_MODULE(API_NAME_PLUGIN_VERSION, fn_plugin_version),
		FIND_MODULE(API_NAME_PLUGIN_INIT, plugin->init),
		FIND_MODULE(API_NAME_PLUGIN_CLEANUP, plugin->cleanup),
	};

	ret = os_find_module_symbols(plugin->module, basic_symbols, ARRAY_SIZE(basic_symbols), true);
	if (ret) {
		log_err("'%s' is missing basic symbols\n", module_path);
		return ret;
	}

	plugin->api_version = fn_plugin_api_version();
	if (GET_MAJOR_VERSION(plugin->api_version) != pluginmgmt->required_api_version_major) {
		log_err("The API major version of %s plugin '%s' mismatches: expect %u, got %u\n",
			pluginmgmt->name, module_path, pluginmgmt->required_api_version_major,
			GET_MAJOR_VERSION(plugin->api_version));
		return UFP_FAIL;
	}

	plugin->desc = fn_plugin_desc();

	os_find_module_symbols(plugin->module, optional_symbols, ARRAY_SIZE(optional_symbols), false);

	if (fn_plugin_version)
		plugin->version = fn_plugin_version();
	else
		plugin->version = 0;

	if (pluginmgmt->api_init) {
		ret = pluginmgmt->api_init(plugin, module_path);
		if (ret)
			return ret;
	}

	return UFP_OK;
}

static int UFPROG_API dir_enum_configs(void *priv, uint32_t index, const char *dir)
{
	struct plugin_config_load_priv *plugin_cfg_priv = priv;
	char *config_path;
	ufprog_status ret;
	size_t len;

	config_path = path_concat(false, strlen(UFPROG_CONFIG_SUFFIX), dir, plugin_cfg_priv->pluginmgmt->dir_name,
				  plugin_cfg_priv->name, NULL);
	if (!config_path)
		return 0;

	len = strlen(config_path);
	memcpy(config_path + len, UFPROG_CONFIG_SUFFIX, strlen(UFPROG_CONFIG_SUFFIX) + 1);

	log_dbg("Try loading %s plugin config '%s'\n", plugin_cfg_priv->pluginmgmt->name, config_path);

	ret = json_from_file(config_path, &plugin_cfg_priv->config);
	if (ret) {
		if (ret == UFP_FILE_NOT_EXIST)
			log_dbg("'%s' does not exist\n", config_path);

		plugin_cfg_priv->config = NULL;
		free(config_path);
		return 0;
	}

	log_notice("'%s' loaded\n", config_path);

	free(config_path);
	return 1;
}

ufprog_status plugin_config_load(struct plugin_mgmt *pluginmgmt, const char *name, struct json_object **outconfig)
{
	struct plugin_config_load_priv plugin_cfg_priv;

	if (!pluginmgmt || !name || !outconfig)
		return UFP_INVALID_PARAMETER;

	plugin_cfg_priv.pluginmgmt = pluginmgmt;
	plugin_cfg_priv.name = name;
	plugin_cfg_priv.config = NULL;

	dir_enum(DIR_PLUGIN, dir_enum_configs, &plugin_cfg_priv);

	*outconfig = plugin_cfg_priv.config;

	if (!plugin_cfg_priv.config) {
		log_errdbg("No %s plugin config named '%s' could be loaded\n", pluginmgmt->name, name);
		return UFP_NOT_EXIST;
	}

	return UFP_OK;
}

static int UFPROG_API dir_enum_plugins(void *priv, uint32_t index, const char *dir)
{
	struct plugin_load_priv *plugin_priv = priv;
	char *module_path;
	ufprog_status ret;
	int eret = 0;
	size_t len;

	module_path = path_concat(false, strlen(MODULE_SUFFIX), dir, plugin_priv->pluginmgmt->dir_name,
				  plugin_priv->plugin->name, NULL);
	if (!module_path)
		return 0;

	len = strlen(module_path);
	memcpy(module_path + len, MODULE_SUFFIX, strlen(MODULE_SUFFIX) + 1);

	log_dbg("Trying loading %s plugin '%s'\n", plugin_priv->pluginmgmt->name, module_path);

	ret = os_load_module(module_path, &plugin_priv->plugin->module);
	if (ret) {
		if (ret == UFP_FILE_NOT_EXIST)
			log_dbg("'%s' does not exist\n", module_path);

		free(module_path);
		return 0;
	}

	ret = plugin_check(plugin_priv->pluginmgmt, plugin_priv->plugin, module_path);
	if (!ret) {
		log_notice("'%s' loaded as %s plugin\n", module_path, plugin_priv->pluginmgmt->name);
		eret = 1;
	} else {
		log_err("'%s' is not a valid %s plugin\n", module_path, plugin_priv->pluginmgmt->name);
		os_unload_module(plugin_priv->plugin->module);
		plugin_priv->plugin->module = NULL;
	}

	free(module_path);
	return eret;
}

ufprog_status plugin_load(struct plugin_mgmt *pluginmgmt, const char *name, struct plugin **outplugin)
{
	struct plugin_load_priv plugin_priv;
	struct plugin *plugin;
	ufprog_status ret;
	size_t namelen;

	if (!pluginmgmt || !name || !outplugin)
		return UFP_INVALID_PARAMETER;

	if (lookup_table_find(pluginmgmt->plugin_list, name, (void **)&plugin)) {
		*outplugin = plugin;
		return UFP_OK;
	}

	*outplugin = NULL;

	namelen = strlen(name);

	plugin = calloc(1, sizeof(*plugin) + pluginmgmt->extra_struct_size + namelen + 1);
	if (!plugin) {
		log_err("No memory for loading %s plugin\n", pluginmgmt->name);
		return UFP_NOMEM;
	}

	plugin->name = (char *)plugin + sizeof(*plugin) + pluginmgmt->extra_struct_size;
	memcpy(plugin->name, name, namelen + 1);

	plugin_priv.pluginmgmt = pluginmgmt;
	plugin_priv.plugin = plugin;

	dir_enum(DIR_PLUGIN, dir_enum_plugins, &plugin_priv);

	if (!plugin->module) {
		log_err("No %s plugin module named '%s' could be loaded\n", pluginmgmt->name, name);
		ret = UFP_NOT_EXIST;
		goto cleanup;
	}

	if (plugin->init) {
		ret = plugin->init();
		if (ret) {
			log_err("Initialization of %s plugin '%s' failed\n", pluginmgmt->name, name);
			ret = UFP_MODULE_INIT_FAIL;
			goto cleanup_unload;
		}
	}

	if (pluginmgmt->post_init) {
		ret = pluginmgmt->post_init(plugin);
		if (ret) {
			log_err("Post-initialization of %s plugin '%s' failed\n", pluginmgmt->name, name);
			ret = UFP_MODULE_INIT_FAIL;
			goto cleanup_module;
		}
	}

	ret = lookup_table_insert(pluginmgmt->plugin_list, name, plugin);
	if (ret) {
		log_err("No memory for maintaining %s plugin '%s'\n", pluginmgmt->name, name);
		goto cleanup_module;
	}

	if (plugin->version) {
		log_info("Loaded %s plugin %s %u.%u\n", pluginmgmt->name, plugin->desc,
			 GET_MAJOR_VERSION(plugin->version), GET_MINOR_VERSION(plugin->version));
	} else {
		log_info("Loaded %s plugin %s\n", pluginmgmt->name, plugin->desc);
	}

	*outplugin = plugin;
	return UFP_OK;

cleanup_module:
	if (plugin->cleanup)
		plugin->cleanup();

cleanup_unload:
	os_unload_module(plugin->module);

cleanup:
	free(plugin);

	return ret;
}

ufprog_status plugin_unload(struct plugin_mgmt *pluginmgmt, struct plugin *plugin)
{
	ufprog_status ret;

	if (!pluginmgmt || !plugin)
		return UFP_INVALID_PARAMETER;

	lookup_table_delete(pluginmgmt->plugin_list, plugin->name);

	if (plugin->cleanup) {
		ret = plugin->cleanup();
		if (ret)
			log_warn("Cleanup of %s plugin '%s' cleanup failed\n", pluginmgmt->name, plugin->name);
	}

	os_unload_module(plugin->module);

	return UFP_OK;
}

void *plugin_find_symbol(struct plugin *plugin, const char *name)
{
	if (!plugin || !name)
		return NULL;

	return os_find_module_symbol(plugin->module, name);
}

bool plugin_find_module_symbols(struct plugin *plugin, struct symbol_find_entry *list, size_t count, bool full)
{
	if (!plugin)
		return false;

	if (!count)
		return true;

	if (!list)
		return false;

	return os_find_module_symbols(plugin->module, list, count, full) == UFP_OK;
}
