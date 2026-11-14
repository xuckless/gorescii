#include "check.h"
#include "version.h"

#include <cstring>

TEST(version_is_set) { CHECK(std::strlen(tg::version()) > 0); }

int main() {
    for (auto& c : check::registry()) {
        int before = check::failures();
        c.fn();
        std::printf("%s %s\n", check::failures() == before ? "ok  " : "FAIL", c.name);
    }
    return check::failures() == 0 ? 0 : 1;
}
