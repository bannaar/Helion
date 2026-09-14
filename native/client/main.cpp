#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <mutex>
#include <netdb.h>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <unistd.h>

namespace {
constexpr std::size_t kMaxLine = 4096;

bool readLine(int fd, std::string& line) {
  line.clear();
  char ch = '\0';
  while (line.size() < kMaxLine) {
    const ssize_t n = recv(fd, &ch, 1, 0);
    if (n <= 0) return false;
    if (ch == '\n') return true;
    line.push_back(ch);
  }
  return false;
}

bool sendLine(int fd, const std::string& line) {
  const std::string data = line + "\n";
  std::size_t sent = 0;
  while (sent < data.size()) {
    const ssize_t n = send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
    if (n <= 0) return false;
    sent += static_cast<std::size_t>(n);
  }
  return true;
}

int connectTo(const std::string& host, const std::string& port) {
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo* result = nullptr;
  if (getaddrinfo(host.c_str(), port.c_str(), &hints, &result) != 0) return -1;
  int fd = -1;
  for (addrinfo* item = result; item != nullptr; item = item->ai_next) {
    fd = socket(item->ai_family, item->ai_socktype, item->ai_protocol);
    if (fd >= 0 && connect(fd, item->ai_addr, item->ai_addrlen) == 0) break;
    if (fd >= 0) { close(fd); fd = -1; }
  }
  freeaddrinfo(result);
  return fd;
}

// The renderer intentionally uses only the OpenGL 1.x fixed-function API.
void rectangle(float left, float top, float right, float bottom,
               float red, float green, float blue) {
  glColor3f(red, green, blue);
  glBegin(GL_QUADS);
  glVertex2f(left, top); glVertex2f(right, top);
  glVertex2f(right, bottom); glVertex2f(left, bottom);
  glEnd();
}

void render(int width, int height, const std::vector<std::string>& log, bool connected) {
  glViewport(0, 0, width, height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, width, height, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glClearColor(0.025f, 0.04f, 0.08f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  rectangle(0, 0, static_cast<float>(width), 72, 0.08f, 0.14f, 0.25f);
  rectangle(24, 24, 300, 48, connected ? 0.12f : 0.35f, connected ? 0.65f : 0.18f, 0.4f);
  rectangle(24, 92, static_cast<float>(width - 24), static_cast<float>(height - 82),
            0.06f, 0.09f, 0.16f);
  const float rowHeight = 22.0f;
  const std::size_t start = log.size() > 10 ? log.size() - 10 : 0;
  for (std::size_t i = start; i < log.size(); ++i) {
    const float y = 112.0f + static_cast<float>(i - start) * rowHeight;
    rectangle(42, y, static_cast<float>(width - 42), y + 2, 0.18f, 0.38f, 0.58f);
  }
  rectangle(24, static_cast<float>(height - 56), static_cast<float>(width - 24),
            static_cast<float>(height - 24), 0.10f, 0.16f, 0.27f);
  SDL_GL_SwapWindow(static_cast<SDL_Window*>(SDL_GL_GetCurrentWindow()));
}

int terminalClient(int fd) {
  std::cout << "Connected. Commands: /create, /login, /chat, /profile, /state, /quit\n";
  std::string line;
  for (int i = 0; i < 2 && readLine(fd, line); ++i) std::cout << line << '\n';
  while (std::getline(std::cin, line)) {
    if (line.rfind("/create ", 0) == 0) line.replace(0, 8, "CREATE ");
    else if (line.rfind("/login ", 0) == 0) line.replace(0, 7, "LOGIN ");
    else if (line.rfind("/chat ", 0) == 0) line.replace(0, 6, "CHAT ");
    else if (line == "/profile") line = "PROFILE";
    else if (line == "/state") line = "STATE";
    else if (line == "/quit") line = "QUIT";
    else { std::cout << "Use /create, /login, /chat, /profile, /state, or /quit\n"; continue; }
    if (!sendLine(fd, line) || line == "QUIT") break;
    timeval timeout{0, 250000};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    while (readLine(fd, line)) std::cout << line << '\n';
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string host = argc > 1 ? argv[1] : "127.0.0.1";
  const std::string port = argc > 2 ? argv[2] : "4242";
  const bool terminal = argc > 3 && std::string(argv[3]) == "--terminal";
  const int fd = connectTo(host, port);
  if (fd < 0) {
    std::cerr << "unable to connect to " << host << ':' << port << ": "
              << std::strerror(errno) << '\n';
    return 1;
  }
  if (terminal) {
    const int result = terminalClient(fd);
    shutdown(fd, SHUT_RDWR); close(fd);
    return result;
  }

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::cerr << "SDL initialization failed: " << SDL_GetError() << '\n';
    close(fd);
    return 1;
  }
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_Window* window = SDL_CreateWindow("Helion Native Client",
      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 960, 600,
      SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  SDL_GLContext context = window ? SDL_GL_CreateContext(window) : nullptr;
  if (!window || !context) {
    std::cerr << "OpenGL window creation failed: " << SDL_GetError() << '\n';
    if (context) SDL_GL_DeleteContext(context);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit(); close(fd); return 1;
  }
  SDL_GL_SetSwapInterval(1);
  std::vector<std::string> log{"Connected to " + host + ":" + port,
                               "OpenGL renderer: fixed-function 2.1-compatible"};
  bool running = true;
  bool connected = true;
  std::string incoming;
  std::string typed;
  SDL_StartTextInput();
  while (running) {
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) running = false;
      if (event.type == SDL_TEXTINPUT) typed += event.text.text;
      if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_BACKSPACE && !typed.empty()) typed.pop_back();
        if (event.key.keysym.sym == SDLK_RETURN && !typed.empty()) {
          std::string request = typed;
          if (request.rfind("/", 0) == 0) {
            if (request.rfind("/create ", 0) == 0) request.replace(0, 8, "CREATE ");
            else if (request.rfind("/login ", 0) == 0) request.replace(0, 7, "LOGIN ");
            else if (request.rfind("/chat ", 0) == 0) request.replace(0, 6, "CHAT ");
            else if (request == "/profile") request = "PROFILE";
            else if (request == "/state") request = "STATE";
            else if (request == "/quit") running = false;
          }
          if (running && sendLine(fd, request)) log.push_back("> " + typed);
          typed.clear();
        }
      }
    }
    char ch;
    while (recv(fd, &ch, 1, MSG_DONTWAIT) > 0) {
      if (ch == '\n') { log.push_back(incoming); incoming.clear(); }
      else if (incoming.size() < kMaxLine) incoming.push_back(ch);
    }
    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    render(width, height, log, connected);
    SDL_Delay(16);
  }
  SDL_StopTextInput();
  sendLine(fd, "QUIT");
  shutdown(fd, SHUT_RDWR); close(fd);
  SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
  return 0;
}
