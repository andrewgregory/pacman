#ifndef ALPM_RESOLVER_H
#define ALPM_RESOLVER_H

enum _alpm_resolver_flag {
	ALPM_RESOLVER_DEFAULT = 0,
	ALPM_RESOLVER_IGNORE_DEPENDENCY_VERSION = (1<<0),
};

alpm_list_t *_alpm_resolvedeps_thorough(
		alpm_handle_t *handle, alpm_list_t *add, alpm_list_t *remove, int flags);

#endif
