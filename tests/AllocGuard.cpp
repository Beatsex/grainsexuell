#include "AllocGuard.h"

#include <atomic>
#include <cstdlib>
#include <new>

namespace
{
std::atomic<bool> armed { false };
std::atomic<long> count { 0 };
} // namespace

namespace allocguard
{
void arm()
{
    count = 0;
    armed = true;
}

long disarm()
{
    armed = false;
    return count;
}
} // namespace allocguard

void* operator new(std::size_t size)
{
    if (armed)
        ++count;
    if (void* p = std::malloc(size ? size : 1))
        return p;
    throw std::bad_alloc();
}

void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
