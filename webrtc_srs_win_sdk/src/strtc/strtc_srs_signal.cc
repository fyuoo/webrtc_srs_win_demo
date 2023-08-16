#include "strtc_srs_signal.h"

#include "rtc_base/logging.h"
#include "rtc_base/string_encode.h"
#include "third_party/jsoncpp/source/include/json/json.h"

namespace strtc {
constexpr int kDefaultSignalTimeoutMs = 5000;
constexpr int kDefaultSignalConnTimeoutMs = 5000;
constexpr char SRS_BASE_URL_PUBLISH[] = "/rtc/v1/publish/";
constexpr char SRS_BASE_URL_SUBSCRIBE[] = "/rtc/v1/play/";

StrtcSrsSignal::StrtcSrsSignal() { RTC_LOG(LS_INFO) << __FUNCTION__; }

StrtcSrsSignal::~StrtcSrsSignal() { RTC_LOG(LS_INFO) << __FUNCTION__; }

// url: "webrtc://172.16.28.35:1985/live/livestream"
int StrtcSrsSignal::post(const std::string& url, const std::string& offer,
                         std::string* answer, ChannelType type) {
  std::vector<std::string> fields;
  rtc::split(url, '/', &fields);
  if (fields.size() < 3) {
    RTC_LOG(LS_ERROR) << __FUNCTION__ << " url no host";
    return -1;
  }
  std::string http_url =
      "http://" + fields[2] +
      (type == ChannelType::PUBLISH ? SRS_BASE_URL_PUBLISH
                                    : SRS_BASE_URL_SUBSCRIBE);

  std::vector<std::string> tokens;
  rtc::split(url, '?', &tokens);
  if (tokens.size() >= 2) {
    http_url += "?" + tokens[1];
  }
#if 1
  http_client_.reset(new HttpClient(http_url, kDefaultSignalTimeoutMs,
                                    kDefaultSignalConnTimeoutMs));
  if (!http_client_) {
    RTC_LOG(LS_ERROR) << __FUNCTION__ << " http client is nullptr";
    return -1;
  }

  Json::StreamWriterBuilder writer_builder;
  writer_builder["commentStyle"] = "None";
  writer_builder["indentation"] = "";

  Json::Value body;
  body["api"] = http_url;
  body["sdp"] = offer;
  body["streamurl"] = url;

  std::string request_str = Json::writeString(writer_builder, body);

  http_client_->AddHeader("Content-Type", "application/json");
  // http_->AddHeader("RequestId", request_id_);
  http_client_->AddContent(true, "", request_str);
  int code = http_client_->DoEasy();
  if (code != 0) {
    RTC_LOG(LS_ERROR) << __FUNCTION__ << " request failed code:" << code;
    return -1;
  }

  Json::CharReaderBuilder reader_builder;
  std::unique_ptr<Json::CharReader> reader(reader_builder.newCharReader());
  std::string json_err;

  body.clear();
  std::string content = http_client_->GetContent();
  if (!reader->parse(content.c_str(), content.c_str() + content.length(), &body,
                     &json_err) ||
      !body.isObject()) {
    RTC_LOG(LS_ERROR) << __FUNCTION__
                      << " response parse failed:" << json_err.c_str()
                      << " content:" << content;
    return -1;
  }

  writer_builder["commentStyle"] = "All";
  writer_builder["indentation"] = "";

  code = body["code"].asInt64();
  if (code == 0) {
    std::string _answer = body["sdp"].asString();
    *answer = _answer;
  } else {
    RTC_LOG(LS_ERROR) << __FUNCTION__ << " srs occur failed code:" << code;
  }
#else
  // WHIP
  http_url = "http://172.16.28.35:1985/rtc/v1/whip/?app=live&stream=livestream";
  http_client_.reset(new HttpClient(http_url, kDefaultSignalTimeoutMs,
                                    kDefaultSignalConnTimeoutMs));
  if (!http_client_) {
    RTC_LOG(LS_ERROR) << __FUNCTION__ << " http client is nullptr";
    return -1;
  }

  http_client_->AddHeader("Content-Type", "application/sdp");
  // http_->AddHeader("RequestId", request_id_);
  http_client_->AddContent(true, "", offer);
  int code = http_client_->DoEasy();
  if (code != 0) {
    RTC_LOG(LS_ERROR) << __FUNCTION__ << " request failed code:" << code;
    return -1;
  }
  std::string content = http_client_->GetContent();
  int status_code = http_client_->GetHttpStatusCode();
  if (status_code == 201) {
    *answer = content;
  }
#endif
  http_client_.reset(nullptr);
  return code;
}
}  // namespace strtc