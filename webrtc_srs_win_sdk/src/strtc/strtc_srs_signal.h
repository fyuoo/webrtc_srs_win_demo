#ifndef STRTC_SRS_SIGNAL_H_
#define STRTC_SRS_SIGNAL_H_

#include <memory>

#include "strtc_common_define.h"
#include "strtc_http_client.h"

namespace strtc {
class StrtcSrsSignal {
 public:
  StrtcSrsSignal();
  ~StrtcSrsSignal();

  int post(const std::string& url, const std::string& offer,
           std::string* answer, ChannelType type);

 private:
  std::unique_ptr<HttpClient> http_client_;
  std::string request_id_;
};
}  // namespace strtc
#endif  // STRTC_SRS_SIGNAL_H_