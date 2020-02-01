#ifndef HTTP_SERVER
#define HTTP_SERVER

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/config.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>


// Runs a http server where the request is handled by the Handler
template <class Handler>
class HttpServer {
private:
    Handler _handler;
    const net::ip::address& _host;
    unsigned short _port;

public:
    HttpServer(const Handler& handler,const net::ip::address& host, unsigned short port)
        : _handler(handler), _host(host), _port(port) {}

    ~HttpServer() {}

    void run() {
        // The io_context is required for all I/O
        net::io_context ioc{1};

        // The acceptor receives incoming connections
        tcp::acceptor acceptor{ioc, {_host, _port}};
        for(;;)
        {
            // This will receive the new connection
            tcp::socket socket{ioc};

            // Block until we get a connection
            acceptor.accept(socket);

            // Launch the session, transferring ownership of the socket
            std::thread{std::bind(
                &HttpServer<Handler>::do_session,
                this,
                std::move(socket))}.detach();
        }
    }

private:
    // Report a failure
    void fail(beast::error_code ec, char const* what) {
        std::cerr << what << ": " << ec.message() << "\n";
    }

    // This is the C++11 equivalent of a generic lambda.
    // The function object is used to send an HTTP message.
    template<class Stream> 
    struct ReponseSender {
        Stream& stream_;
        bool& close_;
        beast::error_code& ec_;

        explicit ReponseSender(Stream& stream, bool& close, beast::error_code& ec)
            : stream_(stream), close_(close), ec_(ec) {}

        template<bool isRequest, class Body, class Fields> 
        void operator()(http::message<isRequest, Body, Fields>&& msg) const
        {
            // Determine if we should close the connection after
            close_ = msg.need_eof();

            // We need the serializer here because the serializer requires
            // a non-const file_body, and the message oriented version of
            // http::write only works with const messages.
            http::serializer<isRequest, Body, Fields> sr{msg};
            http::write(stream_, sr, ec_);
        }
    };

    // Handles an HTTP server connection
    void do_session(tcp::socket& socket)
    {
        bool close = false;
        beast::error_code ec;

        // This buffer is required to persist across reads
        beast::flat_buffer buffer;

        // This lambda is used to send messages
        ReponseSender<tcp::socket> response_sender{socket, close, ec};

        for(;;)
        {
            // Read a request
            http::request<http::string_body> req;
            http::read(socket, buffer, req, ec);
            if(ec == http::error::end_of_stream)
                break;
            if(ec)
                return fail(ec, "read");

            // Send the response
            auto res = _handler.handle(std::move(req));
            response_sender(std::move(res));
            if(ec)
                return fail(ec, "write");
            if(close)
            {
                // This means we should close the connection, usually because
                // the response indicated the "Connection: close" semantic.
                break;
            }
        }

        // Send a TCP shutdown
        socket.shutdown(tcp::socket::shutdown_send, ec);

        // At this point the connection is closed gracefully
    }

};

#endif // HTTP_SERVER