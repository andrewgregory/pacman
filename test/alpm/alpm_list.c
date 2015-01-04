#include "alpm.h"
#include "tap.h"

#define CHECK_LIST_STR(...) check_list_str(__FILE__, __LINE__, __VA_ARGS__)
int check_list_str(const char *file, int lineno,
		alpm_list_t *got, char **expected, int len, const char *name, ...)
{
    int i = 0, success = 0;
	va_list args;
    va_start(args, name);
    while(got && i < len) {
        const char *g = got->data, *e = expected[i];
        if(!(g == e || (g && e && strcmp(g, e) == 0))) {
            break;
        }
        got = got->next;
        i++;
    }
    success = (got == NULL && i == len);
	if(!_tap_vok(file, lineno, success, name, args)) {
		tap_diag("    Lists began differing at:");
		if(got) {
			tap_diag("          got[%zu] = '%s'", i, got->data);
		} else {
			tap_diag("          got[%zu] = Does not exist", i);
		}
		if(i < len) {
			tap_diag("     expected[%zu] = '%s'", i, expected[i]);
		} else {
			tap_diag("     expected[%zu] = Does not exist", i);
		}
	}
	va_end(args);
	return success;
}

/*************************************
 * List Creators/Modifiers
 *************************************/

void check_alpm_list_add(void)
{
    char *expected[3] = {"1", "2", "3"};
    alpm_list_t *list = NULL;

    list = alpm_list_add(list, "1");
    CHECK_LIST_STR(list, expected, 1, "alpm_list_add to empty list");

    list = alpm_list_add(list, "2");
    CHECK_LIST_STR(list, expected, 2, "alpm_list_add to existing list");

    alpm_list_free(list);
}

void check_alpm_list_mmerge(void)
{
    char *expected[5] = {"1", "2", "3", "4", "5"};

    alpm_list_t *list1 = NULL;
    list1 = alpm_list_add(list1, "1");
    list1 = alpm_list_add(list1, "3");
    list1 = alpm_list_add(list1, "5");

    alpm_list_t *list2 = NULL;
    list2 = alpm_list_add(list2, "2");
    list2 = alpm_list_add(list2, "4");

    list1 = alpm_list_mmerge(list1, list2, (alpm_list_fn_cmp) strcmp);

    CHECK_LIST_STR(list1, expected, 5, "alpm_list_mmerge result order");

    alpm_list_free(list1);
}

void check_alpm_list_msort(void)
{
    char *expected[3] = {"1", "2", "3"};
    alpm_list_t *list = NULL;
    list = alpm_list_add(list, "1");
    list = alpm_list_add(list, "3");
    list = alpm_list_add(list, "2");

    list = alpm_list_msort(list, alpm_list_count(list), (alpm_list_fn_cmp) strcmp);

    CHECK_LIST_STR(list, expected, 3, "alpm_list_msort result order");

    alpm_list_free(list);
}

void check_alpm_list_join(void)
{
    char *expected[5] = {"1", "3", "5", "2", "4"};

    alpm_list_t *list = NULL;
    list = alpm_list_add(list, "1");
    list = alpm_list_add(list, "3");
    list = alpm_list_add(list, "5");

    alpm_list_t *list2 = NULL;
    list2 = alpm_list_add(list2, "2");
    list2 = alpm_list_add(list2, "4");

    list = alpm_list_join(list, list2);

    CHECK_LIST_STR(list, expected, 5, "alpm_list_join order");

    alpm_list_free(list);
}

void check_alpm_list_add_sorted(void)
{
    char *expected[3] = {"1", "2", "3"};
    alpm_list_t *list = NULL;
    list = alpm_list_add(list, "1");
    list = alpm_list_add(list, "3");

    list = alpm_list_add_sorted(list, "2", (alpm_list_fn_cmp) strcmp);

    CHECK_LIST_STR(list, expected, 3, "alpm_list_add_sorted result order");

    alpm_list_free(list);
}

void check_alpm_list_remove_item(void)
{
    char *expected[3] = {"1", "3"};
    alpm_list_t *list = NULL;
    alpm_list_t *removed;
    list = alpm_list_add(list, "1");
    list = alpm_list_add(list, "2");
    list = alpm_list_add(list, "3");

    removed = list->next;
    list = alpm_list_remove_item(list, removed);

    CHECK_LIST_STR(list, expected, 2, "alpm_list_remove_item result order");
    tap_is_str(removed->data, "2", "removed the item");
    tap_ok(removed->next == NULL && removed->prev == NULL, "single item");

    alpm_list_free(list);
    alpm_list_free(removed);
}

void check_alpm_list_remove(void)
{
    char *expected[3] = {"1", "3"};
    alpm_list_t *list = NULL;
    char *removed;

    list = alpm_list_add(list, "1");
    list = alpm_list_add(list, "2");
    list = alpm_list_add(list, "3");

    list = alpm_list_remove(list, "2", (alpm_list_fn_cmp) strcmp, (void*) &removed);

    CHECK_LIST_STR(list, expected, 2, "alpm_list_remove result order");
    tap_is_str(removed, "2", "alpm_list_remove returned data");

    alpm_list_free(list);
}

void check_alpm_list_remove_str(void)
{
    char *expected[3] = {"1", "3"};
    alpm_list_t *list = NULL;
    char *removed;

    list = alpm_list_add(list, "1");
    list = alpm_list_add(list, "2");
    list = alpm_list_add(list, "3");

    list = alpm_list_remove_str(list, "2", &removed);

    CHECK_LIST_STR(list, expected, 2, "alpm_list_remove_str result list");
    tap_is_str(removed, "2", "alpm_list_remove_str returned data");

    alpm_list_free(list);
}

void check_alpm_list_remove_dupes(void)
{
    char *expected[3] = {"1", "2", "3"};
    alpm_list_t *list = NULL, *dedupe;

    list = alpm_list_add(list, expected[0]);
    list = alpm_list_add(list, expected[1]);
    list = alpm_list_add(list, expected[0]);
    list = alpm_list_add(list, expected[2]);
    list = alpm_list_add(list, expected[1]);
    list = alpm_list_add(list, expected[2]);

    dedupe = alpm_list_remove_dupes(list);

    CHECK_LIST_STR(dedupe, expected, 3, "alpm_list_remove_dups result list");

    alpm_list_free(list);
    alpm_list_free(dedupe);
}

void check_alpm_list_strdup(void)
{
    alpm_list_t *list = NULL;
    alpm_list_t *list2, *l1, *l2;

    list = alpm_list_add(list, "1");
    list = alpm_list_add(list, "2");
    list = alpm_list_add(list, "3");

    list2 = alpm_list_strdup(list);

    tap_is_int(alpm_list_count(list), alpm_list_count(list2), NULL);
    for(l1 = list, l2 = list2; l1 && l2; l1 = l1->next, l2 = l2->next) {
        tap_ok(l1 != l2, NULL);
        tap_ok(l1->data != l2->data, NULL);
        tap_is_str(l1->data, l2->data, NULL);
    }

    alpm_list_free(list);
    FREELIST(list2);
}

void check_alpm_list_copy(void)
{
    alpm_list_t *list = NULL;
    alpm_list_t *list2, *l1, *l2;

    list = alpm_list_add(list, "1");
    list = alpm_list_add(list, "2");
    list = alpm_list_add(list, "3");

    list2 = alpm_list_copy(list);

    tap_is_int(alpm_list_count(list), alpm_list_count(list2), NULL);
    for(l1 = list, l2 = list2; l1 && l2; l1 = l1->next, l2 = l2->next) {
        tap_ok(l1 != l2, NULL);
        tap_ok(l1->data == l2->data, NULL);
    }

    alpm_list_free(list);
    alpm_list_free(list2);
}

void check_alpm_list_copy_data(void)
{
    alpm_list_t *list = NULL;
    alpm_list_t *list2, *l1, *l2;

    list = alpm_list_add(list, "1");
    list = alpm_list_add(list, "2");
    list = alpm_list_add(list, "3");

    list2 = alpm_list_copy_data(list, sizeof(char) * 2);

    tap_is_int(alpm_list_count(list), alpm_list_count(list2), NULL);
    for(l1 = list, l2 = list2; l1 && l2; l1 = l1->next, l2 = l2->next) {
        tap_ok(l1 != l2, NULL);
        tap_ok(l1->data != l2->data, NULL);
        tap_is_str(l1->data, l2->data, NULL);
    }

    alpm_list_free(list);
    FREELIST(list2);
}

void check_alpm_list_reverse(void)
{
    char *expected[3] = {"2", "3", "1"};
    alpm_list_t *list = NULL, *rev;
    list = alpm_list_add(list, "1");
    list = alpm_list_add(list, "3");
    list = alpm_list_add(list, "2");

    rev = alpm_list_reverse(list);

    CHECK_LIST_STR(rev, expected, 3, "alpm_list_reverse resulting list");

    alpm_list_free(list);
    alpm_list_free(rev);
}

void check_alpm_list_diff_sorted(void)
{
    char *expected_lhs[3] = {"3", "4"};
    char *expected_rhs[3] = {"7", "8"};
    alpm_list_t *list = NULL;
    alpm_list_t *list2= NULL;
    alpm_list_t *lhs = NULL, *rhs = NULL;

    list = alpm_list_add(list, "1");
    list = alpm_list_add(list, "2");
    list = alpm_list_add(list, "3");
    list = alpm_list_add(list, "4");
    list = alpm_list_add(list, "5");

    list2 = alpm_list_add(list2, "1");
    list2 = alpm_list_add(list2, "2");
    list2 = alpm_list_add(list2, "5");
    list2 = alpm_list_add(list2, "7");
    list2 = alpm_list_add(list2, "8");

    alpm_list_diff_sorted(list, list2, (alpm_list_fn_cmp) strcmp, &lhs, &rhs);

    CHECK_LIST_STR(lhs, expected_lhs, 2, "alpm_list_diff_sorted lhs");
    CHECK_LIST_STR(rhs, expected_rhs, 2, "alpm_list_diff_sorted rhs");

    alpm_list_free(list);
    alpm_list_free(list2);
    alpm_list_free(lhs);
    alpm_list_free(rhs);
}

void check_alpm_list_diff(void)
{
    char *expected[3] = {"3", "4"};
    alpm_list_t *list = NULL;
    alpm_list_t *list2= NULL;
    alpm_list_t *diff;

    list = alpm_list_add(list, "5");
    list = alpm_list_add(list, "1");
    list = alpm_list_add(list, "4");
    list = alpm_list_add(list, "3");
    list = alpm_list_add(list, "2");

    list2 = alpm_list_add(list2, "5");
    list2 = alpm_list_add(list2, "1");
    list2 = alpm_list_add(list2, "2");

    diff = alpm_list_diff(list, list2, (alpm_list_fn_cmp) strcmp);

    CHECK_LIST_STR(diff, expected, 2, "alpm_list_diff result list");

    alpm_list_free(list);
    alpm_list_free(list2);
    alpm_list_free(diff);
}

/*************************************
 * List Accessors
 *************************************/

void check_alpm_list_nth(void)
{
    alpm_list_t *list = NULL;

    list = alpm_list_add(list, "1");
    list = alpm_list_add(list, "2");
    list = alpm_list_add(list, "3");

    tap_is_str(alpm_list_nth(list, 0)->data, "1", "alpm_list_nth first item");
    tap_is_str(alpm_list_nth(list, 1)->data, "2", "alpm_list_nth middle item");
    tap_is_str(alpm_list_nth(list, 2)->data, "3", "alpm_list_nth last item");

    alpm_list_free(list);
}

void check_alpm_list_count()
{
    alpm_list_t *list = NULL;

    tap_is_int(alpm_list_count(list), 0, "alpm_list_count empty list");

    list = alpm_list_add(list, "1");
    tap_is_int(alpm_list_count(list), 1, "alpm_list_count 1-item list");

    list = alpm_list_add(list, "2");
    tap_is_int(alpm_list_count(list), 2, "alpm_list_count 2-item list");

    list = alpm_list_add(list, "3");
    tap_is_int(alpm_list_count(list), 3, "alpm_list_count 3-item list");

    alpm_list_free(list);
}

void check_alpm_list_last()
{
    alpm_list_t *list = NULL;

    tap_ok(alpm_list_last(list) == NULL, "alpm_list_last empty list");

    list = alpm_list_add(list, "1");
    tap_is_str(alpm_list_last(list)->data, "1", "alpm_list_last 1-item list");

    list = alpm_list_add(list, "2");
    tap_is_str(alpm_list_last(list)->data, "2", "alpm_list_last 2-item list");

    list = alpm_list_add(list, "3");
    tap_is_str(alpm_list_last(list)->data, "3", "alpm_list_last 3-item list");

    alpm_list_free(list);
}

void check_alpm_list_next()
{
    alpm_list_t *list = NULL;

    tap_ok(alpm_list_next(list) == NULL, "alpm_list_next empty list");

    list = alpm_list_add(list, "1");
    tap_ok(alpm_list_next(list) == NULL, "alpm_list_next 1-item list");

    list = alpm_list_add(list, "2");
    tap_ok(alpm_list_next(list) == list->next, "alpm_list_next 2-item list");

    alpm_list_free(list);
}

void check_alpm_list_previous(void)
{
    alpm_list_t *list = NULL;
    tap_ok(alpm_list_previous(list) == NULL, NULL);
    list = alpm_list_add(list, "1");
    tap_ok(alpm_list_previous(list) == NULL, NULL);
    list = alpm_list_add(list, "2");
    tap_ok(alpm_list_previous(list->next) == list, NULL);
    alpm_list_free(list);
}

void check_alpm_list_find_str(void)
{
    alpm_list_t *list = NULL;
    list = alpm_list_add(list, "1");
    list = alpm_list_add(list, "2");
    list = alpm_list_add(list, "3");
    tap_ok(alpm_list_find_str(list, "0") == NULL, NULL);
    tap_is_str(alpm_list_find_str(list, "2"), "2", NULL);
    alpm_list_free(list);
}

void check_alpm_list_to_array(void)
{
    char *array;
    alpm_list_t *list = NULL;
    list = alpm_list_add(list, "1");
    list = alpm_list_add(list, "2");
    list = alpm_list_add(list, "3");

    array = alpm_list_to_array(list, alpm_list_count(list), sizeof(char));
    tap_is_int(array[0], '1', NULL);
    tap_is_int(array[1], '2', NULL);
    tap_is_int(array[2], '3', NULL);
    free(array);

    array = alpm_list_to_array(list, 2, sizeof(char));
    tap_is_int(array[0], '1', NULL);
    tap_is_int(array[1], '2', NULL);
    free(array);

    alpm_list_free(list);
}


void check_alpm_list_find(void)
{
    char *expected[3] = {"2", "3", "1"};
    char *found;
    alpm_list_t *list = NULL;

    list = alpm_list_add(list, "1");
    list = alpm_list_add(list, "3");
    list = alpm_list_add(list, "2");

    found = alpm_list_find(list, "2", (alpm_list_fn_cmp) strcmp);

    tap_ok(found == expected[0], NULL);

    alpm_list_free(list);
}

void check_alpm_list_find_ptr(void)
{
    char *expected[3] = {"2", "3", "1"};
    char *found;
    alpm_list_t *list = NULL;

    list = alpm_list_add(list, "1");
    list = alpm_list_add(list, "3");
    list = alpm_list_add(list, "2");

    found = alpm_list_find_ptr(list, expected[0]);

    tap_ok(found == expected[0], NULL);

    alpm_list_free(list);
}

int main(void)
{
    tap_plan(71);

    check_alpm_list_add();
    check_alpm_list_nth();
    check_alpm_list_count();
    check_alpm_list_last();
    check_alpm_list_next();
    check_alpm_list_previous();
    check_alpm_list_find_str();
    check_alpm_list_to_array();
    check_alpm_list_mmerge();
    check_alpm_list_msort();
    check_alpm_list_join();
    check_alpm_list_add_sorted();
    check_alpm_list_remove_item();
    check_alpm_list_remove();
    check_alpm_list_remove_str();
    check_alpm_list_remove_dupes();
    check_alpm_list_strdup();
    check_alpm_list_copy();
    check_alpm_list_copy_data();
    check_alpm_list_reverse();
    check_alpm_list_find();
    check_alpm_list_find_ptr();
    check_alpm_list_diff();
    check_alpm_list_diff_sorted();

    return tap_finish();
}
