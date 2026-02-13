# ============================================
# Stage 1: Build the C binary
# ============================================
FROM ubuntu:22.04 AS c-builder

RUN apt-get update && apt-get install -y \
    build-essential \
    libpcre2-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY src/ src/
COPY include/ include/
COPY patterns/ patterns/
COPY Makefile .

RUN make

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
FROM ubuntu:22.04 AS runner

# Install Node.js + PCRE2 runtime
RUN apt-get update && apt-get install -y \
    curl \
    libpcre2-8-0 \
    && curl -fsSL https://deb.nodesource.com/setup_20.x | bash - \
    && apt-get install -y nodejs \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy the compiled C binary
COPY --from=c-builder /app/build/bin/plumbr ./build/bin/plumbr
RUN chmod +x ./build/bin/plumbr

# Copy pattern files
COPY --from=c-builder /app/patterns/ ./patterns/

# Copy the Next.js production build
COPY --from=web-builder /app/web/.next/standalone ./web/
COPY --from=web-builder /app/web/.next/static ./web/.next/static
COPY --from=web-builder /app/web/public ./web/public

# Create data directory for SQLite
RUN mkdir -p /app/data

ENV NODE_ENV=production
ENV PORT=3000
ENV HOSTNAME=0.0.0.0
ENV DB_PATH=/app/data/plumbr.db
ENV JWT_SECRET=change-this-in-production

EXPOSE 3000

# Data volume for SQLite persistence
VOLUME ["/app/data"]

WORKDIR /app/web
CMD ["node", "server.js"]

