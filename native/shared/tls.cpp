#include "shared/tls.h"

#include <openssl/err.h>
#include <openssl/x509v3.h>
#include <arpa/inet.h>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <stdexcept>

namespace helion::tls {
namespace {
using Clock = std::chrono::steady_clock;
std::runtime_error error(const std::string& message) {
  const auto code = ERR_get_error();
  char detail[256]{};
  if (code) ERR_error_string_n(code, detail, sizeof(detail));
  return std::runtime_error(message + (code ? ": " + std::string(detail) : ""));
}
Context context() {
  Context ctx(SSL_CTX_new(TLS_method()), SSL_CTX_free);
  if (!ctx || SSL_CTX_set_min_proto_version(ctx.get(), TLS1_2_VERSION) != 1)
    throw error("cannot initialize TLS");
  SSL_CTX_set_options(ctx.get(), SSL_OP_NO_COMPRESSION | SSL_OP_NO_RENEGOTIATION);
  if (SSL_CTX_set_cipher_list(ctx.get(), "ECDHE+AESGCM:ECDHE+CHACHA20") != 1)
    throw error("cannot configure TLS ciphers");
  // Do not accept application data until certificate validation has completed.
  SSL_CTX_set_max_early_data(ctx.get(), 0);
  return ctx;
}
bool waitFor(int fd, short event, Clock::time_point deadline) {
  while (true) {
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
    if (remaining <= 0) { errno = ETIMEDOUT; return false; }
    pollfd ready{fd, event, 0};
    const int result = poll(&ready, 1, static_cast<int>(remaining));
    if (result < 0 && errno == EINTR) continue;
    if (result <= 0) { if (!result) errno = ETIMEDOUT; return false; }
    return true; // A hangup is consumed by the next TLS call.
  }
}
short needed(int result) { return result == SSL_ERROR_WANT_WRITE ? POLLOUT : POLLIN; }
bool retryable(int result) { return result == SSL_ERROR_WANT_READ || result == SSL_ERROR_WANT_WRITE; }
} // namespace

Context serverContext(const std::string& certificate, const std::string& key) {
  if (certificate.empty() || key.empty()) throw std::runtime_error("TLS requires --cert and --key");
  auto ctx = context();
  if (SSL_CTX_use_certificate_chain_file(ctx.get(), certificate.c_str()) != 1 ||
      SSL_CTX_use_PrivateKey_file(ctx.get(), key.c_str(), SSL_FILETYPE_PEM) != 1 ||
      SSL_CTX_check_private_key(ctx.get()) != 1) throw error("cannot load TLS certificate/private key");
  return ctx;
}
Context clientContext(const std::string& caFile) {
  auto ctx = context();
  SSL_CTX_set_verify(ctx.get(), SSL_VERIFY_PEER, nullptr);
  const int loaded = caFile.empty() ? SSL_CTX_set_default_verify_paths(ctx.get()) :
    SSL_CTX_load_verify_locations(ctx.get(), caFile.c_str(), nullptr);
  if (loaded != 1) throw error("cannot load TLS trust roots");
  return ctx;
}
Connection::Connection(SSL_CTX* context, int fd) : fd_(fd) {
  const int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
    throw std::runtime_error("cannot configure TLS socket");
  ssl_ = SSL_new(context);
  if (!ssl_ || SSL_set_fd(ssl_, fd) != 1) {
    SSL_free(ssl_); ssl_ = nullptr; throw error("cannot create TLS connection");
  }
}
Connection::~Connection() { SSL_free(ssl_); }
void Connection::handshake(bool server, const std::string& host, int timeoutMs) {
  if (!server) {
    if (host.empty()) throw std::runtime_error("TLS requires a server identity");
    unsigned char address[16];
    auto* parameters = SSL_get0_param(ssl_);
    X509_VERIFY_PARAM_set_hostflags(parameters, X509_CHECK_FLAG_NEVER_CHECK_SUBJECT | X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
    if (inet_pton(AF_INET, host.c_str(), address) == 1 || inet_pton(AF_INET6, host.c_str(), address) == 1) {
      if (X509_VERIFY_PARAM_set1_ip_asc(parameters, host.c_str()) != 1) throw error("cannot set TLS IP identity");
    } else if (SSL_set1_host(ssl_, host.c_str()) != 1 || SSL_set_tlsext_host_name(ssl_, host.c_str()) != 1) {
      throw error("cannot set TLS hostname");
    }
  }
  const auto deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);
  while (true) {
    ERR_clear_error();
    const int result = server ? SSL_accept(ssl_) : SSL_connect(ssl_);
    if (result == 1) return;
    const int reason = SSL_get_error(ssl_, result);
    if (!retryable(reason)) throw error("TLS handshake or certificate verification failed");
    if (!waitFor(fd_, needed(reason), deadline)) throw std::runtime_error("TLS handshake timed out or disconnected");
  }
}
ssize_t Connection::read(void* bytes, std::size_t size) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!size) return 0;
  if (available_ == 0) {
    if (failed_) { errno = EIO; return -1; }
    // OpenSSL requires an unfinished write to be retried with identical data.
    if (!pendingWrite_.empty()) { errno = EAGAIN; return -1; }
    ERR_clear_error();
    const int result = SSL_read(ssl_, input_.data(), static_cast<int>(input_.size()));
    if (result <= 0) {
      const int reason = SSL_get_error(ssl_, result);
      if (retryable(reason)) {
        readWantsWrite_ = reason == SSL_ERROR_WANT_WRITE;
        readEvent_ = needed(reason); errno = EAGAIN; return -1;
      }
      failed_ = true;
      if (reason == SSL_ERROR_ZERO_RETURN) return 0;
      errno = EIO; return -1;
    }
    readWantsWrite_ = false; readEvent_ = POLLIN;
    available_ = static_cast<std::size_t>(result); offset_ = 0;
  }
  const auto count = std::min(size, available_);
  std::memcpy(bytes, input_.data() + offset_, count);
  offset_ += count; available_ -= count;
  return static_cast<ssize_t>(count);
}
ssize_t Connection::write(const void* bytes, std::size_t size) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (failed_) { errno = EIO; return -1; }
  if (!size) return 0;
  if (readWantsWrite_) { errno = EAGAIN; return -1; }
  if (pendingWrite_.empty()) pendingWrite_.assign(static_cast<const char*>(bytes), std::min<std::size_t>(size, 16384));
  else if (size < pendingWrite_.size() || std::memcmp(bytes, pendingWrite_.data(), pendingWrite_.size()) != 0) {
    failed_ = true; errno = EINVAL; return -1;
  }
  ERR_clear_error();
  const int result = SSL_write(ssl_, pendingWrite_.data(), static_cast<int>(pendingWrite_.size()));
  if (result > 0) { pendingWrite_.clear(); writeEvent_ = POLLOUT; return result; }
  const int reason = SSL_get_error(ssl_, result);
  if (retryable(reason)) { writeEvent_ = needed(reason); errno = EAGAIN; return -1; }
  failed_ = true; errno = EIO; return -1;
}
bool Connection::sendAll(const std::string& bytes, int timeoutMs) {
  const auto deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);
  std::size_t sent = 0;
  while (sent < bytes.size()) {
    const auto count = write(bytes.data() + sent, bytes.size() - sent);
    if (count > 0) { sent += static_cast<std::size_t>(count); continue; }
    if (errno != EAGAIN) return false;
    short event;
    { std::lock_guard<std::mutex> lock(mutex_); event = writeEvent_ ? writeEvent_ : POLLOUT; }
    if (!waitFor(fd_, event, deadline)) return false;
  }
  return true;
}
ssize_t Connection::receive(void* bytes, std::size_t size, int timeoutMs) {
  const auto deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);
  while (true) {
    const auto count = read(bytes, size);
    if (count >= 0 || errno != EAGAIN) return count;
    short event;
    { std::lock_guard<std::mutex> lock(mutex_); event = readEvent_ ? readEvent_ : POLLIN; }
    // Check periodically: another thread can finish a pending write while we wait.
    const auto slice = std::min(deadline, Clock::now() + std::chrono::milliseconds(50));
    if (!waitFor(fd_, event, slice) && Clock::now() >= deadline) return -1;
  }
}
void Connection::closeNotify() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!failed_ && pendingWrite_.empty()) { ERR_clear_error(); SSL_shutdown(ssl_); }
}
} // namespace helion::tls
