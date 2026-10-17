#pragma once

// Counts global operator new calls on all threads while armed. Used to prove processBlock does not allocate.
namespace allocguard
{
void arm();
long disarm(); // returns the number of allocations since arm()
} // namespace allocguard
