#include "PromptQueue.h"

namespace qtmux {

bool mayDispatchNext(const DispatchContext &ctx) {
    // Nichts anzubieten — die häufigste Antwort, deshalb zuerst.
    if (!ctx.queueNotEmpty) return false;

    // Tote Session: keine Abgabe, egal wie ruhig es dort ist. Bewusst VOR der
    // Verzweigung, denn `Closed` kommt aus dem Backend-Zustand und ist auch dann
    // aussagekräftig, wenn die Session nie selbst etwas gemeldet hat.
    if (ctx.activity == ActivityClosed) return false;

    // Der Mensch tippt dort gerade: nichts abgeben, in keinem der beiden Zweige. Der
    // Eintrag hinge sonst an seinem Entwurf und ginge mit ihm zusammen hinaus.
    if (ctx.ownerTyping) return false;

    // Zweig 1: Die Session meldet ihren Zustand selbst → ihrer Meldung wird geglaubt.
    // Alles außer „arbeitet gerade" ist ein Ruhezustand (Begründung im Header).
    if (ctx.activityReported) return ctx.activity != ActivityRunning;

    // Zweig 2: nie gemeldet. `ctx.activity` ist hier bedeutungslos (Startwert `Running`)
    // und wird NICHT geprüft — sonst stünde die Warteschlange bei jeder gewöhnlichen
    // Shell für immer. Entschieden wird über Ruhe im Ausgabestrom.
    if (ctx.msSinceLastOutput < 0) return false;   // noch kein Lebenszeichen → nicht bereit
    return ctx.msSinceLastOutput >= ctx.quietMs;
}

PromptQueue::PromptQueue(QObject *parent) : QObject(parent) {}

bool PromptQueue::isAcceptable(const QString &text) {
    return !text.trimmed().isEmpty();
}

QString PromptQueue::at(int index) const {
    if (index < 0 || index >= m_entries.size()) return {};
    return m_entries.at(index);
}

QString PromptQueue::senderAt(int index) const {
    if (index < 0 || index >= m_senders.size()) return {};
    return m_senders.at(index);
}

bool PromptQueue::enqueue(const QString &text, const QString &sender) {
    if (!isAcceptable(text)) return false;
    m_entries.append(text);          // ungetrimmt ablegen (s. isAcceptable)
    m_senders.append(sender);
    emit changed();
    return true;
}

bool PromptQueue::insert(int index, const QString &text, const QString &sender) {
    if (!isAcceptable(text)) return false;
    const int pos = qBound(0, index, int(m_entries.size()));   // klemmen statt abweisen
    m_entries.insert(pos, text);
    m_senders.insert(pos, sender);
    emit changed();
    return true;
}

bool PromptQueue::removeAt(int index) {
    if (index < 0 || index >= m_entries.size()) return false;
    m_entries.removeAt(index);
    m_senders.removeAt(index);
    emit changed();
    return true;
}

int PromptQueue::clear() {
    const int n = int(m_entries.size());
    if (n == 0) return 0;            // kein Signal für ein Nichts-Ereignis
    m_entries.clear();
    m_senders.clear();
    emit changed();
    return n;
}

bool PromptQueue::move(int from, int to) {
    const int n = int(m_entries.size());
    // Hier wird NICHT geklemmt, anders als in insert(): Umsortieren ist eine
    // Zieloperation auf vorhandenen Elementen — ein Index daneben ist ein Fehler des
    // Aufrufers und soll sichtbar scheitern, nicht stillschweigend etwas anderes tun.
    if (from < 0 || from >= n || to < 0 || to >= n || from == to) return false;
    m_entries.move(from, to);
    m_senders.move(from, to);
    emit changed();
    return true;
}

QString PromptQueue::takeNext() {
    // Reentranz-Wächter: verschachtelte Abgabe (aus einem `changed`-Slot heraus) würde
    // die Reihenfolge auf der Leitung vertauschen — Begründung im Header.
    if (m_dispatching) return {};
    if (m_entries.isEmpty()) return {};

    m_dispatching = true;
    // Erst entnehmen, DANN melden: ein Slot, der auf `changed` hin einreiht, landet
    // dadurch zwangsläufig hinter den noch wartenden Einträgen.
    const QString next = m_entries.takeFirst();
    m_lastTakenSender = m_senders.takeFirst();
    emit changed();
    m_dispatching = false;
    return next;
}

} // namespace qtmux
