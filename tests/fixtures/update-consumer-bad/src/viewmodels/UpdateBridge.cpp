// Gegenprobe fuer test_update_consumer_guard_bites (nie gebaut): Die Wahl des Hubs
// steht unter einer einzeiligen Bedingung. Regel 5 muss anschlagen.
UpdateBridge::UpdateBridge(Wiring wiring)
{
    if (wiring.selfTest)
        m_flow->setInstallBackend(appupdate::makeInstallBackendForThisSystem());
}
