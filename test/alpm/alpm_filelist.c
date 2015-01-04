#include "alpm.h"
#include "tap.h"

#if 0 /* TODO: export filelist_difference and filelist_intersection */

void check_filelist_difference(void)
{
	alpm_file_t files1[] = {
		{ .name = "baz" },
		{ .name = "foo/" },
		{ .name = "foo/bar/" },
		{ .name = "xxx" },
	};
	alpm_filelist_t fl1 = {
		.files = files1,
		.count = sizeof(files1) / sizeof(*files1),
	};
	alpm_file_t files2[] = {
		{ .name = "baz" },
		{ .name = "foo/" },
		{ .name = "foo/bar" },
		{ .name = "yyy" },
		{ .name = "zzz" },
	};
	alpm_filelist_t fl2 = {
		.files = files2,
		.count = sizeof(files2) / sizeof(*files2),
	};

#define CHECK_DIFF(a, b, len, ...) \
	do { \
		alpm_list_t *intersection = _alpm_filelist_difference(a, b); \
		alpm_list_t *expected = _mklist(len, __VA_ARGS__); \
		IS_LIST_STR(intersection, expected, "alpm_filelist_difference"); \
		alpm_list_free(intersection); \
		alpm_list_free(expected); \
	} while(0)

	CHECK_DIFF(&fl1, &fl2, 2, "foo/bar/", "xxx");
	CHECK_DIFF(&fl2, &fl1, 3, "foo/bar", "yyy", "zzz");

#undef CHECK_DIFF
}

void check_filelist_intersection(void)
{
	alpm_file_t files1[] = {
		{ .name = "baz" },
		{ .name = "foo/" },
		{ .name = "foo/bar/" },
		{ .name = "xxx" },
	};
	alpm_filelist_t fl1 = {
		.files = files1,
		.count = sizeof(files1) / sizeof(*files1),
	};
	alpm_file_t files2[] = {
		{ .name = "baz" },
		{ .name = "foo/" },
		{ .name = "foo/bar" },
		{ .name = "yyy" },
	};
	alpm_filelist_t fl2 = {
		.files = files2,
		.count = sizeof(files2) / sizeof(*files2),
	};

#define CHECK(a, b, len, ...) \
	do { \
		alpm_list_t *intersection = _alpm_filelist_intersection(a, b); \
		alpm_list_t *expected = _mklist(len, __VA_ARGS__); \
		IS_LIST_STR(intersection, expected, "alpm_filelist_intersection"); \
		alpm_list_free(intersection); \
		alpm_list_free(expected); \
	} while(0)

	CHECK(&fl1, &fl2, 2, "baz", "foo/bar/");
	CHECK(&fl2, &fl1, 2, "baz", "foo/bar");

#undef CHECK
}

#endif

void check_filelist_contains(void)
{
	alpm_file_t files[] = {
		{ .name = "foo/" },
		{ .name = "foo/bar/" },
		{ .name = "foo/bar/baz" },
		{ .name = "quux" },
	};
	alpm_filelist_t fl = {
		.files = files,
		.count = sizeof(files) / sizeof(*files),
	};

#define CHECK(p, e) do { \
		alpm_file_t *f = alpm_filelist_contains(&fl, p); \
		tap_is_str(f ? f->name : NULL, e, "alpm_filelist_contains(%s)", #p); \
	} while(0)
#define CHECKIN(p) CHECK(p, p)
#define CHECKOUT(p) CHECK(p, NULL)

	CHECKIN("foo/");
	CHECKIN("foo/bar/");
	CHECKIN("foo/bar/baz");
	CHECKIN("quux");

	CHECKOUT("foo");
	CHECKOUT("quux/");

#undef CHECKOUT
#undef CHECKIN
#undef CHECK
}

int main(void)
{
	tap_plan(6);
	check_filelist_contains();
	return tap_finish();
}

/* vim: set noet: */
