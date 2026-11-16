# Lacunas de Patricia (rede para apps)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Patricia (rede para apps) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**63** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| CFNetwork.framework | `_kCFProxyPasswordKey` | 5 | krita, musescore, obs, qbittorrent |
| CFNetwork.framework | `_kCFProxyTypeFTP` | 5 | krita, musescore, obs, qbittorrent |
| CFNetwork.framework | `_kCFProxyUsernameKey` | 5 | krita, musescore, obs, qbittorrent |
| SystemConfiguration.framework | `_kSCPropNetProxiesGopherEnable` | 5 | blender, gimp, inkscape, krita |
| SystemConfiguration.framework | `_kSCPropNetProxiesGopherPort` | 5 | blender, gimp, inkscape, krita |
| SystemConfiguration.framework | `_kSCPropNetProxiesGopherProxy` | 5 | blender, gimp, inkscape, krita |
| CFNetwork.framework | `_kCFNetworkProxiesExcludeSimpleHostnames` | 4 | musescore, obs, qbittorrent, shotcut |
| CFNetwork.framework | `_kCFNetworkProxiesFTPEnable` | 4 | musescore, obs, qbittorrent, shotcut |
| CFNetwork.framework | `_kCFNetworkProxiesFTPPort` | 4 | musescore, obs, qbittorrent, shotcut |
| CFNetwork.framework | `_kCFNetworkProxiesFTPProxy` | 4 | musescore, obs, qbittorrent, shotcut |
| CFNetwork.framework | `_kCFNetworkProxiesSOCKSPort` | 4 | musescore, obs, qbittorrent, shotcut |
| CFNetwork.framework | `_kCFNetworkProxiesSOCKSProxy` | 4 | musescore, obs, qbittorrent, shotcut |
| SystemConfiguration.framework | `_kSCCompAnyRegex` | 3 | firefox, qbittorrent, thunderbird |
| libresolv.9.dylib | `_res_9_nmkquery` | 3 | musescore, obs, shotcut |
| libresolv.9.dylib | `_res_9_nsend` | 3 | musescore, obs, shotcut |
| libresolv.9.dylib | `_res_9_setservers` | 3 | musescore, obs, shotcut |
| Network.framework | `_nw_path_monitor_set_cancel_handler` | 2 | musescore, qbittorrent |
| Network.framework | `_nw_release` | 2 | musescore, qbittorrent |
| SystemConfiguration.framework | `_kSCDynamicStoreDomainSetup` | 2 | firefox, thunderbird |
| SystemConfiguration.framework | `_kSCEntNetDNS` | 2 | firefox, thunderbird |
| IOKit.framework | `_SCPreferencesCreate` | 1 | stats |
| Kerberos.framework | `_gss_wrap_size_limit` | 1 | libreoffice |
| SystemConfiguration.framework | `_SCNetworkInterfaceCopyMediaOptions` | 1 | stats |
| SystemConfiguration.framework | `_SCNetworkInterfaceGetInterfaceType` | 1 | stats |
| SystemConfiguration.framework | `_SCNetworkInterfaceGetLocalizedDisplayName` | 1 | stats |
| SystemConfiguration.framework | `_SCNetworkServiceCopyAll` | 1 | stats |
| SystemConfiguration.framework | `_SCNetworkServiceGetInterface` | 1 | stats |
| SystemConfiguration.framework | `_SCNetworkServiceGetServiceID` | 1 | stats |
| SystemConfiguration.framework | `_kSCNetworkInterfaceTypeBluetooth` | 1 | stats |
| SystemConfiguration.framework | `_kSCNetworkInterfaceTypeEthernet` | 1 | stats |
| SystemConfiguration.framework | `_kSCNetworkInterfaceTypeIEEE80211` | 1 | stats |
| SystemConfiguration.framework | `_kSCNetworkInterfaceTypeWWAN` | 1 | stats |
| SystemConfiguration.framework | `_kSCPropNetEthernetMediaSubType` | 1 | stats |
| libswiftNetwork.dylib | `_$s7Network10NWEndpointO16debugDescriptionSSvg` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network10NWListenerC5StateO2eeoiySbAE_AEtFZ` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network10NWListenerC5stateAC5StateOvg` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network10NWListenerCMn` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network12NWConnectionC14ContentContextC16protocolMetadataSayAA010NWProtocolF0CGvgTj` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network12NWConnectionC14SendCompletionO10idempotentyA2EmFWC` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network12NWConnectionC14receiveMessage10completionyy10Foundation4DataVSg_AC14ContentContextCSgSbAA7NWErrorOSgtc_tF` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network12NWConnectionC16debugDescriptionSSvg` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC13ProtocolStackC20applicationProtocolsSayAA17NWProtocolOptionsCGvMTj` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC20defaultProtocolStackAC0dE0Cvg` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC23allowLocalEndpointReuseSbvs` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network17NWProtocolOptionsCMa` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network17NWProtocolOptionsCMn` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network18NWProtocolMetadataCMa` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network18NWProtocolMetadataCMn` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network19NWProtocolWebSocketC6OpcodeO4pingyA2EmFWC` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network19NWProtocolWebSocketC6OpcodeO4pongyA2EmFWC` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network19NWProtocolWebSocketC6OpcodeO6binaryyA2EmFWC` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network19NWProtocolWebSocketC6OpcodeOMa` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network19NWProtocolWebSocketC6OpcodeOSQAAMc` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network19NWProtocolWebSocketC7OptionsC13autoReplyPingSbvsTj` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network19NWProtocolWebSocketC7OptionsCMa` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network19NWProtocolWebSocketC7OptionsCyAeC7VersionOcfc` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network19NWProtocolWebSocketC7VersionO9version13yA2EmFWC` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network19NWProtocolWebSocketC7VersionOMa` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network19NWProtocolWebSocketC8MetadataC6opcodeAC6OpcodeOvg` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network19NWProtocolWebSocketC8MetadataC6opcodeAeC6OpcodeO_tcfc` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network19NWProtocolWebSocketC8MetadataCMa` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network19NWProtocolWebSocketC8MetadataCMn` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network7NWErrorOs28CustomDebugStringConvertibleAAMc` | 1 | iina |
