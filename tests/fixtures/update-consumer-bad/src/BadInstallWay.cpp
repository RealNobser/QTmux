// Gegenprobe fuer test_update_consumer_guard_bites (nie gebaut): Die App waehlt den
// Installationsweg selbst. Regel 5 muss anschlagen.
void wireUpdates(appupdate::UpdateFlow *flow)
{
    flow->setInstallBackend(appupdate::makeSystemInstallerBackend());
}
