/** @file
* @brief scoped ptr
* @author zhangyafeikimi@gmail.com
* @date
* @version
*
*/
#ifndef LIBEV_SCOPED_PTR_H
#define LIBEV_SCOPED_PTR_H

namespace libev {

  template<class T>
  class ScopedPtr
  {
  private:
    ScopedPtr(const ScopedPtr&);
    ScopedPtr& operator=(const ScopedPtr&);

  public:
    typedef ScopedPtr<T> self_type;
    typedef T * pointer;
    typedef T& reference;

  private:
    pointer px_;

  public:
    explicit ScopedPtr(pointer p = 0): px_(p)
    {
    }

    ~ScopedPtr()
    {
      delete px_;
    }

    void reset(pointer p = 0)
    {
      if (p == px_)
        return;

      self_type(p).swap(*this);
    }

    reference operator*() const
    {
      return *px_;
    }

    pointer operator->() const
    {
      return px_;
    }

    pointer get() const
    {
      return px_;
    }

    operator bool () const
    {
      return px_ != 0;
    }

    void swap(self_type& b)
    {
      T * tmp = px_;
      px_ = b.px_;
      b.px_ = tmp;
    }

    friend inline void swap(self_type& a, self_type& b)
    {
      T * tmp = a.px_;
      a.px_ = b.px_;
      b.px_ = tmp;
    }
  };
}

#endif
