// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_CALLBACK_H_
#define BACKUP2_SRC_CALLBACK_H_

namespace backup2 {

// ResultCallback2 is a callback that returns a value, and takes two arguments
// to its Run() routine.
template<typename R, typename Arg1, typename Arg2>
class ResultCallback2 {
 public:
  virtual ~ResultCallback2() {}
  virtual R Run(Arg1 arg1, Arg2 arg2) = 0;
};

// ResultMethodClosure2 is like ResultCallback2, but allows for passing in
// method functions instead of free functions.
template<typename Result, class Class, typename Arg1, typename Arg2>
class ResultMethodClosure2 : public ResultCallback2<Result, Arg1, Arg2> {
 public:
  typedef Result (Class::*MethodType)(Arg1, Arg2);

  ResultMethodClosure2(Class* object, MethodType method, bool self_deleting)
      : object_(object), method_(method), self_deleting_(self_deleting) {}
  ~ResultMethodClosure2() {}

  Result Run(Arg1 arg1, Arg2 arg2) {
    bool needs_delete = self_deleting_;
    Result value = (object_->*method_)(arg1, arg2);
    if (needs_delete) delete this;
    return value;
  }

 private:
  Class* object_;
  MethodType method_;
  bool self_deleting_;
};

// Create a new permanent (i.e. won't be deleted after use) callback for a
// member function taking two arguments and returning a value.
template<typename Result, class Class, typename Arg1, typename Arg2>
inline ResultCallback2<Result, Arg1, Arg2>* NewPermanentCallback(
    Class* object, Result (Class::*method)(Arg1, Arg2)) {
  return new ResultMethodClosure2<Result, Class, Arg1, Arg2>(
      object, method, false);
}

}  // namespace backup2

#endif  // BACKUP2_SRC_CALLBACK_H_
