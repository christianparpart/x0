#include <xzero/raft/ServerUtil.h>
#include <xzero/Random.h>
#include <algorithm>

namespace xzero {
namespace raft {

Duration ServerUtil::alleviatedDuration(Duration base) {
  static Random rng_;
  auto emin = base.milliseconds() / 2;
  auto emax = base.milliseconds();
  auto e = emin + rng_.random64() % (emax - emin);

  return Duration::fromMilliseconds(e);
}

Duration ServerUtil::cumulativeDuration(Duration base) {
  static Random rng_;
  auto emin = base.milliseconds();
  auto emax = base.milliseconds() / 2 + emin;
  auto e = emin + rng_.random64() % (emax - emin);

  return Duration::fromMilliseconds(e);
}

} // namespace raft
} // namespace xzero
