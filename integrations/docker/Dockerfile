# PlumbrC Docker Image
FROM alpine:3.19 AS builder

RUN apk add --no-cache \
    gcc \
    musl-dev \
    make \
    pcre2-dev \
    linux-headers

WORKDIR /build
COPY . .
RUN make clean && make

# Runtime image
FROM alpine:3.19

RUN apk add --no-cache pcre2

COPY --from=builder /build/build/bin/plumbr /usr/local/bin/plumbr
COPY --from=builder /build/patterns /etc/plumbr/patterns

# Default: use built-in patterns
ENTRYPOINT ["/usr/local/bin/plumbr"]
