# Lacunas de Leonardo (runtime C/C++)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Leonardo (runtime C/C++) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**1299** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| libSystem.B.dylib | `_putenv` | 21 | android-studio, cyberduck, dbeaver-community, deluge |
| libSystem.B.dylib | `_getprotobyname` | 18 | android-studio, blender, deluge, docker-desktop |
| libSystem.B.dylib | `_getloadavg` | 14 | android-studio, blender, cyberduck, dbeaver-community |
| libSystem.B.dylib | `_pthread_attr_getschedpolicy` | 14 | audacity, db-browser-for-sqlite, dolphin, keepassxc |
| libSystem.B.dylib | `_OSAtomicAdd32Barrier` | 13 | cmux, deluge, firefox, floorp |
| libSystem.B.dylib | `__os_activity_create` | 13 | audacity, bitwarden, db-browser-for-sqlite, dolphin |
| libSystem.B.dylib | `_pthread_setcanceltype` | 13 | deluge, dolphin, godot, inkscape |
| libSystem.B.dylib | `_if_freenameindex` | 12 | android-studio, blender, gimp, inkscape |
| libSystem.B.dylib | `_if_nameindex` | 12 | android-studio, blender, gimp, inkscape |
| libcups.2.dylib | `_ppdClose` | 12 | bruno, db-browser-for-sqlite, gimp, inkscape |
| libcups.2.dylib | `_ppdFindOption` | 12 | bruno, db-browser-for-sqlite, gimp, inkscape |
| libSystem.B.dylib | `_hstrerror` | 11 | android-studio, blender, deluge, gimp |
| libSystem.B.dylib | `_wcsstr` | 11 | android-studio, audacity, dolphin, gimp |
| libSystem.B.dylib | `_ctermid_r` | 10 | android-studio, blender, deluge, gimp |
| libSystem.B.dylib | `_getprotobynumber` | 10 | docker-desktop, firefox, floorp, gimp |
| libSystem.B.dylib | `_hash_create` | 10 | alt-tab, cyberduck, flycut, hex-fiend |
| libSystem.B.dylib | `_hash_search` | 10 | alt-tab, cyberduck, flycut, hex-fiend |
| libSystem.B.dylib | `_setpgrp` | 10 | android-studio, blender, deluge, gimp |
| libSystem.B.dylib | `_wcsxfrm` | 10 | android-studio, blender, gimp, inkscape |
| libcups.2.dylib | `_ppdFindMarkedChoice` | 10 | bruno, db-browser-for-sqlite, krita, lmms |
| libSystem.B.dylib | `_sethostname` | 9 | android-studio, blender, gimp, inkscape |
| libSystem.B.dylib | `_wcsrchr` | 9 | android-studio, blender, gimp, inkscape |
| libSystem.B.dylib | `_wcstok` | 9 | android-studio, blender, gimp, inkscape |
| libcups.2.dylib | `_cupsGetDests2` | 9 | bruno, firefox, floorp, mullvad-browser |
| libcups.2.dylib | `_ppdOpenFile` | 9 | db-browser-for-sqlite, inkscape, krita, lmms |
| libSystem.B.dylib | `_OSAtomicCompareAndSwapIntBarrier` | 8 | firefox, floorp, mullvad-browser, obs |
| libSystem.B.dylib | `__res` | 8 | audacity, dolphin, keepassxc, microsoft-edge |
| libSystem.B.dylib | `_dbm_clearerr` | 8 | android-studio, blender, gimp, inkscape |
| libSystem.B.dylib | `_dbm_close` | 8 | android-studio, blender, gimp, inkscape |
| libSystem.B.dylib | `_dbm_delete` | 8 | android-studio, blender, gimp, inkscape |
| libSystem.B.dylib | `_dbm_error` | 8 | android-studio, blender, gimp, inkscape |
| libSystem.B.dylib | `_dbm_fetch` | 8 | android-studio, blender, gimp, inkscape |
| libSystem.B.dylib | `_dbm_firstkey` | 8 | android-studio, blender, gimp, inkscape |
| libSystem.B.dylib | `_dbm_nextkey` | 8 | android-studio, blender, gimp, inkscape |
| libSystem.B.dylib | `_dbm_open` | 8 | android-studio, blender, gimp, inkscape |
| libSystem.B.dylib | `_dbm_store` | 8 | android-studio, blender, gimp, inkscape |
| libSystem.B.dylib | `_gethostbyname2` | 8 | firefox, floorp, gimp, libreoffice |
| libSystem.B.dylib | `_uuid_generate_time` | 8 | android-studio, blender, gimp, inkscape |
| libcups.2.dylib | `_httpGetHostname` | 8 | firefox, floorp, gimp, inkscape |
| libedit.3.dylib | `_add_history` | 7 | blender, deluge, hammerspoon, kicad |
| libncurses.5.4.dylib | `_setupterm` | 7 | blender, deluge, inkscape, iterm2 |
| libncurses.5.4.dylib | `_tigetnum` | 7 | blender, deluge, inkscape, iterm2 |
| libSystem.B.dylib | `_drand48` | 6 | clipy, gimp, iterm2, krita |
| libSystem.B.dylib | `_getsectbynamefromheader_64` | 6 | clipy, inkscape, orbstack, pencil |
| libSystem.B.dylib | `_getsectdatafromheader_64` | 6 | alt-tab, firefox, inkscape, pencil |
| libSystem.B.dylib | `_pthread_rwlockattr_destroy` | 6 | firefox, floorp, mullvad-browser, obs |
| libSystem.B.dylib | `_pthread_rwlockattr_init` | 6 | firefox, floorp, mullvad-browser, obs |
| libSystem.B.dylib | `_timespec_get` | 6 | docker-desktop, kicad, obs, postman |
| libSystem.B.dylib | `_wcscasecmp` | 6 | audacity, krita, openrct2, prusaslicer |
| libSystem.B.dylib | `_wcslcpy` | 6 | dolphin, krita, openrct2, shotcut |
| libcups.2.dylib | `_cupsConnectDest` | 6 | firefox, floorp, mullvad-browser, thunderbird |
| libcups.2.dylib | `_cupsGetDestMediaByIndex` | 6 | firefox, floorp, mullvad-browser, thunderbird |
| libcups.2.dylib | `_cupsGetDestMediaCount` | 6 | firefox, floorp, mullvad-browser, thunderbird |
| libcups.2.dylib | `_cupsGetDestMediaDefault` | 6 | firefox, floorp, mullvad-browser, thunderbird |
| libcups.2.dylib | `_cupsGetDests` | 6 | firefox, floorp, mullvad-browser, thunderbird |
| libcups.2.dylib | `_cupsLocalizeDestMedia` | 6 | firefox, floorp, mullvad-browser, thunderbird |
| libcups.2.dylib | `_httpAddrPort` | 6 | firefox, floorp, mullvad-browser, thunderbird |
| libcups.2.dylib | `_httpGetAddress` | 6 | firefox, floorp, mullvad-browser, thunderbird |
| libedit.3.dylib | `_history_truncate_file` | 6 | blender, hammerspoon, kicad, kitty |
| libedit.3.dylib | `_read_history` | 6 | blender, hammerspoon, kicad, kitty |
| libedit.3.dylib | `_rl_attempted_completion_function` | 6 | blender, hammerspoon, kicad, kitty |
| libedit.3.dylib | `_rl_attempted_completion_over` | 6 | blender, hammerspoon, kicad, kitty |
| libedit.3.dylib | `_rl_completion_append_character` | 6 | blender, hammerspoon, kicad, kitty |
| libedit.3.dylib | `_rl_completion_matches` | 6 | blender, hammerspoon, kicad, kitty |
| libedit.3.dylib | `_write_history` | 6 | blender, hammerspoon, kicad, kitty |
| libexpat.1.dylib | `_XML_ErrorString` | 6 | github, iina, inkscape, kicad |
| libexpat.1.dylib | `_XML_GetErrorCode` | 6 | github, iina, inkscape, kicad |
| libexpat.1.dylib | `_XML_ParserFree` | 6 | github, iina, inkscape, kicad |
| libexpat.1.dylib | `_XML_SetElementHandler` | 6 | github, iina, inkscape, kicad |
| libexpat.1.dylib | `_XML_SetUserData` | 6 | github, iina, inkscape, kicad |
| libncurses.5.4.dylib | `_tigetflag` | 6 | blender, deluge, inkscape, iterm2 |
| libncurses.5.4.dylib | `_tigetstr` | 6 | blender, deluge, inkscape, iterm2 |
| libSystem.B.dylib | `_csqrt` | 5 | blender, gimp, inkscape, kitty |
| libSystem.B.dylib | `_exc_server` | 5 | cmux, ghostty, inkscape, openttd |
| libSystem.B.dylib | `_wcslcat` | 5 | dolphin, krita, openrct2, shotcut |
| libcups.2.dylib | `_httpBlocking` | 5 | bruno, gimp, inkscape, pencil |
| libcups.2.dylib | `_httpError` | 5 | bruno, gimp, inkscape, pencil |
| libcups.2.dylib | `_ppdFindAttr` | 5 | bruno, gimp, inkscape, pencil |
| libcups.2.dylib | `_ppdFindChoice` | 5 | bruno, gimp, inkscape, pencil |
| libcups.2.dylib | `_ppdMarkDefaults` | 5 | bruno, gimp, inkscape, pencil |
| libedit.3.dylib | `_clear_history` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_emacs_meta_keymap` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_history_get` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_history_get_history_state` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_remove_history` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_replace_history_entry` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_rl_bind_key` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_rl_bind_key_in_map` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_rl_callback_handler_install` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_rl_callback_handler_remove` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_rl_callback_read_char` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_rl_cleanup_after_signal` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_rl_complete` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_rl_completer_word_break_characters` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_rl_completion_display_matches_hook` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_rl_completion_type` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_rl_free_line_state` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_rl_initialize` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_rl_insert` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_rl_insert_text` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_rl_instream` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_rl_library_version` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_rl_line_buffer` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_rl_outstream` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_rl_parse_and_bind` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_rl_pre_input_hook` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_rl_prep_terminal` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_rl_read_init_file` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_rl_readline_name` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_rl_readline_version` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_rl_redisplay` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_rl_startup_hook` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_rl_variable_bind` | 5 | blender, kicad, kitty, krita |
| libedit.3.dylib | `_using_history` | 5 | blender, kicad, kitty, krita |
| libexpat.1.dylib | `_XML_GetBuffer` | 5 | iina, inkscape, kicad, kitty |
| libexpat.1.dylib | `_XML_ParseBuffer` | 5 | iina, inkscape, kicad, kitty |
| libexpat.1.dylib | `_XML_ParserCreate` | 5 | github, iina, inkscape, kicad |
| libexpat.1.dylib | `_XML_SetCharacterDataHandler` | 5 | github, iina, inkscape, kicad |
| libiconv.2.dylib | `_libiconv_set_relocation_prefix` | 5 | deluge, krita, lmms, openscad |
| libncurses.5.4.dylib | `_COLORS` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_COLOR_PAIRS` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_COLS` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_LINES` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_acs_map` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_baudrate` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_beep` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_can_change_color` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_cbreak` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_clearok` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_color_content` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_copywin` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_curs_set` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_def_prog_mode` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_def_shell_mode` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_delay_output` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_delwin` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_derwin` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_doupdate` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_echo` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_endwin` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_erasechar` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_filter` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_flash` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_flushinp` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_getmouse` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_getwin` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_halfdelay` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_has_colors` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_has_ic` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_has_il` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_has_key` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_idcok` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_idlok` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_immedok` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_init_color` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_init_pair` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_initscr` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_intrflush` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_is_linetouched` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_is_term_resized` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_is_wintouched` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_isendwin` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_keyname` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_keypad` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_killchar` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_leaveok` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_longname` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_meta` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_mouseinterval` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_mousemask` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_mvderwin` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_mvwin` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_napms` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_newpad` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_newscr` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_newwin` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_nl` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_nocbreak` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_nodelay` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_noecho` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_nonl` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_noqiflush` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_noraw` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_notimeout` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_overlay` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_overwrite` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_pair_content` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_pechochar` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_pnoutrefresh` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_prefresh` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_putp` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_putwin` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_qiflush` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_raw` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_reset_prog_mode` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_reset_shell_mode` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_resetty` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_resize_term` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_resizeterm` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_savetty` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_scrollok` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_start_color` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_stdscr` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_subpad` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_subwin` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_syncok` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_termattrs` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_termname` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_tparm` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_typeahead` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_unctrl` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_ungetch` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_ungetmouse` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_use_default_colors` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_use_env` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_waddch` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_waddnstr` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_wattr_off` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_wattr_on` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_wbkgd` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_wbkgdset` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_wborder` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_wchgat` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_wclear` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_wclrtobot` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_wclrtoeol` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_wcursyncup` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_wdelch` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_wechochar` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_wenclose` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_werase` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_wgetch` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_wgetnstr` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_whline` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_winch` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_winnstr` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_winsch` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_winsdelln` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_winsnstr` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_wmove` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_wnoutrefresh` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_wredrawln` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_wrefresh` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_wresize` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_wscrl` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_wsetscrreg` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_wsyncdown` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_wsyncup` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_wtimeout` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_wtouchln` | 5 | blender, deluge, inkscape, kitty |
| libncurses.5.4.dylib | `_wvline` | 5 | blender, deluge, inkscape, kitty |
| libSystem.B.dylib | `_OSAtomicCompareAndSwap64Barrier` | 4 | deluge, godot, openscad, pixelorama |
| libSystem.B.dylib | `_OSSpinLockLock` | 4 | alt-tab, kicad, webtorrent, xquartz |
| libSystem.B.dylib | `_OSSpinLockUnlock` | 4 | alt-tab, kicad, webtorrent, xquartz |
| libSystem.B.dylib | `___divdc3` | 4 | krita, libreoffice, shotcut, tic80 |
| libSystem.B.dylib | `_catanh` | 4 | blender, inkscape, kicad, tic80 |
| libSystem.B.dylib | `_cexp` | 4 | blender, inkscape, lmms, tic80 |
| libSystem.B.dylib | `_csqrtf` | 4 | blender, gimp, inkscape, kitty |
| libSystem.B.dylib | `_csqrtl` | 4 | blender, gimp, inkscape, kitty |
| libSystem.B.dylib | `_swap_fat_arch` | 4 | google-chrome, kicad, microsoft-edge, orbstack |
| libSystem.B.dylib | `_tmpnam` | 4 | deluge, google-chrome, microsoft-edge, xquartz |
| libSystem.B.dylib | `_wcsncasecmp` | 4 | krita, openrct2, shotcut, tic80 |
| libcups.2.dylib | `_ppdOpenFd` | 4 | bruno, gimp, inkscape, pencil |
| libedit.3.dylib | `_rl_basic_word_break_characters` | 4 | blender, kitty, krita, libreoffice |
| libexpat.1.dylib | `_XML_GetCurrentLineNumber` | 4 | iina, inkscape, kicad, kitty |
| libffi.dylib | `_ffi_call` | 4 | kicad, kitty, krita, libreoffice |
| libffi.dylib | `_ffi_prep_cif` | 4 | kicad, kitty, krita, libreoffice |
| libicucore.A.dylib | `_u_strToLower` | 4 | bitwarden, monitorcontrol, mos, opencode-desktop |
| libicucore.A.dylib | `_u_strToUpper` | 4 | bitwarden, monitorcontrol, mos, opencode-desktop |
| libicucore.A.dylib | `_ubrk_close` | 4 | bitwarden, monitorcontrol, mos, opencode-desktop |
| libicucore.A.dylib | `_ubrk_following` | 4 | bitwarden, monitorcontrol, mos, opencode-desktop |
| libicucore.A.dylib | `_ubrk_open` | 4 | bitwarden, monitorcontrol, mos, opencode-desktop |
| libicucore.A.dylib | `_ubrk_setText` | 4 | bitwarden, monitorcontrol, mos, opencode-desktop |
| libicucore.A.dylib | `_unorm2_getNFCInstance` | 4 | bitwarden, monitorcontrol, mos, opencode-desktop |
| libicucore.A.dylib | `_unorm2_normalize` | 4 | bitwarden, monitorcontrol, mos, opencode-desktop |
| libncurses.5.4.dylib | `_ESCDELAY` | 4 | blender, inkscape, kitty, krita |
| libncurses.5.4.dylib | `_TABSIZE` | 4 | blender, inkscape, kitty, krita |
| libncurses.5.4.dylib | `_set_escdelay` | 4 | blender, inkscape, kitty, krita |
| libncurses.5.4.dylib | `_set_tabsize` | 4 | blender, inkscape, kitty, krita |
| libncurses.5.4.dylib | `_setcchar` | 4 | android-studio, blender, inkscape, kitty |
| libncurses.5.4.dylib | `_unget_wch` | 4 | android-studio, blender, inkscape, kitty |
| libncurses.5.4.dylib | `_wadd_wch` | 4 | android-studio, blender, inkscape, kitty |
| libncurses.5.4.dylib | `_waddnwstr` | 4 | android-studio, blender, inkscape, kitty |
| libncurses.5.4.dylib | `_wget_wch` | 4 | android-studio, blender, inkscape, kitty |
| libncurses.5.4.dylib | `_wins_nwstr` | 4 | android-studio, blender, inkscape, kitty |
| libpanel.5.4.dylib | `_bottom_panel` | 4 | blender, inkscape, kitty, krita |
| libpanel.5.4.dylib | `_del_panel` | 4 | blender, inkscape, kitty, krita |
| libpanel.5.4.dylib | `_hide_panel` | 4 | blender, inkscape, kitty, krita |
| libpanel.5.4.dylib | `_move_panel` | 4 | blender, inkscape, kitty, krita |
| libpanel.5.4.dylib | `_new_panel` | 4 | blender, inkscape, kitty, krita |
| libpanel.5.4.dylib | `_panel_above` | 4 | blender, inkscape, kitty, krita |
| libpanel.5.4.dylib | `_panel_below` | 4 | blender, inkscape, kitty, krita |
| libpanel.5.4.dylib | `_panel_hidden` | 4 | blender, inkscape, kitty, krita |
| libpanel.5.4.dylib | `_panel_userptr` | 4 | blender, inkscape, kitty, krita |
| libpanel.5.4.dylib | `_replace_panel` | 4 | blender, inkscape, kitty, krita |
| libpanel.5.4.dylib | `_set_panel_userptr` | 4 | blender, inkscape, kitty, krita |
| libpanel.5.4.dylib | `_show_panel` | 4 | blender, inkscape, kitty, krita |
| libpanel.5.4.dylib | `_top_panel` | 4 | blender, inkscape, kitty, krita |
| libpanel.5.4.dylib | `_update_panels` | 4 | blender, inkscape, kitty, krita |
| Tcl.framework | `_TclFreeObj` | 3 | deluge, hex-fiend, inkscape |
| Tcl.framework | `_Tcl_AddErrorInfo` | 3 | deluge, hex-fiend, inkscape |
| Tcl.framework | `_Tcl_CreateInterp` | 3 | deluge, hex-fiend, inkscape |
| Tcl.framework | `_Tcl_DeleteInterp` | 3 | deluge, hex-fiend, inkscape |
| Tcl.framework | `_Tcl_Eval` | 3 | deluge, hex-fiend, inkscape |
| Tcl.framework | `_Tcl_EvalFile` | 3 | deluge, hex-fiend, inkscape |
| Tcl.framework | `_Tcl_Free` | 3 | deluge, hex-fiend, inkscape |
| Tcl.framework | `_Tcl_GetByteArrayFromObj` | 3 | deluge, hex-fiend, inkscape |
| Tcl.framework | `_Tcl_GetLongFromObj` | 3 | deluge, hex-fiend, inkscape |
| Tcl.framework | `_Tcl_GetString` | 3 | deluge, hex-fiend, inkscape |
| Tcl.framework | `_Tcl_GetStringFromObj` | 3 | deluge, hex-fiend, inkscape |
| Tcl.framework | `_Tcl_GetStringResult` | 3 | deluge, hex-fiend, inkscape |
| Tcl.framework | `_Tcl_Init` | 3 | deluge, hex-fiend, inkscape |
| Tcl.framework | `_Tcl_NewBooleanObj` | 3 | deluge, hex-fiend, inkscape |
| Tcl.framework | `_Tcl_NewByteArrayObj` | 3 | deluge, hex-fiend, inkscape |
| Tcl.framework | `_Tcl_NewDoubleObj` | 3 | deluge, hex-fiend, inkscape |
| Tcl.framework | `_Tcl_NewStringObj` | 3 | deluge, hex-fiend, inkscape |
| Tcl.framework | `_Tcl_NewWideIntObj` | 3 | deluge, hex-fiend, inkscape |
| Tcl.framework | `_Tcl_SetObjResult` | 3 | deluge, hex-fiend, inkscape |
| libSystem.B.dylib | `___divsc3` | 3 | krita, ollama-app, shotcut |
| libSystem.B.dylib | `___invert_f4` | 3 | monitorcontrol, mos, raycast |
| libSystem.B.dylib | `_cacos` | 3 | blender, inkscape, tic80 |
| libSystem.B.dylib | `_cacosh` | 3 | blender, inkscape, tic80 |
| libSystem.B.dylib | `_casin` | 3 | blender, inkscape, tic80 |
| libSystem.B.dylib | `_casinh` | 3 | blender, inkscape, tic80 |
| libSystem.B.dylib | `_catan` | 3 | blender, inkscape, tic80 |
| libSystem.B.dylib | `_ccos` | 3 | blender, inkscape, tic80 |
| libSystem.B.dylib | `_ccosh` | 3 | blender, inkscape, tic80 |
| libSystem.B.dylib | `_clnt_create` | 3 | blender, inkscape, kicad |
| libSystem.B.dylib | `_clnt_spcreateerror` | 3 | blender, inkscape, kicad |
| libSystem.B.dylib | `_clog` | 3 | blender, inkscape, tic80 |
| libSystem.B.dylib | `_cpow` | 3 | blender, inkscape, tic80 |
| libSystem.B.dylib | `_csin` | 3 | blender, inkscape, tic80 |
| libSystem.B.dylib | `_csinh` | 3 | blender, inkscape, tic80 |
| libSystem.B.dylib | `_ctan` | 3 | blender, inkscape, tic80 |
| libSystem.B.dylib | `_ctanh` | 3 | blender, inkscape, tic80 |
| libSystem.B.dylib | `_duplocale` | 3 | gimp, krita, thunderbird |
| libSystem.B.dylib | `_errc` | 3 | brave-browser, google-chrome, microsoft-edge |
| libSystem.B.dylib | `_fmemopen` | 3 | gimp, libreoffice, shotcut |
| libSystem.B.dylib | `_fmtcheck` | 3 | lmms, shotcut, zoom |
| libSystem.B.dylib | `_fputws` | 3 | audacity, kicad, prusaslicer |
| libSystem.B.dylib | `_funopen` | 3 | gimp, libreoffice, utm |
| libSystem.B.dylib | `_fwprintf` | 3 | audacity, kicad, prusaslicer |
| libSystem.B.dylib | `_getwchar` | 3 | android-studio, insomnia, ollama-app |
| libSystem.B.dylib | `_pthread_key_init_np` | 3 | bitwarden, monitorcontrol, mos |
| libSystem.B.dylib | `_swab` | 3 | krita, shotcut, vlc |
| libSystem.B.dylib | `_swap_fat_header` | 3 | google-chrome, microsoft-edge, orbstack |
| libSystem.B.dylib | `_swapcontext` | 3 | blender, kicad, prusaslicer |
| libSystem.B.dylib | `_swscanf` | 3 | audacity, kicad, prusaslicer |
| libSystem.B.dylib | `_vfprintf_l` | 3 | inkscape, kicad, prusaslicer |
| libSystem.B.dylib | `_wcscspn` | 3 | android-studio, gimp, libreoffice |
| libSystem.B.dylib | `_wcsncat` | 3 | inkscape, kicad, sublime-text |
| libSystem.B.dylib | `_wcsspn` | 3 | audacity, kicad, prusaslicer |
| libSystem.B.dylib | `_wcstod_l` | 3 | audacity, kicad, prusaslicer |
| libSystem.B.dylib | `_wcstol_l` | 3 | audacity, kicad, prusaslicer |
| libSystem.B.dylib | `_wcstoul_l` | 3 | audacity, kicad, prusaslicer |
| libSystem.B.dylib | `_xdr_enum` | 3 | blender, inkscape, kicad |
| libSystem.B.dylib | `_xdr_pointer` | 3 | blender, inkscape, kicad |
| libSystem.B.dylib | `_xdr_string` | 3 | blender, inkscape, kicad |
| libSystem.B.dylib | `_yp_all` | 3 | blender, inkscape, kicad |
| libSystem.B.dylib | `_yp_get_default_domain` | 3 | blender, inkscape, kicad |
| libSystem.B.dylib | `_yp_master` | 3 | blender, inkscape, kicad |
| libSystem.B.dylib | `_yp_match` | 3 | blender, inkscape, kicad |
| libSystem.B.dylib | `_yperr_string` | 3 | blender, inkscape, kicad |
| libcups.2.dylib | `_cupsGetDest` | 3 | bruno, pencil, webtorrent |
| libcups.2.dylib | `_cupsGetPPD` | 3 | bruno, pencil, webtorrent |
| libcups.2.dylib | `_cupsGetPPD2` | 3 | bruno, pencil, webtorrent |
| libcups.2.dylib | `_cupsMarkOptions` | 3 | bruno, pencil, webtorrent |
| libcups.2.dylib | `_cupsSetUser` | 3 | gimp, inkscape, pencil |
| libcups.2.dylib | `_httpConnectEncrypt` | 3 | bruno, pencil, webtorrent |
| libcups.2.dylib | `_ppdErrorString` | 3 | bruno, pencil, webtorrent |
| libcups.2.dylib | `_ppdLastError` | 3 | bruno, pencil, webtorrent |
| libexpat.1.dylib | `_XML_SetDoctypeDeclHandler` | 3 | iina, inkscape, kicad |
| libffi.dylib | `_ffi_closure_alloc` | 3 | kitty, krita, libreoffice |
| libffi.dylib | `_ffi_closure_free` | 3 | kitty, krita, libreoffice |
| libffi.dylib | `_ffi_prep_cif_var` | 3 | kitty, krita, libreoffice |
| libffi.dylib | `_ffi_prep_closure_loc` | 3 | kitty, krita, libreoffice |
| libffi.dylib | `_ffi_type_double` | 3 | kitty, krita, libreoffice |
| libffi.dylib | `_ffi_type_float` | 3 | kitty, krita, libreoffice |
| libffi.dylib | `_ffi_type_longdouble` | 3 | kitty, krita, libreoffice |
| libffi.dylib | `_ffi_type_pointer` | 3 | kitty, krita, libreoffice |
| libffi.dylib | `_ffi_type_sint16` | 3 | kitty, krita, libreoffice |
| libffi.dylib | `_ffi_type_sint32` | 3 | kitty, krita, libreoffice |
| libffi.dylib | `_ffi_type_sint64` | 3 | kitty, krita, libreoffice |
| libffi.dylib | `_ffi_type_sint8` | 3 | kitty, krita, libreoffice |
| libffi.dylib | `_ffi_type_uint16` | 3 | kitty, krita, libreoffice |
| libffi.dylib | `_ffi_type_uint32` | 3 | kitty, krita, libreoffice |
| libffi.dylib | `_ffi_type_uint64` | 3 | kitty, krita, libreoffice |
| libffi.dylib | `_ffi_type_uint8` | 3 | kitty, krita, libreoffice |
| libffi.dylib | `_ffi_type_void` | 3 | kitty, krita, libreoffice |
| libicucore.A.dylib | `_u_charAge` | 3 | bitwarden, monitorcontrol, mos |
| libicucore.A.dylib | `_u_charName` | 3 | bitwarden, monitorcontrol, mos |
| libicucore.A.dylib | `_u_errorName` | 3 | iterm2, opencode-desktop, sequel-ace |
| libicucore.A.dylib | `_u_getNumericValue` | 3 | bitwarden, monitorcontrol, mos |
| libicucore.A.dylib | `_u_strToTitle` | 3 | bitwarden, monitorcontrol, mos |
| libicucore.A.dylib | `_ubrk_preceding` | 3 | bitwarden, monitorcontrol, mos |
| libicucore.A.dylib | `_ubrk_setUText` | 3 | bitwarden, monitorcontrol, mos |
| libicucore.A.dylib | `_unorm2_hasBoundaryBefore` | 3 | bitwarden, monitorcontrol, mos |
| libicucore.A.dylib | `_unorm2_spanQuickCheckYes` | 3 | bitwarden, monitorcontrol, mos |
| libicucore.A.dylib | `_utext_openUChars` | 3 | bitwarden, monitorcontrol, mos |
| libicucore.A.dylib | `_utext_openUTF8` | 3 | bitwarden, monitorcontrol, mos |
| libncurses.5.4.dylib | `_BC` | 3 | inkscape, keepassxc, lmms |
| libncurses.5.4.dylib | `_PC` | 3 | inkscape, keepassxc, lmms |
| libncurses.5.4.dylib | `_UP` | 3 | inkscape, keepassxc, lmms |
| libncurses.5.4.dylib | `_tgetent` | 3 | inkscape, keepassxc, lmms |
| libncurses.5.4.dylib | `_tgetflag` | 3 | inkscape, keepassxc, lmms |
| libncurses.5.4.dylib | `_tgetnum` | 3 | inkscape, keepassxc, lmms |
| libncurses.5.4.dylib | `_tgetstr` | 3 | inkscape, keepassxc, lmms |
| libncurses.5.4.dylib | `_tgoto` | 3 | inkscape, keepassxc, lmms |
| libncurses.5.4.dylib | `_tputs` | 3 | inkscape, keepassxc, lmms |
| Tcl.framework | `_TclBN_mp_clear` | 2 | deluge, inkscape |
| Tcl.framework | `_TclBN_mp_init` | 2 | deluge, inkscape |
| Tcl.framework | `_TclBN_mp_read_radix` | 2 | deluge, inkscape |
| Tcl.framework | `_TclBN_mp_to_unsigned_bin_n` | 2 | deluge, inkscape |
| Tcl.framework | `_TclBN_mp_unsigned_bin_size` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_AttemptAlloc` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_ConditionFinalize` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_ConditionNotify` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_ConditionWait` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_CreateFileHandler` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_CreateObjCommand` | 2 | hex-fiend, inkscape |
| Tcl.framework | `_Tcl_CreateTimerHandler` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_DeleteCommand` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_DeleteFileHandler` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_DeleteTimerHandler` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_DoOneEvent` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_EvalObjv` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_ExprBoolean` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_ExprDouble` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_ExprLong` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_ExprString` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_FindExecutable` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_GetBignumFromObj` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_GetBoolean` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_GetBooleanFromObj` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_GetCurrentThread` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_GetDouble` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_GetDoubleFromObj` | 2 | hex-fiend, inkscape |
| Tcl.framework | `_Tcl_GetObjResult` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_GetObjType` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_GetThreadData` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_GetVar` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_GetVar2Ex` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_GetWideIntFromObj` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_ListObjGetElements` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_ListObjIndex` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_ListObjLength` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_MutexLock` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_MutexUnlock` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_NewBignumObj` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_NewListObj` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_NewLongObj` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_RecordAndEval` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_ResetResult` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_SetVar` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_SetVar2` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_SetVar2Ex` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_SplitList` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_ThreadAlert` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_ThreadQueueEvent` | 2 | deluge, inkscape |
| Tcl.framework | `_Tcl_UnsetVar2` | 2 | deluge, inkscape |
| Tk.framework | `_Tk_GetNumMainWindows` | 2 | deluge, inkscape |
| Tk.framework | `_Tk_Init` | 2 | deluge, inkscape |
| Tk.framework | `_Tk_MainWindow` | 2 | deluge, inkscape |
| libSystem.B.dylib | `_NSLibraryNameForModule` | 2 | inkscape, kicad |
| libSystem.B.dylib | `_NSLookupAndBindSymbol` | 2 | inkscape, kicad |
| libSystem.B.dylib | `_NSModuleForSymbol` | 2 | inkscape, kicad |
| libSystem.B.dylib | `_NXGetArchInfoFromName` | 2 | openttd, vlc |
| libSystem.B.dylib | `_OSAtomicAnd32` | 2 | audacity, lmms |
| libSystem.B.dylib | `_OSAtomicDequeue` | 2 | hex-fiend, obs |
| libSystem.B.dylib | `_OSAtomicEnqueue` | 2 | hex-fiend, obs |
| libSystem.B.dylib | `_OSAtomicIncrement64Barrier` | 2 | cmux, hex-fiend |
| libSystem.B.dylib | `_OSAtomicOr32` | 2 | audacity, lmms |
| libSystem.B.dylib | `___divxc3` | 2 | krita, shotcut |
| libSystem.B.dylib | `___floatuntixf` | 2 | hex-fiend, kicad |
| libSystem.B.dylib | `___invert_d2` | 2 | monitorcontrol, mos |
| libSystem.B.dylib | `___invert_d3` | 2 | monitorcontrol, mos |
| libSystem.B.dylib | `___invert_d4` | 2 | monitorcontrol, mos |
| libSystem.B.dylib | `___invert_f2` | 2 | monitorcontrol, mos |
| libSystem.B.dylib | `___mulxc3` | 2 | krita, shotcut |
| libSystem.B.dylib | `___truncsfhf2` | 2 | raycast, zoom |
| libSystem.B.dylib | `__os_log_create` | 2 | monitorcontrol, wireshark-app |
| libSystem.B.dylib | `__os_log_internal` | 2 | monitorcontrol, wireshark-app |
| libSystem.B.dylib | `__sigtramp` | 2 | pencil, webtorrent |
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
| libSystem.B.dylib | `_dbopen` | 2 | android-studio, cyberduck |
| libSystem.B.dylib | `_devname` | 2 | utm, vnote |
| libSystem.B.dylib | `_devname_r` | 2 | hex-fiend, vnote |
| libSystem.B.dylib | `_fstatfs64` | 2 | docker-desktop, orbstack |
| libSystem.B.dylib | `_getmntinfo_r_np$INODE64` | 2 | brave-browser, cmux |
| libSystem.B.dylib | `_getsectbynamefromheader` | 2 | clipy, sequel-ace |
| libSystem.B.dylib | `_inet_net_pton` | 2 | pencil, webtorrent |
| libSystem.B.dylib | `_open_memstream` | 2 | shotcut, utm |
| libSystem.B.dylib | `_pthread_setconcurrency` | 2 | kicad, libreoffice |
| libSystem.B.dylib | `_readpassphrase` | 2 | tor-browser, utm |
| libSystem.B.dylib | `_regcomp_l` | 2 | iterm2, kicad |
| libSystem.B.dylib | `_sbrk` | 2 | godot, pixelorama |
| libSystem.B.dylib | `_strunvis` | 2 | android-studio, gimp |
| libSystem.B.dylib | `_strvis` | 2 | android-studio, gimp |
| libSystem.B.dylib | `_swap_linkedit_data_command` | 2 | google-chrome, microsoft-edge |
| libSystem.B.dylib | `_swap_load_command` | 2 | google-chrome, microsoft-edge |
| libSystem.B.dylib | `_swap_mach_header` | 2 | google-chrome, microsoft-edge |
| libSystem.B.dylib | `_swap_mach_header_64` | 2 | google-chrome, microsoft-edge |
| libSystem.B.dylib | `_swap_segment_command` | 2 | google-chrome, microsoft-edge |
| libSystem.B.dylib | `_swap_segment_command_64` | 2 | google-chrome, microsoft-edge |
| libSystem.B.dylib | `_swap_symtab_command` | 2 | google-chrome, microsoft-edge |
| libSystem.B.dylib | `_tempnam` | 2 | deluge, vlc |
| libSystem.B.dylib | `_unmount` | 2 | docker-desktop, orbstack |
| libSystem.B.dylib | `_vis` | 2 | android-studio, gimp |
| libSystem.B.dylib | `_vprintf_l` | 2 | kicad, prusaslicer |
| libSystem.B.dylib | `_vsprintf_l` | 2 | kicad, prusaslicer |
| libSystem.B.dylib | `_wcsnlen` | 2 | dolphin, zoom |
| libSystem.B.dylib | `_wprintf` | 2 | audacity, kicad |
| libbz2.1.0.dylib | `_BZ2_bzWriteClose` | 2 | deluge, sequel-ace |
| libbz2.1.0.dylib | `_BZ2_bzread` | 2 | inkscape, sequel-ace |
| libbz2.1.0.dylib | `_BZ2_bzwrite` | 2 | inkscape, sequel-ace |
| libcups.2.dylib | `_cupsArrayFirst` | 2 | gimp, inkscape |
| libcups.2.dylib | `_cupsDoAuthentication` | 2 | gimp, inkscape |
| libcups.2.dylib | `_cupsEncryption` | 2 | gimp, inkscape |
| libcups.2.dylib | `_cupsFreeOptions` | 2 | gimp, inkscape |
| libcups.2.dylib | `_cupsLangDefault` | 2 | gimp, inkscape |
| libcups.2.dylib | `_cupsLangFree` | 2 | gimp, inkscape |
| libcups.2.dylib | `_cupsParseOptions` | 2 | gimp, inkscape |
| libcups.2.dylib | `_cupsRemoveDest` | 2 | bruno, pencil |
| libcups.2.dylib | `_cupsSetEncryption` | 2 | gimp, inkscape |
| libcups.2.dylib | `_cupsSetPasswordCB` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpAddrFreeList` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpAddrGetList` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpAddrLength` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpAssembleURIf` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpClearFields` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpEncryption` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpFlush` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpGet` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpGetAuthString` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpGetFd` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpGetField` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpGetLength2` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpPost` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpRead2` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpReconnect2` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpSeparateURI` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpSetField` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpUpdate` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpWait` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpWrite2` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ippAddResolution` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ippErrorString` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ippGetBoolean` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ippGetOperation` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ippGetValueTag` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ippLength` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ippRead` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ippSetOperation` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ippSetRequestId` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ippSetState` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ippWrite` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ppdConflicts` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ppdFindCustomOption` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ppdFirstCustomParam` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ppdLocalize` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ppdMarkOption` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ppdNextCustomParam` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ppdPageSize` | 2 | gimp, inkscape |
| libedit.3.dylib | `_readline` | 2 | deluge, hammerspoon |
| libexpat.1.dylib | `_XML_Parse` | 2 | github, kitty |
| libexslt.0.dylib | `_exsltRegisterAll` | 2 | inkscape, libreoffice |
| libicucore.A.dylib | `_u_strlen` | 2 | iterm2, sequel-ace |
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
| libncurses.5.4.dylib | `_curses_version` | 2 | kitty, krita |
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
| libIOReport.dylib | `_IOReportChannelGetChannelName` | 1 | stats |
| libIOReport.dylib | `_IOReportChannelGetGroup` | 1 | stats |
| libIOReport.dylib | `_IOReportChannelGetSubGroup` | 1 | stats |
| libIOReport.dylib | `_IOReportChannelGetUnitLabel` | 1 | stats |
| libIOReport.dylib | `_IOReportCopyChannelsInGroup` | 1 | stats |
| libIOReport.dylib | `_IOReportCreateSamples` | 1 | stats |
| libIOReport.dylib | `_IOReportCreateSamplesDelta` | 1 | stats |
| libIOReport.dylib | `_IOReportCreateSubscription` | 1 | stats |
| libIOReport.dylib | `_IOReportMergeChannels` | 1 | stats |
| libIOReport.dylib | `_IOReportStateGetCount` | 1 | stats |
| libIOReport.dylib | `_IOReportStateGetNameForIndex` | 1 | stats |
| libIOReport.dylib | `_IOReportStateGetResidency` | 1 | stats |
| libSystem.B.dylib | `_NSAddressOfSymbol` | 1 | kicad |
| libSystem.B.dylib | `_NSCreateObjectFileImageFromMemory` | 1 | kicad |
| libSystem.B.dylib | `_NSDestroyObjectFileImage` | 1 | kicad |
| libSystem.B.dylib | `_NSLinkEditError` | 1 | kicad |
| libSystem.B.dylib | `_NSLinkModule` | 1 | kicad |
| libSystem.B.dylib | `_NSLookupSymbolInImage` | 1 | kicad |
| libSystem.B.dylib | `_NSLookupSymbolInModule` | 1 | kicad |
| libSystem.B.dylib | `_NSUnLinkModule` | 1 | kicad |
| libSystem.B.dylib | `_NXFindBestFatArch` | 1 | kicad |
| libSystem.B.dylib | `_OSAtomicAdd64` | 1 | hex-fiend |
| libSystem.B.dylib | `_OSAtomicAdd64Barrier` | 1 | cmux |
| libSystem.B.dylib | `_OSAtomicDecrement64` | 1 | hex-fiend |
| libSystem.B.dylib | `_OSAtomicIncrement64` | 1 | hex-fiend |
| libSystem.B.dylib | `_SANDBOX_CHECK_NO_REPORT` | 1 | visual-studio-code |
| libSystem.B.dylib | `__NSConcreteAutoBlock` | 1 | localsend |
| libSystem.B.dylib | `__NSConcreteFinalizingBlock` | 1 | localsend |
| libSystem.B.dylib | `__NSConcreteWeakBlockVariable` | 1 | localsend |
| libSystem.B.dylib | `__NSGetProgname` | 1 | kitty |
| libSystem.B.dylib | `___atomic_exchange` | 1 | spotify |
| libSystem.B.dylib | `___powidf2` | 1 | shotcut |
| libSystem.B.dylib | `___powisf2` | 1 | shotcut |
| libSystem.B.dylib | `___pthread_chdir` | 1 | android-studio |
| libSystem.B.dylib | `___udivmodti4` | 1 | kicad |
| libSystem.B.dylib | `__keymgr_get_and_lock_processwide_ptr` | 1 | inkscape |
| libSystem.B.dylib | `__keymgr_set_and_unlock_processwide_ptr` | 1 | inkscape |
| libSystem.B.dylib | `__keymgr_unlock_processwide_ptr` | 1 | inkscape |
| libSystem.B.dylib | `__os_activity_current` | 1 | bitwarden |
| libSystem.B.dylib | `__os_activity_initiate` | 1 | alt-tab |
| libSystem.B.dylib | `__os_trace_with_buffer` | 1 | alt-tab |
| libSystem.B.dylib | `__simd_pow_f4` | 1 | dolphin |
| libSystem.B.dylib | `__simd_round_d2` | 1 | linearmouse |
| libSystem.B.dylib | `_acct` | 1 | localsend |
| libSystem.B.dylib | `_aio_cancel` | 1 | localsend |
| libSystem.B.dylib | `_arc4random_addrandom` | 1 | tor-browser |
| libSystem.B.dylib | `_asl_log_descriptor` | 1 | xquartz |
| libSystem.B.dylib | `_chmodx_np` | 1 | utm |
| libSystem.B.dylib | `_daemon` | 1 | utm |
| libSystem.B.dylib | `_daemon$1050` | 1 | vlc |
| libSystem.B.dylib | `_endhostent` | 1 | vlc |
| libSystem.B.dylib | `_erand48` | 1 | vlc |
| libSystem.B.dylib | `_ether_aton` | 1 | docker-desktop |
| libSystem.B.dylib | `_fchmodx_np` | 1 | utm |
| libSystem.B.dylib | `_ffsctl` | 1 | utm |
| libSystem.B.dylib | `_filesec_dup` | 1 | utm |
| libSystem.B.dylib | `_filesec_free` | 1 | utm |
| libSystem.B.dylib | `_filesec_get_property` | 1 | utm |
| libSystem.B.dylib | `_filesec_init` | 1 | utm |
| libSystem.B.dylib | `_filesec_set_property` | 1 | utm |
| libSystem.B.dylib | `_freehostent` | 1 | deluge |
| libSystem.B.dylib | `_fstatx_np$INODE64` | 1 | utm |
| libSystem.B.dylib | `_ftime` | 1 | zoom |
| libSystem.B.dylib | `_get_end` | 1 | inkscape |
| libSystem.B.dylib | `_get_etext` | 1 | inkscape |
| libSystem.B.dylib | `_getaudit_addr` | 1 | android-studio |
| libSystem.B.dylib | `_getipnodebyaddr` | 1 | deluge |
| libSystem.B.dylib | `_getipnodebyname` | 1 | deluge |
| libSystem.B.dylib | `_getopt_long_only` | 1 | android-studio |
| libSystem.B.dylib | `_getpass` | 1 | sequel-ace |
| libSystem.B.dylib | `_getsectdatafromheader` | 1 | alt-tab |
| libSystem.B.dylib | `_imaxabs` | 1 | android-studio |
| libSystem.B.dylib | `_jrand48` | 1 | vlc |
| libSystem.B.dylib | `_kSBXProfilePureComputation` | 1 | vnote |
| libSystem.B.dylib | `_lfind` | 1 | wireshark-app |
| libSystem.B.dylib | `_lio_listio` | 1 | localsend |
| libSystem.B.dylib | `_lstatx_np$INODE64` | 1 | utm |
| libSystem.B.dylib | `_minherit` | 1 | tor-browser |
| libSystem.B.dylib | `_mount` | 1 | orbstack |
| libSystem.B.dylib | `_nl_langinfo_l` | 1 | kicad |
| libSystem.B.dylib | `_nrand48` | 1 | vlc |
| libSystem.B.dylib | `_os_trace_debug_enabled` | 1 | alt-tab |
| libSystem.B.dylib | `_pthread_chdir_np` | 1 | 1password |
| libSystem.B.dylib | `_pthread_cpu_number_np` | 1 | vnote |
| libSystem.B.dylib | `_pthread_getconcurrency` | 1 | libreoffice |
| libSystem.B.dylib | `_putwc` | 1 | inkscape |
| libSystem.B.dylib | `_sethostent` | 1 | vlc |
| libSystem.B.dylib | `_statx_np$INODE64` | 1 | utm |
| libSystem.B.dylib | `_strcasecmp_l` | 1 | blender |
| libSystem.B.dylib | `_strfmon_l` | 1 | kicad |
| libSystem.B.dylib | `_strptime_l` | 1 | orbstack |
| libSystem.B.dylib | `_sys_errlist` | 1 | vlc |
| libSystem.B.dylib | `_vswscanf` | 1 | kicad |
| libSystem.B.dylib | `_wcstombs_l` | 1 | zoom |
| libX11.6.dylib | `_XChangeProperty` | 1 | xquartz |
| libX11.6.dylib | `_XCloseDisplay` | 1 | xquartz |
| libX11.6.dylib | `_XConvertSelection` | 1 | xquartz |
| libX11.6.dylib | `_XCreateSimpleWindow` | 1 | xquartz |
| libX11.6.dylib | `_XDestroyWindow` | 1 | xquartz |
| libX11.6.dylib | `_XFlush` | 1 | xquartz |
| libX11.6.dylib | `_XFree` | 1 | xquartz |
| libX11.6.dylib | `_XGetAtomName` | 1 | xquartz |
| libX11.6.dylib | `_XGetSelectionOwner` | 1 | xquartz |
| libX11.6.dylib | `_XGetWindowProperty` | 1 | xquartz |
| libX11.6.dylib | `_XInternAtom` | 1 | xquartz |
| libX11.6.dylib | `_XNextEvent` | 1 | xquartz |
| libX11.6.dylib | `_XOpenDisplay` | 1 | xquartz |
| libX11.6.dylib | `_XPending` | 1 | xquartz |
| libX11.6.dylib | `_XSelectInput` | 1 | xquartz |
| libX11.6.dylib | `_XSendEvent` | 1 | xquartz |
| libX11.6.dylib | `_XSetErrorHandler` | 1 | xquartz |
| libX11.6.dylib | `_XSetIOErrorHandler` | 1 | xquartz |
| libX11.6.dylib | `_XSetSelectionOwner` | 1 | xquartz |
| libX11.6.dylib | `_Xutf8TextListToTextProperty` | 1 | xquartz |
| libXau.6.dylib | `_XauDisposeAuth` | 1 | xquartz |
| libXau.6.dylib | `_XauReadAuth` | 1 | xquartz |
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
| libcups.2.dylib | `_cupsCloseDestJob` | 1 | pencil |
| libcups.2.dylib | `_cupsCreateDestJob` | 1 | pencil |
| libcups.2.dylib | `_cupsFinishDestDocument` | 1 | pencil |
| libcups.2.dylib | `_cupsGetDestMediaByName` | 1 | pencil |
| libcups.2.dylib | `_cupsStartDestDocument` | 1 | pencil |
| libcupsimage.2.dylib | `_cupsRasterClose` | 1 | inkscape |
| libcupsimage.2.dylib | `_cupsRasterOpen` | 1 | inkscape |
| libcupsimage.2.dylib | `_cupsRasterWriteHeader2` | 1 | inkscape |
| libcupsimage.2.dylib | `_cupsRasterWritePixels` | 1 | inkscape |
| libexpat.1.dylib | `_XML_ExpatVersion` | 1 | kitty |
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
| libffi.dylib | `_ffi_type_complex_double` | 1 | kitty |
| libffi.dylib | `_ffi_type_complex_float` | 1 | kitty |
| libffi.dylib | `_ffi_type_complex_longdouble` | 1 | kitty |
| libicucore.A.dylib | `_u_charDirection` | 1 | opencode-desktop |
| libicucore.A.dylib | `_u_charType` | 1 | opencode-desktop |
| libicucore.A.dylib | `_u_getUnicodeVersion` | 1 | opencode-desktop |
| libicucore.A.dylib | `_u_getVersion` | 1 | opencode-desktop |
| libicucore.A.dylib | `_u_isUWhiteSpace` | 1 | opencode-desktop |
| libicucore.A.dylib | `_u_toupper` | 1 | opencode-desktop |
| libicucore.A.dylib | `_u_versionToString` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ubrk_clone` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ubrk_countAvailable` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ubrk_current` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ubrk_first` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ubrk_getAvailable` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ubrk_getRuleStatus` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ubrk_next` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ubrk_previous` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucal_add` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucal_clear` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucal_clone` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucal_close` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucal_get` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucal_getAttribute` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucal_getCanonicalTimeZoneID` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucal_getDayOfWeekType` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucal_getHostTimeZone` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucal_getKeywordValuesForLocale` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucal_getLimit` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucal_getMillis` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucal_getTimeZoneDisplayName` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucal_getTimeZoneOffsetFromLocal` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucal_getTimeZoneTransitionDate` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucal_open` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucal_openTimeZoneIDEnumeration` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucal_openTimeZones` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucal_set` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucal_setGregorianChange` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucal_setMillis` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucal_setTimeZone` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucfpos_close` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucfpos_constrainCategory` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucfpos_constrainField` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucfpos_getCategory` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucfpos_getField` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucfpos_getIndexes` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucfpos_open` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucfpos_reset` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucol_close` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucol_countAvailable` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucol_getAttribute` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucol_getAvailable` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucol_getKeywordValues` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucol_getKeywordValuesForLocale` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucol_getRules` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucol_open` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucol_setAttribute` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucol_strcoll` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucol_strcollUTF8` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucurr_getName` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ucurr_openISOCurrencies` | 1 | opencode-desktop |
| libicucore.A.dylib | `_udat_applyPattern` | 1 | opencode-desktop |
| libicucore.A.dylib | `_udat_clone` | 1 | opencode-desktop |
| libicucore.A.dylib | `_udat_close` | 1 | opencode-desktop |
| libicucore.A.dylib | `_udat_format` | 1 | opencode-desktop |
| libicucore.A.dylib | `_udat_formatForFields` | 1 | opencode-desktop |
| libicucore.A.dylib | `_udat_getCalendar` | 1 | opencode-desktop |
| libicucore.A.dylib | `_udat_open` | 1 | opencode-desktop |
| libicucore.A.dylib | `_udat_toPattern` | 1 | opencode-desktop |
| libicucore.A.dylib | `_udatpg_close` | 1 | opencode-desktop |
| libicucore.A.dylib | `_udatpg_getBestPatternWithOptions` | 1 | opencode-desktop |
| libicucore.A.dylib | `_udatpg_getFieldDisplayName` | 1 | opencode-desktop |
| libicucore.A.dylib | `_udatpg_getSkeleton` | 1 | opencode-desktop |
| libicucore.A.dylib | `_udatpg_open` | 1 | opencode-desktop |
| libicucore.A.dylib | `_udtitvfmt_close` | 1 | opencode-desktop |
| libicucore.A.dylib | `_udtitvfmt_closeResult` | 1 | opencode-desktop |
| libicucore.A.dylib | `_udtitvfmt_formatCalendarToResult` | 1 | opencode-desktop |
| libicucore.A.dylib | `_udtitvfmt_formatToResult` | 1 | opencode-desktop |
| libicucore.A.dylib | `_udtitvfmt_open` | 1 | opencode-desktop |
| libicucore.A.dylib | `_udtitvfmt_openResult` | 1 | opencode-desktop |
| libicucore.A.dylib | `_udtitvfmt_resultAsValue` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uenum_close` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uenum_count` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uenum_next` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uenum_unext` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ufieldpositer_close` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ufieldpositer_next` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ufieldpositer_open` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ufmtval_getString` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ufmtval_nextPosition` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uidna_nameToASCII` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uidna_nameToUnicode` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uldn_close` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uldn_keyValueDisplayName` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uldn_localeDisplayName` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uldn_openForContext` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uldn_regionDisplayName` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uldn_scriptDisplayName` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ulistfmt_close` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ulistfmt_closeResult` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ulistfmt_format` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ulistfmt_formatStringsToResult` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ulistfmt_openForType` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ulistfmt_openResult` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ulistfmt_resultAsValue` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uloc_addLikelySubtags` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uloc_canonicalize` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uloc_countAvailable` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uloc_forLanguageTag` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uloc_getAvailable` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uloc_getBaseName` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uloc_getCharacterOrientation` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uloc_getCountry` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uloc_getDefault` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uloc_getKeywordValue` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uloc_getLanguage` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uloc_getScript` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uloc_minimizeSubtags` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uloc_openKeywords` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uloc_setDefault` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uloc_setKeywordValue` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uloc_toLanguageTag` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uloc_toUnicodeLocaleType` | 1 | opencode-desktop |
| libicucore.A.dylib | `_unorm2_getInstance` | 1 | opencode-desktop |
| libicucore.A.dylib | `_unorm2_getNFDInstance` | 1 | opencode-desktop |
| libicucore.A.dylib | `_unorm2_getNFKCInstance` | 1 | opencode-desktop |
| libicucore.A.dylib | `_unorm2_getNFKDInstance` | 1 | opencode-desktop |
| libicucore.A.dylib | `_unorm2_isNormalized` | 1 | opencode-desktop |
| libicucore.A.dylib | `_unum_close` | 1 | opencode-desktop |
| libicucore.A.dylib | `_unum_open` | 1 | opencode-desktop |
| libicucore.A.dylib | `_unum_setAttribute` | 1 | opencode-desktop |
| libicucore.A.dylib | `_unumf_close` | 1 | opencode-desktop |
| libicucore.A.dylib | `_unumf_closeResult` | 1 | opencode-desktop |
| libicucore.A.dylib | `_unumf_formatDecimal` | 1 | opencode-desktop |
| libicucore.A.dylib | `_unumf_formatDouble` | 1 | opencode-desktop |
| libicucore.A.dylib | `_unumf_openForSkeletonAndLocale` | 1 | opencode-desktop |
| libicucore.A.dylib | `_unumf_openResult` | 1 | opencode-desktop |
| libicucore.A.dylib | `_unumf_resultGetAllFieldPositions` | 1 | opencode-desktop |
| libicucore.A.dylib | `_unumf_resultToString` | 1 | opencode-desktop |
| libicucore.A.dylib | `_unumrf_close` | 1 | opencode-desktop |
| libicucore.A.dylib | `_unumrf_closeResult` | 1 | opencode-desktop |
| libicucore.A.dylib | `_unumrf_formatDecimalRange` | 1 | opencode-desktop |
| libicucore.A.dylib | `_unumrf_formatDoubleRange` | 1 | opencode-desktop |
| libicucore.A.dylib | `_unumrf_openForSkeletonWithCollapseAndIdentityFallback` | 1 | opencode-desktop |
| libicucore.A.dylib | `_unumrf_openResult` | 1 | opencode-desktop |
| libicucore.A.dylib | `_unumrf_resultAsValue` | 1 | opencode-desktop |
| libicucore.A.dylib | `_unumsys_close` | 1 | opencode-desktop |
| libicucore.A.dylib | `_unumsys_getName` | 1 | opencode-desktop |
| libicucore.A.dylib | `_unumsys_isAlgorithmic` | 1 | opencode-desktop |
| libicucore.A.dylib | `_unumsys_open` | 1 | opencode-desktop |
| libicucore.A.dylib | `_unumsys_openAvailableNames` | 1 | opencode-desktop |
| libicucore.A.dylib | `_unumsys_openByName` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uplrules_close` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uplrules_getKeywords` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uplrules_openForType` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uplrules_selectForRange` | 1 | opencode-desktop |
| libicucore.A.dylib | `_uplrules_selectFormatted` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ureldatefmt_close` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ureldatefmt_closeResult` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ureldatefmt_format` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ureldatefmt_formatNumeric` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ureldatefmt_formatNumericToResult` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ureldatefmt_formatToResult` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ureldatefmt_open` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ureldatefmt_openResult` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ureldatefmt_resultAsValue` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ures_close` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ures_getByKey` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ures_getStringByKey` | 1 | opencode-desktop |
| libicucore.A.dylib | `_ures_open` | 1 | opencode-desktop |
| libncurses.5.4.dylib | `_assume_default_colors` | 1 | kitty |
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
| libpcap.A.dylib | `_pcap_activate` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_breakloop` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_can_set_rfmon` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_close` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_compile` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_create` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_datalink` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_datalink_name_to_val` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_datalink_val_to_description` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_datalink_val_to_name` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_dispatch` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_findalldevs` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_free_datalinks` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_free_tstamp_types` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_freealldevs` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_freecode` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_geterr` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_lib_version` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_list_datalinks` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_list_tstamp_types` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_lookupnet` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_open_dead` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_open_live` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_set_buffer_size` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_set_datalink` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_set_promisc` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_set_rfmon` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_set_snaplen` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_set_timeout` | 1 | wireshark-app |
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
| libsandbox.1.dylib | `_sandbox_apply_container` | 1 | iterm2 |
| libsasl2.2.dylib | `_sasl_decode` | 1 | utm |
| libsasl2.2.dylib | `_sasl_dispose` | 1 | utm |
| libsasl2.2.dylib | `_sasl_encode` | 1 | utm |
| libsasl2.2.dylib | `_sasl_errdetail` | 1 | utm |
| libsasl2.2.dylib | `_sasl_errstring` | 1 | utm |
| libsasl2.2.dylib | `_sasl_getprop` | 1 | utm |
| libsasl2.2.dylib | `_sasl_listmech` | 1 | utm |
| libsasl2.2.dylib | `_sasl_server_init` | 1 | utm |
| libsasl2.2.dylib | `_sasl_server_new` | 1 | utm |
| libsasl2.2.dylib | `_sasl_server_start` | 1 | utm |
| libsasl2.2.dylib | `_sasl_server_step` | 1 | utm |
| libsasl2.2.dylib | `_sasl_setprop` | 1 | utm |
| libsqlite3.dylib | `_sqlite3_mutex_alloc` | 1 | whatsapp |
| libsqlite3.dylib | `_sqlite3_mutex_enter` | 1 | whatsapp |
| libsqlite3.dylib | `_sqlite3_mutex_free` | 1 | whatsapp |
| libsqlite3.dylib | `_sqlite3_mutex_leave` | 1 | whatsapp |
| libsqlite3.dylib | `_sqlite3_snapshot_cmp` | 1 | clipy |
| libsqlite3.dylib | `_sqlite3_snapshot_free` | 1 | clipy |
| libsqlite3.dylib | `_sqlite3_snapshot_get` | 1 | clipy |
| libsqlite3.dylib | `_sqlite3_snapshot_open` | 1 | clipy |
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
