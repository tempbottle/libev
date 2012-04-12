/** @file
* @brief intrusive doubly linked list
* @author zhangyafeikimi@gmail.com
* @date
* @version
*
*/
#ifndef LIBEV_LIST_H
#define LIBEV_LIST_H

namespace libev {

  struct ListNode
  {
    ListNode * prev;
    ListNode * next;
  };

  class List
  {
  private:
    ListNode head_;

    static void __push(ListNode * _new,
      ListNode * prev, ListNode * next)
    {
      next->prev = _new;
      _new->next = next;
      _new->prev = prev;
      prev->next = _new;
    }

  public:
    List()
    {
      head_.next = &head_;
      head_.prev = &head_;
    }

    ~List()
    {
      head_.next = &head_;
      head_.prev = &head_;
    }

    void push_back(ListNode * _new)
    {
      __push(_new, head_.prev, &head_);
    }

    void push_front(ListNode * _new)
    {
      __push(_new, &head_, head_.next);
    }

    static void erase(ListNode * node)
    {
      node->next->prev = node->prev;
      node->prev->next = node->next;
    }

    void pop_back()
    {
      erase(head_.prev);
    }

    void pop_front()
    {
      erase(head_.next);
    }

    ListNode * back()const
    {
      return head_.prev;
    }

    ListNode * front()const
    {
      return head_.next;
    }

    ListNode * begin()
    {
      return head_.next;
    }

    const ListNode * begin()const
    {
      return head_.next;
    }

    ListNode * end()
    {
      return &head_;
    }

    const ListNode * end()const
    {
      return &head_;
    }

    bool empty()const
    {
      return &head_ == head_.next;
    }
  };
}

#endif
