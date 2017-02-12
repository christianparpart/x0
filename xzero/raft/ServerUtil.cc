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

Index ServerUtil::majorityIndexOf(const ServerIndexMap& set) {
  const auto compFn = [](std::pair<Id, Index> a, std::pair<Id, Index> b) {
    return a.second < b.second;
  };
  const Index low = std::min_element(set.begin(),
                                     set.end(),
                                     compFn)->second;
  const Index high = std::max_element(set.begin(),
                                      set.end(),
                                      compFn)->second;
  const size_t quorum = set.size() / 2 + 1;
  Index result = low;

  // logDebug("raft.Server", "computeCommitIndex: min=$0, max=$1, quorum=$2",
  //          low, high, quorum);

  for (Index N = low; N <= high; N++) {
    size_t ok = 0;

    for (const auto& element: set) {
      if (element.second >= N) {
        ok++;
      }
    }

    if (ok >= quorum && N > result) {
      result = N;
    }
  }

  return result;
}


} // namespace raft
} // namespace xzero
