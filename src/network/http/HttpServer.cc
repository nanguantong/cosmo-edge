// HttpServer — Http Server implementation.

#include "network/http/HttpServer.h"

#include <event2/keyvalq_struct.h>
#include <event2/util.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>

#include "event2/http.h"
#include "network/http/HttpCommon.h"
#include "network/http/MultipartStreamParser.h"
#include "util/CipherUtil.h"
#include "util/FileUtil.h"
#include "util/JsonStructUtil.h"
#include "util/Log.h"
#include "util/UuidUtil.h"

namespace chrono = std::chrono;

namespace cosmo::network::http {

namespace {

    constexpr auto kShutdownDrainTimeout = chrono::seconds(5);

    const char* FindHeader(struct evkeyvalq* headers, const char* name) {
        if (headers == nullptr) {
            return nullptr;
        }
        return evhttp_find_header(headers, name);
    }

}  // namespace

HttpServer::~HttpServer() {
    UnInitialize();
}

void HttpServer::ComGenCb(struct evhttp_request* req, void* arg) {
    if (!req) {
        LOG_WARN("{}", "ComGenCb Get Null Request");
        return;
    }
    auto* http_server = static_cast<HttpServer*>(arg);
    if (http_server == nullptr) {
        return;
    }
    if (!http_server->is_accepting_requests_) {
        http_server->SendImmediateError(req, HttpResponseCode::kServiceUnavailable);
        return;
    }

    auto uri = evhttp_request_get_uri(req);
    if (uri == nullptr) {
        http_server->SendImmediateError(req, HttpResponseCode::kBadRequest);
        return;
    }
    if (strncmp(uri, "/v1/cwai/aihost/", strlen("/v1/cwai/aihost/")) == 0) {
        http_server->InsertHttpReqMsg(req);
    } else if (strncmp(uri, "/logs/", strlen("/logs/")) == 0) {
        http_server->InsertHttpOctetMsg(req);
    } else {
        http_server->InsertHttpReqMsg(req);
    }
}

void HttpServer::RequestCompleteCb(struct evhttp_request* req, void* arg) {
    auto* context = static_cast<RequestContext*>(arg);
    if (context == nullptr) {
        return;
    }
    auto* server = context->server;
    auto token   = context->token;
    server->CompleteRequest(token, req);
}

void HttpServer::ConnectionCloseCb(struct evhttp_connection* connection, void* arg) {
    auto* context = static_cast<RequestContext*>(arg);
    if (context == nullptr) {
        return;
    }
    auto* server = context->server;
    auto token   = context->token;
    server->CloseRequest(token, connection);
}

void HttpServer::InsertHttpReqMsg(struct evhttp_request* req) {
    InsertHttpMsg(req, InnerMsgId::kHttpReq);
}

void HttpServer::InsertHttpOctetMsg(struct evhttp_request* req) {
    InsertHttpMsg(req, InnerMsgId::kHttpOctetReq);
}

void HttpServer::InsertHttpMsg(struct evhttp_request* req, InnerMsgId msg_id) {
    auto task = BuildHttpReqTask(req);
    if (!task) {
        SendImmediateError(req, HttpResponseCode::kBadRequest);
        return;
    }

    auto token = RegisterRequest(req);
    if (token == kInvalidHttpRequestToken) {
        if (task->has_tmp_path) {
            cosmo::util::RemovePath(task->tmp_file_path);
        }
        SendImmediateError(req, HttpResponseCode::kInternalError);
        return;
    }
    task->request_token = token;

    auto tmp_file_path = task->tmp_file_path;
    auto has_tmp_path  = task->has_tmp_path;
    cosmo::MsgEnvelope msg(static_cast<int>(msg_id), std::move(task));
    if (thread_pool_.PutMsg(std::move(msg)) < 0) {
        if (has_tmp_path) {
            cosmo::util::RemovePath(tmp_file_path);
        }
        HttpAckTask response_task(static_cast<int>(HttpResponseCode::kServiceUnavailable), token, "{}", "");
        DispatchJsonMsg(&response_task);
        return;
    }
    LOG_INFO("Put request {} token {} to HTTP pool", static_cast<int>(msg_id), token);
}

std::unique_ptr<HttpReqTask> HttpServer::BuildHttpReqTask(struct evhttp_request* req) {
    auto* uri = evhttp_request_get_uri(req);
    if (uri == nullptr) {
        LOG_WARN("{}", "Cannot get request URI");
        return nullptr;
    }

    auto task          = std::make_unique<HttpReqTask>();
    task->request_time = chrono::steady_clock::now();
    task->interface    = uri;

    auto* input_headers = evhttp_request_get_input_headers(req);
    if (const auto* forwarded_for = FindHeader(input_headers, "x-forwarded-for")) {
        task->x_forwarded_for = forwarded_for;
    }
    if (const auto* mtk = FindHeader(input_headers, "mtk")) {
        task->mtk = mtk;
    }

    const auto* content_type = FindHeader(input_headers, "Content-Type");
    if (content_type != nullptr && strstr(content_type, "multipart/form-data") != nullptr) {
        if (!ExtractMultipartBody(task.get(), req, content_type)) {
            return nullptr;
        }
        return task;
    }

    auto* input_buffer = evhttp_request_get_input_buffer(req);
    auto body_length   = evbuffer_get_length(input_buffer);
    if (body_length == 0) {
        return task;
    }

    auto* body_data = evbuffer_pullup(input_buffer, -1);
    if (body_data == nullptr) {
        LOG_ERRO("Failed to copy request body for URI {}", task->interface);
        return nullptr;
    }
    task->body.resize(body_length);
    std::memcpy(task->body.data(), body_data, body_length);
    return task;
}

bool HttpServer::ExtractMultipartBody(HttpReqTask* task, struct evhttp_request* req,
                                      const std::string& content_type) {
    auto* input_headers = evhttp_request_get_input_headers(req);
    LOG_INFO("Receive uri:{}, handle multipart/form-data", task->interface);
    if (const auto* content_length = FindHeader(input_headers, "Content-Length")) {
        LOG_INFO("Content-Length:{}", content_length);
    }

    auto boundary_start = content_type.find("boundary=");
    if (boundary_start == std::string::npos) {
        LOG_WARN("Receive uri:{}, multipart boundary is missing", task->interface);
        return false;
    }
    boundary_start += strlen("boundary=");
    auto boundary_end = content_type.find(';', boundary_start);
    auto boundary     = content_type.substr(boundary_start, boundary_end - boundary_start);
    if (boundary.size() >= 2 && boundary.front() == '"' && boundary.back() == '"') {
        boundary = boundary.substr(1, boundary.size() - 2);
    }
    if (boundary.empty()) {
        LOG_WARN("Receive uri:{}, multipart boundary is empty", task->interface);
        return false;
    }

    if (!callbacks_.get_upload_tmp_path) {
        LOG_ERRO("{}", "Upload temporary path callback is not configured");
        return false;
    }
    task->tmp_file_path = callbacks_.get_upload_tmp_path();
    task->has_tmp_path  = true;

    MultipartStreamParser parser(std::move(boundary));
    auto result = parser.ParseToFile(evhttp_request_get_input_buffer(req), [&](const std::string& filename) {
        return (std::filesystem::path(task->tmp_file_path) / filename).string();
    });
    if (!result.ok) {
        LOG_ERRO("Parse multipart request failed: {}", result.err);
        cosmo::util::RemovePath(task->tmp_file_path);
        task->has_tmp_path = false;
        task->tmp_file_path.clear();
        return false;
    }
    if (!cosmo::util::EncodeJson(result.fields, task->body)) {
        LOG_ERRO("{}", "Encode multipart fields failed");
        cosmo::util::RemovePath(task->tmp_file_path);
        task->has_tmp_path = false;
        task->tmp_file_path.clear();
        return false;
    }
    return true;
}

void HttpServer::SetAppInfo(const std::string& app_key, const std::string& app_secret) {
    app_key_    = app_key;
    app_secret_ = app_secret;
}

bool HttpServer::Initialize(const std::string& host_ip, int port, DispatcherFactory factory,
                            HttpServerCallbacks callbacks) {
    {
        std::lock_guard<std::mutex> lock(lifecycle_mutex_);
        if (lifecycle_state_ != LifecycleState::kStopped) {
            LOG_ERRO("{}", "HTTP server is already initialized");
            return false;
        }
    }

    dispatcher_factory_ = std::move(factory);
    callbacks_          = std::move(callbacks);
    if (!(event_base_ = event_base_new())) {
        LOG_ERRO("{}", "event_base_new return NULL");
        return false;
    }

    if (!(event_http_ = evhttp_new(event_base_))) {
        LOG_ERRO("{}", "evhttp_new return NULL");
        CleanupEventResources();
        return false;
    }

    // 5ms tick: ensures max response latency is bounded, while blocking to yield CPU when idle
    if (!tick_event_) {
        tick_event_ = event_new(
            event_base_, -1, EV_PERSIST,
            [](evutil_socket_t /*fd*/, short /*events*/, void* /*arg*/) {
                // Only used to periodically wake the event loop so dispatchMsg() can process the response
                // queue, avoiding busy-waiting.
            },
            this);
        if (!tick_event_) {
            LOG_ERRO("{}", "event_new tick failed");
            CleanupEventResources();
            return false;
        }
        struct timeval tv;
        tv.tv_sec  = 0;
        tv.tv_usec = 5000;  // 5ms
        if (event_add(tick_event_, &tv) != 0) {
            LOG_ERRO("{}", "event_add tick failed");
            event_free(tick_event_);
            tick_event_ = nullptr;
            CleanupEventResources();
            return false;
        }
    }

    // IMPORTANT: libevent caches the entire request body in memory by default.
    // For multipart uploads, allowing oversized bodies can easily trigger OOM.
    // The frontend allows 500MB file uploads (bulk face import zip), plus multipart encoding overhead,
    // so the limit must be >= 500MB. Otherwise libevent closes the connection without HTTP response,
    // causing nginx to return 502 Bad Gateway.
    evhttp_set_max_body_size(event_http_, 600LL * 1024 * 1024);  // 600MB

    // generic cb
    evhttp_set_gencb(event_http_, ComGenCb, this);

    // allow post method
    evhttp_set_allowed_methods(event_http_, EVHTTP_REQ_POST | EVHTTP_REQ_GET | EVHTTP_REQ_HEAD);

    bound_socket_ = evhttp_bind_socket_with_handle(event_http_, host_ip.c_str(), static_cast<uint16_t>(port));
    if (bound_socket_ == nullptr) {
        LOG_ERRO("evhttp_bind_socket ip:[{}], port:[{}] failed", host_ip, port);
        CleanupEventResources();
        return false;
    }

    constexpr int kThreadNum = 4;
    if (!thread_pool_.Initialize(kThreadNum, this, dispatcher_factory_)) {
        CleanupEventResources();
        return false;
    }

    next_request_token_    = 1;
    is_accepting_requests_ = true;
    is_running_            = true;
    {
        std::lock_guard<std::mutex> lock(lifecycle_mutex_);
        lifecycle_state_ = LifecycleState::kInitialized;
    }

    LOG_INFO("libevent http server ip[{}]-port[{}] start success", host_ip, port);

    return true;
}

void HttpServer::UnInitialize() {
    is_accepting_requests_ = false;
    is_running_            = false;

    std::unique_lock<std::mutex> lock(lifecycle_mutex_);
    if (lifecycle_state_ == LifecycleState::kStopped) {
        return;
    }

    if (lifecycle_state_ == LifecycleState::kDispatching || lifecycle_state_ == LifecycleState::kStopping) {
        if (event_thread_id_ == std::this_thread::get_id()) {
            return;
        }
        lifecycle_cv_.wait(lock, [this]() { return lifecycle_state_ == LifecycleState::kStopped; });
        return;
    }

    lifecycle_state_ = LifecycleState::kStopping;
    event_thread_id_ = std::this_thread::get_id();
    lock.unlock();
    ShutdownOnEventThread();
    MarkStopped();
}

void HttpServer::StopAccepting() {
    is_accepting_requests_ = false;
    if (event_http_ != nullptr && bound_socket_ != nullptr) {
        evhttp_del_accept_socket(event_http_, bound_socket_);
        bound_socket_ = nullptr;
    }
}

void HttpServer::ShutdownOnEventThread() {
    StopAccepting();

    // No new request tasks can enter after StopAccepting. Drain every task that
    // was accepted before releasing any request or connection owned by libevent.
    thread_pool_.Uninitialize();
    HandleRespMsgList();

    auto deadline = chrono::steady_clock::now() + kShutdownDrainTimeout;
    while (event_base_ != nullptr && !active_requests_.empty() && chrono::steady_clock::now() < deadline) {
        auto rc = event_base_loop(event_base_, EVLOOP_ONCE);
        if (rc < 0) {
            LOG_ERRO("{}", "event_base_loop failed while draining HTTP responses");
            break;
        }
        HandleRespMsgList();
    }
    if (!active_requests_.empty()) {
        LOG_WARN("Force closing {} HTTP request(s) after shutdown drain timeout", active_requests_.size());
    }

    CleanupEventResources();
    {
        std::lock_guard<std::mutex> lock(msg_mutex_);
        msg_list_.clear();
    }
}

void HttpServer::CleanupEventResources() {
    if (tick_event_ != nullptr) {
        event_free(tick_event_);
        tick_event_ = nullptr;
    }
    if (event_http_ != nullptr) {
        evhttp_free(event_http_);
        event_http_   = nullptr;
        bound_socket_ = nullptr;
    }
    active_requests_.clear();
    if (event_base_ != nullptr) {
        event_base_free(event_base_);
        event_base_ = nullptr;
    }
}

void HttpServer::MarkStopped() {
    {
        std::lock_guard<std::mutex> lock(lifecycle_mutex_);
        lifecycle_state_ = LifecycleState::kStopped;
        event_thread_id_ = std::thread::id{};
    }
    lifecycle_cv_.notify_all();
}

HttpRequestToken HttpServer::RegisterRequest(struct evhttp_request* req) {
    auto* connection = evhttp_request_get_connection(req);
    if (connection == nullptr) {
        LOG_ERRO("{}", "HTTP request has no connection");
        return kInvalidHttpRequestToken;
    }

    auto token = next_request_token_++;
    while (token == kInvalidHttpRequestToken || active_requests_.find(token) != active_requests_.end()) {
        token = next_request_token_++;
    }
    auto context      = std::make_unique<RequestContext>(RequestContext{this, token, req, connection});
    auto* context_ptr = context.get();
    active_requests_.emplace(token, std::move(context));
    evhttp_request_set_on_complete_cb(req, RequestCompleteCb, context_ptr);
    evhttp_connection_set_closecb(connection, ConnectionCloseCb, context_ptr);
    return token;
}

struct evhttp_request* HttpServer::FindRequest(HttpRequestToken token) const {
    auto it = active_requests_.find(token);
    if (it == active_requests_.end()) {
        return nullptr;
    }
    return it->second->request;
}

void HttpServer::CompleteRequest(HttpRequestToken token, const struct evhttp_request* req) {
    auto it = active_requests_.find(token);
    if (it == active_requests_.end() || it->second->request != req) {
        return;
    }
    evhttp_connection_set_closecb(it->second->connection, nullptr, nullptr);
    active_requests_.erase(it);
}

void HttpServer::CloseRequest(HttpRequestToken token, struct evhttp_connection* connection) {
    auto it = active_requests_.find(token);
    if (it == active_requests_.end() || it->second->connection != connection) {
        return;
    }
    evhttp_connection_set_closecb(connection, nullptr, nullptr);
    active_requests_.erase(it);
}

void HttpServer::SendImmediateError(struct evhttp_request* req, HttpResponseCode code) {
    const auto& code_messages = GetHttpResCodeMsg();
    auto code_value           = static_cast<int>(code);
    auto it                   = code_messages.find(code_value);
    const char* message       = it == code_messages.end() ? "ERROR" : it->second.c_str();
    evhttp_send_error(req, code_value, message);
}

int HttpServer::DispatchJsonMsg(HttpAckTask* task) {
    struct evhttp_request* ev_http_req = FindRequest(task->request_token);
    if (ev_http_req == nullptr) {
        LOG_WARN("Drop late HTTP response for token {}", task->request_token);
        return 0;
    }

    if (!AddHttpHeader(ev_http_req, task->request_id)) {
        return 0;
    }

    // NOTE: Cannot reuse a static evbuffer. Otherwise each evbuffer_add accumulates,
    // causing memory growth in high-frequency response scenarios like chunked uploads.
    struct evbuffer* evRes = evbuffer_new();
    if (!evRes) {
        LOG_ERRO("{}", "evbuffer_new failed");
        return 0;
    }
    if (task->response.empty()) {
        LOG_ERRO("{}", "task->response is empty");
    }
    evbuffer_add(evRes, task->response.c_str(), task->response.size());

    auto& codeMsg   = GetHttpResCodeMsg();
    auto it         = codeMsg.find(task->http_ack_code);
    const char* msg = (it != codeMsg.end()) ? it->second.c_str() : "UNKNOWN";
    evhttp_send_reply(ev_http_req, task->http_ack_code, msg, evRes);
    evbuffer_free(evRes);
    return 1;
}

int HttpServer::DispatchFileMsg(HttpAckTask* task) {
    struct evhttp_request* ev_http_req = FindRequest(task->request_token);
    if (ev_http_req == nullptr) {
        LOG_WARN("Drop late HTTP file response for token {}", task->request_token);
        return 0;
    }

    FILE* fp = fopen(task->file_path.c_str(), "rb");
    if (!fp) {
        struct evbuffer* buf = evbuffer_new();
        evbuffer_add_printf(buf, "File not found: %s", task->file_name.c_str());
        evhttp_send_reply(ev_http_req, HTTP_NOTFOUND, "File Not Found", buf);
        evbuffer_free(buf);
        LOG_ERRO("File Path:{} Name:{} Not Found", task->file_path, task->file_name);
        return 0;
    }

    fseek(fp, 0L, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    if (!AddHttpOctetHeader(ev_http_req, task->request_id, task->file_name, fsize)) {
        fclose(fp);
        return 0;
    }

    evhttp_send_reply_start(ev_http_req, HTTP_OK, "OK");

    char buf[8192];
    size_t nread = 0;
    while ((nread = fread(buf, 1, sizeof(buf), fp)) > 0) {
        struct evbuffer* chunk = evbuffer_new();
        evbuffer_add(chunk, buf, nread);
        evhttp_send_reply_chunk(ev_http_req, chunk);
        evbuffer_free(chunk);
    }
    evhttp_send_reply_end(ev_http_req);
    fclose(fp);
    return 1;
}

void HttpServer::DispatchMsg() {
    {
        std::lock_guard<std::mutex> lock(lifecycle_mutex_);
        if (lifecycle_state_ != LifecycleState::kInitialized || event_base_ == nullptr) {
            LOG_ERRO("{}", "DispatchMsg: HTTP server is not initialized");
            return;
        }
        lifecycle_state_ = LifecycleState::kDispatching;
        event_thread_id_ = std::this_thread::get_id();
    }

    while (is_running_) {
        // Block once: woken by network events or 5ms tick
        int rc = event_base_loop(event_base_, EVLOOP_ONCE);
        if (rc < 0) {
            LOG_ERRO("{}", "event_base_loop failed");
            break;
        }

        int handle_cnt = HandleRespMsgList();
        if (handle_cnt < 0) {
            LOG_WARN("{}", "QUIT");
            is_running_ = false;
            break;
        }
    }
    is_running_ = false;

    {
        std::lock_guard<std::mutex> lock(lifecycle_mutex_);
        lifecycle_state_ = LifecycleState::kStopping;
    }
    ShutdownOnEventThread();
    MarkStopped();
}

int HttpServer::Put(cosmo::MsgEnvelope&& msg) {
    std::lock_guard<std::mutex> lck(msg_mutex_);
    msg_list_.push_back(std::move(msg));
    return static_cast<int>(msg_list_.size());
}

int HttpServer::HandleRespMsgList() {
    cosmo::MsgEnvelopeList lst_msg;
    {
        std::lock_guard<std::mutex> lck(msg_mutex_);
        if (msg_list_.empty()) {
            return 0;
        }
        lst_msg.swap(msg_list_);
    }

    int handle_cnt = 0;

    for (auto& it : lst_msg) {
        LOG_INFO("Handle:{}", it.GetMsgId());
        auto acl_task = it.ReleaseData();

        auto task = static_cast<HttpAckTask*>(acl_task.get());
        if (task != nullptr) {
            auto msgId = static_cast<InnerMsgId>(it.GetMsgId());
            switch (msgId) {
                case InnerMsgId::kHttpOctetAck:
                    handle_cnt += DispatchFileMsg(task);
                    break;
                case InnerMsgId::kHttpQuitAck:
                    return -1;
                default:
                    handle_cnt += DispatchJsonMsg(task);
                    break;
            }
        } else {
            LOG_ERRO("{}", "task is nullptr!");
            return 0;
        }
    }

    return handle_cnt;
}

bool HttpServer::AddCommonHeaders(struct evkeyvalq* ev_header, const std::string& req_id) {
    evhttp_add_header(ev_header, "Server", "CWAI");
    evhttp_add_header(ev_header, "RequestId", req_id.c_str());
    evhttp_add_header(ev_header, "AppKey", app_key_.c_str());

    std::string Nonce = cosmo::util::GenerateUUID();
    evhttp_add_header(ev_header, "Nonce", Nonce.c_str());

    auto now      = std::chrono::system_clock::now().time_since_epoch();
    auto cur_time = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

    char cur_time_buf[32] = {0};
    snprintf(cur_time_buf, sizeof(cur_time_buf), "%013ld", cur_time);
    evhttp_add_header(ev_header, "CurTime", cur_time_buf);

    std::string sha_str   = Nonce + app_secret_ + cur_time_buf;
    std::string check_sum = cosmo::util::Sha1(sha_str);
    evhttp_add_header(ev_header, "CheckSum", check_sum.c_str());
    return true;
}

bool HttpServer::AddHttpHeader(struct evhttp_request* ev_http_req, const std::string& req_id) {
    auto* ev_header = evhttp_request_get_output_headers(ev_http_req);
    if (ev_header == nullptr) {
        LOG_ERRO("{}", "ev_header is nullptr!");
        return false;
    }

    if (!AddCommonHeaders(ev_header, req_id)) {
        return false;
    }

    evhttp_add_header(ev_header, "Content-Type", "application/json");
    evhttp_add_header(ev_header, "Connection", "close");
    return true;
}

bool HttpServer::AddHttpOctetHeader(struct evhttp_request* ev_http_req, const std::string& req_id,
                                    const std::string& file_name, long fsize) {
    auto* ev_header = evhttp_request_get_output_headers(ev_http_req);
    if (ev_header == nullptr) {
        LOG_ERRO("{}", "ev_header is nullptr!");
        return false;
    }

    if (!AddCommonHeaders(ev_header, req_id)) {
        return false;
    }

    evhttp_add_header(ev_header, "Content-Type", "application/octet-stream");
    std::string content_length = std::to_string(fsize);
    evhttp_add_header(ev_header, "Content-Length", content_length.c_str());
    std::string attachmentFile = "attachment; filename=" + file_name;
    evhttp_add_header(ev_header, "Content-Disposition", attachmentFile.c_str());
    return true;
}

}  // namespace cosmo::network::http
