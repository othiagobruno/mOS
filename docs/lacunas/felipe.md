# Lacunas de Felipe (Wi-Fi, Bluetooth, Ethernet)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Felipe (Wi-Fi, Bluetooth, Ethernet) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**16** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| IOBluetooth.framework | `_IOBluetoothNSStringToDeviceAddress` | 2 | anki, qgis |
| IOBluetooth.framework | `_OBJC_CLASS_$_IOBluetoothDevicePair` | 2 | anki, qgis |
| CoreWLAN.framework | `_CWBSSIDDidChangeNotification` | 1 | hammerspoon |
| CoreWLAN.framework | `_CWCountryCodeDidChangeNotification` | 1 | hammerspoon |
| CoreWLAN.framework | `_CWKeychainFindWiFiPassword` | 1 | prusaslicer |
| CoreWLAN.framework | `_CWLinkDidChangeNotification` | 1 | hammerspoon |
| CoreWLAN.framework | `_CWLinkQualityDidChangeNotification` | 1 | hammerspoon |
| CoreWLAN.framework | `_CWLinkQualityNotificationRSSIKey` | 1 | hammerspoon |
| CoreWLAN.framework | `_CWLinkQualityNotificationTransmitRateKey` | 1 | hammerspoon |
| CoreWLAN.framework | `_CWModeDidChangeNotification` | 1 | hammerspoon |
| CoreWLAN.framework | `_CWPowerDidChangeNotification` | 1 | hammerspoon |
| CoreWLAN.framework | `_CWSSIDDidChangeNotification` | 1 | hammerspoon |
| CoreWLAN.framework | `_CWScanCacheDidUpdateNotification` | 1 | hammerspoon |
| CoreWLAN.framework | `_OBJC_CLASS_$_CWNetwork` | 1 | bartender |
| CoreWLAN.framework | `_OBJC_CLASS_$_CWNetworkProfile` | 1 | protonvpn |
| CoreWLAN.framework | `_OBJC_METACLASS_$_CWNetworkProfile` | 1 | protonvpn |
