#include "strtc_engine_interface.h"

#include "strtc_engine.h"

namespace strtc {

StrtcEngineInterface* StrtcEngineInterface::create(
    StrtcEngineObserver* observer) {
  return new StrtcEngine(observer);
}
}  // namespace strtc