# Lacunas de Patricia (rede para apps)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Patricia (rede para apps) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**398** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| CFNetwork.framework | `_kCFProxyPasswordKey` | 15 | 1password, audacity, db-browser-for-sqlite, hammerspoon |
| CFNetwork.framework | `_kCFProxyUsernameKey` | 15 | 1password, audacity, db-browser-for-sqlite, hammerspoon |
| CFNetwork.framework | `_kCFProxyTypeFTP` | 12 | audacity, db-browser-for-sqlite, keepassxc, krita |
| SystemConfiguration.framework | `_kSCPropNetProxiesGopherEnable` | 11 | android-studio, blender, deluge, gimp |
| SystemConfiguration.framework | `_kSCPropNetProxiesGopherPort` | 11 | android-studio, blender, deluge, gimp |
| SystemConfiguration.framework | `_kSCPropNetProxiesGopherProxy` | 11 | android-studio, blender, deluge, gimp |
| CFNetwork.framework | `_kCFNetworkProxiesSOCKSPort` | 9 | audacity, cmux, keepassxc, musescore |
| CFNetwork.framework | `_kCFNetworkProxiesSOCKSProxy` | 9 | audacity, cmux, keepassxc, musescore |
| SystemConfiguration.framework | `_kSCCompAnyRegex` | 9 | firefox, floorp, librewolf, mullvad-browser |
| CFNetwork.framework | `_kCFNetworkProxiesExcludeSimpleHostnames` | 8 | audacity, cmux, keepassxc, musescore |
| Network.framework | `_nw_release` | 8 | audacity, dotnet-sdk, git-credential-manager, github |
| CFNetwork.framework | `_kCFNetworkProxiesFTPEnable` | 7 | audacity, keepassxc, musescore, obs |
| CFNetwork.framework | `_kCFNetworkProxiesFTPPort` | 7 | audacity, keepassxc, musescore, obs |
| CFNetwork.framework | `_kCFNetworkProxiesFTPProxy` | 7 | audacity, keepassxc, musescore, obs |
| SystemConfiguration.framework | `_kSCDynamicStoreDomainSetup` | 7 | firefox, floorp, librewolf, mullvad-browser |
| SystemConfiguration.framework | `_kSCEntNetDNS` | 7 | firefox, floorp, librewolf, mullvad-browser |
| libresolv.9.dylib | `_res_9_nmkquery` | 6 | audacity, keepassxc, musescore, obs |
| libresolv.9.dylib | `_res_9_nsend` | 6 | audacity, keepassxc, musescore, obs |
| libresolv.9.dylib | `_res_9_setservers` | 6 | audacity, keepassxc, musescore, obs |
| Network.framework | `_nw_path_monitor_set_cancel_handler` | 5 | audacity, keepassxc, musescore, qbittorrent |
| SystemConfiguration.framework | `_SCNetworkInterfaceGetInterfaceType` | 5 | cmux, rustdesk, stats, wireshark-app |
| SystemConfiguration.framework | `_SCNetworkInterfaceGetLocalizedDisplayName` | 5 | cmux, stats, vorssaint, wireshark-app |
| Kerberos.framework | `_com_err_va` | 4 | android-studio, cyberduck, dbeaver-community, temurin |
| Kerberos.framework | `_krb5_cc_default` | 4 | android-studio, cyberduck, dbeaver-community, temurin |
| Kerberos.framework | `_krb5_cc_end_seq_get` | 4 | android-studio, cyberduck, dbeaver-community, temurin |
| Kerberos.framework | `_krb5_cc_next_cred` | 4 | android-studio, cyberduck, dbeaver-community, temurin |
| Kerberos.framework | `_krb5_cc_set_flags` | 4 | android-studio, cyberduck, dbeaver-community, temurin |
| Kerberos.framework | `_krb5_cc_start_seq_get` | 4 | android-studio, cyberduck, dbeaver-community, temurin |
| IOKit.framework | `_SCPreferencesCreate` | 3 | hammerspoon, stats, tailscale-app |
| Network.framework | `_$s7Network10NWEndpointO4PortV8rawValues6UInt16Vvg` | 3 | cmux, openclaw, orbstack |
| Network.framework | `_$s7Network10NWListenerC4portAA10NWEndpointO4PortVSgvg` | 3 | cmux, openclaw, orbstack |
| Network.framework | `_$s7Network10NWListenerCMn` | 3 | cmux, openclaw, orbstack |
| Network.framework | `_$s7Network11IPv4AddressV8rawValue10Foundation4DataVvg` | 3 | cmux, openclaw, orbstack |
| Network.framework | `_$s7Network11IPv6AddressV8rawValue10Foundation4DataVvg` | 3 | cmux, openclaw, orbstack |
| Network.framework | `_$s7Network11IPv6AddressVMa` | 3 | cmux, openclaw, orbstack |
| Network.framework | `_$s7Network11IPv6AddressVMn` | 3 | cmux, openclaw, orbstack |
| Network.framework | `_nw_connection_copy_protocol_metadata` | 3 | dotnet-sdk, git-credential-manager, github |
| Network.framework | `_nw_framer_async` | 3 | dotnet-sdk, git-credential-manager, github |
| Network.framework | `_nw_framer_create_definition` | 3 | dotnet-sdk, git-credential-manager, github |
| Network.framework | `_nw_framer_create_options` | 3 | dotnet-sdk, git-credential-manager, github |
| Network.framework | `_nw_framer_deliver_input` | 3 | dotnet-sdk, git-credential-manager, github |
| Network.framework | `_nw_framer_message_create` | 3 | dotnet-sdk, git-credential-manager, github |
| Network.framework | `_nw_framer_parse_output` | 3 | dotnet-sdk, git-credential-manager, github |
| Network.framework | `_nw_framer_set_cleanup_handler` | 3 | dotnet-sdk, git-credential-manager, github |
| Network.framework | `_nw_framer_set_output_handler` | 3 | dotnet-sdk, git-credential-manager, github |
| Network.framework | `_nw_framer_set_stop_handler` | 3 | dotnet-sdk, git-credential-manager, github |
| Network.framework | `_nw_parameters_copy_default_protocol_stack` | 3 | dotnet-sdk, git-credential-manager, github |
| Network.framework | `_nw_parameters_create_secure_udp` | 3 | dotnet-sdk, git-credential-manager, github |
| Network.framework | `_nw_protocol_copy_tls_definition` | 3 | dotnet-sdk, git-credential-manager, github |
| Network.framework | `_nw_protocol_stack_prepend_application_protocol` | 3 | dotnet-sdk, git-credential-manager, github |
| Network.framework | `_nw_tls_copy_sec_protocol_metadata` | 3 | dotnet-sdk, git-credential-manager, github |
| Network.framework | `_nw_tls_create_options` | 3 | dotnet-sdk, git-credential-manager, github |
| NetworkExtension.framework | `_NEVPNStatusDidChangeNotification` | 3 | brave-browser, cmux, tailscale-app |
| SystemConfiguration.framework | `_SCNetworkServiceGetInterface` | 3 | stats, tailscale-app, zoom |
| SystemConfiguration.framework | `_kSCNetworkInterfaceTypeBluetooth` | 3 | stats, wireshark-app, zoom |
| SystemConfiguration.framework | `_kSCNetworkInterfaceTypeIEEE80211` | 3 | stats, wireshark-app, zoom |
| SystemConfiguration.framework | `_kSCNetworkInterfaceTypeWWAN` | 3 | stats, wireshark-app, zoom |
| libSystem.B.dylib | `_DNSServiceBrowse` | 3 | cmux, cyberduck, zoom |
| libSystem.B.dylib | `_DNSServiceResolve` | 3 | cmux, cyberduck, zoom |
| libSystem.B.dylib | `_DNSServiceSetDispatchQueue` | 3 | cmux, spotify, utm |
| libresolv.9.dylib | `_res_9_init` | 3 | deluge, dolphin, microsoft-edge |
| libresolv.9.dylib | `_res_9_ns_initparse` | 3 | microsoft-edge, pgadmin4, sequel-ace |
| libresolv.9.dylib | `_res_9_ns_parserr` | 3 | microsoft-edge, pgadmin4, sequel-ace |
| libswiftNetwork.dylib | `_$s7Network10NWListenerCMn` | 3 | iina, raycast, sequel-ace |
| libswiftNetwork.dylib | `_$s7Network12NWConnectionC14receiveMessage10completionyy10Foundation4DataVSg_AC14ContentContextCSgSbAA7NWErrorOSgtc_tF` | 3 | iina, raycast, utm |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC13ProtocolStackC20applicationProtocolsSayAA17NWProtocolOptionsCGvMTj` | 3 | iina, raycast, utm |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC20defaultProtocolStackAC0dE0Cvg` | 3 | iina, raycast, utm |
| libswiftNetwork.dylib | `_$s7Network17NWProtocolOptionsCMa` | 3 | iina, raycast, utm |
| CFNetwork.framework | `_CFHTTPMessageAppendBytes` | 2 | hammerspoon, raycast |
| CFNetwork.framework | `_CFHTTPMessageCopyAllHeaderFields` | 2 | hammerspoon, raycast |
| CFNetwork.framework | `_CFHTTPMessageCopyRequestMethod` | 2 | hammerspoon, raycast |
| CFNetwork.framework | `_CFHTTPMessageCopySerializedMessage` | 2 | hammerspoon, raycast |
| CFNetwork.framework | `_CFHTTPMessageCreateEmpty` | 2 | hammerspoon, raycast |
| CFNetwork.framework | `_CFHTTPMessageCreateRequest` | 2 | hammerspoon, raycast |
| CFNetwork.framework | `_CFHTTPMessageCreateResponse` | 2 | hammerspoon, raycast |
| CFNetwork.framework | `_CFHTTPMessageGetResponseStatusCode` | 2 | hammerspoon, raycast |
| CFNetwork.framework | `_CFHTTPMessageIsHeaderComplete` | 2 | hammerspoon, raycast |
| CFNetwork.framework | `_CFHTTPMessageSetBody` | 2 | hammerspoon, raycast |
| CFNetwork.framework | `_CFHTTPMessageSetHeaderFieldValue` | 2 | hammerspoon, raycast |
| CFNetwork.framework | `_kCFHTTPVersion1_1` | 2 | hammerspoon, raycast |
| CFNetwork.framework | `_kCFStreamPropertySSLPeerTrust` | 2 | hammerspoon, raycast |
| Network.framework | `_$s7Network10NWEndpointO4HostO2eeoiySbAE_AEtFZ` | 2 | cmux, orbstack |
| Network.framework | `_$s7Network10NWEndpointO4HostO4ipv6yAeA11IPv6AddressVcAEmFWC` | 2 | cmux, orbstack |
| Network.framework | `_$s7Network10NWEndpointO4unixyACSS_tcACmFWC` | 2 | cmux, orbstack |
| Network.framework | `_$s7Network11IPv4AddressVyACSg10Foundation4DataV_AA11NWInterfaceVSgtcfC` | 2 | cmux, orbstack |
| Network.framework | `_$s7Network11IPv6AddressVyACSg10Foundation4DataV_AA11NWInterfaceVSgtcfC` | 2 | cmux, orbstack |
| Network.framework | `_$s7Network11IPv6AddressVyACSgSScfC` | 2 | cmux, openclaw |
| Network.framework | `_$s7Network12NWConnectionC10parametersAA12NWParametersCvg` | 2 | cmux, orbstack |
| Network.framework | `_$s7Network12NWConnectionC14ContentContextC12finalMessageAEvgZ` | 2 | cmux, orbstack |
| Network.framework | `_$s7Network12NWConnectionC17pathUpdateHandleryAA6NWPathVcSgvs` | 2 | cmux, orbstack |
| Network.framework | `_$s7Network12NWParametersC21requiredLocalEndpointAA10NWEndpointOSgvs` | 2 | cmux, orbstack |
| Network.framework | `_$s7Network12NWParametersC23allowLocalEndpointReuseSbvs` | 2 | cmux, orbstack |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC7noDelaySbvsTj` | 2 | cmux, orbstack |
| Network.framework | `_nw_listener_cancel` | 2 | dotnet-sdk, spotify |
| Network.framework | `_nw_listener_create` | 2 | dotnet-sdk, spotify |
| Network.framework | `_nw_listener_set_new_connection_handler` | 2 | dotnet-sdk, spotify |
| Network.framework | `_nw_listener_set_queue` | 2 | dotnet-sdk, spotify |
| Network.framework | `_nw_listener_start` | 2 | dotnet-sdk, spotify |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEOnDemandRuleConnect` | 2 | brave-browser, tailscale-app |
| NetworkExtension.framework | `_OBJC_CLASS_$_NETunnelProviderManager` | 2 | cmux, tailscale-app |
| NetworkExtension.framework | `_OBJC_CLASS_$_NETunnelProviderProtocol` | 2 | cmux, tailscale-app |
| SystemConfiguration.framework | `_SCDynamicStoreCopyDHCPInfo` | 2 | hammerspoon, tailscale-app |
| SystemConfiguration.framework | `_SCNetworkServiceCopyAll` | 2 | stats, tailscale-app |
| SystemConfiguration.framework | `_SCNetworkServiceGetServiceID` | 2 | stats, tailscale-app |
| SystemConfiguration.framework | `_kSCNetworkInterfaceTypeEthernet` | 2 | stats, zoom |
| SystemConfiguration.framework | `_kSCNetworkInterfaceTypePPP` | 2 | wireshark-app, zoom |
| libSystem.B.dylib | `_DNSServiceConstructFullName` | 2 | cyberduck, zoom |
| libSystem.B.dylib | `_DNSServiceRegisterRecord` | 2 | cmux, cyberduck |
| libSystem.B.dylib | `_TXTRecordCreate` | 2 | spotify, zoom |
| libSystem.B.dylib | `_TXTRecordDeallocate` | 2 | spotify, zoom |
| libSystem.B.dylib | `_TXTRecordGetBytesPtr` | 2 | spotify, zoom |
| libSystem.B.dylib | `_TXTRecordGetLength` | 2 | spotify, zoom |
| libSystem.B.dylib | `_TXTRecordSetValue` | 2 | spotify, zoom |
| libswiftNetwork.dylib | `_$s7Network11IPv6AddressVMa` | 2 | tailscale-app, utm |
| libswiftNetwork.dylib | `_$s7Network12NWConnectionC14ContentContextC16protocolMetadataSayAA010NWProtocolF0CGvgTj` | 2 | iina, raycast |
| libswiftNetwork.dylib | `_$s7Network12NWConnectionC14SendCompletionO10idempotentyA2EmFWC` | 2 | iina, raycast |
| libswiftNetwork.dylib | `_$s7Network12NWConnectionC16debugDescriptionSSvg` | 2 | iina, raycast |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC21requiredLocalEndpointAA10NWEndpointOSgvs` | 2 | raycast, sequel-ace |
| libswiftNetwork.dylib | `_$s7Network13NWPathMonitorC11currentPathAA0B0Vvg` | 2 | raycast, tailscale-app |
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
| libswiftNetwork.dylib | `_$s7Network6NWPathV17UnsatisfiedReasonO05localA6DeniedyA2EmFWC` | 2 | sequel-ace, utm |
| CFNetwork.framework | `_CFHTTPMessageCopyBody` | 1 | hammerspoon |
| CFNetwork.framework | `_CFHTTPMessageCopyHeaderFieldValue` | 1 | hammerspoon |
| CFNetwork.framework | `_CFHTTPMessageCopyRequestURL` | 1 | hammerspoon |
| CFNetwork.framework | `_CFHTTPMessageCopyResponseStatusLine` | 1 | hammerspoon |
| CFNetwork.framework | `_CFHTTPMessageCopyVersion` | 1 | hammerspoon |
| CFNetwork.framework | `_CFHostCreateWithAddress` | 1 | hammerspoon |
| CFNetwork.framework | `_CFHostGetReachability` | 1 | hammerspoon |
| CFNetwork.framework | `_CFNetDiagnosticCreateWithURL` | 1 | cyberduck |
| CFNetwork.framework | `_CFNetDiagnosticDiagnoseProblemInteractively` | 1 | cyberduck |
| CFNetwork.framework | `_kCFHTTPVersion1_0` | 1 | hammerspoon |
| CFNetwork.framework | `_kCFStreamErrorDomainFTP` | 1 | hammerspoon |
| CFNetwork.framework | `_kCFStreamErrorDomainHTTP` | 1 | hammerspoon |
| CFNetwork.framework | `_kCFStreamErrorDomainMach` | 1 | hammerspoon |
| CFNetwork.framework | `_kCFStreamErrorDomainSystemConfiguration` | 1 | hammerspoon |
| CFNetwork.framework | `_kCFStreamErrorDomainWinSock` | 1 | hammerspoon |
| CFNetwork.framework | `_kCFStreamPropertySSLContext` | 1 | raycast |
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
| IOKit.framework | `_SCPreferencesApplyChanges` | 1 | hammerspoon |
| IOKit.framework | `_SCPreferencesCommitChanges` | 1 | hammerspoon |
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
| Network.framework | `_$s7Network10NWEndpointO2eeoiySbAC_ACtFZ` | 1 | orbstack |
| Network.framework | `_$s7Network10NWEndpointO4HostO4nameyAESS_AA11NWInterfaceVSgtcAEmFWC` | 1 | cmux |
| Network.framework | `_$s7Network10NWEndpointO4HostOSHAAMc` | 1 | cmux |
| Network.framework | `_$s7Network10NWEndpointO4PortVSHAAMc` | 1 | cmux |
| Network.framework | `_$s7Network10NWEndpointO4PortVSQAAMc` | 1 | cmux |
| Network.framework | `_$s7Network10NWEndpointO4PortVyAESgSScfC` | 1 | cmux |
| Network.framework | `_$s7Network10NWEndpointO9txtRecordAA11NWTXTRecordVSgvg` | 1 | openclaw |
| Network.framework | `_$s7Network10NWEndpointOSQAAMc` | 1 | cmux |
| Network.framework | `_$s7Network10NWListenerC10parametersAA12NWParametersCvg` | 1 | orbstack |
| Network.framework | `_$s7Network10NWListenerC5StateOMn` | 1 | cmux |
| Network.framework | `_$s7Network10NWListenerC5stateAC5StateOvg` | 1 | orbstack |
| Network.framework | `_$s7Network11IPv4AddressV16debugDescriptionSSvg` | 1 | orbstack |
| Network.framework | `_$s7Network11IPv4AddressVAA9IPAddressAAMc` | 1 | cmux |
| Network.framework | `_$s7Network11IPv4AddressVSQAAMc` | 1 | cmux |
| Network.framework | `_$s7Network11IPv6AddressV16debugDescriptionSSvg` | 1 | orbstack |
| Network.framework | `_$s7Network11IPv6AddressVAA9IPAddressAAMc` | 1 | cmux |
| Network.framework | `_$s7Network11IPv6AddressVSQAAMc` | 1 | cmux |
| Network.framework | `_$s7Network11NWInterfaceV13InterfaceTypeOSQAAMc` | 1 | cmux |
| Network.framework | `_$s7Network11NWInterfaceV5indexSivg` | 1 | cmux |
| Network.framework | `_$s7Network11NWTXTRecordV10dictionarySDyS2SGvg` | 1 | openclaw |
| Network.framework | `_$s7Network11NWTXTRecordVMa` | 1 | openclaw |
| Network.framework | `_$s7Network11NWTXTRecordVMn` | 1 | openclaw |
| Network.framework | `_$s7Network12NWConnectionC19EstablishmentReportVMa` | 1 | orbstack |
| Network.framework | `_$s7Network12NWConnectionC19EstablishmentReportVMn` | 1 | orbstack |
| Network.framework | `_$s7Network12NWConnectionC23startDataTransferReportAC07PendingdeF0CyF` | 1 | orbstack |
| Network.framework | `_$s7Network12NWConnectionC25PendingDataTransferReportCMa` | 1 | orbstack |
| Network.framework | `_$s7Network12NWConnectionC25PendingDataTransferReportCMn` | 1 | orbstack |
| Network.framework | `_$s7Network12NWConnectionC26requestEstablishmentReport5queue10completionySo012OS_dispatch_F0C_yAC0dE0VSgctF` | 1 | orbstack |
| Network.framework | `_$s7Network12NWConnectionC5batchyyyyXEF` | 1 | orbstack |
| Network.framework | `_$s7Network12NWConnectionC8metadata10definitionAA18NWProtocolMetadataCSgAA0E10DefinitionC_tF` | 1 | orbstack |
| Network.framework | `_$s7Network12NWParametersC17requiredInterfaceAA11NWInterfaceVSgvg` | 1 | cmux |
| Network.framework | `_$s7Network12NWParametersC20MultipathServiceTypeO8disabledyA2EmFWC` | 1 | orbstack |
| Network.framework | `_$s7Network12NWParametersC20MultipathServiceTypeO8handoveryA2EmFWC` | 1 | orbstack |
| Network.framework | `_$s7Network12NWParametersC20MultipathServiceTypeOMa` | 1 | orbstack |
| Network.framework | `_$s7Network12NWParametersC20MultipathServiceTypeOMn` | 1 | orbstack |
| Network.framework | `_$s7Network12NWParametersC20multipathServiceTypeAC09MultipathdE0Ovs` | 1 | orbstack |
| Network.framework | `_$s7Network12NWParametersC21requiredLocalEndpointAA10NWEndpointOSgvg` | 1 | orbstack |
| Network.framework | `_$s7Network12NWParametersC4dtls3udpAcA13NWProtocolTLSC7OptionsCSg_AA0E3UDPCAHCtcfC` | 1 | orbstack |
| Network.framework | `_$s7Network12NWParametersCMn` | 1 | orbstack |
| Network.framework | `_$s7Network13NWPathMonitorC11currentPathAA0B0Vvg` | 1 | cmux |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC02noD0SbvgTj` | 1 | orbstack |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC02noD0SbvsTj` | 1 | orbstack |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC13keepaliveIdleSivgTj` | 1 | orbstack |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC14keepaliveCountSivgTj` | 1 | orbstack |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC15enableKeepaliveSbvgTj` | 1 | orbstack |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC17connectionTimeoutSivgTj` | 1 | orbstack |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC17keepaliveIntervalSivgTj` | 1 | orbstack |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC17retransmitFinDropSbvgTj` | 1 | orbstack |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC18connectionDropTimeSivgTj` | 1 | orbstack |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC18maximumSegmentSizeSivgTj` | 1 | orbstack |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC18maximumSegmentSizeSivsTj` | 1 | orbstack |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC20disableAckStretchingSbvgTj` | 1 | orbstack |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC20disableAckStretchingSbvsTj` | 1 | orbstack |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC6noPushSbvgTj` | 1 | orbstack |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC6noPushSbvsTj` | 1 | orbstack |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC7noDelaySbvgTj` | 1 | orbstack |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsCMn` | 1 | orbstack |
| Network.framework | `_$s7Network13NWProtocolTLSC10definitionAA0B10DefinitionCvgZ` | 1 | orbstack |
| Network.framework | `_$s7Network13NWProtocolTLSC7OptionsCAEycfCTj` | 1 | orbstack |
| Network.framework | `_$s7Network13NWProtocolTLSC7OptionsCMn` | 1 | orbstack |
| Network.framework | `_$s7Network13NWProtocolTLSC8MetadataC016securityProtocolD0So24OS_sec_protocol_metadata_pvgTj` | 1 | orbstack |
| Network.framework | `_$s7Network13NWProtocolTLSC8MetadataCMa` | 1 | orbstack |
| Network.framework | `_$s7Network13NWProtocolTLSCMa` | 1 | orbstack |
| Network.framework | `_$s7Network13NWProtocolUDPC7OptionsCAEycfc` | 1 | orbstack |
| Network.framework | `_$s7Network13NWProtocolUDPC7OptionsCMa` | 1 | orbstack |
| Network.framework | `_$s7Network13NWProtocolUDPC7OptionsCMn` | 1 | orbstack |
| Network.framework | `_$s7Network18NWProtocolMetadataCMn` | 1 | orbstack |
| Network.framework | `_$s7Network18ProxyConfigurationV07socksv5B0AcA10NWEndpointO_tcfC` | 1 | cmux |
| Network.framework | `_$s7Network18ProxyConfigurationV12matchDomainsSaySSGvs` | 1 | cmux |
| Network.framework | `_$s7Network18ProxyConfigurationV13allowFailoverSbvs` | 1 | cmux |
| Network.framework | `_$s7Network18ProxyConfigurationV15applyCredential8username8passwordySS_SStF` | 1 | cmux |
| Network.framework | `_$s7Network18ProxyConfigurationV15excludedDomainsSaySSGvs` | 1 | cmux |
| Network.framework | `_$s7Network18ProxyConfigurationV16httpCONNECTProxy10tlsOptionsAcA10NWEndpointO_AA13NWProtocolTLSC0G0CSgtcfC` | 1 | cmux |
| Network.framework | `_$s7Network18ProxyConfigurationVMa` | 1 | cmux |
| Network.framework | `_$s7Network18ProxyConfigurationVMn` | 1 | cmux |
| Network.framework | `_$s7Network20NWProtocolDefinitionC2eeoiySbAC_ACtFZ` | 1 | orbstack |
| Network.framework | `_$s7Network20NWProtocolDefinitionCMa` | 1 | orbstack |
| Network.framework | `_$s7Network20NWProtocolDefinitionCMn` | 1 | orbstack |
| Network.framework | `_$s7Network6NWPathV17usesInterfaceTypeySbAA11NWInterfaceV0dE0OF` | 1 | cmux |
| Network.framework | `_$s7Network6NWPathV2eeoiySbAC_ACtFZ` | 1 | orbstack |
| Network.framework | `_$s7Network6NWPathV8gatewaysSayAA10NWEndpointOGvg` | 1 | cmux |
| Network.framework | `_$s7Network9IPAddressMp` | 1 | cmux |
| Network.framework | `_$s7Network9IPAddressP8rawValue10Foundation4DataVvgTj` | 1 | cmux |
| Network.framework | `_$s7Network9NWBrowserC5StateOMn` | 1 | openclaw |
| Network.framework | `_$s7Network9NWBrowserC6ResultV8MetadataO7bonjouryAgA11NWTXTRecordVcAGmFWC` | 1 | openclaw |
| Network.framework | `_$s7Network9NWBrowserC6ResultV8MetadataOMa` | 1 | openclaw |
| Network.framework | `_$s7Network9NWBrowserC6ResultV8metadataAE8MetadataOvg` | 1 | openclaw |
| Network.framework | `_$s7Network9NWBrowserC6ResultVSHAAMc` | 1 | openclaw |
| Network.framework | `_$s7Network9NWBrowserC6ResultVSQAAMc` | 1 | openclaw |
| Network.framework | `_$s7Network9NWBrowserCMn` | 1 | openclaw |
| Network.framework | `_$sSo25NSURLSessionConfigurationC7NetworkE19proxyConfigurationsSayAC05ProxyB0VGvs` | 1 | cmux |
| Network.framework | `_nw_advertise_descriptor_create_bonjour_service` | 1 | spotify |
| Network.framework | `_nw_advertise_descriptor_set_txt_record_object` | 1 | spotify |
| Network.framework | `_nw_browse_descriptor_set_include_txt_record` | 1 | spotify |
| Network.framework | `_nw_browse_result_copy_txt_record_object` | 1 | spotify |
| Network.framework | `_nw_framer_copy_options` | 1 | dotnet-sdk |
| Network.framework | `_nw_framer_options_copy_object_value` | 1 | dotnet-sdk |
| Network.framework | `_nw_framer_options_set_object_value` | 1 | dotnet-sdk |
| Network.framework | `_nw_listener_get_port` | 1 | dotnet-sdk |
| Network.framework | `_nw_listener_set_advertise_descriptor` | 1 | spotify |
| Network.framework | `_nw_listener_set_state_changed_handler` | 1 | dotnet-sdk |
| Network.framework | `_nw_parameters_set_local_endpoint` | 1 | dotnet-sdk |
| Network.framework | `_nw_retain` | 1 | dotnet-sdk |
| Network.framework | `_nw_txt_record_access_key` | 1 | spotify |
| Network.framework | `_nw_txt_record_create_dictionary` | 1 | spotify |
| Network.framework | `_nw_txt_record_set_key` | 1 | spotify |
| NetworkExtension.framework | `_NEVPNConfigurationChangeNotification` | 1 | tailscale-app |
| NetworkExtension.framework | `_NEVPNConnectionErrorDomain` | 1 | tailscale-app |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEDNSSettings` | 1 | cmux |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEEvaluateConnectionRule` | 1 | tailscale-app |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEHotspotConfiguration` | 1 | whatsapp |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEHotspotConfigurationManager` | 1 | whatsapp |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEHotspotNetwork` | 1 | whatsapp |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEIPv4Route` | 1 | cmux |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEIPv4Settings` | 1 | cmux |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEIPv6Route` | 1 | cmux |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEIPv6Settings` | 1 | cmux |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEOnDemandRule` | 1 | tailscale-app |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEOnDemandRuleDisconnect` | 1 | tailscale-app |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEOnDemandRuleEvaluateConnection` | 1 | tailscale-app |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEPacketTunnelNetworkSettings` | 1 | cmux |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEPacketTunnelProvider` | 1 | cmux |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEProvider` | 1 | cmux |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEProxySettings` | 1 | brave-browser |
| NetworkExtension.framework | `_OBJC_CLASS_$_NETunnelProviderSession` | 1 | tailscale-app |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEVPNConnection` | 1 | cmux |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEVPNManager` | 1 | brave-browser |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEVPNProtocolIKEv2` | 1 | brave-browser |
| NetworkExtension.framework | `_OBJC_METACLASS_$_NEPacketTunnelProvider` | 1 | cmux |
| SystemConfiguration.framework | `_DHCPInfoGetOptionData` | 1 | tailscale-app |
| SystemConfiguration.framework | `_SCDynamicStoreCopyLocation` | 1 | hammerspoon |
| SystemConfiguration.framework | `_SCNetworkInterfaceCopyMediaOptions` | 1 | stats |
| SystemConfiguration.framework | `_SCNetworkServiceCopy` | 1 | zoom |
| SystemConfiguration.framework | `_SCNetworkServiceGetEnabled` | 1 | zoom |
| SystemConfiguration.framework | `_SCNetworkServiceGetName` | 1 | zoom |
| SystemConfiguration.framework | `_SCNetworkSetCopyAll` | 1 | hammerspoon |
| SystemConfiguration.framework | `_SCNetworkSetCopyCurrent` | 1 | zoom |
| SystemConfiguration.framework | `_SCNetworkSetGetName` | 1 | hammerspoon |
| SystemConfiguration.framework | `_SCNetworkSetGetServiceOrder` | 1 | zoom |
| SystemConfiguration.framework | `_SCNetworkSetGetSetID` | 1 | hammerspoon |
| SystemConfiguration.framework | `_SCNetworkSetSetCurrent` | 1 | hammerspoon |
| SystemConfiguration.framework | `_SCPreferencesCreate` | 1 | zoom |
| SystemConfiguration.framework | `_SCPreferencesCreateWithOptions` | 1 | hammerspoon |
| SystemConfiguration.framework | `_kSCNetworkInterfaceTypeIPSec` | 1 | zoom |
| SystemConfiguration.framework | `_kSCNetworkInterfaceTypeModem` | 1 | wireshark-app |
| SystemConfiguration.framework | `_kSCPropNetEthernetMediaSubType` | 1 | stats |
| SystemConfiguration.framework | `_kSCPropNetProxiesFTPPassive` | 1 | cyberduck |
| libSystem.B.dylib | `_DNSServiceAddRecord` | 1 | cyberduck |
| libSystem.B.dylib | `_DNSServiceEnumerateDomains` | 1 | cyberduck |
| libSystem.B.dylib | `_DNSServiceNATPortMappingCreate` | 1 | utm |
| libSystem.B.dylib | `_DNSServiceReconfirmRecord` | 1 | cyberduck |
| libSystem.B.dylib | `_DNSServiceRemoveRecord` | 1 | cyberduck |
| libSystem.B.dylib | `_DNSServiceUpdateRecord` | 1 | cyberduck |
| libSystem.B.dylib | `_TXTRecordGetCount` | 1 | zoom |
| libSystem.B.dylib | `_TXTRecordGetItemAtIndex` | 1 | zoom |
| libresolv.9.dylib | `_dns_free` | 1 | pgadmin4 |
| libresolv.9.dylib | `_dns_open` | 1 | pgadmin4 |
| libresolv.9.dylib | `_dns_search` | 1 | pgadmin4 |
| libresolv.9.dylib | `_dns_search_list_domain` | 1 | pgadmin4 |
| libresolv.9.dylib | `_res_9_ns_name_uncompress` | 1 | pgadmin4 |
| libresolv.9.dylib | `_res_9_query` | 1 | deluge |
| libswiftNetwork.dylib | `_$s7Network10NWEndpointO16debugDescriptionSSvg` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network10NWEndpointO4HostO4ipv6yAeA11IPv6AddressVcAEmFWC` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network10NWEndpointO4HostO4nameyAESS_AA11NWInterfaceVSgtcAEmFWC` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network10NWEndpointO4PortV8rawValues6UInt16Vvg` | 1 | sequel-ace |
| libswiftNetwork.dylib | `_$s7Network10NWListenerC4portAA10NWEndpointO4PortVSgvg` | 1 | sequel-ace |
| libswiftNetwork.dylib | `_$s7Network10NWListenerC5StateO2eeoiySbAE_AEtFZ` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network10NWListenerC5StateOMn` | 1 | raycast |
| libswiftNetwork.dylib | `_$s7Network10NWListenerC5stateAC5StateOvg` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network10NWListenerC7ServiceV4name4type6domain9txtRecordAESSSg_SSAjA11NWTXTRecordVtcfC` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network11IPv4AddressV8loopbackACvgZ` | 1 | raycast |
| libswiftNetwork.dylib | `_$s7Network11IPv4AddressVAA9IPAddressAAMc` | 1 | tailscale-app |
| libswiftNetwork.dylib | `_$s7Network11IPv6AddressVAA9IPAddressAAMc` | 1 | tailscale-app |
| libswiftNetwork.dylib | `_$s7Network11IPv6AddressVMn` | 1 | tailscale-app |
| libswiftNetwork.dylib | `_$s7Network11IPv6AddressVyACSgSScfC` | 1 | tailscale-app |
| libswiftNetwork.dylib | `_$s7Network11NWInterfaceV13InterfaceTypeOSQAAMc` | 1 | tailscale-app |
| libswiftNetwork.dylib | `_$s7Network11NWTXTRecordVMa` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network11NWTXTRecordVMn` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network11NWTXTRecordVyACSDyS2SGcfC` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network12NWConnectionC4send20discontiguousContent14contentContext10isComplete10completiony8Dispatch0K4DataV_AC0eG0CSbAC14SendCompletionOtF` | 1 | raycast |
| libswiftNetwork.dylib | `_$s7Network12NWConnectionC8metadata10definitionAA18NWProtocolMetadataCSgAA0E10DefinitionC_tF` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC23allowLocalEndpointReuseSbvs` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network13NWPathMonitorC21requiredInterfaceTypeAcA11NWInterfaceV0eF0O_tcfCTj` | 1 | raycast |
| libswiftNetwork.dylib | `_$s7Network13NWPathMonitorC24prohibitedInterfaceTypesACSayAA11NWInterfaceV0E4TypeOG_tcfCTj` | 1 | raycast |
| libswiftNetwork.dylib | `_$s7Network13NWProtocolTLSC10definitionAA0B10DefinitionCvgZ` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network13NWProtocolTLSC8MetadataC016securityProtocolD0So24OS_sec_protocol_metadata_pvgTj` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network13NWProtocolTLSC8MetadataCMa` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network13NWProtocolTLSCMa` | 1 | utm |
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
| libswiftNetwork.dylib | `_$s7Network18NWProtocolMetadataCMn` | 1 | iina |
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
| libswiftNetwork.dylib | `_$s7Network6NWPathV16debugDescriptionSSvg` | 1 | raycast |
| libswiftNetwork.dylib | `_$s7Network6NWPathV17UnsatisfiedReasonOMn` | 1 | sequel-ace |
| libswiftNetwork.dylib | `_$s7Network6NWPathV17UnsatisfiedReasonOSQAAMc` | 1 | sequel-ace |
| libswiftNetwork.dylib | `_$s7Network6NWPathV17usesInterfaceTypeySbAA11NWInterfaceV0dE0OF` | 1 | tailscale-app |
| libswiftNetwork.dylib | `_$s7Network6NWPathV6StatusO18requiresConnectionyA2EmFWC` | 1 | raycast |
| libswiftNetwork.dylib | `_$s7Network7NWErrorOs28CustomDebugStringConvertibleAAMc` | 1 | iina |
| libswiftNetwork.dylib | `_$s7Network9IPAddressMp` | 1 | tailscale-app |
| libswiftNetwork.dylib | `_$s7Network9IPAddressP8rawValue10Foundation4DataVvgTj` | 1 | tailscale-app |
| libswiftNetwork.dylib | `_$s7Network9NWBrowserC10DescriptorO20bonjourWithTXTRecordyAESS_SSSgtcAEmFWC` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network9NWBrowserC13browseResultsShyAC6ResultVGvg` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network9NWBrowserCMn` | 1 | utm |
