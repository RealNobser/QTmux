#pragma once

// appupdate::ProxyConfig — the proxy contract of MacPCANUpdater (MAC-36).
//
// CANONICAL: RAFTNG consumes this via submodule, QTmux vendors it
// byte-identically. It is built once, here.
//
// The library stays GUI-free and QSettings-free: it owns the CONFIG type
// and a credential CALLBACK, never a dialog and never a settings file.
// Each app persists what it wants and shows its own dialog.
//
// NEVER PERSIST A PASSWORD. This struct deliberately has no password
// field, so the thing an app is tempted to write to disk cannot contain
// one. In a corporate setting proxy credentials are usually DOMAIN
// credentials; a plaintext INI leaks them into Time Machine, OneDrive and
// roaming profiles. Session memory by default, platform keychain as an
// opt-in owned by the app.
//
// Two platform findings drive the design (measured 2026-08-05 against
// Qt 6.10.3, not taken from the docs — see docs/concept-updater-proxy.md):
//
//   * macOS resolves PAC/WPAD, but IGNORES http_proxy/https_proxy
//     entirely. Without a system setting, only Mode::Manual gets through.
//   * Linux is the mirror image: no PAC, no WPAD (libproxy is disabled in
//     the official Qt builds), but the environment route works fully,
//     including no_proxy exceptions.
//   * A configured but UNREACHABLE PAC URL blocks the first system
//     resolution for 64 SECONDS synchronously and then answers "no
//     proxy" — a silent failure with a wrong result. Hence
//     resolveSystemProxy() is documented as blocking, carries its own
//     timeout, and reports "found nothing" and "could not resolve" as
//     DIFFERENT outcomes.

#include <QNetworkProxy>
#include <QString>
#include <QUrl>

#include <cstdint>

namespace appupdate {

struct ProxyConfig {
    enum class Mode : std::uint8_t {
        System,   // default: whatever the OS says (PAC/WPAD on Win/macOS)
        None,     // explicitly direct — do not consult the system
        Manual,   // host/port/type below
    };

    Mode mode = Mode::System;
    // Only meaningful for Mode::Manual. HttpProxy or Socks5Proxy;
    // QT_FEATURE_socks5 is on in all our builds.
    QNetworkProxy::ProxyType type = QNetworkProxy::HttpProxy;
    QString  host;
    quint16  port = 0;
    // Optional. Empty means "ask the credential provider when the proxy
    // challenges us". THERE IS NO PASSWORD FIELD — see the file header.
    QString  user;

    [[nodiscard]] bool operator==(const ProxyConfig& o) const noexcept
    {
        return mode == o.mode && type == o.type && host == o.host
            && port == o.port && user == o.user;
    }
    [[nodiscard]] bool operator!=(const ProxyConfig& o) const noexcept
    {
        return !(*this == o);
    }
};

// True when this config could actually be applied. Manual mode needs a
// host and a non-zero port; the other two modes are always usable.
[[nodiscard]] bool isUsable(const ProxyConfig& cfg) noexcept;

// Manual mode → the QNetworkProxy to install. Any other mode yields
// NoProxy: System is resolved through resolveSystemProxy() instead, which
// is a different (blocking) operation.
[[nodiscard]] QNetworkProxy toQNetworkProxy(const ProxyConfig& cfg);

// ---- System resolution ----------------------------------------------------

// How the system resolution ended. The distinction between NoProxy and
// Failed/TimedOut is the whole point: a broken PAC file surfaces as a
// plain host-not-found later on, and the operator then looks in the wrong
// place.
enum class ProxyResolution : std::uint8_t {
    NoProxy,    // the system answered, and the answer is "go direct"
    Proxy,      // the system named a proxy
    TimedOut,   // no answer within the budget — treat as unknown, NOT as direct
    Failed,     // the query itself failed
};

struct ResolvedProxy {
    ProxyResolution status = ProxyResolution::NoProxy;
    QNetworkProxy   proxy;        // only meaningful for status == Proxy
    // Human-readable note for the app's error text, e.g. which proxy the
    // system named, or that the query ran into its timeout. Never a
    // credential.
    QString         diagnostic;
};

// BLOCKING. Call it off the GUI thread — see the 64-second finding in the
// file header. `timeoutMs` bounds the wait, not the underlying OS call:
// a stuck CFNetwork query cannot be cancelled, so on timeout the result is
// abandoned and reported as TimedOut while the OS call runs out on its own.
[[nodiscard]] ResolvedProxy resolveSystemProxy(const QUrl& target,
                                               int timeoutMs = 5000);

}  // namespace appupdate
