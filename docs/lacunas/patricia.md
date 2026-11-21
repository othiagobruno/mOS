# Lacunas de Patricia (rede para apps)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Patricia (rede para apps) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**696** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| libresolv.9.dylib | `_res_9_ns_initparse` | 10 | adobe-acrobat-reader, beekeeper-studio, freecad, microsoft-edge |
| libresolv.9.dylib | `_res_9_ns_parserr` | 10 | adobe-acrobat-reader, beekeeper-studio, freecad, microsoft-edge |
| NetworkExtension.framework | `_NEVPNStatusDidChangeNotification` | 9 | brave-browser, cloudflare-warp, cmux, duckduckgo |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEProvider` | 9 | cloudflare-warp, cmux, duckduckgo, lulu |
| libresolv.9.dylib | `_res_9_init` | 9 | crossover, deluge, dolphin, microsoft-edge |
| Kerberos.framework | `_com_err_va` | 8 | android-studio, cyberduck, datagrip, dbeaver-community |
| Kerberos.framework | `_krb5_cc_default` | 8 | android-studio, cyberduck, datagrip, dbeaver-community |
| Kerberos.framework | `_krb5_cc_end_seq_get` | 8 | android-studio, cyberduck, datagrip, dbeaver-community |
| Kerberos.framework | `_krb5_cc_next_cred` | 8 | android-studio, cyberduck, datagrip, dbeaver-community |
| Kerberos.framework | `_krb5_cc_set_flags` | 8 | android-studio, cyberduck, datagrip, dbeaver-community |
| Kerberos.framework | `_krb5_cc_start_seq_get` | 8 | android-studio, cyberduck, datagrip, dbeaver-community |
| Network.framework | `_$s7Network11IPv6AddressVMa` | 8 | cmux, fluidvoice, nordvpn, openclaw |
| NetworkExtension.framework | `_OBJC_CLASS_$_NETunnelProviderProtocol` | 8 | cloudflare-warp, cmux, duckduckgo, nordvpn |
| libSystem.B.dylib | `_DNSServiceSetDispatchQueue` | 8 | cmux, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| libswiftNetwork.dylib | `_$s7Network10NWListenerCMn` | 8 | bettertouchtool, duckduckgo, easydict, fork |
| Network.framework | `_$s7Network10NWListenerCMn` | 7 | bartender, cmux, codexbar, fluidvoice |
| Network.framework | `_$s7Network11IPv6AddressVMn` | 7 | cmux, nordvpn, openclaw, orbstack |
| Network.framework | `_$s7Network6NWPathV17usesInterfaceTypeySbAA11NWInterfaceV0dE0OF` | 7 | cmux, microsoft-office, microsoft-outlook, nordvpn |
| Network.framework | `_$s7Network6NWPathV6StatusO18requiresConnectionyA2EmFWC` | 7 | microsoft-excel, microsoft-office, microsoft-outlook, microsoft-powerpoint |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEDNSSettings` | 7 | cloudflare-warp, cmux, duckduckgo, nordvpn |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEIPv4Route` | 7 | cloudflare-warp, cmux, duckduckgo, nordvpn |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEIPv4Settings` | 7 | cloudflare-warp, cmux, duckduckgo, nordvpn |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEIPv6Route` | 7 | cloudflare-warp, cmux, duckduckgo, nordvpn |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEIPv6Settings` | 7 | cloudflare-warp, cmux, duckduckgo, nordvpn |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEOnDemandRuleConnect` | 7 | brave-browser, cloudflare-warp, duckduckgo, nordvpn |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEPacketTunnelNetworkSettings` | 7 | cloudflare-warp, cmux, duckduckgo, nordvpn |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEPacketTunnelProvider` | 7 | cloudflare-warp, cmux, duckduckgo, nordvpn |
| NetworkExtension.framework | `_OBJC_CLASS_$_NETunnelProviderManager` | 7 | cloudflare-warp, cmux, duckduckgo, nordvpn |
| NetworkExtension.framework | `_OBJC_METACLASS_$_NEPacketTunnelProvider` | 7 | cloudflare-warp, cmux, duckduckgo, nordvpn |
| GSS.framework | `_GSSCreateName` | 6 | microsoft-excel, microsoft-office, microsoft-outlook, microsoft-powerpoint |
| GSS.framework | `___gss_c_nt_export_name_oid_desc` | 6 | microsoft-excel, microsoft-office, microsoft-outlook, microsoft-powerpoint |
| GSS.framework | `_gss_aapl_initial_cred` | 6 | microsoft-excel, microsoft-office, microsoft-outlook, microsoft-powerpoint |
| GSS.framework | `_gss_add_oid_set_member` | 6 | microsoft-excel, microsoft-office, microsoft-outlook, microsoft-powerpoint |
| GSS.framework | `_gss_create_empty_oid_set` | 6 | microsoft-excel, microsoft-office, microsoft-outlook, microsoft-powerpoint |
| GSS.framework | `_gss_destroy_cred` | 6 | microsoft-excel, microsoft-office, microsoft-outlook, microsoft-powerpoint |
| GSS.framework | `_gss_inquire_cred` | 6 | microsoft-excel, microsoft-outlook, microsoft-powerpoint, microsoft-word |
| Network.framework | `_$s7Network10NWEndpointO4PortV8rawValues6UInt16Vvg` | 6 | cmux, nordvpn, openclaw, orbstack |
| Network.framework | `_$s7Network11IPv4AddressV8rawValue10Foundation4DataVvg` | 6 | cmux, openclaw, orbstack, orion |
| Network.framework | `_$s7Network11IPv6AddressV8rawValue10Foundation4DataVvg` | 6 | cmux, openclaw, orbstack, orion |
| Network.framework | `_$s7Network13NWPathMonitorC11currentPathAA0B0Vvg` | 6 | applite, cmux, microsoft-outlook, nordvpn |
| Network.framework | `_nw_framer_async` | 6 | dotnet-sdk, git-credential-manager, github, jellyfin |
| Network.framework | `_nw_framer_create_definition` | 6 | dotnet-sdk, git-credential-manager, github, jellyfin |
| Network.framework | `_nw_framer_create_options` | 6 | dotnet-sdk, git-credential-manager, github, jellyfin |
| Network.framework | `_nw_framer_deliver_input` | 6 | dotnet-sdk, git-credential-manager, github, jellyfin |
| Network.framework | `_nw_framer_message_create` | 6 | dotnet-sdk, git-credential-manager, github, jellyfin |
| Network.framework | `_nw_framer_parse_output` | 6 | dotnet-sdk, git-credential-manager, github, jellyfin |
| Network.framework | `_nw_framer_set_cleanup_handler` | 6 | dotnet-sdk, git-credential-manager, github, jellyfin |
| Network.framework | `_nw_framer_set_output_handler` | 6 | dotnet-sdk, git-credential-manager, github, jellyfin |
| Network.framework | `_nw_framer_set_stop_handler` | 6 | dotnet-sdk, git-credential-manager, github, jellyfin |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEOnDemandRule` | 6 | cloudflare-warp, duckduckgo, nordvpn, protonvpn |
| NetworkExtension.framework | `_OBJC_CLASS_$_NETunnelProviderSession` | 6 | duckduckgo, nordvpn, protonvpn, proxyman |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEVPNConnection` | 6 | cloudflare-warp, cmux, duckduckgo, nordvpn |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC20defaultProtocolStackAC0dE0Cvg` | 6 | bettertouchtool, duckduckgo, iina, istat-menus |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC21requiredLocalEndpointAA10NWEndpointOSgvs` | 6 | duckduckgo, easydict, fork, raycast |
| GSS.framework | `___gss_krb5_copy_ccache_x_oid_desc` | 5 | microsoft-excel, microsoft-office, microsoft-outlook, microsoft-powerpoint |
| GSS.framework | `_gss_export_name` | 5 | microsoft-excel, microsoft-office, microsoft-outlook, microsoft-powerpoint |
| GSS.framework | `_gss_inquire_cred_by_oid` | 5 | microsoft-excel, microsoft-office, microsoft-outlook, microsoft-powerpoint |
| GSS.framework | `_gss_release_buffer_set` | 5 | microsoft-excel, microsoft-office, microsoft-outlook, microsoft-powerpoint |
| Network.framework | `_$s7Network10NWEndpointO4HostO4ipv6yAeA11IPv6AddressVcAEmFWC` | 5 | cmux, fluidvoice, orbstack, protonvpn |
| Network.framework | `_$s7Network11IPv6AddressVyACSgSScfC` | 5 | cmux, nordvpn, openclaw, orion |
| NetworkExtension.framework | `_OBJC_CLASS_$_NENetworkRule` | 5 | duckduckgo, lulu, nordvpn, protonvpn |
| SystemConfiguration.framework | `_SCNetworkSetCopyCurrent` | 5 | clash-verge-rev, crossover, istat-menus, virtualbox |
| libSystem.B.dylib | `_DNSServiceBrowse` | 5 | cmux, cyberduck, realvnc-connect-viewer, teamviewer |
| libSystem.B.dylib | `_DNSServiceResolve` | 5 | cmux, cyberduck, realvnc-connect-viewer, teamviewer |
| libswiftNetwork.dylib | `_$s7Network10NWEndpointO4PortV8rawValues6UInt16Vvg` | 5 | bettertouchtool, duckduckgo, easydict, sequel-ace |
| libswiftNetwork.dylib | `_$s7Network11IPv6AddressVMa` | 5 | duckduckgo, easydict, tableplus, tailscale-app |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC23allowLocalEndpointReuseSbvs` | 5 | bettertouchtool, duckduckgo, easydict, iina |
| vmnet.framework | `_vmnet_interface_set_event_callback` | 5 | docker-desktop, orbstack, rancher, utm |
| vmnet.framework | `_vmnet_max_packet_size_key` | 5 | docker-desktop, orbstack, rancher, utm |
| vmnet.framework | `_vmnet_operation_mode_key` | 5 | docker-desktop, orbstack, rancher, utm |
| vmnet.framework | `_vmnet_read` | 5 | docker-desktop, orbstack, rancher, utm |
| vmnet.framework | `_vmnet_start_interface` | 5 | docker-desktop, orbstack, rancher, utm |
| vmnet.framework | `_vmnet_write` | 5 | docker-desktop, orbstack, rancher, utm |
| GSS.framework | `_GSSCredentialCopyUUID` | 4 | microsoft-excel, microsoft-outlook, microsoft-powerpoint, microsoft-word |
| GSS.framework | `_gss_compare_name` | 4 | microsoft-excel, microsoft-outlook, microsoft-powerpoint, microsoft-word |
| Network.framework | `_$s7Network10NWEndpointO4HostO2eeoiySbAE_AEtFZ` | 4 | cmux, orbstack, protonvpn, proxyman |
| Network.framework | `_$s7Network10NWListenerC4portAA10NWEndpointO4PortVSgvg` | 4 | cmux, openclaw, orbstack, proxyman |
| Network.framework | `_$s7Network11IPv4AddressVyACSg10Foundation4DataV_AA11NWInterfaceVSgtcfC` | 4 | cmux, orbstack, protonvpn, proxyman |
| Network.framework | `_$s7Network11IPv6AddressVyACSg10Foundation4DataV_AA11NWInterfaceVSgtcfC` | 4 | cmux, orbstack, protonvpn, proxyman |
| Network.framework | `_$s7Network12NWConnectionC10parametersAA12NWParametersCvg` | 4 | cmux, orbstack, protonvpn, proxyman |
| Network.framework | `_$s7Network12NWConnectionC14ContentContextC12finalMessageAEvgZ` | 4 | cmux, nordvpn, orbstack, proxyman |
| Network.framework | `_$s7Network12NWParametersC23allowLocalEndpointReuseSbvs` | 4 | bartender, cmux, orbstack, proxyman |
| Network.framework | `_$s7Network13NWPathMonitorC17makeAsyncIteratorAC0F0VyF` | 4 | microsoft-excel, microsoft-outlook, microsoft-powerpoint, microsoft-word |
| Network.framework | `_$s7Network13NWPathMonitorC8IteratorV4nextAA0B0VSgyYaF` | 4 | microsoft-excel, microsoft-outlook, microsoft-powerpoint, microsoft-word |
| Network.framework | `_$s7Network13NWPathMonitorC8IteratorV4nextAA0B0VSgyYaFTu` | 4 | microsoft-excel, microsoft-outlook, microsoft-powerpoint, microsoft-word |
| Network.framework | `_$s7Network13NWPathMonitorC8IteratorVMa` | 4 | microsoft-excel, microsoft-outlook, microsoft-powerpoint, microsoft-word |
| Network.framework | `_OBJC_CLASS_$_NWHostEndpoint` | 4 | duckduckgo, lulu, nordvpn, protonvpn |
| NetworkExtension.framework | `_NEVPNErrorDomain` | 4 | cloudflare-warp, duckduckgo, nordvpn, proxyman |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEAppProxyTCPFlow` | 4 | duckduckgo, nordvpn, protonvpn, proxyman |
| NetworkExtension.framework | `_OBJC_CLASS_$_NETransparentProxyManager` | 4 | duckduckgo, nordvpn, protonvpn, proxyman |
| NetworkExtension.framework | `_OBJC_CLASS_$_NETransparentProxyNetworkSettings` | 4 | duckduckgo, nordvpn, protonvpn, proxyman |
| NetworkExtension.framework | `_OBJC_CLASS_$_NETransparentProxyProvider` | 4 | duckduckgo, nordvpn, protonvpn, proxyman |
| NetworkExtension.framework | `_OBJC_METACLASS_$_NETransparentProxyProvider` | 4 | duckduckgo, nordvpn, protonvpn, proxyman |
| SystemConfiguration.framework | `_SCDynamicStoreCopyDHCPInfo` | 4 | crossover, hammerspoon, tailscale-app, unity-hub |
| SystemConfiguration.framework | `_SCDynamicStoreKeyCreateConsoleUser` | 4 | anydesk, displaylink, openvpn-connect, teamviewer |
| libresolv.9.dylib | `_res_9_query` | 4 | crossover, deluge, freecad, windows-app |
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
| vmnet.framework | `_vmnet_allocate_mac_address_key` | 4 | orbstack, rancher, utm, virtualbox |
| vmnet.framework | `_vmnet_interface_id_key` | 4 | docker-desktop, orbstack, rancher, virtualbox |
| vmnet.framework | `_vmnet_mtu_key` | 4 | docker-desktop, orbstack, rancher, utm |
| vmnet.framework | `_vmnet_stop_interface` | 4 | orbstack, rancher, utm, virtualbox |
| CFNetwork.framework | `_CFNetDiagnosticCreateWithURL` | 3 | cyberduck, mactex, mactex-no-gui |
| Kerberos.framework | `_cc_initialize` | 3 | freecad, mysqlworkbench, pgadmin4 |
| Network.framework | `_$s7Network10NWEndpointO2eeoiySbAC_ACtFZ` | 3 | orbstack, protonvpn, proxyman |
| Network.framework | `_$s7Network10NWEndpointO4HostO4nameyAESS_AA11NWInterfaceVSgtcAEmFWC` | 3 | cmux, fluidvoice, protonvpn |
| Network.framework | `_$s7Network10NWEndpointO4PortVyAESgSScfC` | 3 | cmux, nordvpn, protonvpn |
| Network.framework | `_$s7Network10NWEndpointO4unixyACSS_tcACmFWC` | 3 | cmux, orbstack, proxyman |
| Network.framework | `_$s7Network10NWListenerC5StateOMn` | 3 | bartender, cmux, fluidvoice |
| Network.framework | `_$s7Network11IPv4AddressVSQAAMc` | 3 | cmux, nordvpn, protonvpn |
| Network.framework | `_$s7Network11IPv6AddressVSQAAMc` | 3 | cmux, nordvpn, protonvpn |
| Network.framework | `_$s7Network12NWConnectionC17pathUpdateHandleryAA6NWPathVcSgvs` | 3 | cmux, orbstack, proxyman |
| Network.framework | `_$s7Network12NWParametersC21requiredLocalEndpointAA10NWEndpointOSgvs` | 3 | cmux, orbstack, proxyman |
| Network.framework | `_$s7Network13NWProtocolTCPC7OptionsC7noDelaySbvsTj` | 3 | cmux, orbstack, proxyman |
| Network.framework | `_$s7Network6NWPathV6StatusOMn` | 3 | macwhisper, nordvpn, protonvpn |
| NetworkExtension.framework | `_NEVPNConfigurationChangeNotification` | 3 | duckduckgo, protonvpn, tailscale-app |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEOnDemandRuleDisconnect` | 3 | nordvpn, sfm, tailscale-app |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEProxySettings` | 3 | brave-browser, nordvpn, sfm |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEVPNManager` | 3 | brave-browser, nordvpn, protonvpn |
| SystemConfiguration.framework | `_DHCPInfoGetOptionData` | 3 | crossover, tailscale-app, unity-hub |
| SystemConfiguration.framework | `_SCNetworkConnectionCreateWithServiceID` | 3 | istat-menus, microsoft-outlook, microsoft-teams |
| SystemConfiguration.framework | `_SCNetworkSetCopyServices` | 3 | clash-verge-rev, crossover, virtualbox |
| SystemConfiguration.framework | `_kSCPropNetIPv6PrefixLength` | 3 | cloudflare-warp, nordvpn, teamviewer |
| libSystem.B.dylib | `_DNSServiceNATPortMappingCreate` | 3 | gemini, the-unarchiver, utm |
| libSystem.B.dylib | `_DNSServiceRegisterRecord` | 3 | cmux, cyberduck, orion |
| libresolv.9.dylib | `_dns_free` | 3 | freecad, mysqlworkbench, pgadmin4 |
| libresolv.9.dylib | `_dns_open` | 3 | freecad, mysqlworkbench, pgadmin4 |
| libresolv.9.dylib | `_dns_search` | 3 | freecad, mysqlworkbench, pgadmin4 |
| libresolv.9.dylib | `_dns_search_list_domain` | 3 | freecad, mysqlworkbench, pgadmin4 |
| libresolv.9.dylib | `_res_9_ns_name_uncompress` | 3 | freecad, mysqlworkbench, pgadmin4 |
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
| libswiftNetwork.dylib | `_$s7Network6NWPathV6StatusO18requiresConnectionyA2EmFWC` | 3 | raycast, sfm, tailscale-app |
| vmnet.framework | `_vmnet_enable_isolation_key` | 3 | orbstack, rancher, utm |
| vmnet.framework | `_vmnet_end_address_key` | 3 | rancher, utm, virtualbox |
| vmnet.framework | `_vmnet_network_identifier_key` | 3 | orbstack, rancher, utm |
| vmnet.framework | `_vmnet_shared_interface_name_key` | 3 | rancher, utm, virtualbox |
| vmnet.framework | `_vmnet_start_address_key` | 3 | rancher, utm, virtualbox |
| vmnet.framework | `_vmnet_subnet_mask_key` | 3 | rancher, utm, virtualbox |
| CFNetwork.framework | `_CFNetDiagnosticCopyNetworkStatusPassively` | 2 | mactex, mactex-no-gui |
| CFNetwork.framework | `_CFNetServiceBrowserCreate` | 2 | bbedit, dropbox |
| CFNetwork.framework | `_CFNetServiceSetClient` | 2 | bbedit, dropbox |
| CFNetwork.framework | `_kCFStreamErrorDomainWinSock` | 2 | hammerspoon, windows-app |
| LDAP.framework | `_ldap_count_entries` | 2 | adobe-acrobat-reader, tableplus |
| LDAP.framework | `_ldap_result` | 2 | adobe-acrobat-reader, tableplus |
| LDAP.framework | `_ldap_unbind` | 2 | adobe-acrobat-reader, tableplus |
| Network.framework | `_$s7Network10NWEndpointO3urlyAC10Foundation3URLVcACmFWC` | 2 | nordvpn, windows-app |
| Network.framework | `_$s7Network10NWEndpointO4HostOSHAAMc` | 2 | cmux, protonvpn |
| Network.framework | `_$s7Network10NWEndpointO4PortVSHAAMc` | 2 | cmux, protonvpn |
| Network.framework | `_$s7Network10NWEndpointO4PortVSQAAMc` | 2 | cmux, protonvpn |
| Network.framework | `_$s7Network10NWEndpointOSQAAMc` | 2 | cmux, nordvpn |
| Network.framework | `_$s7Network10NWListenerC10parametersAA12NWParametersCvg` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network10NWListenerC5stateAC5StateOvg` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network11IPv4AddressV16debugDescriptionSSvg` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network11IPv4AddressVAA9IPAddressAAMc` | 2 | cmux, protonvpn |
| Network.framework | `_$s7Network11IPv6AddressV16debugDescriptionSSvg` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network11IPv6AddressVAA9IPAddressAAMc` | 2 | cmux, protonvpn |
| Network.framework | `_$s7Network11NWInterfaceV13InterfaceTypeOSQAAMc` | 2 | cmux, nordvpn |
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
| Network.framework | `_$s7Network18ProxyConfigurationV07socksv5B0AcA10NWEndpointO_tcfC` | 2 | cmux, orion |
| Network.framework | `_$s7Network18ProxyConfigurationV15excludedDomainsSaySSGvs` | 2 | cmux, orion |
| Network.framework | `_$s7Network18ProxyConfigurationV16httpCONNECTProxy10tlsOptionsAcA10NWEndpointO_AA13NWProtocolTLSC0G0CSgtcfC` | 2 | cmux, orion |
| Network.framework | `_$s7Network18ProxyConfigurationVMa` | 2 | cmux, orion |
| Network.framework | `_$s7Network18ProxyConfigurationVMn` | 2 | cmux, orion |
| Network.framework | `_$s7Network20NWProtocolDefinitionC2eeoiySbAC_ACtFZ` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network20NWProtocolDefinitionCMa` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network20NWProtocolDefinitionCMn` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network6NWPathV2eeoiySbAC_ACtFZ` | 2 | orbstack, proxyman |
| Network.framework | `_$s7Network6NWPathV8gatewaysSayAA10NWEndpointOGvg` | 2 | cmux, nordvpn |
| Network.framework | `_$s7Network9IPAddressMp` | 2 | cmux, protonvpn |
| Network.framework | `_$s7Network9IPAddressP8rawValue10Foundation4DataVvgTj` | 2 | cmux, protonvpn |
| Network.framework | `_$s7Network9NWBrowserCMn` | 2 | openclaw, protonvpn |
| NetworkExtension.framework | `_NEAppProxyErrorDomain` | 2 | duckduckgo, proxyman |
| NetworkExtension.framework | `_NEVPNConnectionErrorDomain` | 2 | nordvpn, tailscale-app |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEAppProxyFlow` | 2 | duckduckgo, nordvpn |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEAppProxyUDPFlow` | 2 | nordvpn, protonvpn |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEDNSOverHTTPSSettings` | 2 | protonvpn, tailscale-app |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEEvaluateConnectionRule` | 2 | sfm, tailscale-app |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEOnDemandRuleEvaluateConnection` | 2 | sfm, tailscale-app |
| NetworkExtension.framework | `_OBJC_CLASS_$_NEVPNProtocolIKEv2` | 2 | brave-browser, protonvpn |
| SystemConfiguration.framework | `_SCNetworkConnectionGetStatus` | 2 | microsoft-outlook, microsoft-teams |
| SystemConfiguration.framework | `_SCNetworkConnectionStart` | 2 | istat-menus, microsoft-outlook |
| SystemConfiguration.framework | `_SCNetworkConnectionStop` | 2 | istat-menus, microsoft-outlook |
| SystemConfiguration.framework | `_SCNetworkProtocolGetConfiguration` | 2 | clash-verge-rev, nordvpn |
| SystemConfiguration.framework | `_SCNetworkServiceCopyProtocol` | 2 | clash-verge-rev, nordvpn |
| SystemConfiguration.framework | `_SCNetworkSetCopyAll` | 2 | hammerspoon, istat-menus |
| SystemConfiguration.framework | `_SCNetworkSetGetName` | 2 | hammerspoon, istat-menus |
| SystemConfiguration.framework | `_SCNetworkSetGetServiceOrder` | 2 | clash-verge-rev, zoom |
| SystemConfiguration.framework | `_SCNetworkSetGetSetID` | 2 | hammerspoon, istat-menus |
| SystemConfiguration.framework | `_kSCEntNetModem` | 2 | adobe-acrobat-reader, microsoft-outlook |
| SystemConfiguration.framework | `_kSCPropNetIPv6ConfigMethod` | 2 | nordvpn, teamviewer |
| SystemConfiguration.framework | `_kSCValNetIPv6ConfigMethodLinkLocal` | 2 | nordvpn, teamviewer |
| SystemConfiguration.framework | `_kSCValNetIPv6ConfigMethodManual` | 2 | nordvpn, teamviewer |
| libSystem.B.dylib | `_DNSServiceConstructFullName` | 2 | cyberduck, zoom |
| libSystem.B.dylib | `_DNSServiceReconfirmRecord` | 2 | cyberduck, teamviewer |
| libSystem.B.dylib | `_TXTRecordCreate` | 2 | teamviewer, zoom |
| libSystem.B.dylib | `_TXTRecordDeallocate` | 2 | teamviewer, zoom |
| libSystem.B.dylib | `_TXTRecordGetBytesPtr` | 2 | teamviewer, zoom |
| libSystem.B.dylib | `_TXTRecordGetCount` | 2 | teamviewer, zoom |
| libSystem.B.dylib | `_TXTRecordGetLength` | 2 | teamviewer, zoom |
| libSystem.B.dylib | `_TXTRecordSetValue` | 2 | teamviewer, zoom |
| libswiftNetwork.dylib | `_$s7Network10NWEndpointO4HostO4nameyAESS_AA11NWInterfaceVSgtcAEmFWC` | 2 | duckduckgo, utm |
| libswiftNetwork.dylib | `_$s7Network10NWListenerC10parametersAA12NWParametersCvg` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network10NWListenerC5StateO2eeoiySbAE_AEtFZ` | 2 | bettertouchtool, iina |
| libswiftNetwork.dylib | `_$s7Network10NWListenerC5StateOMn` | 2 | raycast, tableplus |
| libswiftNetwork.dylib | `_$s7Network11IPv4AddressV8loopbackACvgZ` | 2 | raycast, tableplus |
| libswiftNetwork.dylib | `_$s7Network11IPv4AddressVAA9IPAddressAAMc` | 2 | duckduckgo, tailscale-app |
| libswiftNetwork.dylib | `_$s7Network11IPv6AddressVAA9IPAddressAAMc` | 2 | duckduckgo, tailscale-app |
| libswiftNetwork.dylib | `_$s7Network11IPv6AddressVyACSgSScfC` | 2 | duckduckgo, tailscale-app |
| libswiftNetwork.dylib | `_$s7Network11NWInterfaceV13InterfaceTypeOSQAAMc` | 2 | duckduckgo, tailscale-app |
| libswiftNetwork.dylib | `_$s7Network11NWInterfaceV5indexSivg` | 2 | sfm, tailscale-app |
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
| libswiftNetwork.dylib | `_$s7Network12NWParametersC13ProtocolStackC08internetC0AA17NWProtocolOptionsCSgvgTj` | 2 | duckduckgo, istat-menus |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC20MultipathServiceTypeO8disabledyA2EmFWC` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC20MultipathServiceTypeOMa` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC20MultipathServiceTypeOMn` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC20multipathServiceTypeAC09MultipathdE0Ovs` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC21requiredLocalEndpointAA10NWEndpointOSgvg` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC4dtls3udpAcA13NWProtocolTLSC7OptionsCSg_AA0E3UDPCAHCtcfC` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network12NWParametersCMn` | 2 | easydict, tableplus |
| libswiftNetwork.dylib | `_$s7Network12NWProtocolIPC7OptionsC7VersionO2v4yA2GmFWC` | 2 | duckduckgo, istat-menus |
| libswiftNetwork.dylib | `_$s7Network12NWProtocolIPC7OptionsC7VersionO2v6yA2GmFWC` | 2 | duckduckgo, istat-menus |
| libswiftNetwork.dylib | `_$s7Network12NWProtocolIPC7OptionsC7VersionOMa` | 2 | duckduckgo, istat-menus |
| libswiftNetwork.dylib | `_$s7Network12NWProtocolIPC7OptionsC7versionAE7VersionOvsTj` | 2 | duckduckgo, istat-menus |
| libswiftNetwork.dylib | `_$s7Network12NWProtocolIPC7OptionsCMa` | 2 | duckduckgo, istat-menus |
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
| libswiftNetwork.dylib | `_$s7Network6NWPathV8gatewaysSayAA10NWEndpointOGvg` | 2 | duckduckgo, sfm |
| libswiftNetwork.dylib | `_$s7Network9IPAddressMp` | 2 | duckduckgo, tailscale-app |
| libswiftNetwork.dylib | `_$s7Network9IPAddressP8rawValue10Foundation4DataVvgTj` | 2 | duckduckgo, tailscale-app |
| libswiftNetwork.dylib | `_$s7Network9NWBrowserCMn` | 2 | bettertouchtool, utm |
| vmnet.framework | `_vmnet_copy_shared_interface_list` | 2 | rancher, utm |
| vmnet.framework | `_vmnet_estimated_packets_available_key` | 2 | rancher, virtualbox |
| vmnet.framework | `_vmnet_nat66_prefix_key` | 2 | rancher, utm |
| CFNetwork.framework | `_CFHTTPCookieStorageCopyCookies` | 1 | orion |
| CFNetwork.framework | `_CFHTTPCookieStorageCreateFromFile` | 1 | orion |
| CFNetwork.framework | `_CFHTTPCookieStorageCreateFromIdentifyingData` | 1 | orion |
| CFNetwork.framework | `_CFHTTPCookieStorageCreateIdentifyingData` | 1 | orion |
| CFNetwork.framework | `_CFHTTPCookieStorageDeleteAllCookies` | 1 | orion |
| CFNetwork.framework | `_CFHTTPCookieStorageDeleteCookie` | 1 | orion |
| CFNetwork.framework | `_CFHTTPCookieStorageGetCookieAcceptPolicy` | 1 | orion |
| CFNetwork.framework | `_CFHTTPCookieStorageScheduleWithRunLoop` | 1 | orion |
| CFNetwork.framework | `_CFHTTPCookieStorageSetCookieAcceptPolicy` | 1 | orion |
| CFNetwork.framework | `_CFNetDiagnosticDiagnoseProblemInteractively` | 1 | cyberduck |
| CFNetwork.framework | `_CFNetServiceBrowserInvalidate` | 1 | bbedit |
| CFNetwork.framework | `_CFNetServiceBrowserScheduleWithRunLoop` | 1 | bbedit |
| CFNetwork.framework | `_CFNetServiceBrowserSearchForServices` | 1 | bbedit |
| CFNetwork.framework | `_CFNetServiceBrowserUnscheduleFromRunLoop` | 1 | bbedit |
| CFNetwork.framework | `_CFNetServiceCancel` | 1 | bbedit |
| CFNetwork.framework | `_CFNetServiceCreate` | 1 | bbedit |
| CFNetwork.framework | `_CFNetServiceGetName` | 1 | bbedit |
| CFNetwork.framework | `_CFNetServiceGetPortNumber` | 1 | bbedit |
| CFNetwork.framework | `_CFNetServiceGetTargetHost` | 1 | bbedit |
| CFNetwork.framework | `_CFNetServiceMonitorCreate` | 1 | dropbox |
| CFNetwork.framework | `_CFNetServiceResolve` | 1 | bbedit |
| CFNetwork.framework | `_CFNetServiceScheduleWithRunLoop` | 1 | bbedit |
| CFNetwork.framework | `_CFNetServiceUnscheduleFromRunLoop` | 1 | bbedit |
| CFNetwork.framework | `_CFURLCacheCopyResponseForRequest` | 1 | orion |
| CFNetwork.framework | `_CFURLCacheCopySharedURLCache` | 1 | orion |
| CFNetwork.framework | `_CFURLCacheMemoryCapacity` | 1 | orion |
| CFNetwork.framework | `_CFURLCacheSetDiskCapacity` | 1 | orion |
| CFNetwork.framework | `_CFURLCacheSetMemoryCapacity` | 1 | orion |
| CFNetwork.framework | `_CFURLRequestCreateMutableCopy` | 1 | orion |
| CFNetwork.framework | `_CFURLRequestGetRequestPriority` | 1 | orion |
| CFNetwork.framework | `_CFURLRequestSetHTTPRequestBody` | 1 | orion |
| CFNetwork.framework | `_CFURLRequestSetHTTPRequestBodyStream` | 1 | orion |
| CFNetwork.framework | `_CFURLRequestSetRequestPriority` | 1 | orion |
| CFNetwork.framework | `_CFURLRequestSetShouldPipelineHTTP` | 1 | orion |
| CFNetwork.framework | `_CFURLResponseGetHTTPResponse` | 1 | orion |
| CFNetwork.framework | `_CFURLResponseGetMIMEType` | 1 | orion |
| CFNetwork.framework | `_CFURLResponseGetURL` | 1 | orion |
| CFNetwork.framework | `_CFURLResponseSetMIMEType` | 1 | orion |
| CFNetwork.framework | `_NSURLAuthenticationMethodOAuth` | 1 | orion |
| CFNetwork.framework | `_OBJC_CLASS_$_NSURLDownload` | 1 | orion |
| CFNetwork.framework | `_OBJC_CLASS_$__NSHSTSStorage` | 1 | orion |
| CFNetwork.framework | `_OBJC_CLASS_$__NSHTTPAlternativeServicesFilter` | 1 | orion |
| CFNetwork.framework | `_OBJC_CLASS_$__NSHTTPAlternativeServicesStorage` | 1 | orion |
| CFNetwork.framework | `_OBJC_METACLASS_$_NSURLDownload` | 1 | orion |
| CFNetwork.framework | `__CFCachedURLResponseGetMemMappedData` | 1 | orion |
| CFNetwork.framework | `__CFCachedURLResponseSetBecameFileBackedCallBackBlock` | 1 | orion |
| CFNetwork.framework | `__CFHTTPCookieStorageGetDefault` | 1 | orion |
| CFNetwork.framework | `__CFHTTPMessageSetResponseProxyURL` | 1 | orion |
| CFNetwork.framework | `__CFHTTPMessageSetResponseURL` | 1 | orion |
| CFNetwork.framework | `__CFHostIsDomainTopLevel` | 1 | orion |
| CFNetwork.framework | `__CFNetworkCopyATSContext` | 1 | orion |
| CFNetwork.framework | `__CFNetworkHTTPConnectionCacheGetLimit` | 1 | orion |
| CFNetwork.framework | `__CFNetworkHTTPConnectionCacheSetLimit` | 1 | orion |
| CFNetwork.framework | `__CFNetworkIsKnownHSTSHostWithSession` | 1 | orion |
| CFNetwork.framework | `__CFNetworkSetATSContext` | 1 | orion |
| CFNetwork.framework | `__CFURLCacheCopyCacheDirectory` | 1 | orion |
| CFNetwork.framework | `__CFURLConnectionSetFrameworkStubs` | 1 | orion |
| CFNetwork.framework | `__CFURLRequestSetProtocolProperty` | 1 | orion |
| CFNetwork.framework | `__CFURLRequestSetStorageSession` | 1 | orion |
| CFNetwork.framework | `__CFURLResponseGetSSLCertificateContext` | 1 | orion |
| CFNetwork.framework | `__CFURLStorageSessionCopyCache` | 1 | orion |
| CFNetwork.framework | `__CFURLStorageSessionCopyCookieStorage` | 1 | orion |
| CFNetwork.framework | `__CFURLStorageSessionCreate` | 1 | orion |
| CFNetwork.framework | `__CFURLStorageSessionDisableCache` | 1 | orion |
| CFNetwork.framework | `__kCFStreamSocketSetNoDelay` | 1 | orion |
| CFNetwork.framework | `__kCFURLCachePartitionKey` | 1 | orion |
| CFNetwork.framework | `__kCFURLConnectionPropertyShouldSniff` | 1 | orion |
| CFNetwork.framework | `__kCFURLStorageSessionIsPrivate` | 1 | orion |
| CFNetwork.framework | `_kCFErrorDomainWinSock` | 1 | windows-app |
| CFNetwork.framework | `_kCFFTPResourceGroup` | 1 | bbedit |
| CFNetwork.framework | `_kCFFTPResourceLink` | 1 | bbedit |
| CFNetwork.framework | `_kCFFTPResourceModDate` | 1 | bbedit |
| CFNetwork.framework | `_kCFFTPResourceMode` | 1 | bbedit |
| CFNetwork.framework | `_kCFFTPResourceName` | 1 | bbedit |
| CFNetwork.framework | `_kCFFTPResourceOwner` | 1 | bbedit |
| CFNetwork.framework | `_kCFFTPResourceSize` | 1 | bbedit |
| CFNetwork.framework | `_kCFFTPResourceType` | 1 | bbedit |
| CFNetwork.framework | `_kCFHTTPCookieLocalFileDomain` | 1 | orion |
| CFNetwork.framework | `_kCFStreamPropertyCONNECTAdditionalHeaders` | 1 | orion |
| CFNetwork.framework | `_kCFStreamPropertyCONNECTProxy` | 1 | orion |
| CFNetwork.framework | `_kCFStreamPropertyCONNECTProxyHost` | 1 | orion |
| CFNetwork.framework | `_kCFStreamPropertyCONNECTProxyPort` | 1 | orion |
| CFNetwork.framework | `_kCFStreamPropertyCONNECTResponse` | 1 | orion |
| CFNetwork.framework | `_kCFStreamPropertySourceApplication` | 1 | orion |
| CFNetwork.framework | `_kCFStreamSocketSecurityLevelTLSv1_2` | 1 | orion |
| CFNetwork.framework | `_kCFURLRequestAllowAllPOSTCaching` | 1 | orion |
| CFNetwork.framework | `_kCFURLRequestContentDecoderSkipURLCheck` | 1 | orion |
| GSS.framework | `___ApplePrivate_gss_krb5_get_tkt_flags` | 1 | pgadmin4 |
| GSS.framework | `___ApplePrivate_gss_krb5_import_cred` | 1 | pgadmin4 |
| GSS.framework | `___ApplePrivate_gss_release_iov_buffer` | 1 | pgadmin4 |
| GSS.framework | `___ApplePrivate_gss_unwrap_iov` | 1 | pgadmin4 |
| GSS.framework | `___ApplePrivate_gss_wrap_iov` | 1 | pgadmin4 |
| GSS.framework | `___ApplePrivate_gss_wrap_iov_length` | 1 | pgadmin4 |
| GSS.framework | `___ApplePrivate_gsskrb5_extract_authtime_from_sec_context` | 1 | pgadmin4 |
| GSS.framework | `___gss_c_nt_anonymous_oid_desc` | 1 | pgadmin4 |
| GSS.framework | `___gss_c_nt_machine_uid_name_oid_desc` | 1 | pgadmin4 |
| GSS.framework | `___gss_c_nt_string_uid_name_oid_desc` | 1 | pgadmin4 |
| GSS.framework | `___gss_krb5_nt_principal_name_oid_desc` | 1 | pgadmin4 |
| GSS.framework | `_gss_wrap_size_limit` | 1 | tableplus |
| Kerberos.framework | `_gss_add_oid_set_member` | 1 | wireshark-app |
| Kerberos.framework | `_gss_create_empty_oid_set` | 1 | wireshark-app |
| Kerberos.framework | `_gss_inquire_cred` | 1 | wireshark-app |
| Kerberos.framework | `_gss_inquire_cred_by_mech` | 1 | wireshark-app |
| Kerberos.framework | `_gss_release_oid` | 1 | wireshark-app |
| Kerberos.framework | `_gss_test_oid_set_member` | 1 | wireshark-app |
| Kerberos.framework | `_gss_wrap_size_limit` | 1 | libreoffice |
| Kerberos.framework | `_krb5_c_decrypt` | 1 | wireshark-app |
| Kerberos.framework | `_krb5_free_host_realm` | 1 | microsoft-excel |
| Kerberos.framework | `_krb5_get_host_realm` | 1 | microsoft-excel |
| Kerberos.framework | `_krb5_kt_resolve` | 1 | wireshark-app |
| LDAP.framework | `_ldap_bind_s` | 1 | adobe-acrobat-reader |
| LDAP.framework | `_ldap_count_values_len` | 1 | adobe-acrobat-reader |
| LDAP.framework | `_ldap_initialize` | 1 | adobe-acrobat-reader |
| LDAP.framework | `_ldap_search_ext` | 1 | adobe-acrobat-reader |
| LDAP.framework | `_ldap_search_st` | 1 | tableplus |
| LDAP.framework | `_ldap_simple_bind` | 1 | tableplus |
| Network.framework | `_$s7Network10NWEndpointO16debugDescriptionSSvg` | 1 | protonvpn |
| Network.framework | `_$s7Network10NWEndpointO4PortV16debugDescriptionSSvg` | 1 | proxyman |
| Network.framework | `_$s7Network10NWEndpointO9txtRecordAA11NWTXTRecordVSgvg` | 1 | openclaw |
| Network.framework | `_$s7Network10NWEndpointOSHAAMc` | 1 | nordvpn |
| Network.framework | `_$s7Network11IPv4AddressV9broadcastACvgZ` | 1 | windows-app |
| Network.framework | `_$s7Network11IPv4AddressV9interfaceAA11NWInterfaceVSgvg` | 1 | nordvpn |
| Network.framework | `_$s7Network11NWInterfaceV13InterfaceTypeO2eeoiySbAE_AEtFZ` | 1 | nordvpn |
| Network.framework | `_$s7Network11NWInterfaceV13InterfaceTypeOSHAAMc` | 1 | nordvpn |
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
| Network.framework | `_$s7Network12NWParametersC3tlsACvgZ` | 1 | nordvpn |
| Network.framework | `_$s7Network13NWPathMonitorC17pathUpdateHandleryAA0B0VcSgvM` | 1 | nordvpn |
| Network.framework | `_$s7Network13NWPathMonitorC5queueSo012OS_dispatch_D0CSgvg` | 1 | nordvpn |
| Network.framework | `_$s7Network13NWProtocolTLSC7OptionsCAEycfCTj` | 1 | orbstack |
| Network.framework | `_$s7Network18ProxyConfigurationV12matchDomainsSaySSGvs` | 1 | cmux |
| Network.framework | `_$s7Network18ProxyConfigurationV13allowFailoverSbvs` | 1 | cmux |
| Network.framework | `_$s7Network18ProxyConfigurationV15applyCredential8username8passwordySS_SStF` | 1 | cmux |
| Network.framework | `_$s7Network18ProxyConfigurationV15excludedDomainsSaySSGvM` | 1 | orion |
| Network.framework | `_$s7Network6NWPathV16debugDescriptionSSvg` | 1 | nordvpn |
| Network.framework | `_$s7Network6NWPathV17UnsatisfiedReasonO11vpnInactiveyA2EmFWC` | 1 | nordvpn |
| Network.framework | `_$s7Network6NWPathV17UnsatisfiedReasonO12notAvailableyA2EmFWC` | 1 | nordvpn |
| Network.framework | `_$s7Network6NWPathV17UnsatisfiedReasonO2eeoiySbAE_AEtFZ` | 1 | nordvpn |
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
| Network.framework | `_nw_array_get_count` | 1 | orion |
| Network.framework | `_nw_array_get_object_at_index` | 1 | orion |
| Network.framework | `_nw_context_create` | 1 | orion |
| Network.framework | `_nw_context_set_privacy_level` | 1 | orion |
| Network.framework | `_nw_endpoint_get_known_tracker_name` | 1 | orion |
| Network.framework | `_nw_framer_copy_options` | 1 | dotnet-sdk |
| Network.framework | `_nw_framer_options_copy_object_value` | 1 | dotnet-sdk |
| Network.framework | `_nw_framer_options_set_object_value` | 1 | dotnet-sdk |
| Network.framework | `_nw_ip_create_metadata` | 1 | bbedit |
| Network.framework | `_nw_ip_metadata_set_service_class` | 1 | bbedit |
| Network.framework | `_nw_parameters_set_context` | 1 | orion |
| Network.framework | `_nw_parameters_set_source_application_by_bundle_id` | 1 | orion |
| Network.framework | `_nw_path_create_evaluator_for_endpoint` | 1 | orion |
| Network.framework | `_nw_path_evaluator_copy_path` | 1 | orion |
| Network.framework | `_nw_resolver_cancel` | 1 | orion |
| Network.framework | `_nw_resolver_create_with_path` | 1 | orion |
| Network.framework | `_nw_resolver_set_update_handler` | 1 | orion |
| Network.framework | `_nw_ws_metadata_copy_server_response` | 1 | microsoft-outlook |
| Network.framework | `_nw_ws_response_enumerate_additional_headers` | 1 | microsoft-outlook |
| NetworkExtension.framework | `_$s16NetworkExtension25NEAppProxyUDPFlowHandlingMp` | 1 | proxyman |
| NetworkExtension.framework | `_$s16NetworkExtension25NEAppProxyUDPFlowHandlingP09handleNewE0_25initialRemoteFlowEndpointSbSo0cdE0C_0A010NWEndpointOtFTq` | 1 | proxyman |
| NetworkExtension.framework | `_$sSo13NENetworkRuleC16NetworkExtensionE011destinationC8Endpoint6prefix8protocolAB0C010NWEndpointO_SiSo0aB8ProtocolVtcfC` | 1 | proxyman |
| NetworkExtension.framework | `_$sSo13NENetworkRuleC16NetworkExtensionE06remoteC8Endpoint0E6Prefix05localcF00hG08protocol9directionAB0C010NWEndpointOSg_SiAMSiSo0aB8ProtocolVSo18NETrafficDirectionVtcfC` | 1 | proxyman |
| NetworkExtension.framework | `_$sSo14NEAppProxyFlowC16NetworkExtensionE11setMetadata2ony0D012NWParametersC_tF` | 1 | proxyman |
| NetworkExtension.framework | `_$sSo14NEAppProxyFlowC16NetworkExtensionE4open09withLocalC8Endpoint17completionHandlery0D010NWEndpointOSg_ys5Error_pSgctF` | 1 | proxyman |
| NetworkExtension.framework | `_$sSo14NEAppProxyFlowC16NetworkExtensionE9interface0D011NWInterfaceVSgvs` | 1 | proxyman |
| NetworkExtension.framework | `_$sSo17NEAppProxyTCPFlowC16NetworkExtensionE18remoteFlowEndpoint0D010NWEndpointOvg` | 1 | proxyman |
| NetworkExtension.framework | `_NETunnelProviderErrorDomain` | 1 | nordvpn |
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
| NetworkExtension.framework | `_OBJC_CLASS_$_NEVPNProtocol` | 1 | nordvpn |
| NetworkExtension.framework | `_OBJC_METACLASS_$_NEFilterDataProvider` | 1 | lulu |
| NetworkStatistics.framework | `_NStatManagerAddAllTCP` | 1 | lulu |
| NetworkStatistics.framework | `_NStatManagerAddAllUDP` | 1 | lulu |
| NetworkStatistics.framework | `_NStatManagerCreate` | 1 | lulu |
| NetworkStatistics.framework | `_NStatManagerDestroy` | 1 | lulu |
| NetworkStatistics.framework | `_NStatManagerQueryAllSources` | 1 | lulu |
| NetworkStatistics.framework | `_NStatManagerQueryAllSourcesDescriptions` | 1 | lulu |
| NetworkStatistics.framework | `_NStatManagerSetFlags` | 1 | lulu |
| NetworkStatistics.framework | `_NStatSourceSetDescriptionBlock` | 1 | lulu |
| NetworkStatistics.framework | `_NStatSourceSetRemovedBlock` | 1 | lulu |
| NetworkStatistics.framework | `_kNStatSrcKeyInterface` | 1 | lulu |
| NetworkStatistics.framework | `_kNStatSrcKeyLocal` | 1 | lulu |
| NetworkStatistics.framework | `_kNStatSrcKeyPID` | 1 | lulu |
| NetworkStatistics.framework | `_kNStatSrcKeyProvider` | 1 | lulu |
| NetworkStatistics.framework | `_kNStatSrcKeyRemote` | 1 | lulu |
| NetworkStatistics.framework | `_kNStatSrcKeyRxBytes` | 1 | lulu |
| NetworkStatistics.framework | `_kNStatSrcKeyTCPState` | 1 | lulu |
| NetworkStatistics.framework | `_kNStatSrcKeyTxBytes` | 1 | lulu |
| NetworkStatistics.framework | `_kNStatSrcKeyUUID` | 1 | lulu |
| SystemConfiguration.framework | `_SCCopyLastError` | 1 | nordvpn |
| SystemConfiguration.framework | `_SCDynamicStoreCopyLocation` | 1 | hammerspoon |
| SystemConfiguration.framework | `_SCNetworkConnectionCopyExtendedStatus` | 1 | istat-menus |
| SystemConfiguration.framework | `_SCNetworkConnectionCopyUserPreferences` | 1 | microsoft-outlook |
| SystemConfiguration.framework | `_SCNetworkInterfaceForceConfigurationRefresh` | 1 | istat-menus |
| SystemConfiguration.framework | `_SCNetworkProtocolGetEnabled` | 1 | nordvpn |
| SystemConfiguration.framework | `_SCNetworkProtocolSetConfiguration` | 1 | clash-verge-rev |
| SystemConfiguration.framework | `_SCNetworkSetSetCurrent` | 1 | hammerspoon |
| SystemConfiguration.framework | `_SCPreferencesCreateWithOptions` | 1 | hammerspoon |
| SystemConfiguration.framework | `_SCPreferencesPathSetValue` | 1 | proxyman |
| SystemConfiguration.framework | `_kSCDynamicStorePropNetInterfaces` | 1 | orion |
| SystemConfiguration.framework | `_kSCDynamicStorePropNetPrimaryService` | 1 | cloudflare-warp |
| SystemConfiguration.framework | `_kSCPropNetIPv4BroadcastAddresses` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCPropNetIPv6Flags` | 1 | cloudflare-warp |
| SystemConfiguration.framework | `_kSCPropNetModemConnectSpeed` | 1 | adobe-acrobat-reader |
| SystemConfiguration.framework | `_kSCPropNetPPPDialOnDemand` | 1 | microsoft-outlook |
| SystemConfiguration.framework | `_kSCValNetIPv4ConfigMethodAutomatic` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCValNetIPv4ConfigMethodBOOTP` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCValNetIPv4ConfigMethodINFORM` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCValNetIPv4ConfigMethodLinkLocal` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCValNetIPv4ConfigMethodManual` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCValNetIPv4ConfigMethodPPP` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCValNetIPv6ConfigMethod6to4` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCValNetIPv6ConfigMethodAutomatic` | 1 | teamviewer |
| SystemConfiguration.framework | `_kSCValNetIPv6ConfigMethodRouterAdvertisement` | 1 | teamviewer |
| libSystem.B.dylib | `_DNSServiceAddRecord` | 1 | cyberduck |
| libSystem.B.dylib | `_DNSServiceEnumerateDomains` | 1 | cyberduck |
| libSystem.B.dylib | `_DNSServiceRemoveRecord` | 1 | cyberduck |
| libSystem.B.dylib | `_DNSServiceUpdateRecord` | 1 | cyberduck |
| libSystem.B.dylib | `_TXTRecordContainsKey` | 1 | teamviewer |
| libSystem.B.dylib | `_TXTRecordGetItemAtIndex` | 1 | zoom |
| libSystem.B.dylib | `_TXTRecordGetValuePtr` | 1 | teamviewer |
| libSystem.B.dylib | `_dns_configuration_copy` | 1 | emacs-app |
| libSystem.B.dylib | `_dns_configuration_free` | 1 | emacs-app |
| libSystem.B.dylib | `_dns_configuration_notify_key` | 1 | emacs-app |
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
| libswiftNetwork.dylib | `_$s7Network11NWTXTRecordVMa` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network11NWTXTRecordVMn` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network11NWTXTRecordVyACSDyS2SGcfC` | 1 | utm |
| libswiftNetwork.dylib | `_$s7Network12NWConnectionC4send20discontiguousContent14contentContext10isComplete10completiony8Dispatch0K4DataV_AC0eG0CSbAC14SendCompletionOtF` | 1 | raycast |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC12ServiceClassO16interactiveVideoyA2EmFWC` | 1 | bettertouchtool |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC12ServiceClassOMa` | 1 | bettertouchtool |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC12serviceClassAC07ServiceD0Ovs` | 1 | bettertouchtool |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC13ProtocolStackC09transportC0AA17NWProtocolOptionsCSgvgTj` | 1 | bettertouchtool |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC13ProtocolStackC09transportC0AA17NWProtocolOptionsCSgvsTj` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC20MultipathServiceTypeO8handoveryA2EmFWC` | 1 | easydict |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC20prohibitedInterfacesSayAA11NWInterfaceVGSgvs` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network12NWParametersC3udpACvgZ` | 1 | duckduckgo |
| libswiftNetwork.dylib | `_$s7Network12NWProtocolIPC7OptionsC7VersionOMn` | 1 | istat-menus |
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
| self) | `_SCNetworkConnectionCreateWithServiceID` | 1 | dropbox |
| self) | `_SCPreferencesSetCallback` | 1 | dropbox |
| vmnet.framework | `_vmnet_enable_checksum_offload_key` | 1 | orbstack |
| vmnet.framework | `_vmnet_enable_tso_key` | 1 | orbstack |
| vmnet.framework | `_vmnet_host_ip_address_key` | 1 | orbstack |
| vmnet.framework | `_vmnet_host_ipv6_address_key` | 1 | orbstack |
| vmnet.framework | `_vmnet_host_subnet_mask_key` | 1 | orbstack |
| vmnet.framework | `_vmnet_mac_address_key` | 1 | docker-desktop |
