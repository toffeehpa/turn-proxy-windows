#include "test.h"

#include <cstdio>
#include <exception>

namespace ft::test {

namespace {
int g_failures = 0;
}

std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

void fail(std::string_view file, int line, std::string_view expr, const std::string& detail) {
    ++g_failures;
    std::fprintf(stderr, "  FAIL %.*s:%d: %.*s %s\n", static_cast<int>(file.size()), file.data(), line,
                 static_cast<int>(expr.size()), expr.data(), detail.c_str());
}

} // namespace ft::test

int main(int argc, char** argv) {
    using namespace ft::test;
    const std::string_view filter = argc > 1 ? argv[1] : "";
    int ran = 0;
    int failed_cases = 0;
    for (const auto& c : registry()) {
        if (!filter.empty() && c.name.find(filter) == std::string_view::npos) {
            continue;
        }
        ++ran;
        const int before = g_failures;
        try {
            c.fn();
        } catch (const Abort&) {
        } catch (const std::exception& e) {
            fail(c.name, 0, "exception", e.what());
        }
        if (g_failures != before) {
            ++failed_cases;
            std::fprintf(stderr, "[FAIL] %.*s\n", static_cast<int>(c.name.size()), c.name.data());
        } else {
            std::printf("[ ok ] %.*s\n", static_cast<int>(c.name.size()), c.name.data());
        }
    }
    std::printf("%d cases, %d failed\n", ran, failed_cases);
    return failed_cases == 0 && ran > 0 ? 0 : 1;
}
