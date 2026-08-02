#include <QtTest>

#include <QCoreApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "McpAccess.h"

using namespace qtmux;

/// QTMUX-127: Zugriffsregeln des MCP-Servers — an welche Adresse er bindet, wann ein
/// Token Pflicht ist, wie das Token geprüft wird.
///
/// Diese Regeln SIND die Zugriffskontrolle des Servers (`send_text` ist faktisch
/// Befehlsausführung unter der UID des Prozesses). Sie liegen deshalb Gui-frei in
/// `McpAccess.{h,cpp}` und werden hier festgenagelt — inklusive der Richtungen, die
/// bei einer versehentlichen Lockerung als Erstes kippen würden: ungültige Adresse
/// darf NICHT „alle Schnittstellen" bedeuten, ein leeres Token darf nie passen.
class tst_mcpaccess : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();

    void defaultBindsToLoopback();
    void keywordsAndLiteralsResolve();
    void invalidAddressFallsBackToLoopbackWithReason();
    void environmentBeatsSetting();
    void tokenResolutionOrder();
    void generatedTokenIsRandomAndUrlSafe();
    void tokenCompareRejectsPrefixAndEmpty();
    void bearerHeaderParsing();
    void networkWithoutTokenIsRefused();
    void loopbackNeedsNoToken();
    void autoGenerationOnlyForSettingsSource();

private:
    QTemporaryDir m_dir;
};

void tst_mcpaccess::initTestCase() {
    QVERIFY(m_dir.isValid());
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_dir.path());
    QCoreApplication::setOrganizationName(QStringLiteral("QTmuxTest"));
    QCoreApplication::setApplicationName(QStringLiteral("tst_mcpaccess"));
}

void tst_mcpaccess::cleanup() {
    QSettings s;
    s.clear();
    s.sync();
    qunsetenv("QTMUX_MCP_BIND");
    qunsetenv("QTMUX_MCP_TOKEN");
}

void tst_mcpaccess::defaultBindsToLoopback() {
    const mcpaccess::Bind b = mcpaccess::defaultBind();
    QVERIFY(b.isLoopback());
    QCOMPARE(b.text(), QStringLiteral("127.0.0.1"));
    QVERIFY(b.error.isEmpty());
    QVERIFY(!b.fromEnvironment);
    QVERIFY(!mcpaccess::authRequiredFor(b));
}

void tst_mcpaccess::keywordsAndLiteralsResolve() {
    QVERIFY(mcpaccess::resolveBind(QStringLiteral("localhost")).isLoopback());
    QVERIFY(mcpaccess::resolveBind(QStringLiteral("  ")).isLoopback());
    QVERIFY(mcpaccess::resolveBind(QStringLiteral("::1")).isLoopback());

    const mcpaccess::Bind any = mcpaccess::resolveBind(QStringLiteral("0.0.0.0"));
    QVERIFY(any.error.isEmpty());
    QVERIFY(!any.isLoopback());
    QCOMPARE(any.text(), QStringLiteral("0.0.0.0"));
    QVERIFY(mcpaccess::authRequiredFor(any));

    QVERIFY(!mcpaccess::resolveBind(QStringLiteral("any")).isLoopback());
    QVERIFY(!mcpaccess::resolveBind(QStringLiteral("*")).isLoopback());

    const mcpaccess::Bind lan = mcpaccess::resolveBind(QStringLiteral("192.168.0.10"));
    QVERIFY(lan.error.isEmpty());
    QCOMPARE(lan.text(), QStringLiteral("192.168.0.10"));
    QVERIFY(mcpaccess::authRequiredFor(lan));
}

// 🔑 Die wichtigste Richtung: Unsinn darf NICHT zu QHostAddress::Any werden. Ein
// Tippfehler in der Einstellung würde den Server sonst still ins Netz stellen.
// Auch ein DNS-Name ist ungültig — sonst hinge der Start an einer Namensauflösung,
// und ein Name kann auf eine fremde Adresse zeigen.
void tst_mcpaccess::invalidAddressFallsBackToLoopbackWithReason() {
    for (const QString &bad : {QStringLiteral("banane"), QStringLiteral("999.1.1.1"),
                               QStringLiteral("192.168.0.10:7345"),
                               QStringLiteral("example.com")}) {
        const mcpaccess::Bind b = mcpaccess::resolveBind(bad);
        QVERIFY2(b.isLoopback(), qPrintable(bad));
        QVERIFY2(!b.error.isEmpty(), qPrintable(bad));
        QVERIFY2(b.error.contains(bad), qPrintable(b.error));
        QVERIFY2(!mcpaccess::authRequiredFor(b), qPrintable(bad));
    }
}

void tst_mcpaccess::environmentBeatsSetting() {
    QSettings s;
    s.setValue(QStringLiteral("mcp/bindAddress"), QStringLiteral("192.168.0.10"));
    s.sync();

    mcpaccess::Bind fromSetting = mcpaccess::defaultBind();
    QCOMPARE(fromSetting.text(), QStringLiteral("192.168.0.10"));
    QVERIFY(!fromSetting.fromEnvironment);

    qputenv("QTMUX_MCP_BIND", "0.0.0.0");
    const mcpaccess::Bind fromEnv = mcpaccess::defaultBind();
    QCOMPARE(fromEnv.text(), QStringLiteral("0.0.0.0"));
    QVERIFY(fromEnv.fromEnvironment);

    // Leere Umgebungsvariable zählt nicht als Angabe.
    qputenv("QTMUX_MCP_BIND", "   ");
    QCOMPARE(mcpaccess::defaultBind().text(), QStringLiteral("192.168.0.10"));
    QVERIFY(!mcpaccess::defaultBind().fromEnvironment);
}

void tst_mcpaccess::tokenResolutionOrder() {
    QVERIFY(mcpaccess::resolveToken().isEmpty());
    QVERIFY(!mcpaccess::tokenFromEnvironment());

    QSettings s;
    s.setValue(QStringLiteral("mcp/token"), QStringLiteral("aus-der-einstellung"));
    s.sync();
    QCOMPARE(mcpaccess::resolveToken(), QStringLiteral("aus-der-einstellung"));
    QVERIFY(!mcpaccess::tokenFromEnvironment());

    qputenv("QTMUX_MCP_TOKEN", "aus-der-umgebung");
    QCOMPARE(mcpaccess::resolveToken(), QStringLiteral("aus-der-umgebung"));
    QVERIFY(mcpaccess::tokenFromEnvironment());
}

void tst_mcpaccess::generatedTokenIsRandomAndUrlSafe() {
    const QString a = mcpaccess::generateToken();
    const QString b = mcpaccess::generateToken();
    QVERIFY(a != b);
    // 32 Byte base64url ohne Polsterung = 43 Zeichen.
    QCOMPARE(a.size(), 43);
    const QRegularExpression urlSafe(QStringLiteral("^[A-Za-z0-9_-]+$"));
    QVERIFY(urlSafe.match(a).hasMatch());
    QVERIFY(urlSafe.match(b).hasMatch());
}

void tst_mcpaccess::tokenCompareRejectsPrefixAndEmpty() {
    const QByteArray expected = "s3cret-token";
    QVERIFY(mcpaccess::tokenAccepted(expected, expected));
    QVERIFY(!mcpaccess::tokenAccepted("s3cret-toke", expected));    // zu kurz
    QVERIFY(!mcpaccess::tokenAccepted("s3cret-tokenX", expected));  // zu lang
    QVERIFY(!mcpaccess::tokenAccepted("S3cret-token", expected));   // ein Bit anders
    QVERIFY(!mcpaccess::tokenAccepted("", expected));
    // 🔑 Ein leeres erwartetes Token darf NIE passen — sonst öffnete eine vergessene
    // Konfiguration den Server für jeden, der gar keinen Kopf mitschickt.
    QVERIFY(!mcpaccess::tokenAccepted("", QByteArray()));
    QVERIFY(!mcpaccess::tokenAccepted("irgendwas", QByteArray()));
}

void tst_mcpaccess::bearerHeaderParsing() {
    const QByteArray head =
        "POST /mcp HTTP/1.1\r\n"
        "Host: 192.168.0.5:7345\r\n"
        "AUTHORIZATION: Bearer  abc123-XYZ\r\n"
        "Content-Type: application/json";
    QCOMPARE(mcpaccess::bearerToken(head), QByteArray("abc123-XYZ"));

    // Kleinschreibung des Schemas ist laut RFC 7235 zulässig.
    QCOMPARE(mcpaccess::bearerToken("Authorization: bearer tok"), QByteArray("tok"));
    // Anderes Schema oder gar kein Kopf → nichts (führt zu 401).
    QVERIFY(mcpaccess::bearerToken("Authorization: Basic dXNlcjpwdw==").isEmpty());
    QVERIFY(mcpaccess::bearerToken("POST /mcp HTTP/1.1\r\nHost: x").isEmpty());
    QVERIFY(mcpaccess::bearerToken(QByteArray()).isEmpty());
}

void tst_mcpaccess::networkWithoutTokenIsRefused() {
    const mcpaccess::Bind net = mcpaccess::resolveBind(QStringLiteral("0.0.0.0"));
    const mcpaccess::StartCheck without = mcpaccess::checkStart(net, QString());
    QVERIFY(!without.allowed);
    QVERIFY(without.authRequired);
    QVERIFY(!without.reason.isEmpty());

    const mcpaccess::StartCheck with = mcpaccess::checkStart(net, QStringLiteral("tok"));
    QVERIFY(with.allowed);
    QVERIFY(with.authRequired);
    QVERIFY(with.reason.isEmpty());

    // Auch eine konkrete LAN-Adresse ist „nicht Loopback" und damit tokenpflichtig.
    const mcpaccess::Bind lan = mcpaccess::resolveBind(QStringLiteral("192.168.0.10"));
    QVERIFY(!mcpaccess::checkStart(lan, QString()).allowed);
}

// Bestehende lokale Setups müssen unverändert weiterlaufen: auf Loopback wird kein
// Token verlangt, auch wenn eines konfiguriert ist.
void tst_mcpaccess::loopbackNeedsNoToken() {
    const mcpaccess::Bind lo = mcpaccess::resolveBind(QStringLiteral("127.0.0.1"));
    const mcpaccess::StartCheck c = mcpaccess::checkStart(lo, QString());
    QVERIFY(c.allowed);
    QVERIFY(!c.authRequired);
    QVERIFY(!mcpaccess::checkStart(lo, QStringLiteral("tok")).authRequired);
}

// Ein Token darf nur dort automatisch entstehen, wo die Oberfläche es anzeigen kann.
// Kommt die Öffnung aus QTMUX_MCP_BIND (Skript/CI), bekäme es niemand zu sehen —
// dann ist die Startverweigerung die ehrliche Antwort.
void tst_mcpaccess::autoGenerationOnlyForSettingsSource() {
    mcpaccess::Bind fromSetting = mcpaccess::resolveBind(QStringLiteral("0.0.0.0"));
    fromSetting.fromEnvironment = false;
    QVERIFY(mcpaccess::shouldAutoGenerateToken(fromSetting, QString()));
    QVERIFY(!mcpaccess::shouldAutoGenerateToken(fromSetting, QStringLiteral("da")));

    mcpaccess::Bind fromEnv = fromSetting;
    fromEnv.fromEnvironment = true;
    QVERIFY(!mcpaccess::shouldAutoGenerateToken(fromEnv, QString()));

    const mcpaccess::Bind lo = mcpaccess::resolveBind(QStringLiteral("127.0.0.1"));
    QVERIFY(!mcpaccess::shouldAutoGenerateToken(lo, QString()));
}

QTEST_MAIN(tst_mcpaccess)
#include "tst_mcpaccess.moc"
