# HLS proxy

## Build and run

We assume docker is up and running to launch the server.

To build `docker build -t server .`

To run : `docker run --rm -it -p 8080:8080 server <URL_to_initial_m3u8>`

Example :
```
docker run --rm -it -p 8080:8080 server http://bitdash-a.akamaihd.net/content/MI201109210084_1/m3u8s/f08e80da-bf1d-4e3d-8899-f0f6155f6efa.m3u8
```

This will run the server on url 0.0.0.0:8080 (can be changed in DockerFile to use a parameter instead).
The path to the m3u8 file should be appended to this URL.

Example :
```
http://0.0.0.0:8080/content/MI201109210084_1/m3u8s/f08e80da-bf1d-4e3d-8899-f0f6155f6efa.m3u8
```

To stop the server, from another bash session, run : `docker ps -q | xargs docker kill`

## What it does

Given a HLS stream, our server will be a proxy to this server and will print some useful information about 
where we are in the HLS protocol.

It tells how long a request took.
It adds information whether it is a Manifest file or a Segment file.

## How does it work

It is mostly based on the boost beast library to create the server and client, which is very low level.

## Limitations

Unfortunately, due to the complexity to implement this project in C++, I did not have time to implement the following:

- Support for global URI in manifests
- Support for an easy URL. For now, you need to enter the same path to the m3u8 file than the one on the server we miror.
- Support for audio manifest meta data : We cannot print that it is a manifest.
- No support for https

No unit test has been implemented unlike a production ready server, due to the lack of time and difficulty of the project in C++.
Similarly it has been tested on very limited number of streams.