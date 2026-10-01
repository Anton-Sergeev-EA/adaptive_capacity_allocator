# Minimal C++ environment: Ubuntu 16.04 + gcc + text editors, builds a console "Hello world".
FROM ubuntu:16.04

RUN apt-get update \
 && apt-get install -y --no-install-recommends g++ make nano vim \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY hello.cpp .
RUN g++ -Wall -O2 -o hello hello.cpp

CMD ["./hello"]
