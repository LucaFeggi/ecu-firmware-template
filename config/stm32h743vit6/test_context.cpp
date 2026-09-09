#include "test_context.hpp"

namespace target::test {
namespace {

Context test_context{};

} // namespace

Context& context() noexcept { return test_context; }

void check_in(Test test) noexcept {
  context().check_ins.fetch_or(bit(test), std::memory_order_relaxed);
}

void record(Test test, bool success) noexcept {
  auto& state = context();
  const auto mask = bit(test);
  if (success) {
    state.failed.fetch_and(~mask, std::memory_order_relaxed);
    state.passed.fetch_or(mask, std::memory_order_relaxed);
  } else {
    state.passed.fetch_and(~mask, std::memory_order_relaxed);
    state.failed.fetch_or(mask, std::memory_order_relaxed);
  }
}

} // namespace target::test
