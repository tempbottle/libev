/** @file
 * @brief libev internal header
 * @author zhangyafeikimi@gmail.com
 * @date
 * @version
 *
 */
#ifndef LIBEV_EV_INTERNAL_H
#define LIBEV_EV_INTERNAL_H

// from linux kernel
#define ev_offsetof(type, member) (size_t)((char *)&(((type *)1)->member) - 1)
#define ev_container_of(ptr, type, member) ((type *)((char *)ptr - ev_offsetof(type, member)))

// from google
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&); TypeName& operator=(const TypeName&)

#endif
