#include <xzero/base64.h>

namespace xzero {
namespace base64url {
  extern const int indexmap[256];

  template<typename Iterator, typename Output>
  std::string encode(Iterator begin, Iterator end) {
    static constexpr char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789-_";
    return base64::encode(begin, end, alphabet);
  }

  template<typename Iterator, typename Output>
  size_t decode(Iterator begin, Iterator end, Output* output) {
    return base64::decode(begin, end, indexmap, output);
  }
} // namespace base64url
} // namespace xzero

