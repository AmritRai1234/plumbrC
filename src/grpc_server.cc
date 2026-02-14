/*
 * PlumbrC gRPC Server
 * High-performance bidirectional streaming redaction
 *
 * Links against libplumbr.a for the core redaction engine.
 * Each gRPC thread gets its own libplumbr_t instance (zero contention).
 *
 * Build: make grpc
 * Usage: plumbr-grpc [--port 50051] [--threads 4] [--pattern-dir DIR]
 */

#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include "plumbr.grpc.pb.h"
#include "plumbr.pb.h"

extern "C" {
#include "libplumbr.h"
}

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReaderWriter;
using grpc::Status;
using plumbr::v1::HealthRequest;
using plumbr::v1::HealthResponse;
using plumbr::v1::PlumbrService;
using plumbr::v1::RedactBatchRequest;
using plumbr::v1::RedactBatchResponse;
using plumbr::v1::RedactRequest;
using plumbr::v1::RedactResponse;

/* ─── Per-thread plumbr instance ─────────────────────── */

static libplumbr_config_t g_config = {};
static std::chrono::steady_clock::time_point g_start_time;

static thread_local libplumbr_t *tl_plumbr = nullptr;

static libplumbr_t *get_plumbr() {
  if (!tl_plumbr) {
    tl_plumbr = libplumbr_new(&g_config);
    if (!tl_plumbr) {
      std::cerr << "[grpc] FATAL: Failed to create plumbr instance\n";
    }
  }
  return tl_plumbr;
}

/* ─── Helper: count [REDACTED:...] patterns ──────────── */

static int count_patterns(const char *text, size_t len) {
  int count = 0;
  for (size_t i = 0; i + 10 < len; i++) {
    if (text[i] == '[' && std::strncmp(text + i + 1, "REDACTED:", 9) == 0) {
      count++;
    }
  }
  return count;
}

/* ─── Service implementation ─────────────────────────── */

class PlumbrServiceImpl final : public PlumbrService::Service {

  /* Unary: single line redaction */
  Status Redact(ServerContext *ctx, const RedactRequest *req,
                RedactResponse *resp) override {
    (void)ctx;
    auto t0 = std::chrono::steady_clock::now();

    libplumbr_t *p = get_plumbr();
    if (!p) {
      return Status(grpc::INTERNAL, "Failed to init plumbr");
    }

    size_t out_len = 0;
    char *redacted =
        libplumbr_redact(p, req->text().data(), req->text().size(), &out_len);

    if (redacted) {
      resp->set_redacted(redacted, out_len);
      resp->set_patterns_matched(count_patterns(redacted, out_len));
      free(redacted);
    } else {
      resp->set_redacted(req->text());
      resp->set_patterns_matched(0);
    }

    auto t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    resp->set_processing_time_ms(ms);

    return Status::OK;
  }

  /* Batch: multiple lines in one call */
  Status RedactBatch(ServerContext *ctx, const RedactBatchRequest *req,
                     RedactBatchResponse *resp) override {
    (void)ctx;
    auto t0 = std::chrono::steady_clock::now();

    libplumbr_t *p = get_plumbr();
    if (!p) {
      return Status(grpc::INTERNAL, "Failed to init plumbr");
    }

    int total_modified = 0;

    for (const auto &text : req->texts()) {
      size_t out_len = 0;
      char *redacted = libplumbr_redact(p, text.data(), text.size(), &out_len);

      auto *result = resp->add_results();
      if (redacted) {
        result->set_redacted(redacted, out_len);
        int pm = count_patterns(redacted, out_len);
        result->set_patterns_matched(pm);
        if (out_len != text.size() ||
            std::memcmp(redacted, text.data(), text.size()) != 0) {
          total_modified++;
        }
        free(redacted);
      } else {
        result->set_redacted(text);
        result->set_patterns_matched(0);
      }
    }

    auto t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    resp->set_total_lines(req->texts_size());
    resp->set_total_modified(total_modified);
    resp->set_processing_time_ms(ms);

    return Status::OK;
  }

  /* Bidirectional streaming: the throughput monster */
  Status RedactStream(
      ServerContext *ctx,
      ServerReaderWriter<RedactResponse, RedactRequest> *stream) override {
    (void)ctx;

    libplumbr_t *p = get_plumbr();
    if (!p) {
      return Status(grpc::INTERNAL, "Failed to init plumbr");
    }

    RedactRequest req;
    while (stream->Read(&req)) {
      auto t0 = std::chrono::steady_clock::now();

      size_t out_len = 0;
      char *redacted =
          libplumbr_redact(p, req.text().data(), req.text().size(), &out_len);

      RedactResponse resp;
      if (redacted) {
        resp.set_redacted(redacted, out_len);
        resp.set_patterns_matched(count_patterns(redacted, out_len));
        free(redacted);
      } else {
        resp.set_redacted(req.text());
        resp.set_patterns_matched(0);
      }

      auto t1 = std::chrono::steady_clock::now();
      double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
      resp.set_processing_time_ms(ms);

      stream->Write(resp);
    }

    return Status::OK;
  }

  /* Health check */
  Status Health(ServerContext *ctx, const HealthRequest *req,
                HealthResponse *resp) override {
    (void)ctx;
    (void)req;

    resp->set_status("healthy");
    resp->set_version(libplumbr_version());

    libplumbr_t *p = get_plumbr();
    if (p) {
      resp->set_patterns_loaded(
          static_cast<int64_t>(libplumbr_pattern_count(p)));
    }

    auto now = std::chrono::steady_clock::now();
    double uptime = std::chrono::duration<double>(now - g_start_time).count();
    resp->set_uptime_seconds(uptime);

    return Status::OK;
  }
};

/* ─── Main ───────────────────────────────────────────── */

static void print_usage(const char *prog) {
  std::cerr << "PlumbrC gRPC Server\n\n"
            << "Usage: " << prog << " [OPTIONS]\n\n"
            << "Options:\n"
            << "  --port PORT          Listen port (default: 50051)\n"
            << "  --threads N          gRPC threads (default: 4)\n"
            << "  --pattern-dir DIR    Load patterns from directory\n"
            << "  --pattern-file FILE  Load patterns from file\n"
            << "  --help               Show this help\n";
}

int main(int argc, char *argv[]) {
  int port = 50051;
  int num_threads = 4;
  const char *pattern_dir = nullptr;
  const char *pattern_file = nullptr;

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--port" && i + 1 < argc) {
      port = std::stoi(argv[++i]);
    } else if (arg == "--threads" && i + 1 < argc) {
      num_threads = std::stoi(argv[++i]);
    } else if (arg == "--pattern-dir" && i + 1 < argc) {
      pattern_dir = argv[++i];
    } else if (arg == "--pattern-file" && i + 1 < argc) {
      pattern_file = argv[++i];
    } else if (arg == "--help") {
      print_usage(argv[0]);
      return 0;
    }
  }

  /* Configure libplumbr */
  g_config.pattern_file = pattern_file;
  g_config.pattern_dir = pattern_dir;
  g_config.num_threads = 1;
  g_config.quiet = 1;

  g_start_time = std::chrono::steady_clock::now();

  /* Verify plumbr works */
  libplumbr_t *test = libplumbr_new(&g_config);
  if (!test) {
    std::cerr << "FATAL: Failed to create plumbr instance\n";
    return 1;
  }
  size_t pattern_count = libplumbr_pattern_count(test);
  libplumbr_free(test);

  /* Build gRPC server */
  std::string addr = "0.0.0.0:" + std::to_string(port);

  PlumbrServiceImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  /* Set thread pool size */
  builder.SetSyncServerOption(ServerBuilder::SyncServerOption::NUM_CQS,
                              num_threads);
  builder.SetSyncServerOption(ServerBuilder::SyncServerOption::MIN_POLLERS,
                              num_threads);
  builder.SetSyncServerOption(ServerBuilder::SyncServerOption::MAX_POLLERS,
                              num_threads * 2);

  /* Performance options */
  builder.SetMaxReceiveMessageSize(2 * 1024 * 1024); /* 2MB */
  builder.SetMaxSendMessageSize(2 * 1024 * 1024);

  std::unique_ptr<Server> server(builder.BuildAndStart());
  if (!server) {
    std::cerr << "FATAL: Failed to start gRPC server on " << addr << "\n";
    return 1;
  }

  std::cerr << "\n"
            << "╔══════════════════════════════════════════╗\n"
            << "║     PlumbrC gRPC Server                  ║\n"
            << "╠══════════════════════════════════════════╣\n"
            << "║  Port:     " << port << "                          ║\n"
            << "║  Threads:  " << num_threads
            << "                               ║\n"
            << "║  Patterns: " << pattern_count
            << "                             ║\n"
            << "╚══════════════════════════════════════════╝\n\n"
            << "Listening on " << addr << "\n"
            << "Press Ctrl+C to stop\n\n";

  server->Wait();

  return 0;
}
