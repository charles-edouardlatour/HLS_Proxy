FROM ubuntu:focal
RUN apt-get update && apt-get upgrade -y
RUN apt-get install -y --no-install-recommends \
    cmake \
    build-essential \
    libboost-all-dev \
    libssl-dev

WORKDIR /root/server
ADD . /root/server
RUN g++ --std=c++11 cdn_proxy.cpp http_client.cpp -o cdn_proxy \
    -pthread -lboost_system
ENTRYPOINT ["/root/server/cdn_proxy", "0.0.0.0", "8080"]