// Deliberate defects, built only for sanitizer infrastructure self-tests.
#include <limits>
#include <memory>
#include <string_view>

int main(int argc, char **argv) {
    if (argc != 2)
        return 90;
    if (std::string_view{argv[1]} == "address") {
        auto values = std::make_unique<int[]>(1);
        volatile int index = argc;
        return values[static_cast<std::size_t>(index)];
    }
    volatile int maximum = std::numeric_limits<int>::max();
    return maximum + 1;
}
