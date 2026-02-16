#include "io/http_server.hpp"

#include <array>
#include <atomic>
#include <cctype>
#include <csignal>
#include <cstdint>
#include <condition_variable>
#include <cstring>
#include <fstream>
#include <mutex>
#include <queue>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace crosside::io
{
    namespace
    {

        // ===== CONSTANTS =====
        constexpr std::size_t kMaxHeaderSize = 64 * 1024;
        constexpr std::size_t kRecvBufferSize = 4 * 1024;
        constexpr std::size_t kSendChunkSize = 16 * 1024;
        constexpr int kListenBacklog = 128;
        constexpr int kSocketTimeoutSeconds = 30;

#ifdef _WIN32
        using SocketHandle = SOCKET;
        constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
        using SocketHandle = int;
        constexpr SocketHandle kInvalidSocket = -1;
#endif

        // Global flag for graceful shutdown
        std::atomic<bool> g_serverRunning{true};

        void closeSocket(SocketHandle handle)
        {
            if (handle == kInvalidSocket)
            {
                return;
            }
#ifdef _WIN32
            closesocket(handle);
#else
            close(handle);
#endif
        }

        std::string socketErrorText()
        {
#ifdef _WIN32
            return std::to_string(WSAGetLastError());
#else
            return std::strerror(errno);
#endif
        }

        bool setSocketTimeout(SocketHandle sock, int seconds)
        {
#ifdef _WIN32
            DWORD timeout = seconds * 1000;
            return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
                              reinterpret_cast<const char *>(&timeout), sizeof(timeout)) == 0;
#else
            struct timeval tv;
            tv.tv_sec = seconds;
            tv.tv_usec = 0;
            return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0;
#endif
        }

        bool sendAll(SocketHandle sock, const char *data, std::size_t size)
        {
            std::size_t sent = 0;
            while (sent < size)
            {
                // Limitar tamanho por envio para evitar overflow de int
                const std::size_t toSend = std::min(
                    size - sent,
                    static_cast<std::size_t>(INT_MAX));

                const int n = send(
                    sock,
                    data + sent,
                    static_cast<int>(toSend),
                    0);

                if (n < 0)
                {
#ifdef _WIN32
                    int err = WSAGetLastError();
                    if (err == WSAEWOULDBLOCK || err == WSAEINTR)
                    {
                        continue; // Erro temporário, tentar novamente
                    }
#else
                    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                    {
                        continue; // Erro temporário, tentar novamente
                    }
#endif
                    return false; // Erro real
                }

                if (n == 0)
                {
                    return false; // Conexão fechada
                }

                sent += static_cast<std::size_t>(n);
            }
            return true;
        }

        std::string toLower(std::string value)
        {
            for (char &ch : value)
            {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            return value;
        }

        int hexValue(char c)
        {
            if (c >= '0' && c <= '9')
            {
                return c - '0';
            }
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (c >= 'a' && c <= 'f')
            {
                return 10 + (c - 'a');
            }
            return -1;
        }

        std::string urlDecode(const std::string &raw)
        {
            std::string out;
            out.reserve(raw.size());

            for (std::size_t i = 0; i < raw.size(); ++i)
            {
                const char ch = raw[i];
                if (ch == '%' && i + 2 < raw.size())
                {
                    const int hi = hexValue(raw[i + 1]);
                    const int lo = hexValue(raw[i + 2]);
                    if (hi >= 0 && lo >= 0)
                    {
                        out.push_back(static_cast<char>((hi << 4) | lo));
                        i += 2;
                        continue;
                    }
                }
                if (ch == '+')
                {
                    out.push_back(' ');
                    continue;
                }
                out.push_back(ch);
            }
            return out;
        }

        std::string detectMimeType(const fs::path &path)
        {
            static const std::unordered_map<std::string, std::string> kTable = {
                {".html", "text/html; charset=utf-8"},
                {".htm", "text/html; charset=utf-8"},
                {".js", "application/javascript; charset=utf-8"},
                {".mjs", "application/javascript; charset=utf-8"},
                {".css", "text/css; charset=utf-8"},
                {".json", "application/json; charset=utf-8"},
                {".txt", "text/plain; charset=utf-8"},
                {".xml", "application/xml; charset=utf-8"},
                {".wasm", "application/wasm"},
                {".data", "application/octet-stream"},
                {".bin", "application/octet-stream"},
                {".png", "image/png"},
                {".jpg", "image/jpeg"},
                {".jpeg", "image/jpeg"},
                {".gif", "image/gif"},
                {".webp", "image/webp"},
                {".svg", "image/svg+xml"},
                {".ico", "image/x-icon"},
                {".wav", "audio/wav"},
                {".ogg", "audio/ogg"},
                {".mp3", "audio/mpeg"},
                {".mp4", "video/mp4"},
            };

            const std::string ext = toLower(path.extension().string());
            auto it = kTable.find(ext);
            if (it != kTable.end())
            {
                return it->second;
            }
            return "application/octet-stream";
        }

        std::string statusText(int code)
        {
            switch (code)
            {
            case 200:
                return "OK";
            case 400:
                return "Bad Request";
            case 403:
                return "Forbidden";
            case 404:
                return "Not Found";
            case 405:
                return "Method Not Allowed";
            case 500:
                return "Internal Server Error";
            default:
                return "Error";
            }
        }

        bool sendSimpleResponse(
            SocketHandle sock,
            int statusCode,
            const std::string &body,
            const std::string &contentType,
            bool headOnly)
        {
            std::string header = "HTTP/1.1 " + std::to_string(statusCode) + " " + statusText(statusCode) + "\r\n";
            header += "Content-Type: " + contentType + "\r\n";
            header += "Content-Length: " + std::to_string(body.size()) + "\r\n";
            header += "Connection: close\r\n";
            if (statusCode == 405)
            {
                header += "Allow: GET, HEAD\r\n";
            }
            header += "\r\n";

            if (!sendAll(sock, header.data(), header.size()))
            {
                return false;
            }
            if (!headOnly && !body.empty())
            {
                return sendAll(sock, body.data(), body.size());
            }
            return true;
        }

        bool parseRequestLine(
            const std::string &requestHeader,
            std::string &methodOut,
            std::string &targetOut)
        {
            const std::size_t firstEnd = requestHeader.find("\r\n");
            if (firstEnd == std::string::npos)
            {
                return false;
            }

            const std::string firstLine = requestHeader.substr(0, firstEnd);
            const std::size_t p1 = firstLine.find(' ');
            if (p1 == std::string::npos)
            {
                return false;
            }
            const std::size_t p2 = firstLine.find(' ', p1 + 1);
            if (p2 == std::string::npos)
            {
                return false;
            }

            methodOut = firstLine.substr(0, p1);
            targetOut = firstLine.substr(p1 + 1, p2 - (p1 + 1));
            return !methodOut.empty() && !targetOut.empty();
        }

        bool sanitizeRequestPath(
            const std::string &rawTarget,
            const std::string &indexFile,
            fs::path &relativeOut)
        {
            std::string target = rawTarget;

            // Remove query string e fragment
            const std::size_t q = target.find('?');
            if (q != std::string::npos)
            {
                target = target.substr(0, q);
            }
            const std::size_t h = target.find('#');
            if (h != std::string::npos)
            {
                target = target.substr(0, h);
            }

            if (target.empty())
            {
                target = "/";
            }

            // URL decode
            std::string decoded = urlDecode(target);

            // Normalizar separadores
            for (char &ch : decoded)
            {
                if (ch == '\\')
                {
                    ch = '/';
                }
            }

            // Remover / inicial
            if (!decoded.empty() && decoded.front() == '/')
            {
                decoded.erase(decoded.begin());
            }

            // Parse path components com proteção contra path traversal
            fs::path rel;
            std::size_t depth = 0;
            std::size_t begin = 0;

            while (begin <= decoded.size())
            {
                std::size_t end = decoded.find('/', begin);
                if (end == std::string::npos)
                {
                    end = decoded.size();
                }

                const std::string token = decoded.substr(begin, end - begin);
                begin = end + 1;

                if (token.empty() || token == ".")
                {
                    if (end == decoded.size())
                    {
                        break;
                    }
                    continue;
                }

                // PROTEÇÃO: Rejeitar ".." e variações encodadas
                if (token == "..")
                {
                    return false;
                }

                // PROTEÇÃO: Rejeitar null bytes
                if (token.find('\0') != std::string::npos)
                {
                    return false;
                }

                // PROTEÇÃO: Rejeitar caracteres de controle
                for (char ch : token)
                {
                    if (ch < 32 || ch == 127)
                    {
                        return false;
                    }
                }

                rel /= token;
                ++depth;

                if (end == decoded.size())
                {
                    break;
                }
            }

            if (rel.empty())
            {
                rel = indexFile;
            }

            relativeOut = rel;
            return true;
        }

        bool isPathSafe(const fs::path &filePath, const fs::path &serveRoot)
        {
            std::error_code ec;

            // Canonicalizar ambos os paths
            fs::path canonicalFile = fs::canonical(filePath, ec);
            if (ec)
            {
                return false; // Arquivo não existe ou erro
            }

            fs::path canonicalRoot = fs::canonical(serveRoot, ec);
            if (ec)
            {
                return false; // Root inválido
            }

            // Verificar se canonicalFile está dentro de canonicalRoot
            // Comparar componentes do path
            auto fileIt = canonicalFile.begin();
            auto rootIt = canonicalRoot.begin();

            while (rootIt != canonicalRoot.end())
            {
                if (fileIt == canonicalFile.end() || *fileIt != *rootIt)
                {
                    return false; // File está fora do root
                }
                ++fileIt;
                ++rootIt;
            }

            return true; // File está dentro do root
        }

        bool sendFileResponse(
            SocketHandle sock,
            const fs::path &filePath,
            const fs::path &serveRoot,
            bool headOnly)
        {
            // PROTEÇÃO: Verificar que o path está dentro do serveRoot
            if (!isPathSafe(filePath, serveRoot))
            {
                return sendSimpleResponse(sock, 403, "Forbidden\n", "text/plain; charset=utf-8", headOnly);
            }

            // Abrir arquivo (RAII protege contra TOCTOU)
            std::ifstream in(filePath, std::ios::binary);
            if (!in.is_open())
            {
                return sendSimpleResponse(sock, 404, "Not found\n", "text/plain; charset=utf-8", headOnly);
            }

            // Obter tamanho do arquivo
            std::error_code ec;
            const std::uintmax_t size = fs::file_size(filePath, ec);
            if (ec)
            {
                return sendSimpleResponse(sock, 500, "Failed to read file size\n", "text/plain; charset=utf-8", headOnly);
            }

            // Enviar headers
            std::string header = "HTTP/1.1 200 OK\r\n";
            header += "Content-Type: " + detectMimeType(filePath) + "\r\n";
            header += "Content-Length: " + std::to_string(size) + "\r\n";
            header += "Connection: close\r\n";
            header += "Cache-Control: no-cache\r\n";
            header += "\r\n";

            if (!sendAll(sock, header.data(), header.size()))
            {
                return false;
            }

            if (headOnly)
            {
                return true;
            }

            // Enviar conteúdo do arquivo
            std::array<char, kSendChunkSize> chunk{};
            while (in.good())
            {
                in.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
                const std::streamsize got = in.gcount();
                if (got <= 0)
                {
                    break;
                }
                if (!sendAll(sock, chunk.data(), static_cast<std::size_t>(got)))
                {
                    return false;
                }
            }
            return true;
        }

        bool handleClient(
            SocketHandle client,
            const fs::path &serveRoot,
            const std::string &indexFile)
        {
            // Configurar timeout no socket
            setSocketTimeout(client, kSocketTimeoutSeconds);

            std::string request;
            request.reserve(kRecvBufferSize);

            std::array<char, kRecvBufferSize> buffer{};

            // Ler headers com proteção contra headers muito grandes
            for (;;)
            {
                // PROTEÇÃO: Verificar tamanho ANTES de recv
                if (request.size() >= kMaxHeaderSize)
                {
                    return sendSimpleResponse(client, 400, "Header too large\n", "text/plain; charset=utf-8", false);
                }

                // Limitar quanto podemos ler
                const std::size_t canRead = std::min(
                    buffer.size(),
                    kMaxHeaderSize - request.size());

                const int got = recv(client, buffer.data(), static_cast<int>(canRead), 0);
                if (got <= 0)
                {
                    return false; // Erro ou conexão fechada
                }

                request.append(buffer.data(), static_cast<std::size_t>(got));

                if (request.find("\r\n\r\n") != std::string::npos)
                {
                    break; // Headers completos
                }
            }

            // Parse request line
            std::string method;
            std::string target;
            if (!parseRequestLine(request, method, target))
            {
                return sendSimpleResponse(client, 400, "Bad request\n", "text/plain; charset=utf-8", false);
            }

            // Verificar método
            const bool headOnly = method == "HEAD";
            if (!(method == "GET" || headOnly))
            {
                return sendSimpleResponse(client, 405, "Only GET/HEAD supported\n", "text/plain; charset=utf-8", headOnly);
            }

            // Sanitizar path (proteção contra path traversal)
            fs::path rel;
            if (!sanitizeRequestPath(target, indexFile, rel))
            {
                return sendSimpleResponse(client, 403, "Forbidden\n", "text/plain; charset=utf-8", headOnly);
            }

            // Construir path completo
            fs::path filePath = serveRoot / rel;

            // Se for diretório, adicionar index file
            std::error_code ec;
            if (fs::is_directory(filePath, ec))
            {
                filePath /= indexFile;
            }

            // Verificar se arquivo existe e é regular
            if (!fs::exists(filePath, ec) || !fs::is_regular_file(filePath, ec))
            {
                return sendSimpleResponse(client, 404, "Not found\n", "text/plain; charset=utf-8", headOnly);
            }

            // Servir arquivo (com proteção contra symlinks)
            return sendFileResponse(client, filePath, serveRoot, headOnly);
        }

        class SocketRuntime
        {
        public:
            explicit SocketRuntime(const crosside::Context &ctx) : ok_(true)
            {
#ifdef _WIN32
                WSADATA data{};
                if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
                {
                    ok_ = false;
                    ctx.error("WSAStartup failed");
                }
#else
                (void)ctx;
#endif
            }

            ~SocketRuntime()
            {
#ifdef _WIN32
                if (ok_)
                {
                    WSACleanup();
                }
#endif
            }

            bool ok() const { return ok_; }

        private:
            bool ok_ = false;
        };

        std::size_t computeWorkerCount()
        {
            unsigned int cpu = std::thread::hardware_concurrency();
            if (cpu == 0)
            {
                return 4;
            }
            if (cpu < 2)
            {
                return 2;
            }
            if (cpu > 8)
            {
                return 8;
            }
            return static_cast<std::size_t>(cpu);
        }

        class ClientWorkerPool
        {
        public:
            ClientWorkerPool(std::size_t workerCount, fs::path root, std::string indexFile)
                : root_(std::move(root)), indexFile_(std::move(indexFile))
            {
                workers_.reserve(workerCount);
                for (std::size_t i = 0; i < workerCount; ++i)
                {
                    workers_.emplace_back([this]()
                                          { workerLoop(); });
                }
            }

            ~ClientWorkerPool()
            {
                shutdown();
            }

            void enqueue(SocketHandle client)
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (stopping_)
                {
                    closeSocket(client);
                    return;
                }
                queue_.push(client);
                cv_.notify_one();
            }

            void shutdown()
            {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    if (stopping_)
                    {
                        return;
                    }
                    stopping_ = true;

                    // Fechar todos os sockets pendentes
                    while (!queue_.empty())
                    {
                        closeSocket(queue_.front());
                        queue_.pop();
                    }

                    // CORREÇÃO: Notify DENTRO do lock
                    cv_.notify_all();
                }

                // Aguardar workers terminarem
                for (auto &worker : workers_)
                {
                    if (worker.joinable())
                    {
                        worker.join();
                    }
                }
                workers_.clear();
            }

        private:
            void workerLoop()
            {
                for (;;)
                {
                    SocketHandle client = kInvalidSocket;
                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        cv_.wait(lock, [this]()
                                 { return stopping_ || !queue_.empty(); });

                        if (stopping_ && queue_.empty())
                        {
                            return;
                        }

                        client = queue_.front();
                        queue_.pop();
                    }

                    if (client != kInvalidSocket)
                    {
                        (void)handleClient(client, root_, indexFile_);
                        closeSocket(client);
                    }
                }
            }

            fs::path root_;
            std::string indexFile_;
            std::mutex mutex_;
            std::condition_variable cv_;
            std::queue<SocketHandle> queue_;
            std::vector<std::thread> workers_;
            bool stopping_ = false;
        };

    } // namespace

    bool serveStaticHttp(const crosside::Context &ctx, const StaticHttpServerOptions &options)
    {
        if (options.port <= 0 || options.port > 65535)
        {
            ctx.error("Invalid HTTP server port: ", options.port);
            return false;
        }

        std::error_code ec;
        const fs::path root = fs::absolute(options.root, ec);
        if (ec || !fs::exists(root, ec) || !fs::is_directory(root, ec))
        {
            ctx.error("Invalid HTTP server root: ", options.root.string());
            return false;
        }

        SocketRuntime runtime(ctx);
        if (!runtime.ok())
        {
            return false;
        }

        SocketHandle server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (server == kInvalidSocket)
        {
            ctx.error("Failed create socket: ", socketErrorText());
            return false;
        }

        int reuse = 1;
#ifdef _WIN32
        setsockopt(server, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&reuse), sizeof(reuse));
#else
        setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<unsigned short>(options.port));

        const std::string host = options.host.empty() ? "127.0.0.1" : options.host;
        if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1)
        {
            ctx.error("Invalid HTTP server host: ", host);
            closeSocket(server);
            return false;
        }

        if (bind(server, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0)
        {
            ctx.error("Failed bind ", host, ":", options.port, " : ", socketErrorText());
            closeSocket(server);
            return false;
        }

        if (listen(server, kListenBacklog) != 0)
        {
            ctx.error("Failed listen on ", host, ":", options.port, " : ", socketErrorText());
            closeSocket(server);
            return false;
        }

        const std::string indexFile = options.indexFile.empty() ? std::string("index.html") : options.indexFile;
        const std::size_t workerCount = computeWorkerCount();
        ClientWorkerPool workers(workerCount, root, indexFile);

        ctx.log("HTTP server listening on http://", host, ":", options.port, "/");
        ctx.log("Serve root: ", root.string());
        ctx.log("Worker threads: ", workerCount);
        ctx.log("Press Ctrl+C to stop.");

        // Reset global flag
        g_serverRunning = true;

        // CORREÇÃO: Loop com condição de saída
        while (g_serverRunning)
        {
            // Usar select com timeout para verificar periodicamente g_serverRunning
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(server, &readfds);

            struct timeval tv;
            tv.tv_sec = 1;
            tv.tv_usec = 0;

#ifdef _WIN32
            int selectResult = select(0, &readfds, nullptr, nullptr, &tv);
#else
            int selectResult = select(server + 1, &readfds, nullptr, nullptr, &tv);
#endif

            if (selectResult < 0)
            {
                if (!g_serverRunning)
                {
                    break; // Sinal recebido
                }
                ctx.warn("Select failed: ", socketErrorText());
                continue;
            }

            if (selectResult == 0)
            {
                // Timeout - verificar flag novamente
                continue;
            }

            // Accept connection
            sockaddr_in clientAddr{};
#ifdef _WIN32
            int addrLen = sizeof(clientAddr);
#else
            socklen_t addrLen = sizeof(clientAddr);
#endif
            SocketHandle client = accept(server, reinterpret_cast<sockaddr *>(&clientAddr), &addrLen);

            if (client == kInvalidSocket)
            {
                if (!g_serverRunning)
                {
                    break;
                }
                ctx.warn("Accept failed: ", socketErrorText());
                continue;
            }

            workers.enqueue(client);
        }

        ctx.log("Shutting down HTTP server...");
        workers.shutdown();
        closeSocket(server);
        ctx.log("HTTP server stopped.");

        return true;
    }

    void stopHttpServer()
    {
        g_serverRunning = false;
    }

    bool isHttpPortAvailable(const crosside::Context &ctx, const std::string &hostInput, int port)
    {
        if (port <= 0 || port > 65535)
        {
            return false;
        }

        SocketRuntime runtime(ctx);
        if (!runtime.ok())
        {
            return false;
        }

        SocketHandle server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (server == kInvalidSocket)
        {
            return false;
        }

        int reuse = 1;
#ifdef _WIN32
        setsockopt(server, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&reuse), sizeof(reuse));
#else
        setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<unsigned short>(port));

        const std::string host = hostInput.empty() ? "127.0.0.1" : hostInput;
        if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1)
        {
            closeSocket(server);
            return false;
        }

        const bool available = bind(server, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0;
        closeSocket(server);
        return available;
    }

    std::string detectHttpMimeType(const fs::path &path)
    {
        return detectMimeType(path);
    }

    bool sanitizeHttpRelativePath(
        const std::string &rawTarget,
        const std::string &indexFile,
        fs::path &relativeOut)
    {
        return sanitizeRequestPath(rawTarget, indexFile, relativeOut);
    }

    bool isHttpPathSafe(const fs::path &filePath, const fs::path &serveRoot)
    {
        return isPathSafe(filePath, serveRoot);
    }

} // namespace crosside::io
