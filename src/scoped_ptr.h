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
    typedef ScopedPtr<T> SelfType;
    typedef T ElementType;
    typedef T ValueType;
    typedef T * Pointer;
    typedef T& Reference;

  private:
    Pointer px_;

  public:
    explicit ScopedPtr(Pointer p = 0): px_(p)
    {
    }

    ~ScopedPtr() // never throws
    {
      delete px_;
    }

    void Reset(Pointer p = 0) // never throws
    {
      if (p == px_)
        return;

      SelfType(p).Swap(*this);
    }

    Reference operator*() const // never throws
    {
      return *px_;
    }

    Pointer operator->() const // never throws
    {
      return px_;
    }

    Pointer Get() const // never throws
    {
      return px_;
    }

    operator bool () const
    {
      return px_ != 0;
    }

    void Swap(SelfType& b) // never throws
    {
      T * tmp = px_;
      px_ = b.px_;
      b.px_ = tmp;
    }

    friend inline void Swap(SelfType& a, SelfType& b) // never throws
    {
      T * tmp = a.px_;
      a.px_ = b.px_;
      b.px_ = tmp;
    }
  };
}

#endif
