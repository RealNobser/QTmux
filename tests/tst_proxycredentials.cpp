// QTMUX-129: Sitzungsspeicher für Proxy-Anmeldedaten.
//
// Der Schwerpunkt liegt auf der Ein-Versuch-Regel: `appupdate` erzwingt keine
// Obergrenze (die Lib reicht nur `previousAttemptFailed` durch), also muss sie
// hier sitzen. Ein zweiter Versuch mit falschem Passwort sperrt in einer
// AD-Umgebung das Domänen-Konto.

#include <QtTest>

#include "ProxyCredentials.h"

using qtmux::ProxyCredentials;

class TestProxyCredentials : public QObject {
    Q_OBJECT

private slots:
    void emptyStoreAnswersNothing();
    void passwordIsOfferedExactlyOnce();
    void rejectionDiscardsPasswordButKeepsUser();
    void rejectionIsReportedForTheDialog();
    void freshCredentialsGetAFreshAttempt();
    void clearForgetsEverything();
    void neverAnswersAfterFailureEvenWithPassword();
};

void TestProxyCredentials::emptyStoreAnswersNothing()
{
    ProxyCredentials c;
    QVERIFY(!c.hasUsablePassword());
    // Ohne Anmeldedaten wird nicht geantwortet — die Lib meldet dann
    // ProxyAuthenticationRequired, und erst das öffnet den Dialog.
    QVERIFY(!c.mayAnswer(false));
}

void TestProxyCredentials::passwordIsOfferedExactlyOnce()
{
    ProxyCredentials c;
    c.set(QStringLiteral("DOMAENE\\nutzer"), QStringLiteral("geheim"));
    QVERIFY(c.hasUsablePassword());

    QVERIFY(c.mayAnswer(false));            // erster Versuch: ja
    QCOMPARE(c.user(), QStringLiteral("DOMAENE\\nutzer"));
    QCOMPARE(c.password(), QStringLiteral("geheim"));

    // Zweite Aufforderung OHNE Fehlermeldung (z. B. zweite Anfrage derselben
    // Sitzung): der eine Versuch ist verbraucht.
    QVERIFY(!c.mayAnswer(false));
    QVERIFY(!c.hasUsablePassword());
}

void TestProxyCredentials::rejectionDiscardsPasswordButKeepsUser()
{
    ProxyCredentials c;
    c.set(QStringLiteral("DOMAENE\\nutzer"), QStringLiteral("falsch"));
    QVERIFY(c.mayAnswer(false));

    // Die Gegenstelle lehnt ab.
    QVERIFY(!c.mayAnswer(true));
    QVERIFY(c.password().isEmpty());        // weg — es war falsch
    QCOMPARE(c.user(), QStringLiteral("DOMAENE\\nutzer"));  // bleibt: kein Geheimnis
}

void TestProxyCredentials::rejectionIsReportedForTheDialog()
{
    ProxyCredentials c;
    QVERIFY(!c.lastAttemptRejected());
    c.set(QStringLiteral("u"), QStringLiteral("p"));
    QVERIFY(c.mayAnswer(false));
    QVERIFY(!c.lastAttemptRejected());
    QVERIFY(!c.mayAnswer(true));
    // Ohne dieses Flag fragt der Dialog beim zweiten Mal wortgleich nach und
    // wirkt kaputt.
    QVERIFY(c.lastAttemptRejected());
}

void TestProxyCredentials::freshCredentialsGetAFreshAttempt()
{
    ProxyCredentials c;
    c.set(QStringLiteral("u"), QStringLiteral("falsch"));
    QVERIFY(c.mayAnswer(false));
    QVERIFY(!c.mayAnswer(true));

    // Der Mensch korrigiert den Tippfehler — das MUSS wieder einen Versuch geben,
    // sonst wäre die Korrektur wirkungslos und der Proxy dauerhaft unerreichbar.
    c.set(QStringLiteral("u"), QStringLiteral("richtig"));
    QVERIFY(!c.lastAttemptRejected());
    QVERIFY(c.hasUsablePassword());
    QVERIFY(c.mayAnswer(false));
    QCOMPARE(c.password(), QStringLiteral("richtig"));
}

void TestProxyCredentials::clearForgetsEverything()
{
    ProxyCredentials c;
    c.set(QStringLiteral("u"), QStringLiteral("p"));
    QVERIFY(!c.mayAnswer(true));
    c.clear();
    QVERIFY(c.user().isEmpty());
    QVERIFY(c.password().isEmpty());
    QVERIFY(!c.lastAttemptRejected());
    QVERIFY(!c.mayAnswer(false));
}

void TestProxyCredentials::neverAnswersAfterFailureEvenWithPassword()
{
    // Der eigentliche Lockout-Schutz, scharf gestellt: Selbst wenn frische
    // Anmeldedaten vorliegen, wird auf eine FEHLGESCHLAGENE Aufforderung hin
    // nicht geantwortet. Wer das aufweicht, baut die Kontosperre wieder ein.
    ProxyCredentials c;
    c.set(QStringLiteral("u"), QStringLiteral("p"));
    QVERIFY(!c.mayAnswer(true));
    QVERIFY(c.password().isEmpty());
}

QTEST_MAIN(TestProxyCredentials)
#include "tst_proxycredentials.moc"
