/** @file
 * @brief intrusive min time heap
 * @author yafei.zhang@langtaojin.com
 * @date
 * @version
 *
 */
#ifndef LIBEV_HEAP_H
#define LIBEV_HEAP_H

#include "ev.h"
#include "log.h"
#include "header.h"
#include <vector>

namespace libev {

  class Heap
  {
    private:
      DISALLOW_COPY_AND_ASSIGN(Heap);

      std::vector<Event *> heap_;

    private:
      static bool greater(const Event * a, const Event * b)
      {
        return timespec_greater(&a->timeout, &b->timeout);
      }

      void shift_up(size_t hole_index, Event * node)
      {
        size_t parent = (hole_index - 1) >> 1;
        while (hole_index && greater(heap_[parent], node))
        {
          (heap_[hole_index] = heap_[parent])->fd = (int)hole_index;
          hole_index = parent;
          parent = (hole_index - 1) >> 1;
        }
        (heap_[hole_index] = node)->fd = (int)hole_index;
      }

      void shift_down(size_t hole_index, Event * node)
      {
        size_t min_child = (hole_index + 1) << 1;
        while (min_child <= heap_.size())
        {
          //lint -e514
          min_child -= (min_child == heap_.size()
              || greater(heap_[min_child], heap_[min_child - 1]));
          if(!(greater(node, heap_[min_child])))
            break;
          (heap_[hole_index] = heap_[min_child])->fd = (int)hole_index;
          hole_index = min_child;
          min_child = (hole_index + 1) << 1;
        }
        shift_up(hole_index,  node);
      }

    public:
      Heap() {}

      bool empty()const
      {
        return heap_.empty();
      }

      size_t size()const
      {
        return heap_.size();
      }

      void clear()
      {
        heap_.clear();
      }

      Event * top()
      {
        EV_ASSERT(!heap_.empty());
        return heap_[0];
      }

      const Event * top()const
      {
        EV_ASSERT(!heap_.empty());
        return heap_[0];
      }

      void push(Event * node)
      {
        EV_ASSERT(timespec_isset(&node->timeout));
        EV_ASSERT(node->fd == -1);

        heap_.push_back(node);// may throw(caught)
        shift_up(heap_.size()-1, node);
      }

      void pop()
      {
        EV_ASSERT(!heap_.empty());
        Event * node = heap_[0];
        shift_down(0, heap_[heap_.size()-1]);
        heap_.pop_back();
        EV_ASSERT(timespec_isset(&node->timeout));
        EV_ASSERT(node->fd != -1);
        node->fd = -1;
      }

      void erase(Event * node)
      {
        EV_ASSERT(timespec_isset(&node->timeout));
        EV_ASSERT(node->fd != -1);
        EV_ASSERT(!heap_.empty());

        Event * last = heap_.back();
        heap_.pop_back();
        size_t parent = ((size_t)node->fd - 1) << 1;
        if (node->fd > 0 && greater(heap_[parent], last))
          shift_up((size_t)node->fd, last);
        else
          shift_down((size_t)node->fd, last);
        node->fd = -1;
      }
  };

}

#endif
