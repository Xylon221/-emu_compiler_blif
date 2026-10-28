#include "strstore.h"
#include <cassert>
#include <cstring>
#include <string>
int main() {
    const char raw[] = {'a', 'b', 'c', 'X'};
    const char* first = strstore_intern(raw, 3);
    assert(std::strcmp(first, "abc") == 0);
    assert(first == strstore_intern("abc", 3));
    // Exceed 70% of the original 1M-slot table, exercising rehash.
    for (unsigned i = 0; i < 740000; ++i) {
        std::string key = "signal_" + std::to_string(i);
        strstore_intern(key.data(), key.size());
    }
    assert(strstore_unique_count() == 740001);
    assert(first == strstore_intern("abc", 3)); // stable string addresses
    strstore_reset();
    assert(strstore_unique_count() == 0);
    assert(std::strcmp(strstore_intern("new", 3), "new") == 0);
    strstore_reset();
}
