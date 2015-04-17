#include "../alpmtest.h"

#define CHECK(v1, v2, e) tap_is_int(alpm_pkg_vercmp(v1, v2), e, v1", "v2)

int main(void) {
    CHECK("1.0.0", "1.0.0", 0);
    CHECK("1.0.0", "1.0.1", -1);
    CHECK("1.0.1", "1.0.0", 1);

    CHECK("1.0.0", "0:1.0.0", 0);
    CHECK("1.0.0", "1:1.0.0", -1);
    CHECK("1.0.1", "1:1.0.0", -1);

    CHECK("1.0.0-1", "1.0.0-1", 0);
    CHECK("1.0.0", "1.0.0-1", 0);
    CHECK("1.0.0-1", "1.0.0", 0);
    CHECK("1.0.0-1", "1.0.0-2", -1);

    CHECK("01", "02", -1);
    CHECK("01", "001", 0);

    tap_done_testing();
    return tap_finish();
}
