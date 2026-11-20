// Gegenprobe für tests/CheckUpdateConsumer.cmake: jede Zeile unten ist ein Verstoß,
// den der Wächter finden muss. Wird nie gebaut.
// UpdateChecker in einem Kommentar zählt NICHT.
#include <QVersionNumber>
void bad()
{
    appupdate::UpdateChecker checker;
    QSettings().value("update/autoCheck");
    const bool newer = QVersionNumber::fromString("2.0") > QVersionNumber::fromString("1.0");
}
