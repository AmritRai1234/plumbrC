# ============================================
# Stage 1: Build the C binaries
# ============================================
FROM alpine:3.21 AS c-builder

RUN apk add --no-cache build-base pcre2-dev linux-headers grpc-dev protobuf-dev

WORKDIR /app
COPY src/ src/
COPY include/ include/
COPY patterns/ patterns/
COPY proto/ proto/
COPY Makefile .

# Build CLI + HTTP server + gRPC server
RUN make OPT_FLAGS="-O3 -march=x86-64 -flto -fomit-frame-pointer -fno-plt -ffunction-sections -fdata-sections" && \
    make server OPT_FLAGS="-O3 -march=x86-64 -flto -fomit-frame-pointer -fno-plt -ffunction-sections -fdata-sections" && \
    make grpc

# ============================================
# Stage 2: Build the Next.js app
# ============================================
FROM node:20-alpine AS web-builder

# Install build tools for native modules (better-sqlite3)
RUN apk add --no-cache python3 make g++

WORKDIR /app/web
COPY web/package.json web/package-lock.json ./
RUN npm ci

COPY web/ .

# Need the C binary available during build for any build-time checks
ENV NEXT_TELEMETRY_DISABLED=1
RUN npm run build

# ============================================
# Stage 3: Production runtime
# ============================================
FROM node:20-alpine AS runner

# Install PCRE2 runtime + libc compatibility + nginx
# Note: gRPC shared libs are copied from builder to guarantee ABI match
RUN apk add --no-cache pcre2 libstdc++ nginx libgcc

WORKDIR /app

# Copy the compiled C binaries
COPY --from=c-builder /app/build/bin/plumbr ./build/bin/plumbr
COPY --from=c-builder /app/build/bin/plumbr-server ./build/bin/plumbr-server
COPY --from=c-builder /app/build/bin/plumbr-grpc ./build/bin/plumbr-grpc
RUN chmod +x ./build/bin/plumbr ./build/bin/plumbr-server ./build/bin/plumbr-grpc

# Copy gRPC/protobuf shared libraries from builder to guarantee exact ABI match
COPY --from=c-builder /usr/lib/libgrpc*.so* /usr/lib/
COPY --from=c-builder /usr/lib/libgpr.so* /usr/lib/
COPY --from=c-builder /usr/lib/libprotobuf.so* /usr/lib/
COPY --from=c-builder /usr/lib/libupb*.so* /usr/lib/
COPY --from=c-builder /usr/lib/libaddress_sorting.so* /usr/lib/
COPY --from=c-builder /usr/lib/libre2.so* /usr/lib/
COPY --from=c-builder /usr/lib/libabsl*.so* /usr/lib/
RUN ldconfig /usr/lib 2>/dev/null || true

# Copy pattern files
COPY --from=c-builder /app/patterns/ ./patterns/

# Copy the Next.js production build
COPY --from=web-builder /app/web/.next/standalone ./web/
COPY --from=web-builder /app/web/.next/static ./web/.next/static
COPY --from=web-builder /app/web/public ./web/public

# Nginx reverse proxy config (routes /api/redact → C server directly)
COPY nginx.conf /etc/nginx/nginx.conf
RUN mkdir -p /var/log/nginx /run/nginx

# Create data directory for SQLite
RUN mkdir -p /app/data

# Startup script: nginx + C server + Next.js
COPY start.sh /app/start.sh
RUN chmod +x /app/start.sh

ENV NODE_ENV=production
ENV PORT=3001
ENV HOSTNAME=0.0.0.0
ENV DB_PATH=/app/data/plumbr.db
ENV JWT_SECRET=change-this-in-production
ENV PLUMBR_THREADS=4

# nginx on :3000 (public), C server on :8081, gRPC on :50051, Next.js on :3001 (internal)
EXPOSE 3000 50051

# Data volume for SQLite persistence
VOLUME ["/app/data"]

CMD ["/app/start.sh"]
