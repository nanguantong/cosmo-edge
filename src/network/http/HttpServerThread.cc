// HttpServerThread — HTTP request handler thread

#include "network/http/HttpServerThread.h"

#include <memory>

#include "network/http/HttpServer.h"
#include "util/FileUtil.h"
#include "util/Log.h"
#include "util/StringUtil.h"

namespace cosmo::network::http {

namespace chrono = std::chrono;

MsgHanderThread::MsgHanderThread(const std::string& name, HttpServer* server,
                                 std::unique_ptr<cosmo::IRequestDispatcher> dispatcher, size_t maxCount)
    : cosmo::MsgThread(name), handler_(std::move(dispatcher)), server_(server) {
    (void)maxCount;
}

MsgHanderThread::~MsgHanderThread() {
    Stop();
}

void MsgHanderThread::HandleMsg(cosmo::MsgEnvelope& msg) {
    is_thread_busy_.store(true, std::memory_order_relaxed);
    auto ptrAclTask = msg.ReleaseData();
    HttpReqTask* request_task{nullptr};

    try {
        auto msg_id  = static_cast<InnerMsgId>(msg.GetMsgId());
        request_task = static_cast<HttpReqTask*>(ptrAclTask.get());
        if (request_task != nullptr) {
            switch (msg_id) {
                case InnerMsgId::kHttpReq:
                    ProcessHttpReqTask(*request_task);
                    break;
                case InnerMsgId::kHttpOctetReq:
                    ProcessHttpOctetReqTask(*request_task);
                    break;
                default:
                    break;
            }
            DelTmpPath(*request_task);
        }
    } catch (...) {
        if (request_task != nullptr) {
            DelTmpPath(*request_task);
        }
        is_thread_busy_.store(false, std::memory_order_relaxed);
        throw;
    }

    is_thread_busy_.store(false, std::memory_order_relaxed);
}

void MsgHanderThread::ClearMsg(cosmo::MsgEnvelope& msg) {
    auto msg_id = static_cast<InnerMsgId>(msg.GetMsgId());
    if (msg_id == InnerMsgId::kHttpReq || msg_id == InnerMsgId::kHttpOctetReq) {
        if (auto* task = static_cast<HttpReqTask*>(msg.GetData())) {
            DelTmpPath(*task);
        }
    }
    cosmo::MsgThread::ClearMsg(msg);
}

void MsgHanderThread::DelTmpPath(const HttpReqTask& task) {
    if (task.has_tmp_path) {
        cosmo::util::RemovePath(task.tmp_file_path);
    }
}

void MsgHanderThread::ProcessHttpReqTask(HttpReqTask& task) {
    const auto& uri = task.interface;
    if (!task.x_forwarded_for.empty()) {
        LOG_INFO("{} Handle {}, From:{}", Name(), uri, task.x_forwarded_for);
    } else {
        LOG_INFO("{} Handle {}", Name(), uri);
    }
    std::string request_id;

    // Truncate large bodies in logs to avoid memory/disk pressure
    if (task.body.size() > 4096) {
        LOG_INFO("{} Receive uri:{}, body: {:.4096} ...", Name(), uri, task.body);
    } else {
        LOG_INFO("{} Receive uri:{}, body: {}", Name(), uri, task.body);
    }

    std::string response;

    if (!handler_->SupportsRoute(uri)) {
        LOG_INFO("{} Handle uri:{} BAD REQUEST", Name(), uri);
        SendHttpAck(HttpResponseCode::kBadRequest, task.request_token, std::string("{}"),
                    std::move(request_id));
        return;
    }

    if (task.mtk.empty()) {
        LOG_WARN("{}", "MTK Empty");
    }
    if (!handler_->DispatchRequest(uri, task.mtk, task.body, response)) {
        LOG_INFO("{} Handle uri:{} With {} Ms Rsp: MV_HTTP_NEED_AUTHENTICATE", Name(), uri,
                 chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - task.request_time)
                     .count());
        SendHttpAck(HttpResponseCode::kNeedAuthenticate, task.request_token, std::move(response),
                    std::move(request_id));
        return;
    }

    LOG_INFO(
        "{} Handle uri:{} With {} Ms Rsp: {:.4096}{}", Name(), uri,
        chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - task.request_time).count(),
        response, response.size() > 4096 ? " ..." : "");
    SendHttpAck(HttpResponseCode::kOk, task.request_token, std::move(response), std::move(request_id));
}

void MsgHanderThread::SendHttpAck(HttpResponseCode code, HttpRequestToken request_token, std::string response,
                                  std::string request_id) {
    auto ackTask = std::make_unique<HttpAckTask>(static_cast<int>(code), request_token, std::move(response),
                                                 std::move(request_id));
    cosmo::MsgEnvelope msg(static_cast<int>(InnerMsgId::kHttpAck), std::move(ackTask));
    server_->Put(std::move(msg));
}

void MsgHanderThread::ProcessHttpOctetReqTask(HttpReqTask& task) {
    std::string response;
    std::string request_id;
    const auto& uri = task.interface;
    auto ackTask = std::make_unique<HttpAckTask>(static_cast<int>(HttpResponseCode::kOk), task.request_token,
                                                 std::move(response), std::move(request_id));
    ackTask->file_name = std::string(cosmo::util::GetLastPathSegment(uri));
    ackTask->file_path = server_->callbacks().get_user_data_path() + uri;
    LOG_INFO("{} Handle {} interface:{}", Name(), uri, ackTask->file_name);
    cosmo::MsgEnvelope msg(static_cast<int>(InnerMsgId::kHttpOctetAck), std::move(ackTask));
    server_->Put(std::move(msg));
}

}  // namespace cosmo::network::http
