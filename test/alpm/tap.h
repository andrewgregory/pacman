#include "tap.c"

#include <stdlib.h>

#define IS_LIST_STR(...) is_list_str(__FILE__, __LINE__, __VA_ARGS__)

int is_list_str(const char *file, int lineno,
		alpm_list_t *got, alpm_list_t *expected, const char *name, ...)
{
	alpm_list_t *l1 = got, *l2 = expected;
	int success = 1;
	size_t idx = 0;
	va_list args;
	while(l1 && l2) {
		const char *c1 = l1->data, *c2 = l2->data;
		if(!(c1 == c2 || (c1 && c2 && strcmp(c1, c2) == 0))) {
			break;
		}
		l1 = l1->next;
		l2 = l2->next;
		idx++;
	}
	if(l1 || l2) {
		success = 0;
	}
	if(!_tap_vok(file, lineno, success, name, args)) {
		tap_diag("    Lists began differing at:");
		if(l1) {
			tap_diag("          got[%zu] = '%s'", idx, l1->data);
		} else {
			tap_diag("          got[%zu] = Does not exist", idx);
		}
		if(l2) {
			tap_diag("     expected[%zu] = '%s'", idx, l2->data);
		} else {
			tap_diag("     expected[%zu] = Does not exist", idx);
		}
	}
	va_end(args);
	return success;
}

alpm_list_t *_mklist(size_t len, ...)
{
	alpm_list_t *list = NULL;
	size_t l = len;
	va_list args;
	va_start(args, len);
	while(l--) {
		list = alpm_list_add(list, va_arg(args, void*));
	}
	va_end(args);
	if(alpm_list_count(list) != len) {
		tap_bail("_mklist: alpm_list_add failed");
		exit(99); /* automake "hard error" */
	}
	return list;
}

/* vim: set noet: */
