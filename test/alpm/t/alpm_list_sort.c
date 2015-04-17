#include "../alpmtest.h"

static int64_t get_time_ms(void)
{
	struct timespec ts = {0, 0};
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (ts.tv_sec * 1000000) + (ts.tv_nsec / 1000);
}

void print_list(alpm_list_t *list) {
    size_t i = 0;
    while(list) { printf("%zd: %s\n", i, list->data); i++; list = list->next; }
}

alpm_list_t *cmp_sort(alpm_list_t *list, const char *label) {
    alpm_list_t *copy = alpm_list_copy(list);
    int64_t start, finish;
    size_t count = alpm_list_count(list);

    start = get_time_ms();
    list = alpm_list_msort(list, count, (alpm_list_fn_cmp) strcmp);
    finish = get_time_ms();
    printf("%s lists - %zd items - standard: %d us\n", label, count, finish - start);
    
    /* start = get_time_ms(); */
    /* copy = alpm_list_msort_natural(copy, (alpm_list_fn_cmp) strcmp); */
    /* finish = get_time_ms(); */
    /* printf("%s lists - %zd items - natural:  %d ms\n", label, count, finish - start); */
    
    alpm_list_free(copy);
    return list;
}

void cmp_sorted(void) {
    char buf[10];
    size_t i;
    alpm_list_t *list = NULL;

    for(i = 0; i < 10; i++) {
        sprintf(buf, "%06d", i);
        list = alpm_list_add(list, strdup(buf));
    }
    cmp_sort(list, "sorted");
    list = NULL;

    for(i = 0; i < 1000; i++) {
        sprintf(buf, "%06d", i);
        list = alpm_list_add(list, strdup(buf));
    }
    cmp_sort(list, "sorted");
    list = NULL;

    for(i = 0; i < 100000; i++) {
        sprintf(buf, "%06d", i);
        list = alpm_list_add(list, strdup(buf));
    }
    cmp_sort(list, "sorted");
}

void cmp_reversed(void) {
    char buf[10];
    size_t i;
    alpm_list_t *list = NULL;

    for(i = 10; i > 0; i--) {
        sprintf(buf, "%06d", i);
        list = alpm_list_add(list, strdup(buf));
    }
    list = cmp_sort(list, "reversed");
    list = NULL;

    for(i = 1000; i > 0; i--) {
        sprintf(buf, "%06d", i);
        list = alpm_list_add(list, strdup(buf));
    }
    cmp_sort(list, "reversed");
    list = NULL;

    for(i = 100000; i > 0; i--) {
        sprintf(buf, "%06d", i);
        list = alpm_list_add(list, strdup(buf));
    }
    cmp_sort(list, "reversed");
}

void cmp_random(void) {
    char buf[10];
    size_t i;
    alpm_list_t *list = NULL;

    for(i = 0; i < 10; i++) {
        sprintf(buf, "%06d", rand() % 100000);
        list = alpm_list_add(list, strdup(buf));
    }
    cmp_sort(list, "random");
    list = NULL;

    for(i = 0; i < 1000; i++) {
        sprintf(buf, "%06d", rand() % 100000);
        list = alpm_list_add(list, strdup(buf));
    }
    cmp_sort(list, "random");
    list = NULL;

    for(i = 0; i < 100000; i++) {
        sprintf(buf, "%06d", rand() % 100000);
        list = alpm_list_add(list, strdup(buf));
    }
    cmp_sort(list, "random");
}

void main(void) {
    cmp_sorted();
    printf("---------------------------------\n");
    cmp_sorted();
    cmp_reversed();
    cmp_random();
}
