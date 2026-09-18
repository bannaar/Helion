#pragma once

#include <openssl/ssl.h>
#include <array>
#include <memory>
#include <mutex>
#include <string>
#include <sys/types.h>

namespace helion::tls {
using Context = std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)>;
Context serverContext(const std::string& certificate, const std::string& key);
Context clientContext(const std::string& caFile);

// Owns SSL state, but not the socket. TLS calls on a connection are serialized.
// Nonblocking I/O returns -1/EAGAIN when its next TLS operation needs readiness.
class Connection {
 public:
  Connection(SSL_CTX* context, int fd);
  ~Connection();
  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;
  void handshake(bool server, const std::string& host = {}, int timeoutMs = 5000);
  ssize_t read(void* bytes, std::size_t size);
  ssize_t write(const void* bytes, std::size_t size);
  bool sendAll(const std::string& bytes, int timeoutMs = 5000);
  ssize_t receive(void* bytes, std::size_t size, int timeoutMs = 30000);
  void closeNotify();
  int fd() const { return fd_; }
 private:
  SSL* ssl_ = nullptr;
  int fd_;
  std::mutex mutex_;
  std::array<char, 16384> input_{};
  std::size_t available_ = 0, offset_ = 0;
  std::string pendingWrite_;
  bool readWantsWrite_ = false;
  short readEvent_ = 0, writeEvent_ = 0;
  bool failed_ = false;
};
} // namespace helion::tls
