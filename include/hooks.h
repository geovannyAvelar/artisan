#pragma once

#include <memory>
#include <utility>

namespace artisan {

// A minimal `useState`-style hook: gives a Setup_<name>(Node&) function
// (see app.h) local state its event-handler closures can read and update,
// without hand-rolling a shared_ptr<T> or reaching for a class just to
// hold it.
//
//   auto [count, setCount] = UseState(0);
//   Node *label = root.FindById("count-label");
//   Node *button = root.FindById("increment-btn");
//   button->SetOnClick([=]() mutable {
//     setCount(count() + 1);
//     label->SetTextContent(std::to_string(count()));
//   });
//
// `count` and `setCount` both close over the same heap-allocated T, so any
// closure capturing either by value shares the same state - calling
// setCount from one handler is visible to another handler's count() call.
//
// Unlike a React hook, this does not re-run Setup_<name> or touch the Node
// tree on its own: setCount(v) only stores v. Setup_<name> still runs
// exactly once at startup (see app.h), so whatever DOM update needs to
// happen in response - SetTextContent, SetAttribute, etc. - stays the
// caller's job, same as any other imperative mutation in this framework.
template <typename T> class StateGetter {
public:
  explicit StateGetter(std::shared_ptr<T> value) : value_(std::move(value)) {}
  const T &operator()() const { return *value_; }

private:
  std::shared_ptr<T> value_;
};

template <typename T> class StateSetter {
public:
  explicit StateSetter(std::shared_ptr<T> value) : value_(std::move(value)) {}
  void operator()(T newValue) const { *value_ = std::move(newValue); }

private:
  std::shared_ptr<T> value_;
};

template <typename T>
std::pair<StateGetter<T>, StateSetter<T>> UseState(T initial) {
  auto value = std::make_shared<T>(std::move(initial));
  return {StateGetter<T>(value), StateSetter<T>(value)};
}

} // namespace artisan
