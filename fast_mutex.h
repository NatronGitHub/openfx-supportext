/* -*- mode: c++; tab-width: 2; indent-tabs-mode: nil; -*-
Copyright (c) 2010-2012 Marcus Geelnard

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
    claim that you wrote the original software. If you use this software
    in a product, an acknowledgment in the product documentation would be
    appreciated but is not required.

    2. Altered source versions must be plainly marked as such, and must not be
    misrepresented as being the original software.

    3. This notice may not be removed or altered from any source
    distribution.
*/

#ifndef _FAST_MUTEX_H_
#define _FAST_MUTEX_H_

// tthread::fast_mutex: a spinlock, kept for the plug-ins that use it
#include <atomic>
#include <thread>

namespace tthread {

// A spinlock based on std::atomic_flag
// Ref: Antony Williams, "C++ Concurrency in Action" (Listing 5.1)
class fast_mutex
{
 private:
  std::atomic_flag _lockFlag = ATOMIC_FLAG_INIT;

 public:
  fast_mutex() = default;

  void lock()
  {
    while(_lockFlag.test_and_set(std::memory_order_acquire))
    {
      std::this_thread::yield(); // don't busy-wait
    }
  }

  bool try_lock()
  {
    return !_lockFlag.test_and_set(std::memory_order_acquire);
  }

  void unlock()
  {
    _lockFlag.clear(std::memory_order_release);
  }
};

}


#endif // _FAST_MUTEX_H_
