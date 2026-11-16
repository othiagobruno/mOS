/* mOS — tccutil(1): desfaz escolhas do TCC, como o /usr/bin/tccutil do macOS.
 *   tccutil reset SERVIÇO [BUNDLE_ID]      SERVIÇO: All, Camera, Microphone, AddressBook, Calendar, Reminders,
 *                                          Photos, PhotosAdd, ScreenCapture, Accessibility, AppleEvents,
 *                                          ListenEvent, PostEvent, SystemPolicyAllFiles, SystemPolicyDesktopFolder,
 *                                          SystemPolicyDocumentsFolder, SystemPolicyDownloadsFolder,
 *                                          SystemPolicyRemovableVolumes, SystemPolicyNetworkVolumes, BluetoothAlways,
 *                                          SpeechRecognition, MediaLibrary, Location, LocalNetwork, FocusStatus,
 *                                          Notifications (as do UserNotifications), ou o nome kTCCService... inteiro.
 * O registro é o de userland/libsystem/tcc_core.inc (~/Library/Application Support/com.mos.tcc/decisions.txt).
 * AppleEvents apaga também os pares por app alvo (kTCCServiceAppleEvents+<alvo>). Saída e códigos como os da Apple:
 * "Successfully reset X" (0) e "tccutil: ..." no stderr (1). Teste de host: tools/tccutiltest.sh. */
#define TCC_HOST 1
#include "../libsystem/tcc_core.inc"

static const char *known[] = {
    "Camera", "Microphone", "AddressBook", "Calendar", "Reminders", "Photos", "PhotosAdd", "ScreenCapture",
    "Accessibility", "AppleEvents", "ListenEvent", "PostEvent", "SystemPolicyAllFiles", "SystemPolicyDesktopFolder",
    "SystemPolicyDocumentsFolder", "SystemPolicyDownloadsFolder", "SystemPolicyRemovableVolumes",
    "SystemPolicyNetworkVolumes", "BluetoothAlways", "SpeechRecognition", "MediaLibrary", "Location", "LocalNetwork",
    "FocusStatus", "Willow", "Siri", 0};

static int usage(void) {
    fprintf(stderr, "Usage: tccutil reset SERVICE [BUNDLE_ID]\n");
    return 1;
}

/* o registro bate com o serviço pedido? (full = "kTCCServiceX"; AppleEvents casa também com os "+alvo") */
static int matches(const char *rec, const char *full, int all) {
    if (all) return 1;
    size_t n = strlen(full);
    if (!strncmp(rec, full, n) && (rec[n] == 0 || (rec[n] == '+' && !strcmp(full, "kTCCServiceAppleEvents")))) return 1;
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3 || argc > 4 || strcmp(argv[1], "reset")) return usage();
    const char *svc = argv[2], *bundle = argc == 4 ? argv[3] : 0;
    char full[200];
    int all = !strcmp(svc, "All");
    if (!all) {
        if (!strcmp(svc, "Notifications") || !strcmp(svc, "userNotifications")) snprintf(full, sizeof full, "userNotifications");
        else if (!strncmp(svc, "kTCCService", 11)) snprintf(full, sizeof full, "%s", svc);
        else {
            int ok = 0;
            for (int i = 0; known[i]; i++) if (!strcmp(known[i], svc)) ok = 1;
            if (!ok) { fprintf(stderr, "tccutil: Unknown service: %s\n", svc); return 1; }
            snprintf(full, sizeof full, "kTCCService%s", svc);
        }
    }
    if (bundle && !tcc_tok_ok(bundle)) { fprintf(stderr, "tccutil: No such bundle identifier \"%s\": invalid name\n", bundle); return 1; }
    const char *home = getenv("HOME");
    if (!home || !*home) home = "/Users/Visitante";
    char path[1024];
    snprintf(path, sizeof path, "%s/Library/Application Support/com.mos.tcc/decisions.txt", home);
    tcc_rec r[TCC_CAP];
    int n = tcc_load(path, r, TCC_CAP), k = 0;
    for (int i = 0; i < n; i++) {
        int drop = matches(r[i].service, all ? "" : full, all) && (!bundle || !strcmp(r[i].bundle, bundle));
        if (!drop) r[k++] = r[i];
    }
    if (k != n && tcc_save(path, r, k) != 0) {
        fprintf(stderr, "tccutil: Failed to reset %s%s%s\n", svc, bundle ? " approval status for " : "", bundle ? bundle : "");
        return 1;
    }
    if (bundle) printf("Successfully reset %s approval status for %s\n", svc, bundle);
    else printf("Successfully reset %s\n", svc);
    return 0;
}
