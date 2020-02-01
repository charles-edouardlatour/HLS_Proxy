#include "http_client.h"
#include "http_server.h"

#include <chrono>
#include <iostream>
#include <regex>
#include <string>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include <boost/algorithm/string.hpp>

struct ParsedURI {
  std::string protocol;
  std::string domain;  // only domain must be present
  std::string port;
  std::string resource;
  std::string query;   // everything after '?', possibly nothing
};

ParsedURI parseURI(const std::string& url) {
  ParsedURI result;
  auto value_or = [](const std::string& value, std::string&& deflt) -> std::string {
    return (value.empty() ? deflt : value);
  };
  // Note: only "http", "https", "ws", and "wss" protocols are supported
  static const std::regex PARSE_URL{ R"((([httpsw]{2,5})://)?([^/ :]+)(:(\d+))?(/([^ ?]+)?)?/?\??([^/ ]+\=[^/ ]+)?)", 
                                     std::regex_constants::ECMAScript | std::regex_constants::icase };
  std::smatch match;
  if (std::regex_match(url, match, PARSE_URL) && match.size() == 9) {
    result.protocol = value_or(boost::algorithm::to_lower_copy(std::string(match[2])), "http");
    result.domain   = match[3];  
    const bool is_sequre_protocol = (result.protocol == "https" || result.protocol == "wss");
    result.port     = value_or(match[5], (is_sequre_protocol)? "443" : "80");
    result.resource = value_or(match[6], "/");
    result.query = match[8];
    assert(!result.domain.empty());
  }
  return result;
}

/// This handler is the main handler for this project.
/// It is meant to forward the HLS request to its initial server and send it back to the client.
/// In the meantime, some logs are added to the strandard output to give some information
/// about where we are in the protocol.
class RequestHandler {
private:
    HttpClient _client;
    std::unordered_map<std::string, std::string> localToRemotePath;
    std::unordered_set<std::string> manifestList;
    std::unordered_set<std::string> newTrackLocalPath;

    std::string globalFilesLocation;

public:
    RequestHandler(const HttpClient& client,
                   const std::string& localManifestPath,
                   const std::string& remoteManifestPath):
            _client(client),
            localToRemotePath{{localManifestPath, remoteManifestPath}},
            manifestList{localManifestPath}
        {
            auto found = remoteManifestPath.find_last_of("/\\");
            globalFilesLocation = remoteManifestPath.substr(0,found);
        }

    http::response<http::dynamic_body> handle(
            http::request<http::string_body>&& req) {
        auto start = std::chrono::steady_clock::now();

        auto target = std::string(req.target());
        std::stringstream fullPath;
        fullPath << req[http::field::host] << target;

        req.set(http::field::host, _client.get_host());
        auto remotePath = localToRemotePath.find(target);
        if(remotePath != localToRemotePath.end()) {
            req.target(remotePath->second);
        }

        http::response<http::dynamic_body> res;

        if(newTrackLocalPath.find(target) != newTrackLocalPath.end()) {
            std::cout << "[TRACK SWITCH]" << std::endl;
        }
        if(manifestList.find(target) != manifestList.end()) {
            res = handleManifest(std::move(req), fullPath.str(), std::cout);
        } else {
            res = handleSegment(std::move(req), fullPath.str(), std::cout);
        }

        auto end = std::chrono::steady_clock::now();

        std::cout << " (" 
                  << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
                  << "ms)"
                  << std::endl;
        return res;
    }

private:
    http::response<http::dynamic_body> handleManifest(http::request<http::string_body>&& req,
                                                      const std::string& fullPath,
                                                      std::ostream& msgStream) {
        msgStream << "[IN][MANIFEST] " << fullPath << std::endl;
        auto res = _client.get(req);
        parseAndUpdateManifest(buffers_to_string(res.body().data()));
        msgStream << "[OUT][MANIFEST] " << fullPath;
        return res;
    }

    http::response<http::dynamic_body> handleSegment(http::request<http::string_body>&& req,
                                                     const std::string& fullPath,
                                                     std::ostream& msgStream) const {
        msgStream << "[IN][SEGMENT] " << fullPath << std::endl;
        auto res = _client.get(req);
        msgStream << "[OUT][SEGMENT] " << fullPath;
        return res;
    }

    // Meant to update the local information about what the manifest information is about
    // TODO : update the localToRemotePath dictionary and the manifest to return accordingly
    // so that client can request the right URL and we can forward it correctly to the 
    // HLS initial server
    // TODO : fix audio manifest not being considered
    std::string parseAndUpdateManifest(const std::string& initialManifest) {
        std::istringstream f(initialManifest);
        std::stringstream modifManifest;
        std::string line;
        bool nextLineManifest = false;
        bool nextLineNewTrack = false;
        while(std::getline(f, line)) {
            auto fullPathLine = globalFilesLocation + "/" + line;
            if(nextLineManifest) {
                manifestList.insert(fullPathLine);
                nextLineManifest = false;
            }
            if(nextLineNewTrack) {
                newTrackLocalPath.insert(fullPathLine);
                nextLineNewTrack = false;
            }
            if(line.rfind("#EXT-X-STREAM-INF:BANDWIDTH=", 0) == 0) {
                nextLineManifest = true;
                nextLineNewTrack = true;
            }
        }
        return modifManifest.str();
    }
};

//------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    try
    {
        // Check command line arguments.
        if (argc != 4)
        {
            std::cerr <<
                "Usage: http-server-sync <address> <port> <cdn_server>\n" <<
                "Example:\n" <<
                "    http-server-sync 0.0.0.0 8080 http://cdn_server.com/content/video.m3u8\n";
            return EXIT_FAILURE;
        }
        auto const address = net::ip::make_address(argv[1]);
        auto const port = static_cast<unsigned short>(std::atoi(argv[2]));
        auto const cdn_server = parseURI(argv[3]);

        HttpClient cdn_client(cdn_server.domain, cdn_server.port);
        RequestHandler handler(cdn_client, cdn_server.resource, cdn_server.resource);
        HttpServer<RequestHandler> server(handler, address, port);

        server.run();

    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}