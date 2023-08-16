#ifndef STRTC_HTTP_CLIENT_H_
#define STRTC_HTTP_CLIENT_H_

#include <string>

struct curl_slist;

namespace strtc {
class HttpClient {
 public:
  HttpClient(const std::string& url, int timeout, int conn_timeout_ms);
  ~HttpClient();

  static void Init();
  static void Unit();

  void AddHeader(const std::string& name, const std::string& value);
  void AddContent(bool post, const std::string& form_post,
                  const std::string& post_field);
  void SetSharedHandler();
  int DoEasy();
  std::string GetContent();
  long GetHttpStatusCode();

 private:
  static size_t WriteMemory(void* data, size_t size, size_t count, void* param);

 private:
  std::string url_;
  std::string content_;
  int content_bytes_ = 0;
  void* curl_handle_ = nullptr;
  curl_slist* curl_list_ = nullptr;
};
}  // namespace strtc
#endif  // STRTC_HTTP_CLIENT_H_