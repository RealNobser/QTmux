// Gegenprobe fuer test_update_consumer_guard_bites (nie gebaut): Die App waehlt den
// Installationsweg selbst. Regel 5 muss an jeder der drei Zeilen anschlagen.
void wireUpdates(appupdate::UpdateFlow *flow)
{
    flow->setInstallBackend(appupdate::makeSystemInstallerBackend());
    flow->setInstallBackend(appupdate::makeInstallBackendForThisSystem()); flow->setInstallBackend(appupdate::makeSystemInstallerBackend());
    qInfo("file://x"); flow->setInstallBackend(appupdate::makeSelfInstallBackend());
}
