# Lacunas de Patricia (rede para apps)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Patricia (rede para apps) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**686** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| CFNetwork.framework | `_kCFProxyPasswordKey` | 30 | 1password, adobe-acrobat-reader, audacity, bettertouchtool |
| CFNetwork.framework | `_kCFProxyUsernameKey` | 30 | 1password, adobe-acrobat-reader, audacity, bettertouchtool |
| CFNetwork.framework | `_kCFProxyTypeFTP` | 21 | audacity, calibre, db-browser-for-sqlite, keepassxc |
| CFNetwork.framework | `_kCFNetworkProxiesSOCKSPort` | 19 | anydesk, applite, audacity, calibre |
| CFNetwork.framework | `_kCFNetworkProxiesSOCKSProxy` | 19 | anydesk, applite, audacity, calibre |
| CFNetwork.framework | `_kCFNetworkProxiesExcludeSimpleHostnames` | 14 | audacity, calibre, cmux, keepassxc |
| Network.framework | `_nw_release` | 14 | audacity, bbedit, calibre, dotnet-sdk |
| CFNetwork.framework | `_kCFNetworkProxiesFTPEnable` | 13 | audacity, calibre, keepassxc, moonlight |
| CFNetwork.framework | `_kCFNetworkProxiesFTPPort` | 13 | audacity, calibre, keepassxc, moonlight |
| CFNetwork.framework | `_kCFNetworkProxiesFTPProxy` | 13 | audacity, calibre, keepassxc, moonlight |
| SystemConfiguration.framework | `_kSCPropNetProxiesGopherEnable` | 13 | android-studio, blender, calibre, deluge |
| SystemConfiguration.framework | `_kSCPropNetProxiesGopherPort` | 13 | android-studio, blender, calibre, deluge |
| SystemConfiguration.framework | `_kSCPropNetProxiesGopherProxy` | 13 | android-studio, blender, calibre, deluge |
| SystemConfiguration.framework | `_kSCCompAnyRegex` | 12 | adobe-acrobat-reader, firefox, floorp, librewolf |
| libresolv.9.dylib | `_res_9_nmkquery` | 11 | audacity, calibre, keepassxc, moonlight |
| libresolv.9.dylib | `_res_9_nsend` | 11 | audacity, calibre, keepassxc, moonlight |
| libresolv.9.dylib | `_res_9_setservers` | 11 | audacity, calibre, keepassxc, moonlight |
| SystemConfiguration.framework | `_kSCDynamicStoreDomainSetup` | 10 | cloudflare-warp, firefox, floorp, librewolf |
| SystemConfiguration.framework | `_kSCEntNetDNS` | 10 | cloudflare-warp, firefox, floorp, librewolf |
| Network.framework | `_nw_path_monitor_set_cancel_handler` | 8 | audacity, calibre, keepassxc, moonlight |
| NetworkExtension.framework | `_NEVPNStatusDidChangeNotification` | 8 | brave-browser, cloudflare-warp, cmux, duckduckgo |
| SystemConfiguration.framework | `_SCNetworkInterfaceGetInterfaceType` | 8 | adobe-acrobat-reader, cmux, microsoft-teams, rustdesk |
| SystemConfiguration.framework | `_SCNetworkServiceGetInterface` | 8 | clash-verge-rev, cloudflare-warp, microsoft-teams, stats |
| libswiftNetwork.dylib | `_$s7Network10NWListenerCMn` | 8 | bettertouchtool, duckduckgo, easydict, fork |
| CFNetwork.framework | `_CFHTTPMessageCopyAllHeaderFields` | 7 | adobe-acrobat-reader, bbedit, bettertouchtool, hammerspoon |
| CFNetwork.framework | `_CFHTTPMessageSetHeaderFieldValue` | 7 | adobe-acrobat-reader, bettertouchtool, daisydisk, hammerspoon |
| CFNetwork.framework | `_kCFHTTPVersion1_1` | 7 | adobe-acrobat-reader, bettertouchtool, daisydisk, hammerspoon |
| IOKit.framework | `_SCPreferencesCreate` | 7 | cloudflare-warp, hammerspoon, proxyman, stats |
| Network.framework | `_$s7Network10NWListenerCMn` | 7 | bartender, cmux, codexbar, fluidvoice |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEProvider` | 7 | cloudflare-warp, cmux, duckduckgo, lulu |
| NetworkExtension.framework | `_OBJC_CLASS_$_NETunnelProviderProtocol` | 7 | cloudflare-warp, cmux, duckduckgo, protonvpn |
| SystemConfiguration.framework | `_SCNetworkInterfaceGetLocalizedDisplayName` | 7 | cmux, stats, teamviewer, virtualbox |
| libresolv.9.dylib | `_res_9_ns_initparse` | 7 | adobe-acrobat-reader, microsoft-edge, mongodb-compass, pgadmin4 |
| libresolv.9.dylib | `_res_9_ns_parserr` | 7 | adobe-acrobat-reader, microsoft-edge, mongodb-compass, pgadmin4 |
| CFNetwork.framework | `_CFHTTPMessageAppendBytes` | 6 | bbedit, bettertouchtool, hammerspoon, raycast |
| CFNetwork.framework | `_CFHTTPMessageCopySerializedMessage` | 6 | bettertouchtool, daisydisk, hammerspoon, raycast |
| CFNetwork.framework | `_CFHTTPMessageCreateEmpty` | 6 | bbedit, bettertouchtool, hammerspoon, raycast |
| CFNetwork.framework | `_CFHTTPMessageCreateRequest` | 6 | adobe-acrobat-reader, bettertouchtool, hammerspoon, raycast |
| CFNetwork.framework | `_CFHTTPMessageGetResponseStatusCode` | 6 | adobe-acrobat-reader, bettertouchtool, hammerspoon, raycast |
| CFNetwork.framework | `_CFHTTPMessageIsHeaderComplete` | 6 | bbedit, bettertouchtool, hammerspoon, raycast |
| CFNetwork.framework | `_CFHTTPMessageSetBody` | 6 | adobe-acrobat-reader, bettertouchtool, daisydisk, hammerspoon |
| Kerberos.framework | `_com_err_va` | 6 | android-studio, cyberduck, dbeaver-community, intellij-idea |
| Kerberos.framework | `_krb5_cc_default` | 6 | android-studio, cyberduck, dbeaver-community, intellij-idea |
| Kerberos.framework | `_krb5_cc_end_seq_get` | 6 | android-studio, cyberduck, dbeaver-community, intellij-idea |
| Kerberos.framework | `_krb5_cc_next_cred` | 6 | android-studio, cyberduck, dbeaver-community, intellij-idea |
| Kerberos.framework | `_krb5_cc_set_flags` | 6 | android-studio, cyberduck, dbeaver-community, intellij-idea |
| Kerberos.framework | `_krb5_cc_start_seq_get` | 6 | android-studio, cyberduck, dbeaver-community, intellij-idea |
| Network.framework | `_$s7Network11IPv6AddressVMa` | 6 | cmux, fluidvoice, openclaw, orbstack |
| Network.framework | `_nw_parameters_copy_default_protocol_stack` | 6 | bbedit, dotnet-sdk, git-credential-manager, github |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEOnDemandRuleConnect` | 6 | brave-browser, cloudflare-warp, duckduckgo, protonvpn |
| NetworkExtension.framework | `_OBJC_CLASS_$_NETunnelProviderManager` | 6 | cloudflare-warp, cmux, duckduckgo, protonvpn |
| SystemConfiguration.framework | `_SCNetworkServiceCopyAll` | 6 | cloudflare-warp, microsoft-teams, proxyman, stats |
| SystemConfiguration.framework | `_SCNetworkServiceGetServiceID` | 6 | clash-verge-rev, cloudflare-warp, microsoft-teams, stats |
| libSystem.B.dylib | `_DNSServiceBrowse` | 6 | cmux, cyberduck, libndi, realvnc-connect-viewer |
| libSystem.B.dylib | `_DNSServiceResolve` | 6 | cmux, cyberduck, libndi, realvnc-connect-viewer |
| libresolv.9.dylib | `_res_9_init` | 6 | deluge, dolphin, microsoft-edge, microsoft-teams |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC21requiredLocalEndpointAA10NWEndpointOSgvs` | 6 | duckduckgo, easydict, fork, raycast |
| CFNetwork.framework | `_CFHTTPMessageCreateResponse` | 5 | bettertouchtool, daisydisk, hammerspoon, raycast |
| CFNetwork.framework | `_kCFStreamPropertySSLPeerTrust` | 5 | bettertouchtool, hammerspoon, raycast, superwhisper |
| Network.framework | `_$s7Network10NWEndpointO4HostO4ipv6yAeA11IPv6AddressVcAEmFWC` | 5 | cmux, fluidvoice, orbstack, protonvpn |
| Network.framework | `_$s7Network10NWEndpointO4PortV8rawValues6UInt16Vvg` | 5 | cmux, openclaw, orbstack, protonvpn |
| Network.framework | `_$s7Network11IPv4AddressV8rawValue10Foundation4DataVvg` | 5 | cmux, openclaw, orbstack, protonvpn |
| Network.framework | `_$s7Network11IPv6AddressV8rawValue10Foundation4DataVvg` | 5 | cmux, openclaw, orbstack, protonvpn |
| Network.framework | `_$s7Network11IPv6AddressVMn` | 5 | cmux, openclaw, orbstack, protonvpn |
| Network.framework | `_nw_connection_copy_protocol_metadata` | 5 | dotnet-sdk, git-credential-manager, github, jellyfin |
| Network.framework | `_nw_framer_async` | 5 | dotnet-sdk, git-credential-manager, github, jellyfin |
| Network.framework | `_nw_framer_create_definition` | 5 | dotnet-sdk, git-credential-manager, github, jellyfin |
| Network.framework | `_nw_framer_create_options` | 5 | dotnet-sdk, git-credential-manager, github, jellyfin |
| Network.framework | `_nw_framer_deliver_input` | 5 | dotnet-sdk, git-credential-manager, github, jellyfin |
| Network.framework | `_nw_framer_message_create` | 5 | dotnet-sdk, git-credential-manager, github, jellyfin |
| Network.framework | `_nw_framer_parse_output` | 5 | dotnet-sdk, git-credential-manager, github, jellyfin |
| Network.framework | `_nw_framer_set_cleanup_handler` | 5 | dotnet-sdk, git-credential-manager, github, jellyfin |
| Network.framework | `_nw_framer_set_output_handler` | 5 | dotnet-sdk, git-credential-manager, github, jellyfin |
| Network.framework | `_nw_framer_set_stop_handler` | 5 | dotnet-sdk, git-credential-manager, github, jellyfin |
| Network.framework | `_nw_parameters_create_secure_udp` | 5 | dotnet-sdk, git-credential-manager, github, jellyfin |
| Network.framework | `_nw_protocol_copy_tls_definition` | 5 | dotnet-sdk, git-credential-manager, github, jellyfin |
| Network.framework | `_nw_protocol_stack_prepend_application_protocol` | 5 | dotnet-sdk, git-credential-manager, github, jellyfin |
| Network.framework | `_nw_tls_copy_sec_protocol_metadata` | 5 | dotnet-sdk, git-credential-manager, github, jellyfin |
| Network.framework | `_nw_tls_create_options` | 5 | dotnet-sdk, git-credential-manager, github, jellyfin |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEDNSSettings` | 5 | cloudflare-warp, cmux, duckduckgo, protonvpn |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEIPv4Route` | 5 | cloudflare-warp, cmux, duckduckgo, protonvpn |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEIPv4Settings` | 5 | cloudflare-warp, cmux, duckduckgo, protonvpn |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEIPv6Route` | 5 | cloudflare-warp, cmux, duckduckgo, protonvpn |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEIPv6Settings` | 5 | cloudflare-warp, cmux, duckduckgo, protonvpn |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEOnDemandRule` | 5 | cloudflare-warp, duckduckgo, protonvpn, sfm |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEPacketTunnelNetworkSettings` | 5 | cloudflare-warp, cmux, duckduckgo, protonvpn |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEPacketTunnelProvider` | 5 | cloudflare-warp, cmux, duckduckgo, protonvpn |
| NetworkExtension.framework | `_OBJC_CLASS_$_NETunnelProviderSession` | 5 | duckduckgo, protonvpn, proxyman, sfm |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEVPNConnection` | 5 | cloudflare-warp, cmux, duckduckgo, protonvpn |
| NetworkExtension.framework | `_OBJC_METACLASS_$_NEPacketTunnelProvider` | 5 | cloudflare-warp, cmux, duckduckgo, protonvpn |
| SystemConfiguration.framework | `_SCNetworkServiceGetName` | 5 | clash-verge-rev, proxyman, teamviewer, virtualbox |
| SystemConfiguration.framework | `_kSCNetworkInterfaceTypeIEEE80211` | 5 | adobe-acrobat-reader, stats, teamviewer, wireshark-app |
| SystemConfiguration.framework | `_kSCNetworkInterfaceTypeWWAN` | 5 | adobe-acrobat-reader, stats, teamviewer, wireshark-app |
| libswiftNetwork.dylib | `_$s7Network10NWEndpointO4PortV8rawValues6UInt16Vvg` | 5 | bettertouchtool, duckduckgo, easydict, sequel-ace |
| libswiftNetwork.dylib | `_$s7Network11IPv6AddressVMa` | 5 | duckduckgo, easydict, tableplus, tailscale-app |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC20defaultProtocolStackAC0dE0Cvg` | 5 | bettertouchtool, duckduckgo, iina, raycast |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC23allowLocalEndpointReuseSbvs` | 5 | bettertouchtool, duckduckgo, easydict, iina |
| CFNetwork.framework | `_CFHTTPMessageCopyHeaderFieldValue` | 4 | adobe-acrobat-reader, bettertouchtool, hammerspoon, ubersicht |
| CFNetwork.framework | `_CFHTTPMessageCopyRequestMethod` | 4 | bettertouchtool, hammerspoon, raycast, superwhisper |
| Network.framework | `_$s7Network10NWEndpointO4HostO2eeoiySbAE_AEtFZ` | 4 | cmux, orbstack, protonvpn, proxyman |
| Network.framework | `_$s7Network10NWListenerC4portAA10NWEndpointO4PortVSgvg` | 4 | cmux, openclaw, orbstack, proxyman |
| Network.framework | `_$s7Network11IPv4AddressVyACSg10Foundation4DataV_AA11NWInterfaceVSgtcfC` | 4 | cmux, orbstack, protonvpn, proxyman |
| Network.framework | `_$s7Network11IPv6AddressVyACSg10Foundation4DataV_AA11NWInterfaceVSgtcfC` | 4 | cmux, orbstack, protonvpn, proxyman |
| Network.framework | `_$s7Network12NWConnectionC10parametersAA12NWParametersCvg` | 4 | cmux, orbstack, protonvpn, proxyman |
| Network.framework | `_$s7Network12NWParametersC23allowLocalEndpointReuseSbvs` | 4 | bartender, cmux, orbstack, proxyman |
| Network.framework | `_$s7Network13NWPathMonitorC11currentPathAA0B0Vvg` | 4 | applite, cmux, protonvpn, supacode |
| Network.framework | `_$s7Network6NWPathV17usesInterfaceTypeySbAA11NWInterfaceV0dE0OF` | 4 | cmux, protonvpn, proxyman, windows-app |
| NetworkExtension.framework | `_OBJC_CLASS_$_NENetworkRule` | 4 | duckduckgo, lulu, protonvpn, proxyman |
| SystemConfiguration.framework | `_SCDynamicStoreKeyCreateConsoleUser` | 4 | anydesk, displaylink, openvpn-connect, teamviewer |
| SystemConfiguration.framework | `_SCNetworkServiceGetEnabled` | 4 | clash-verge-rev, proxyman, teamviewer, zoom |
| SystemConfiguration.framework | `_kSCNetworkInterfaceTypeBluetooth` | 4 | stats, teamviewer, wireshark-app, zoom |
| SystemConfiguration.framework | `_kSCNetworkInterfaceTypeEthernet` | 4 | adobe-acrobat-reader, stats, teamviewer, zoom |
| libSystem.B.dylib | `_DNSServiceSetDispatchQueue` | 4 | cmux, spotify, teamviewer, utm |
| libSystem.B.dylib | `_TXTRecordCreate` | 4 | libndi, spotify, teamviewer, zoom |
| libSystem.B.dylib | `_TXTRecordDeallocate` | 4 | libndi, spotify, teamviewer, zoom |
| libSystem.B.dylib | `_TXTRecordGetBytesPtr` | 4 | libndi, spotify, teamviewer, zoom |
| libSystem.B.dylib | `_TXTRecordGetLength` | 4 | libndi, spotify, teamviewer, zoom |
| libSystem.B.dylib | `_TXTRecordSetValue` | 4 | libndi, spotify, teamviewer, zoom |
| libswiftNetwork.dylib | `_$s7Network10NWEndpointO4HostO4ipv6yAeA11IPv6AddressVcAEmFWC` | 4 | duckduckgo, easydict, tableplus, utm |
| libswiftNetwork.dylib | `_$s7Network10NWEndpointO4unixyACSS_tcACmFWC` | 4 | duckduckgo, easydict, fork, tableplus |
| libswiftNetwork.dylib | `_$s7Network10NWListenerC4portAA10NWEndpointO4PortVSgvg` | 4 | bettertouchtool, easydict, sequel-ace, tableplus |
| libswiftNetwork.dylib | `_$s7Network10NWListenerC5stateAC5StateOvg` | 4 | bettertouchtool, easydict, iina, tableplus |
| libswiftNetwork.dylib | `_$s7Network11IPv6AddressVMn` | 4 | duckduckgo, easydict, tableplus, tailscale-app |
| libswiftNetwork.dylib | `_$s7Network12NWConnectionC14receiveMessage10completionyy10Foundation4DataVSg_AC14ContentContextCSgSbAA7NWErrorOSgtc_tF` | 4 | duckduckgo, iina, raycast, utm |
| libswiftNetwork.dylib | `_$s7Network12NWConnectionC8metadata10definitionAA18NWProtocolMetadataCSgAA0E10DefinitionC_tF` | 4 | bettertouchtool, easydict, tableplus, utm |
| libswiftNetwork.dylib | `_$s7Network13NWPathMonitorC11currentPathAA0B0Vvg` | 4 | duckduckgo, raycast, sfm, tailscale-app |
| libswiftNetwork.dylib | `_$s7Network13NWProtocolTLSC10definitionAA0B10DefinitionCvgZ` | 4 | bettertouchtool, easydict, tableplus, utm |
| libswiftNetwork.dylib | `_$s7Network13NWProtocolTLSC8MetadataC016securityProtocolD0So24OS_sec_protocol_metadata_pvgTj` | 4 | bettertouchtool, easydict, tableplus, utm |
| libswiftNetwork.dylib | `_$s7Network13NWProtocolTLSC8MetadataCMa` | 4 | bettertouchtool, easydict, tableplus, utm |
| libswiftNetwork.dylib | `_$s7Network13NWProtocolTLSCMa` | 4 | bettertouchtool, easydict, tableplus, utm |
| libswiftNetwork.dylib | `_$s7Network6NWPathV17usesInterfaceTypeySbAA11NWInterfaceV0dE0OF` | 4 | duckduckgo, sfm, tableplus, tailscale-app |
| CFNetwork.framework | `_CFHTTPMessageCopyBody` | 3 | bbedit, bettertouchtool, hammerspoon |
| CFNetwork.framework | `_CFHTTPMessageCopyRequestURL` | 3 | adobe-acrobat-reader, bettertouchtool, hammerspoon |
| CFNetwork.framework | `_CFHTTPMessageCopyVersion` | 3 | adobe-acrobat-reader, bettertouchtool, hammerspoon |
| Network.framework | `_$s7Network10NWEndpointO2eeoiySbAC_ACtFZ` | 3 | orbstack, protonvpn, proxyman |
| Network.framework | `_$s7Network10NWEndpointO4HostO4nameyAESS_AA11NWInterfaceVSgtcAEmFWC` | 3 | cmux, fluidvoice, protonvpn |
| Network.framework | `_$s7Network10NWEndpointO4unixyACSS_tcACmFWC` | 3 | cmux, orbstack, proxyman |
| Network.framework | `_$s7Network10NWListenerC5StateOMn` | 3 | bartender, cmux, fluidvoice |
| Network.framework | `_$s7Network11IPv6AddressVyACSgSScfC` | 3 | cmux, openclaw, protonvpn |
| Network.framework | `_$s7Network12NWConnectionC14ContentContextC12finalMessageAEvgZ` | 3 | cmux, orbstack, proxyman |
| Network.framework | `_$s7Network12NWConnectionC17pathUpdateHandleryAA6NWPathVcSgvs` | 3 | cmux, orbstack, proxyman |
| Network.framework | `_$s7Network12NWParametersC21requiredLocalEndpointAA10NWEndpointOSgvs` | 3 | cmux, orbstack, proxyman |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC7noDelaySbvsTj` | 3 | cmux, orbstack, proxyman |
| Network.framework | `_OBJC_CLASS_$_NWHostEndpoint` | 3 | duckduckgo, lulu, protonvpn |
| Network.framework | `_nw_listener_cancel` | 3 | bbedit, dotnet-sdk, spotify |
| Network.framework | `_nw_listener_create` | 3 | bbedit, dotnet-sdk, spotify |
| Network.framework | `_nw_listener_set_new_connection_handler` | 3 | bbedit, dotnet-sdk, spotify |
| Network.framework | `_nw_listener_set_queue` | 3 | bbedit, dotnet-sdk, spotify |
| Network.framework | `_nw_listener_start` | 3 | bbedit, dotnet-sdk, spotify |
| NetworkExtension.framework | `_NEVPNConfigurationChangeNotification` | 3 | duckduckgo, protonvpn, tailscale-app |
| NetworkExtension.framework | `_NEVPNErrorDomain` | 3 | cloudflare-warp, duckduckgo, proxyman |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEAppProxyTCPFlow` | 3 | duckduckgo, protonvpn, proxyman |
| NetworkExtension.framework | `_OBJC_CLASS_$_NETransparentProxyManager` | 3 | duckduckgo, protonvpn, proxyman |
| NetworkExtension.framework | `_OBJC_CLASS_$_NETransparentProxyNetworkSettings` | 3 | duckduckgo, protonvpn, proxyman |
| NetworkExtension.framework | `_OBJC_CLASS_$_NETransparentProxyProvider` | 3 | duckduckgo, protonvpn, proxyman |
| NetworkExtension.framework | `_OBJC_METACLASS_$_NETransparentProxyProvider` | 3 | duckduckgo, protonvpn, proxyman |
| SystemConfiguration.framework | `_SCDynamicStoreCopyDHCPInfo` | 3 | hammerspoon, tailscale-app, unity-hub |
| SystemConfiguration.framework | `_SCNetworkSetCopyCurrent` | 3 | clash-verge-rev, virtualbox, zoom |
| SystemConfiguration.framework | `_SCPreferencesCreate` | 3 | clash-verge-rev, microsoft-teams, zoom |
| SystemConfiguration.framework | `_kSCEntNetProxies` | 3 | cloudflare-warp, proxyman, teamviewer |
| SystemConfiguration.framework | `_kSCNetworkInterfaceTypePPP` | 3 | teamviewer, wireshark-app, zoom |
| libSystem.B.dylib | `_DNSServiceConstructFullName` | 3 | cyberduck, libndi, zoom |
| libSystem.B.dylib | `_DNSServiceNATPortMappingCreate` | 3 | gemini, the-unarchiver, utm |
| libswiftNetwork.dylib | `_$s7Network10NWEndpointO2eeoiySbAC_ACtFZ` | 3 | bettertouchtool, easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network10NWEndpointO4HostO2eeoiySbAE_AEtFZ` | 3 | duckduckgo, easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network11IPv4AddressV16debugDescriptionSSvg` | 3 | duckduckgo, easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network11IPv4AddressV8rawValue10Foundation4DataVvg` | 3 | duckduckgo, easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network11IPv4AddressVyACSg10Foundation4DataV_AA11NWInterfaceVSgtcfC` | 3 | duckduckgo, easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network11IPv6AddressV16debugDescriptionSSvg` | 3 | duckduckgo, easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network11IPv6AddressV8rawValue10Foundation4DataVvg` | 3 | duckduckgo, easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network11IPv6AddressVyACSg10Foundation4DataV_AA11NWInterfaceVSgtcfC` | 3 | duckduckgo, easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC13ProtocolStackC20applicationProtocolsSayAA17NWProtocolOptionsCGvMTj` | 3 | iina, raycast, utm |
| libswiftNetwork.dylib | `_$s7Network13NWProtocolTCPC7OptionsC7noDelaySbvsTj` | 3 | bettertouchtool, easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network17NWProtocolOptionsCMa` | 3 | iina, raycast, utm |
| libswiftNetwork.dylib | `_$s7Network18NWProtocolMetadataCMn` | 3 | easydict, iina, tableplus |
| CFNetwork.framework | `_CFHTTPMessageCopyResponseStatusLine` | 2 | adobe-acrobat-reader, hammerspoon |
| CFNetwork.framework | `_CFHostCreateWithAddress` | 2 | hammerspoon, lulu |
| CFNetwork.framework | `_NSURLSessionTaskPriorityDefault` | 2 | cleanshot, protonvpn |
| CFNetwork.framework | `_NSURLSessionTaskPriorityLow` | 2 | cleanshot, protonvpn |
| CFNetwork.framework | `_kCFHTTPVersion1_0` | 2 | bettertouchtool, hammerspoon |
| CFNetwork.framework | `_kCFStreamErrorDomainFTP` | 2 | hammerspoon, windows-app |
| CFNetwork.framework | `_kCFStreamErrorDomainHTTP` | 2 | hammerspoon, windows-app |
| CFNetwork.framework | `_kCFStreamErrorDomainMach` | 2 | hammerspoon, windows-app |
| CFNetwork.framework | `_kCFStreamErrorDomainSystemConfiguration` | 2 | hammerspoon, windows-app |
| CFNetwork.framework | `_kCFStreamErrorDomainWinSock` | 2 | hammerspoon, windows-app |
| CFNetwork.framework | `_kCFStreamPropertySSLContext` | 2 | raycast, superwhisper |
| IOKit.framework | `_SCPreferencesApplyChanges` | 2 | hammerspoon, proxyman |
| IOKit.framework | `_SCPreferencesCommitChanges` | 2 | hammerspoon, proxyman |
| Network.framework | `_$s7Network10NWEndpointO4HostOSHAAMc` | 2 | cmux, protonvpn |
| Network.framework | `_$s7Network10NWEndpointO4PortVSHAAMc` | 2 | cmux, protonvpn |
| Network.framework | `_$s7Network10NWEndpointO4PortVSQAAMc` | 2 | cmux, protonvpn |
| Network.framework | `_$s7Network10NWEndpointO4PortVyAESgSScfC` | 2 | cmux, protonvpn |
| Network.framework | `_$s7Network10NWListenerC10parametersAA12NWParametersCvg` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network10NWListenerC5stateAC5StateOvg` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network11IPv4AddressV16debugDescriptionSSvg` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network11IPv4AddressVAA9IPAddressAAMc` | 2 | cmux, protonvpn |
| Network.framework | `_$s7Network11IPv4AddressVSQAAMc` | 2 | cmux, protonvpn |
| Network.framework | `_$s7Network11IPv6AddressV16debugDescriptionSSvg` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network11IPv6AddressVAA9IPAddressAAMc` | 2 | cmux, protonvpn |
| Network.framework | `_$s7Network11IPv6AddressVSQAAMc` | 2 | cmux, protonvpn |
| Network.framework | `_$s7Network11NWInterfaceV5indexSivg` | 2 | cmux, protonvpn |
| Network.framework | `_$s7Network12NWConnectionC19EstablishmentReportVMa` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network12NWConnectionC19EstablishmentReportVMn` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network12NWConnectionC23startDataTransferReportAC07PendingdeF0CyF` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network12NWConnectionC25PendingDataTransferReportCMa` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network12NWConnectionC25PendingDataTransferReportCMn` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network12NWConnectionC26requestEstablishmentReport5queue10completionySo012OS_dispatch_F0C_yAC0dE0VSgctF` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network12NWConnectionC5StateO2eeoiySbAE_AEtFZ` | 2 | protonvpn, proxyman |
| Network.framework | `_$s7Network12NWConnectionC5batchyyyyXEF` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network12NWConnectionC8metadata10definitionAA18NWProtocolMetadataCSgAA0E10DefinitionC_tF` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network12NWParametersC17requiredInterfaceAA11NWInterfaceVSgvg` | 2 | cmux, protonvpn |
| Network.framework | `_$s7Network12NWParametersC20MultipathServiceTypeO8disabledyA2EmFWC` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network12NWParametersC20MultipathServiceTypeOMa` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network12NWParametersC20MultipathServiceTypeOMn` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network12NWParametersC20multipathServiceTypeAC09MultipathdE0Ovs` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network12NWParametersC21requiredLocalEndpointAA10NWEndpointOSgvg` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network12NWParametersC3udpACvgZ` | 2 | protonvpn, windows-app |
| Network.framework | `_$s7Network12NWParametersC4dtls3udpAcA13NWProtocolTLSC7OptionsCSg_AA0E3UDPCAHCtcfC` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network12NWParametersCMn` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC02noD0SbvgTj` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC02noD0SbvsTj` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC13keepaliveIdleSivgTj` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC14keepaliveCountSivgTj` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC15enableKeepaliveSbvgTj` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC17connectionTimeoutSivgTj` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC17keepaliveIntervalSivgTj` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC17retransmitFinDropSbvgTj` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC18connectionDropTimeSivgTj` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC18maximumSegmentSizeSivgTj` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC18maximumSegmentSizeSivsTj` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC20disableAckStretchingSbvgTj` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC20disableAckStretchingSbvsTj` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC6noPushSbvgTj` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC6noPushSbvsTj` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC7noDelaySbvgTj` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsCMn` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network13NWProtocolTLSC10definitionAA0B10DefinitionCvgZ` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network13NWProtocolTLSC7OptionsCMn` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network13NWProtocolTLSC8MetadataC016securityProtocolD0So24OS_sec_protocol_metadata_pvgTj` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network13NWProtocolTLSC8MetadataCMa` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network13NWProtocolTLSCMa` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network13NWProtocolUDPC7OptionsCAEycfc` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network13NWProtocolUDPC7OptionsCMa` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network13NWProtocolUDPC7OptionsCMn` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network18NWProtocolMetadataCMn` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network20NWProtocolDefinitionC2eeoiySbAC_ACtFZ` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network20NWProtocolDefinitionCMa` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network20NWProtocolDefinitionCMn` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network6NWPathV2eeoiySbAC_ACtFZ` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network6NWPathV6StatusOMn` | 2 | macwhisper, protonvpn |
| Network.framework | `_$s7Network9IPAddressMp` | 2 | cmux, protonvpn |
| Network.framework | `_$s7Network9IPAddressP8rawValue10Foundation4DataVvgTj` | 2 | cmux, protonvpn |
| Network.framework | `_$s7Network9NWBrowserCMn` | 2 | openclaw, protonvpn |
| Network.framework | `_nw_listener_get_port` | 2 | bbedit, dotnet-sdk |
| Network.framework | `_nw_listener_set_state_changed_handler` | 2 | bbedit, dotnet-sdk |
| Network.framework | `_nw_parameters_set_local_endpoint` | 2 | bbedit, dotnet-sdk |
| Network.framework | `_nw_retain` | 2 | bbedit, dotnet-sdk |
| NetworkExtension.framework | `_NEAppProxyErrorDomain` | 2 | duckduckgo, proxyman |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEEvaluateConnectionRule` | 2 | sfm, tailscale-app |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEOnDemandRuleDisconnect` | 2 | sfm, tailscale-app |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEOnDemandRuleEvaluateConnection` | 2 | sfm, tailscale-app |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEProxySettings` | 2 | brave-browser, sfm |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEVPNManager` | 2 | brave-browser, protonvpn |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEVPNProtocolIKEv2` | 2 | brave-browser, protonvpn |
| SystemConfiguration.framework | `_DHCPInfoGetOptionData` | 2 | tailscale-app, unity-hub |
| SystemConfiguration.framework | `_SCNetworkInterfaceCopyMediaOptions` | 2 | stats, teamviewer |
| SystemConfiguration.framework | `_SCNetworkServiceCopy` | 2 | clash-verge-rev, zoom |
| SystemConfiguration.framework | `_SCNetworkSetCopyServices` | 2 | clash-verge-rev, virtualbox |
| SystemConfiguration.framework | `_SCNetworkSetGetServiceOrder` | 2 | clash-verge-rev, zoom |
| SystemConfiguration.framework | `_kSCEntNetAirPort` | 2 | cloudflare-warp, teamviewer |
| SystemConfiguration.framework | `_kSCNetworkInterfaceTypeIPSec` | 2 | teamviewer, zoom |
| SystemConfiguration.framework | `_kSCNetworkInterfaceTypeModem` | 2 | teamviewer, wireshark-app |
| SystemConfiguration.framework | `_kSCPropNetDNSDomainName` | 2 | cloudflare-warp, teamviewer |
| SystemConfiguration.framework | `_kSCPropNetDNSServerAddresses` | 2 | cloudflare-warp, teamviewer |
| SystemConfiguration.framework | `_kSCPropNetEthernetMediaSubType` | 2 | stats, teamviewer |
| SystemConfiguration.framework | `_kSCPropNetIPv4Addresses` | 2 | cloudflare-warp, teamviewer |
| SystemConfiguration.framework | `_kSCPropNetIPv4Router` | 2 | cloudflare-warp, teamviewer |
| SystemConfiguration.framework | `_kSCPropNetIPv4SubnetMasks` | 2 | cloudflare-warp, teamviewer |
| SystemConfiguration.framework | `_kSCPropNetIPv6Addresses` | 2 | cloudflare-warp, teamviewer |
| SystemConfiguration.framework | `_kSCPropNetIPv6PrefixLength` | 2 | cloudflare-warp, teamviewer |
| SystemConfiguration.framework | `_kSCPropNetIPv6Router` | 2 | cloudflare-warp, teamviewer |
| libSystem.B.dylib | `_DNSServiceReconfirmRecord` | 2 | cyberduck, teamviewer |
| libSystem.B.dylib | `_DNSServiceRegisterRecord` | 2 | cmux, cyberduck |
| libSystem.B.dylib | `_TXTRecordContainsKey` | 2 | libndi, teamviewer |
| libSystem.B.dylib | `_TXTRecordGetCount` | 2 | teamviewer, zoom |
| libSystem.B.dylib | `_TXTRecordGetValuePtr` | 2 | libndi, teamviewer |
| libresolv.9.dylib | `_res_9_query` | 2 | deluge, windows-app |
| libswiftNetwork.dylib | `_$s7Network10NWEndpointO4HostO4nameyAESS_AA11NWInterfaceVSgtcAEmFWC` | 2 | duckduckgo, utm |
| libswiftNetwork.dylib | `_$s7Network10NWListenerC10parametersAA12NWParametersCvg` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network10NWListenerC5StateO2eeoiySbAE_AEtFZ` | 2 | bettertouchtool, iina |
| libswiftNetwork.dylib | `_$s7Network10NWListenerC5StateOMn` | 2 | raycast, tableplus |
| libswiftNetwork.dylib | `_$s7Network11IPv4AddressV8loopbackACvgZ` | 2 | raycast, tableplus |
| libswiftNetwork.dylib | `_$s7Network11IPv4AddressVAA9IPAddressAAMc` | 2 | duckduckgo, tailscale-app |
| libswiftNetwork.dylib | `_$s7Network11IPv6AddressVAA9IPAddressAAMc` | 2 | duckduckgo, tailscale-app |
| libswiftNetwork.dylib | `_$s7Network11IPv6AddressVyACSgSScfC` | 2 | duckduckgo, tailscale-app |
| libswiftNetwork.dylib | `_$s7Network11NWInterfaceV13InterfaceTypeOSQAAMc` | 2 | duckduckgo, tailscale-app |
| libswiftNetwork.dylib | `_$s7Network12NWConnectionC10parametersAA12NWParametersCvg` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network12NWConnectionC14ContentContextC12finalMessageAEvgZ` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network12NWConnectionC14ContentContextC16protocolMetadataSayAA010NWProtocolF0CGvgTj` | 2 | iina, raycast |
| libswiftNetwork.dylib | `_$s7Network12NWConnectionC14SendCompletionO10idempotentyA2EmFWC` | 2 | iina, raycast |
| libswiftNetwork.dylib | `_$s7Network12NWConnectionC16debugDescriptionSSvg` | 2 | iina, raycast |
| libswiftNetwork.dylib | `_$s7Network12NWConnectionC17pathUpdateHandleryAA6NWPathVcSgvs` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network12NWConnectionC19EstablishmentReportVMa` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network12NWConnectionC19EstablishmentReportVMn` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network12NWConnectionC23startDataTransferReportAC07PendingdeF0CyF` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network12NWConnectionC25PendingDataTransferReportCMa` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network12NWConnectionC25PendingDataTransferReportCMn` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network12NWConnectionC26requestEstablishmentReport5queue10completionySo012OS_dispatch_F0C_yAC0dE0VSgctF` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network12NWConnectionC5StateO2eeoiySbAE_AEtFZ` | 2 | bettertouchtool, duckduckgo |
| libswiftNetwork.dylib | `_$s7Network12NWConnectionC5batchyyyyXEF` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC20MultipathServiceTypeO8disabledyA2EmFWC` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC20MultipathServiceTypeOMa` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC20MultipathServiceTypeOMn` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC20multipathServiceTypeAC09MultipathdE0Ovs` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC21requiredLocalEndpointAA10NWEndpointOSgvg` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC4dtls3udpAcA13NWProtocolTLSC7OptionsCSg_AA0E3UDPCAHCtcfC` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network12NWParametersCMn` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network13NWProtocolTCPC7OptionsC02noD0SbvgTj` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network13NWProtocolTCPC7OptionsC02noD0SbvsTj` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network13NWProtocolTCPC7OptionsC13keepaliveIdleSivgTj` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network13NWProtocolTCPC7OptionsC14keepaliveCountSivgTj` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network13NWProtocolTCPC7OptionsC15enableKeepaliveSbvgTj` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network13NWProtocolTCPC7OptionsC17connectionTimeoutSivgTj` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network13NWProtocolTCPC7OptionsC17keepaliveIntervalSivgTj` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network13NWProtocolTCPC7OptionsC17retransmitFinDropSbvgTj` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network13NWProtocolTCPC7OptionsC18connectionDropTimeSivgTj` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network13NWProtocolTCPC7OptionsC18maximumSegmentSizeSivgTj` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network13NWProtocolTCPC7OptionsC18maximumSegmentSizeSivsTj` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network13NWProtocolTCPC7OptionsC20disableAckStretchingSbvgTj` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network13NWProtocolTCPC7OptionsC20disableAckStretchingSbvsTj` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network13NWProtocolTCPC7OptionsC6noPushSbvgTj` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network13NWProtocolTCPC7OptionsC6noPushSbvsTj` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network13NWProtocolTCPC7OptionsC7noDelaySbvgTj` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network13NWProtocolTCPC7OptionsCMn` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network13NWProtocolTLSC7OptionsCMn` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network13NWProtocolUDPC7OptionsCAEycfc` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network13NWProtocolUDPC7OptionsCMa` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network13NWProtocolUDPC7OptionsCMn` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network17NWProtocolOptionsCMn` | 2 | iina, utm |
| libswiftNetwork.dylib | `_$s7Network18NWProtocolMetadataCMa` | 2 | iina, raycast |
| libswiftNetwork.dylib | `_$s7Network19NWProtocolWebSocketC6OpcodeO4pingyA2EmFWC` | 2 | iina, raycast |
| libswiftNetwork.dylib | `_$s7Network19NWProtocolWebSocketC6OpcodeO4pongyA2EmFWC` | 2 | iina, raycast |
| libswiftNetwork.dylib | `_$s7Network19NWProtocolWebSocketC6OpcodeO6binaryyA2EmFWC` | 2 | iina, raycast |
| libswiftNetwork.dylib | `_$s7Network19NWProtocolWebSocketC6OpcodeOMa` | 2 | iina, raycast |
| libswiftNetwork.dylib | `_$s7Network19NWProtocolWebSocketC6OpcodeOSQAAMc` | 2 | iina, raycast |
| libswiftNetwork.dylib | `_$s7Network19NWProtocolWebSocketC7OptionsC13autoReplyPingSbvsTj` | 2 | iina, raycast |
| libswiftNetwork.dylib | `_$s7Network19NWProtocolWebSocketC7OptionsCMa` | 2 | iina, raycast |
| libswiftNetwork.dylib | `_$s7Network19NWProtocolWebSocketC7OptionsCyAeC7VersionOcfc` | 2 | iina, raycast |
| libswiftNetwork.dylib | `_$s7Network19NWProtocolWebSocketC7VersionO9version13yA2EmFWC` | 2 | iina, raycast |
| libswiftNetwork.dylib | `_$s7Network19NWProtocolWebSocketC7VersionOMa` | 2 | iina, raycast |
| libswiftNetwork.dylib | `_$s7Network19NWProtocolWebSocketC8MetadataC6opcodeAC6OpcodeOvg` | 2 | iina, raycast |
| libswiftNetwork.dylib | `_$s7Network19NWProtocolWebSocketC8MetadataC6opcodeAeC6OpcodeO_tcfc` | 2 | iina, raycast |
| libswiftNetwork.dylib | `_$s7Network19NWProtocolWebSocketC8MetadataCMa` | 2 | iina, raycast |
| libswiftNetwork.dylib | `_$s7Network20NWProtocolDefinitionC2eeoiySbAC_ACtFZ` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network20NWProtocolDefinitionCMa` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network20NWProtocolDefinitionCMn` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network6NWPathV17UnsatisfiedReasonO05localA6DeniedyA2EmFWC` | 2 | sequel-ace, utm |
| libswiftNetwork.dylib | `_$s7Network6NWPathV2eeoiySbAC_ACtFZ` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network6NWPathV6StatusO18requiresConnectionyA2EmFWC` | 2 | raycast, sfm |
| libswiftNetwork.dylib | `_$s7Network6NWPathV8gatewaysSayAA10NWEndpointOGvg` | 2 | duckduckgo, sfm |
| libswiftNetwork.dylib | `_$s7Network9IPAddressMp` | 2 | duckduckgo, tailscale-app |
| libswiftNetwork.dylib | `_$s7Network9IPAddressP8rawValue10Foundation4DataVvgTj` | 2 | duckduckgo, tailscale-app |
| libswiftNetwork.dylib | `_$s7Network9NWBrowserCMn` | 2 | bettertouchtool, utm |
| CFNetwork.framework | `_CFHTTPAuthenticationCreateFromResponse` | 1 | adobe-acrobat-reader |
| CFNetwork.framework | `_CFHTTPAuthenticationRequiresUserNameAndPassword` | 1 | adobe-acrobat-reader |
| CFNetwork.framework | `_CFHTTPMessageAddAuthentication` | 1 | adobe-acrobat-reader |
| CFNetwork.framework | `_CFHTTPMessageApplyCredentialDictionary` | 1 | adobe-acrobat-reader |
| CFNetwork.framework | `_CFHTTPMessageGetTypeID` | 1 | adobe-acrobat-reader |
| CFNetwork.framework | `_CFHostGetReachability` | 1 | hammerspoon |
| CFNetwork.framework | `_CFNetDiagnosticCreateWithURL` | 1 | cyberduck |
| CFNetwork.framework | `_CFNetDiagnosticDiagnoseProblemInteractively` | 1 | cyberduck |
| CFNetwork.framework | `_CFNetServiceBrowserCreate` | 1 | bbedit |
| CFNetwork.framework | `_CFNetServiceBrowserInvalidate` | 1 | bbedit |
| CFNetwork.framework | `_CFNetServiceBrowserScheduleWithRunLoop` | 1 | bbedit |
| CFNetwork.framework | `_CFNetServiceBrowserSearchForServices` | 1 | bbedit |
| CFNetwork.framework | `_CFNetServiceBrowserUnscheduleFromRunLoop` | 1 | bbedit |
| CFNetwork.framework | `_CFNetServiceCancel` | 1 | bbedit |
| CFNetwork.framework | `_CFNetServiceCreate` | 1 | bbedit |
| CFNetwork.framework | `_CFNetServiceGetName` | 1 | bbedit |
| CFNetwork.framework | `_CFNetServiceGetPortNumber` | 1 | bbedit |
| CFNetwork.framework | `_CFNetServiceGetTargetHost` | 1 | bbedit |
| CFNetwork.framework | `_CFNetServiceResolve` | 1 | bbedit |
| CFNetwork.framework | `_CFNetServiceScheduleWithRunLoop` | 1 | bbedit |
| CFNetwork.framework | `_CFNetServiceSetClient` | 1 | bbedit |
| CFNetwork.framework | `_CFNetServiceUnscheduleFromRunLoop` | 1 | bbedit |
| CFNetwork.framework | `_CFReadStreamCreateForHTTPRequest` | 1 | adobe-acrobat-reader |
| CFNetwork.framework | `_CFReadStreamCreateForStreamedHTTPRequest` | 1 | adobe-acrobat-reader |
| CFNetwork.framework | `_NSHTTPCookieOriginURL` | 1 | protonvpn |
| CFNetwork.framework | `_kCFErrorDomainWinSock` | 1 | windows-app |
| CFNetwork.framework | `_kCFFTPResourceGroup` | 1 | bbedit |
| CFNetwork.framework | `_kCFFTPResourceLink` | 1 | bbedit |
| CFNetwork.framework | `_kCFFTPResourceModDate` | 1 | bbedit |
| CFNetwork.framework | `_kCFFTPResourceMode` | 1 | bbedit |
| CFNetwork.framework | `_kCFFTPResourceName` | 1 | bbedit |
| CFNetwork.framework | `_kCFFTPResourceOwner` | 1 | bbedit |
| CFNetwork.framework | `_kCFFTPResourceSize` | 1 | bbedit |
| CFNetwork.framework | `_kCFFTPResourceType` | 1 | bbedit |
| CFNetwork.framework | `_kCFHTTPAuthenticationPassword` | 1 | adobe-acrobat-reader |
| CFNetwork.framework | `_kCFHTTPAuthenticationSchemeNegotiate` | 1 | adobe-acrobat-reader |
| CFNetwork.framework | `_kCFHTTPAuthenticationUsername` | 1 | adobe-acrobat-reader |
| CFNetwork.framework | `_kCFStreamPropertyHTTPFinalURL` | 1 | adobe-acrobat-reader |
| CFNetwork.framework | `_kCFStreamPropertyHTTPProxy` | 1 | adobe-acrobat-reader |
| CFNetwork.framework | `_kCFStreamPropertyHTTPProxyHost` | 1 | adobe-acrobat-reader |
| CFNetwork.framework | `_kCFStreamPropertyHTTPProxyPort` | 1 | adobe-acrobat-reader |
| CFNetwork.framework | `_kCFStreamPropertyHTTPResponseHeader` | 1 | adobe-acrobat-reader |
| CFNetwork.framework | `_kCFStreamPropertyHTTPSProxyHost` | 1 | adobe-acrobat-reader |
| CFNetwork.framework | `_kCFStreamPropertyHTTPSProxyPort` | 1 | adobe-acrobat-reader |
| CFNetwork.framework | `_kCFStreamPropertyHTTPShouldAutoredirect` | 1 | adobe-acrobat-reader |
| GSS.framework | `___ApplePrivate_gss_krb5_get_tkt_flags` | 1 | pgadmin4 |
| GSS.framework | `___ApplePrivate_gss_krb5_import_cred` | 1 | pgadmin4 |
| GSS.framework | `___ApplePrivate_gss_release_iov_buffer` | 1 | pgadmin4 |
| GSS.framework | `___ApplePrivate_gss_unwrap_iov` | 1 | pgadmin4 |
| GSS.framework | `___ApplePrivate_gss_wrap_iov` | 1 | pgadmin4 |
| GSS.framework | `___ApplePrivate_gss_wrap_iov_length` | 1 | pgadmin4 |
| GSS.framework | `___ApplePrivate_gsskrb5_extract_authtime_from_sec_context` | 1 | pgadmin4 |
| GSS.framework | `___gss_c_nt_anonymous_oid_desc` | 1 | pgadmin4 |
| GSS.framework | `___gss_c_nt_export_name_oid_desc` | 1 | pgadmin4 |
| GSS.framework | `___gss_c_nt_machine_uid_name_oid_desc` | 1 | pgadmin4 |
| GSS.framework | `___gss_c_nt_string_uid_name_oid_desc` | 1 | pgadmin4 |
| GSS.framework | `___gss_krb5_nt_principal_name_oid_desc` | 1 | pgadmin4 |
| GSS.framework | `_gss_destroy_cred` | 1 | windows-app |
| GSS.framework | `_gss_inquire_cred` | 1 | windows-app |
| GSS.framework | `_gss_wrap_size_limit` | 1 | tableplus |
| IOKit.framework | `_SCPreferencesCreateWithAuthorization` | 1 | proxyman |
| IOKit.framework | `_SCPreferencesGetValue` | 1 | proxyman |
| Kerberos.framework | `_cc_initialize` | 1 | pgadmin4 |
| Kerberos.framework | `_gss_add_oid_set_member` | 1 | wireshark-app |
| Kerberos.framework | `_gss_create_empty_oid_set` | 1 | wireshark-app |
| Kerberos.framework | `_gss_inquire_cred` | 1 | wireshark-app |
| Kerberos.framework | `_gss_inquire_cred_by_mech` | 1 | wireshark-app |
| Kerberos.framework | `_gss_release_oid` | 1 | wireshark-app |
| Kerberos.framework | `_gss_test_oid_set_member` | 1 | wireshark-app |
| Kerberos.framework | `_gss_wrap_size_limit` | 1 | libreoffice |
| Kerberos.framework | `_krb5_c_decrypt` | 1 | wireshark-app |
| Kerberos.framework | `_krb5_kt_resolve` | 1 | wireshark-app |
| Network.framework | `_$s7Network10NWEndpointO16debugDescriptionSSvg` | 1 | protonvpn |
| Network.framework | `_$s7Network10NWEndpointO3urlyAC10Foundation3URLVcACmFWC` | 1 | windows-app |
| Network.framework | `_$s7Network10NWEndpointO4PortV16debugDescriptionSSvg` | 1 | proxyman |
| Network.framework | `_$s7Network10NWEndpointO9txtRecordAA11NWTXTRecordVSgvg` | 1 | openclaw |
| Network.framework | `_$s7Network10NWEndpointOSQAAMc` | 1 | cmux |
| Network.framework | `_$s7Network11IPv4AddressV9broadcastACvgZ` | 1 | windows-app |
| Network.framework | `_$s7Network11NWInterfaceV13InterfaceTypeOSQAAMc` | 1 | cmux |
| Network.framework | `_$s7Network11NWTXTRecordV10dictionarySDyS2SGvg` | 1 | openclaw |
| Network.framework | `_$s7Network11NWTXTRecordVMa` | 1 | openclaw |
| Network.framework | `_$s7Network11NWTXTRecordVMn` | 1 | openclaw |
| Network.framework | `_$s7Network12NWConnectionC14receiveMessage10completionyy10Foundation4DataVSg_AC14ContentContextCSgSbAA7NWErrorOSgtc_tF` | 1 | protonvpn |
| Network.framework | `_$s7Network12NWConnectionC19EstablishmentReportV9usedProxySbvg` | 1 | proxyman |
| Network.framework | `_$s7Network12NWParametersC14PrivacyContextC21ResolverConfigurationO3tlsyAgA10NWEndpointO_SayAJGtcAGmFWC` | 1 | macwhisper |
| Network.framework | `_$s7Network12NWParametersC14PrivacyContextC21ResolverConfigurationOMa` | 1 | macwhisper |
| Network.framework | `_$s7Network12NWParametersC14PrivacyContextC21ResolverConfigurationOMn` | 1 | macwhisper |
| Network.framework | `_$s7Network12NWParametersC14PrivacyContextC30requireEncryptedNameResolution_16fallbackResolverySb_AE0J13ConfigurationOSgtFTj` | 1 | macwhisper |
| Network.framework | `_$s7Network12NWParametersC14PrivacyContextC7defaultAEvgZ` | 1 | macwhisper |
| Network.framework | `_$s7Network12NWParametersC14PrivacyContextCMa` | 1 | macwhisper |
| Network.framework | `_$s7Network12NWParametersC20MultipathServiceTypeO8handoveryA2EmFWC` | 1 | orbstack |
| Network.framework | `_$s7Network13NWProtocolTLSC7OptionsCAEycfCTj` | 1 | orbstack |
| Network.framework | `_$s7Network18ProxyConfigurationV07socksv5B0AcA10NWEndpointO_tcfC` | 1 | cmux |
| Network.framework | `_$s7Network18ProxyConfigurationV12matchDomainsSaySSGvs` | 1 | cmux |
| Network.framework | `_$s7Network18ProxyConfigurationV13allowFailoverSbvs` | 1 | cmux |
| Network.framework | `_$s7Network18ProxyConfigurationV15applyCredential8username8passwordySS_SStF` | 1 | cmux |
| Network.framework | `_$s7Network18ProxyConfigurationV15excludedDomainsSaySSGvs` | 1 | cmux |
| Network.framework | `_$s7Network18ProxyConfigurationV16httpCONNECTProxy10tlsOptionsAcA10NWEndpointO_AA13NWProtocolTLSC0G0CSgtcfC` | 1 | cmux |
| Network.framework | `_$s7Network18ProxyConfigurationVMa` | 1 | cmux |
| Network.framework | `_$s7Network18ProxyConfigurationVMn` | 1 | cmux |
| Network.framework | `_$s7Network6NWPathV6StatusO18requiresConnectionyA2EmFWC` | 1 | protonvpn |
| Network.framework | `_$s7Network6NWPathV8gatewaysSayAA10NWEndpointOGvg` | 1 | cmux |
| Network.framework | `_$s7Network9NWBrowserC5StateOMn` | 1 | openclaw |
| Network.framework | `_$s7Network9NWBrowserC6ResultV6ChangeO5FlagsVMa` | 1 | protonvpn |
| Network.framework | `_$s7Network9NWBrowserC6ResultV6ChangeO5FlagsVMn` | 1 | protonvpn |
| Network.framework | `_$s7Network9NWBrowserC6ResultV6ChangeO5addedyAgEcAGmFWC` | 1 | protonvpn |
| Network.framework | `_$s7Network9NWBrowserC6ResultV6ChangeO7changedyAgE_AeG5FlagsVtcAGmFWC` | 1 | protonvpn |
| Network.framework | `_$s7Network9NWBrowserC6ResultV6ChangeO7removedyAgEcAGmFWC` | 1 | protonvpn |
| Network.framework | `_$s7Network9NWBrowserC6ResultV6ChangeOMa` | 1 | protonvpn |
| Network.framework | `_$s7Network9NWBrowserC6ResultV8MetadataO7bonjouryAgA11NWTXTRecordVcAGmFWC` | 1 | openclaw |
| Network.framework | `_$s7Network9NWBrowserC6ResultV8MetadataOMa` | 1 | openclaw |
| Network.framework | `_$s7Network9NWBrowserC6ResultV8metadataAE8MetadataOvg` | 1 | openclaw |
| Network.framework | `_$s7Network9NWBrowserC6ResultVSHAAMc` | 1 | openclaw |
| Network.framework | `_$s7Network9NWBrowserC6ResultVSQAAMc` | 1 | openclaw |
| Network.framework | `_$sSo25NSURLSessionConfigurationC7NetworkE19proxyConfigurationsSayAC05ProxyB0VGvs` | 1 | cmux |
| Network.framework | `_OBJC_CLASS_$_NWEndpoint` | 1 | duckduckgo |
| Network.framework | `_OBJC_CLASS_$_NWTCPConnection` | 1 | protonvpn |
| Network.framework | `__nw_content_context_default_stream` | 1 | sfm |
| Network.framework | `__nw_content_context_final_send` | 1 | bbedit |
| Network.framework | `_nw_advertise_descriptor_create_bonjour_service` | 1 | spotify |
| Network.framework | `_nw_advertise_descriptor_set_txt_record_object` | 1 | spotify |
| Network.framework | `_nw_browse_descriptor_set_include_txt_record` | 1 | spotify |
| Network.framework | `_nw_browse_result_copy_txt_record_object` | 1 | spotify |
| Network.framework | `_nw_connection_copy_current_path` | 1 | bbedit |
| Network.framework | `_nw_content_context_create` | 1 | bbedit |
| Network.framework | `_nw_content_context_set_metadata_for_protocol` | 1 | bbedit |
| Network.framework | `_nw_endpoint_get_address` | 1 | bbedit |
| Network.framework | `_nw_framer_copy_options` | 1 | dotnet-sdk |
| Network.framework | `_nw_framer_options_copy_object_value` | 1 | dotnet-sdk |
| Network.framework | `_nw_framer_options_set_object_value` | 1 | dotnet-sdk |
| Network.framework | `_nw_ip_create_metadata` | 1 | bbedit |
| Network.framework | `_nw_ip_metadata_set_service_class` | 1 | bbedit |
| Network.framework | `_nw_listener_set_advertise_descriptor` | 1 | spotify |
| Network.framework | `_nw_parameters_set_reuse_local_address` | 1 | bbedit |
| Network.framework | `_nw_parameters_set_service_class` | 1 | bbedit |
| Network.framework | `_nw_path_copy_effective_local_endpoint` | 1 | bbedit |
| Network.framework | `_nw_path_copy_effective_remote_endpoint` | 1 | bbedit |
| Network.framework | `_nw_protocol_stack_copy_transport_protocol` | 1 | bbedit |
| Network.framework | `_nw_tcp_options_set_enable_keepalive` | 1 | bbedit |
| Network.framework | `_nw_txt_record_access_key` | 1 | spotify |
| Network.framework | `_nw_txt_record_create_dictionary` | 1 | spotify |
| Network.framework | `_nw_txt_record_set_key` | 1 | spotify |
| NetworkExtension.framework | `_$s16NetworkExtension25NEAppProxyUDPFlowHandlingMp` | 1 | proxyman |
| NetworkExtension.framework | `_$s16NetworkExtension25NEAppProxyUDPFlowHandlingP09handleNewE0_25initialRemoteFlowEndpointSbSo0cdE0C_0A010NWEndpointOtFTq` | 1 | proxyman |
| NetworkExtension.framework | `_$sSo13NENetworkRuleC16NetworkExtensionE011destinationC8Endpoint6prefix8protocolAB0C010NWEndpointO_SiSo0aB8ProtocolVtcfC` | 1 | proxyman |
| NetworkExtension.framework | `_$sSo13NENetworkRuleC16NetworkExtensionE06remoteC8Endpoint0E6Prefix05localcF00hG08protocol9directionAB0C010NWEndpointOSg_SiAMSiSo0aB8ProtocolVSo18NETrafficDirectionVtcfC` | 1 | proxyman |
| NetworkExtension.framework | `_$sSo14NEAppProxyFlowC16NetworkExtensionE11setMetadata2ony0D012NWParametersC_tF` | 1 | proxyman |
| NetworkExtension.framework | `_$sSo14NEAppProxyFlowC16NetworkExtensionE4open09withLocalC8Endpoint17completionHandlery0D010NWEndpointOSg_ys5Error_pSgctF` | 1 | proxyman |
| NetworkExtension.framework | `_$sSo14NEAppProxyFlowC16NetworkExtensionE9interface0D011NWInterfaceVSgvs` | 1 | proxyman |
| NetworkExtension.framework | `_$sSo17NEAppProxyTCPFlowC16NetworkExtensionE18remoteFlowEndpoint0D010NWEndpointOvg` | 1 | proxyman |
| NetworkExtension.framework | `_NEVPNConnectionErrorDomain` | 1 | tailscale-app |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEAppProxyFlow` | 1 | duckduckgo |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEAppProxyUDPFlow` | 1 | protonvpn |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEDNSOverHTTPSSettings` | 1 | protonvpn |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEDNSOverTLSSettings` | 1 | protonvpn |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEFilterDataProvider` | 1 | lulu |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEFilterManager` | 1 | lulu |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEFilterNewFlowVerdict` | 1 | lulu |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEFilterProviderConfiguration` | 1 | lulu |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEFilterRule` | 1 | lulu |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEFilterSettings` | 1 | lulu |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEHotspotConfiguration` | 1 | whatsapp |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEHotspotConfigurationManager` | 1 | whatsapp |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEHotspotNetwork` | 1 | whatsapp |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEOnDemandRuleIgnore` | 1 | sfm |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEProxyServer` | 1 | sfm |
| NetworkExtension.framework | `_OBJC_METACLASS_$_NEFilterDataProvider` | 1 | lulu |
| SystemConfiguration.framework | `_SCDynamicStoreCopyLocation` | 1 | hammerspoon |
| SystemConfiguration.framework | `_SCNetworkConnectionCreateWithServiceID` | 1 | microsoft-teams |
| SystemConfiguration.framework | `_SCNetworkConnectionGetStatus` | 1 | microsoft-teams |
| SystemConfiguration.framework | `_SCNetworkInterfaceCopyMTU` | 1 | teamviewer |
| SystemConfiguration.framework | `_SCNetworkInterfaceGetSupportedInterfaceTypes` | 1 | teamviewer |
| SystemConfiguration.framework | `_SCNetworkInterfaceGetSupportedProtocolTypes` | 1 | teamviewer |
| SystemConfiguration.framework | `_SCNetworkProtocolGetConfiguration` | 1 | clash-verge-rev |
| SystemConfiguration.framework | `_SCNetworkProtocolSetConfiguration` | 1 | clash-verge-rev |
| SystemConfiguration.framework | `_SCNetworkServiceCopyProtocol` | 1 | clash-verge-rev |
| SystemConfiguration.framework | `_SCNetworkServiceGetTypeID` | 1 | microsoft-teams |
| SystemConfiguration.framework | `_SCNetworkSetCopyAll` | 1 | hammerspoon |
| SystemConfiguration.framework | `_SCNetworkSetGetName` | 1 | hammerspoon |
| SystemConfiguration.framework | `_SCNetworkSetGetSetID` | 1 | hammerspoon |
| SystemConfiguration.framework | `_SCNetworkSetSetCurrent` | 1 | hammerspoon |
| SystemConfiguration.framework | `_SCPreferencesApplyChanges` | 1 | clash-verge-rev |
| SystemConfiguration.framework | `_SCPreferencesCommitChanges` | 1 | clash-verge-rev |
| SystemConfiguration.framework | `_SCPreferencesCreateWithOptions` | 1 | hammerspoon |
| SystemConfiguration.framework | `_SCPreferencesLock` | 1 | clash-verge-rev |
| SystemConfiguration.framework | `_SCPreferencesPathSetValue` | 1 | proxyman |
| SystemConfiguration.framework | `_SCPreferencesSynchronize` | 1 | proxyman |
| SystemConfiguration.framework | `_SCPreferencesUnlock` | 1 | clash-verge-rev |
| SystemConfiguration.framework | `_kSCDynamicStorePropNetPrimaryService` | 1 | cloudflare-warp |
| SystemConfiguration.framework | `_kSCEntNetDHCP` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCEntNetEthernet` | 1 | cloudflare-warp |
| SystemConfiguration.framework | `_kSCEntNetLink` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCEntNetModem` | 1 | adobe-acrobat-reader |
| SystemConfiguration.framework | `_kSCNetworkInterfaceType6to4` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCNetworkInterfaceTypeBond` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCNetworkInterfaceTypeFireWire` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCNetworkInterfaceTypeIPv4` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCNetworkInterfaceTypeL2TP` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCNetworkInterfaceTypeSerial` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCNetworkInterfaceTypeVLAN` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCPrefNetworkServices` | 1 | proxyman |
| SystemConfiguration.framework | `_kSCPropInterfaceName` | 1 | cloudflare-warp |
| SystemConfiguration.framework | `_kSCPropNetDNSSearchDomains` | 1 | cloudflare-warp |
| SystemConfiguration.framework | `_kSCPropNetEthernetMediaOptions` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCPropNetIPv4BroadcastAddresses` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCPropNetIPv4ConfigMethod` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCPropNetIPv6ConfigMethod` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCPropNetIPv6Flags` | 1 | cloudflare-warp |
| SystemConfiguration.framework | `_kSCPropNetInterfaceDeviceName` | 1 | cloudflare-warp |
| SystemConfiguration.framework | `_kSCPropNetInterfaceHardware` | 1 | cloudflare-warp |
| SystemConfiguration.framework | `_kSCPropNetModemConnectSpeed` | 1 | adobe-acrobat-reader |
| SystemConfiguration.framework | `_kSCPropNetProxiesFTPPassive` | 1 | cyberduck |
| SystemConfiguration.framework | `_kSCPropNetServiceOrder` | 1 | cloudflare-warp |
| SystemConfiguration.framework | `_kSCValNetIPv4ConfigMethodAutomatic` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCValNetIPv4ConfigMethodBOOTP` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCValNetIPv4ConfigMethodDHCP` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCValNetIPv4ConfigMethodINFORM` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCValNetIPv4ConfigMethodLinkLocal` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCValNetIPv4ConfigMethodManual` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCValNetIPv4ConfigMethodPPP` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCValNetIPv6ConfigMethod6to4` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCValNetIPv6ConfigMethodAutomatic` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCValNetIPv6ConfigMethodLinkLocal` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCValNetIPv6ConfigMethodManual` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCValNetIPv6ConfigMethodRouterAdvertisement` | 1 | teamviewer |
| libSystem.B.dylib | `_DNSServiceAddRecord` | 1 | cyberduck |
| libSystem.B.dylib | `_DNSServiceEnumerateDomains` | 1 | cyberduck |
| libSystem.B.dylib | `_DNSServiceRemoveRecord` | 1 | cyberduck |
| libSystem.B.dylib | `_DNSServiceUpdateRecord` | 1 | cyberduck |
| libSystem.B.dylib | `_TXTRecordGetItemAtIndex` | 1 | zoom |
| libSystem.B.dylib | `_dns_configuration_copy` | 1 | emacs-app |
| libSystem.B.dylib | `_dns_configuration_free` | 1 | emacs-app |
| libSystem.B.dylib | `_dns_configuration_notify_key` | 1 | emacs-app |
| libresolv.9.dylib | `_dns_free` | 1 | pgadmin4 |
| libresolv.9.dylib | `_dns_open` | 1 | pgadmin4 |
| libresolv.9.dylib | `_dns_search` | 1 | pgadmin4 |
| libresolv.9.dylib | `_dns_search_list_domain` | 1 | pgadmin4 |
| libresolv.9.dylib | `_res_9_ns_name_uncompress` | 1 | pgadmin4 |
| libswiftNetwork.dylib | `_$s7Network10NWEndpointO16debugDescriptionSSvg` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network10NWEndpointO4HostOSHAAMc` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network10NWEndpointO4PortV5httpsAEvgZ` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network10NWEndpointO4PortVSHAAMc` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network10NWEndpointO4PortVSQAAMc` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network10NWEndpointO4PortVyAESgSScfC` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network10NWEndpointOSHAAMc` | 1 | bettertouchtool |
| libswiftNetwork.dylib | `_$s7Network10NWListenerC7ServiceV4name4type6domain9txtRecordAESSSg_SSAjA11NWTXTRecordVtcfC` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network11IPv4AddressV10isLoopbackSbvg` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network11IPv4AddressV11isLinkLocalSbvg` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network11IPv4AddressV11isMulticastSbvg` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network11IPv4AddressV2eeoiySbAC_ACtFZ` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network11IPv4AddressV3anyACvgZ` | 1 | tableplus |
| libswiftNetwork.dylib | `_$s7Network11IPv4AddressVSHAAMc` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network11IPv4AddressVSQAAMc` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network11IPv6AddressV10isLoopbackSbvg` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network11IPv6AddressV11isLinkLocalSbvg` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network11IPv6AddressV11isMulticastSbvg` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network11IPv6AddressV12isIPv4MappedSbvg` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network11IPv6AddressV2eeoiySbAC_ACtFZ` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network11IPv6AddressV3anyACvgZ` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network11IPv6AddressV5isAnySbvg` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network11IPv6AddressV6asIPv4AA0eC0VSgvg` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network11IPv6AddressV8loopbackACvgZ` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network11IPv6AddressVSHAAMc` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network11IPv6AddressVSQAAMc` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network11NWInterfaceV5indexSivg` | 1 | sfm |
| libswiftNetwork.dylib | `_$s7Network11NWTXTRecordVMa` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network11NWTXTRecordVMn` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network11NWTXTRecordVyACSDyS2SGcfC` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network12NWConnectionC4send20discontiguousContent14contentContext10isComplete10completiony8Dispatch0K4DataV_AC0eG0CSbAC14SendCompletionOtF` | 1 | raycast |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC12ServiceClassO16interactiveVideoyA2EmFWC` | 1 | bettertouchtool |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC12ServiceClassOMa` | 1 | bettertouchtool |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC12serviceClassAC07ServiceD0Ovs` | 1 | bettertouchtool |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC13ProtocolStackC08internetC0AA17NWProtocolOptionsCSgvgTj` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC13ProtocolStackC09transportC0AA17NWProtocolOptionsCSgvgTj` | 1 | bettertouchtool |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC13ProtocolStackC09transportC0AA17NWProtocolOptionsCSgvsTj` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC20MultipathServiceTypeO8handoveryA2EmFWC` | 1 | easydict |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC20prohibitedInterfacesSayAA11NWInterfaceVGSgvs` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC3udpACvgZ` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network12NWProtocolIPC7OptionsC7VersionO2v4yA2GmFWC` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network12NWProtocolIPC7OptionsC7VersionO2v6yA2GmFWC` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network12NWProtocolIPC7OptionsC7VersionOMa` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network12NWProtocolIPC7OptionsC7versionAE7VersionOvsTj` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network12NWProtocolIPC7OptionsCMa` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network13NWPathMonitorC21requiredInterfaceTypeAcA11NWInterfaceV0eF0O_tcfCTj` | 1 | raycast |
| libswiftNetwork.dylib | `_$s7Network13NWPathMonitorC24prohibitedInterfaceTypesACSayAA11NWInterfaceV0E4TypeOG_tcfCTj` | 1 | raycast |
| libswiftNetwork.dylib | `_$s7Network16NWProtocolFramerC10DefinitionC14implementationAeA0bC14Implementation_pXp_tcfc` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network16NWProtocolFramerC10DefinitionCMa` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network16NWProtocolFramerC11StartResultO5readyyA2EmFWC` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network16NWProtocolFramerC11StartResultOMa` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network16NWProtocolFramerC7MessageC10definitionAeC10DefinitionC_tcfc` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network16NWProtocolFramerC7MessageCMa` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network16NWProtocolFramerC7OptionsC10definitionAeC10DefinitionC_tcfc` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network16NWProtocolFramerC7OptionsCMa` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network16NWProtocolFramerC8InstanceC10parseInput23minimumIncompleteLength07maximumI00E0SbSi_S2iSwSg_SbtXEtF` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network16NWProtocolFramerC8InstanceC11writeOutput4datay10Foundation4DataV_tF` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network16NWProtocolFramerC8InstanceC17writeOutputNoCopy6lengthySi_tKF` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network16NWProtocolFramerC8InstanceC18deliverInputNoCopy6length7message10isCompleteSbSi_AC7MessageCSbtF` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network19NWProtocolWebSocketC6OpcodeOMn` | 1 | raycast |
| libswiftNetwork.dylib | `_$s7Network19NWProtocolWebSocketC8MetadataCMn` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network30NWProtocolFramerImplementationMp` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network30NWProtocolFramerImplementationP11handleInput6framerSiAA0bC0C8InstanceC_tFTq` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network30NWProtocolFramerImplementationP12handleOutput6framer7message0H6Length10isCompleteyAA0bC0C8InstanceC_AJ7MessageCSiSbtFTq` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network30NWProtocolFramerImplementationP4stop6framerSbAA0bC0C8InstanceC_tFTq` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network30NWProtocolFramerImplementationP5labelSSvgZTq` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network30NWProtocolFramerImplementationP5start6framerAA0bC0C11StartResultOAG8InstanceC_tFTq` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network30NWProtocolFramerImplementationP6framerxAA0bC0C8InstanceC_tcfCTq` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network30NWProtocolFramerImplementationP6wakeup6frameryAA0bC0C8InstanceC_tFTq` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network30NWProtocolFramerImplementationP7cleanup6frameryAA0bC0C8InstanceC_tFTq` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network6NWPathV11supportsDNSSbvg` | 1 | sfm |
| libswiftNetwork.dylib | `_$s7Network6NWPathV12supportsIPv4Sbvg` | 1 | sfm |
| libswiftNetwork.dylib | `_$s7Network6NWPathV12supportsIPv6Sbvg` | 1 | sfm |
| libswiftNetwork.dylib | `_$s7Network6NWPathV16debugDescriptionSSvg` | 1 | raycast |
| libswiftNetwork.dylib | `_$s7Network6NWPathV17UnsatisfiedReasonOMn` | 1 | sequel-ace |
| libswiftNetwork.dylib | `_$s7Network6NWPathV17UnsatisfiedReasonOSQAAMc` | 1 | sequel-ace |
| libswiftNetwork.dylib | `_$s7Network6NWPathV6StatusOMn` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network7NWErrorOs28CustomDebugStringConvertibleAAMc` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network9IPAddressP10isLoopbackSbvgTj` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network9IPAddressP11isLinkLocalSbvgTj` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network9IPAddressP11isMulticastSbvgTj` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network9IPAddressP9interfaceAA11NWInterfaceVSgvgTj` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network9IPAddressPyxSg10Foundation4DataV_AA11NWInterfaceVSgtcfCTj` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network9NWBrowserC10DescriptorO20bonjourWithTXTRecordyAESS_SSSgtcAEmFWC` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network9NWBrowserC13browseResultsShyAC6ResultVGvg` | 1 | utm |
