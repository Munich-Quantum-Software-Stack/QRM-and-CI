

#include <cudaq.h>

struct bell_state {
  void operator()() __qpu__ {
    cudaq::qvector q(2);

    h(q[0]);
    x<cudaq::ctrl>(q[0], q[1]);  // CNOT: q[0] controls, q[1] target

    mz(q);
  }
};

int main() {
  auto result = cudaq::sample(bell_state{});
  result.dump();
  return 0;
}