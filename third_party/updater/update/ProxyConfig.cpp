#include "update/ProxyConfig.hpp"

#include <QNetworkProxyFactory>
#include <QNetworkProxyQuery>

#include <chrono>
#include <future>
#include <memory>

namespace appupdate {

bool isUsable(const ProxyConfig& cfg) noexcept
{
    if (cfg.mode != ProxyConfig::Mode::Manual) return true;
    return !cfg.host.isEmpty() && cfg.port != 0;
}

QNetworkProxy toQNetworkProxy(const ProxyConfig& cfg)
{
    if (cfg.mode != ProxyConfig::Mode::Manual || !isUsable(cfg)) {
        return QNetworkProxy(QNetworkProxy::NoProxy);
    }
    QNetworkProxy p(cfg.type, cfg.host, cfg.port);
    // The user may travel in the config; the password never does — it
    // arrives through the credential callback, per request.
    if (!cfg.user.isEmpty()) p.setUser(cfg.user);
    return p;
}

ResolvedProxy resolveSystemProxy(const QUrl& target, int timeoutMs)
{
    // The query runs on a detached worker because it cannot be cancelled:
    // a dead PAC URL keeps CFNetwork busy for ~64 s (measured), and we
    // refuse to hand that latency to the caller. On timeout we abandon the
    // result — the worker finishes into a shared promise that nobody reads
    // anymore, then dies on its own.
    auto promise = std::make_shared<std::promise<QList<QNetworkProxy>>>();
    auto future  = promise->get_future();

    std::thread([promise, target]() {
        // May block for a long time. Nothing here touches shared state
        // except the promise, which outlives us via the shared_ptr.
        auto list = QNetworkProxyFactory::systemProxyForQuery(
            QNetworkProxyQuery(target));
        promise->set_value(list);
    }).detach();

    ResolvedProxy out;
    if (future.wait_for(std::chrono::milliseconds(timeoutMs))
        != std::future_status::ready) {
        out.status = ProxyResolution::TimedOut;
        out.diagnostic = QStringLiteral(
            "the system proxy query did not answer within %1 ms — a "
            "configured but unreachable PAC/WPAD URL is the usual cause")
            .arg(timeoutMs);
        return out;
    }

    const QList<QNetworkProxy> proxies = future.get();
    if (proxies.isEmpty()) {
        // An empty list is not the same as "go direct": the platform gave
        // us nothing at all to work with.
        out.status = ProxyResolution::Failed;
        out.diagnostic = QStringLiteral(
            "the system returned no proxy entries at all");
        return out;
    }

    for (const auto& p : proxies) {
        if (p.type() != QNetworkProxy::NoProxy
            && p.type() != QNetworkProxy::DefaultProxy) {
            out.status     = ProxyResolution::Proxy;
            out.proxy      = p;
            out.diagnostic = QStringLiteral("system proxy %1:%2")
                                 .arg(p.hostName()).arg(p.port());
            return out;
        }
    }

    out.status = ProxyResolution::NoProxy;
    out.diagnostic = QStringLiteral("the system asked for a direct connection");
    return out;
}

}  // namespace appupdate
