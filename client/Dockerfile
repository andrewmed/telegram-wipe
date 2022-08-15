FROM debian:testing-backports

RUN set -ex; \
    apt-get update && apt-get install -y --no-install-recommends \
    libc++1-11

RUN mkdir -p /app
COPY build/client /app

WORKDIR /app
CMD /app/client
