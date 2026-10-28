FROM ubuntu:22.04
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential python3 flex bison \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /project
COPY . .
RUN make -j2 && make test
CMD ["make", "bench"]
