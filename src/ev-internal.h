/** @file
* @brief libev internal header
* @author zhangyafeikimi@gmail.com
* @date
* @version
*
*/
#ifndef LIBEV_EV_INTERNAL_H
#define LIBEV_EV_INTERNAL_H

#include "list.h"
#include "log.h"

// from linux kernel
#define ev_offsetof(type, member) (size_t)((char *)&(((type *)1)->member) - 1)
#define ev_container_of(ptr, type, member) ((type *)((char *)ptr - ev_offsetof(type, member)))

// from google
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&); void operator=(const TypeName&)

namespace libev {

  enum EventInternalFlag
  {
    kInAllList = 0x01,
    kInActiveList = 0x02,
    kInCallback = 0x04,
  };

  struct EventInternal
  {
    ListNode all;// node in all event list(signal and timer events)
    ListNode active;// node in active event list(all events)

    int real_event;// the real event to be passed to callback
    int sig_times;// the times that the signal is triggered but pending(signal events)
    int flags;// bit or of EventInternalFlag

    EventInternal() : real_event(0), sig_times(0), flags(0) {}

    // return 1, in
    // return 0, not in
    int IsInList()const// signal and timer events
    {
      return flags & kInAllList;
    }

    void AddToList(List * list)// signal and timer events
    {
      EV_ASSERT(list);
      EV_ASSERT(!IsInList());
      list->push_back(&all);
      flags |= kInAllList;
    }

    void DelFromList(List * list)// signal and timer events
    {
      EV_ASSERT(list);
      EV_ASSERT(IsInList());
      list->erase(&all);
      flags &= ~kInAllList;
    }

    // return 1, active
    // return 0, not active
    int IsActive()const// all events
    {
      return flags & kInActiveList;
    }

    void AddToActive(List * active_list)// all events
    {
      EV_ASSERT(active_list);
      EV_ASSERT(!IsActive());
      active_list->push_back(&active);
      flags |= kInActiveList;
    }

    void DelFromActive(List * active_list)// all events
    {
      EV_ASSERT(active_list);
      EV_ASSERT(IsActive());
      active_list->erase(&active);
      flags &= ~kInActiveList;
    }
  };

}

#endif
