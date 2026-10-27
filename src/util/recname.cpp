#include "recname.h"
#include <cstdio>
#include <cstring>

namespace recname {
int next(const int* existing, int n) {
    int hi = 0;
    for (int i = 0; existing && i < n; i++)
        if (existing[i] > hi) hi = existing[i];
    return hi + 1;
}

void path(int number, char* out, size_t cap) { snprintf(out, cap, "/rec/%03d.log", number); }

int parse(const char* name) {
    if (!name) return -1;
    const char* dot = strchr(name, '.');
    if (!dot || strcmp(dot, ".log") != 0) return -1;
    const size_t digits = static_cast<size_t>(dot - name);
    if (digits == 0 || digits > 6) return -1;
    int v = 0;
    for (size_t i = 0; i < digits; i++) {
        if (name[i] < '0' || name[i] > '9') return -1;
        v = v * 10 + (name[i] - '0');
    }
    return v;
}

int oldest(const int* existing, int n, int current) {
    int lo = -1;
    for (int i = 0; existing && i < n; i++)
        if (existing[i] != current && (lo < 0 || existing[i] < lo)) lo = existing[i];
    return lo;
}

Space planSpace(size_t freeBytes, size_t reserveBytes, const int* existing, int n, int current) {
    if (freeBytes >= reserveBytes) return Space{0, false};
    if (oldest(existing, n, current) < 0) return Space{0, true};
    return Space{1, false};
}
}
