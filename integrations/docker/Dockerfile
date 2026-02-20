# PlumbrC Docker Image (standalone)
FROM alpine:3.21 AS builder

RUN apk add --no-cache \
    gcc \
    musl-dev \
    make \
    pcre2-dev \
    linux-headers

WORKDIR /build
COPY . .
RUN make clean && \
    make OPT_FLAGS="-O3 -march=x86-64 -flto -fomit-frame-pointer -ffunction-sections -fdata-sections" && \
    make server OPT_FLAGS="-O3 -march=x86-64 -flto -fomit-frame-pointer -ffunction-sections -fdata-sections" && \
    make shared

# Runtime image
FROM alpine:3.21

RUN apk add --no-cache pcre2

COPY --from=builder /build/build/bin/plumbr /usr/local/bin/plumbr
COPY --from=builder /build/build/bin/plumbr-server /usr/local/bin/plumbr-server
COPY --from=builder /build/build/lib/libplumbr.so /usr/local/lib/libplumbr.so
COPY --from=builder /build/patterns /etc/plumbr/patterns

# Default: CLI mode with built-in patterns
ENTRYPOINT ["/usr/local/bin/plumbr"]
