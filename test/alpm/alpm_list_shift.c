#include "alpm_list.h"
#include "tap.h"

int main(void)
{
	alpm_list_t *l = NULL;
	char *c, *data1 = "foo", *data2 = "bar", *data3 = "baz";

	alpm_list_append(&l, data1);
	alpm_list_append(&l, data2);
	alpm_list_append(&l, data3);

	tap_plan(4);

	c = alpm_list_shift(&l);
	tap_ok(c == data1, NULL);

	c = alpm_list_shift(&l);
	tap_ok(c == data2, NULL);

	c = alpm_list_shift(&l);
	tap_ok(c == data3, NULL);

	c = alpm_list_shift(&l);
	tap_ok(c == NULL, NULL);

	return tap_finish();
}
