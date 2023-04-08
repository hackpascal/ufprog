/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Generic plugin framework
 */
#pragma once

#ifndef _UFPROG_PLUGIN_COMMON_H_
#define _UFPROG_PLUGIN_COMMON_H_

#include <stdbool.h>
#include <ufprog/api_plugin.h>
#include <ufprog/lookup_table.h>
#include <ufprog/config.h>
#include <ufprog/osdef.h>

struct plugin_mgmt;

struct plugin {
	module_handle module;
	char *name;

	uint32_t version;
	uint32_t api_version;
	const char *desc;

	api_plugin_init init;
	api_plugin_cleanup cleanup;
};

typedef ufprog_status (*plugin_api_init_fn)(struct plugin *plugin, const char *module_path);
typedef ufprog_status (*plugin_post_init_fn)(struct plugin *plugin);

ufprog_status plugin_mgmt_create(const char *name, const char *dirname, size_t total_struct_size,
				 uint32_t required_api_version_major, plugin_api_init_fn api_init_fn,
				 plugin_post_init_fn post_init_fn, struct plugin_mgmt **out_pluginmgmt);
ufprog_status plugin_mgmt_destroy(struct plugin_mgmt *pluginmgmt);
const char *plugin_mgmt_name(struct plugin_mgmt *pluginmgmt);
const char *plugin_dir_name(struct plugin_mgmt *pluginmgmt);
size_t plugin_struct_size(struct plugin_mgmt *pluginmgmt);
uint32_t plugin_mgmt_loaded_count(struct plugin_mgmt *pluginmgmt);

ufprog_status plugin_config_load(struct plugin_mgmt *pluginmgmt, const char *name, struct json_object **outconfig);
ufprog_status plugin_load(struct plugin_mgmt *pluginmgmt, const char *name, struct plugin **outplugin);
ufprog_status plugin_unload(struct plugin_mgmt *pluginmgmt, struct plugin *plugin);

void *plugin_find_symbol(struct plugin *plugin, const char *name);
bool plugin_find_module_symbols(struct plugin *plugin, struct symbol_find_entry *list, size_t count, bool full);

#endif /* _UFPROG_PLUGIN_COMMON_H_ */
