# ============================================
# Stage 1: Build the C binaries
# ============================================
FROM alpine:3.19 AS c-builder

RUN apk add --no-cache build-base pcre2-dev linux-headers

WORKDIR /app
COPY src/ src/
COPY include/ include/
COPY patterns/ patterns/
COPY Makefile .

# Build CLI + HTTP server without -march=native (not portable in containers)
RUN make OPT_FLAGS="-O3 -march=x86-64 -flto -fomit-frame-pointer -fno-plt -ffunction-sections -fdata-sections" && \
    make server OPT_FLAGS="-O3 -march=x86-64 -flto -fomit-frame-pointer -fno-plt -ffunction-sections -fdata-sections"

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

# Install PCRE2 runtime + libc compatibility
RUN apk add --no-cache pcre2 libstdc++

WORKDIR /app

# Copy the compiled C binaries
COPY --from=c-builder /app/build/bin/plumbr ./build/bin/plumbr
COPY --from=c-builder /app/build/bin/plumbr-server ./build/bin/plumbr-server
RUN chmod +x ./build/bin/plumbr ./build/bin/plumbr-server

# Copy pattern files
COPY --from=c-builder /app/patterns/ ./patterns/

# Copy the Next.js production build
COPY --from=web-builder /app/web/.next/standalone ./web/
COPY --from=web-builder /app/web/.next/static ./web/.next/static
COPY --from=web-builder /app/web/public ./web/public

# Create data directory for SQLite
RUN mkdir -p /app/data

# Startup script that runs both C server + Next.js
COPY start.sh /app/start.sh
RUN chmod +x /app/start.sh

ENV NODE_ENV=production
ENV PORT=3000
ENV HOSTNAME=0.0.0.0
ENV DB_PATH=/app/data/plumbr.db
ENV JWT_SECRET=change-this-in-production
ENV PLUMBR_THREADS=4

EXPOSE 3000 8081

# Data volume for SQLite persistence
VOLUME ["/app/data"]

CMD ["/app/start.sh"]
