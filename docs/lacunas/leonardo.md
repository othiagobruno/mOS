# Lacunas de Leonardo (runtime C/C++)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Leonardo (runtime C/C++) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**1614** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| libSystem.B.dylib | `_putenv` | 35 | android-studio, basictex, comfy, cyberduck |
| libSystem.B.dylib | `_getloadavg` | 26 | android-studio, blender, calibre, cloudflare-warp |
| libSystem.B.dylib | `_getprotobyname` | 24 | adobe-acrobat-reader, android-studio, blender, calibre |
| libSystem.B.dylib | `_OSAtomicAdd32Barrier` | 19 | cmux, deluge, firefox, floorp |
| libSystem.B.dylib | `__os_activity_create` | 18 | audacity, bitwarden, calibre, db-browser-for-sqlite |
| libSystem.B.dylib | `_pthread_attr_getschedpolicy` | 18 | audacity, calibre, db-browser-for-sqlite, dolphin |
| libSystem.B.dylib | `_wcsstr` | 17 | adobe-acrobat-reader, android-studio, audacity, comfy |
| libcups.2.dylib | `_ppdClose` | 17 | adobe-acrobat-reader, bruno, calibre, db-browser-for-sqlite |
| libcups.2.dylib | `_ppdFindOption` | 17 | adobe-acrobat-reader, bruno, calibre, db-browser-for-sqlite |
| libSystem.B.dylib | `_hstrerror` | 16 | android-studio, basictex, blender, calibre |
| libSystem.B.dylib | `_if_freenameindex` | 16 | android-studio, blender, calibre, cloudflare-warp |
| libSystem.B.dylib | `_if_nameindex` | 16 | android-studio, blender, calibre, cloudflare-warp |
| libSystem.B.dylib | `__res` | 15 | adobe-acrobat-reader, audacity, calibre, dolphin |
| libSystem.B.dylib | `_pthread_setcanceltype` | 15 | adobe-acrobat-reader, deluge, dolphin, godot |
| libSystem.B.dylib | `_hash_create` | 14 | alt-tab, cyberduck, flycut, gemini |
| libSystem.B.dylib | `_hash_search` | 14 | alt-tab, cyberduck, flycut, gemini |
| libSystem.B.dylib | `_setpgrp` | 14 | android-studio, blender, calibre, deluge |
| libSystem.B.dylib | `_wcsxfrm` | 14 | android-studio, blender, calibre, emacs-app |
| libcups.2.dylib | `_ppdFindMarkedChoice` | 14 | adobe-acrobat-reader, bruno, calibre, db-browser-for-sqlite |
| libSystem.B.dylib | `_OSAtomicCompareAndSwapIntBarrier` | 13 | firefox, floorp, librewolf, microsoft-teams |
| libSystem.B.dylib | `_ctermid_r` | 13 | android-studio, blender, calibre, deluge |
| libSystem.B.dylib | `_getprotobynumber` | 13 | docker-desktop, emacs-app, firefox, floorp |
| libSystem.B.dylib | `_wcsrchr` | 13 | adobe-acrobat-reader, android-studio, blender, calibre |
| libSystem.B.dylib | `_wcstok` | 13 | adobe-acrobat-reader, android-studio, blender, calibre |
| libcups.2.dylib | `_cupsGetDests2` | 13 | adobe-acrobat-reader, bruno, firefox, floorp |
| libSystem.B.dylib | `_sethostname` | 12 | android-studio, blender, calibre, gimp |
| libSystem.B.dylib | `_dbm_clearerr` | 11 | android-studio, blender, calibre, gimp |
| libSystem.B.dylib | `_dbm_close` | 11 | android-studio, blender, calibre, gimp |
| libSystem.B.dylib | `_dbm_delete` | 11 | android-studio, blender, calibre, gimp |
| libSystem.B.dylib | `_dbm_error` | 11 | android-studio, blender, calibre, gimp |
| libSystem.B.dylib | `_dbm_fetch` | 11 | android-studio, blender, calibre, gimp |
| libSystem.B.dylib | `_dbm_firstkey` | 11 | android-studio, blender, calibre, gimp |
| libSystem.B.dylib | `_dbm_nextkey` | 11 | android-studio, blender, calibre, gimp |
| libSystem.B.dylib | `_dbm_open` | 11 | android-studio, blender, calibre, gimp |
| libSystem.B.dylib | `_dbm_store` | 11 | android-studio, blender, calibre, gimp |
| libSystem.B.dylib | `_getsectdatafromheader_64` | 11 | adobe-acrobat-reader, alt-tab, firefox, inkscape |
| libSystem.B.dylib | `_uuid_generate_time` | 11 | android-studio, blender, calibre, gimp |
| libcups.2.dylib | `_httpGetHostname` | 11 | firefox, floorp, gimp, inkscape |
| libcups.2.dylib | `_ppdOpenFile` | 11 | calibre, db-browser-for-sqlite, inkscape, krita |
| libSystem.B.dylib | `_gethostbyname2` | 10 | firefox, floorp, gimp, libreoffice |
| libedit.3.dylib | `_add_history` | 10 | blender, calibre, deluge, hammerspoon |
| libSystem.B.dylib | `_OSSpinLockLock` | 9 | alt-tab, gemini, kicad, microsoft-teams |
| libSystem.B.dylib | `_OSSpinLockUnlock` | 9 | alt-tab, gemini, kicad, microsoft-teams |
| libSystem.B.dylib | `_getsectbynamefromheader_64` | 9 | adobe-acrobat-reader, aldente, clipy, inkscape |
| libSystem.B.dylib | `_pthread_rwlockattr_destroy` | 9 | firefox, floorp, librewolf, microsoft-teams |
| libSystem.B.dylib | `_pthread_rwlockattr_init` | 9 | firefox, floorp, librewolf, microsoft-teams |
| libedit.3.dylib | `_read_history` | 9 | blender, calibre, hammerspoon, kicad |
| libedit.3.dylib | `_rl_attempted_completion_function` | 9 | blender, calibre, hammerspoon, kicad |
| libedit.3.dylib | `_write_history` | 9 | blender, calibre, hammerspoon, kicad |
| libexpat.1.dylib | `_XML_ParserFree` | 9 | github, iina, inkscape, keka |
| libexpat.1.dylib | `_XML_SetElementHandler` | 9 | github, iina, inkscape, keka |
| libexpat.1.dylib | `_XML_SetUserData` | 9 | github, iina, inkscape, keka |
| libicucore.A.dylib | `_u_strToLower` | 9 | bitwarden, gemini, monitorcontrol, mos |
| libicucore.A.dylib | `_u_strToUpper` | 9 | bitwarden, gemini, monitorcontrol, mos |
| libicucore.A.dylib | `_ubrk_close` | 9 | bitwarden, gemini, monitorcontrol, mos |
| libicucore.A.dylib | `_ubrk_following` | 9 | bitwarden, gemini, monitorcontrol, mos |
| libicucore.A.dylib | `_ubrk_open` | 9 | bitwarden, gemini, monitorcontrol, mos |
| libicucore.A.dylib | `_ubrk_setText` | 9 | bitwarden, gemini, monitorcontrol, mos |
| libicucore.A.dylib | `_unorm2_getNFCInstance` | 9 | bitwarden, gemini, monitorcontrol, mos |
| libicucore.A.dylib | `_unorm2_normalize` | 9 | bitwarden, gemini, monitorcontrol, mos |
| libncurses.5.4.dylib | `_setupterm` | 9 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_tigetnum` | 9 | blender, calibre, deluge, inkscape |
| libSystem.B.dylib | `_wcscasecmp` | 8 | adobe-acrobat-reader, audacity, krita, microsoft-teams |
| libcups.2.dylib | `_cupsConnectDest` | 8 | firefox, floorp, librewolf, mullvad-browser |
| libcups.2.dylib | `_cupsGetDestMediaByIndex` | 8 | firefox, floorp, librewolf, mullvad-browser |
| libcups.2.dylib | `_cupsGetDestMediaCount` | 8 | firefox, floorp, librewolf, mullvad-browser |
| libcups.2.dylib | `_cupsGetDestMediaDefault` | 8 | firefox, floorp, librewolf, mullvad-browser |
| libcups.2.dylib | `_cupsGetDests` | 8 | firefox, floorp, librewolf, mullvad-browser |
| libcups.2.dylib | `_cupsLocalizeDestMedia` | 8 | firefox, floorp, librewolf, mullvad-browser |
| libcups.2.dylib | `_httpAddrPort` | 8 | firefox, floorp, librewolf, mullvad-browser |
| libcups.2.dylib | `_httpBlocking` | 8 | adobe-acrobat-reader, bruno, gimp, inkscape |
| libcups.2.dylib | `_httpError` | 8 | adobe-acrobat-reader, bruno, gimp, inkscape |
| libcups.2.dylib | `_httpGetAddress` | 8 | firefox, floorp, librewolf, mullvad-browser |
| libcups.2.dylib | `_ppdFindAttr` | 8 | adobe-acrobat-reader, bruno, gimp, inkscape |
| libcups.2.dylib | `_ppdFindChoice` | 8 | adobe-acrobat-reader, bruno, gimp, inkscape |
| libcups.2.dylib | `_ppdMarkDefaults` | 8 | adobe-acrobat-reader, bruno, gimp, inkscape |
| libedit.3.dylib | `_history_truncate_file` | 8 | blender, calibre, hammerspoon, kicad |
| libedit.3.dylib | `_rl_attempted_completion_over` | 8 | blender, calibre, hammerspoon, kicad |
| libedit.3.dylib | `_rl_completion_append_character` | 8 | blender, calibre, hammerspoon, kicad |
| libedit.3.dylib | `_rl_completion_matches` | 8 | blender, calibre, hammerspoon, kicad |
| libedit.3.dylib | `_rl_line_buffer` | 8 | blender, calibre, kicad, kitty |
| libedit.3.dylib | `_rl_readline_name` | 8 | blender, calibre, kicad, kitty |
| libexpat.1.dylib | `_XML_ErrorString` | 8 | github, iina, inkscape, kicad |
| libexpat.1.dylib | `_XML_GetErrorCode` | 8 | github, iina, inkscape, kicad |
| libexpat.1.dylib | `_XML_ParserCreate` | 8 | github, iina, inkscape, keka |
| libexpat.1.dylib | `_XML_SetCharacterDataHandler` | 8 | github, iina, inkscape, keka |
| libncurses.5.4.dylib | `_tigetflag` | 8 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_tigetstr` | 8 | blender, calibre, deluge, inkscape |
| libSystem.B.dylib | `_pthread_key_init_np` | 7 | bitwarden, gemini, monitorcontrol, mos |
| libSystem.B.dylib | `_swap_fat_arch` | 7 | aldente, google-chrome, kicad, microsoft-edge |
| libSystem.B.dylib | `_timespec_get` | 7 | docker-desktop, fuse-t, kicad, obs |
| libSystem.B.dylib | `_wcslcpy` | 7 | dolphin, krita, moonlight, openrct2 |
| libSystem.B.dylib | `_wcsncat` | 7 | adobe-acrobat-reader, calibre, inkscape, kicad |
| libcups.2.dylib | `_ppdOpenFd` | 7 | adobe-acrobat-reader, bruno, gimp, inkscape |
| libedit.3.dylib | `_clear_history` | 7 | blender, calibre, kicad, kitty |
| libedit.3.dylib | `_emacs_meta_keymap` | 7 | blender, calibre, kicad, kitty |
| libedit.3.dylib | `_history_get` | 7 | blender, calibre, kicad, kitty |
| libedit.3.dylib | `_history_get_history_state` | 7 | blender, calibre, kicad, kitty |
| libedit.3.dylib | `_remove_history` | 7 | blender, calibre, kicad, kitty |
| libedit.3.dylib | `_replace_history_entry` | 7 | blender, calibre, kicad, kitty |
| libedit.3.dylib | `_rl_bind_key` | 7 | blender, calibre, kicad, kitty |
| libedit.3.dylib | `_rl_bind_key_in_map` | 7 | blender, calibre, kicad, kitty |
| libedit.3.dylib | `_rl_callback_handler_install` | 7 | blender, calibre, kicad, kitty |
| libedit.3.dylib | `_rl_callback_handler_remove` | 7 | blender, calibre, kicad, kitty |
| libedit.3.dylib | `_rl_callback_read_char` | 7 | blender, calibre, kicad, kitty |
| libedit.3.dylib | `_rl_cleanup_after_signal` | 7 | blender, calibre, kicad, kitty |
| libedit.3.dylib | `_rl_complete` | 7 | blender, calibre, kicad, kitty |
| libedit.3.dylib | `_rl_completer_word_break_characters` | 7 | blender, calibre, kicad, kitty |
| libedit.3.dylib | `_rl_completion_display_matches_hook` | 7 | blender, calibre, kicad, kitty |
| libedit.3.dylib | `_rl_completion_type` | 7 | blender, calibre, kicad, kitty |
| libedit.3.dylib | `_rl_free_line_state` | 7 | blender, calibre, kicad, kitty |
| libedit.3.dylib | `_rl_initialize` | 7 | blender, calibre, kicad, kitty |
| libedit.3.dylib | `_rl_insert` | 7 | blender, calibre, kicad, kitty |
| libedit.3.dylib | `_rl_insert_text` | 7 | blender, calibre, kicad, kitty |
| libedit.3.dylib | `_rl_instream` | 7 | blender, calibre, kicad, kitty |
| libedit.3.dylib | `_rl_library_version` | 7 | blender, calibre, kicad, kitty |
| libedit.3.dylib | `_rl_outstream` | 7 | blender, calibre, kicad, kitty |
| libedit.3.dylib | `_rl_parse_and_bind` | 7 | blender, calibre, kicad, kitty |
| libedit.3.dylib | `_rl_pre_input_hook` | 7 | blender, calibre, kicad, kitty |
| libedit.3.dylib | `_rl_prep_terminal` | 7 | blender, calibre, kicad, kitty |
| libedit.3.dylib | `_rl_read_init_file` | 7 | blender, calibre, kicad, kitty |
| libedit.3.dylib | `_rl_readline_version` | 7 | blender, calibre, kicad, kitty |
| libedit.3.dylib | `_rl_redisplay` | 7 | blender, calibre, kicad, kitty |
| libedit.3.dylib | `_rl_startup_hook` | 7 | blender, calibre, kicad, kitty |
| libedit.3.dylib | `_rl_variable_bind` | 7 | blender, calibre, kicad, kitty |
| libedit.3.dylib | `_using_history` | 7 | blender, calibre, kicad, kitty |
| libicucore.A.dylib | `_u_charAge` | 7 | bitwarden, gemini, monitorcontrol, mos |
| libicucore.A.dylib | `_u_charName` | 7 | bitwarden, gemini, monitorcontrol, mos |
| libicucore.A.dylib | `_u_getNumericValue` | 7 | bitwarden, gemini, monitorcontrol, mos |
| libicucore.A.dylib | `_u_strToTitle` | 7 | bitwarden, gemini, monitorcontrol, mos |
| libicucore.A.dylib | `_ubrk_preceding` | 7 | bitwarden, gemini, monitorcontrol, mos |
| libicucore.A.dylib | `_ubrk_setUText` | 7 | bitwarden, gemini, monitorcontrol, mos |
| libicucore.A.dylib | `_unorm2_hasBoundaryBefore` | 7 | bitwarden, gemini, monitorcontrol, mos |
| libicucore.A.dylib | `_unorm2_spanQuickCheckYes` | 7 | bitwarden, gemini, monitorcontrol, mos |
| libicucore.A.dylib | `_utext_openUChars` | 7 | bitwarden, gemini, monitorcontrol, mos |
| libicucore.A.dylib | `_utext_openUTF8` | 7 | bitwarden, gemini, monitorcontrol, mos |
| libncurses.5.4.dylib | `_COLORS` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_COLOR_PAIRS` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_COLS` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_LINES` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_acs_map` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_baudrate` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_beep` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_can_change_color` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_cbreak` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_clearok` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_color_content` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_copywin` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_curs_set` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_def_prog_mode` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_def_shell_mode` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_delay_output` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_delwin` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_derwin` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_doupdate` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_echo` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_endwin` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_erasechar` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_filter` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_flash` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_flushinp` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_getmouse` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_getwin` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_halfdelay` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_has_colors` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_has_ic` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_has_il` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_has_key` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_idcok` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_idlok` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_immedok` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_init_color` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_init_pair` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_initscr` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_intrflush` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_is_linetouched` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_is_term_resized` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_is_wintouched` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_isendwin` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_keyname` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_keypad` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_killchar` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_leaveok` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_longname` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_meta` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_mouseinterval` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_mousemask` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_mvderwin` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_mvwin` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_napms` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_newpad` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_newscr` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_newwin` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_nl` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_nocbreak` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_nodelay` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_noecho` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_nonl` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_noqiflush` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_noraw` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_notimeout` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_overlay` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_overwrite` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_pair_content` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_pechochar` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_pnoutrefresh` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_prefresh` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_putp` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_putwin` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_qiflush` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_raw` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_reset_prog_mode` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_reset_shell_mode` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_resetty` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_resize_term` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_resizeterm` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_savetty` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_scrollok` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_start_color` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_stdscr` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_subpad` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_subwin` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_syncok` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_termattrs` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_termname` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_tparm` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_typeahead` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_unctrl` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_ungetch` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_ungetmouse` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_use_default_colors` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_use_env` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_waddch` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_waddnstr` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_wattr_off` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_wattr_on` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_wbkgd` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_wbkgdset` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_wborder` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_wchgat` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_wclear` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_wclrtobot` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_wclrtoeol` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_wcursyncup` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_wdelch` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_wechochar` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_wenclose` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_werase` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_wgetch` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_wgetnstr` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_whline` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_winch` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_winnstr` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_winsch` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_winsdelln` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_winsnstr` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_wmove` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_wnoutrefresh` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_wredrawln` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_wrefresh` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_wresize` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_wscrl` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_wsetscrreg` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_wsyncdown` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_wsyncup` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_wtimeout` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_wtouchln` | 7 | blender, calibre, deluge, inkscape |
| libncurses.5.4.dylib | `_wvline` | 7 | blender, calibre, deluge, inkscape |
| libSystem.B.dylib | `___invert_f4` | 6 | gemini, monitorcontrol, mos, raycast |
| libSystem.B.dylib | `_csqrt` | 6 | blender, calibre, gimp, inkscape |
| libSystem.B.dylib | `_drand48` | 6 | clipy, gimp, iterm2, krita |
| libSystem.B.dylib | `_exc_server` | 6 | cmux, ghostty, inkscape, openttd |
| libSystem.B.dylib | `_fputws` | 6 | adobe-acrobat-reader, audacity, keka, kicad |
| libSystem.B.dylib | `_tmpnam` | 6 | calibre, deluge, google-chrome, intellij-idea |
| libSystem.B.dylib | `_wcscspn` | 6 | adobe-acrobat-reader, android-studio, gimp, libreoffice |
| libSystem.B.dylib | `_wcslcat` | 6 | dolphin, krita, moonlight, openrct2 |
| libcups.2.dylib | `_cupsSetUser` | 6 | adobe-acrobat-reader, gimp, inkscape, meld |
| libedit.3.dylib | `_rl_basic_word_break_characters` | 6 | blender, calibre, kitty, krita |
| libexpat.1.dylib | `_XML_GetBuffer` | 6 | iina, inkscape, kicad, kitty |
| libexpat.1.dylib | `_XML_ParseBuffer` | 6 | iina, inkscape, kicad, kitty |
| libffi.dylib | `_ffi_call` | 6 | calibre, kicad, kitty, krita |
| libffi.dylib | `_ffi_prep_cif` | 6 | calibre, kicad, kitty, krita |
| libncurses.5.4.dylib | `_ESCDELAY` | 6 | blender, calibre, inkscape, kitty |
| libncurses.5.4.dylib | `_TABSIZE` | 6 | blender, calibre, inkscape, kitty |
| libncurses.5.4.dylib | `_set_escdelay` | 6 | blender, calibre, inkscape, kitty |
| libncurses.5.4.dylib | `_set_tabsize` | 6 | blender, calibre, inkscape, kitty |
| libncurses.5.4.dylib | `_tgetent` | 6 | inkscape, keepassxc, lmms, meld |
| libncurses.5.4.dylib | `_tgetflag` | 6 | inkscape, keepassxc, lmms, meld |
| libncurses.5.4.dylib | `_tgetnum` | 6 | inkscape, keepassxc, lmms, meld |
| libncurses.5.4.dylib | `_tgetstr` | 6 | inkscape, keepassxc, lmms, meld |
| libncurses.5.4.dylib | `_tgoto` | 6 | inkscape, keepassxc, lmms, meld |
| libncurses.5.4.dylib | `_tputs` | 6 | inkscape, keepassxc, lmms, meld |
| libpanel.5.4.dylib | `_bottom_panel` | 6 | blender, calibre, inkscape, kitty |
| libpanel.5.4.dylib | `_del_panel` | 6 | blender, calibre, inkscape, kitty |
| libpanel.5.4.dylib | `_hide_panel` | 6 | blender, calibre, inkscape, kitty |
| libpanel.5.4.dylib | `_move_panel` | 6 | blender, calibre, inkscape, kitty |
| libpanel.5.4.dylib | `_new_panel` | 6 | blender, calibre, inkscape, kitty |
| libpanel.5.4.dylib | `_panel_above` | 6 | blender, calibre, inkscape, kitty |
| libpanel.5.4.dylib | `_panel_below` | 6 | blender, calibre, inkscape, kitty |
| libpanel.5.4.dylib | `_panel_hidden` | 6 | blender, calibre, inkscape, kitty |
| libpanel.5.4.dylib | `_panel_userptr` | 6 | blender, calibre, inkscape, kitty |
| libpanel.5.4.dylib | `_replace_panel` | 6 | blender, calibre, inkscape, kitty |
| libpanel.5.4.dylib | `_set_panel_userptr` | 6 | blender, calibre, inkscape, kitty |
| libpanel.5.4.dylib | `_show_panel` | 6 | blender, calibre, inkscape, kitty |
| libpanel.5.4.dylib | `_top_panel` | 6 | blender, calibre, inkscape, kitty |
| libpanel.5.4.dylib | `_update_panels` | 6 | blender, calibre, inkscape, kitty |
| libSystem.B.dylib | `_OSAtomicCompareAndSwap64Barrier` | 5 | deluge, godot, openscad, pixelorama |
| libSystem.B.dylib | `___divdc3` | 5 | basictex, krita, libreoffice, shotcut |
| libSystem.B.dylib | `___invert_d2` | 5 | gemini, monitorcontrol, mos, rustdesk |
| libSystem.B.dylib | `___invert_d3` | 5 | gemini, monitorcontrol, mos, rustdesk |
| libSystem.B.dylib | `___invert_d4` | 5 | gemini, monitorcontrol, mos, rustdesk |
| libSystem.B.dylib | `___invert_f2` | 5 | gemini, monitorcontrol, mos, rustdesk |
| libSystem.B.dylib | `_csqrtf` | 5 | blender, calibre, gimp, inkscape |
| libSystem.B.dylib | `_csqrtl` | 5 | blender, calibre, gimp, inkscape |
| libSystem.B.dylib | `_devname` | 5 | adobe-acrobat-reader, calibre, emacs-app, utm |
| libSystem.B.dylib | `_getpass` | 5 | basictex, comfy, keka, sequel-ace |
| libSystem.B.dylib | `_swap_fat_header` | 5 | aldente, google-chrome, microsoft-edge, orbstack |
| libSystem.B.dylib | `_wcsncasecmp` | 5 | krita, microsoft-teams, openrct2, shotcut |
| libcups.2.dylib | `_cupsGetDest` | 5 | adobe-acrobat-reader, bruno, openmtp, pencil |
| libcups.2.dylib | `_cupsGetPPD` | 5 | adobe-acrobat-reader, bruno, openmtp, pencil |
| libcups.2.dylib | `_cupsGetPPD2` | 5 | adobe-acrobat-reader, bruno, openmtp, pencil |
| libcups.2.dylib | `_cupsMarkOptions` | 5 | adobe-acrobat-reader, bruno, openmtp, pencil |
| libcups.2.dylib | `_httpConnectEncrypt` | 5 | adobe-acrobat-reader, bruno, openmtp, pencil |
| libcups.2.dylib | `_ppdErrorString` | 5 | adobe-acrobat-reader, bruno, openmtp, pencil |
| libcups.2.dylib | `_ppdLastError` | 5 | adobe-acrobat-reader, bruno, openmtp, pencil |
| libexpat.1.dylib | `_XML_GetCurrentLineNumber` | 5 | iina, inkscape, kicad, kitty |
| libffi.dylib | `_ffi_closure_alloc` | 5 | calibre, kitty, krita, libreoffice |
| libffi.dylib | `_ffi_closure_free` | 5 | calibre, kitty, krita, libreoffice |
| libffi.dylib | `_ffi_prep_cif_var` | 5 | calibre, kitty, krita, libreoffice |
| libffi.dylib | `_ffi_prep_closure_loc` | 5 | calibre, kitty, krita, libreoffice |
| libffi.dylib | `_ffi_type_double` | 5 | calibre, kitty, krita, libreoffice |
| libffi.dylib | `_ffi_type_float` | 5 | calibre, kitty, krita, libreoffice |
| libffi.dylib | `_ffi_type_longdouble` | 5 | calibre, kitty, krita, libreoffice |
| libffi.dylib | `_ffi_type_pointer` | 5 | calibre, kitty, krita, libreoffice |
| libffi.dylib | `_ffi_type_sint16` | 5 | calibre, kitty, krita, libreoffice |
| libffi.dylib | `_ffi_type_sint32` | 5 | calibre, kitty, krita, libreoffice |
| libffi.dylib | `_ffi_type_sint64` | 5 | calibre, kitty, krita, libreoffice |
| libffi.dylib | `_ffi_type_sint8` | 5 | calibre, kitty, krita, libreoffice |
| libffi.dylib | `_ffi_type_uint16` | 5 | calibre, kitty, krita, libreoffice |
| libffi.dylib | `_ffi_type_uint32` | 5 | calibre, kitty, krita, libreoffice |
| libffi.dylib | `_ffi_type_uint64` | 5 | calibre, kitty, krita, libreoffice |
| libffi.dylib | `_ffi_type_uint8` | 5 | calibre, kitty, krita, libreoffice |
| libffi.dylib | `_ffi_type_void` | 5 | calibre, kitty, krita, libreoffice |
| libiconv.2.dylib | `_libiconv_set_relocation_prefix` | 5 | deluge, krita, lmms, openscad |
| libncurses.5.4.dylib | `_BC` | 5 | inkscape, keepassxc, lmms, meld |
| libncurses.5.4.dylib | `_PC` | 5 | inkscape, keepassxc, lmms, meld |
| libncurses.5.4.dylib | `_UP` | 5 | inkscape, keepassxc, lmms, meld |
| libncurses.5.4.dylib | `_setcchar` | 5 | android-studio, blender, inkscape, kitty |
| libncurses.5.4.dylib | `_unget_wch` | 5 | android-studio, blender, inkscape, kitty |
| libncurses.5.4.dylib | `_wadd_wch` | 5 | android-studio, blender, inkscape, kitty |
| libncurses.5.4.dylib | `_waddnwstr` | 5 | android-studio, blender, inkscape, kitty |
| libncurses.5.4.dylib | `_wget_wch` | 5 | android-studio, blender, inkscape, kitty |
| libncurses.5.4.dylib | `_wins_nwstr` | 5 | android-studio, blender, inkscape, kitty |
| Tcl.framework | `_TclFreeObj` | 4 | deluge, hex-fiend, inkscape, meld |
| Tcl.framework | `_Tcl_AddErrorInfo` | 4 | deluge, hex-fiend, inkscape, meld |
| Tcl.framework | `_Tcl_CreateInterp` | 4 | deluge, hex-fiend, inkscape, meld |
| Tcl.framework | `_Tcl_DeleteInterp` | 4 | deluge, hex-fiend, inkscape, meld |
| Tcl.framework | `_Tcl_Eval` | 4 | deluge, hex-fiend, inkscape, meld |
| Tcl.framework | `_Tcl_EvalFile` | 4 | deluge, hex-fiend, inkscape, meld |
| Tcl.framework | `_Tcl_Free` | 4 | deluge, hex-fiend, inkscape, meld |
| Tcl.framework | `_Tcl_GetByteArrayFromObj` | 4 | deluge, hex-fiend, inkscape, meld |
| Tcl.framework | `_Tcl_GetLongFromObj` | 4 | deluge, hex-fiend, inkscape, meld |
| Tcl.framework | `_Tcl_GetString` | 4 | deluge, hex-fiend, inkscape, meld |
| Tcl.framework | `_Tcl_GetStringFromObj` | 4 | deluge, hex-fiend, inkscape, meld |
| Tcl.framework | `_Tcl_GetStringResult` | 4 | deluge, hex-fiend, inkscape, meld |
| Tcl.framework | `_Tcl_Init` | 4 | deluge, hex-fiend, inkscape, meld |
| Tcl.framework | `_Tcl_NewBooleanObj` | 4 | deluge, hex-fiend, inkscape, meld |
| Tcl.framework | `_Tcl_NewByteArrayObj` | 4 | deluge, hex-fiend, inkscape, meld |
| Tcl.framework | `_Tcl_NewDoubleObj` | 4 | deluge, hex-fiend, inkscape, meld |
| Tcl.framework | `_Tcl_NewStringObj` | 4 | deluge, hex-fiend, inkscape, meld |
| Tcl.framework | `_Tcl_NewWideIntObj` | 4 | deluge, hex-fiend, inkscape, meld |
| Tcl.framework | `_Tcl_SetObjResult` | 4 | deluge, hex-fiend, inkscape, meld |
| libSystem.B.dylib | `_NSAddressOfSymbol` | 4 | adobe-acrobat-reader, keka, kicad, pgadmin4 |
| libSystem.B.dylib | `_NSLookupAndBindSymbol` | 4 | adobe-acrobat-reader, inkscape, kicad, meld |
| libSystem.B.dylib | `_OSAtomicIncrement64Barrier` | 4 | cmux, gemini, hex-fiend, vorssaint |
| libSystem.B.dylib | `___divsc3` | 4 | krita, ollama-app, openclaw, shotcut |
| libSystem.B.dylib | `_aio_cancel` | 4 | adobe-acrobat-reader, cloudflare-warp, localsend, virtualbox |
| libSystem.B.dylib | `_catanh` | 4 | blender, inkscape, kicad, tic80 |
| libSystem.B.dylib | `_cexp` | 4 | blender, inkscape, lmms, tic80 |
| libSystem.B.dylib | `_clnt_create` | 4 | blender, inkscape, kicad, meld |
| libSystem.B.dylib | `_clnt_spcreateerror` | 4 | blender, inkscape, kicad, meld |
| libSystem.B.dylib | `_cpow` | 4 | basictex, blender, inkscape, tic80 |
| libSystem.B.dylib | `_dbopen` | 4 | android-studio, cyberduck, intellij-idea, temurin |
| libSystem.B.dylib | `_fmtcheck` | 4 | lmms, shotcut, tableplus, zoom |
| libSystem.B.dylib | `_fwprintf` | 4 | adobe-acrobat-reader, audacity, kicad, prusaslicer |
| libSystem.B.dylib | `_inet_net_pton` | 4 | openmtp, pencil, webtorrent, xcodes-app |
| libSystem.B.dylib | `_nl_langinfo_l` | 4 | kicad, pgadmin4, tableplus, virtualbox |
| libSystem.B.dylib | `_readpassphrase` | 4 | keka, tor-browser, utm, vagrant |
| libSystem.B.dylib | `_swab` | 4 | adobe-acrobat-reader, krita, shotcut, vlc |
| libSystem.B.dylib | `_swscanf` | 4 | adobe-acrobat-reader, audacity, kicad, prusaslicer |
| libSystem.B.dylib | `_wcsspn` | 4 | adobe-acrobat-reader, audacity, kicad, prusaslicer |
| libSystem.B.dylib | `_xdr_enum` | 4 | blender, inkscape, kicad, meld |
| libSystem.B.dylib | `_xdr_pointer` | 4 | blender, inkscape, kicad, meld |
| libSystem.B.dylib | `_xdr_string` | 4 | blender, inkscape, kicad, meld |
| libSystem.B.dylib | `_yp_all` | 4 | blender, inkscape, kicad, meld |
| libSystem.B.dylib | `_yp_get_default_domain` | 4 | blender, inkscape, kicad, meld |
| libSystem.B.dylib | `_yp_master` | 4 | blender, inkscape, kicad, meld |
| libSystem.B.dylib | `_yp_match` | 4 | blender, inkscape, kicad, meld |
| libSystem.B.dylib | `_yperr_string` | 4 | blender, inkscape, kicad, meld |
| libcups.2.dylib | `_cupsRemoveDest` | 4 | adobe-acrobat-reader, bruno, openmtp, pencil |
| libedit.3.dylib | `_readline` | 4 | deluge, hammerspoon, openmtp, pgadmin4 |
| libexpat.1.dylib | `_XML_Parse` | 4 | github, keka, kitty, sourcetree |
| libexpat.1.dylib | `_XML_SetDoctypeDeclHandler` | 4 | iina, inkscape, kicad, meld |
| libicucore.A.dylib | `_u_errorName` | 4 | iterm2, openclaw, opencode-desktop, sequel-ace |
| libsqlite3.dylib | `_sqlite3_snapshot_cmp` | 4 | applite, clipy, openclaw, protonvpn |
| libsqlite3.dylib | `_sqlite3_snapshot_free` | 4 | applite, clipy, openclaw, protonvpn |
| libsqlite3.dylib | `_sqlite3_snapshot_get` | 4 | applite, clipy, openclaw, protonvpn |
| libsqlite3.dylib | `_sqlite3_snapshot_open` | 4 | applite, clipy, openclaw, protonvpn |
| Tcl.framework | `_TclBN_mp_clear` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_TclBN_mp_init` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_TclBN_mp_read_radix` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_TclBN_mp_to_unsigned_bin_n` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_TclBN_mp_unsigned_bin_size` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_AttemptAlloc` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_ConditionFinalize` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_ConditionNotify` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_ConditionWait` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_CreateFileHandler` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_CreateObjCommand` | 3 | hex-fiend, inkscape, meld |
| Tcl.framework | `_Tcl_CreateTimerHandler` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_DeleteCommand` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_DeleteFileHandler` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_DeleteTimerHandler` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_DoOneEvent` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_EvalObjv` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_ExprBoolean` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_ExprDouble` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_ExprLong` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_ExprString` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_FindExecutable` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_GetBignumFromObj` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_GetBoolean` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_GetBooleanFromObj` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_GetCurrentThread` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_GetDouble` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_GetDoubleFromObj` | 3 | hex-fiend, inkscape, meld |
| Tcl.framework | `_Tcl_GetObjResult` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_GetObjType` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_GetThreadData` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_GetVar` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_GetVar2Ex` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_GetWideIntFromObj` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_ListObjGetElements` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_ListObjIndex` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_ListObjLength` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_MutexLock` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_MutexUnlock` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_NewBignumObj` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_NewListObj` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_NewLongObj` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_RecordAndEval` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_ResetResult` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_SetVar` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_SetVar2` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_SetVar2Ex` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_SplitList` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_ThreadAlert` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_ThreadQueueEvent` | 3 | deluge, inkscape, meld |
| Tcl.framework | `_Tcl_UnsetVar2` | 3 | deluge, inkscape, meld |
| Tk.framework | `_Tk_GetNumMainWindows` | 3 | deluge, inkscape, meld |
| Tk.framework | `_Tk_Init` | 3 | deluge, inkscape, meld |
| Tk.framework | `_Tk_MainWindow` | 3 | deluge, inkscape, meld |
| libSystem.B.dylib | `_NSLibraryNameForModule` | 3 | inkscape, kicad, meld |
| libSystem.B.dylib | `_NSLinkEditError` | 3 | keka, kicad, pgadmin4 |
| libSystem.B.dylib | `_NSLinkModule` | 3 | keka, kicad, pgadmin4 |
| libSystem.B.dylib | `_NSLookupSymbolInModule` | 3 | keka, kicad, pgadmin4 |
| libSystem.B.dylib | `_NSModuleForSymbol` | 3 | inkscape, kicad, meld |
| libSystem.B.dylib | `_NSUnLinkModule` | 3 | keka, kicad, pgadmin4 |
| libSystem.B.dylib | `_NXGetArchInfoFromName` | 3 | openttd, steam, vlc |
| libSystem.B.dylib | `_OSAtomicDequeue` | 3 | hex-fiend, microsoft-teams, obs |
| libSystem.B.dylib | `_OSAtomicEnqueue` | 3 | hex-fiend, microsoft-teams, obs |
| libSystem.B.dylib | `___truncsfhf2` | 3 | moonlight, raycast, zoom |
| libSystem.B.dylib | `__sigtramp` | 3 | openmtp, pencil, webtorrent |
| libSystem.B.dylib | `_cacos` | 3 | blender, inkscape, tic80 |
| libSystem.B.dylib | `_cacosh` | 3 | blender, inkscape, tic80 |
| libSystem.B.dylib | `_casin` | 3 | blender, inkscape, tic80 |
| libSystem.B.dylib | `_casinh` | 3 | blender, inkscape, tic80 |
| libSystem.B.dylib | `_catan` | 3 | blender, inkscape, tic80 |
| libSystem.B.dylib | `_ccos` | 3 | blender, inkscape, tic80 |
| libSystem.B.dylib | `_ccosh` | 3 | blender, inkscape, tic80 |
| libSystem.B.dylib | `_clog` | 3 | blender, inkscape, tic80 |
| libSystem.B.dylib | `_csin` | 3 | blender, inkscape, tic80 |
| libSystem.B.dylib | `_csinh` | 3 | blender, inkscape, tic80 |
| libSystem.B.dylib | `_ctan` | 3 | blender, inkscape, tic80 |
| libSystem.B.dylib | `_ctanh` | 3 | blender, inkscape, tic80 |
| libSystem.B.dylib | `_devname_r` | 3 | calibre, hex-fiend, vnote |
| libSystem.B.dylib | `_duplocale` | 3 | gimp, krita, thunderbird |
| libSystem.B.dylib | `_errc` | 3 | brave-browser, google-chrome, microsoft-edge |
| libSystem.B.dylib | `_fgetws` | 3 | adobe-acrobat-reader, keka, microsoft-teams |
| libSystem.B.dylib | `_fmemopen` | 3 | gimp, libreoffice, shotcut |
| libSystem.B.dylib | `_funopen` | 3 | gimp, libreoffice, utm |
| libSystem.B.dylib | `_getsectdatafromheader` | 3 | alt-tab, microsoft-teams, windows-app |
| libSystem.B.dylib | `_getwchar` | 3 | android-studio, insomnia, ollama-app |
| libSystem.B.dylib | `_lio_listio` | 3 | cloudflare-warp, localsend, virtualbox |
| libSystem.B.dylib | `_regcomp_l` | 3 | comfy, iterm2, kicad |
| libSystem.B.dylib | `_strunvis` | 3 | android-studio, gimp, tableplus |
| libSystem.B.dylib | `_strvis` | 3 | android-studio, gimp, tableplus |
| libSystem.B.dylib | `_swap_load_command` | 3 | google-chrome, microsoft-edge, steam |
| libSystem.B.dylib | `_swap_mach_header` | 3 | google-chrome, microsoft-edge, steam |
| libSystem.B.dylib | `_swap_segment_command` | 3 | google-chrome, microsoft-edge, steam |
| libSystem.B.dylib | `_swapcontext` | 3 | blender, kicad, prusaslicer |
| libSystem.B.dylib | `_tempnam` | 3 | adobe-acrobat-reader, deluge, vlc |
| libSystem.B.dylib | `_vfprintf_l` | 3 | inkscape, kicad, prusaslicer |
| libSystem.B.dylib | `_vis` | 3 | android-studio, gimp, tableplus |
| libSystem.B.dylib | `_wcsnlen` | 3 | dolphin, moonlight, zoom |
| libSystem.B.dylib | `_wcstod_l` | 3 | audacity, kicad, prusaslicer |
| libSystem.B.dylib | `_wcstol_l` | 3 | audacity, kicad, prusaslicer |
| libSystem.B.dylib | `_wcstoul_l` | 3 | audacity, kicad, prusaslicer |
| libSystem.B.dylib | `_wprintf` | 3 | adobe-acrobat-reader, audacity, kicad |
| libbz2.1.0.dylib | `_BZ2_bzWriteClose` | 3 | deluge, sequel-ace, tableplus |
| libbz2.1.0.dylib | `_BZ2_bzread` | 3 | inkscape, sequel-ace, tableplus |
| libbz2.1.0.dylib | `_BZ2_bzwrite` | 3 | inkscape, sequel-ace, tableplus |
| libcups.2.dylib | `_cupsArrayFirst` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_cupsCloseDestJob` | 3 | adobe-acrobat-reader, openmtp, pencil |
| libcups.2.dylib | `_cupsCreateDestJob` | 3 | adobe-acrobat-reader, openmtp, pencil |
| libcups.2.dylib | `_cupsDoAuthentication` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_cupsEncryption` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_cupsFinishDestDocument` | 3 | adobe-acrobat-reader, openmtp, pencil |
| libcups.2.dylib | `_cupsFreeOptions` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_cupsGetDestMediaByName` | 3 | adobe-acrobat-reader, openmtp, pencil |
| libcups.2.dylib | `_cupsLangDefault` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_cupsLangFree` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_cupsParseOptions` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_cupsSetEncryption` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_cupsSetPasswordCB` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_cupsStartDestDocument` | 3 | adobe-acrobat-reader, openmtp, pencil |
| libcups.2.dylib | `_httpAddrFreeList` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_httpAddrGetList` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_httpAddrLength` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_httpAssembleURIf` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_httpClearFields` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_httpEncryption` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_httpFlush` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_httpGet` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_httpGetAuthString` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_httpGetFd` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_httpGetField` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_httpGetLength2` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_httpPost` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_httpRead2` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_httpReconnect2` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_httpSeparateURI` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_httpSetField` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_httpUpdate` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_httpWrite2` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_ippAddResolution` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_ippErrorString` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_ippGetBoolean` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_ippGetOperation` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_ippGetValueTag` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_ippLength` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_ippRead` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_ippSetOperation` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_ippSetRequestId` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_ippSetState` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_ippWrite` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_ppdConflicts` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_ppdFindCustomOption` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_ppdFirstCustomParam` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_ppdLocalize` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_ppdMarkOption` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_ppdNextCustomParam` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_ppdPageSize` | 3 | gimp, inkscape, meld |
| libncurses.5.4.dylib | `_curses_version` | 3 | calibre, kitty, krita |
| libIOReport.dylib | `_IOReportChannelGetChannelName` | 2 | figma, stats |
| libIOReport.dylib | `_IOReportChannelGetGroup` | 2 | figma, stats |
| libIOReport.dylib | `_IOReportChannelGetSubGroup` | 2 | figma, stats |
| libIOReport.dylib | `_IOReportChannelGetUnitLabel` | 2 | figma, stats |
| libIOReport.dylib | `_IOReportCopyChannelsInGroup` | 2 | figma, stats |
| libIOReport.dylib | `_IOReportCreateSamples` | 2 | figma, stats |
| libIOReport.dylib | `_IOReportCreateSamplesDelta` | 2 | figma, stats |
| libIOReport.dylib | `_IOReportCreateSubscription` | 2 | figma, stats |
| libIOReport.dylib | `_IOReportMergeChannels` | 2 | figma, stats |
| libIOReport.dylib | `_IOReportStateGetCount` | 2 | figma, stats |
| libIOReport.dylib | `_IOReportStateGetNameForIndex` | 2 | figma, stats |
| libIOReport.dylib | `_IOReportStateGetResidency` | 2 | figma, stats |
| libSystem.B.dylib | `_NSCreateObjectFileImageFromMemory` | 2 | kicad, pgadmin4 |
| libSystem.B.dylib | `_NSDestroyObjectFileImage` | 2 | kicad, pgadmin4 |
| libSystem.B.dylib | `_NSLookupSymbolInImage` | 2 | kicad, pgadmin4 |
| libSystem.B.dylib | `_NXFindBestFatArch` | 2 | kicad, pgadmin4 |
| libSystem.B.dylib | `_OSAtomicAdd64Barrier` | 2 | cmux, vorssaint |
| libSystem.B.dylib | `_OSAtomicAnd32` | 2 | audacity, lmms |
| libSystem.B.dylib | `_OSAtomicIncrement64` | 2 | gemini, hex-fiend |
| libSystem.B.dylib | `_OSAtomicOr32` | 2 | audacity, lmms |
| libSystem.B.dylib | `___divxc3` | 2 | krita, shotcut |
| libSystem.B.dylib | `___floatuntixf` | 2 | hex-fiend, kicad |
| libSystem.B.dylib | `___mulxc3` | 2 | krita, shotcut |
| libSystem.B.dylib | `___udivmodti4` | 2 | kicad, meld |
| libSystem.B.dylib | `__os_log_create` | 2 | monitorcontrol, wireshark-app |
| libSystem.B.dylib | `__os_log_internal` | 2 | monitorcontrol, wireshark-app |
| libSystem.B.dylib | `_acct` | 2 | cloudflare-warp, localsend |
| libSystem.B.dylib | `_cabs` | 2 | blender, inkscape |
| libSystem.B.dylib | `_cabsf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_cabsl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_cacosf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_cacoshf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_cacoshl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_cacosl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_carg` | 2 | blender, inkscape |
| libSystem.B.dylib | `_cargf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_cargl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_casinf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_casinhf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_casinhl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_casinl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_catanf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_catanhf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_catanhl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_catanl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_ccosf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_ccoshf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_ccoshl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_ccosl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_cexpf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_cexpl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_clock_get_attributes` | 2 | mongodb-compass, unity-hub |
| libSystem.B.dylib | `_clogf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_clogl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_cpowf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_cpowl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_csinf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_csinhf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_csinhl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_csinl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_ctanf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_ctanhf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_ctanhl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_ctanl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_daemon$1050` | 2 | vlc, xcodes-app |
| libSystem.B.dylib | `_filesec_free` | 2 | openclaw, utm |
| libSystem.B.dylib | `_filesec_get_property` | 2 | openclaw, utm |
| libSystem.B.dylib | `_filesec_init` | 2 | openclaw, utm |
| libSystem.B.dylib | `_fstatfs64` | 2 | docker-desktop, orbstack |
| libSystem.B.dylib | `_fstatx_np$INODE64` | 2 | openclaw, utm |
| libSystem.B.dylib | `_ftime` | 2 | adobe-acrobat-reader, zoom |
| libSystem.B.dylib | `_getmntinfo_r_np$INODE64` | 2 | brave-browser, cmux |
| libSystem.B.dylib | `_getsectbynamefromheader` | 2 | clipy, sequel-ace |
| libSystem.B.dylib | `_kSBXProfilePureComputation` | 2 | calibre, vnote |
| libSystem.B.dylib | `_open_memstream` | 2 | shotcut, utm |
| libSystem.B.dylib | `_pthread_chdir_np` | 2 | 1password, adobe-acrobat-reader |
| libSystem.B.dylib | `_pthread_cpu_number_np` | 2 | calibre, vnote |
| libSystem.B.dylib | `_pthread_setconcurrency` | 2 | kicad, libreoffice |
| libSystem.B.dylib | `_sbrk` | 2 | godot, pixelorama |
| libSystem.B.dylib | `_statx_np$INODE64` | 2 | openclaw, utm |
| libSystem.B.dylib | `_strptime_l` | 2 | orbstack, sourcetree |
| libSystem.B.dylib | `_swap_linkedit_data_command` | 2 | google-chrome, microsoft-edge |
| libSystem.B.dylib | `_swap_mach_header_64` | 2 | google-chrome, microsoft-edge |
| libSystem.B.dylib | `_swap_segment_command_64` | 2 | google-chrome, microsoft-edge |
| libSystem.B.dylib | `_swap_symtab_command` | 2 | google-chrome, microsoft-edge |
| libSystem.B.dylib | `_unmount` | 2 | docker-desktop, orbstack |
| libSystem.B.dylib | `_vfwprintf` | 2 | adobe-acrobat-reader, microsoft-teams |
| libSystem.B.dylib | `_vprintf_l` | 2 | kicad, prusaslicer |
| libSystem.B.dylib | `_vsprintf_l` | 2 | kicad, prusaslicer |
| libSystem.B.dylib | `_wcstombs_l` | 2 | adobe-acrobat-reader, zoom |
| libX11.6.dylib | `_XChangeProperty` | 2 | basictex, xquartz |
| libX11.6.dylib | `_XCreateSimpleWindow` | 2 | basictex, xquartz |
| libX11.6.dylib | `_XDestroyWindow` | 2 | basictex, xquartz |
| libX11.6.dylib | `_XFlush` | 2 | basictex, xquartz |
| libX11.6.dylib | `_XFree` | 2 | basictex, xquartz |
| libX11.6.dylib | `_XGetAtomName` | 2 | basictex, xquartz |
| libX11.6.dylib | `_XGetWindowProperty` | 2 | basictex, xquartz |
| libX11.6.dylib | `_XInternAtom` | 2 | basictex, xquartz |
| libX11.6.dylib | `_XNextEvent` | 2 | basictex, xquartz |
| libX11.6.dylib | `_XOpenDisplay` | 2 | basictex, xquartz |
| libX11.6.dylib | `_XSelectInput` | 2 | basictex, xquartz |
| libX11.6.dylib | `_XSendEvent` | 2 | basictex, xquartz |
| libX11.6.dylib | `_XSetErrorHandler` | 2 | basictex, xquartz |
| libX11.6.dylib | `_XSetIOErrorHandler` | 2 | basictex, xquartz |
| libcups.2.dylib | `_httpWait` | 2 | gimp, inkscape |
| libexpat.1.dylib | `_XML_ExpatVersion` | 2 | keka, kitty |
| libexslt.0.dylib | `_exsltRegisterAll` | 2 | inkscape, libreoffice |
| libffi.dylib | `_ffi_type_complex_double` | 2 | calibre, kitty |
| libffi.dylib | `_ffi_type_complex_float` | 2 | calibre, kitty |
| libffi.dylib | `_ffi_type_complex_longdouble` | 2 | calibre, kitty |
| libicucore.A.dylib | `_u_charDirection` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_u_charType` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_u_getUnicodeVersion` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_u_getVersion` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_u_isUWhiteSpace` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_u_strlen` | 2 | iterm2, sequel-ace |
| libicucore.A.dylib | `_u_toupper` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_u_versionToString` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ubrk_clone` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ubrk_countAvailable` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ubrk_current` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ubrk_first` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ubrk_getAvailable` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ubrk_getRuleStatus` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ubrk_next` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ubrk_previous` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucal_add` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucal_clear` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucal_clone` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucal_close` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucal_get` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucal_getAttribute` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucal_getCanonicalTimeZoneID` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucal_getDayOfWeekType` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucal_getHostTimeZone` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucal_getKeywordValuesForLocale` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucal_getLimit` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucal_getMillis` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucal_getTimeZoneDisplayName` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucal_getTimeZoneOffsetFromLocal` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucal_getTimeZoneTransitionDate` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucal_open` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucal_openTimeZoneIDEnumeration` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucal_openTimeZones` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucal_set` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucal_setGregorianChange` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucal_setMillis` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucal_setTimeZone` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucfpos_close` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucfpos_constrainCategory` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucfpos_constrainField` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucfpos_getCategory` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucfpos_getField` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucfpos_getIndexes` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucfpos_open` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucfpos_reset` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucol_close` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucol_countAvailable` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucol_getAttribute` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucol_getAvailable` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucol_getKeywordValues` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucol_getKeywordValuesForLocale` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucol_getRules` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucol_open` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucol_setAttribute` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucol_strcoll` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucol_strcollUTF8` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucurr_getName` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucurr_openISOCurrencies` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_udat_applyPattern` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_udat_clone` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_udat_close` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_udat_format` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_udat_formatForFields` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_udat_getCalendar` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_udat_open` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_udat_toPattern` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_udatpg_close` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_udatpg_getBestPatternWithOptions` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_udatpg_getFieldDisplayName` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_udatpg_getSkeleton` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_udatpg_open` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_udtitvfmt_close` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_udtitvfmt_closeResult` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_udtitvfmt_formatCalendarToResult` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_udtitvfmt_formatToResult` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_udtitvfmt_open` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_udtitvfmt_openResult` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_udtitvfmt_resultAsValue` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uenum_close` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uenum_count` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uenum_next` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uenum_unext` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ufieldpositer_close` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ufieldpositer_next` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ufieldpositer_open` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ufmtval_getString` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ufmtval_nextPosition` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uidna_nameToASCII` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uidna_nameToUnicode` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uldn_close` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uldn_keyValueDisplayName` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uldn_localeDisplayName` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uldn_openForContext` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uldn_regionDisplayName` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uldn_scriptDisplayName` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ulistfmt_close` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ulistfmt_closeResult` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ulistfmt_format` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ulistfmt_formatStringsToResult` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ulistfmt_openForType` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ulistfmt_openResult` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ulistfmt_resultAsValue` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uloc_addLikelySubtags` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uloc_canonicalize` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uloc_countAvailable` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uloc_forLanguageTag` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uloc_getAvailable` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uloc_getBaseName` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uloc_getCharacterOrientation` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uloc_getCountry` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uloc_getDefault` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uloc_getKeywordValue` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uloc_getLanguage` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uloc_getScript` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uloc_minimizeSubtags` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uloc_openKeywords` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uloc_setDefault` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uloc_setKeywordValue` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uloc_toLanguageTag` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uloc_toUnicodeLocaleType` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_unorm2_getInstance` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_unorm2_getNFDInstance` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_unorm2_getNFKCInstance` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_unorm2_getNFKDInstance` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_unorm2_isNormalized` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_unum_close` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_unum_open` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_unum_setAttribute` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_unumf_close` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_unumf_closeResult` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_unumf_formatDecimal` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_unumf_formatDouble` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_unumf_openForSkeletonAndLocale` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_unumf_openResult` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_unumf_resultGetAllFieldPositions` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_unumf_resultToString` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_unumrf_close` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_unumrf_closeResult` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_unumrf_formatDecimalRange` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_unumrf_formatDoubleRange` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_unumrf_openForSkeletonWithCollapseAndIdentityFallback` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_unumrf_openResult` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_unumrf_resultAsValue` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_unumsys_close` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_unumsys_getName` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_unumsys_isAlgorithmic` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_unumsys_open` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_unumsys_openAvailableNames` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_unumsys_openByName` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uplrules_close` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uplrules_getKeywords` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uplrules_openForType` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uplrules_selectForRange` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uplrules_selectFormatted` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_uregex_appendReplacement` | 2 | iterm2, sequel-ace |
| libicucore.A.dylib | `_uregex_appendTail` | 2 | iterm2, sequel-ace |
| libicucore.A.dylib | `_uregex_clone` | 2 | iterm2, sequel-ace |
| libicucore.A.dylib | `_uregex_close` | 2 | iterm2, sequel-ace |
| libicucore.A.dylib | `_uregex_end` | 2 | iterm2, sequel-ace |
| libicucore.A.dylib | `_uregex_find` | 2 | iterm2, sequel-ace |
| libicucore.A.dylib | `_uregex_findNext` | 2 | iterm2, sequel-ace |
| libicucore.A.dylib | `_uregex_groupCount` | 2 | iterm2, sequel-ace |
| libicucore.A.dylib | `_uregex_open` | 2 | iterm2, sequel-ace |
| libicucore.A.dylib | `_uregex_reset` | 2 | iterm2, sequel-ace |
| libicucore.A.dylib | `_uregex_setText` | 2 | iterm2, sequel-ace |
| libicucore.A.dylib | `_uregex_start` | 2 | iterm2, sequel-ace |
| libicucore.A.dylib | `_ureldatefmt_close` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ureldatefmt_closeResult` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ureldatefmt_format` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ureldatefmt_formatNumeric` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ureldatefmt_formatNumericToResult` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ureldatefmt_formatToResult` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ureldatefmt_open` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ureldatefmt_openResult` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ureldatefmt_resultAsValue` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ures_close` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ures_getByKey` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ures_getStringByKey` | 2 | openclaw, opencode-desktop |
| libicucore.A.dylib | `_ures_open` | 2 | openclaw, opencode-desktop |
| libncurses.5.4.dylib | `_assume_default_colors` | 2 | calibre, kitty |
| libpcap.A.dylib | `_pcap_activate` | 2 | cloudflare-warp, wireshark-app |
| libpcap.A.dylib | `_pcap_close` | 2 | cloudflare-warp, wireshark-app |
| libpcap.A.dylib | `_pcap_create` | 2 | cloudflare-warp, wireshark-app |
| libpcap.A.dylib | `_pcap_findalldevs` | 2 | cloudflare-warp, wireshark-app |
| libpcap.A.dylib | `_pcap_freealldevs` | 2 | cloudflare-warp, wireshark-app |
| libpcap.A.dylib | `_pcap_geterr` | 2 | cloudflare-warp, wireshark-app |
| libpcap.A.dylib | `_pcap_set_snaplen` | 2 | cloudflare-warp, wireshark-app |
| libpcap.A.dylib | `_pcap_set_timeout` | 2 | cloudflare-warp, wireshark-app |
| libsasl2.2.dylib | `_sasl_dispose` | 2 | tableplus, utm |
| libsasl2.2.dylib | `_sasl_errdetail` | 2 | tableplus, utm |
| libsasl2.2.dylib | `_sasl_errstring` | 2 | tableplus, utm |
| libxslt.1.dylib | `_xsltApplyStylesheet` | 2 | inkscape, libreoffice |
| libxslt.1.dylib | `_xsltApplyStylesheetUser` | 2 | inkscape, libreoffice |
| libxslt.1.dylib | `_xsltFreeSecurityPrefs` | 2 | inkscape, libreoffice |
| libxslt.1.dylib | `_xsltFreeStylesheet` | 2 | inkscape, libreoffice |
| libxslt.1.dylib | `_xsltFreeTransformContext` | 2 | inkscape, libreoffice |
| libxslt.1.dylib | `_xsltGenericError` | 2 | inkscape, libreoffice |
| libxslt.1.dylib | `_xsltGenericErrorContext` | 2 | inkscape, libreoffice |
| libxslt.1.dylib | `_xsltNewSecurityPrefs` | 2 | inkscape, libreoffice |
| libxslt.1.dylib | `_xsltNewTransformContext` | 2 | inkscape, libreoffice |
| libxslt.1.dylib | `_xsltParseStylesheetDoc` | 2 | inkscape, libreoffice |
| libxslt.1.dylib | `_xsltSaveResultTo` | 2 | inkscape, libreoffice |
| libxslt.1.dylib | `_xsltSaveResultToString` | 2 | inkscape, libreoffice |
| libxslt.1.dylib | `_xsltSecurityAllow` | 2 | inkscape, libreoffice |
| libxslt.1.dylib | `_xsltSecurityForbid` | 2 | inkscape, libreoffice |
| libxslt.1.dylib | `_xsltSetCtxtSecurityPrefs` | 2 | inkscape, libreoffice |
| libxslt.1.dylib | `_xsltSetSecurityPrefs` | 2 | inkscape, libreoffice |
| Tcl.framework | `_Tcl_Alloc` | 1 | hex-fiend |
| Tcl.framework | `_Tcl_AppendPrintfToObj` | 1 | hex-fiend |
| Tcl.framework | `_Tcl_AppendToObj` | 1 | hex-fiend |
| Tcl.framework | `_Tcl_CreateCommand` | 1 | deluge |
| Tcl.framework | `_Tcl_DictObjGet` | 1 | hex-fiend |
| Tcl.framework | `_Tcl_EvalObjEx` | 1 | hex-fiend |
| Tcl.framework | `_Tcl_GetCharLength` | 1 | deluge |
| Tcl.framework | `_Tcl_GetCommandInfo` | 1 | hex-fiend |
| Tcl.framework | `_Tcl_GetIntFromObj` | 1 | hex-fiend |
| Tcl.framework | `_Tcl_GetReturnOptions` | 1 | hex-fiend |
| Tcl.framework | `_Tcl_GetTime` | 1 | hex-fiend |
| Tcl.framework | `_Tcl_GetUnicode` | 1 | deluge |
| Tcl.framework | `_Tcl_GlobalEval` | 1 | deluge |
| Tcl.framework | `_Tcl_LimitSetTime` | 1 | hex-fiend |
| Tcl.framework | `_Tcl_LimitTypeSet` | 1 | hex-fiend |
| Tcl.framework | `_Tcl_Merge` | 1 | deluge |
| Tcl.framework | `_Tcl_NewIntObj` | 1 | hex-fiend |
| Tcl.framework | `_Tcl_NewUnicodeObj` | 1 | deluge |
| Tcl.framework | `_Tcl_ObjPrintf` | 1 | hex-fiend |
| Tcl.framework | `_Tcl_Realloc` | 1 | hex-fiend |
| Tcl.framework | `_Tcl_SetErrno` | 1 | hex-fiend |
| Tcl.framework | `_Tcl_WrongNumArgs` | 1 | hex-fiend |
| libAppleWM.7.dylib | `_XAppleWMQueryExtension` | 1 | xquartz |
| libAppleWM.7.dylib | `_XAppleWMSelectInput` | 1 | xquartz |
| libIOReport.dylib | `_IOReportCopyAllChannels` | 1 | figma |
| libSystem.B.dylib | `_NSCreateObjectFileImageFromFile` | 1 | keka |
| libSystem.B.dylib | `_NSIsSymbolNameDefined` | 1 | adobe-acrobat-reader |
| libSystem.B.dylib | `_OSAtomicAdd64` | 1 | hex-fiend |
| libSystem.B.dylib | `_OSAtomicCompareAndSwap32` | 1 | protonvpn |
| libSystem.B.dylib | `_OSAtomicDecrement64` | 1 | hex-fiend |
| libSystem.B.dylib | `_OSAtomicTestAndSetBarrier` | 1 | gemini |
| libSystem.B.dylib | `_SANDBOX_CHECK_NO_REPORT` | 1 | visual-studio-code |
| libSystem.B.dylib | `__NSConcreteAutoBlock` | 1 | localsend |
| libSystem.B.dylib | `__NSConcreteFinalizingBlock` | 1 | localsend |
| libSystem.B.dylib | `__NSConcreteWeakBlockVariable` | 1 | localsend |
| libSystem.B.dylib | `__NSGetProgname` | 1 | kitty |
| libSystem.B.dylib | `___atomic_exchange` | 1 | spotify |
| libSystem.B.dylib | `___powidf2` | 1 | shotcut |
| libSystem.B.dylib | `___powisf2` | 1 | shotcut |
| libSystem.B.dylib | `___pthread_chdir` | 1 | android-studio |
| libSystem.B.dylib | `___sandbox_ms` | 1 | adobe-acrobat-reader |
| libSystem.B.dylib | `__keymgr_get_and_lock_processwide_ptr` | 1 | inkscape |
| libSystem.B.dylib | `__keymgr_set_and_unlock_processwide_ptr` | 1 | inkscape |
| libSystem.B.dylib | `__keymgr_unlock_processwide_ptr` | 1 | inkscape |
| libSystem.B.dylib | `__os_activity_current` | 1 | bitwarden |
| libSystem.B.dylib | `__os_activity_initiate` | 1 | alt-tab |
| libSystem.B.dylib | `__os_activity_label_useraction` | 1 | protonvpn |
| libSystem.B.dylib | `__os_trace_with_buffer` | 1 | alt-tab |
| libSystem.B.dylib | `__simd_pow_f4` | 1 | dolphin |
| libSystem.B.dylib | `__simd_round_d2` | 1 | linearmouse |
| libSystem.B.dylib | `_arc4random_addrandom` | 1 | tor-browser |
| libSystem.B.dylib | `_asl_log_descriptor` | 1 | xquartz |
| libSystem.B.dylib | `_chmodx_np` | 1 | utm |
| libSystem.B.dylib | `_coalition_info_resource_usage` | 1 | adobe-acrobat-reader |
| libSystem.B.dylib | `_daemon` | 1 | utm |
| libSystem.B.dylib | `_dirstat_np` | 1 | telegram |
| libSystem.B.dylib | `_endhostent` | 1 | vlc |
| libSystem.B.dylib | `_endprotoent` | 1 | emacs-app |
| libSystem.B.dylib | `_erand48` | 1 | vlc |
| libSystem.B.dylib | `_ether_aton` | 1 | docker-desktop |
| libSystem.B.dylib | `_fchmodx_np` | 1 | utm |
| libSystem.B.dylib | `_ffsctl` | 1 | utm |
| libSystem.B.dylib | `_filesec_dup` | 1 | utm |
| libSystem.B.dylib | `_filesec_query_property` | 1 | openclaw |
| libSystem.B.dylib | `_filesec_set_property` | 1 | utm |
| libSystem.B.dylib | `_freehostent` | 1 | deluge |
| libSystem.B.dylib | `_gcvt` | 1 | qlmarkdown |
| libSystem.B.dylib | `_get_end` | 1 | inkscape |
| libSystem.B.dylib | `_get_etext` | 1 | inkscape |
| libSystem.B.dylib | `_getaudit_addr` | 1 | android-studio |
| libSystem.B.dylib | `_getipnodebyaddr` | 1 | deluge |
| libSystem.B.dylib | `_getipnodebyname` | 1 | deluge |
| libSystem.B.dylib | `_getopt_long_only` | 1 | android-studio |
| libSystem.B.dylib | `_getprotoent` | 1 | emacs-app |
| libSystem.B.dylib | `_imaxabs` | 1 | android-studio |
| libSystem.B.dylib | `_in6addr_linklocal_allnodes` | 1 | telegram |
| libSystem.B.dylib | `_in6addr_linklocal_allrouters` | 1 | telegram |
| libSystem.B.dylib | `_in6addr_nodelocal_allnodes` | 1 | telegram |
| libSystem.B.dylib | `_inet_nsap_addr` | 1 | emacs-app |
| libSystem.B.dylib | `_inet_nsap_ntoa` | 1 | emacs-app |
| libSystem.B.dylib | `_jrand48` | 1 | vlc |
| libSystem.B.dylib | `_lfind` | 1 | wireshark-app |
| libSystem.B.dylib | `_lrand48` | 1 | telegram |
| libSystem.B.dylib | `_lstatx_np$INODE64` | 1 | utm |
| libSystem.B.dylib | `_minherit` | 1 | tor-browser |
| libSystem.B.dylib | `_mount` | 1 | orbstack |
| libSystem.B.dylib | `_nrand48` | 1 | vlc |
| libSystem.B.dylib | `_os_trace_debug_enabled` | 1 | alt-tab |
| libSystem.B.dylib | `_pthread_getconcurrency` | 1 | libreoffice |
| libSystem.B.dylib | `_putwc` | 1 | inkscape |
| libSystem.B.dylib | `_putwchar` | 1 | keka |
| libSystem.B.dylib | `_sethostent` | 1 | vlc |
| libSystem.B.dylib | `_setprotoent` | 1 | emacs-app |
| libSystem.B.dylib | `_si_async_call` | 1 | emacs-app |
| libSystem.B.dylib | `_si_async_cancel` | 1 | emacs-app |
| libSystem.B.dylib | `_si_async_handle_reply` | 1 | emacs-app |
| libSystem.B.dylib | `_si_item_release` | 1 | emacs-app |
| libSystem.B.dylib | `_si_module_release` | 1 | emacs-app |
| libSystem.B.dylib | `_si_module_with_name` | 1 | emacs-app |
| libSystem.B.dylib | `_strcasecmp_l` | 1 | blender |
| libSystem.B.dylib | `_strfmon_l` | 1 | kicad |
| libSystem.B.dylib | `_swap_dylib_command` | 1 | steam |
| libSystem.B.dylib | `_swap_section` | 1 | steam |
| libSystem.B.dylib | `_sys_errlist` | 1 | vlc |
| libSystem.B.dylib | `_sys_siglist` | 1 | tableplus |
| libSystem.B.dylib | `_vswscanf` | 1 | kicad |
| libSystem.B.dylib | `_vwprintf` | 1 | adobe-acrobat-reader |
| libSystem.B.dylib | `_wordexp` | 1 | anydesk |
| libSystem.B.dylib | `_wordfree` | 1 | anydesk |
| libX11.6.dylib | `_XAllocColor` | 1 | basictex |
| libX11.6.dylib | `_XAllocColorCells` | 1 | basictex |
| libX11.6.dylib | `_XBell` | 1 | basictex |
| libX11.6.dylib | `_XChangeGC` | 1 | basictex |
| libX11.6.dylib | `_XCheckMaskEvent` | 1 | basictex |
| libX11.6.dylib | `_XCheckTypedEvent` | 1 | basictex |
| libX11.6.dylib | `_XClearArea` | 1 | basictex |
| libX11.6.dylib | `_XClearWindow` | 1 | basictex |
| libX11.6.dylib | `_XCloseDisplay` | 1 | xquartz |
| libX11.6.dylib | `_XConfigureWindow` | 1 | basictex |
| libX11.6.dylib | `_XConvertSelection` | 1 | xquartz |
| libX11.6.dylib | `_XCopyArea` | 1 | basictex |
| libX11.6.dylib | `_XCopyColormapAndFree` | 1 | basictex |
| libX11.6.dylib | `_XCopyPlane` | 1 | basictex |
| libX11.6.dylib | `_XCreateBitmapFromData` | 1 | basictex |
| libX11.6.dylib | `_XCreateColormap` | 1 | basictex |
| libX11.6.dylib | `_XCreateFontCursor` | 1 | basictex |
| libX11.6.dylib | `_XCreateGC` | 1 | basictex |
| libX11.6.dylib | `_XCreateImage` | 1 | basictex |
| libX11.6.dylib | `_XCreatePixmap` | 1 | basictex |
| libX11.6.dylib | `_XCreatePixmapCursor` | 1 | basictex |
| libX11.6.dylib | `_XCreateRegion` | 1 | basictex |
| libX11.6.dylib | `_XCreateWindow` | 1 | basictex |
| libX11.6.dylib | `_XDefineCursor` | 1 | basictex |
| libX11.6.dylib | `_XDeleteProperty` | 1 | basictex |
| libX11.6.dylib | `_XDestroyRegion` | 1 | basictex |
| libX11.6.dylib | `_XDisplayKeycodes` | 1 | basictex |
| libX11.6.dylib | `_XDisplayString` | 1 | basictex |
| libX11.6.dylib | `_XDrawImageString` | 1 | basictex |
| libX11.6.dylib | `_XDrawLine` | 1 | basictex |
| libX11.6.dylib | `_XDrawLines` | 1 | basictex |
| libX11.6.dylib | `_XDrawPoint` | 1 | basictex |
| libX11.6.dylib | `_XDrawRectangle` | 1 | basictex |
| libX11.6.dylib | `_XDrawSegments` | 1 | basictex |
| libX11.6.dylib | `_XFillPolygon` | 1 | basictex |
| libX11.6.dylib | `_XFillRectangle` | 1 | basictex |
| libX11.6.dylib | `_XFillRectangles` | 1 | basictex |
| libX11.6.dylib | `_XFreeColors` | 1 | basictex |
| libX11.6.dylib | `_XFreeGC` | 1 | basictex |
| libX11.6.dylib | `_XFreePixmap` | 1 | basictex |
| libX11.6.dylib | `_XGeometry` | 1 | basictex |
| libX11.6.dylib | `_XGetDefault` | 1 | basictex |
| libX11.6.dylib | `_XGetErrorDatabaseText` | 1 | basictex |
| libX11.6.dylib | `_XGetErrorText` | 1 | basictex |
| libX11.6.dylib | `_XGetIconSizes` | 1 | basictex |
| libX11.6.dylib | `_XGetInputFocus` | 1 | basictex |
| libX11.6.dylib | `_XGetSelectionOwner` | 1 | xquartz |
| libX11.6.dylib | `_XGetVisualInfo` | 1 | basictex |
| libX11.6.dylib | `_XGetWindowAttributes` | 1 | basictex |
| libX11.6.dylib | `_XInstallColormap` | 1 | basictex |
| libX11.6.dylib | `_XInternAtoms` | 1 | basictex |
| libX11.6.dylib | `_XIntersectRegion` | 1 | basictex |
| libX11.6.dylib | `_XKeysymToString` | 1 | basictex |
| libX11.6.dylib | `_XListDepths` | 1 | basictex |
| libX11.6.dylib | `_XListPixmapFormats` | 1 | basictex |
| libX11.6.dylib | `_XLoadQueryFont` | 1 | basictex |
| libX11.6.dylib | `_XLookupColor` | 1 | basictex |
| libX11.6.dylib | `_XLookupString` | 1 | basictex |
| libX11.6.dylib | `_XMapRaised` | 1 | basictex |
| libX11.6.dylib | `_XMapWindow` | 1 | basictex |
| libX11.6.dylib | `_XMaxRequestSize` | 1 | basictex |
| libX11.6.dylib | `_XMoveWindow` | 1 | basictex |
| libX11.6.dylib | `_XParseColor` | 1 | basictex |
| libX11.6.dylib | `_XParseGeometry` | 1 | basictex |
| libX11.6.dylib | `_XPending` | 1 | xquartz |
| libX11.6.dylib | `_XPutImage` | 1 | basictex |
| libX11.6.dylib | `_XQueryBestSize` | 1 | basictex |
| libX11.6.dylib | `_XQueryColor` | 1 | basictex |
| libX11.6.dylib | `_XQueryColors` | 1 | basictex |
| libX11.6.dylib | `_XQueryPointer` | 1 | basictex |
| libX11.6.dylib | `_XRaiseWindow` | 1 | basictex |
| libX11.6.dylib | `_XResizeWindow` | 1 | basictex |
| libX11.6.dylib | `_XScreenNumberOfScreen` | 1 | basictex |
| libX11.6.dylib | `_XSetClipMask` | 1 | basictex |
| libX11.6.dylib | `_XSetClipRectangles` | 1 | basictex |
| libX11.6.dylib | `_XSetForeground` | 1 | basictex |
| libX11.6.dylib | `_XSetRegion` | 1 | basictex |
| libX11.6.dylib | `_XSetSelectionOwner` | 1 | xquartz |
| libX11.6.dylib | `_XSetStandardProperties` | 1 | basictex |
| libX11.6.dylib | `_XSetWMHints` | 1 | basictex |
| libX11.6.dylib | `_XSetWMProtocols` | 1 | basictex |
| libX11.6.dylib | `_XSetWindowBackground` | 1 | basictex |
| libX11.6.dylib | `_XSetWindowBorderWidth` | 1 | basictex |
| libX11.6.dylib | `_XStoreColor` | 1 | basictex |
| libX11.6.dylib | `_XSubtractRegion` | 1 | basictex |
| libX11.6.dylib | `_XSync` | 1 | basictex |
| libX11.6.dylib | `_XTextWidth` | 1 | basictex |
| libX11.6.dylib | `_XTranslateCoordinates` | 1 | basictex |
| libX11.6.dylib | `_XUnionRectWithRegion` | 1 | basictex |
| libX11.6.dylib | `_XUnionRegion` | 1 | basictex |
| libX11.6.dylib | `_XUnmapWindow` | 1 | basictex |
| libX11.6.dylib | `_XVisualIDFromVisual` | 1 | basictex |
| libX11.6.dylib | `_XWarpPointer` | 1 | basictex |
| libX11.6.dylib | `_XWithdrawWindow` | 1 | basictex |
| libX11.6.dylib | `_XmbTextListToTextProperty` | 1 | basictex |
| libX11.6.dylib | `_XrmCombineFileDatabase` | 1 | basictex |
| libX11.6.dylib | `_XrmGetDatabase` | 1 | basictex |
| libX11.6.dylib | `_XrmMergeDatabases` | 1 | basictex |
| libX11.6.dylib | `_XrmParseCommand` | 1 | basictex |
| libX11.6.dylib | `_XrmPutFileDatabase` | 1 | basictex |
| libX11.6.dylib | `_XrmPutResource` | 1 | basictex |
| libX11.6.dylib | `_XrmPutStringResource` | 1 | basictex |
| libX11.6.dylib | `_Xutf8TextListToTextProperty` | 1 | xquartz |
| libXau.6.dylib | `_XauDisposeAuth` | 1 | xquartz |
| libXau.6.dylib | `_XauReadAuth` | 1 | xquartz |
| libXaw.7.dylib | `_FMT8BIT` | 1 | basictex |
| libXaw.7.dylib | `_XawFmt8Bit` | 1 | basictex |
| libXaw.7.dylib | `_XawInitializeWidgetSet` | 1 | basictex |
| libXaw.7.dylib | `_XawListChange` | 1 | basictex |
| libXaw.7.dylib | `_XawListHighlight` | 1 | basictex |
| libXaw.7.dylib | `_XawListShowCurrent` | 1 | basictex |
| libXaw.7.dylib | `_XawScrollbarSetThumb` | 1 | basictex |
| libXaw.7.dylib | `_XawTextDisplayCaret` | 1 | basictex |
| libXaw.7.dylib | `_XawTextGetInsertionPoint` | 1 | basictex |
| libXaw.7.dylib | `_XawTextReplace` | 1 | basictex |
| libXaw.7.dylib | `_XawTextSetInsertionPoint` | 1 | basictex |
| libXaw.7.dylib | `_XawToggleGetCurrent` | 1 | basictex |
| libXaw.7.dylib | `_XawToggleSetCurrent` | 1 | basictex |
| libXaw.7.dylib | `_XawViewportSetCoordinates` | 1 | basictex |
| libXaw.7.dylib | `_asciiTextWidgetClass` | 1 | basictex |
| libXaw.7.dylib | `_commandWidgetClass` | 1 | basictex |
| libXaw.7.dylib | `_formWidgetClass` | 1 | basictex |
| libXaw.7.dylib | `_labelWidgetClass` | 1 | basictex |
| libXaw.7.dylib | `_listWidgetClass` | 1 | basictex |
| libXaw.7.dylib | `_menuButtonWidgetClass` | 1 | basictex |
| libXaw.7.dylib | `_panedWidgetClass` | 1 | basictex |
| libXaw.7.dylib | `_scrollbarWidgetClass` | 1 | basictex |
| libXaw.7.dylib | `_simpleMenuWidgetClass` | 1 | basictex |
| libXaw.7.dylib | `_smeBSBObjectClass` | 1 | basictex |
| libXaw.7.dylib | `_smeLineObjectClass` | 1 | basictex |
| libXaw.7.dylib | `_toggleClassRec` | 1 | basictex |
| libXaw.7.dylib | `_viewportWidgetClass` | 1 | basictex |
| libXdmcp.6.dylib | `_XdmcpARRAY8Equal` | 1 | xquartz |
| libXdmcp.6.dylib | `_XdmcpAllocARRAY8` | 1 | xquartz |
| libXdmcp.6.dylib | `_XdmcpCompareKeys` | 1 | xquartz |
| libXdmcp.6.dylib | `_XdmcpDecrementKey` | 1 | xquartz |
| libXdmcp.6.dylib | `_XdmcpDisposeARRAY16` | 1 | xquartz |
| libXdmcp.6.dylib | `_XdmcpDisposeARRAY8` | 1 | xquartz |
| libXdmcp.6.dylib | `_XdmcpDisposeARRAYofARRAY8` | 1 | xquartz |
| libXdmcp.6.dylib | `_XdmcpFill` | 1 | xquartz |
| libXdmcp.6.dylib | `_XdmcpFlush` | 1 | xquartz |
| libXdmcp.6.dylib | `_XdmcpGenerateKey` | 1 | xquartz |
| libXdmcp.6.dylib | `_XdmcpReadARRAY8` | 1 | xquartz |
| libXdmcp.6.dylib | `_XdmcpReadCARD32` | 1 | xquartz |
| libXdmcp.6.dylib | `_XdmcpReadCARD8` | 1 | xquartz |
| libXdmcp.6.dylib | `_XdmcpReadHeader` | 1 | xquartz |
| libXdmcp.6.dylib | `_XdmcpReallocARRAY16` | 1 | xquartz |
| libXdmcp.6.dylib | `_XdmcpReallocARRAYofARRAY8` | 1 | xquartz |
| libXdmcp.6.dylib | `_XdmcpUnwrap` | 1 | xquartz |
| libXdmcp.6.dylib | `_XdmcpWrap` | 1 | xquartz |
| libXdmcp.6.dylib | `_XdmcpWriteARRAY8` | 1 | xquartz |
| libXdmcp.6.dylib | `_XdmcpWriteARRAYofARRAY8` | 1 | xquartz |
| libXdmcp.6.dylib | `_XdmcpWriteCARD16` | 1 | xquartz |
| libXdmcp.6.dylib | `_XdmcpWriteCARD32` | 1 | xquartz |
| libXdmcp.6.dylib | `_XdmcpWriteCARD8` | 1 | xquartz |
| libXdmcp.6.dylib | `_XdmcpWriteHeader` | 1 | xquartz |
| libXfixes.3.dylib | `_XFixesQueryExtension` | 1 | xquartz |
| libXfixes.3.dylib | `_XFixesSelectSelectionInput` | 1 | xquartz |
| libXfont2.2.dylib | `_xfont2_add_font_names_name` | 1 | xquartz |
| libXfont2.2.dylib | `_xfont2_free_font_names` | 1 | xquartz |
| libXfont2.2.dylib | `_xfont2_free_font_pattern_cache` | 1 | xquartz |
| libXfont2.2.dylib | `_xfont2_init` | 1 | xquartz |
| libXfont2.2.dylib | `_xfont2_init_glyph_caching` | 1 | xquartz |
| libXfont2.2.dylib | `_xfont2_make_font_names_record` | 1 | xquartz |
| libXfont2.2.dylib | `_xfont2_make_font_pattern_cache` | 1 | xquartz |
| libXfont2.2.dylib | `_xfont2_parse_glyph_caching_mode` | 1 | xquartz |
| libXfont2.2.dylib | `_xfont2_query_glyph_extents` | 1 | xquartz |
| libXfont2.2.dylib | `_xfont2_query_text_extents` | 1 | xquartz |
| libXmu.6.dylib | `_XmuConvertStandardSelection` | 1 | basictex |
| libXmu.6.dylib | `_XmuInternAtom` | 1 | basictex |
| libXmu.6.dylib | `__XA_COMPOUND_TEXT` | 1 | basictex |
| libXmu.6.dylib | `__XA_TEXT` | 1 | basictex |
| libXmu.6.dylib | `__XEditResCheckMessages` | 1 | basictex |
| libXplugin.1.dylib | `_xp_attach_gl_context` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_configure_surface` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_configure_window` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_copy_bytes` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_copy_bytes_threshold` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_copy_window` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_create_surface` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_create_window` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_destroy_surface` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_destroy_window` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_disable_hot_keys` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_disable_update` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_export_surface` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_find_window` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_flush_window` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_frame_draw` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_frame_get_rect` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_frame_hit_test` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_get_native_window` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_get_window_bounds` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_hide_cursor` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_init` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_is_symbolic_hotkey_event` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_lock_window` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_lookup_native_window` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_mark_window` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_reenable_update` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_scroll_area_threshold` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_select_events` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_set_cursor` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_set_dock_proxy` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_show_cursor` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_unlock_window` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_update_gl_context` | 1 | xquartz |
| libXplugin.1.dylib | `_xp_window_bring_all_to_front` | 1 | xquartz |
| libXpm.4.dylib | `_XpmCreatePixmapFromData` | 1 | basictex |
| libXpm.4.dylib | `_XpmFreeAttributes` | 1 | basictex |
| libXt.6.dylib | `_XtAddActions` | 1 | basictex |
| libXt.6.dylib | `_XtAddCallback` | 1 | basictex |
| libXt.6.dylib | `_XtAddEventHandler` | 1 | basictex |
| libXt.6.dylib | `_XtAppAddActions` | 1 | basictex |
| libXt.6.dylib | `_XtAppAddWorkProc` | 1 | basictex |
| libXt.6.dylib | `_XtAppCreateShell` | 1 | basictex |
| libXt.6.dylib | `_XtAppError` | 1 | basictex |
| libXt.6.dylib | `_XtAppNextEvent` | 1 | basictex |
| libXt.6.dylib | `_XtCallActionProc` | 1 | basictex |
| libXt.6.dylib | `_XtCallCallbacks` | 1 | basictex |
| libXt.6.dylib | `_XtConvertAndStore` | 1 | basictex |
| libXt.6.dylib | `_XtCreateManagedWidget` | 1 | basictex |
| libXt.6.dylib | `_XtCreatePopupShell` | 1 | basictex |
| libXt.6.dylib | `_XtCvtStringToPixel` | 1 | basictex |
| libXt.6.dylib | `_XtDatabase` | 1 | basictex |
| libXt.6.dylib | `_XtDestroyWidget` | 1 | basictex |
| libXt.6.dylib | `_XtDisownSelection` | 1 | basictex |
| libXt.6.dylib | `_XtDispatchEvent` | 1 | basictex |
| libXt.6.dylib | `_XtDisplay` | 1 | basictex |
| libXt.6.dylib | `_XtDisplayOfObject` | 1 | basictex |
| libXt.6.dylib | `_XtDisplayStringConversionWarning` | 1 | basictex |
| libXt.6.dylib | `_XtFree` | 1 | basictex |
| libXt.6.dylib | `_XtGetApplicationResources` | 1 | basictex |
| libXt.6.dylib | `_XtGetGC` | 1 | basictex |
| libXt.6.dylib | `_XtGetSelectionRequest` | 1 | basictex |
| libXt.6.dylib | `_XtGetValues` | 1 | basictex |
| libXt.6.dylib | `_XtInitialize` | 1 | basictex |
| libXt.6.dylib | `_XtInstallAccelerators` | 1 | basictex |
| libXt.6.dylib | `_XtInstallAllAccelerators` | 1 | basictex |
| libXt.6.dylib | `_XtIsManaged` | 1 | basictex |
| libXt.6.dylib | `_XtIsRealized` | 1 | basictex |
| libXt.6.dylib | `_XtIsSensitive` | 1 | basictex |
| libXt.6.dylib | `_XtLastTimestampProcessed` | 1 | basictex |
| libXt.6.dylib | `_XtMalloc` | 1 | basictex |
| libXt.6.dylib | `_XtManageChild` | 1 | basictex |
| libXt.6.dylib | `_XtName` | 1 | basictex |
| libXt.6.dylib | `_XtNameToWidget` | 1 | basictex |
| libXt.6.dylib | `_XtOverrideTranslations` | 1 | basictex |
| libXt.6.dylib | `_XtOwnSelection` | 1 | basictex |
| libXt.6.dylib | `_XtParent` | 1 | basictex |
| libXt.6.dylib | `_XtParseAcceleratorTable` | 1 | basictex |
| libXt.6.dylib | `_XtParseTranslationTable` | 1 | basictex |
| libXt.6.dylib | `_XtPending` | 1 | basictex |
| libXt.6.dylib | `_XtPopdown` | 1 | basictex |
| libXt.6.dylib | `_XtPopup` | 1 | basictex |
| libXt.6.dylib | `_XtPopupSpringLoaded` | 1 | basictex |
| libXt.6.dylib | `_XtQueryGeometry` | 1 | basictex |
| libXt.6.dylib | `_XtRealizeWidget` | 1 | basictex |
| libXt.6.dylib | `_XtRealloc` | 1 | basictex |
| libXt.6.dylib | `_XtRemoveEventHandler` | 1 | basictex |
| libXt.6.dylib | `_XtScreen` | 1 | basictex |
| libXt.6.dylib | `_XtScreenDatabase` | 1 | basictex |
| libXt.6.dylib | `_XtSetKeyboardFocus` | 1 | basictex |
| libXt.6.dylib | `_XtSetLanguageProc` | 1 | basictex |
| libXt.6.dylib | `_XtSetMappedWhenManaged` | 1 | basictex |
| libXt.6.dylib | `_XtSetSensitive` | 1 | basictex |
| libXt.6.dylib | `_XtSetTypeConverter` | 1 | basictex |
| libXt.6.dylib | `_XtSetValues` | 1 | basictex |
| libXt.6.dylib | `_XtShellStrings` | 1 | basictex |
| libXt.6.dylib | `_XtStrings` | 1 | basictex |
| libXt.6.dylib | `_XtUnmanageChild` | 1 | basictex |
| libXt.6.dylib | `_XtVaAppCreateShell` | 1 | basictex |
| libXt.6.dylib | `_XtVaCreateManagedWidget` | 1 | basictex |
| libXt.6.dylib | `_XtVaCreatePopupShell` | 1 | basictex |
| libXt.6.dylib | `_XtVaCreateWidget` | 1 | basictex |
| libXt.6.dylib | `_XtVaGetValues` | 1 | basictex |
| libXt.6.dylib | `_XtVaSetValues` | 1 | basictex |
| libXt.6.dylib | `_XtWidgetToApplicationContext` | 1 | basictex |
| libXt.6.dylib | `_XtWindow` | 1 | basictex |
| libXt.6.dylib | `_XtWindowOfObject` | 1 | basictex |
| libXt.6.dylib | `_XtWindowToWidget` | 1 | basictex |
| libXt.6.dylib | `__XtComputeLateBindings` | 1 | basictex |
| libXt.6.dylib | `__XtInherit` | 1 | basictex |
| libXt.6.dylib | `__XtInheritTranslations` | 1 | basictex |
| libXt.6.dylib | `__XtWindowedAncestor` | 1 | basictex |
| libXt.6.dylib | `_colorConvertArgs` | 1 | basictex |
| libXt.6.dylib | `_compositeWidgetClass` | 1 | basictex |
| libXt.6.dylib | `_transientShellWidgetClass` | 1 | basictex |
| libXt.6.dylib | `_widgetClass` | 1 | basictex |
| libXt.6.dylib | `_widgetClassRec` | 1 | basictex |
| libaprutil-1.0.dylib | `_apr_base64_decode` | 1 | iterm2 |
| libaprutil-1.0.dylib | `_apr_base64_decode_len` | 1 | iterm2 |
| libaprutil-1.0.dylib | `_apr_base64_encode_binary` | 1 | iterm2 |
| libaprutil-1.0.dylib | `_apr_base64_encode_len` | 1 | iterm2 |
| libawt.dylib | `_Disposer_AddRecord` | 1 | cyberduck |
| libawt.dylib | `_GetNativePrim` | 1 | cyberduck |
| libawt.dylib | `_GrPrim_Sg2dGetClip` | 1 | cyberduck |
| libawt.dylib | `_GrPrim_Sg2dGetCompInfo` | 1 | cyberduck |
| libawt.dylib | `_GrPrim_Sg2dGetEaRGB` | 1 | cyberduck |
| libawt.dylib | `_GrPrim_Sg2dGetLCDTextContrast` | 1 | cyberduck |
| libawt.dylib | `_GrPrim_Sg2dGetPixel` | 1 | cyberduck |
| libawt.dylib | `_J2dTraceImpl` | 1 | cyberduck |
| libawt.dylib | `_SurfaceData_GetOps` | 1 | cyberduck |
| libawt.dylib | `_SurfaceData_InitOps` | 1 | cyberduck |
| libawt.dylib | `_SurfaceData_IntersectBounds` | 1 | cyberduck |
| libawt_lwawt.dylib | `_AccelGlyphCache_RemoveAllCellInfos` | 1 | cyberduck |
| libawt_lwawt.dylib | `_awt_CreateEmbeddedFrame` | 1 | cyberduck |
| libawt_lwawt.dylib | `_awt_FreeDrawingSurface` | 1 | cyberduck |
| libawt_lwawt.dylib | `_awt_GetComponent` | 1 | cyberduck |
| libawt_lwawt.dylib | `_awt_GetDrawingSurface` | 1 | cyberduck |
| libawt_lwawt.dylib | `_awt_Lock` | 1 | cyberduck |
| libawt_lwawt.dylib | `_awt_SetBounds` | 1 | cyberduck |
| libawt_lwawt.dylib | `_awt_SynthesizeWindowActivation` | 1 | cyberduck |
| libawt_lwawt.dylib | `_awt_Unlock` | 1 | cyberduck |
| libbz2.1.0.dylib | `_BZ2_bzclose` | 1 | inkscape |
| libbz2.1.0.dylib | `_BZ2_bzerror` | 1 | inkscape |
| libbz2.1.0.dylib | `_BZ2_bzflush` | 1 | inkscape |
| libbz2.1.0.dylib | `_BZ2_bzopen` | 1 | inkscape |
| libcarla_native-plugin.dylib | `_carla_get_library_filename` | 1 | lmms |
| libcarla_native-plugin.dylib | `_carla_get_native_patchbay_plugin` | 1 | lmms |
| libcarla_native-plugin.dylib | `_carla_get_native_rack_plugin` | 1 | lmms |
| libcups.2.dylib | `_httpCheck` | 1 | meld |
| libcupsimage.2.dylib | `_cupsRasterClose` | 1 | inkscape |
| libcupsimage.2.dylib | `_cupsRasterOpen` | 1 | inkscape |
| libcupsimage.2.dylib | `_cupsRasterWriteHeader2` | 1 | inkscape |
| libcupsimage.2.dylib | `_cupsRasterWritePixels` | 1 | inkscape |
| libedit.3.dylib | `_current_history` | 1 | pgadmin4 |
| libedit.3.dylib | `_history_set_pos` | 1 | pgadmin4 |
| libedit.3.dylib | `_next_history` | 1 | pgadmin4 |
| libedit.3.dylib | `_previous_history` | 1 | pgadmin4 |
| libedit.3.dylib | `_rl_completer_quote_characters` | 1 | pgadmin4 |
| libedit.3.dylib | `_rl_filename_completion_function` | 1 | pgadmin4 |
| libedit.3.dylib | `_stifle_history` | 1 | pgadmin4 |
| libexpat.1.dylib | `_XML_ExpatVersionInfo` | 1 | kitty |
| libexpat.1.dylib | `_XML_ExternalEntityParserCreate` | 1 | kitty |
| libexpat.1.dylib | `_XML_FreeContentModel` | 1 | kitty |
| libexpat.1.dylib | `_XML_GetBase` | 1 | kitty |
| libexpat.1.dylib | `_XML_GetCurrentByteIndex` | 1 | kitty |
| libexpat.1.dylib | `_XML_GetCurrentColumnNumber` | 1 | kitty |
| libexpat.1.dylib | `_XML_GetFeatureList` | 1 | kitty |
| libexpat.1.dylib | `_XML_GetInputContext` | 1 | kitty |
| libexpat.1.dylib | `_XML_GetSpecifiedAttributeCount` | 1 | kitty |
| libexpat.1.dylib | `_XML_ParserCreate_MM` | 1 | kitty |
| libexpat.1.dylib | `_XML_SetAttlistDeclHandler` | 1 | kitty |
| libexpat.1.dylib | `_XML_SetBase` | 1 | kitty |
| libexpat.1.dylib | `_XML_SetBillionLaughsAttackProtectionActivationThreshold` | 1 | kitty |
| libexpat.1.dylib | `_XML_SetBillionLaughsAttackProtectionMaximumAmplification` | 1 | kitty |
| libexpat.1.dylib | `_XML_SetCommentHandler` | 1 | kitty |
| libexpat.1.dylib | `_XML_SetDefaultHandler` | 1 | kitty |
| libexpat.1.dylib | `_XML_SetDefaultHandlerExpand` | 1 | kitty |
| libexpat.1.dylib | `_XML_SetElementDeclHandler` | 1 | kitty |
| libexpat.1.dylib | `_XML_SetEncoding` | 1 | kitty |
| libexpat.1.dylib | `_XML_SetEndCdataSectionHandler` | 1 | kitty |
| libexpat.1.dylib | `_XML_SetEndDoctypeDeclHandler` | 1 | kitty |
| libexpat.1.dylib | `_XML_SetEndElementHandler` | 1 | kitty |
| libexpat.1.dylib | `_XML_SetEndNamespaceDeclHandler` | 1 | kitty |
| libexpat.1.dylib | `_XML_SetEntityDeclHandler` | 1 | kitty |
| libexpat.1.dylib | `_XML_SetExternalEntityRefHandler` | 1 | kitty |
| libexpat.1.dylib | `_XML_SetHashSalt` | 1 | kitty |
| libexpat.1.dylib | `_XML_SetNamespaceDeclHandler` | 1 | kitty |
| libexpat.1.dylib | `_XML_SetNotStandaloneHandler` | 1 | kitty |
| libexpat.1.dylib | `_XML_SetNotationDeclHandler` | 1 | kitty |
| libexpat.1.dylib | `_XML_SetParamEntityParsing` | 1 | kitty |
| libexpat.1.dylib | `_XML_SetProcessingInstructionHandler` | 1 | kitty |
| libexpat.1.dylib | `_XML_SetReparseDeferralEnabled` | 1 | kitty |
| libexpat.1.dylib | `_XML_SetReturnNSTriplet` | 1 | kitty |
| libexpat.1.dylib | `_XML_SetSkippedEntityHandler` | 1 | kitty |
| libexpat.1.dylib | `_XML_SetStartCdataSectionHandler` | 1 | kitty |
| libexpat.1.dylib | `_XML_SetStartDoctypeDeclHandler` | 1 | kitty |
| libexpat.1.dylib | `_XML_SetStartElementHandler` | 1 | kitty |
| libexpat.1.dylib | `_XML_SetStartNamespaceDeclHandler` | 1 | kitty |
| libexpat.1.dylib | `_XML_SetUnknownEncodingHandler` | 1 | kitty |
| libexpat.1.dylib | `_XML_SetUnparsedEntityDeclHandler` | 1 | kitty |
| libexpat.1.dylib | `_XML_SetXmlDeclHandler` | 1 | kitty |
| libexpat.1.dylib | `_XML_StopParser` | 1 | kitty |
| libexpat.1.dylib | `_XML_UseForeignDTD` | 1 | kitty |
| libexslt.0.dylib | `_exsltDateXpathCtxtRegister` | 1 | inkscape |
| libexslt.0.dylib | `_exsltMathXpathCtxtRegister` | 1 | inkscape |
| libexslt.0.dylib | `_exsltSetsXpathCtxtRegister` | 1 | inkscape |
| libexslt.0.dylib | `_exsltStrXpathCtxtRegister` | 1 | inkscape |
| libicucore.A.dylib | `_uidna_IDNToASCII` | 1 | adobe-acrobat-reader |
| libmimerapi.dylib | `_MimerAddBatch` | 1 | moonlight |
| libmimerapi.dylib | `_MimerBeginSession8` | 1 | moonlight |
| libmimerapi.dylib | `_MimerBeginStatement8` | 1 | moonlight |
| libmimerapi.dylib | `_MimerBeginTransaction` | 1 | moonlight |
| libmimerapi.dylib | `_MimerCloseCursor` | 1 | moonlight |
| libmimerapi.dylib | `_MimerColumnCount` | 1 | moonlight |
| libmimerapi.dylib | `_MimerColumnName` | 1 | moonlight |
| libmimerapi.dylib | `_MimerColumnType` | 1 | moonlight |
| libmimerapi.dylib | `_MimerCurrentRow` | 1 | moonlight |
| libmimerapi.dylib | `_MimerEndSession` | 1 | moonlight |
| libmimerapi.dylib | `_MimerEndStatement` | 1 | moonlight |
| libmimerapi.dylib | `_MimerEndTransaction` | 1 | moonlight |
| libmimerapi.dylib | `_MimerExecute` | 1 | moonlight |
| libmimerapi.dylib | `_MimerExecuteStatement8` | 1 | moonlight |
| libmimerapi.dylib | `_MimerFetch` | 1 | moonlight |
| libmimerapi.dylib | `_MimerFetchScroll` | 1 | moonlight |
| libmimerapi.dylib | `_MimerGetBinary` | 1 | moonlight |
| libmimerapi.dylib | `_MimerGetBlobData` | 1 | moonlight |
| libmimerapi.dylib | `_MimerGetBoolean` | 1 | moonlight |
| libmimerapi.dylib | `_MimerGetDouble` | 1 | moonlight |
| libmimerapi.dylib | `_MimerGetError` | 1 | moonlight |
| libmimerapi.dylib | `_MimerGetFloat` | 1 | moonlight |
| libmimerapi.dylib | `_MimerGetInt32` | 1 | moonlight |
| libmimerapi.dylib | `_MimerGetInt64` | 1 | moonlight |
| libmimerapi.dylib | `_MimerGetLob` | 1 | moonlight |
| libmimerapi.dylib | `_MimerGetNclobData` | 1 | moonlight |
| libmimerapi.dylib | `_MimerGetSequenceInt64` | 1 | moonlight |
| libmimerapi.dylib | `_MimerGetString` | 1 | moonlight |
| libmimerapi.dylib | `_MimerGetUUID` | 1 | moonlight |
| libmimerapi.dylib | `_MimerIsNull` | 1 | moonlight |
| libmimerapi.dylib | `_MimerOpenCursor` | 1 | moonlight |
| libmimerapi.dylib | `_MimerParameterCount` | 1 | moonlight |
| libmimerapi.dylib | `_MimerParameterType` | 1 | moonlight |
| libmimerapi.dylib | `_MimerSetBinary` | 1 | moonlight |
| libmimerapi.dylib | `_MimerSetBlobData` | 1 | moonlight |
| libmimerapi.dylib | `_MimerSetBoolean` | 1 | moonlight |
| libmimerapi.dylib | `_MimerSetDouble` | 1 | moonlight |
| libmimerapi.dylib | `_MimerSetFloat` | 1 | moonlight |
| libmimerapi.dylib | `_MimerSetInt32` | 1 | moonlight |
| libmimerapi.dylib | `_MimerSetInt64` | 1 | moonlight |
| libmimerapi.dylib | `_MimerSetLob` | 1 | moonlight |
| libmimerapi.dylib | `_MimerSetNclobData8` | 1 | moonlight |
| libmimerapi.dylib | `_MimerSetNull` | 1 | moonlight |
| libmimerapi.dylib | `_MimerSetString8` | 1 | moonlight |
| libmimerapi.dylib | `_MimerSetUUID` | 1 | moonlight |
| libncurses.5.4.dylib | `_boolnames` | 1 | iterm2 |
| libncurses.5.4.dylib | `_cur_term` | 1 | iterm2 |
| libncurses.5.4.dylib | `_del_curterm` | 1 | utm |
| libncurses.5.4.dylib | `_getattrs` | 1 | blender |
| libncurses.5.4.dylib | `_getbegx` | 1 | blender |
| libncurses.5.4.dylib | `_getbegy` | 1 | blender |
| libncurses.5.4.dylib | `_getbkgd` | 1 | blender |
| libncurses.5.4.dylib | `_getcchar` | 1 | kitty |
| libncurses.5.4.dylib | `_getcurx` | 1 | blender |
| libncurses.5.4.dylib | `_getcury` | 1 | blender |
| libncurses.5.4.dylib | `_getmaxx` | 1 | blender |
| libncurses.5.4.dylib | `_getmaxy` | 1 | blender |
| libncurses.5.4.dylib | `_getparx` | 1 | blender |
| libncurses.5.4.dylib | `_getpary` | 1 | blender |
| libncurses.5.4.dylib | `_is_leaveok` | 1 | blender |
| libncurses.5.4.dylib | `_numnames` | 1 | iterm2 |
| libncurses.5.4.dylib | `_redrawwin` | 1 | blender |
| libncurses.5.4.dylib | `_set_curterm` | 1 | utm |
| libncurses.5.4.dylib | `_strnames` | 1 | iterm2 |
| libncurses.5.4.dylib | `_wattrset` | 1 | blender |
| libncurses.5.4.dylib | `_win_wch` | 1 | kitty |
| libncurses.5.4.dylib | `_wins_wch` | 1 | kitty |
| libpam.2.dylib | `_pam_acct_mgmt` | 1 | utm |
| libpam.2.dylib | `_pam_end` | 1 | utm |
| libpam.2.dylib | `_pam_start` | 1 | utm |
| libpam.2.dylib | `_pam_strerror` | 1 | utm |
| libpcap.A.dylib | `_bpf_image` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_breakloop` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_can_set_rfmon` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_compile` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_datalink` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_datalink_name_to_val` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_datalink_val_to_description` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_datalink_val_to_name` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_dispatch` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_dump` | 1 | cloudflare-warp |
| libpcap.A.dylib | `_pcap_dump_close` | 1 | cloudflare-warp |
| libpcap.A.dylib | `_pcap_dump_open` | 1 | cloudflare-warp |
| libpcap.A.dylib | `_pcap_free_datalinks` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_free_tstamp_types` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_freecode` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_lib_version` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_list_datalinks` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_list_tstamp_types` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_lookupnet` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_next_ex` | 1 | cloudflare-warp |
| libpcap.A.dylib | `_pcap_open_dead` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_open_live` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_set_buffer_size` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_set_datalink` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_set_immediate_mode` | 1 | cloudflare-warp |
| libpcap.A.dylib | `_pcap_set_promisc` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_set_rfmon` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_set_tstamp_type` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_setfilter` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_snapshot` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_stats` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_statustostr` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_tstamp_type_name_to_val` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_tstamp_type_val_to_description` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_tstamp_type_val_to_name` | 1 | wireshark-app |
| libpixman-1.0.dylib | `_pixman_add_traps` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_add_triangles` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_blt` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_composite_glyphs` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_composite_glyphs_no_mask` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_composite_trapezoids` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_composite_triangles` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_f_transform_bounds` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_f_transform_from_pixman_transform` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_f_transform_init_identity` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_f_transform_init_scale` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_f_transform_init_translate` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_f_transform_invert` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_f_transform_multiply` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_f_transform_point` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_f_transform_rotate` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_f_transform_scale` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_f_transform_translate` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_fill` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_glyph_cache_create` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_glyph_cache_destroy` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_glyph_cache_freeze` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_glyph_cache_insert` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_glyph_cache_lookup` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_glyph_cache_remove` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_glyph_cache_thaw` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_glyph_get_extents` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_image_composite` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_image_create_bits` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_image_create_conical_gradient` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_image_create_linear_gradient` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_image_create_radial_gradient` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_image_create_solid_fill` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_image_set_alpha_map` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_image_set_clip_region` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_image_set_component_alpha` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_image_set_destroy_function` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_image_set_filter` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_image_set_has_client_clip` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_image_set_indexed` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_image_set_repeat` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_image_set_source_clipping` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_image_set_transform` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_image_unref` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_rasterize_trapezoid` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_region_contains_point` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_region_contains_rectangle` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_region_copy` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_region_equal` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_region_fini` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_region_init` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_region_intersect` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_region_inverse` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_region_n_rects` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_region_not_empty` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_region_rectangles` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_region_set_static_pointers` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_region_subtract` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_region_translate` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_region_union` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_transform_bounds` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_transform_from_pixman_f_transform` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_transform_init_identity` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_transform_init_translate` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_transform_is_identity` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_transform_multiply` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_transform_point` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_transform_point_3d` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_transform_rotate` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_transform_scale` | 1 | xquartz |
| libpixman-1.0.dylib | `_pixman_transform_translate` | 1 | xquartz |
| libpmenergy.dylib | `_pm_energy_impact` | 1 | adobe-acrobat-reader |
| libpmsample.dylib | `_pm_sample_task` | 1 | adobe-acrobat-reader |
| libsandbox.1.dylib | `_sandbox_apply_container` | 1 | iterm2 |
| libsasl2.2.dylib | `_sasl_client_done` | 1 | tableplus |
| libsasl2.2.dylib | `_sasl_client_init` | 1 | tableplus |
| libsasl2.2.dylib | `_sasl_client_new` | 1 | tableplus |
| libsasl2.2.dylib | `_sasl_client_start` | 1 | tableplus |
| libsasl2.2.dylib | `_sasl_client_step` | 1 | tableplus |
| libsasl2.2.dylib | `_sasl_decode` | 1 | utm |
| libsasl2.2.dylib | `_sasl_encode` | 1 | utm |
| libsasl2.2.dylib | `_sasl_getprop` | 1 | utm |
| libsasl2.2.dylib | `_sasl_global_listmech` | 1 | tableplus |
| libsasl2.2.dylib | `_sasl_listmech` | 1 | utm |
| libsasl2.2.dylib | `_sasl_server_init` | 1 | utm |
| libsasl2.2.dylib | `_sasl_server_new` | 1 | utm |
| libsasl2.2.dylib | `_sasl_server_start` | 1 | utm |
| libsasl2.2.dylib | `_sasl_server_step` | 1 | utm |
| libsasl2.2.dylib | `_sasl_set_mutex` | 1 | tableplus |
| libsasl2.2.dylib | `_sasl_setprop` | 1 | utm |
| libsqlite3.dylib | `_sqlite3_mutex_alloc` | 1 | whatsapp |
| libsqlite3.dylib | `_sqlite3_mutex_enter` | 1 | whatsapp |
| libsqlite3.dylib | `_sqlite3_mutex_free` | 1 | whatsapp |
| libsqlite3.dylib | `_sqlite3_mutex_leave` | 1 | whatsapp |
| libstdc++.6.dylib | `__ZNSs4_Rep10_M_destroyERKSaIcE` | 1 | deluge |
| libstdc++.6.dylib | `__ZNSs4_Rep20_S_empty_rep_storageE` | 1 | deluge |
| libstdc++.6.dylib | `__ZNSs6assignEPKcm` | 1 | deluge |
| libstdc++.6.dylib | `__ZNSsC1EPKcRKSaIcE` | 1 | deluge |
| libstdc++.6.dylib | `__ZNSsC1ERKSs` | 1 | deluge |
| libstdc++.6.dylib | `__ZNSsD1Ev` | 1 | deluge |
| libxar.1.dylib | `_xar_err_get_errno` | 1 | zoom |
| libxar.1.dylib | `_xar_err_get_string` | 1 | zoom |
| libxar.1.dylib | `_xar_get_safe_path` | 1 | docker-desktop |
| libxar.1.dylib | `_xar_register_errhandler` | 1 | zoom |
| libxar.1.dylib | `_xar_signature_copy_signed_data` | 1 | zoom |
| libxar.1.dylib | `_xar_signature_first` | 1 | zoom |
| libxar.1.dylib | `_xar_signature_get_x509certificate_count` | 1 | zoom |
| libxar.1.dylib | `_xar_signature_get_x509certificate_data` | 1 | zoom |
| libxar.1.dylib | `_xar_signature_next` | 1 | zoom |
| libxar.1.dylib | `_xar_signature_type` | 1 | zoom |
| libxslt.1.dylib | `_xsltApplyOneTemplate` | 1 | inkscape |
| libxslt.1.dylib | `_xsltCleanupGlobals` | 1 | inkscape |
| libxslt.1.dylib | `_xsltDocDefaultLoader` | 1 | inkscape |
| libxslt.1.dylib | `_xsltGetDefaultSecurityPrefs` | 1 | libreoffice |
| libxslt.1.dylib | `_xsltGetProfileInformation` | 1 | inkscape |
| libxslt.1.dylib | `_xsltGetSecurityPrefs` | 1 | inkscape |
| libxslt.1.dylib | `_xsltLibxsltVersion` | 1 | inkscape |
| libxslt.1.dylib | `_xsltMaxDepth` | 1 | inkscape |
| libxslt.1.dylib | `_xsltNextImport` | 1 | inkscape |
| libxslt.1.dylib | `_xsltParseStylesheetFile` | 1 | libreoffice |
| libxslt.1.dylib | `_xsltProcessOneNode` | 1 | inkscape |
| libxslt.1.dylib | `_xsltQuoteOneUserParam` | 1 | inkscape |
| libxslt.1.dylib | `_xsltQuoteUserParams` | 1 | libreoffice |
| libxslt.1.dylib | `_xsltRegisterAllExtras` | 1 | inkscape |
| libxslt.1.dylib | `_xsltRegisterExtElement` | 1 | inkscape |
| libxslt.1.dylib | `_xsltRegisterExtFunction` | 1 | inkscape |
| libxslt.1.dylib | `_xsltRegisterExtModule` | 1 | libreoffice |
| libxslt.1.dylib | `_xsltRegisterExtModuleFunction` | 1 | libreoffice |
| libxslt.1.dylib | `_xsltSaveResultToFilename` | 1 | inkscape |
| libxslt.1.dylib | `_xsltSetCtxtParseOptions` | 1 | inkscape |
| libxslt.1.dylib | `_xsltSetDefaultSecurityPrefs` | 1 | libreoffice |
| libxslt.1.dylib | `_xsltSetGenericErrorFunc` | 1 | inkscape |
| libxslt.1.dylib | `_xsltSetLoaderFunc` | 1 | inkscape |
| libxslt.1.dylib | `_xsltSetTransformErrorFunc` | 1 | inkscape |
| libxslt.1.dylib | `_xsltTransformError` | 1 | inkscape |
| libxslt.1.dylib | `_xsltXPathGetTransformContext` | 1 | libreoffice |
