// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_COMMON_H_
#define BACKUP2_SRC_COMMON_H_

#ifdef __GNUC__
  #define MUST_USE_RESULT __attribute__ ((warn_unused_result))
#else
  #define MUST_USE_RESULT
#endif

// Macro to disallow the invocation of copy constructor
// and assignment operator.
#ifdef __GNUC__
  #define DISALLOW_COPY_AND_ASSIGN(T) \
    T& operator=(const T&) = delete; \
    T(const T&) = delete;

  #define DISALLOW_CTOR(T) \
    T() = delete
#else
  // VS2012 does not support delete yet so we go with
  // the old version instead.
  #define DISALLOW_COPY_AND_ASSIGN(T) \
    T(const T&); \
    void operator=(const T&);

  #define DISALLOW_CTOR(T) \
    T()
#endif

#endif  // BACKUP2_SRC_COMMON_H_
