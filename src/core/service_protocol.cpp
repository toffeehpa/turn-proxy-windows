#include "core/service_protocol.h"

#include "core/json.h"
#include "core/profile_json.h"

namespace ft {

std::string encode_request(const ServiceRequest& r) {
    json::Value v = json::Object{{"op", r.kind == ServiceRequest::Kind::Start ? "start" : "stop"}};
    if (r.kind == ServiceRequest::Kind::Start) {
        json::Value p = profile_to_json(r.profile);
        p.set("secrets", secrets_to_json(r.profile.secrets));
        v.set("profile", std::move(p));
    }
    return json::serialize(v, -1);
}

std::expected<ServiceRequest, std::string> decode_request(std::string_view msg) {
    if (msg.size() > kMaxServiceMessage) {
        return std::unexpected("request too large");
    }
    auto v = json::parse(msg);
    if (!v || !v->is_object()) {
        return std::unexpected("bad request");
    }
    const auto* op = v->find("op");
    const std::string kind = op != nullptr ? op->as_string() : std::string();
    if (kind == "stop") {
        return ServiceRequest{ServiceRequest::Kind::Stop, {}};
    }
    if (kind != "start") {
        return std::unexpected("unknown op");
    }
    const auto* pv = v->find("profile");
    if (pv == nullptr) {
        return std::unexpected("missing profile");
    }
    auto p = profile_from_json(*pv);
    if (!p) {
        return std::unexpected(p.error());
    }
    if (const auto issues = validate(*p); !issues.empty()) {
        return std::unexpected("invalid profile: " + std::string(to_string(issues.front())));
    }
    return ServiceRequest{ServiceRequest::Kind::Start, std::move(*p)};
}

std::string encode_event(const ServiceEvent& e) {
    const char* kind = e.kind == ServiceEvent::Kind::Output   ? "output"
                       : e.kind == ServiceEvent::Kind::Exited ? "exited"
                                                              : "error";
    return json::serialize(json::Object{{"ev", kind}, {"text", e.text}, {"code", e.code}}, -1);
}

std::optional<ServiceEvent> decode_event(std::string_view msg) {
    if (msg.size() > kMaxServiceMessage) {
        return std::nullopt;
    }
    auto v = json::parse(msg);
    if (!v || !v->is_object()) {
        return std::nullopt;
    }
    const auto* ev = v->find("ev");
    const std::string kind = ev != nullptr ? ev->as_string() : std::string();
    ServiceEvent e;
    if (kind == "output") {
        e.kind = ServiceEvent::Kind::Output;
    } else if (kind == "exited") {
        e.kind = ServiceEvent::Kind::Exited;
    } else if (kind == "error") {
        e.kind = ServiceEvent::Kind::Error;
    } else {
        return std::nullopt;
    }
    if (const auto* t = v->find("text")) {
        e.text = t->as_string();
    }
    if (const auto* c = v->find("code")) {
        e.code = c->as_int(0);
    }
    return e;
}

} // namespace ft
