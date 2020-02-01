#ifndef HTTP_CLIENT
#define HTTP_CLIENT

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <string>

namespace http = boost::beast::http;    // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>

/// Client to send http requests.
class HttpClient
{
public:
    HttpClient(const std::string& host, const std::string& port);
    ~HttpClient();
    http::response<http::dynamic_body> get(http::request<http::string_body>& req) const;

    std::string get_host() const {
        return _host;
    }

private:
    std::string _host;
    std::string _port;
};

#endif // HTTP_CLIENT