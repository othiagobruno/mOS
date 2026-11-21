# Lacunas de Leonardo (runtime C/C++)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Leonardo (runtime C/C++) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**8652** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| libSystem.B.dylib | `_vswprintf` | 88 | 1password, adobe-acrobat-reader, anki, another-redis-desktop-manager |
| libSystem.B.dylib | `_swprintf` | 86 | 1password, adobe-acrobat-reader, affinity, android-studio |
| libcups.2.dylib | `_ppdClose` | 31 | adobe-acrobat-reader, anki, another-redis-desktop-manager, anythingllm |
| libcups.2.dylib | `_ppdFindOption` | 31 | adobe-acrobat-reader, anki, another-redis-desktop-manager, anythingllm |
| libcups.2.dylib | `_ppdFindMarkedChoice` | 28 | adobe-acrobat-reader, anki, another-redis-desktop-manager, anythingllm |
| libSystem.B.dylib | `_hash_create` | 24 | alt-tab, android-file-transfer, bbedit, bettertouchtool |
| libSystem.B.dylib | `_hash_search` | 24 | alt-tab, android-file-transfer, bbedit, bettertouchtool |
| libSystem.B.dylib | `__res` | 20 | adobe-acrobat-reader, anki, audacity, calibre |
| libcups.2.dylib | `_cupsGetDests2` | 20 | adobe-acrobat-reader, another-redis-desktop-manager, anythingllm, arduino-ide |
| libSystem.B.dylib | `_dbm_clearerr` | 18 | android-studio, anki, blender, calibre |
| libSystem.B.dylib | `_dbm_close` | 18 | android-studio, anki, blender, calibre |
| libSystem.B.dylib | `_dbm_delete` | 18 | android-studio, anki, blender, calibre |
| libSystem.B.dylib | `_dbm_error` | 18 | android-studio, anki, blender, calibre |
| libSystem.B.dylib | `_dbm_fetch` | 18 | android-studio, anki, blender, calibre |
| libSystem.B.dylib | `_dbm_firstkey` | 18 | android-studio, anki, blender, calibre |
| libSystem.B.dylib | `_dbm_nextkey` | 18 | android-studio, anki, blender, calibre |
| libSystem.B.dylib | `_dbm_open` | 18 | android-studio, anki, blender, calibre |
| libSystem.B.dylib | `_dbm_store` | 18 | android-studio, anki, blender, calibre |
| libcups.2.dylib | `_ppdOpenFile` | 18 | anki, another-redis-desktop-manager, calibre, db-browser-for-sqlite |
| libedit.3.dylib | `_add_history` | 18 | anki, blender, calibre, capcut |
| libcups.2.dylib | `_httpBlocking` | 16 | adobe-acrobat-reader, another-redis-desktop-manager, anythingllm, arduino-ide |
| libcups.2.dylib | `_httpError` | 16 | adobe-acrobat-reader, another-redis-desktop-manager, anythingllm, arduino-ide |
| libcups.2.dylib | `_ppdFindAttr` | 16 | adobe-acrobat-reader, another-redis-desktop-manager, anythingllm, arduino-ide |
| libcups.2.dylib | `_ppdFindChoice` | 16 | adobe-acrobat-reader, another-redis-desktop-manager, anythingllm, arduino-ide |
| libcups.2.dylib | `_ppdMarkDefaults` | 16 | adobe-acrobat-reader, another-redis-desktop-manager, anythingllm, arduino-ide |
| libedit.3.dylib | `_read_history` | 16 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_rl_attempted_completion_function` | 16 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_write_history` | 16 | anki, blender, calibre, capcut |
| libffi.dylib | `_ffi_call` | 16 | anki, calibre, capcut, dropbox |
| libedit.3.dylib | `_history_truncate_file` | 15 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_rl_attempted_completion_over` | 15 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_rl_completion_append_character` | 15 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_rl_completion_matches` | 15 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_rl_line_buffer` | 15 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_rl_readline_name` | 15 | anki, blender, calibre, capcut |
| libffi.dylib | `_ffi_prep_cif` | 15 | anki, calibre, capcut, dropbox |
| libicucore.A.dylib | `_u_strToLower` | 15 | bitwarden, capcut, cherry-studio, gemini |
| libicucore.A.dylib | `_u_strToUpper` | 15 | bitwarden, capcut, cherry-studio, gemini |
| libicucore.A.dylib | `_ubrk_close` | 15 | bitwarden, capcut, cherry-studio, gemini |
| libicucore.A.dylib | `_ubrk_following` | 15 | bitwarden, capcut, cherry-studio, gemini |
| libicucore.A.dylib | `_ubrk_open` | 15 | bitwarden, capcut, cherry-studio, gemini |
| libicucore.A.dylib | `_ubrk_setText` | 15 | bitwarden, capcut, cherry-studio, gemini |
| libicucore.A.dylib | `_unorm2_getNFCInstance` | 15 | bitwarden, capcut, cherry-studio, gemini |
| libicucore.A.dylib | `_unorm2_normalize` | 15 | bitwarden, capcut, cherry-studio, gemini |
| libcups.2.dylib | `_ppdOpenFd` | 14 | adobe-acrobat-reader, anythingllm, arduino-ide, battery |
| libedit.3.dylib | `_clear_history` | 14 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_emacs_meta_keymap` | 14 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_history_get` | 14 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_history_get_history_state` | 14 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_remove_history` | 14 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_replace_history_entry` | 14 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_rl_bind_key` | 14 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_rl_bind_key_in_map` | 14 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_rl_callback_handler_install` | 14 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_rl_callback_handler_remove` | 14 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_rl_callback_read_char` | 14 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_rl_cleanup_after_signal` | 14 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_rl_complete` | 14 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_rl_completer_word_break_characters` | 14 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_rl_completion_display_matches_hook` | 14 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_rl_completion_type` | 14 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_rl_free_line_state` | 14 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_rl_initialize` | 14 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_rl_insert` | 14 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_rl_insert_text` | 14 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_rl_instream` | 14 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_rl_library_version` | 14 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_rl_outstream` | 14 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_rl_parse_and_bind` | 14 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_rl_pre_input_hook` | 14 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_rl_prep_terminal` | 14 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_rl_read_init_file` | 14 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_rl_readline_version` | 14 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_rl_redisplay` | 14 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_rl_startup_hook` | 14 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_rl_variable_bind` | 14 | anki, blender, calibre, capcut |
| libedit.3.dylib | `_using_history` | 14 | anki, blender, calibre, capcut |
| libncurses.5.4.dylib | `_setupterm` | 14 | arduino-ide, blender, calibre, crossover |
| libncurses.5.4.dylib | `_tigetnum` | 14 | arduino-ide, blender, calibre, crossover |
| libcups.2.dylib | `_cupsGetDest` | 13 | adobe-acrobat-reader, another-redis-desktop-manager, anythingllm, arduino-ide |
| libcups.2.dylib | `_cupsGetPPD` | 13 | adobe-acrobat-reader, another-redis-desktop-manager, anythingllm, arduino-ide |
| libcups.2.dylib | `_cupsGetPPD2` | 13 | adobe-acrobat-reader, another-redis-desktop-manager, anythingllm, arduino-ide |
| libcups.2.dylib | `_cupsMarkOptions` | 13 | adobe-acrobat-reader, another-redis-desktop-manager, anythingllm, arduino-ide |
| libcups.2.dylib | `_httpConnectEncrypt` | 13 | adobe-acrobat-reader, another-redis-desktop-manager, anythingllm, arduino-ide |
| libcups.2.dylib | `_ppdErrorString` | 13 | adobe-acrobat-reader, another-redis-desktop-manager, anythingllm, arduino-ide |
| libcups.2.dylib | `_ppdLastError` | 13 | adobe-acrobat-reader, another-redis-desktop-manager, anythingllm, arduino-ide |
| libexpat.1.dylib | `_XML_ParserFree` | 13 | anki, anythingllm, fork, github |
| libexpat.1.dylib | `_XML_SetElementHandler` | 13 | anki, anythingllm, fork, github |
| libexpat.1.dylib | `_XML_SetUserData` | 13 | anki, anythingllm, fork, github |
| libffi.dylib | `_ffi_type_void` | 13 | anki, calibre, capcut, dropbox |
| libicucore.A.dylib | `_ubrk_preceding` | 13 | bitwarden, capcut, cherry-studio, gemini |
| libncurses.5.4.dylib | `_tigetstr` | 13 | blender, calibre, crossover, deluge |
| libexpat.1.dylib | `_XML_ErrorString` | 12 | anki, anythingllm, fork, github |
| libexpat.1.dylib | `_XML_GetErrorCode` | 12 | anki, anythingllm, fork, github |
| libexpat.1.dylib | `_XML_ParserCreate` | 12 | anki, anythingllm, fork, github |
| libexpat.1.dylib | `_XML_SetCharacterDataHandler` | 12 | anki, anythingllm, fork, github |
| libffi.dylib | `_ffi_type_pointer` | 12 | anki, calibre, capcut, dropbox |
| libffi.dylib | `_ffi_type_uint64` | 12 | anki, calibre, capcut, dropbox |
| libicucore.A.dylib | `_u_charName` | 12 | bbedit, bitwarden, capcut, gemini |
| libicucore.A.dylib | `_u_strToTitle` | 12 | bitwarden, capcut, gemini, monitorcontrol |
| libicucore.A.dylib | `_ubrk_setUText` | 12 | bitwarden, capcut, gemini, monitorcontrol |
| libncurses.5.4.dylib | `_COLORS` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_COLOR_PAIRS` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_COLS` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_LINES` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_acs_map` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_beep` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_cbreak` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_clearok` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_def_prog_mode` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_def_shell_mode` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_delwin` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_doupdate` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_endwin` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_getmouse` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_has_colors` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_init_color` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_init_pair` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_keyname` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_keypad` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_mousemask` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_mvwin` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_napms` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_newpad` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_newwin` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_nl` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_nodelay` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_noecho` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_nonl` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_prefresh` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_resetty` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_resize_term` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_savetty` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_scrollok` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_start_color` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_stdscr` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_tigetflag` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_ungetch` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_use_default_colors` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_waddch` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_waddnstr` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_wborder` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_wclrtobot` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_wclrtoeol` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_werase` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_wgetch` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_wmove` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_wnoutrefresh` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_wrefresh` | 12 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_wresize` | 12 | blender, calibre, crossover, deluge |
| libSystem.B.dylib | `_swscanf` | 11 | adobe-acrobat-reader, audacity, bambu-studio, epic-games |
| libcups.2.dylib | `_cupsRemoveDest` | 11 | adobe-acrobat-reader, anythingllm, arduino-ide, battery |
| libedit.3.dylib | `_rl_basic_word_break_characters` | 11 | anki, blender, calibre, capcut |
| libffi.dylib | `_ffi_type_double` | 11 | anki, calibre, capcut, dropbox |
| libffi.dylib | `_ffi_type_float` | 11 | anki, calibre, capcut, dropbox |
| libffi.dylib | `_ffi_type_sint32` | 11 | anki, calibre, capcut, dropbox |
| libffi.dylib | `_ffi_type_sint64` | 11 | anki, calibre, capcut, dropbox |
| libffi.dylib | `_ffi_type_uint32` | 11 | anki, calibre, capcut, dropbox |
| libffi.dylib | `_ffi_type_uint8` | 11 | anki, calibre, capcut, dropbox |
| libicucore.A.dylib | `_u_charAge` | 11 | bitwarden, capcut, gemini, monitorcontrol |
| libicucore.A.dylib | `_u_getNumericValue` | 11 | bitwarden, capcut, gemini, monitorcontrol |
| libicucore.A.dylib | `_unorm2_hasBoundaryBefore` | 11 | bitwarden, capcut, gemini, monitorcontrol |
| libicucore.A.dylib | `_unorm2_spanQuickCheckYes` | 11 | bitwarden, capcut, gemini, monitorcontrol |
| libicucore.A.dylib | `_utext_openUChars` | 11 | bitwarden, capcut, gemini, monitorcontrol |
| libicucore.A.dylib | `_utext_openUTF8` | 11 | bitwarden, capcut, gemini, monitorcontrol |
| libncurses.5.4.dylib | `_baudrate` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_can_change_color` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_color_content` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_copywin` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_curs_set` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_delay_output` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_derwin` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_echo` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_erasechar` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_filter` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_flash` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_flushinp` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_getwin` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_halfdelay` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_has_ic` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_has_il` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_has_key` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_idcok` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_idlok` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_immedok` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_initscr` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_intrflush` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_is_linetouched` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_is_term_resized` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_is_wintouched` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_isendwin` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_killchar` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_leaveok` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_longname` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_meta` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_mouseinterval` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_mvderwin` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_newscr` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_nocbreak` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_noqiflush` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_noraw` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_notimeout` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_overlay` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_overwrite` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_pair_content` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_pechochar` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_pnoutrefresh` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_putp` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_putwin` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_qiflush` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_raw` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_reset_prog_mode` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_reset_shell_mode` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_resizeterm` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_subpad` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_subwin` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_syncok` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_termattrs` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_termname` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_tparm` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_typeahead` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_unctrl` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_ungetmouse` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_use_env` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_wattr_off` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_wattr_on` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_wbkgd` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_wbkgdset` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_wchgat` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_wclear` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_wcursyncup` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_wdelch` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_wechochar` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_wenclose` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_wgetnstr` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_whline` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_winch` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_winnstr` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_winsch` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_winsdelln` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_winsnstr` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_wredrawln` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_wscrl` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_wsetscrreg` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_wsyncdown` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_wsyncup` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_wtimeout` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_wtouchln` | 11 | blender, calibre, crossover, deluge |
| libncurses.5.4.dylib | `_wvline` | 11 | blender, calibre, crossover, deluge |
| libcups.2.dylib | `_httpGetHostname` | 10 | firefox, floorp, gimp, inkscape |
| libffi.dylib | `_ffi_type_longdouble` | 10 | anki, calibre, capcut, dropbox |
| libffi.dylib | `_ffi_type_sint16` | 10 | anki, calibre, capcut, dropbox |
| libffi.dylib | `_ffi_type_sint8` | 10 | anki, calibre, capcut, dropbox |
| libffi.dylib | `_ffi_type_uint16` | 10 | anki, calibre, capcut, dropbox |
| libncurses.5.4.dylib | `_ESCDELAY` | 10 | blender, calibre, crossover, inkscape |
| libncurses.5.4.dylib | `_TABSIZE` | 10 | blender, calibre, crossover, inkscape |
| libncurses.5.4.dylib | `_set_escdelay` | 10 | blender, calibre, crossover, inkscape |
| libncurses.5.4.dylib | `_set_tabsize` | 10 | blender, calibre, crossover, inkscape |
| libpanel.5.4.dylib | `_bottom_panel` | 10 | blender, calibre, crossover, inkscape |
| libpanel.5.4.dylib | `_del_panel` | 10 | blender, calibre, crossover, inkscape |
| libpanel.5.4.dylib | `_hide_panel` | 10 | blender, calibre, crossover, inkscape |
| libpanel.5.4.dylib | `_move_panel` | 10 | blender, calibre, crossover, inkscape |
| libpanel.5.4.dylib | `_new_panel` | 10 | blender, calibre, crossover, inkscape |
| libpanel.5.4.dylib | `_panel_above` | 10 | blender, calibre, crossover, inkscape |
| libpanel.5.4.dylib | `_panel_below` | 10 | blender, calibre, crossover, inkscape |
| libpanel.5.4.dylib | `_panel_hidden` | 10 | blender, calibre, crossover, inkscape |
| libpanel.5.4.dylib | `_panel_userptr` | 10 | blender, calibre, crossover, inkscape |
| libpanel.5.4.dylib | `_replace_panel` | 10 | blender, calibre, crossover, inkscape |
| libpanel.5.4.dylib | `_set_panel_userptr` | 10 | blender, calibre, crossover, inkscape |
| libpanel.5.4.dylib | `_show_panel` | 10 | blender, calibre, crossover, inkscape |
| libpanel.5.4.dylib | `_top_panel` | 10 | blender, calibre, crossover, inkscape |
| libpanel.5.4.dylib | `_update_panels` | 10 | blender, calibre, crossover, inkscape |
| libSystem.B.dylib | `_clnt_create` | 9 | blender, freecad, inkscape, kicad |
| libSystem.B.dylib | `_clnt_spcreateerror` | 9 | blender, freecad, inkscape, kicad |
| libSystem.B.dylib | `_devname` | 9 | adobe-acrobat-reader, calibre, capcut, emacs-app |
| libSystem.B.dylib | `_xdr_enum` | 9 | blender, freecad, inkscape, kicad |
| libSystem.B.dylib | `_xdr_pointer` | 9 | blender, freecad, inkscape, kicad |
| libSystem.B.dylib | `_xdr_string` | 9 | blender, freecad, inkscape, kicad |
| libSystem.B.dylib | `_yp_all` | 9 | blender, freecad, inkscape, kicad |
| libSystem.B.dylib | `_yp_get_default_domain` | 9 | blender, freecad, inkscape, kicad |
| libSystem.B.dylib | `_yp_master` | 9 | blender, freecad, inkscape, kicad |
| libSystem.B.dylib | `_yp_match` | 9 | blender, freecad, inkscape, kicad |
| libSystem.B.dylib | `_yperr_string` | 9 | blender, freecad, inkscape, kicad |
| libexpat.1.dylib | `_XML_GetBuffer` | 9 | anki, anythingllm, iina, inkscape |
| libexpat.1.dylib | `_XML_ParseBuffer` | 9 | anki, anythingllm, iina, inkscape |
| libSystem.B.dylib | `___invert_d4` | 8 | capcut, gemini, macwhisper, monitorcontrol |
| libSystem.B.dylib | `___invert_f2` | 8 | capcut, feishu, gemini, monitorcontrol |
| libSystem.B.dylib | `___invert_f4` | 8 | capcut, gemini, monitorcontrol, mos |
| libSystem.B.dylib | `___sandbox_ms` | 8 | adobe-acrobat-reader, anythingllm, arduino-ide, battery |
| libSystem.B.dylib | `_ftime` | 8 | adobe-acrobat-reader, feishu, microsoft-excel, microsoft-office |
| libSystem.B.dylib | `_nl_langinfo_l` | 8 | bambu-studio, freecad, kicad, orcaslicer |
| libcups.2.dylib | `_cupsGetDests` | 8 | firefox, floorp, librewolf, teamviewer |
| libexpat.1.dylib | `_XML_GetCurrentLineNumber` | 8 | anki, anythingllm, iina, inkscape |
| libffi.dylib | `_ffi_closure_alloc` | 8 | calibre, kitty, krita, libreoffice |
| libffi.dylib | `_ffi_prep_closure_loc` | 8 | calibre, kitty, krita, libreoffice |
| libSystem.B.dylib | `_NSAddressOfSymbol` | 7 | adobe-acrobat-reader, freecad, keka, kicad |
| libSystem.B.dylib | `_NSLookupAndBindSymbol` | 7 | adobe-acrobat-reader, capcut, inkscape, kicad |
| libSystem.B.dylib | `___invert_d2` | 7 | capcut, gemini, monitorcontrol, mos |
| libSystem.B.dylib | `___invert_d3` | 7 | capcut, gemini, monitorcontrol, mos |
| libSystem.B.dylib | `_dbopen` | 7 | android-studio, cyberduck, datagrip, duckduckgo |
| libSystem.B.dylib | `_exc_server` | 7 | android-file-transfer, cmux, ghostty, inkscape |
| libSystem.B.dylib | `_putwc` | 7 | freecad, inkscape, microsoft-excel, microsoft-outlook |
| libSystem.B.dylib | `_wcsspn` | 7 | adobe-acrobat-reader, affinity, audacity, bambu-studio |
| libcups.2.dylib | `_cupsConnectDest` | 7 | firefox, floorp, librewolf, thunderbird |
| libcups.2.dylib | `_cupsGetDestMediaByIndex` | 7 | firefox, floorp, librewolf, thunderbird |
| libcups.2.dylib | `_cupsGetDestMediaCount` | 7 | firefox, floorp, librewolf, thunderbird |
| libcups.2.dylib | `_cupsGetDestMediaDefault` | 7 | firefox, floorp, librewolf, thunderbird |
| libcups.2.dylib | `_cupsLocalizeDestMedia` | 7 | firefox, floorp, librewolf, thunderbird |
| libcups.2.dylib | `_cupsSetUser` | 7 | adobe-acrobat-reader, gimp, inkscape, meld |
| libcups.2.dylib | `_httpAddrPort` | 7 | firefox, floorp, librewolf, thunderbird |
| libcups.2.dylib | `_httpGetAddress` | 7 | firefox, floorp, librewolf, thunderbird |
| libexpat.1.dylib | `_XML_Parse` | 7 | anki, fork, github, keka |
| libexpat.1.dylib | `_XML_SetDoctypeDeclHandler` | 7 | anki, anythingllm, iina, inkscape |
| libffi.dylib | `_ffi_closure_free` | 7 | calibre, kitty, krita, libreoffice |
| libffi.dylib | `_ffi_prep_cif_var` | 7 | calibre, kitty, krita, libreoffice |
| libiconv.2.dylib | `_libiconv_set_relocation_prefix` | 7 | deluge, krita, lmms, openscad |
| libicucore.A.dylib | `_u_errorName` | 7 | cherry-studio, iterm2, openclaw, opencode-desktop |
| libncurses.5.4.dylib | `_setcchar` | 7 | android-studio, blender, inkscape, kitty |
| libncurses.5.4.dylib | `_tgetent` | 7 | gcc-arm-embedded, inkscape, keepassxc, lmms |
| libncurses.5.4.dylib | `_tgetflag` | 7 | gcc-arm-embedded, inkscape, keepassxc, lmms |
| libncurses.5.4.dylib | `_tgetnum` | 7 | gcc-arm-embedded, inkscape, keepassxc, lmms |
| libncurses.5.4.dylib | `_tgetstr` | 7 | gcc-arm-embedded, inkscape, keepassxc, lmms |
| libncurses.5.4.dylib | `_tgoto` | 7 | gcc-arm-embedded, inkscape, keepassxc, lmms |
| libncurses.5.4.dylib | `_tputs` | 7 | gcc-arm-embedded, inkscape, keepassxc, lmms |
| libncurses.5.4.dylib | `_unget_wch` | 7 | android-studio, blender, inkscape, kitty |
| libncurses.5.4.dylib | `_wadd_wch` | 7 | android-studio, blender, inkscape, kitty |
| libncurses.5.4.dylib | `_waddnwstr` | 7 | android-studio, blender, inkscape, kitty |
| libncurses.5.4.dylib | `_wget_wch` | 7 | android-studio, blender, inkscape, kitty |
| libncurses.5.4.dylib | `_wins_nwstr` | 7 | android-studio, blender, inkscape, kitty |
| libSystem.B.dylib | `_NSLibraryNameForModule` | 6 | capcut, inkscape, kicad, mactex |
| libSystem.B.dylib | `_NSLinkEditError` | 6 | freecad, keka, kicad, mactex |
| libSystem.B.dylib | `_NSLinkModule` | 6 | freecad, keka, kicad, mactex |
| libSystem.B.dylib | `_NSLookupSymbolInModule` | 6 | freecad, keka, kicad, mactex |
| libSystem.B.dylib | `_NSModuleForSymbol` | 6 | capcut, inkscape, kicad, mactex |
| libSystem.B.dylib | `_NSUnLinkModule` | 6 | freecad, keka, kicad, mactex |
| libSystem.B.dylib | `_NXFindBestFatArch` | 6 | freecad, kicad, mactex, mactex-no-gui |
| libSystem.B.dylib | `___udivmodti4` | 6 | capcut, kicad, mactex, mactex-no-gui |
| libSystem.B.dylib | `__os_log_internal` | 6 | capcut, mac-mouse-fix, monitorcontrol, onlyoffice |
| libSystem.B.dylib | `_getwchar` | 6 | android-studio, anythingllm, insomnia, ollama-app |
| libSystem.B.dylib | `_inet_net_pton` | 6 | another-redis-desktop-manager, openmtp, pencil, ubersicht |
| libSystem.B.dylib | `_open_memstream` | 6 | freecad, istat-menus, qgis, shotcut |
| libSystem.B.dylib | `_pthread_chdir_np` | 6 | 1password, adobe-acrobat-reader, capcut, feishu |
| libSystem.B.dylib | `_swapcontext` | 6 | bambu-studio, blender, freecad, kicad |
| libSystem.B.dylib | `_vfprintf_l` | 6 | bambu-studio, freecad, inkscape, kicad |
| libSystem.B.dylib | `_wcstod_l` | 6 | audacity, bambu-studio, espanso, kicad |
| libncurses.5.4.dylib | `_BC` | 6 | gcc-arm-embedded, inkscape, keepassxc, lmms |
| libncurses.5.4.dylib | `_PC` | 6 | gcc-arm-embedded, inkscape, keepassxc, lmms |
| libncurses.5.4.dylib | `_UP` | 6 | gcc-arm-embedded, inkscape, keepassxc, lmms |
| libSystem.B.dylib | `_NSCreateObjectFileImageFromMemory` | 5 | freecad, kicad, mactex, mactex-no-gui |
| libSystem.B.dylib | `_NSDestroyObjectFileImage` | 5 | freecad, kicad, mactex, mactex-no-gui |
| libSystem.B.dylib | `_NSLookupSymbolInImage` | 5 | freecad, kicad, mactex, mactex-no-gui |
| libSystem.B.dylib | `___truncsfhf2` | 5 | moonlight, orion, qgis, raycast |
| libSystem.B.dylib | `__malloc_fork_parent` | 5 | microsoft-auto-update, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| libSystem.B.dylib | `__malloc_fork_prepare` | 5 | microsoft-auto-update, microsoft-excel, microsoft-outlook, microsoft-powerpoint |
| libSystem.B.dylib | `__os_log_create` | 5 | mac-mouse-fix, monitorcontrol, onlyoffice, ubersicht |
| libSystem.B.dylib | `_cacos` | 5 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_cacosh` | 5 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_casin` | 5 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_casinh` | 5 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_catan` | 5 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_ccos` | 5 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_ccosh` | 5 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_csin` | 5 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_csinh` | 5 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_ctan` | 5 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_ctanh` | 5 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_daemon$1050` | 5 | freecad, nordvpn, tunnelblick, vlc |
| libSystem.B.dylib | `_fgetws` | 5 | adobe-acrobat-reader, crossover, keka, microsoft-teams |
| libSystem.B.dylib | `_fmemopen` | 5 | freecad, gimp, istat-menus, libreoffice |
| libSystem.B.dylib | `_strfmon_l` | 5 | bambu-studio, freecad, kicad, orcaslicer |
| libSystem.B.dylib | `_vprintf_l` | 5 | bambu-studio, freecad, kicad, orcaslicer |
| libSystem.B.dylib | `_vsprintf_l` | 5 | bambu-studio, freecad, kicad, orcaslicer |
| libSystem.B.dylib | `_wcstol_l` | 5 | audacity, bambu-studio, kicad, orcaslicer |
| libSystem.B.dylib | `_wcstoul_l` | 5 | audacity, bambu-studio, kicad, orcaslicer |
| libedit.3.dylib | `_readline` | 5 | deluge, hammerspoon, openmtp, orion |
| libncurses.5.4.dylib | `_curses_version` | 5 | calibre, crossover, kitty, krita |
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
| libSystem.B.dylib | `_NXGetArchInfoFromName` | 4 | android-file-transfer, openttd, steam, vlc |
| libSystem.B.dylib | `__NSGetProgname` | 4 | bambu-studio, kitty, orcaslicer, orion |
| libSystem.B.dylib | `__sigtramp` | 4 | another-redis-desktop-manager, openmtp, pencil, webtorrent |
| libSystem.B.dylib | `_aio_cancel` | 4 | adobe-acrobat-reader, cloudflare-warp, localsend, virtualbox |
| libSystem.B.dylib | `_cabs` | 4 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_cabsf` | 4 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_cabsl` | 4 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_cacosf` | 4 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_cacoshf` | 4 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_cacoshl` | 4 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_cacosl` | 4 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_carg` | 4 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_cargf` | 4 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_cargl` | 4 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_casinf` | 4 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_casinhf` | 4 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_casinhl` | 4 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_casinl` | 4 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_catanf` | 4 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_catanl` | 4 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_ccosf` | 4 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_ccoshf` | 4 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_ccoshl` | 4 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_ccosl` | 4 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_csinf` | 4 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_csinhf` | 4 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_csinhl` | 4 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_csinl` | 4 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_ctanf` | 4 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_ctanhf` | 4 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_ctanhl` | 4 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_ctanl` | 4 | blender, freecad, inkscape, qgis |
| libSystem.B.dylib | `_fcvt` | 4 | microsoft-excel, microsoft-outlook, microsoft-powerpoint, microsoft-word |
| libSystem.B.dylib | `_fgetwc` | 4 | microsoft-excel, microsoft-outlook, microsoft-powerpoint, microsoft-word |
| libSystem.B.dylib | `_fmtcheck` | 4 | lmms, shotcut, tableplus, zoom |
| libSystem.B.dylib | `_gcvt` | 4 | microsoft-excel, onlyoffice, qlmarkdown, wechat |
| libSystem.B.dylib | `_getsectdatafromheader` | 4 | alt-tab, microsoft-teams, onedrive, windows-app |
| libSystem.B.dylib | `_iswascii` | 4 | microsoft-excel, microsoft-outlook, microsoft-powerpoint, microsoft-word |
| libSystem.B.dylib | `_pthread_cpu_number_np` | 4 | anki, calibre, microsoft-outlook, vnote |
| libSystem.B.dylib | `_strptime_l` | 4 | easydict, orbstack, proxyman, sourcetree |
| libSystem.B.dylib | `_strunvis` | 4 | android-studio, freecad, gimp, tableplus |
| libSystem.B.dylib | `_strvis` | 4 | android-studio, freecad, gimp, tableplus |
| libSystem.B.dylib | `_vis` | 4 | android-studio, freecad, gimp, tableplus |
| libSystem.B.dylib | `_wcsnlen` | 4 | dolphin, freecad, moonlight, zoom |
| libSystem.B.dylib | `_wordexp` | 4 | anydesk, freecad, google-drive, teamviewer |
| libSystem.B.dylib | `_wordfree` | 4 | anydesk, freecad, google-drive, teamviewer |
| libbz2.1.0.dylib | `_BZ2_bzread` | 4 | bbedit, inkscape, sequel-ace, tableplus |
| libbz2.1.0.dylib | `_BZ2_bzwrite` | 4 | bbedit, inkscape, sequel-ace, tableplus |
| libcups.2.dylib | `_cupsCloseDestJob` | 4 | adobe-acrobat-reader, onlyoffice, openmtp, pencil |
| libcups.2.dylib | `_cupsCreateDestJob` | 4 | adobe-acrobat-reader, onlyoffice, openmtp, pencil |
| libcups.2.dylib | `_cupsFinishDestDocument` | 4 | adobe-acrobat-reader, onlyoffice, openmtp, pencil |
| libcups.2.dylib | `_cupsGetDestMediaByName` | 4 | adobe-acrobat-reader, onlyoffice, openmtp, pencil |
| libcups.2.dylib | `_cupsStartDestDocument` | 4 | adobe-acrobat-reader, onlyoffice, openmtp, pencil |
| libicucore.A.dylib | `_u_charDirection` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_u_charType` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_u_getVersion` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_u_isUWhiteSpace` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_u_toupper` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ubrk_clone` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ubrk_countAvailable` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ubrk_current` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ubrk_first` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ubrk_getAvailable` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ubrk_getRuleStatus` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ubrk_next` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucal_clone` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucal_close` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucal_get` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucal_getAttribute` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucal_getCanonicalTimeZoneID` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucal_getDayOfWeekType` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucal_getHostTimeZone` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucal_getKeywordValuesForLocale` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucal_getTimeZoneDisplayName` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucal_getTimeZoneOffsetFromLocal` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucal_open` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucal_openTimeZoneIDEnumeration` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucal_openTimeZones` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucal_setGregorianChange` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucal_setMillis` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucfpos_close` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucfpos_constrainCategory` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucfpos_constrainField` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucfpos_getCategory` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucfpos_getField` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucfpos_getIndexes` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucfpos_open` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucfpos_reset` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucol_close` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucol_countAvailable` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucol_getAttribute` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucol_getAvailable` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucol_getKeywordValues` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucol_getKeywordValuesForLocale` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucol_getRules` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucol_open` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucol_setAttribute` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucol_strcoll` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucol_strcollUTF8` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucurr_getName` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ucurr_openISOCurrencies` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_udat_close` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_udat_format` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_udat_formatForFields` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_udat_getCalendar` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_udat_open` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_udat_toPattern` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_udatpg_close` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_udatpg_getBestPatternWithOptions` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_udatpg_getFieldDisplayName` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_udatpg_getSkeleton` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_udatpg_open` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_udtitvfmt_close` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_udtitvfmt_closeResult` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_udtitvfmt_formatCalendarToResult` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_udtitvfmt_formatToResult` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_udtitvfmt_open` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_udtitvfmt_openResult` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_udtitvfmt_resultAsValue` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uenum_close` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uenum_count` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uenum_next` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uenum_unext` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ufieldpositer_close` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ufieldpositer_next` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ufieldpositer_open` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ufmtval_getString` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ufmtval_nextPosition` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uidna_nameToASCII` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uidna_nameToUnicode` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uldn_close` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uldn_keyValueDisplayName` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uldn_localeDisplayName` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uldn_openForContext` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uldn_regionDisplayName` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uldn_scriptDisplayName` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ulistfmt_close` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ulistfmt_closeResult` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ulistfmt_format` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ulistfmt_formatStringsToResult` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ulistfmt_openForType` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ulistfmt_openResult` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ulistfmt_resultAsValue` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uloc_addLikelySubtags` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uloc_canonicalize` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uloc_countAvailable` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uloc_forLanguageTag` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uloc_getAvailable` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uloc_getBaseName` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uloc_getCharacterOrientation` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uloc_getCountry` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uloc_getDefault` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uloc_getKeywordValue` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uloc_getLanguage` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uloc_getScript` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uloc_minimizeSubtags` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uloc_setKeywordValue` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uloc_toLanguageTag` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uloc_toUnicodeLocaleType` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_unorm2_getNFDInstance` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_unorm2_getNFKCInstance` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_unorm2_getNFKDInstance` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_unorm2_isNormalized` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_unum_close` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_unum_open` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_unum_setAttribute` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_unumf_close` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_unumf_closeResult` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_unumf_formatDecimal` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_unumf_formatDouble` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_unumf_openForSkeletonAndLocale` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_unumf_openResult` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_unumf_resultGetAllFieldPositions` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_unumf_resultToString` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_unumrf_close` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_unumrf_closeResult` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_unumrf_formatDecimalRange` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_unumrf_formatDoubleRange` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_unumrf_openForSkeletonWithCollapseAndIdentityFallback` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_unumrf_openResult` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_unumrf_resultAsValue` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_unumsys_close` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_unumsys_getName` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_unumsys_isAlgorithmic` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_unumsys_open` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_unumsys_openAvailableNames` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_unumsys_openByName` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uplrules_close` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uplrules_getKeywords` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uplrules_openForType` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uplrules_selectForRange` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_uplrules_selectFormatted` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ureldatefmt_close` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ureldatefmt_closeResult` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ureldatefmt_format` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ureldatefmt_formatNumeric` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ureldatefmt_formatNumericToResult` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ureldatefmt_formatToResult` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ureldatefmt_open` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ureldatefmt_openResult` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ureldatefmt_resultAsValue` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ures_close` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ures_getByKey` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ures_getStringByKey` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libicucore.A.dylib | `_ures_open` | 4 | cherry-studio, openclaw, opencode-desktop, orion |
| libncurses.5.4.dylib | `_getcurx` | 4 | blender, gcc-arm-embedded, orcaslicer, qgis |
| libncurses.5.4.dylib | `_getcury` | 4 | blender, gcc-arm-embedded, orcaslicer, qgis |
| libncurses.5.4.dylib | `_getmaxx` | 4 | blender, gcc-arm-embedded, orcaslicer, qgis |
| libncurses.5.4.dylib | `_getmaxy` | 4 | blender, gcc-arm-embedded, orcaslicer, qgis |
| libpcap.A.dylib | `_pcap_activate` | 4 | cloudflare-warp, crossover, istat-menus, wireshark-app |
| libpcap.A.dylib | `_pcap_close` | 4 | cloudflare-warp, crossover, istat-menus, wireshark-app |
| libpcap.A.dylib | `_pcap_create` | 4 | cloudflare-warp, crossover, istat-menus, wireshark-app |
| libpcap.A.dylib | `_pcap_set_snaplen` | 4 | cloudflare-warp, crossover, istat-menus, wireshark-app |
| libpcap.A.dylib | `_pcap_set_timeout` | 4 | cloudflare-warp, crossover, istat-menus, wireshark-app |
| libxar.1.dylib | `_xar_signature_first` | 4 | microsoft-auto-update, opera, teamviewer, zoom |
| libxar.1.dylib | `_xar_signature_get_x509certificate_count` | 4 | microsoft-auto-update, opera, teamviewer, zoom |
| libxar.1.dylib | `_xar_signature_get_x509certificate_data` | 4 | microsoft-auto-update, opera, teamviewer, zoom |
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
| libSystem.B.dylib | `___powidf2` | 3 | capcut, feishu, shotcut |
| libSystem.B.dylib | `__simd_pow_f4` | 3 | crossover, dolphin, upscayl |
| libSystem.B.dylib | `_coalition_info_resource_usage` | 3 | adobe-acrobat-reader, capcut, onlyoffice |
| libSystem.B.dylib | `_devname_r` | 3 | calibre, hex-fiend, vnote |
| libSystem.B.dylib | `_errc` | 3 | brave-browser, dropbox, microsoft-edge |
| libSystem.B.dylib | `_getmntinfo_r_np$INODE64` | 3 | bbedit, brave-browser, cmux |
| libSystem.B.dylib | `_getsectbynamefromheader` | 3 | clipy, nordvpn, sequel-ace |
| libSystem.B.dylib | `_htons` | 3 | capcut, quarto, rstudio |
| libSystem.B.dylib | `_kSBXProfilePureComputation` | 3 | anki, calibre, vnote |
| libSystem.B.dylib | `_lio_listio` | 3 | cloudflare-warp, localsend, virtualbox |
| libSystem.B.dylib | `_pthread_setconcurrency` | 3 | freecad, kicad, libreoffice |
| libSystem.B.dylib | `_regcomp_l` | 3 | comfy, iterm2, kicad |
| libSystem.B.dylib | `_sbrk` | 3 | freecad, godot, pixelorama |
| libSystem.B.dylib | `_tempnam` | 3 | adobe-acrobat-reader, deluge, vlc |
| libbz2.1.0.dylib | `_BZ2_bzWriteClose` | 3 | deluge, sequel-ace, tableplus |
| libcharset.1.dylib | `_locale_charset` | 3 | qgis, quarto, rstudio |
| libcups.2.dylib | `_cupsArrayFirst` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_cupsDoAuthentication` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_cupsEncryption` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_cupsFreeOptions` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_cupsLangDefault` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_cupsLangFree` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_cupsParseOptions` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_cupsSetEncryption` | 3 | gimp, inkscape, meld |
| libcups.2.dylib | `_cupsSetPasswordCB` | 3 | gimp, inkscape, meld |
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
| libexpat.1.dylib | `_XML_ExpatVersion` | 3 | anki, keka, kitty |
| libicucore.A.dylib | `_u_getUnicodeVersion` | 3 | cherry-studio, openclaw, opencode-desktop |
| libicucore.A.dylib | `_u_strlen` | 3 | iterm2, r-app, sequel-ace |
| libicucore.A.dylib | `_u_versionToString` | 3 | cherry-studio, openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucal_add` | 3 | cherry-studio, openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucal_clear` | 3 | cherry-studio, openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucal_getLimit` | 3 | cherry-studio, openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucal_getMillis` | 3 | cherry-studio, openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucal_getTimeZoneTransitionDate` | 3 | cherry-studio, openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucal_set` | 3 | cherry-studio, openclaw, opencode-desktop |
| libicucore.A.dylib | `_ucal_setTimeZone` | 3 | cherry-studio, openclaw, opencode-desktop |
| libicucore.A.dylib | `_udat_applyPattern` | 3 | cherry-studio, openclaw, opencode-desktop |
| libicucore.A.dylib | `_udat_clone` | 3 | cherry-studio, openclaw, opencode-desktop |
| libicucore.A.dylib | `_uloc_openKeywords` | 3 | cherry-studio, openclaw, opencode-desktop |
| libicucore.A.dylib | `_uloc_setDefault` | 3 | cherry-studio, openclaw, opencode-desktop |
| libicucore.A.dylib | `_unorm2_getInstance` | 3 | cherry-studio, openclaw, opencode-desktop |
| libicucore.A.dylib | `_uregex_appendReplacement` | 3 | iterm2, r-app, sequel-ace |
| libicucore.A.dylib | `_uregex_appendTail` | 3 | iterm2, r-app, sequel-ace |
| libicucore.A.dylib | `_uregex_clone` | 3 | iterm2, r-app, sequel-ace |
| libicucore.A.dylib | `_uregex_close` | 3 | iterm2, r-app, sequel-ace |
| libicucore.A.dylib | `_uregex_end` | 3 | iterm2, r-app, sequel-ace |
| libicucore.A.dylib | `_uregex_find` | 3 | iterm2, r-app, sequel-ace |
| libicucore.A.dylib | `_uregex_findNext` | 3 | iterm2, r-app, sequel-ace |
| libicucore.A.dylib | `_uregex_groupCount` | 3 | iterm2, r-app, sequel-ace |
| libicucore.A.dylib | `_uregex_open` | 3 | iterm2, r-app, sequel-ace |
| libicucore.A.dylib | `_uregex_reset` | 3 | iterm2, r-app, sequel-ace |
| libicucore.A.dylib | `_uregex_setText` | 3 | iterm2, r-app, sequel-ace |
| libicucore.A.dylib | `_uregex_start` | 3 | iterm2, r-app, sequel-ace |
| libncurses.5.4.dylib | `_del_curterm` | 3 | arduino-ide, crossover, utm |
| libncurses.5.4.dylib | `_getattrs` | 3 | blender, orcaslicer, qgis |
| libncurses.5.4.dylib | `_getbegx` | 3 | blender, orcaslicer, qgis |
| libncurses.5.4.dylib | `_getbegy` | 3 | blender, orcaslicer, qgis |
| libncurses.5.4.dylib | `_getbkgd` | 3 | blender, orcaslicer, qgis |
| libncurses.5.4.dylib | `_getparx` | 3 | blender, orcaslicer, qgis |
| libncurses.5.4.dylib | `_getpary` | 3 | blender, orcaslicer, qgis |
| libncurses.5.4.dylib | `_is_leaveok` | 3 | blender, orcaslicer, qgis |
| libncurses.5.4.dylib | `_redrawwin` | 3 | blender, orcaslicer, qgis |
| libncurses.5.4.dylib | `_set_curterm` | 3 | arduino-ide, crossover, utm |
| libncurses.5.4.dylib | `_wattrset` | 3 | blender, orcaslicer, qgis |
| libpcap.A.dylib | `_pcap_findalldevs` | 3 | cloudflare-warp, crossover, wireshark-app |
| libpcap.A.dylib | `_pcap_freealldevs` | 3 | cloudflare-warp, crossover, wireshark-app |
| libpcap.A.dylib | `_pcap_geterr` | 3 | cloudflare-warp, crossover, wireshark-app |
| libpcap.A.dylib | `_pcap_next_ex` | 3 | cloudflare-warp, crossover, istat-menus |
| libsasl2.2.dylib | `_sasl_dispose` | 3 | mysqlworkbench, tableplus, utm |
| libxar.1.dylib | `_xar_signature_copy_signed_data` | 3 | microsoft-auto-update, opera, zoom |
| libxar.1.dylib | `_xar_signature_next` | 3 | microsoft-auto-update, opera, zoom |
| libxar.1.dylib | `_xar_signature_type` | 3 | microsoft-auto-update, opera, zoom |
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
| libSystem.B.dylib | `_CCSymmetricKeyUnwrap` | 2 | microsoft-outlook, orion |
| libSystem.B.dylib | `_CCSymmetricKeyWrap` | 2 | microsoft-outlook, orion |
| libSystem.B.dylib | `_CCSymmetricUnwrappedSize` | 2 | microsoft-outlook, orion |
| libSystem.B.dylib | `_CCSymmetricWrappedSize` | 2 | microsoft-outlook, orion |
| libSystem.B.dylib | `_CCrfc3394_iv` | 2 | microsoft-outlook, orion |
| libSystem.B.dylib | `_CCrfc3394_ivLen` | 2 | microsoft-outlook, orion |
| libSystem.B.dylib | `_SANDBOX_CHECK_NO_REPORT` | 2 | orion, visual-studio-code |
| libSystem.B.dylib | `__Unwind_Resume_or_Rethrow` | 2 | emacs-app, stremio |
| libSystem.B.dylib | `___floatuntixf` | 2 | hex-fiend, kicad |
| libSystem.B.dylib | `___truncdfhf2` | 2 | orion, qgis |
| libSystem.B.dylib | `__os_trace_with_buffer` | 2 | alt-tab, keycastr |
| libSystem.B.dylib | `_acct` | 2 | cloudflare-warp, localsend |
| libSystem.B.dylib | `_arc4random_addrandom` | 2 | qgis, tor-browser |
| libSystem.B.dylib | `_clock_get_attributes` | 2 | mongodb-compass, unity-hub |
| libSystem.B.dylib | `_daemon` | 2 | rancher, utm |
| libSystem.B.dylib | `_filesec_free` | 2 | openclaw, utm |
| libSystem.B.dylib | `_filesec_get_property` | 2 | openclaw, utm |
| libSystem.B.dylib | `_filesec_init` | 2 | openclaw, utm |
| libSystem.B.dylib | `_fstatfs64` | 2 | docker-desktop, orbstack |
| libSystem.B.dylib | `_fstatx_np$INODE64` | 2 | openclaw, utm |
| libSystem.B.dylib | `_htonl` | 2 | quarto, rstudio |
| libSystem.B.dylib | `_lfind` | 2 | onlyoffice, wireshark-app |
| libSystem.B.dylib | `_link_ntoa` | 2 | capcut, feishu |
| libSystem.B.dylib | `_malloc_printf` | 2 | mactex, mactex-no-gui |
| libSystem.B.dylib | `_os_trace_debug_enabled` | 2 | alt-tab, keycastr |
| libSystem.B.dylib | `_pthread_getconcurrency` | 2 | freecad, libreoffice |
| libSystem.B.dylib | `_pthread_self_is_exiting_np` | 2 | capcut, orion |
| libSystem.B.dylib | `_putwchar` | 2 | crossover, keka |
| libSystem.B.dylib | `_statx_np$INODE64` | 2 | openclaw, utm |
| libSystem.B.dylib | `_swap_load_command` | 2 | microsoft-edge, steam |
| libSystem.B.dylib | `_swap_mach_header` | 2 | microsoft-edge, steam |
| libSystem.B.dylib | `_swap_segment_command` | 2 | microsoft-edge, steam |
| libSystem.B.dylib | `_sys_siglist` | 2 | tableplus, tunnelblick |
| libSystem.B.dylib | `_unmount` | 2 | docker-desktop, orbstack |
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
| libbz2.1.0.dylib | `_BZ2_bzclose` | 2 | bbedit, inkscape |
| libbz2.1.0.dylib | `_BZ2_bzflush` | 2 | bbedit, inkscape |
| libbz2.1.0.dylib | `_BZ2_bzopen` | 2 | bbedit, inkscape |
| libcups.2.dylib | `_httpWait` | 2 | gimp, inkscape |
| libexslt.0.dylib | `_exsltRegisterAll` | 2 | inkscape, libreoffice |
| libffi.dylib | `_ffi_type_complex_double` | 2 | calibre, kitty |
| libffi.dylib | `_ffi_type_complex_float` | 2 | calibre, kitty |
| libffi.dylib | `_ffi_type_complex_longdouble` | 2 | calibre, kitty |
| libicucore.A.dylib | `_ubrk_previous` | 2 | openclaw, opencode-desktop |
| libmimerapi.dylib | `_MimerAddBatch` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerBeginSession8` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerBeginStatement8` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerBeginTransaction` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerCloseCursor` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerColumnCount` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerColumnName` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerColumnType` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerCurrentRow` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerEndSession` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerEndStatement` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerEndTransaction` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerExecute` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerExecuteStatement8` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerFetch` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerFetchScroll` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerGetBinary` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerGetBlobData` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerGetBoolean` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerGetDouble` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerGetError` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerGetFloat` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerGetInt32` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerGetInt64` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerGetLob` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerGetNclobData` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerGetSequenceInt64` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerGetString` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerGetUUID` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerIsNull` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerOpenCursor` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerParameterCount` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerParameterType` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerSetBinary` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerSetBlobData` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerSetBoolean` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerSetDouble` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerSetFloat` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerSetInt32` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerSetInt64` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerSetLob` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerSetNclobData8` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerSetNull` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerSetString8` | 2 | anki, moonlight |
| libmimerapi.dylib | `_MimerSetUUID` | 2 | anki, moonlight |
| libncurses.5.4.dylib | `_assume_default_colors` | 2 | calibre, kitty |
| libpam.2.dylib | `_pam_acct_mgmt` | 2 | freecad, utm |
| libpam.2.dylib | `_pam_end` | 2 | freecad, utm |
| libpam.2.dylib | `_pam_start` | 2 | freecad, utm |
| libpam.2.dylib | `_pam_strerror` | 2 | freecad, utm |
| libpcap.A.dylib | `_pcap_breakloop` | 2 | crossover, wireshark-app |
| libpcap.A.dylib | `_pcap_can_set_rfmon` | 2 | crossover, wireshark-app |
| libpcap.A.dylib | `_pcap_compile` | 2 | crossover, wireshark-app |
| libpcap.A.dylib | `_pcap_datalink` | 2 | crossover, wireshark-app |
| libpcap.A.dylib | `_pcap_datalink_name_to_val` | 2 | crossover, wireshark-app |
| libpcap.A.dylib | `_pcap_datalink_val_to_description` | 2 | crossover, wireshark-app |
| libpcap.A.dylib | `_pcap_datalink_val_to_name` | 2 | crossover, wireshark-app |
| libpcap.A.dylib | `_pcap_dump` | 2 | cloudflare-warp, crossover |
| libpcap.A.dylib | `_pcap_dump_close` | 2 | cloudflare-warp, crossover |
| libpcap.A.dylib | `_pcap_dump_open` | 2 | cloudflare-warp, crossover |
| libpcap.A.dylib | `_pcap_free_datalinks` | 2 | crossover, wireshark-app |
| libpcap.A.dylib | `_pcap_free_tstamp_types` | 2 | crossover, wireshark-app |
| libpcap.A.dylib | `_pcap_freecode` | 2 | crossover, wireshark-app |
| libpcap.A.dylib | `_pcap_lib_version` | 2 | crossover, wireshark-app |
| libpcap.A.dylib | `_pcap_list_datalinks` | 2 | crossover, wireshark-app |
| libpcap.A.dylib | `_pcap_list_tstamp_types` | 2 | crossover, wireshark-app |
| libpcap.A.dylib | `_pcap_lookupnet` | 2 | crossover, wireshark-app |
| libpcap.A.dylib | `_pcap_open_live` | 2 | crossover, wireshark-app |
| libpcap.A.dylib | `_pcap_set_buffer_size` | 2 | crossover, wireshark-app |
| libpcap.A.dylib | `_pcap_set_datalink` | 2 | crossover, wireshark-app |
| libpcap.A.dylib | `_pcap_set_immediate_mode` | 2 | cloudflare-warp, crossover |
| libpcap.A.dylib | `_pcap_set_promisc` | 2 | crossover, wireshark-app |
| libpcap.A.dylib | `_pcap_set_rfmon` | 2 | crossover, wireshark-app |
| libpcap.A.dylib | `_pcap_set_tstamp_type` | 2 | crossover, wireshark-app |
| libpcap.A.dylib | `_pcap_setfilter` | 2 | crossover, wireshark-app |
| libpcap.A.dylib | `_pcap_snapshot` | 2 | crossover, wireshark-app |
| libpcap.A.dylib | `_pcap_stats` | 2 | crossover, wireshark-app |
| libpcap.A.dylib | `_pcap_tstamp_type_name_to_val` | 2 | crossover, wireshark-app |
| libpcap.A.dylib | `_pcap_tstamp_type_val_to_description` | 2 | crossover, wireshark-app |
| libpcap.A.dylib | `_pcap_tstamp_type_val_to_name` | 2 | crossover, wireshark-app |
| libpmenergy.dylib | `_pm_energy_impact` | 2 | adobe-acrobat-reader, onlyoffice |
| libpmsample.dylib | `_pm_sample_task` | 2 | adobe-acrobat-reader, onlyoffice |
| libsasl2.2.dylib | `_sasl_client_done` | 2 | mysqlworkbench, tableplus |
| libsasl2.2.dylib | `_sasl_client_init` | 2 | mysqlworkbench, tableplus |
| libsasl2.2.dylib | `_sasl_client_new` | 2 | mysqlworkbench, tableplus |
| libsasl2.2.dylib | `_sasl_client_start` | 2 | mysqlworkbench, tableplus |
| libsasl2.2.dylib | `_sasl_client_step` | 2 | mysqlworkbench, tableplus |
| libsasl2.2.dylib | `_sasl_errdetail` | 2 | tableplus, utm |
| libsasl2.2.dylib | `_sasl_errstring` | 2 | tableplus, utm |
| libstdc++.6.dylib | `__ZNSs4_Rep10_M_destroyERKSaIcE` | 2 | android-file-transfer, deluge |
| libstdc++.6.dylib | `__ZNSs4_Rep20_S_empty_rep_storageE` | 2 | android-file-transfer, deluge |
| libstdc++.6.dylib | `__ZNSs6assignEPKcm` | 2 | android-file-transfer, deluge |
| libstdc++.6.dylib | `__ZNSsC1EPKcRKSaIcE` | 2 | android-file-transfer, deluge |
| libstdc++.6.dylib | `__ZNSsC1ERKSs` | 2 | android-file-transfer, deluge |
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
| libEndpointSecurity.dylib | `_es_delete_client` | 1 | nordvpn |
| libEndpointSecurity.dylib | `_es_mute_process` | 1 | nordvpn |
| libEndpointSecurity.dylib | `_es_new_client` | 1 | nordvpn |
| libEndpointSecurity.dylib | `_es_respond_auth_result` | 1 | nordvpn |
| libEndpointSecurity.dylib | `_es_respond_flags_result` | 1 | nordvpn |
| libEndpointSecurity.dylib | `_es_subscribe` | 1 | nordvpn |
| libEndpointSecurity.dylib | `_es_unsubscribe_all` | 1 | nordvpn |
| libIOReport.dylib | `_IOReportCopyAllChannels` | 1 | figma |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation12QMorphTarget11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation12QMorphTarget11qt_metacastEPKc` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation12QMorphTarget12addAttributeEPN8Qt3DCore10QAttributeE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation12QMorphTarget12fromGeometryEPN8Qt3DCore9QGeometryERK5QListI7QStringE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation12QMorphTarget13setAttributesERK5QListIPN8Qt3DCore10QAttributeEE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation12QMorphTarget15removeAttributeEPN8Qt3DCore10QAttributeE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation12QMorphTarget16staticMetaObjectE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation12QMorphTargetC2EP7QObject` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation13QClipAnimator11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation13QClipAnimator11qt_metacastEPKc` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation13QClipAnimator16staticMetaObjectE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation13QClipAnimator7setClipEPNS_22QAbstractAnimationClipE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation13QClipAnimatorC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation13QClipAnimatorD2Ev` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation14QAnimationClip11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation14QAnimationClip11qt_metacastEPKc` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation14QAnimationClip11setClipDataERKNS_18QAnimationClipDataE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation14QAnimationClip16staticMetaObjectE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation14QAnimationClipC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation14QAnimationClipD2Ev` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation14QChannelMapper10addMappingEPNS_23QAbstractChannelMappingE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation14QChannelMapper11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation14QChannelMapper11qt_metacastEPKc` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation14QChannelMapper13removeMappingEPNS_23QAbstractChannelMappingE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation14QChannelMapper16staticMetaObjectE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation14QChannelMapperC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation14QChannelMapperD2Ev` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation14QLerpClipBlend10setEndClipEPNS_22QAbstractClipBlendNodeE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation14QLerpClipBlend11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation14QLerpClipBlend11qt_metacastEPKc` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation14QLerpClipBlend12setStartClipEPNS_22QAbstractClipBlendNodeE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation14QLerpClipBlend14setBlendFactorEf` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation14QLerpClipBlend16staticMetaObjectE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation14QLerpClipBlendC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation14QLerpClipBlendD2Ev` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation15QAnimationGroup11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation15QAnimationGroup11qt_metacastEPKc` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation15QAnimationGroup11setPositionEf` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation15QAnimationGroup12addAnimationEPNS_18QAbstractAnimationE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation15QAnimationGroup13animationListEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation15QAnimationGroup13setAnimationsERK5QListIPNS_18QAbstractAnimationEE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation15QAnimationGroup15removeAnimationEPNS_18QAbstractAnimationE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation15QAnimationGroup16staticMetaObjectE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation15QAnimationGroup7setNameERK7QString` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation15QAnimationGroupC2EP7QObject` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation15QChannelMapping11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation15QChannelMapping11qt_metacastEPKc` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation15QChannelMapping11setPropertyERK7QString` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation15QChannelMapping14setChannelNameERK7QString` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation15QChannelMapping16staticMetaObjectE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation15QChannelMapping9setTargetEPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation15QChannelMappingC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation15QChannelMappingD2Ev` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation15QClipBlendValue11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation15QClipBlendValue11qt_metacastEPKc` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation15QClipBlendValue16staticMetaObjectE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation15QClipBlendValue7setClipEPNS_22QAbstractAnimationClipE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation15QClipBlendValueC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation15QClipBlendValueC2EPNS_22QAbstractAnimationClipEPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation15QClipBlendValueD2Ev` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation16QAnimationAspect11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation16QAnimationAspect11qt_metacastEPKc` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation16QAnimationAspect13jobsToExecuteEx` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation16QAnimationAspect16staticMetaObjectE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation16QAnimationAspectC2EP7QObject` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation16QAnimationAspectD2Ev` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation16QCallbackMapping11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation16QCallbackMapping11qt_metacastEPKc` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation16QCallbackMapping11setCallbackEiPNS_18QAnimationCallbackE6QFlagsINS1_4FlagEE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation16QCallbackMapping14setChannelNameERK7QString` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation16QCallbackMapping16staticMetaObjectE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation16QCallbackMappingC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation16QCallbackMappingD2Ev` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation16QSkeletonMapping11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation16QSkeletonMapping11qt_metacastEPKc` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation16QSkeletonMapping11setSkeletonEPN8Qt3DCore17QAbstractSkeletonE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation16QSkeletonMapping16staticMetaObjectE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation16QSkeletonMappingC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation16QSkeletonMappingD2Ev` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation17QChannelComponent14appendKeyFrameERKNS_9QKeyFrameE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation17QChannelComponent14clearKeyFramesEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation17QChannelComponent14insertKeyFrameEiRKNS_9QKeyFrameE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation17QChannelComponent14removeKeyFrameEi` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation17QChannelComponent7setNameERK7QString` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation17QChannelComponentC1ERK7QString` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation17QChannelComponentC1ERKS0_` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation17QChannelComponentC1Ev` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation17QChannelComponentD1Ev` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation17QChannelComponentaSERKS0_` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QAbstractAnimation11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QAbstractAnimation11qt_metacastEPKc` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QAbstractAnimation11setDurationEf` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QAbstractAnimation11setPositionEf` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QAbstractAnimation16setAnimationNameERK7QString` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QAbstractAnimation16staticMetaObjectE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QAdditiveClipBlend11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QAdditiveClipBlend11qt_metacastEPKc` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QAdditiveClipBlend11setBaseClipEPNS_22QAbstractClipBlendNodeE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QAdditiveClipBlend15setAdditiveClipEPNS_22QAbstractClipBlendNodeE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QAdditiveClipBlend16staticMetaObjectE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QAdditiveClipBlend17setAdditiveFactorEf` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QAdditiveClipBlendC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QAdditiveClipBlendD2Ev` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QAnimationCallbackD2Ev` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QAnimationClipData13appendChannelERKNS_8QChannelE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QAnimationClipData13clearChannelsEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QAnimationClipData13insertChannelEiRKNS_8QChannelE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QAnimationClipData13removeChannelEi` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QAnimationClipData7setNameERK7QString` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QAnimationClipDataC1ERKS0_` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QAnimationClipDataC1Ev` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QAnimationClipDataD1Ev` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QAnimationClipDataaSERKS0_` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QKeyframeAnimation10setEndModeENS0_10RepeatModeE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QKeyframeAnimation11addKeyframeEPN8Qt3DCore10QTransformE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QKeyframeAnimation11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QKeyframeAnimation11qt_metacastEPKc` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QKeyframeAnimation12setKeyframesERK5QListIPN8Qt3DCore10QTransformEE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QKeyframeAnimation12setStartModeENS0_10RepeatModeE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QKeyframeAnimation13setTargetNameERK7QString` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QKeyframeAnimation14removeKeyframeEPN8Qt3DCore10QTransformE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QKeyframeAnimation16staticMetaObjectE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QKeyframeAnimation17setFramePositionsERK5QListIfE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QKeyframeAnimation9setEasingERK12QEasingCurve` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QKeyframeAnimation9setTargetEPN8Qt3DCore10QTransformE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QKeyframeAnimationC2EP7QObject` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QMorphingAnimation10getWeightsEi` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QMorphingAnimation10setWeightsEiRK5QListIfE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QMorphingAnimation11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QMorphingAnimation11qt_metacastEPKc` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QMorphingAnimation13setTargetNameE7QString` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QMorphingAnimation14addMorphTargetEPNS_12QMorphTargetE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QMorphingAnimation15morphTargetListEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QMorphingAnimation15setMorphTargetsERK5QListIPNS_12QMorphTargetEE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QMorphingAnimation16staticMetaObjectE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QMorphingAnimation17removeMorphTargetEPNS_12QMorphTargetE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QMorphingAnimation18setTargetPositionsERK5QListIfE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QMorphingAnimation9setEasingERK12QEasingCurve` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QMorphingAnimation9setMethodENS0_6MethodE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QMorphingAnimation9setTargetEPN10Qt3DRender17QGeometryRendererE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation18QMorphingAnimationC2EP7QObject` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation20QAnimationClipLoader11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation20QAnimationClipLoader11qt_metacastEPKc` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation20QAnimationClipLoader16staticMetaObjectE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation20QAnimationClipLoader9setSourceERK4QUrl` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation20QAnimationClipLoaderC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation20QAnimationClipLoaderC2ERK4QUrlPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation20QAnimationClipLoaderD2Ev` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation20QAnimationController11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation20QAnimationController11qt_metacastEPKc` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation20QAnimationController11setPositionEf` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation20QAnimationController12setRecursiveEb` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation20QAnimationController16setPositionScaleEf` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation20QAnimationController16staticMetaObjectE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation20QAnimationController17addAnimationGroupEPNS_15QAnimationGroupE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation20QAnimationController17setPositionOffsetEf` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation20QAnimationController18animationGroupListEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation20QAnimationController18setAnimationGroupsERK5QListIPNS_15QAnimationGroupEE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation20QAnimationController20removeAnimationGroupEPNS_15QAnimationGroupE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation20QAnimationController23setActiveAnimationGroupEi` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation20QAnimationController9setEntityEPN8Qt3DCore7QEntityE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation20QAnimationControllerC2EP7QObject` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation20QBlendedClipAnimator11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation20QBlendedClipAnimator11qt_metacastEPKc` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation20QBlendedClipAnimator12setBlendTreeEPNS_22QAbstractClipBlendNodeE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation20QBlendedClipAnimator16staticMetaObjectE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation20QBlendedClipAnimatorC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation20QBlendedClipAnimatorD2Ev` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation21QAbstractClipAnimator10setRunningEb` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation21QAbstractClipAnimator11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation21QAbstractClipAnimator11qt_metacastEPKc` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation21QAbstractClipAnimator12setLoopCountEi` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation21QAbstractClipAnimator16setChannelMapperEPNS_14QChannelMapperE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation21QAbstractClipAnimator16staticMetaObjectE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation21QAbstractClipAnimator17setNormalizedTimeEf` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation21QAbstractClipAnimator4stopEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation21QAbstractClipAnimator5startEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation21QAbstractClipAnimator8setClockEPNS_6QClockE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation21QAbstractClipAnimatorC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation21QAbstractClipAnimatorD2Ev` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation21QVertexBlendAnimation11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation21QVertexBlendAnimation11qt_metacastEPKc` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation21QVertexBlendAnimation13setTargetNameE7QString` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation21QVertexBlendAnimation14addMorphTargetEPNS_12QMorphTargetE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation21QVertexBlendAnimation15morphTargetListEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation21QVertexBlendAnimation15setMorphTargetsERK5QListIPNS_12QMorphTargetEE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation21QVertexBlendAnimation16staticMetaObjectE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation21QVertexBlendAnimation17removeMorphTargetEPNS_12QMorphTargetE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation21QVertexBlendAnimation18setTargetPositionsERK5QListIfE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation21QVertexBlendAnimation9setTargetEPN10Qt3DRender17QGeometryRendererE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation21QVertexBlendAnimationC2EP7QObject` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation22QAbstractAnimationClip11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation22QAbstractAnimationClip11qt_metacastEPKc` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation22QAbstractAnimationClip16staticMetaObjectE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation22QAbstractAnimationClipD2Ev` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation22QAbstractClipBlendNode11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation22QAbstractClipBlendNode11qt_metacastEPKc` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation22QAbstractClipBlendNode16staticMetaObjectE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation22QAbstractClipBlendNodeC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation22QAbstractClipBlendNodeD2Ev` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation23QAbstractChannelMapping11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation23QAbstractChannelMapping11qt_metacastEPKc` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation23QAbstractChannelMapping16staticMetaObjectE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation23QAbstractChannelMappingD2Ev` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation6QClock11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation6QClock11qt_metacastEPKc` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation6QClock15setPlaybackRateEd` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation6QClock16staticMetaObjectE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation6QClockC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation6QClockD2Ev` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation8QChannel13setJointIndexEi` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation8QChannel22appendChannelComponentERKNS_17QChannelComponentE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation8QChannel22clearChannelComponentsEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation8QChannel22insertChannelComponentEiRKNS_17QChannelComponentE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation8QChannel22removeChannelComponentEi` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation8QChannel7setNameERK7QString` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation8QChannelC1ERK7QString` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation8QChannelC1ERKS0_` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation8QChannelC1Ev` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation8QChannelD1Ev` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimation8QChannelaSERKS0_` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimationeqERKNS_17QChannelComponentES2_` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimationeqERKNS_18QAnimationClipDataES2_` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimationeqERKNS_8QChannelES2_` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZN13Qt3DAnimationneERKNS_18QAnimationClipDataES2_` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation12QMorphTarget10metaObjectEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation12QMorphTarget13attributeListEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation12QMorphTarget14attributeNamesEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation13QClipAnimator10metaObjectEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation13QClipAnimator4clipEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation14QAnimationClip10metaObjectEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation14QAnimationClip8clipDataEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation14QChannelMapper10metaObjectEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation14QChannelMapper8mappingsEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation14QLerpClipBlend10metaObjectEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation14QLerpClipBlend11blendFactorEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation14QLerpClipBlend7endClipEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation14QLerpClipBlend9startClipEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation15QAnimationGroup10metaObjectEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation15QAnimationGroup4nameEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation15QAnimationGroup8durationEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation15QAnimationGroup8positionEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation15QChannelMapping10metaObjectEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation15QChannelMapping11channelNameEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation15QChannelMapping6targetEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation15QChannelMapping8propertyEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation15QClipBlendValue10metaObjectEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation15QClipBlendValue4clipEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation16QAnimationAspect10metaObjectEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation16QCallbackMapping10metaObjectEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation16QCallbackMapping11channelNameEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation16QCallbackMapping8callbackEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation16QSkeletonMapping10metaObjectEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation16QSkeletonMapping8skeletonEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation17QChannelComponent13keyFrameCountEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation17QChannelComponent3endEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation17QChannelComponent4nameEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation17QChannelComponent5beginEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation18QAbstractAnimation10metaObjectEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation18QAbstractAnimation13animationNameEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation18QAbstractAnimation13animationTypeEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation18QAbstractAnimation8durationEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation18QAbstractAnimation8positionEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation18QAdditiveClipBlend10metaObjectEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation18QAdditiveClipBlend12additiveClipEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation18QAdditiveClipBlend14additiveFactorEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation18QAdditiveClipBlend8baseClipEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation18QAnimationClipData12channelCountEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation18QAnimationClipData3endEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation18QAnimationClipData4nameEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation18QAnimationClipData5beginEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation18QAnimationClipData7isValidEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation18QKeyframeAnimation10metaObjectEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation18QKeyframeAnimation10targetNameEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation18QKeyframeAnimation12keyframeListEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation18QKeyframeAnimation14framePositionsEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation18QKeyframeAnimation6easingEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation18QKeyframeAnimation6targetEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation18QKeyframeAnimation7endModeEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation18QKeyframeAnimation9startModeEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation18QMorphingAnimation10metaObjectEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation18QMorphingAnimation10targetNameEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation18QMorphingAnimation12interpolatorEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation18QMorphingAnimation15targetPositionsEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation18QMorphingAnimation6easingEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation18QMorphingAnimation6methodEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation18QMorphingAnimation6targetEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation20QAnimationClipLoader10metaObjectEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation20QAnimationClipLoader6sourceEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation20QAnimationClipLoader6statusEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation20QAnimationController10metaObjectEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation20QAnimationController13positionScaleEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation20QAnimationController14positionOffsetEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation20QAnimationController17getAnimationIndexERK7QString` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation20QAnimationController20activeAnimationGroupEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation20QAnimationController6entityEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation20QAnimationController8getGroupEi` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation20QAnimationController8positionEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation20QAnimationController9recursiveEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation20QBlendedClipAnimator10metaObjectEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation20QBlendedClipAnimator9blendTreeEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation21QAbstractClipAnimator10metaObjectEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation21QAbstractClipAnimator13channelMapperEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation21QAbstractClipAnimator14normalizedTimeEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation21QAbstractClipAnimator5clockEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation21QAbstractClipAnimator9isRunningEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation21QAbstractClipAnimator9loopCountEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation21QVertexBlendAnimation10metaObjectEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation21QVertexBlendAnimation10targetNameEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation21QVertexBlendAnimation12interpolatorEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation21QVertexBlendAnimation15targetPositionsEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation21QVertexBlendAnimation6targetEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation22QAbstractAnimationClip10metaObjectEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation22QAbstractAnimationClip8durationEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation22QAbstractClipBlendNode10metaObjectEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation23QAbstractChannelMapping10metaObjectEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation6QClock10metaObjectEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation6QClock12playbackRateEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation8QChannel10jointIndexEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation8QChannel21channelComponentCountEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation8QChannel3endEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation8QChannel4nameEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZNK13Qt3DAnimation8QChannel5beginEv` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZTIN13Qt3DAnimation12QMorphTargetE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZTIN13Qt3DAnimation13QClipAnimatorE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZTIN13Qt3DAnimation14QAnimationClipE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZTIN13Qt3DAnimation14QChannelMapperE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZTIN13Qt3DAnimation14QLerpClipBlendE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZTIN13Qt3DAnimation15QAnimationGroupE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZTIN13Qt3DAnimation15QChannelMappingE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZTIN13Qt3DAnimation15QClipBlendValueE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZTIN13Qt3DAnimation16QAnimationAspectE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZTIN13Qt3DAnimation16QCallbackMappingE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZTIN13Qt3DAnimation16QSkeletonMappingE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZTIN13Qt3DAnimation18QAbstractAnimationE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZTIN13Qt3DAnimation18QAdditiveClipBlendE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZTIN13Qt3DAnimation18QAnimationCallbackE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZTIN13Qt3DAnimation18QKeyframeAnimationE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZTIN13Qt3DAnimation18QMorphingAnimationE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZTIN13Qt3DAnimation20QAnimationClipLoaderE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZTIN13Qt3DAnimation20QAnimationControllerE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZTIN13Qt3DAnimation20QBlendedClipAnimatorE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZTIN13Qt3DAnimation21QAbstractClipAnimatorE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZTIN13Qt3DAnimation21QVertexBlendAnimationE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZTIN13Qt3DAnimation22QAbstractAnimationClipE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZTIN13Qt3DAnimation22QAbstractClipBlendNodeE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZTIN13Qt3DAnimation23QAbstractChannelMappingE` | 1 | freecad |
| libQt63DAnimation.6.dylib | `__ZTIN13Qt3DAnimation6QClockE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QAspectJob10isRequiredEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QAspectJob9postFrameEPNS_13QAspectEngineE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QAspectJobC2Ev` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QAspectJobD2Ev` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QAttribute10setDivisorEj` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QAttribute11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QAttribute11qt_metacastEPKc` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QAttribute13setByteOffsetEj` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QAttribute13setByteStrideEj` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QAttribute13setVertexSizeEj` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QAttribute16setAttributeTypeENS0_13AttributeTypeE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QAttribute16staticMetaObjectE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QAttribute17setVertexBaseTypeENS0_14VertexBaseTypeE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QAttribute25defaultColorAttributeNameEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QAttribute26defaultNormalAttributeNameEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QAttribute27defaultTangentAttributeNameEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QAttribute28defaultPositionAttributeNameEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QAttribute32defaultJointIndicesAttributeNameEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QAttribute32defaultJointWeightsAttributeNameEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QAttribute37defaultTextureCoordinateAttributeNameEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QAttribute38defaultTextureCoordinate1AttributeNameEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QAttribute38defaultTextureCoordinate2AttributeNameEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QAttribute7setNameERK7QString` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QAttribute8setCountEj` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QAttribute9setBufferEPNS_7QBufferE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QAttributeC2EPNS_5QNodeE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QAttributeC2EPNS_7QBufferENS0_14VertexBaseTypeEjjjjPNS_5QNodeE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QAttributeC2EPNS_7QBufferERK7QStringNS0_14VertexBaseTypeEjjjjPNS_5QNodeE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QAttributeD2Ev` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QComponent11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QComponent11qt_metacastEPKc` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QComponent12setShareableEb` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QComponent16staticMetaObjectE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QComponentC2EPNS_5QNodeE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QComponentD2Ev` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QTransform10setScale3DERK9QVector3D` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QTransform11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QTransform11qt_metacastEPKc` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QTransform11setRotationERK11QQuaternion` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QTransform12rotateAroundERK9QVector3DfS3_` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QTransform12setRotationXEf` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QTransform12setRotationYEf` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QTransform12setRotationZEf` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QTransform14rotateFromAxesERK9QVector3DS3_S3_` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QTransform14setTranslationERK9QVector3D` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QTransform15fromEulerAnglesERK9QVector3D` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QTransform15fromEulerAnglesEfff` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QTransform16fromAxisAndAngleERK9QVector3Df` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QTransform16fromAxisAndAngleEffff` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QTransform16staticMetaObjectE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QTransform17fromAxesAndAnglesERK9QVector3DfS3_f` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QTransform17fromAxesAndAnglesERK9QVector3DfS3_fS3_f` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QTransform8fromAxesERK9QVector3DS3_S3_` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QTransform8setScaleEf` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QTransform9setMatrixERK10QMatrix4x4` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QTransformC2EPNS_5QNodeE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore10QTransformD2Ev` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore11QCoreAspect11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore11QCoreAspect11qt_metacastEPKc` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore11QCoreAspect12onRegisteredEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore11QCoreAspect13jobsToExecuteEx` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore11QCoreAspect14executeCommandERK5QListI7QStringE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore11QCoreAspect14onUnregisteredEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore11QCoreAspect15onEngineStartupEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore11QCoreAspect16staticMetaObjectE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore11QCoreAspect9frameDoneEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore11QCoreAspectC2EP7QObject` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore11QCoreAspectD2Ev` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore12QBackendNode10setEnabledEb` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore12QBackendNode16syncFromFrontEndEPKNS_5QNodeEb` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore12QBackendNodeC2ENS0_4ModeE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore12QBackendNodeD2Ev` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QAspectEngine10setRunModeENS0_7RunModeE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QAspectEngine11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QAspectEngine11qt_metacastEPKc` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QAspectEngine12processFrameEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QAspectEngine13setRootEntityE14QSharedPointerINS_7QEntityEE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QAspectEngine14executeCommandERK7QString` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QAspectEngine14registerAspectEPNS_15QAbstractAspectE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QAspectEngine14registerAspectERK7QString` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QAspectEngine16staticMetaObjectE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QAspectEngine16unregisterAspectEPNS_15QAbstractAspectE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QAspectEngine16unregisterAspectERK7QString` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QAspectEngineC2EP7QObject` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QAspectEngineD2Ev` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QCoreSettings11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QCoreSettings11qt_metacastEPKc` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QCoreSettings16staticMetaObjectE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QCoreSettings25setBoundingVolumesEnabledEb` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QCoreSettingsC2EPNS_5QNodeE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QCoreSettingsD2Ev` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QGeometryView11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QGeometryView11qt_metacastEPKc` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QGeometryView11setGeometryEPNS_9QGeometryE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QGeometryView14setFirstVertexEi` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QGeometryView14setIndexOffsetEi` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QGeometryView14setVertexCountEi` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QGeometryView16setFirstInstanceEi` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QGeometryView16setInstanceCountEi` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QGeometryView16setPrimitiveTypeENS0_13PrimitiveTypeE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QGeometryView16staticMetaObjectE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QGeometryView19setVerticesPerPatchEi` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QGeometryView20setRestartIndexValueEi` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QGeometryView24setIndexBufferByteOffsetEi` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QGeometryView26setPrimitiveRestartEnabledEb` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QGeometryViewC2EPNS_5QNodeE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore13QGeometryViewD2Ev` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QAbstractAspect11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QAbstractAspect11qt_metacastEPKc` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QAbstractAspect12onRegisteredEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QAbstractAspect13jobsToExecuteEx` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QAbstractAspect14executeCommandERK5QListI7QStringE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QAbstractAspect14onUnregisteredEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QAbstractAspect15onEngineStartupEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QAbstractAspect16onEngineShutdownEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QAbstractAspect16staticMetaObjectE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QAbstractAspect19registerBackendTypeERK11QMetaObjectRK14QSharedPointerINS_18QBackendNodeMapperEE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QAbstractAspect21scheduleSingleShotJobERK14QSharedPointerINS_10QAspectJobEE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QAbstractAspect21unregisterBackendTypeERK11QMetaObject` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QAbstractAspect8jobsDoneEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QAbstractAspect9frameDoneEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QAbstractAspectC2EP7QObject` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QAbstractAspectD2Ev` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QBoundingVolume11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QBoundingVolume11qt_metacastEPKc` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QBoundingVolume11setMaxPointERK9QVector3D` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QBoundingVolume11setMinPointERK9QVector3D` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QBoundingVolume16staticMetaObjectE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QBoundingVolume20updateImplicitBoundsEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QBoundingVolume7setViewEPNS_13QGeometryViewE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QBoundingVolumeC2EPNS_5QNodeE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QBoundingVolumeD2Ev` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QSkeletonLoader11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QSkeletonLoader11qt_metacastEPKc` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QSkeletonLoader16staticMetaObjectE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QSkeletonLoader22setCreateJointsEnabledEb` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QSkeletonLoader9setSourceERK4QUrl` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QSkeletonLoaderC2EPNS_5QNodeE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QSkeletonLoaderC2ERK4QUrlPNS_5QNodeE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore15QSkeletonLoaderD2Ev` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore16QAbstractFunctorD2Ev` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore17QAbstractSkeleton11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore17QAbstractSkeleton11qt_metacastEPKc` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore17QAbstractSkeleton16staticMetaObjectE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore17QAbstractSkeletonD2Ev` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore18QBackendNodeMapperD2Ev` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore5QNode10setEnabledEb` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore5QNode11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore5QNode11qt_metacastEPKc` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore5QNode16staticMetaObjectE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore5QNode18blockNotificationsEb` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore5QNode9setParentEPS0_` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore5QNodeC2EPS0_` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore5QNodeD2Ev` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore6QJoint11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore6QJoint11qt_metacastEPKc` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore6QJoint11setRotationERK11QQuaternion` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore6QJoint12setRotationXEf` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore6QJoint12setRotationYEf` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore6QJoint12setRotationZEf` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore6QJoint13addChildJointEPS0_` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore6QJoint13setToIdentityEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore6QJoint14setTranslationERK9QVector3D` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore6QJoint16removeChildJointEPS0_` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore6QJoint16staticMetaObjectE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore6QJoint20setInverseBindMatrixERK10QMatrix4x4` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore6QJoint7setNameERK7QString` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore6QJoint8setScaleERK9QVector3D` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore6QJointC2EPNS_5QNodeE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore6QJointD2Ev` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore7QBuffer10updateDataEiRK10QByteArray` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore7QBuffer11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore7QBuffer11qt_metacastEPKc` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore7QBuffer13setAccessTypeENS0_10AccessTypeE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore7QBuffer16staticMetaObjectE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore7QBuffer7setDataERK10QByteArray` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore7QBuffer8setUsageENS0_9UsageTypeE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore7QBufferC2EPNS_5QNodeE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore7QBufferD2Ev` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore7QEntity11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore7QEntity11qt_metacastEPKc` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore7QEntity12addComponentEPNS_10QComponentE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore7QEntity15removeComponentEPNS_10QComponentE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore7QEntity16staticMetaObjectE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore7QEntityC2EPNS_5QNodeE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore7QEntityD2Ev` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore7QNodeId8createIdEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore9QArmature11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore9QArmature11qt_metacastEPKc` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore9QArmature11setSkeletonEPNS_17QAbstractSkeletonE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore9QArmature16staticMetaObjectE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore9QArmatureC2EPNS_5QNodeE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore9QArmatureD2Ev` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore9QGeometry11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore9QGeometry11qt_metacastEPKc` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore9QGeometry12addAttributeEPNS_10QAttributeE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore9QGeometry15removeAttributeEPNS_10QAttributeE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore9QGeometry16staticMetaObjectE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore9QGeometry34setBoundingVolumePositionAttributeEPNS_10QAttributeE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore9QGeometryC2EPNS_5QNodeE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore9QGeometryD2Ev` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore9QSkeleton11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore9QSkeleton11qt_metacastEPKc` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore9QSkeleton12setRootJointEPNS_6QJointE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore9QSkeleton16staticMetaObjectE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore9QSkeletonC2EPNS_5QNodeE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCore9QSkeletonD2Ev` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZN8Qt3DCorelsE6QDebugNS_7QNodeIdE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore10QAttribute10byteOffsetEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore10QAttribute10byteStrideEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore10QAttribute10metaObjectEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore10QAttribute10vertexSizeEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore10QAttribute13attributeTypeEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore10QAttribute14vertexBaseTypeEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore10QAttribute4nameEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore10QAttribute5countEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore10QAttribute6bufferEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore10QAttribute7divisorEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore10QComponent10metaObjectEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore10QComponent11isShareableEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore10QComponent8entitiesEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore10QTransform10metaObjectEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore10QTransform11translationEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore10QTransform11worldMatrixEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore10QTransform5scaleEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore10QTransform6matrixEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore10QTransform7scale3DEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore10QTransform8rotationEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore10QTransform9rotationXEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore10QTransform9rotationYEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore10QTransform9rotationZEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore11QCoreAspect10metaObjectEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore11QCoreAspect26calculateBoundingVolumeJobEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore12QBackendNode4modeEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore12QBackendNode6peerIdEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore12QBackendNode9isEnabledEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore13QAspectEngine10lookupNodeENS_7QNodeIdE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore13QAspectEngine10metaObjectEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore13QAspectEngine10rootEntityEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore13QAspectEngine11lookupNodesERK5QListINS_7QNodeIdEE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore13QAspectEngine6aspectERK7QString` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore13QAspectEngine7aspectsEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore13QAspectEngine7runModeEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore13QCoreSettings10metaObjectEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore13QCoreSettings22boundingVolumesEnabledEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore13QGeometryView10metaObjectEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore13QGeometryView11firstVertexEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore13QGeometryView11indexOffsetEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore13QGeometryView11vertexCountEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore13QGeometryView13firstInstanceEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore13QGeometryView13instanceCountEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore13QGeometryView13primitiveTypeEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore13QGeometryView16verticesPerPatchEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore13QGeometryView17restartIndexValueEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore13QGeometryView21indexBufferByteOffsetEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore13QGeometryView23primitiveRestartEnabledEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore13QGeometryView8geometryEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore15QAbstractAspect10metaObjectEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore15QAbstractAspect12dependenciesEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore15QAbstractAspect12rootEntityIdEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore15QBoundingVolume10metaObjectEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore15QBoundingVolume16implicitMaxPointEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore15QBoundingVolume16implicitMinPointEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore15QBoundingVolume22areImplicitPointsValidEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore15QBoundingVolume4viewEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore15QBoundingVolume8maxPointEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore15QBoundingVolume8minPointEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore15QSkeletonLoader10metaObjectEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore15QSkeletonLoader21isCreateJointsEnabledEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore15QSkeletonLoader6sourceEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore15QSkeletonLoader6statusEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore15QSkeletonLoader9rootJointEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore17QAbstractSkeleton10jointCountEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore17QAbstractSkeleton10metaObjectEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore5QNode10childNodesEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore5QNode10metaObjectEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore5QNode10parentNodeEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore5QNode20notificationsBlockedEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore5QNode2idEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore5QNode9isEnabledEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore6QJoint10metaObjectEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore6QJoint11childJointsEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore6QJoint11translationEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore6QJoint17inverseBindMatrixEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore6QJoint4nameEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore6QJoint5scaleEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore6QJoint8rotationEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore6QJoint9rotationXEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore6QJoint9rotationYEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore6QJoint9rotationZEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore7QBuffer10accessTypeEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore7QBuffer10metaObjectEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore7QBuffer4dataEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore7QBuffer5usageEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore7QEntity10componentsEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore7QEntity10metaObjectEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore7QEntity12parentEntityEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore9QArmature10metaObjectEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore9QArmature8skeletonEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore9QGeometry10attributesEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore9QGeometry10metaObjectEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore9QGeometry31boundingVolumePositionAttributeEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore9QGeometry9maxExtentEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore9QGeometry9minExtentEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore9QSkeleton10metaObjectEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZNK8Qt3DCore9QSkeleton9rootJointEv` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZTIN8Qt3DCore10QAspectJobE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZTIN8Qt3DCore10QAttributeE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZTIN8Qt3DCore10QComponentE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZTIN8Qt3DCore10QTransformE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZTIN8Qt3DCore11QCoreAspectE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZTIN8Qt3DCore12QBackendNodeE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZTIN8Qt3DCore13QAspectEngineE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZTIN8Qt3DCore13QCoreSettingsE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZTIN8Qt3DCore13QGeometryViewE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZTIN8Qt3DCore15QAbstractAspectE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZTIN8Qt3DCore15QBoundingVolumeE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZTIN8Qt3DCore15QSkeletonLoaderE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZTIN8Qt3DCore16QAbstractFunctorE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZTIN8Qt3DCore17QAbstractSkeletonE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZTIN8Qt3DCore18QBackendNodeMapperE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZTIN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZTIN8Qt3DCore6QJointE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZTIN8Qt3DCore7QBufferE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZTIN8Qt3DCore7QEntityE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZTIN8Qt3DCore9QArmatureE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZTIN8Qt3DCore9QGeometryE` | 1 | freecad |
| libQt63DCore.6.dylib | `__ZTIN8Qt3DCore9QSkeletonE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras10QPlaneMesh11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras10QPlaneMesh11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras10QPlaneMesh11setMirroredEb` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras10QPlaneMesh16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras10QPlaneMesh17setMeshResolutionERK5QSize` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras10QPlaneMesh8setWidthEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras10QPlaneMesh9setHeightEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras10QPlaneMeshC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras10QPlaneMeshD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras10QTorusMesh11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras10QTorusMesh11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras10QTorusMesh14setMinorRadiusEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras10QTorusMesh16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras10QTorusMesh8setRingsEi` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras10QTorusMesh9setRadiusEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras10QTorusMesh9setSlicesEi` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras10QTorusMeshC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras10QTorusMeshD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras10Qt3DWindow11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras10Qt3DWindow11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras10Qt3DWindow11resizeEventEP12QResizeEvent` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras10Qt3DWindow13setRootEntityEPN8Qt3DCore7QEntityE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras10Qt3DWindow14registerAspectEPN8Qt3DCore15QAbstractAspectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras10Qt3DWindow14registerAspectERK7QString` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras10Qt3DWindow16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras10Qt3DWindow19setActiveFrameGraphEPN10Qt3DRender15QFrameGraphNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras10Qt3DWindow5eventEP6QEvent` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras10Qt3DWindow9showEventEP10QShowEvent` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras10Qt3DWindowC2EP7QScreenN10Qt3DRender3APIE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras10Qt3DWindowD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras11QCuboidMesh10setXExtentEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras11QCuboidMesh10setYExtentEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras11QCuboidMesh10setZExtentEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras11QCuboidMesh11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras11QCuboidMesh11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras11QCuboidMesh16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras11QCuboidMesh19setXYMeshResolutionERK5QSize` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras11QCuboidMesh19setXZMeshResolutionERK5QSize` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras11QCuboidMesh19setYZMeshResolutionERK5QSize` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras11QCuboidMeshC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras11QCuboidMeshD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras11QSphereMesh11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras11QSphereMesh11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras11QSphereMesh16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras11QSphereMesh19setGenerateTangentsEb` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras11QSphereMesh8setRingsEi` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras11QSphereMesh9setRadiusEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras11QSphereMesh9setSlicesEi` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras11QSphereMeshC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras11QSphereMeshD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras11QSpriteGrid10setColumnsEi` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras11QSpriteGrid11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras11QSpriteGrid11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras11QSpriteGrid16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras11QSpriteGrid7setRowsEi` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras11QSpriteGridC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras11QSpriteGridD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras12QSpriteSheet10setSpritesE5QListIPNS_16QSpriteSheetItemEE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras12QSpriteSheet11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras12QSpriteSheet11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras12QSpriteSheet12removeSpriteEPNS_16QSpriteSheetItemE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras12QSpriteSheet16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras12QSpriteSheet9addSpriteEPNS_16QSpriteSheetItemE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras12QSpriteSheet9addSpriteEiiii` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras12QSpriteSheetC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras12QSpriteSheetD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QConeGeometry11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QConeGeometry11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QConeGeometry12setTopRadiusEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QConeGeometry13updateIndicesEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QConeGeometry14updateVerticesEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QConeGeometry15setBottomRadiusEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QConeGeometry15setHasTopEndcapEb` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QConeGeometry16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QConeGeometry18setHasBottomEndcapEb` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QConeGeometry8setRingsEi` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QConeGeometry9setLengthEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QConeGeometry9setSlicesEi` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QConeGeometryC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QConeGeometryD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QCylinderMesh11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QCylinderMesh11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QCylinderMesh16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QCylinderMesh8setRingsEi` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QCylinderMesh9setLengthEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QCylinderMesh9setRadiusEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QCylinderMesh9setSlicesEi` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QCylinderMeshC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QCylinderMeshD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QSkyboxEntity11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QSkyboxEntity11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QSkyboxEntity11setBaseNameERK7QString` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QSkyboxEntity12setExtensionERK7QString` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QSkyboxEntity16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QSkyboxEntity22setGammaCorrectEnabledEb` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QSkyboxEntityC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QSkyboxEntityD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QText2DEntity11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QText2DEntity11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QText2DEntity12setAlignmentE6QFlagsIN2Qt13AlignmentFlagEE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QText2DEntity16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QText2DEntity7setFontERK5QFont` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QText2DEntity7setTextERK7QString` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QText2DEntity8setColorERK6QColor` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QText2DEntity8setWidthEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QText2DEntity9setHeightEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QText2DEntityC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras13QText2DEntityD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QGoochMaterial10setDiffuseERK6QColor` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QGoochMaterial11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QGoochMaterial11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QGoochMaterial11setSpecularERK6QColor` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QGoochMaterial12setShininessEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QGoochMaterial16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QGoochMaterial7setBetaEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QGoochMaterial7setCoolERK6QColor` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QGoochMaterial7setWarmERK6QColor` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QGoochMaterial8setAlphaEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QGoochMaterialC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QGoochMaterialD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QPhongMaterial10setAmbientERK6QColor` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QPhongMaterial10setDiffuseERK6QColor` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QPhongMaterial11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QPhongMaterial11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QPhongMaterial11setSpecularERK6QColor` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QPhongMaterial12setShininessEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QPhongMaterial16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QPhongMaterialC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QPhongMaterialD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QPlaneGeometry11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QPlaneGeometry11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QPlaneGeometry11setMirroredEb` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QPlaneGeometry13setResolutionERK5QSize` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QPlaneGeometry13updateIndicesEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QPlaneGeometry14updateVerticesEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QPlaneGeometry16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QPlaneGeometry8setWidthEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QPlaneGeometry9setHeightEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QPlaneGeometryC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QPlaneGeometryD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QTorusGeometry11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QTorusGeometry11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QTorusGeometry13updateIndicesEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QTorusGeometry14setMinorRadiusEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QTorusGeometry14updateVerticesEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QTorusGeometry16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QTorusGeometry8setRingsEi` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QTorusGeometry9setRadiusEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QTorusGeometry9setSlicesEi` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QTorusGeometryC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras14QTorusGeometryD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras15QCuboidGeometry10setXExtentEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras15QCuboidGeometry10setYExtentEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras15QCuboidGeometry10setZExtentEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras15QCuboidGeometry11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras15QCuboidGeometry11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras15QCuboidGeometry13updateIndicesEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras15QCuboidGeometry14updateVerticesEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras15QCuboidGeometry16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras15QCuboidGeometry19setXYMeshResolutionERK5QSize` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras15QCuboidGeometry19setXZMeshResolutionERK5QSize` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras15QCuboidGeometry19setYZMeshResolutionERK5QSize` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras15QCuboidGeometryC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras15QCuboidGeometryD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras15QSphereGeometry11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras15QSphereGeometry11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras15QSphereGeometry13updateIndicesEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras15QSphereGeometry14updateVerticesEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras15QSphereGeometry16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras15QSphereGeometry19setGenerateTangentsEb` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras15QSphereGeometry8setRingsEi` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras15QSphereGeometry9setRadiusEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras15QSphereGeometry9setSlicesEi` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras15QSphereGeometryC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras15QSphereGeometryD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras16QForwardRenderer10setSurfaceEP7QObject` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras16QForwardRenderer11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras16QForwardRenderer11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras16QForwardRenderer13setClearColorERK6QColor` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras16QForwardRenderer15setViewportRectERK6QRectF` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras16QForwardRenderer16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras16QForwardRenderer17setBuffersToClearEN10Qt3DRender13QClearBuffers10BufferTypeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras16QForwardRenderer19setShowDebugOverlayEb` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras16QForwardRenderer24setFrustumCullingEnabledEb` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras16QForwardRenderer27setExternalRenderTargetSizeERK5QSize` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras16QForwardRenderer8setGammaEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras16QForwardRenderer9setCameraEPN8Qt3DCore7QEntityE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras16QForwardRendererC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras16QForwardRendererD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras16QSpriteSheetItem11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras16QSpriteSheetItem11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras16QSpriteSheetItem16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras16QSpriteSheetItem4setXEi` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras16QSpriteSheetItem4setYEi` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras16QSpriteSheetItem8setWidthEi` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras16QSpriteSheetItem9setHeightEi` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras16QSpriteSheetItemC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras16QTextureMaterial10setTextureEPN10Qt3DRender16QAbstractTextureE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras16QTextureMaterial11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras16QTextureMaterial11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras16QTextureMaterial16setTextureOffsetE9QVector2D` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras16QTextureMaterial16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras16QTextureMaterial19setTextureTransformERK14QGenericMatrixILi3ELi3EfE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras16QTextureMaterial23setAlphaBlendingEnabledEb` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras16QTextureMaterialC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras16QTextureMaterialD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras17QConeGeometryView11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras17QConeGeometryView11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras17QConeGeometryView12setTopRadiusEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras17QConeGeometryView15setBottomRadiusEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras17QConeGeometryView15setHasTopEndcapEb` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras17QConeGeometryView16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras17QConeGeometryView18setHasBottomEndcapEb` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras17QConeGeometryView8setRingsEi` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras17QConeGeometryView9setLengthEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras17QConeGeometryView9setSlicesEi` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras17QConeGeometryViewC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras17QConeGeometryViewD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras17QCylinderGeometry11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras17QCylinderGeometry11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras17QCylinderGeometry13updateIndicesEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras17QCylinderGeometry14updateVerticesEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras17QCylinderGeometry16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras17QCylinderGeometry8setRingsEi` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras17QCylinderGeometry9setLengthEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras17QCylinderGeometry9setRadiusEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras17QCylinderGeometry9setSlicesEi` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras17QCylinderGeometryC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras17QCylinderGeometryD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras17QExtrudedTextMesh11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras17QExtrudedTextMesh11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras17QExtrudedTextMesh16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras17QExtrudedTextMesh7setFontERK5QFont` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras17QExtrudedTextMesh7setTextERK7QString` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras17QExtrudedTextMesh8setDepthEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras17QExtrudedTextMeshC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras17QExtrudedTextMeshD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras18QPlaneGeometryView11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras18QPlaneGeometryView11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras18QPlaneGeometryView11setMirroredEb` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras18QPlaneGeometryView16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras18QPlaneGeometryView17setMeshResolutionERK5QSize` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras18QPlaneGeometryView8setWidthEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras18QPlaneGeometryView9setHeightEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras18QPlaneGeometryViewC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras18QPlaneGeometryViewD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras18QTorusGeometryView11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras18QTorusGeometryView11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras18QTorusGeometryView14setMinorRadiusEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras18QTorusGeometryView16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras18QTorusGeometryView8setRingsEi` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras18QTorusGeometryView9setRadiusEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras18QTorusGeometryView9setSlicesEi` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras18QTorusGeometryViewC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras18QTorusGeometryViewD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras18setupWindowSurfaceEP7QWindowN10Qt3DRender3APIE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QCuboidGeometryView10setXExtentEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QCuboidGeometryView10setYExtentEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QCuboidGeometryView10setZExtentEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QCuboidGeometryView11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QCuboidGeometryView11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QCuboidGeometryView16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QCuboidGeometryView19setXYMeshResolutionERK5QSize` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QCuboidGeometryView19setXZMeshResolutionERK5QSize` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QCuboidGeometryView19setYZMeshResolutionERK5QSize` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QCuboidGeometryViewC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QCuboidGeometryViewD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QDiffuseMapMaterial10setAmbientERK6QColor` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QDiffuseMapMaterial10setDiffuseEPN10Qt3DRender16QAbstractTextureE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QDiffuseMapMaterial11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QDiffuseMapMaterial11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QDiffuseMapMaterial11setSpecularERK6QColor` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QDiffuseMapMaterial12setShininessEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QDiffuseMapMaterial15setTextureScaleEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QDiffuseMapMaterial16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QDiffuseMapMaterialC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QDiffuseMapMaterialD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QMetalRoughMaterial11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QMetalRoughMaterial11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QMetalRoughMaterial12setBaseColorERK8QVariant` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QMetalRoughMaterial12setMetalnessERK8QVariant` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QMetalRoughMaterial12setRoughnessERK8QVariant` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QMetalRoughMaterial15setTextureScaleEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QMetalRoughMaterial16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QMetalRoughMaterial19setAmbientOcclusionERK8QVariant` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QMetalRoughMaterial9setNormalERK8QVariant` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QMetalRoughMaterialC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QMetalRoughMaterialD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QMorphPhongMaterial10setAmbientERK6QColor` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QMorphPhongMaterial10setDiffuseERK6QColor` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QMorphPhongMaterial11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QMorphPhongMaterial11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QMorphPhongMaterial11setSpecularERK6QColor` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QMorphPhongMaterial12setShininessEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QMorphPhongMaterial15setInterpolatorEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QMorphPhongMaterial16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QMorphPhongMaterialC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QMorphPhongMaterialD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QPhongAlphaMaterial10setAmbientERK6QColor` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QPhongAlphaMaterial10setDiffuseERK6QColor` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QPhongAlphaMaterial11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QPhongAlphaMaterial11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QPhongAlphaMaterial11setSpecularERK6QColor` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QPhongAlphaMaterial12setShininessEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QPhongAlphaMaterial15setSourceRgbArgEN10Qt3DRender23QBlendEquationArguments8BlendingE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QPhongAlphaMaterial16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QPhongAlphaMaterial17setSourceAlphaArgEN10Qt3DRender23QBlendEquationArguments8BlendingE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QPhongAlphaMaterial19setBlendFunctionArgEN10Qt3DRender14QBlendEquation13BlendFunctionE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QPhongAlphaMaterial20setDestinationRgbArgEN10Qt3DRender23QBlendEquationArguments8BlendingE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QPhongAlphaMaterial22setDestinationAlphaArgEN10Qt3DRender23QBlendEquationArguments8BlendingE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QPhongAlphaMaterial8setAlphaEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QPhongAlphaMaterialC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QPhongAlphaMaterialD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QSphereGeometryView11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QSphereGeometryView11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QSphereGeometryView16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QSphereGeometryView19setGenerateTangentsEb` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QSphereGeometryView8setRingsEi` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QSphereGeometryView9setRadiusEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QSphereGeometryView9setSlicesEi` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QSphereGeometryViewC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras19QSphereGeometryViewD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras20QAbstractSpriteSheet10setTextureEPN10Qt3DRender16QAbstractTextureE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras20QAbstractSpriteSheet11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras20QAbstractSpriteSheet11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras20QAbstractSpriteSheet15setCurrentIndexEi` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras20QAbstractSpriteSheet16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras20QAbstractSpriteSheetD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras21QCylinderGeometryView11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras21QCylinderGeometryView11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras21QCylinderGeometryView16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras21QCylinderGeometryView8setRingsEi` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras21QCylinderGeometryView9setLengthEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras21QCylinderGeometryView9setRadiusEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras21QCylinderGeometryView9setSlicesEi` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras21QCylinderGeometryViewC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras21QCylinderGeometryViewD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras21QExtrudedTextGeometry11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras21QExtrudedTextGeometry11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras21QExtrudedTextGeometry16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras21QExtrudedTextGeometry7setFontERK5QFont` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras21QExtrudedTextGeometry7setTextERK7QString` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras21QExtrudedTextGeometry8setDepthEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras21QExtrudedTextGeometryC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras21QExtrudedTextGeometryD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras22QOrbitCameraController10moveCameraERKNS_25QAbstractCameraController10InputStateEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras22QOrbitCameraController11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras22QOrbitCameraController11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras22QOrbitCameraController11setUpVectorERK9QVector3D` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras22QOrbitCameraController13setInversePanEb` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras22QOrbitCameraController14setInverseTiltEb` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras22QOrbitCameraController14setZoomInLimitEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras22QOrbitCameraController16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras22QOrbitCameraController20setInverseXTranslateEb` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras22QOrbitCameraController20setInverseYTranslateEb` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras22QOrbitCameraController26setZoomTranslateViewCenterEb` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras22QOrbitCameraControllerC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras22QOrbitCameraControllerD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras23QPerVertexColorMaterial11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras23QPerVertexColorMaterial11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras23QPerVertexColorMaterial16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras23QPerVertexColorMaterialC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras23QPerVertexColorMaterialD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras24QDiffuseSpecularMaterial10setAmbientERK6QColor` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras24QDiffuseSpecularMaterial10setDiffuseERK8QVariant` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras24QDiffuseSpecularMaterial11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras24QDiffuseSpecularMaterial11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras24QDiffuseSpecularMaterial11setSpecularERK8QVariant` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras24QDiffuseSpecularMaterial12setShininessEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras24QDiffuseSpecularMaterial15setTextureScaleEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras24QDiffuseSpecularMaterial16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras24QDiffuseSpecularMaterial23setAlphaBlendingEnabledEb` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras24QDiffuseSpecularMaterial9setNormalERK8QVariant` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras24QDiffuseSpecularMaterialC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras24QDiffuseSpecularMaterialD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras25QAbstractCameraController11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras25QAbstractCameraController11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras25QAbstractCameraController12setLookSpeedEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras25QAbstractCameraController14setLinearSpeedEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras25QAbstractCameraController15setAccelerationEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras25QAbstractCameraController15setDecelerationEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras25QAbstractCameraController16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras25QAbstractCameraController9setCameraEPN10Qt3DRender7QCameraE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras25QAbstractCameraControllerC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras25QAbstractCameraControllerD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras25QNormalDiffuseMapMaterial10setAmbientERK6QColor` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras25QNormalDiffuseMapMaterial10setDiffuseEPN10Qt3DRender16QAbstractTextureE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras25QNormalDiffuseMapMaterial11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras25QNormalDiffuseMapMaterial11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras25QNormalDiffuseMapMaterial11setSpecularERK6QColor` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras25QNormalDiffuseMapMaterial12setShininessEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras25QNormalDiffuseMapMaterial15setTextureScaleEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras25QNormalDiffuseMapMaterial16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras25QNormalDiffuseMapMaterial9setNormalEPN10Qt3DRender16QAbstractTextureE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras25QNormalDiffuseMapMaterialC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras25QNormalDiffuseMapMaterialD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras27QDiffuseSpecularMapMaterial10setAmbientERK6QColor` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras27QDiffuseSpecularMapMaterial10setDiffuseEPN10Qt3DRender16QAbstractTextureE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras27QDiffuseSpecularMapMaterial11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras27QDiffuseSpecularMapMaterial11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras27QDiffuseSpecularMapMaterial11setSpecularEPN10Qt3DRender16QAbstractTextureE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras27QDiffuseSpecularMapMaterial12setShininessEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras27QDiffuseSpecularMapMaterial15setTextureScaleEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras27QDiffuseSpecularMapMaterial16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras27QDiffuseSpecularMapMaterialC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras27QDiffuseSpecularMapMaterialD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras28QFirstPersonCameraController10moveCameraERKNS_25QAbstractCameraController10InputStateEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras28QFirstPersonCameraController11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras28QFirstPersonCameraController11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras28QFirstPersonCameraController16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras28QFirstPersonCameraControllerC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras28QFirstPersonCameraControllerD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras30QNormalDiffuseMapAlphaMaterial11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras30QNormalDiffuseMapAlphaMaterial11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras30QNormalDiffuseMapAlphaMaterial16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras30QNormalDiffuseMapAlphaMaterialC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras30QNormalDiffuseMapAlphaMaterialD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras33QNormalDiffuseSpecularMapMaterial10setAmbientERK6QColor` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras33QNormalDiffuseSpecularMapMaterial10setDiffuseEPN10Qt3DRender16QAbstractTextureE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras33QNormalDiffuseSpecularMapMaterial11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras33QNormalDiffuseSpecularMapMaterial11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras33QNormalDiffuseSpecularMapMaterial11setSpecularEPN10Qt3DRender16QAbstractTextureE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras33QNormalDiffuseSpecularMapMaterial12setShininessEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras33QNormalDiffuseSpecularMapMaterial15setTextureScaleEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras33QNormalDiffuseSpecularMapMaterial16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras33QNormalDiffuseSpecularMapMaterial9setNormalEPN10Qt3DRender16QAbstractTextureE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras33QNormalDiffuseSpecularMapMaterialC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras33QNormalDiffuseSpecularMapMaterialD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras9QConeMesh11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras9QConeMesh11qt_metacastEPKc` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras9QConeMesh12setTopRadiusEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras9QConeMesh15setBottomRadiusEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras9QConeMesh15setHasTopEndcapEb` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras9QConeMesh16staticMetaObjectE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras9QConeMesh18setHasBottomEndcapEb` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras9QConeMesh8setRingsEi` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras9QConeMesh9setLengthEf` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras9QConeMesh9setSlicesEi` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras9QConeMeshC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZN10Qt3DExtras9QConeMeshD2Ev` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras10QPlaneMesh10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras10QPlaneMesh14meshResolutionEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras10QPlaneMesh5widthEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras10QPlaneMesh6heightEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras10QPlaneMesh8mirroredEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras10QTorusMesh10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras10QTorusMesh11minorRadiusEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras10QTorusMesh5ringsEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras10QTorusMesh6radiusEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras10QTorusMesh6slicesEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras10Qt3DWindow10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras10Qt3DWindow14renderSettingsEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras10Qt3DWindow16activeFrameGraphEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras10Qt3DWindow17defaultFrameGraphEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras10Qt3DWindow6cameraEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras11QCuboidMesh10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras11QCuboidMesh16xyMeshResolutionEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras11QCuboidMesh16xzMeshResolutionEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras11QCuboidMesh16yzMeshResolutionEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras11QCuboidMesh7xExtentEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras11QCuboidMesh7yExtentEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras11QCuboidMesh7zExtentEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras11QSphereMesh10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras11QSphereMesh16generateTangentsEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras11QSphereMesh5ringsEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras11QSphereMesh6radiusEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras11QSphereMesh6slicesEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras11QSpriteGrid10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras11QSpriteGrid4rowsEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras11QSpriteGrid7columnsEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras12QSpriteSheet10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras12QSpriteSheet7spritesEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras13QConeGeometry10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras13QConeGeometry12bottomRadiusEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras13QConeGeometry12hasTopEndcapEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras13QConeGeometry14indexAttributeEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras13QConeGeometry15hasBottomEndcapEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras13QConeGeometry15normalAttributeEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras13QConeGeometry17positionAttributeEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras13QConeGeometry17texCoordAttributeEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras13QConeGeometry5ringsEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras13QConeGeometry6lengthEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras13QConeGeometry6slicesEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras13QConeGeometry9topRadiusEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras13QCylinderMesh10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras13QCylinderMesh5ringsEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras13QCylinderMesh6lengthEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras13QCylinderMesh6radiusEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras13QCylinderMesh6slicesEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras13QSkyboxEntity10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras13QSkyboxEntity21isGammaCorrectEnabledEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras13QSkyboxEntity8baseNameEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras13QSkyboxEntity9extensionEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras13QText2DEntity10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras13QText2DEntity4fontEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras13QText2DEntity4textEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras13QText2DEntity5colorEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras13QText2DEntity5widthEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras13QText2DEntity6heightEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras13QText2DEntity9alignmentEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras14QGoochMaterial10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras14QGoochMaterial4betaEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras14QGoochMaterial4coolEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras14QGoochMaterial4warmEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras14QGoochMaterial5alphaEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras14QGoochMaterial7diffuseEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras14QGoochMaterial8specularEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras14QGoochMaterial9shininessEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras14QPhongMaterial10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras14QPhongMaterial7ambientEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras14QPhongMaterial7diffuseEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras14QPhongMaterial8specularEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras14QPhongMaterial9shininessEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras14QPlaneGeometry10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras14QPlaneGeometry10resolutionEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras14QPlaneGeometry14indexAttributeEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras14QPlaneGeometry15normalAttributeEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras14QPlaneGeometry16tangentAttributeEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras14QPlaneGeometry17positionAttributeEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras14QPlaneGeometry17texCoordAttributeEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras14QPlaneGeometry5widthEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras14QPlaneGeometry6heightEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras14QPlaneGeometry8mirroredEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras14QTorusGeometry10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras14QTorusGeometry11minorRadiusEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras14QTorusGeometry14indexAttributeEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras14QTorusGeometry15normalAttributeEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras14QTorusGeometry17positionAttributeEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras14QTorusGeometry17texCoordAttributeEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras14QTorusGeometry5ringsEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras14QTorusGeometry6radiusEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras14QTorusGeometry6slicesEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras15QCuboidGeometry10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras15QCuboidGeometry14indexAttributeEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras15QCuboidGeometry15normalAttributeEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras15QCuboidGeometry16tangentAttributeEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras15QCuboidGeometry16xyMeshResolutionEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras15QCuboidGeometry16xzMeshResolutionEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras15QCuboidGeometry16yzMeshResolutionEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras15QCuboidGeometry17positionAttributeEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras15QCuboidGeometry17texCoordAttributeEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras15QCuboidGeometry7xExtentEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras15QCuboidGeometry7yExtentEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras15QCuboidGeometry7zExtentEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras15QSphereGeometry10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras15QSphereGeometry14indexAttributeEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras15QSphereGeometry15normalAttributeEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras15QSphereGeometry16generateTangentsEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras15QSphereGeometry16tangentAttributeEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras15QSphereGeometry17positionAttributeEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras15QSphereGeometry17texCoordAttributeEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras15QSphereGeometry5ringsEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras15QSphereGeometry6radiusEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras15QSphereGeometry6slicesEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras16QForwardRenderer10clearColorEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras16QForwardRenderer10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras16QForwardRenderer12viewportRectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras16QForwardRenderer14buffersToClearEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras16QForwardRenderer16showDebugOverlayEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras16QForwardRenderer23isFrustumCullingEnabledEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras16QForwardRenderer24externalRenderTargetSizeEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras16QForwardRenderer5gammaEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras16QForwardRenderer6cameraEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras16QForwardRenderer7surfaceEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras16QSpriteSheetItem10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras16QSpriteSheetItem1xEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras16QSpriteSheetItem1yEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras16QSpriteSheetItem5widthEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras16QSpriteSheetItem6heightEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras16QTextureMaterial10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras16QTextureMaterial13textureOffsetEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras16QTextureMaterial16textureTransformEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras16QTextureMaterial22isAlphaBlendingEnabledEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras16QTextureMaterial7textureEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras17QConeGeometryView10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras17QConeGeometryView12bottomRadiusEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras17QConeGeometryView12hasTopEndcapEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras17QConeGeometryView15hasBottomEndcapEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras17QConeGeometryView5ringsEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras17QConeGeometryView6lengthEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras17QConeGeometryView6slicesEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras17QConeGeometryView9topRadiusEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras17QCylinderGeometry10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras17QCylinderGeometry14indexAttributeEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras17QCylinderGeometry15normalAttributeEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras17QCylinderGeometry17positionAttributeEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras17QCylinderGeometry17texCoordAttributeEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras17QCylinderGeometry5ringsEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras17QCylinderGeometry6lengthEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras17QCylinderGeometry6radiusEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras17QCylinderGeometry6slicesEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras17QExtrudedTextMesh10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras17QExtrudedTextMesh4fontEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras17QExtrudedTextMesh4textEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras17QExtrudedTextMesh5depthEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras18QPlaneGeometryView10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras18QPlaneGeometryView14meshResolutionEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras18QPlaneGeometryView5widthEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras18QPlaneGeometryView6heightEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras18QPlaneGeometryView8mirroredEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras18QTorusGeometryView10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras18QTorusGeometryView11minorRadiusEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras18QTorusGeometryView5ringsEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras18QTorusGeometryView6radiusEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras18QTorusGeometryView6slicesEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QCuboidGeometryView10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QCuboidGeometryView16xyMeshResolutionEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QCuboidGeometryView16xzMeshResolutionEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QCuboidGeometryView16yzMeshResolutionEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QCuboidGeometryView7xExtentEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QCuboidGeometryView7yExtentEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QCuboidGeometryView7zExtentEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QDiffuseMapMaterial10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QDiffuseMapMaterial12textureScaleEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QDiffuseMapMaterial7ambientEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QDiffuseMapMaterial7diffuseEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QDiffuseMapMaterial8specularEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QDiffuseMapMaterial9shininessEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QMetalRoughMaterial10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QMetalRoughMaterial12textureScaleEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QMetalRoughMaterial16ambientOcclusionEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QMetalRoughMaterial6normalEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QMetalRoughMaterial9baseColorEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QMetalRoughMaterial9metalnessEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QMetalRoughMaterial9roughnessEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QMorphPhongMaterial10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QMorphPhongMaterial12interpolatorEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QMorphPhongMaterial7ambientEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QMorphPhongMaterial7diffuseEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QMorphPhongMaterial8specularEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QMorphPhongMaterial9shininessEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QPhongAlphaMaterial10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QPhongAlphaMaterial12sourceRgbArgEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QPhongAlphaMaterial14sourceAlphaArgEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QPhongAlphaMaterial16blendFunctionArgEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QPhongAlphaMaterial17destinationRgbArgEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QPhongAlphaMaterial19destinationAlphaArgEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QPhongAlphaMaterial5alphaEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QPhongAlphaMaterial7ambientEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QPhongAlphaMaterial7diffuseEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QPhongAlphaMaterial8specularEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QPhongAlphaMaterial9shininessEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QSphereGeometryView10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QSphereGeometryView16generateTangentsEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QSphereGeometryView5ringsEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QSphereGeometryView6radiusEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras19QSphereGeometryView6slicesEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras20QAbstractSpriteSheet10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras20QAbstractSpriteSheet12currentIndexEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras20QAbstractSpriteSheet16textureTransformEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras20QAbstractSpriteSheet7textureEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras21QCylinderGeometryView10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras21QCylinderGeometryView5ringsEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras21QCylinderGeometryView6lengthEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras21QCylinderGeometryView6radiusEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras21QCylinderGeometryView6slicesEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras21QExtrudedTextGeometry10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras21QExtrudedTextGeometry14indexAttributeEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras21QExtrudedTextGeometry15extrusionLengthEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras21QExtrudedTextGeometry15normalAttributeEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras21QExtrudedTextGeometry17positionAttributeEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras21QExtrudedTextGeometry4fontEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras21QExtrudedTextGeometry4textEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras22QOrbitCameraController10inversePanEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras22QOrbitCameraController10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras22QOrbitCameraController11inverseTiltEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras22QOrbitCameraController11zoomInLimitEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras22QOrbitCameraController17inverseXTranslateEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras22QOrbitCameraController17inverseYTranslateEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras22QOrbitCameraController23zoomTranslateViewCenterEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras22QOrbitCameraController8upVectorEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras23QPerVertexColorMaterial10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras24QDiffuseSpecularMaterial10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras24QDiffuseSpecularMaterial12textureScaleEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras24QDiffuseSpecularMaterial22isAlphaBlendingEnabledEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras24QDiffuseSpecularMaterial6normalEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras24QDiffuseSpecularMaterial7ambientEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras24QDiffuseSpecularMaterial7diffuseEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras24QDiffuseSpecularMaterial8specularEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras24QDiffuseSpecularMaterial9shininessEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras25QAbstractCameraController10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras25QAbstractCameraController11linearSpeedEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras25QAbstractCameraController12accelerationEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras25QAbstractCameraController12decelerationEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras25QAbstractCameraController6cameraEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras25QAbstractCameraController9lookSpeedEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras25QNormalDiffuseMapMaterial10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras25QNormalDiffuseMapMaterial12textureScaleEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras25QNormalDiffuseMapMaterial6normalEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras25QNormalDiffuseMapMaterial7ambientEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras25QNormalDiffuseMapMaterial7diffuseEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras25QNormalDiffuseMapMaterial8specularEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras25QNormalDiffuseMapMaterial9shininessEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras27QDiffuseSpecularMapMaterial10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras27QDiffuseSpecularMapMaterial12textureScaleEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras27QDiffuseSpecularMapMaterial7ambientEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras27QDiffuseSpecularMapMaterial7diffuseEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras27QDiffuseSpecularMapMaterial8specularEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras27QDiffuseSpecularMapMaterial9shininessEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras28QFirstPersonCameraController10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras30QNormalDiffuseMapAlphaMaterial10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras33QNormalDiffuseSpecularMapMaterial10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras33QNormalDiffuseSpecularMapMaterial12textureScaleEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras33QNormalDiffuseSpecularMapMaterial6normalEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras33QNormalDiffuseSpecularMapMaterial7ambientEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras33QNormalDiffuseSpecularMapMaterial7diffuseEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras33QNormalDiffuseSpecularMapMaterial8specularEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras33QNormalDiffuseSpecularMapMaterial9shininessEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras9QConeMesh10metaObjectEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras9QConeMesh12bottomRadiusEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras9QConeMesh12hasTopEndcapEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras9QConeMesh15hasBottomEndcapEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras9QConeMesh5ringsEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras9QConeMesh6lengthEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras9QConeMesh6slicesEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZNK10Qt3DExtras9QConeMesh9topRadiusEv` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras10QPlaneMeshE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras10QTorusMeshE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras10Qt3DWindowE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras11QCuboidMeshE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras11QSphereMeshE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras11QSpriteGridE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras12QSpriteSheetE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras13QConeGeometryE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras13QCylinderMeshE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras13QSkyboxEntityE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras13QText2DEntityE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras14QGoochMaterialE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras14QPhongMaterialE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras14QPlaneGeometryE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras14QTorusGeometryE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras15QCuboidGeometryE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras15QSphereGeometryE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras16QForwardRendererE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras16QSpriteSheetItemE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras16QTextureMaterialE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras17QConeGeometryViewE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras17QCylinderGeometryE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras17QExtrudedTextMeshE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras18QPlaneGeometryViewE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras18QTorusGeometryViewE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras19QCuboidGeometryViewE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras19QDiffuseMapMaterialE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras19QMetalRoughMaterialE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras19QMorphPhongMaterialE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras19QPhongAlphaMaterialE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras19QSphereGeometryViewE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras20QAbstractSpriteSheetE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras21QCylinderGeometryViewE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras21QExtrudedTextGeometryE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras22QOrbitCameraControllerE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras23QPerVertexColorMaterialE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras24QDiffuseSpecularMaterialE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras25QAbstractCameraControllerE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras25QNormalDiffuseMapMaterialE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras27QDiffuseSpecularMapMaterialE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras28QFirstPersonCameraControllerE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras30QNormalDiffuseMapAlphaMaterialE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras33QNormalDiffuseSpecularMapMaterialE` | 1 | freecad |
| libQt63DExtras.6.dylib | `__ZTIN10Qt3DExtras9QConeMeshE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput11QInputChord10setTimeoutEi` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput11QInputChord11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput11QInputChord11qt_metacastEPKc` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput11QInputChord11removeChordEPNS_20QAbstractActionInputE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput11QInputChord16staticMetaObjectE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput11QInputChord8addChordEPNS_20QAbstractActionInputE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput11QInputChordC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput11QInputChordD2Ev` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput11QMouseEvent11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput11QMouseEvent11qt_metacastEPKc` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput11QMouseEvent16staticMetaObjectE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput11QMouseEventC2ERK11QMouseEvent` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput11QMouseEventD2Ev` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput11QWheelEvent11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput11QWheelEvent11qt_metacastEPKc` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput11QWheelEvent16staticMetaObjectE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput11QWheelEventC2ERK11QWheelEvent` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput11QWheelEventD2Ev` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QActionInput10setButtonsERK5QListIiE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QActionInput11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QActionInput11qt_metacastEPKc` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QActionInput15setSourceDeviceEPNS_23QAbstractPhysicalDeviceE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QActionInput16staticMetaObjectE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QActionInputC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QActionInputD2Ev` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QAxisSetting11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QAxisSetting11qt_metacastEPKc` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QAxisSetting16setSmoothEnabledEb` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QAxisSetting16staticMetaObjectE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QAxisSetting17setDeadZoneRadiusEf` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QAxisSetting7setAxesERK5QListIiE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QAxisSettingC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QAxisSettingD2Ev` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QInputAspect11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QInputAspect11qt_metacastEPKc` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QInputAspect12onRegisteredEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QInputAspect13jobsToExecuteEx` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QInputAspect14onUnregisteredEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QInputAspect15onEngineStartupEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QInputAspect16staticMetaObjectE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QInputAspect20createPhysicalDeviceERK7QString` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QInputAspect8jobsDoneEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QInputAspectC2EP7QObject` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QInputAspectD2Ev` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QMouseDevice11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QMouseDevice11qt_metacastEPKc` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QMouseDevice14setSensitivityEf` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QMouseDevice16staticMetaObjectE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QMouseDevice25setUpdateAxesContinuouslyEb` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QMouseDeviceC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput12QMouseDeviceD2Ev` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput13QMouseHandler11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput13QMouseHandler11qt_metacastEPKc` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput13QMouseHandler15setSourceDeviceEPNS_12QMouseDeviceE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput13QMouseHandler16setContainsMouseEb` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput13QMouseHandler16staticMetaObjectE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput13QMouseHandlerC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput13QMouseHandlerD2Ev` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput14QInputSequence10setTimeoutEi` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput14QInputSequence11addSequenceEPNS_20QAbstractActionInputE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput14QInputSequence11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput14QInputSequence11qt_metacastEPKc` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput14QInputSequence14removeSequenceEPNS_20QAbstractActionInputE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput14QInputSequence16staticMetaObjectE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput14QInputSequence17setButtonIntervalEi` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput14QInputSequenceC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput14QInputSequenceD2Ev` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput14QInputSettings11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput14QInputSettings11qt_metacastEPKc` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput14QInputSettings14setEventSourceEP7QObject` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput14QInputSettings16staticMetaObjectE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput14QInputSettingsC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput14QInputSettingsD2Ev` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput14QLogicalDevice10removeAxisEPNS_5QAxisE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput14QLogicalDevice11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput14QLogicalDevice11qt_metacastEPKc` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput14QLogicalDevice12removeActionEPNS_7QActionE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput14QLogicalDevice16staticMetaObjectE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput14QLogicalDevice7addAxisEPNS_5QAxisE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput14QLogicalDevice9addActionEPNS_7QActionE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput14QLogicalDeviceC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput14QLogicalDeviceD2Ev` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput15QKeyboardDevice11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput15QKeyboardDevice11qt_metacastEPKc` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput15QKeyboardDevice16staticMetaObjectE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput15QKeyboardDeviceC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput15QKeyboardDeviceD2Ev` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput16QAnalogAxisInput11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput16QAnalogAxisInput11qt_metacastEPKc` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput16QAnalogAxisInput16staticMetaObjectE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput16QAnalogAxisInput7setAxisEi` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput16QAnalogAxisInputC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput16QAnalogAxisInputD2Ev` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput16QAxisAccumulator11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput16QAxisAccumulator11qt_metacastEPKc` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput16QAxisAccumulator13setSourceAxisEPNS_5QAxisE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput16QAxisAccumulator16staticMetaObjectE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput16QAxisAccumulator17setSourceAxisTypeENS0_14SourceAxisTypeE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput16QAxisAccumulator8setScaleEf` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput16QAxisAccumulatorC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput16QAxisAccumulatorD2Ev` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput16QButtonAxisInput10setButtonsERK5QListIiE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput16QButtonAxisInput11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput16QButtonAxisInput11qt_metacastEPKc` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput16QButtonAxisInput15setAccelerationEf` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput16QButtonAxisInput15setDecelerationEf` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput16QButtonAxisInput16staticMetaObjectE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput16QButtonAxisInput8setScaleEf` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput16QButtonAxisInputC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput16QButtonAxisInputD2Ev` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput16QKeyboardHandler11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput16QKeyboardHandler11qt_metacastEPKc` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput16QKeyboardHandler15setSourceDeviceEPNS_15QKeyboardDeviceE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput16QKeyboardHandler16staticMetaObjectE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput16QKeyboardHandler8setFocusEb` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput16QKeyboardHandlerC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput16QKeyboardHandlerD2Ev` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput18QAbstractAxisInput11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput18QAbstractAxisInput11qt_metacastEPKc` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput18QAbstractAxisInput15setSourceDeviceEPNS_23QAbstractPhysicalDeviceE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput18QAbstractAxisInput16staticMetaObjectE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput18QAbstractAxisInputD2Ev` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput20QAbstractActionInput11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput20QAbstractActionInput11qt_metacastEPKc` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput20QAbstractActionInput16staticMetaObjectE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput20QAbstractActionInputD2Ev` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput23QAbstractPhysicalDevice11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput23QAbstractPhysicalDevice11qt_metacastEPKc` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput23QAbstractPhysicalDevice14addAxisSettingEPNS_12QAxisSettingE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput23QAbstractPhysicalDevice16staticMetaObjectE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput23QAbstractPhysicalDevice17removeAxisSettingEPNS_12QAxisSettingE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput23QAbstractPhysicalDeviceC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput23QAbstractPhysicalDeviceD2Ev` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput5QAxis11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput5QAxis11qt_metacastEPKc` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput5QAxis11removeInputEPNS_18QAbstractAxisInputE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput5QAxis16staticMetaObjectE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput5QAxis8addInputEPNS_18QAbstractAxisInputE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput5QAxisC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput5QAxisD2Ev` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput7QAction11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput7QAction11qt_metacastEPKc` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput7QAction11removeInputEPNS_20QAbstractActionInputE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput7QAction16staticMetaObjectE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput7QAction8addInputEPNS_20QAbstractActionInputE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput7QActionC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput7QActionD2Ev` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput9QKeyEvent11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput9QKeyEvent11qt_metacastEPKc` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput9QKeyEvent16staticMetaObjectE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput9QKeyEventC2EN6QEvent4TypeEi6QFlagsIN2Qt16KeyboardModifierEERK7QStringbt` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput9QKeyEventC2ERK9QKeyEvent` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZN9Qt3DInput9QKeyEventD2Ev` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput11QInputChord10metaObjectEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput11QInputChord6chordsEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput11QInputChord7timeoutEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput11QMouseEvent10metaObjectEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput11QMouseEvent6buttonEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput11QMouseEvent7buttonsEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput11QMouseEvent9modifiersEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput11QWheelEvent10metaObjectEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput11QWheelEvent7buttonsEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput11QWheelEvent9modifiersEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput12QActionInput10metaObjectEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput12QActionInput12sourceDeviceEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput12QActionInput7buttonsEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput12QAxisSetting10metaObjectEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput12QAxisSetting14deadZoneRadiusEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput12QAxisSetting15isSmoothEnabledEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput12QAxisSetting4axesEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput12QInputAspect10metaObjectEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput12QInputAspect24availablePhysicalDevicesEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput12QMouseDevice10metaObjectEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput12QMouseDevice11buttonCountEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput12QMouseDevice11buttonNamesEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput12QMouseDevice11sensitivityEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput12QMouseDevice14axisIdentifierERK7QString` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput12QMouseDevice16buttonIdentifierERK7QString` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput12QMouseDevice22updateAxesContinuouslyEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput12QMouseDevice9axisCountEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput12QMouseDevice9axisNamesEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput13QMouseHandler10metaObjectEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput13QMouseHandler12sourceDeviceEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput13QMouseHandler13containsMouseEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput14QInputSequence10metaObjectEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput14QInputSequence14buttonIntervalEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput14QInputSequence7timeoutEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput14QInputSequence9sequencesEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput14QInputSettings10metaObjectEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput14QInputSettings11eventSourceEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput14QLogicalDevice10metaObjectEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput14QLogicalDevice4axesEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput14QLogicalDevice7actionsEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput15QKeyboardDevice10metaObjectEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput15QKeyboardDevice11activeInputEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput15QKeyboardDevice11buttonCountEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput15QKeyboardDevice11buttonNamesEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput15QKeyboardDevice14axisIdentifierERK7QString` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput15QKeyboardDevice16buttonIdentifierERK7QString` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput15QKeyboardDevice9axisCountEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput15QKeyboardDevice9axisNamesEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput16QAnalogAxisInput10metaObjectEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput16QAnalogAxisInput4axisEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput16QAxisAccumulator10metaObjectEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput16QAxisAccumulator10sourceAxisEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput16QAxisAccumulator14sourceAxisTypeEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput16QAxisAccumulator5scaleEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput16QAxisAccumulator5valueEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput16QAxisAccumulator8velocityEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput16QButtonAxisInput10metaObjectEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput16QButtonAxisInput12accelerationEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput16QButtonAxisInput12decelerationEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput16QButtonAxisInput5scaleEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput16QButtonAxisInput7buttonsEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput16QKeyboardHandler10metaObjectEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput16QKeyboardHandler12sourceDeviceEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput16QKeyboardHandler5focusEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput18QAbstractAxisInput10metaObjectEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput18QAbstractAxisInput12sourceDeviceEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput20QAbstractActionInput10metaObjectEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput23QAbstractPhysicalDevice10metaObjectEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput23QAbstractPhysicalDevice11buttonCountEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput23QAbstractPhysicalDevice11buttonNamesEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput23QAbstractPhysicalDevice12axisSettingsEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput23QAbstractPhysicalDevice14axisIdentifierERK7QString` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput23QAbstractPhysicalDevice16buttonIdentifierERK7QString` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput23QAbstractPhysicalDevice9axisCountEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput23QAbstractPhysicalDevice9axisNamesEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput5QAxis10metaObjectEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput5QAxis5valueEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput5QAxis6inputsEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput7QAction10metaObjectEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput7QAction6inputsEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput7QAction8isActiveEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZNK9Qt3DInput9QKeyEvent10metaObjectEv` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZTIN9Qt3DInput11QInputChordE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZTIN9Qt3DInput11QMouseEventE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZTIN9Qt3DInput11QWheelEventE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZTIN9Qt3DInput12QActionInputE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZTIN9Qt3DInput12QAxisSettingE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZTIN9Qt3DInput12QInputAspectE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZTIN9Qt3DInput12QMouseDeviceE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZTIN9Qt3DInput13QMouseHandlerE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZTIN9Qt3DInput14QInputSequenceE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZTIN9Qt3DInput14QInputSettingsE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZTIN9Qt3DInput14QLogicalDeviceE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZTIN9Qt3DInput15QKeyboardDeviceE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZTIN9Qt3DInput16QAnalogAxisInputE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZTIN9Qt3DInput16QAxisAccumulatorE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZTIN9Qt3DInput16QButtonAxisInputE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZTIN9Qt3DInput16QKeyboardHandlerE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZTIN9Qt3DInput18QAbstractAxisInputE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZTIN9Qt3DInput20QAbstractActionInputE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZTIN9Qt3DInput23QAbstractPhysicalDeviceE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZTIN9Qt3DInput5QAxisE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZTIN9Qt3DInput7QActionE` | 1 | freecad |
| libQt63DInput.6.dylib | `__ZTIN9Qt3DInput9QKeyEventE` | 1 | freecad |
| libQt63DLogic.6.dylib | `__ZN9Qt3DLogic12QFrameAction11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DLogic.6.dylib | `__ZN9Qt3DLogic12QFrameAction11qt_metacastEPKc` | 1 | freecad |
| libQt63DLogic.6.dylib | `__ZN9Qt3DLogic12QFrameAction16staticMetaObjectE` | 1 | freecad |
| libQt63DLogic.6.dylib | `__ZN9Qt3DLogic12QFrameActionC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DLogic.6.dylib | `__ZN9Qt3DLogic12QFrameActionD2Ev` | 1 | freecad |
| libQt63DLogic.6.dylib | `__ZN9Qt3DLogic12QLogicAspect11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DLogic.6.dylib | `__ZN9Qt3DLogic12QLogicAspect11qt_metacastEPKc` | 1 | freecad |
| libQt63DLogic.6.dylib | `__ZN9Qt3DLogic12QLogicAspect12onRegisteredEv` | 1 | freecad |
| libQt63DLogic.6.dylib | `__ZN9Qt3DLogic12QLogicAspect13jobsToExecuteEx` | 1 | freecad |
| libQt63DLogic.6.dylib | `__ZN9Qt3DLogic12QLogicAspect15onEngineStartupEv` | 1 | freecad |
| libQt63DLogic.6.dylib | `__ZN9Qt3DLogic12QLogicAspect16staticMetaObjectE` | 1 | freecad |
| libQt63DLogic.6.dylib | `__ZN9Qt3DLogic12QLogicAspectC2EP7QObject` | 1 | freecad |
| libQt63DLogic.6.dylib | `__ZN9Qt3DLogic12QLogicAspectD2Ev` | 1 | freecad |
| libQt63DLogic.6.dylib | `__ZNK9Qt3DLogic12QFrameAction10metaObjectEv` | 1 | freecad |
| libQt63DLogic.6.dylib | `__ZNK9Qt3DLogic12QLogicAspect10metaObjectEv` | 1 | freecad |
| libQt63DLogic.6.dylib | `__ZTIN9Qt3DLogic12QFrameActionE` | 1 | freecad |
| libQt63DLogic.6.dylib | `__ZTIN9Qt3DLogic12QLogicAspectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QAlphaTest11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QAlphaTest11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QAlphaTest16setAlphaFunctionENS0_13AlphaFunctionE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QAlphaTest16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QAlphaTest17setReferenceValueEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QAlphaTestC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QAlphaTestD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QClipPlane11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QClipPlane11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QClipPlane11setDistanceEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QClipPlane13setPlaneIndexEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QClipPlane16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QClipPlane9setNormalE9QVector3D` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QClipPlaneC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QClipPlaneD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QColorMask11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QColorMask11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QColorMask12setRedMaskedEb` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QColorMask13setBlueMaskedEb` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QColorMask14setAlphaMaskedEb` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QColorMask14setGreenMaskedEb` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QColorMask16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QColorMaskC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QColorMaskD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QDepthTest11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QDepthTest11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QDepthTest16setDepthFunctionENS0_13DepthFunctionE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QDepthTest16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QDepthTestC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QDepthTestD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QDithering11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QDithering11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QDithering16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QDitheringC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QDitheringD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QFilterKey11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QFilterKey11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QFilterKey16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QFilterKey7setNameERK7QString` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QFilterKey8setValueERK8QVariant` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QFilterKeyC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QFilterKeyD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QFrontFace11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QFrontFace11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QFrontFace12setDirectionENS0_16WindingDirectionE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QFrontFace16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QFrontFaceC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QFrontFaceD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QLineWidth11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QLineWidth11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QLineWidth16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QLineWidth8setValueEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QLineWidth9setSmoothEb` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QLineWidthC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QLineWidthD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QNoPicking11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QNoPicking11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QNoPicking16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QNoPickingC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QNoPickingD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QParameter11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QParameter11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QParameter16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QParameter7setNameERK7QString` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QParameter8setValueERK8QVariant` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QParameterC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QParameterC2ERK7QStringPNS_16QAbstractTextureEPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QParameterC2ERK7QStringRK8QVariantPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QParameterD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QPickEvent11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QPickEvent11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QPickEvent11setAcceptedEb` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QPickEvent16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QPickEventC2ERK7QPointFRK9QVector3DS6_f` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QPickEventC2ERK7QPointFRK9QVector3DS6_fNS0_7ButtonsEii` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QPickEventC2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QPickEventD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QPointSize11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QPointSize11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QPointSize11setSizeModeENS0_8SizeModeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QPointSize16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QPointSize8setValueEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QPointSizeC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QPointSizeD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QRayCaster11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QRayCaster11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QRayCaster12setDirectionERK9QVector3D` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QRayCaster16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QRayCaster4pickERK9QVector3DS3_f` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QRayCaster7triggerERK9QVector3DS3_f` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QRayCaster7triggerEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QRayCaster9setLengthEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QRayCaster9setOriginERK9QVector3D` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QRayCasterC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QRayCasterD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QSpotLight11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QSpotLight11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QSpotLight14setCutOffAngleEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QSpotLight16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QSpotLight17setLocalDirectionERK9QVector3D` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QSpotLight20setLinearAttenuationEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QSpotLight22setConstantAttenuationEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QSpotLight23setQuadraticAttenuationEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QSpotLightC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QSpotLightD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QTechnique11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QTechnique11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QTechnique12addFilterKeyEPNS_10QFilterKeyE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QTechnique12addParameterEPNS_10QParameterE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QTechnique13addRenderPassEPNS_11QRenderPassE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QTechnique15removeFilterKeyEPNS_10QFilterKeyE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QTechnique15removeParameterEPNS_10QParameterE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QTechnique16removeRenderPassEPNS_11QRenderPassE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QTechnique16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QTechnique17graphicsApiFilterEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QTechniqueC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QTechniqueD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QTexture1D11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QTexture1D11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QTexture1D16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QTexture1DC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QTexture1DD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QTexture2D11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QTexture2D11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QTexture2D16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QTexture2DC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QTexture2DD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QTexture3D11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QTexture3D11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QTexture3D16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QTexture3DC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QTexture3DD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QWaitFence10setTimeoutEy` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QWaitFence11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QWaitFence11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QWaitFence12setWaitOnCPUEb` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QWaitFence13setHandleTypeENS0_10HandleTypeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QWaitFence16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QWaitFence9setHandleE8QVariant` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QWaitFenceC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender10QWaitFenceD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QCameraLens10viewEntityEN8Qt3DCore7QNodeIdES2_` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QCameraLens11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QCameraLens11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QCameraLens11setExposureEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QCameraLens11setFarPlaneEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QCameraLens12setNearPlaneEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QCameraLens14setAspectRatioEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QCameraLens14setFieldOfViewEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QCameraLens16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QCameraLens17setProjectionTypeENS0_14ProjectionTypeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QCameraLens19setProjectionMatrixERK10QMatrix4x4` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QCameraLens20setFrustumProjectionEffffff` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QCameraLens24setPerspectiveProjectionEffff` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QCameraLens25setOrthographicProjectionEffffff` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QCameraLens6setTopEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QCameraLens7setLeftEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QCameraLens7viewAllEN8Qt3DCore7QNodeIdE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QCameraLens8setRightEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QCameraLens9setBottomEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QCameraLensC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QCameraLensD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QDepthRange11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QDepthRange11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QDepthRange11setFarValueEd` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QDepthRange12setNearValueEd` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QDepthRange16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QDepthRangeC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QDepthRangeD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QPointLight11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QPointLight11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QPointLight16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QPointLight20setLinearAttenuationEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QPointLight22setConstantAttenuationEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QPointLight23setQuadraticAttenuationEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QPointLightC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QPointLightD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QRasterMode11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QRasterMode11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QRasterMode11setFaceModeENS0_8FaceModeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QRasterMode13setRasterModeENS0_10RasterModeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QRasterMode16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QRasterModeC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QRasterModeD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QRenderPass11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QRenderPass11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QRenderPass12addFilterKeyEPNS_10QFilterKeyE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QRenderPass12addParameterEPNS_10QParameterE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QRenderPass14addRenderStateEPNS_12QRenderStateE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QRenderPass15removeFilterKeyEPNS_10QFilterKeyE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QRenderPass15removeParameterEPNS_10QParameterE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QRenderPass16setShaderProgramEPNS_14QShaderProgramE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QRenderPass16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QRenderPass17removeRenderStateEPNS_12QRenderStateE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QRenderPassC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QRenderPassD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QShaderData11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QShaderData11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QShaderData16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QShaderData5eventEP6QEvent` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QShaderDataC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QShaderDataD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QSortPolicy11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QSortPolicy11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QSortPolicy12setSortTypesERK5QListINS0_8SortTypeEE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QSortPolicy12setSortTypesERK5QListIiE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QSortPolicy16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QSortPolicyC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender11QSortPolicyD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QLayerFilter11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QLayerFilter11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QLayerFilter11removeLayerEPNS_6QLayerE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QLayerFilter13setFilterModeENS0_10FilterModeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QLayerFilter16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QLayerFilter8addLayerEPNS_6QLayerE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QLayerFilterC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QLayerFilterD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QNoDepthMask11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QNoDepthMask11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QNoDepthMask16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QNoDepthMaskC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QNoDepthMaskD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QRenderState11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QRenderState11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QRenderState16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QRenderStateD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QSceneLoader11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QSceneLoader11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QSceneLoader16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QSceneLoader9setSourceERK4QUrl` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QSceneLoaderC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QSceneLoaderD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QScissorTest11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QScissorTest11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QScissorTest16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QScissorTest7setLeftEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QScissorTest8setWidthEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QScissorTest9setBottomEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QScissorTest9setHeightEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QScissorTestC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QScissorTestD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QShaderImage10setLayeredEb` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QShaderImage10setTextureEPNS_16QAbstractTextureE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QShaderImage11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QShaderImage11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QShaderImage11setMipLevelEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QShaderImage16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QShaderImage8setLayerEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QShaderImage9setAccessENS0_6AccessE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QShaderImage9setFormatENS0_11ImageFormatE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QShaderImageC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QShaderImageD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QStencilMask11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QStencilMask11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QStencilMask16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QStencilMask17setBackOutputMaskEj` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QStencilMask18setFrontOutputMaskEj` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QStencilMaskC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QStencilMaskD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QStencilTest11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QStencilTest11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QStencilTest16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QStencilTestC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QStencilTestD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QTextureData12addImageDataERK14QSharedPointerINS_17QTextureImageDataEE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QTextureData12setWrapModeXENS_16QTextureWrapMode8WrapModeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QTextureData12setWrapModeYENS_16QTextureWrapMode8WrapModeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QTextureData12setWrapModeZENS_16QTextureWrapMode8WrapModeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QTextureData17setComparisonModeENS_16QAbstractTexture14ComparisonModeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QTextureData20setMaximumAnisotropyEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QTextureData21setComparisonFunctionENS_16QAbstractTexture18ComparisonFunctionE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QTextureData21setMinificationFilterENS_16QAbstractTexture6FilterE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QTextureData22setMagnificationFilterENS_16QAbstractTexture6FilterE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QTextureData30setAutoMipMapGenerationEnabledEb` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QTextureData8setDepthEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QTextureData8setWidthEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QTextureData9setFormatENS_16QAbstractTexture13TextureFormatE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QTextureData9setHeightEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QTextureData9setLayersEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QTextureData9setTargetENS_16QAbstractTexture6TargetE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QTextureDataC1Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender12QTextureDataD1Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QClearBuffers10setBuffersENS0_10BufferTypeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QClearBuffers11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QClearBuffers11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QClearBuffers13setClearColorERK6QColor` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QClearBuffers14setColorBufferEPNS_19QRenderTargetOutputE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QClearBuffers16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QClearBuffers18setClearDepthValueEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QClearBuffers20setClearStencilValueEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QClearBuffersC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QClearBuffersD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QDebugOverlay11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QDebugOverlay11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QDebugOverlay16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QDebugOverlayC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QObjectPicker11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QObjectPicker11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QObjectPicker11setPriorityEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QObjectPicker14setDragEnabledEb` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QObjectPicker15setHoverEnabledEb` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QObjectPicker16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QObjectPickerC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QObjectPickerD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QPickingProxy11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QPickingProxy11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QPickingProxy16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QPickingProxyC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QPickingProxyD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QRayCasterHit16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QRayCasterHit8toStringEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QRayCasterHitC1ENS0_7HitTypeEN8Qt3DCore7QNodeIdEfRK9QVector3DS6_jjjj` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QRayCasterHitC1ERKS0_` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QRayCasterHitC1Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QRayCasterHitD1Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QRayCasterHitaSERKS0_` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QRenderAspect11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QRenderAspect11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QRenderAspect12onRegisteredEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QRenderAspect13jobsToExecuteEx` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QRenderAspect14executeCommandERK5QListI7QStringE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QRenderAspect14onUnregisteredEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QRenderAspect15onEngineStartupEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QRenderAspect16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QRenderAspectC2ENS0_14SubmissionTypeEP7QObject` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QRenderAspectC2EP7QObject` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QRenderAspectD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QRenderTarget11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QRenderTarget11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QRenderTarget12removeOutputEPNS_19QRenderTargetOutputE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QRenderTarget16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QRenderTarget9addOutputEPNS_19QRenderTargetOutputE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QRenderTargetC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QRenderTargetD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QTextureImage11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QTextureImage11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QTextureImage11setMirroredEb` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QTextureImage16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QTextureImage9setSourceERK4QUrl` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QTextureImage9setStatusENS0_6StatusE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QTextureImageC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender13QTextureImageD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QAbstractLight11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QAbstractLight11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QAbstractLight12setIntensityEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QAbstractLight16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QAbstractLight8setColorERK6QColor` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QAbstractLightD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QAlphaCoverage11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QAlphaCoverage11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QAlphaCoverage16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QAlphaCoverageC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QAlphaCoverageD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QBlendEquation11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QBlendEquation11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QBlendEquation16setBlendFunctionENS0_13BlendFunctionE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QBlendEquation16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QBlendEquationC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QBlendEquationD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QBufferCapture11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QBufferCapture11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QBufferCapture16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QBufferCaptureC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QBufferCaptureD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QLevelOfDetail11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QLevelOfDetail11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QLevelOfDetail13setThresholdsERK5QListIdE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QLevelOfDetail15setCurrentIndexEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QLevelOfDetail16setThresholdTypeENS0_13ThresholdTypeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QLevelOfDetail16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QLevelOfDetail17setVolumeOverrideERKNS_28QLevelOfDetailBoundingSphereE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QLevelOfDetail20createBoundingSphereERK9QVector3Df` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QLevelOfDetail9setCameraEPNS_7QCameraE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QLevelOfDetailC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QLevelOfDetailD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QMemoryBarrier11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QMemoryBarrier11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QMemoryBarrier16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QMemoryBarrier17setWaitOperationsE6QFlagsINS0_9OperationEE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QMemoryBarrierC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QMemoryBarrierD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QPickLineEvent11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QPickLineEvent11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QPickLineEvent16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QPickLineEventC2ERK7QPointFRK9QVector3DS6_fjjjNS_10QPickEvent7ButtonsEii` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QPickLineEventC2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QPickLineEventD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QPolygonOffset11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QPolygonOffset11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QPolygonOffset13setDepthStepsEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QPolygonOffset14setScaleFactorEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QPolygonOffset16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QPolygonOffsetC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QPolygonOffsetD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QRenderCapture11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QRenderCapture11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QRenderCapture14requestCaptureERK5QRect` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QRenderCapture14requestCaptureEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QRenderCapture14requestCaptureEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QRenderCapture16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QRenderCaptureC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QShaderProgram10loadSourceERK4QUrl` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QShaderProgram11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QShaderProgram11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QShaderProgram13setShaderCodeENS0_10ShaderTypeERK10QByteArray` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QShaderProgram16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QShaderProgram19setVertexShaderCodeERK10QByteArray` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QShaderProgram20setComputeShaderCodeERK10QByteArray` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QShaderProgram21setFragmentShaderCodeERK10QByteArray` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QShaderProgram21setGeometryShaderCodeERK10QByteArray` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QShaderProgram32setTessellationControlShaderCodeERK10QByteArray` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QShaderProgram35setTessellationEvaluationShaderCodeERK10QByteArray` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QShaderProgram9setFormatENS0_6FormatE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QShaderProgramC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QShaderProgramD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QTextureBuffer11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QTextureBuffer11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QTextureBuffer16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QTextureBufferC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QTextureBufferD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QTextureLoader11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QTextureLoader11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QTextureLoader11setMirroredEb` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QTextureLoader16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QTextureLoader9setSourceERK4QUrl` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QTextureLoaderC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender14QTextureLoaderD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QCameraSelector11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QCameraSelector11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QCameraSelector16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QCameraSelector9setCameraEPN8Qt3DCore7QEntityE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QCameraSelectorC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QCameraSelectorD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QComputeCommand10setRunTypeENS0_7RunTypeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QComputeCommand11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QComputeCommand11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QComputeCommand13setWorkGroupXEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QComputeCommand13setWorkGroupYEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QComputeCommand13setWorkGroupZEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QComputeCommand16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QComputeCommand7triggerEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QComputeCommand7triggerEiiii` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QComputeCommandC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QComputeCommandD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QFrameGraphNode11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QFrameGraphNode11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QFrameGraphNode16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QFrameGraphNodeC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QFrameGraphNodeD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QFrustumCulling11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QFrustumCulling11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QFrustumCulling16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QFrustumCullingC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QFrustumCullingD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QPickPointEvent11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QPickPointEvent11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QPickPointEvent16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QPickPointEventC2ERK7QPointFRK9QVector3DS6_fjNS_10QPickEvent7ButtonsEii` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QPickPointEventC2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QPickPointEventD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QRenderSettings11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QRenderSettings11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QRenderSettings15pickingSettingsEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QRenderSettings15setRenderPolicyENS0_12RenderPolicyE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QRenderSettings16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QRenderSettings18renderCapabilitiesEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QRenderSettings19setActiveFrameGraphEPNS_15QFrameGraphNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QRenderSettingsC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QRenderSettingsD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QRenderStateSet11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QRenderStateSet11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QRenderStateSet14addRenderStateEPNS_12QRenderStateE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QRenderStateSet16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QRenderStateSet17removeRenderStateEPNS_12QRenderStateE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QRenderStateSetC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QRenderStateSetD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QSubtreeEnabler11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QSubtreeEnabler11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QSubtreeEnabler13requestUpdateEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QSubtreeEnabler13setEnablementENS0_10EnablementE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QSubtreeEnabler16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QSubtreeEnablerC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QSubtreeEnablerD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QTexture1DArray11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QTexture1DArray11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QTexture1DArray16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QTexture1DArrayC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QTexture1DArrayD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QTexture2DArray11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QTexture2DArray11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QTexture2DArray16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QTexture2DArrayC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QTexture2DArrayD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QTextureCubeMap11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QTextureCubeMap11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QTextureCubeMap16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QTextureCubeMapC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender15QTextureCubeMapD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QAbstractTexture10setSamplesEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QAbstractTexture10updateDataERKNS_18QTextureDataUpdateE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QAbstractTexture11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QAbstractTexture11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QAbstractTexture11setWrapModeERKNS_16QTextureWrapModeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QAbstractTexture12setMipLevelsEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QAbstractTexture13setHandleTypeENS0_10HandleTypeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QAbstractTexture15addTextureImageEPNS_21QAbstractTextureImageE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QAbstractTexture16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QAbstractTexture17setComparisonModeENS0_14ComparisonModeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QAbstractTexture18removeTextureImageEPNS_21QAbstractTextureImageE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QAbstractTexture18setGenerateMipMapsEb` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QAbstractTexture20setMaximumAnisotropyEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QAbstractTexture21setComparisonFunctionENS0_18ComparisonFunctionE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QAbstractTexture21setMinificationFilterENS0_6FilterE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QAbstractTexture22setMagnificationFilterENS0_6FilterE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QAbstractTexture7setSizeEiii` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QAbstractTexture8setDepthEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QAbstractTexture8setWidthEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QAbstractTexture8wrapModeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QAbstractTexture9setFormatENS0_13TextureFormatE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QAbstractTexture9setHandleERK8QVariant` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QAbstractTexture9setHeightEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QAbstractTexture9setLayersEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QAbstractTexture9setStatusENS0_6StatusE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QAbstractTextureC2ENS0_6TargetEPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QAbstractTextureC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QAbstractTextureD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QBlitFramebuffer11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QBlitFramebuffer11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QBlitFramebuffer13setSourceRectERK6QRectF` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QBlitFramebuffer14setDestinationEPNS_13QRenderTargetE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QBlitFramebuffer16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QBlitFramebuffer18setDestinationRectERK6QRectF` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QBlitFramebuffer22setInterpolationMethodENS0_19InterpolationMethodE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QBlitFramebuffer24setSourceAttachmentPointENS_19QRenderTargetOutput15AttachmentPointE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QBlitFramebuffer29setDestinationAttachmentPointENS_19QRenderTargetOutput15AttachmentPointE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QBlitFramebuffer9setSourceEPNS_13QRenderTargetE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QBlitFramebufferC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QBlitFramebufferD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QDispatchCompute11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QDispatchCompute11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QDispatchCompute13setWorkGroupXEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QDispatchCompute13setWorkGroupYEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QDispatchCompute13setWorkGroupZEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QDispatchCompute16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QDispatchComputeC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QDispatchComputeD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QPickingSettings11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QPickingSettings11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QPickingSettings13setPickMethodENS0_10PickMethodE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QPickingSettings16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QPickingSettings17setPickResultModeENS0_14PickResultModeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QPickingSettings22setWorldSpaceToleranceEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QPickingSettings29setFaceOrientationPickingModeENS0_26FaceOrientationPickingModeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QPickingSettingsC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QPickingSettingsD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QProximityFilter11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QProximityFilter11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QProximityFilter16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QProximityFilter20setDistanceThresholdEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QProximityFilter9setEntityEPN8Qt3DCore7QEntityE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QProximityFilterC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QProximityFilterD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QScreenRayCaster11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QScreenRayCaster11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QScreenRayCaster11setPositionERK6QPoint` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QScreenRayCaster16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QScreenRayCaster4pickERK6QPoint` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QScreenRayCaster7triggerERK6QPoint` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QScreenRayCaster7triggerEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QScreenRayCasterC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QScreenRayCasterD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QSeamlessCubemap11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QSeamlessCubemap11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QSeamlessCubemap16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QSeamlessCubemapC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QSeamlessCubemapD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QSharedGLTexture11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QSharedGLTexture11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QSharedGLTexture12setTextureIdEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QSharedGLTexture16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QSharedGLTextureC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QSharedGLTextureD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QTechniqueFilter11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QTechniqueFilter11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QTechniqueFilter11removeMatchEPNS_10QFilterKeyE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QTechniqueFilter12addParameterEPNS_10QParameterE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QTechniqueFilter15removeParameterEPNS_10QParameterE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QTechniqueFilter16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QTechniqueFilter8addMatchEPNS_10QFilterKeyE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QTechniqueFilterC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QTechniqueFilterD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QTextureWrapMode11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QTextureWrapMode11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QTextureWrapMode16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QTextureWrapMode4setXENS0_8WrapModeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QTextureWrapMode4setYENS0_8WrapModeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QTextureWrapMode4setZENS0_8WrapModeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QTextureWrapModeC2ENS0_8WrapModeEP7QObject` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QTextureWrapModeC2ENS0_8WrapModeES1_S1_P7QObject` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender16QTextureWrapModeD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QDirectionalLight11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QDirectionalLight11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QDirectionalLight16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QDirectionalLight17setWorldDirectionERK9QVector3D` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QDirectionalLightC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QDirectionalLightD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QEnvironmentLight11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QEnvironmentLight11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QEnvironmentLight11setSpecularEPNS_16QAbstractTextureE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QEnvironmentLight13setIrradianceEPNS_16QAbstractTextureE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QEnvironmentLight16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QEnvironmentLightC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QEnvironmentLightD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QGeometryRenderer11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QGeometryRenderer11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QGeometryRenderer11setGeometryEPN8Qt3DCore9QGeometryE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QGeometryRenderer12setSortIndexEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QGeometryRenderer14setFirstVertexEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QGeometryRenderer14setIndexOffsetEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QGeometryRenderer14setVertexCountEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QGeometryRenderer16setFirstInstanceEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QGeometryRenderer16setInstanceCountEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QGeometryRenderer16setPrimitiveTypeENS0_13PrimitiveTypeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QGeometryRenderer16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QGeometryRenderer19setVerticesPerPatchEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QGeometryRenderer20setRestartIndexValueEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QGeometryRenderer24setIndexBufferByteOffsetEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QGeometryRenderer26setPrimitiveRestartEnabledEb` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QGeometryRendererC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QGeometryRendererD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QRenderPassFilter11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QRenderPassFilter11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QRenderPassFilter11removeMatchEPNS_10QFilterKeyE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QRenderPassFilter12addParameterEPNS_10QParameterE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QRenderPassFilter15removeParameterEPNS_10QParameterE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QRenderPassFilter16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QRenderPassFilter8addMatchEPNS_10QFilterKeyE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QRenderPassFilterC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QRenderPassFilterD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QStencilOperation11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QStencilOperation11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QStencilOperation16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QStencilOperationC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QStencilOperationD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QTextureImageData12setAlignmentEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QTextureImageData12setMipLevelsEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QTextureImageData12setPixelTypeEN14QOpenGLTexture9PixelTypeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QTextureImageData14setPixelFormatEN14QOpenGLTexture11PixelFormatE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QTextureImageData7cleanupEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QTextureImageData7setDataERK10QByteArrayib` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QTextureImageData8setDepthEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QTextureImageData8setFacesEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QTextureImageData8setImageERK6QImage` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QTextureImageData8setWidthEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QTextureImageData9setFormatEN14QOpenGLTexture13TextureFormatE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QTextureImageData9setHeightEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QTextureImageData9setLayersEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QTextureImageData9setTargetEN14QOpenGLTexture6TargetE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QTextureImageDataC1Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QTextureImageDataD1Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QTextureRectangle11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QTextureRectangle11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QTextureRectangle16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QTextureRectangleC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender17QTextureRectangleD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QAbstractRayCaster10setRunModeENS0_7RunModeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QAbstractRayCaster11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QAbstractRayCaster11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QAbstractRayCaster11removeLayerEPNS_6QLayerE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QAbstractRayCaster13setFilterModeENS0_10FilterModeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QAbstractRayCaster16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QAbstractRayCaster8addLayerEPNS_6QLayerE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QAbstractRayCasterC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QAbstractRayCasterD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QGraphicsApiFilter10setProfileENS0_13OpenGLProfileE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QGraphicsApiFilter11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QGraphicsApiFilter11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QGraphicsApiFilter13setExtensionsERK5QListI7QStringE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QGraphicsApiFilter15setMajorVersionEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QGraphicsApiFilter15setMinorVersionEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QGraphicsApiFilter16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QGraphicsApiFilter6setApiENS0_3ApiE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QGraphicsApiFilter9setVendorERK7QString` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QGraphicsApiFilterC2EP7QObject` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QGraphicsApiFilterD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QPickTriangleEvent11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QPickTriangleEvent11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QPickTriangleEvent16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QPickTriangleEventC2ERK7QPointFRK9QVector3DS6_fjjjj` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QPickTriangleEventC2ERK7QPointFRK9QVector3DS6_fjjjjNS_10QPickEvent7ButtonsEiiS6_` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QPickTriangleEventC2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QPickTriangleEventD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QTextureDataUpdate11setMipLevelEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QTextureDataUpdate4setXEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QTextureDataUpdate4setYEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QTextureDataUpdate4setZEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QTextureDataUpdate7setDataERK14QSharedPointerINS_17QTextureImageDataEE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QTextureDataUpdate7setFaceENS_16QAbstractTexture11CubeMapFaceE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QTextureDataUpdate8setLayerEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QTextureDataUpdateC1ERKS0_` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QTextureDataUpdateC1Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QTextureDataUpdateD1Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender18QTextureDataUpdateaSERKS0_` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender19QRenderCapabilities11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender19QRenderCapabilities11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender19QRenderCapabilities16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender19QRenderCapabilitiesC2EP7QObject` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender19QRenderCapabilitiesD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender19QRenderCaptureReply11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender19QRenderCaptureReply11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender19QRenderCaptureReply16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender19QRenderTargetOutput10setTextureEPNS_16QAbstractTextureE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender19QRenderTargetOutput11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender19QRenderTargetOutput11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender19QRenderTargetOutput11setMipLevelEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender19QRenderTargetOutput16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender19QRenderTargetOutput18setAttachmentPointENS0_15AttachmentPointE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender19QRenderTargetOutput7setFaceENS_16QAbstractTexture11CubeMapFaceE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender19QRenderTargetOutput8setLayerEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender19QRenderTargetOutputC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender19QRenderTargetOutputD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender20QLevelOfDetailSwitch11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender20QLevelOfDetailSwitch11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender20QLevelOfDetailSwitch16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender20QLevelOfDetailSwitchC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender20QLevelOfDetailSwitchD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender20QPaintedTextureImage11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender20QPaintedTextureImage11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender20QPaintedTextureImage16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender20QPaintedTextureImage6updateERK5QRect` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender20QPaintedTextureImage7setSizeE5QSize` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender20QPaintedTextureImage8setWidthEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender20QPaintedTextureImage9setHeightEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender20QPaintedTextureImageC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender20QPaintedTextureImageD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender20QTextureCubeMapArray11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender20QTextureCubeMapArray11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender20QTextureCubeMapArray16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender20QTextureCubeMapArrayC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender20QTextureCubeMapArrayD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QAbstractTextureImage11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QAbstractTextureImage11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QAbstractTextureImage11setMipLevelEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QAbstractTextureImage16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QAbstractTextureImage26notifyDataGeneratorChangedEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QAbstractTextureImage7setFaceENS_16QAbstractTexture11CubeMapFaceE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QAbstractTextureImage8setLayerEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QAbstractTextureImageC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QAbstractTextureImageD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QRenderTargetSelector11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QRenderTargetSelector11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QRenderTargetSelector16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QRenderTargetSelector9setTargetEPNS_13QRenderTargetE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QRenderTargetSelectorC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QRenderTargetSelectorD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QShaderProgramBuilder11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QShaderProgramBuilder11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QShaderProgramBuilder16setEnabledLayersERK5QListI7QStringE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QShaderProgramBuilder16setShaderProgramEPNS_14QShaderProgramE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QShaderProgramBuilder16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QShaderProgramBuilder20setVertexShaderGraphERK4QUrl` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QShaderProgramBuilder21setComputeShaderGraphERK4QUrl` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QShaderProgramBuilder22setFragmentShaderGraphERK4QUrl` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QShaderProgramBuilder22setGeometryShaderGraphERK4QUrl` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QShaderProgramBuilder33setTessellationControlShaderGraphERK4QUrl` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QShaderProgramBuilder36setTessellationEvaluationShaderGraphERK4QUrl` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QShaderProgramBuilderC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QShaderProgramBuilderD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QStencilTestArguments11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QStencilTestArguments11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QStencilTestArguments16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QStencilTestArguments17setComparisonMaskEj` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QStencilTestArguments17setReferenceValueEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QStencilTestArguments18setStencilFunctionENS0_15StencilFunctionE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QStencilTestArgumentsD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QTexture2DMultisample11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QTexture2DMultisample11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QTexture2DMultisample16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QTexture2DMultisampleC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender21QTexture2DMultisampleD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender22QRenderSurfaceSelector10setSurfaceEP7QObject` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender22QRenderSurfaceSelector11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender22QRenderSurfaceSelector11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender22QRenderSurfaceSelector16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender22QRenderSurfaceSelector20setSurfacePixelRatioEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender22QRenderSurfaceSelector27setExternalRenderTargetSizeERK5QSize` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender22QRenderSurfaceSelectorC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender22QRenderSurfaceSelectorD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender23PropertyReaderInterfaceD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender23QBlendEquationArguments11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender23QBlendEquationArguments11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender23QBlendEquationArguments12setSourceRgbENS0_8BlendingE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender23QBlendEquationArguments13setSourceRgbaENS0_8BlendingE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender23QBlendEquationArguments14setBufferIndexEi` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender23QBlendEquationArguments14setSourceAlphaENS0_8BlendingE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender23QBlendEquationArguments16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender23QBlendEquationArguments17setDestinationRgbENS0_8BlendingE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender23QBlendEquationArguments18setDestinationRgbaENS0_8BlendingE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender23QBlendEquationArguments19setDestinationAlphaENS0_8BlendingE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender23QBlendEquationArgumentsC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender23QBlendEquationArgumentsD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender24QMultiSampleAntiAliasing11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender24QMultiSampleAntiAliasing11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender24QMultiSampleAntiAliasing16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender24QMultiSampleAntiAliasingC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender24QMultiSampleAntiAliasingD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender26QStencilOperationArguments11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender26QStencilOperationArguments11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender26QStencilOperationArguments16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender26QStencilOperationArguments24setAllTestsPassOperationENS0_9OperationE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender26QStencilOperationArguments28setDepthTestFailureOperationENS0_9OperationE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender26QStencilOperationArguments30setStencilTestFailureOperationENS0_9OperationE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender26QStencilOperationArgumentsD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender26QTexture2DMultisampleArray11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender26QTexture2DMultisampleArray11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender26QTexture2DMultisampleArray16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender26QTexture2DMultisampleArrayC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender26QTexture2DMultisampleArrayD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender26QTextureImageDataGeneratorD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender28QLevelOfDetailBoundingSphere16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender28QLevelOfDetailBoundingSphereC1E9QVector3Df` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender28QLevelOfDetailBoundingSphereC1ERKS0_` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender28QLevelOfDetailBoundingSphereD1Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender5QMesh11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender5QMesh11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender5QMesh11setMeshNameERK7QString` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender5QMesh16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender5QMesh9setSourceERK4QUrl` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender5QMeshC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender5QMeshD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender6QLayer11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender6QLayer11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender6QLayer12setRecursiveEb` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender6QLayer16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender6QLayerC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender6QLayerD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCamera10viewEntityEPN8Qt3DCore7QEntityE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCamera10viewSphereERK9QVector3Df` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCamera11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCamera11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCamera11setExposureEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCamera11setFarPlaneEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCamera11setPositionERK9QVector3D` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCamera11setUpVectorERK9QVector3D` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCamera12setNearPlaneEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCamera13setViewCenterERK9QVector3D` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCamera14setAspectRatioEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCamera14setFieldOfViewEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCamera14translateWorldERK9QVector3DNS0_23CameraTranslationOptionE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCamera16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCamera17setProjectionTypeENS_11QCameraLens14ProjectionTypeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCamera18panAboutViewCenterEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCamera18panAboutViewCenterEfRK9QVector3D` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCamera19rollAboutViewCenterEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCamera19setProjectionMatrixERK10QMatrix4x4` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCamera19tiltAboutViewCenterEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCamera21rotateAboutViewCenterERK11QQuaternion` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCamera3panEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCamera3panEfRK9QVector3D` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCamera4rollEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCamera4tiltEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCamera6rotateERK11QQuaternion` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCamera6setTopEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCamera7setLeftEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCamera7viewAllEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCamera8setRightEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCamera9setBottomEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCamera9translateERK9QVector3DNS0_23CameraTranslationOptionE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCameraC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QCameraD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QEffect11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QEffect11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QEffect12addParameterEPNS_10QParameterE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QEffect12addTechniqueEPNS_10QTechniqueE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QEffect15removeParameterEPNS_10QParameterE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QEffect15removeTechniqueEPNS_10QTechniqueE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QEffect16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QEffectC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QEffectD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QNoDraw11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QNoDraw11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QNoDraw16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QNoDrawC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender7QNoDrawD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender9QCullFace11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender9QCullFace11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender9QCullFace16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender9QCullFace7setModeENS0_11CullingModeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender9QCullFaceC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender9QCullFaceD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender9QMaterial11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender9QMaterial11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender9QMaterial12addParameterEPNS_10QParameterE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender9QMaterial15removeParameterEPNS_10QParameterE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender9QMaterial16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender9QMaterial9setEffectEPNS_7QEffectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender9QMaterialC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender9QMaterialD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender9QSetFence11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender9QSetFence11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender9QSetFence16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender9QSetFenceC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender9QSetFenceD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender9QViewport11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender9QViewport11qt_metacastEPKc` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender9QViewport16staticMetaObjectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender9QViewport17setNormalizedRectERK6QRectF` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender9QViewport8setGammaEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender9QViewportC2EPN8Qt3DCore5QNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRender9QViewportD2Ev` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZN10Qt3DRendereqERKNS_18QTextureDataUpdateES2_` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QAlphaTest10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QAlphaTest13alphaFunctionEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QAlphaTest14referenceValueEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QClipPlane10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QClipPlane10planeIndexEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QClipPlane6normalEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QClipPlane8distanceEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QColorMask10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QColorMask11isRedMaskedEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QColorMask12isBlueMaskedEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QColorMask13isAlphaMaskedEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QColorMask13isGreenMaskedEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QDepthTest10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QDepthTest13depthFunctionEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QDithering10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QFilterKey10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QFilterKey4nameEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QFilterKey5valueEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QFrontFace10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QFrontFace9directionEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QLineWidth10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QLineWidth5valueEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QLineWidth6smoothEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QNoPicking10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QParameter10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QParameter4nameEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QParameter5valueEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QPickEvent10isAcceptedEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QPickEvent10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QPickEvent17localIntersectionEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QPickEvent17worldIntersectionEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QPickEvent6buttonEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QPickEvent6entityEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QPickEvent7buttonsEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QPickEvent8distanceEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QPickEvent8positionEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QPickEvent8viewportEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QPickEvent9modifiersEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QPointSize10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QPointSize5valueEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QPointSize8sizeModeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QRayCaster10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QRayCaster6lengthEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QRayCaster6originEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QRayCaster9directionEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QSpotLight10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QSpotLight11cutOffAngleEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QSpotLight14localDirectionEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QSpotLight17linearAttenuationEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QSpotLight19constantAttenuationEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QSpotLight20quadraticAttenuationEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QTechnique10filterKeysEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QTechnique10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QTechnique10parametersEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QTechnique12renderPassesEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QTexture1D10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QTexture2D10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QTexture3D10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QWaitFence10handleTypeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QWaitFence10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QWaitFence6handleEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QWaitFence7timeoutEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender10QWaitFence9waitOnCPUEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender11QCameraLens10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender11QCameraLens11aspectRatioEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender11QCameraLens11fieldOfViewEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender11QCameraLens14projectionTypeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender11QCameraLens16projectionMatrixEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender11QCameraLens3topEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender11QCameraLens4leftEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender11QCameraLens5rightEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender11QCameraLens6bottomEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender11QCameraLens8exposureEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender11QCameraLens8farPlaneEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender11QCameraLens9nearPlaneEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender11QDepthRange10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender11QDepthRange8farValueEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender11QDepthRange9nearValueEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender11QPointLight10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender11QPointLight17linearAttenuationEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender11QPointLight19constantAttenuationEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender11QPointLight20quadraticAttenuationEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender11QRasterMode10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender11QRasterMode10rasterModeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender11QRasterMode8faceModeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender11QRenderPass10filterKeysEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender11QRenderPass10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender11QRenderPass10parametersEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender11QRenderPass12renderStatesEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender11QRenderPass13shaderProgramEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender11QShaderData10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender11QShaderData14propertyReaderEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender11QSortPolicy10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender11QSortPolicy12sortTypesIntEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender11QSortPolicy9sortTypesEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QLayerFilter10filterModeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QLayerFilter10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QLayerFilter6layersEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QNoDepthMask10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QRenderState10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QSceneLoader10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QSceneLoader11entityNamesEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QSceneLoader6entityERK7QString` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QSceneLoader6sourceEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QSceneLoader6statusEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QSceneLoader9componentERK7QStringNS0_13ComponentTypeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QScissorTest10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QScissorTest4leftEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QScissorTest5widthEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QScissorTest6bottomEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QScissorTest6heightEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QShaderImage10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QShaderImage5layerEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QShaderImage6accessEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QShaderImage6formatEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QShaderImage7layeredEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QShaderImage7textureEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QShaderImage8mipLevelEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QStencilMask10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QStencilMask14backOutputMaskEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QStencilMask15frontOutputMaskEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QStencilTest10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QStencilTest4backEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QStencilTest5frontEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QTextureData14comparisonModeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QTextureData17maximumAnisotropyEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QTextureData18comparisonFunctionEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QTextureData18minificationFilterEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QTextureData19magnificationFilterEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QTextureData29isAutoMipMapGenerationEnabledEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QTextureData5depthEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QTextureData5widthEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QTextureData6formatEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QTextureData6heightEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QTextureData6layersEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QTextureData6targetEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QTextureData9imageDataEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QTextureData9wrapModeXEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QTextureData9wrapModeYEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender12QTextureData9wrapModeZEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QClearBuffers10clearColorEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QClearBuffers10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QClearBuffers11colorBufferEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QClearBuffers15clearDepthValueEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QClearBuffers17clearStencilValueEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QClearBuffers7buffersEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QDebugOverlay10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QObjectPicker10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QObjectPicker13containsMouseEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QObjectPicker13isDragEnabledEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QObjectPicker14isHoverEnabledEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QObjectPicker8priorityEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QObjectPicker9isPressedEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QPickingProxy10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QRayCasterHit12vertex1IndexEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QRayCasterHit12vertex2IndexEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QRayCasterHit12vertex3IndexEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QRayCasterHit14primitiveIndexEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QRayCasterHit17localIntersectionEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QRayCasterHit17worldIntersectionEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QRayCasterHit4typeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QRayCasterHit6entityEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QRayCasterHit8distanceEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QRayCasterHit8entityIdEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QRenderAspect10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QRenderAspect12dependenciesEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QRenderTarget10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QRenderTarget7outputsEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QTextureImage10isMirroredEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QTextureImage10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QTextureImage13dataGeneratorEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QTextureImage6sourceEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender13QTextureImage6statusEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QAbstractLight10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QAbstractLight4typeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QAbstractLight5colorEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QAbstractLight9intensityEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QAlphaCoverage10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QBlendEquation10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QBlendEquation13blendFunctionEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QBufferCapture10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QLevelOfDetail10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QLevelOfDetail10thresholdsEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QLevelOfDetail12currentIndexEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QLevelOfDetail13thresholdTypeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QLevelOfDetail14volumeOverrideEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QLevelOfDetail6cameraEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QMemoryBarrier10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QMemoryBarrier14waitOperationsEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QPickLineEvent10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QPickLineEvent12vertex1IndexEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QPickLineEvent12vertex2IndexEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QPickLineEvent9edgeIndexEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QPolygonOffset10depthStepsEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QPolygonOffset10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QPolygonOffset11scaleFactorEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QRenderCapture10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QShaderProgram10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QShaderProgram10shaderCodeENS0_10ShaderTypeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QShaderProgram16vertexShaderCodeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QShaderProgram17computeShaderCodeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QShaderProgram18fragmentShaderCodeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QShaderProgram18geometryShaderCodeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QShaderProgram29tessellationControlShaderCodeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QShaderProgram32tessellationEvaluationShaderCodeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QShaderProgram3logEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QShaderProgram6formatEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QShaderProgram6statusEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QTextureBuffer10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QTextureLoader10isMirroredEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QTextureLoader10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender14QTextureLoader6sourceEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender15QCameraSelector10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender15QCameraSelector6cameraEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender15QComputeCommand10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender15QComputeCommand10workGroupXEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender15QComputeCommand10workGroupYEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender15QComputeCommand10workGroupZEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender15QComputeCommand7runTypeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender15QFrameGraphNode10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender15QFrameGraphNode20parentFrameGraphNodeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender15QFrustumCulling10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender15QPickPointEvent10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender15QPickPointEvent10pointIndexEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender15QRenderSettings10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender15QRenderSettings12renderPolicyEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender15QRenderSettings16activeFrameGraphEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender15QRenderStateSet10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender15QRenderStateSet12renderStatesEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender15QSubtreeEnabler10enablementEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender15QSubtreeEnabler10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender15QTexture1DArray10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender15QTexture2DArray10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender15QTextureCubeMap10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QAbstractTexture10handleTypeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QAbstractTexture10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QAbstractTexture13textureImagesEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QAbstractTexture14comparisonModeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QAbstractTexture15generateMipMapsEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QAbstractTexture17maximumAnisotropyEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QAbstractTexture18comparisonFunctionEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QAbstractTexture18minificationFilterEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QAbstractTexture19magnificationFilterEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QAbstractTexture5depthEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QAbstractTexture5widthEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QAbstractTexture6formatEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QAbstractTexture6handleEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QAbstractTexture6heightEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QAbstractTexture6layersEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QAbstractTexture6statusEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QAbstractTexture6targetEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QAbstractTexture7samplesEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QAbstractTexture9mipLevelsEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QBlitFramebuffer10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QBlitFramebuffer10sourceRectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QBlitFramebuffer11destinationEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QBlitFramebuffer15destinationRectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QBlitFramebuffer19interpolationMethodEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QBlitFramebuffer21sourceAttachmentPointEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QBlitFramebuffer26destinationAttachmentPointEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QBlitFramebuffer6sourceEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QDispatchCompute10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QDispatchCompute10workGroupXEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QDispatchCompute10workGroupYEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QDispatchCompute10workGroupZEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QPickingSettings10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QPickingSettings10pickMethodEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QPickingSettings14pickResultModeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QPickingSettings19worldSpaceToleranceEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QPickingSettings26faceOrientationPickingModeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QProximityFilter10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QProximityFilter17distanceThresholdEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QProximityFilter6entityEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QScreenRayCaster10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QScreenRayCaster8positionEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QSeamlessCubemap10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QSharedGLTexture10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QSharedGLTexture9textureIdEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QTechniqueFilter10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QTechniqueFilter10parametersEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QTechniqueFilter8matchAllEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QTextureWrapMode10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QTextureWrapMode1xEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QTextureWrapMode1yEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender16QTextureWrapMode1zEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QDirectionalLight10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QDirectionalLight14worldDirectionEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QEnvironmentLight10irradianceEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QEnvironmentLight10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QEnvironmentLight8specularEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QGeometryRenderer10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QGeometryRenderer11firstVertexEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QGeometryRenderer11indexOffsetEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QGeometryRenderer11vertexCountEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QGeometryRenderer13firstInstanceEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QGeometryRenderer13instanceCountEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QGeometryRenderer13primitiveTypeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QGeometryRenderer16verticesPerPatchEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QGeometryRenderer17restartIndexValueEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QGeometryRenderer21indexBufferByteOffsetEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QGeometryRenderer23primitiveRestartEnabledEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QGeometryRenderer8geometryEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QGeometryRenderer9sortIndexEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QRenderPassFilter10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QRenderPassFilter10parametersEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QRenderPassFilter8matchAnyEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QStencilOperation10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QStencilOperation4backEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QStencilOperation5frontEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QTextureImageData11pixelFormatEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QTextureImageData12isCompressedEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QTextureImageData4dataEiii` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QTextureImageData5depthEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QTextureImageData5facesEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QTextureImageData5widthEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QTextureImageData6formatEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QTextureImageData6heightEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QTextureImageData6layersEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QTextureImageData6targetEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QTextureImageData9alignmentEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QTextureImageData9mipLevelsEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QTextureImageData9pixelTypeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender17QTextureRectangle10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender18QAbstractRayCaster10filterModeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender18QAbstractRayCaster10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender18QAbstractRayCaster4hitsEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender18QAbstractRayCaster6layersEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender18QAbstractRayCaster7runModeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender18QGraphicsApiFilter10extensionsEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender18QGraphicsApiFilter10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender18QGraphicsApiFilter12majorVersionEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender18QGraphicsApiFilter12minorVersionEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender18QGraphicsApiFilter3apiEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender18QGraphicsApiFilter6vendorEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender18QGraphicsApiFilter7profileEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender18QPickTriangleEvent10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender18QPickTriangleEvent12vertex1IndexEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender18QPickTriangleEvent12vertex2IndexEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender18QPickTriangleEvent12vertex3IndexEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender18QPickTriangleEvent13triangleIndexEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender18QPickTriangleEvent3uvwEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender18QTextureDataUpdate1xEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender18QTextureDataUpdate1yEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender18QTextureDataUpdate1zEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender18QTextureDataUpdate4dataEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender18QTextureDataUpdate4faceEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender18QTextureDataUpdate5layerEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender18QTextureDataUpdate8mipLevelEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCapabilities10extensionsEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCapabilities10maxSamplesEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCapabilities10maxUBOSizeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCapabilities10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCapabilities11glslVersionEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCapabilities11maxSSBOSizeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCapabilities11supportsUBOEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCapabilities12majorVersionEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCapabilities12minorVersionEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCapabilities12supportsSSBOEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCapabilities13driverVersionEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCapabilities13maxImageUnitsEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCapabilities14maxTextureSizeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCapabilities14maxUBOBindingsEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCapabilities15maxSSBOBindingsEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCapabilities15maxTextureUnitsEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCapabilities15supportsComputeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCapabilities16maxTextureLayersEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCapabilities17maxWorkGroupSizeXEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCapabilities17maxWorkGroupSizeYEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCapabilities17maxWorkGroupSizeZEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCapabilities18maxWorkGroupCountXEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCapabilities18maxWorkGroupCountYEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCapabilities18maxWorkGroupCountZEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCapabilities18supportsImageStoreEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCapabilities21maxComputeInvocationsEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCapabilities26maxComputeSharedMemorySizeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCapabilities3apiEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCapabilities6vendorEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCapabilities7isValidEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCapabilities7profileEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCapabilities8rendererEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCaptureReply10isCompleteEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCaptureReply10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCaptureReply5imageEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCaptureReply9captureIdEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderCaptureReply9saveImageERK7QString` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderTargetOutput10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderTargetOutput15attachmentPointEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderTargetOutput4faceEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderTargetOutput5layerEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderTargetOutput7textureEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender19QRenderTargetOutput8mipLevelEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender20QLevelOfDetailSwitch10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender20QPaintedTextureImage10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender20QPaintedTextureImage13dataGeneratorEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender20QPaintedTextureImage4sizeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender20QPaintedTextureImage5widthEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender20QPaintedTextureImage6heightEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender20QTextureCubeMapArray10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender21QAbstractTextureImage10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender21QAbstractTextureImage4faceEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender21QAbstractTextureImage5layerEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender21QAbstractTextureImage8mipLevelEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender21QRenderTargetSelector10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender21QRenderTargetSelector6targetEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender21QShaderProgramBuilder10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender21QShaderProgramBuilder13enabledLayersEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender21QShaderProgramBuilder13shaderProgramEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender21QShaderProgramBuilder16vertexShaderCodeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender21QShaderProgramBuilder17computeShaderCodeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender21QShaderProgramBuilder17vertexShaderGraphEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender21QShaderProgramBuilder18computeShaderGraphEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender21QShaderProgramBuilder18fragmentShaderCodeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender21QShaderProgramBuilder18geometryShaderCodeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender21QShaderProgramBuilder19fragmentShaderGraphEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender21QShaderProgramBuilder19geometryShaderGraphEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender21QShaderProgramBuilder29tessellationControlShaderCodeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender21QShaderProgramBuilder30tessellationControlShaderGraphEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender21QShaderProgramBuilder32tessellationEvaluationShaderCodeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender21QShaderProgramBuilder33tessellationEvaluationShaderGraphEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender21QStencilTestArguments10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender21QStencilTestArguments14comparisonMaskEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender21QStencilTestArguments14referenceValueEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender21QStencilTestArguments15stencilFunctionEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender21QStencilTestArguments8faceModeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender21QTexture2DMultisample10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender22QRenderSurfaceSelector10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender22QRenderSurfaceSelector17surfacePixelRatioEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender22QRenderSurfaceSelector24externalRenderTargetSizeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender22QRenderSurfaceSelector7surfaceEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender23QBlendEquationArguments10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender23QBlendEquationArguments11bufferIndexEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender23QBlendEquationArguments11sourceAlphaEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender23QBlendEquationArguments14destinationRgbEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender23QBlendEquationArguments16destinationAlphaEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender23QBlendEquationArguments9sourceRgbEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender24QMultiSampleAntiAliasing10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender26QStencilOperationArguments10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender26QStencilOperationArguments21allTestsPassOperationEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender26QStencilOperationArguments25depthTestFailureOperationEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender26QStencilOperationArguments27stencilTestFailureOperationEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender26QStencilOperationArguments8faceModeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender26QTexture2DMultisampleArray10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender28QLevelOfDetailBoundingSphere6centerEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender28QLevelOfDetailBoundingSphere6radiusEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender28QLevelOfDetailBoundingSphere7isEmptyEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender28QLevelOfDetailBoundingSphereeqERKS0_` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender28QLevelOfDetailBoundingSphereneERKS0_` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender5QMesh10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender5QMesh6sourceEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender5QMesh6statusEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender5QMesh8meshNameEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender6QLayer10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender6QLayer9recursiveEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender7QCamera10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender7QCamera10viewCenterEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender7QCamera10viewMatrixEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender7QCamera10viewVectorEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender7QCamera11aspectRatioEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender7QCamera11fieldOfViewEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender7QCamera11panRotationEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender7QCamera12rollRotationEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender7QCamera12tiltRotationEf` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender7QCamera14projectionTypeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender7QCamera16projectionMatrixEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender7QCamera3topEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender7QCamera4leftEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender7QCamera4lensEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender7QCamera5rightEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender7QCamera6bottomEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender7QCamera8exposureEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender7QCamera8farPlaneEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender7QCamera8positionEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender7QCamera8rotationEfRK9QVector3D` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender7QCamera8upVectorEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender7QCamera9nearPlaneEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender7QCamera9transformEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender7QEffect10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender7QEffect10parametersEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender7QEffect10techniquesEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender7QNoDraw10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender9QCullFace10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender9QCullFace4modeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender9QMaterial10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender9QMaterial10parametersEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender9QMaterial6effectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender9QSetFence10handleTypeEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender9QSetFence10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender9QSetFence6handleEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender9QViewport10metaObjectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender9QViewport14normalizedRectEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZNK10Qt3DRender9QViewport5gammaEv` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender10QAlphaTestE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender10QClipPlaneE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender10QColorMaskE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender10QDepthTestE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender10QDitheringE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender10QFilterKeyE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender10QFrontFaceE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender10QLineWidthE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender10QNoPickingE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender10QParameterE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender10QPickEventE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender10QPointSizeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender10QRayCasterE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender10QSpotLightE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender10QTechniqueE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender10QTexture1DE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender10QTexture2DE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender10QTexture3DE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender10QWaitFenceE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender11QCameraLensE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender11QDepthRangeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender11QPointLightE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender11QRasterModeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender11QRenderPassE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender11QShaderDataE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender11QSortPolicyE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender12QLayerFilterE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender12QNoDepthMaskE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender12QRenderStateE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender12QSceneLoaderE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender12QScissorTestE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender12QShaderImageE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender12QStencilMaskE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender12QStencilTestE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender13QClearBuffersE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender13QDebugOverlayE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender13QObjectPickerE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender13QPickingProxyE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender13QRenderAspectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender13QRenderTargetE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender13QTextureImageE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender14QAbstractLightE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender14QAlphaCoverageE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender14QBlendEquationE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender14QBufferCaptureE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender14QLevelOfDetailE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender14QMemoryBarrierE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender14QPickLineEventE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender14QPolygonOffsetE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender14QRenderCaptureE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender14QShaderProgramE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender14QTextureBufferE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender14QTextureLoaderE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender15QCameraSelectorE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender15QComputeCommandE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender15QFrameGraphNodeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender15QFrustumCullingE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender15QPickPointEventE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender15QRenderSettingsE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender15QRenderStateSetE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender15QSubtreeEnablerE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender15QTexture1DArrayE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender15QTexture2DArrayE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender15QTextureCubeMapE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender16QAbstractTextureE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender16QBlitFramebufferE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender16QDispatchComputeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender16QPickingSettingsE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender16QProximityFilterE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender16QScreenRayCasterE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender16QSeamlessCubemapE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender16QSharedGLTextureE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender16QTechniqueFilterE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender16QTextureWrapModeE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender17QDirectionalLightE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender17QEnvironmentLightE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender17QGeometryRendererE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender17QRenderPassFilterE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender17QStencilOperationE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender17QTextureRectangleE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender18QAbstractRayCasterE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender18QGraphicsApiFilterE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender18QPickTriangleEventE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender19QRenderCapabilitiesE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender19QRenderCaptureReplyE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender19QRenderTargetOutputE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender20QLevelOfDetailSwitchE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender20QPaintedTextureImageE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender20QTextureCubeMapArrayE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender21QAbstractTextureImageE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender21QRenderTargetSelectorE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender21QShaderProgramBuilderE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender21QStencilTestArgumentsE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender21QTexture2DMultisampleE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender22QRenderSurfaceSelectorE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender23PropertyReaderInterfaceE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender23QBlendEquationArgumentsE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender24QMultiSampleAntiAliasingE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender26QStencilOperationArgumentsE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender26QTexture2DMultisampleArrayE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender26QTextureImageDataGeneratorE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender5QMeshE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender6QLayerE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender7QCameraE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender7QEffectE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender7QNoDrawE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender9QCullFaceE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender9QMaterialE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender9QSetFenceE` | 1 | freecad |
| libQt63DRender.6.dylib | `__ZTIN10Qt3DRender9QViewportE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QBarSeries11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QBarSeries11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QBarSeries16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QBarSeriesC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QBarSeriesD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QChartView10wheelEventEP11QWheelEvent` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QChartView11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QChartView11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QChartView11resizeEventEP12QResizeEvent` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QChartView13setRubberBandERK6QFlagsINS_10RubberBandEE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QChartView14mouseMoveEventEP11QMouseEvent` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QChartView15mousePressEventEP11QMouseEvent` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QChartView16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QChartView17mouseReleaseEventEP11QMouseEvent` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QChartView8setChartEP6QChart` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QChartViewC2EP6QChartP7QWidget` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QChartViewC2EP7QWidget` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QChartViewD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QColorAxis11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QColorAxis11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QColorAxis11setGradientERK15QLinearGradient` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QColorAxis12setAutoRangeEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QColorAxis12setTickCountEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QColorAxis16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QColorAxis6setMaxEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QColorAxis6setMinEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QColorAxis7setSizeEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QColorAxis8setRangeEdd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QColorAxisC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QColorAxisD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QPieSeries10setPieSizeEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QPieSeries11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QPieSeries11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QPieSeries11setHoleSizeEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QPieSeries14setPieEndAngleEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QPieSeries16setLabelsVisibleEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QPieSeries16setPieStartAngleEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QPieSeries16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QPieSeries17setLabelsPositionEN9QPieSlice13LabelPositionE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QPieSeries19setVerticalPositionEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QPieSeries21setHorizontalPositionEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QPieSeries4takeEP9QPieSlice` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QPieSeries5clearEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QPieSeries6appendEP9QPieSlice` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QPieSeries6appendERK5QListIP9QPieSliceE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QPieSeries6appendERK7QStringd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QPieSeries6insertEiP9QPieSlice` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QPieSeries6removeEP9QPieSlice` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QPieSeriesC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QPieSeriesD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QPieSerieslsEP9QPieSlice` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QValueAxis11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QValueAxis11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QValueAxis11setTickTypeENS_8TickTypeE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QValueAxis12setTickCountEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QValueAxis13setTickAnchorEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QValueAxis14setLabelFormatERK7QString` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QValueAxis15setTickIntervalEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QValueAxis16applyNiceNumbersEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QValueAxis16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QValueAxis17setMinorTickCountEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QValueAxis6setMaxEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QValueAxis6setMinEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QValueAxis8setRangeEdd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QValueAxisC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN10QValueAxisD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN11QAreaSeries11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN11QAreaSeries11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN11QAreaSeries14setBorderColorERK6QColor` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN11QAreaSeries14setLowerSeriesEP11QLineSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN11QAreaSeries14setUpperSeriesEP11QLineSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN11QAreaSeries16setPointsVisibleEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN11QAreaSeries16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN11QAreaSeries18setPointLabelsFontERK5QFont` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN11QAreaSeries19setPointLabelsColorERK6QColor` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN11QAreaSeries20setPointLabelsFormatERK7QString` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN11QAreaSeries21setPointLabelsVisibleEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN11QAreaSeries22setPointLabelsClippingEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN11QAreaSeries6setPenERK4QPen` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN11QAreaSeries8setBrushERK6QBrush` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN11QAreaSeries8setColorERK6QColor` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN11QAreaSeriesC2EP11QLineSeriesS1_` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN11QAreaSeriesC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN11QAreaSeriesD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN11QLineSeries11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN11QLineSeries11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN11QLineSeries16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN11QLineSeriesC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN11QLineSeriesD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN11QPolarChart11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN11QPolarChart11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN11QPolarChart16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN11QPolarChart20axisPolarOrientationEP13QAbstractAxis` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN11QPolarChart7addAxisEP13QAbstractAxisNS_16PolarOrientationE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN11QPolarChartC2EP13QGraphicsItem6QFlagsIN2Qt10WindowTypeEE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN11QPolarChartD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis10setLinePenERK4QPen` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis10setReverseEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis10setVisibleEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis12setShadesPenERK4QPen` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis12setTitleFontERK5QFont` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis12setTitleTextERK7QString` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis13gridLineColorEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis13setLabelsFontERK5QFont` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis13setTitleBrushERK6QBrush` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis14setGridLinePenERK4QPen` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis14setLabelsAngleEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis14setLabelsBrushERK6QBrush` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis14setLabelsColorE6QColor` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis14setLineVisibleEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis14setShadesBrushERK6QBrush` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis14setShadesColorE6QColor` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis15setLinePenColorE6QColor` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis15setTitleVisibleEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis16setGridLineColorERK6QColor` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis16setLabelsVisibleEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis16setShadesVisibleEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis17setLabelsEditableEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis17setTruncateLabelsEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis18minorGridLineColorEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis18setGridLineVisibleEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis19setMinorGridLinePenERK4QPen` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis20setShadesBorderColorE6QColor` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis21setMinorGridLineColorERK6QColor` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis23setMinorGridLineVisibleEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis4hideEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis4showEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis6setMaxERK8QVariant` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis6setMinERK8QVariant` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxis8setRangeERK8QVariantS2_` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QAbstractAxisD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QCategoryAxis11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QCategoryAxis11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QCategoryAxis12replaceLabelERK7QStringS2_` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QCategoryAxis13setStartValueEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QCategoryAxis16categoriesLabelsEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QCategoryAxis16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QCategoryAxis17setLabelsPositionENS_18AxisLabelsPositionE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QCategoryAxis6appendERK7QStringd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QCategoryAxis6removeERK7QString` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QCategoryAxisC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QCategoryAxisD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QDateTimeAxis11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QDateTimeAxis11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QDateTimeAxis12setTickCountEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QDateTimeAxis16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QDateTimeAxis6setMaxE9QDateTime` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QDateTimeAxis6setMinE9QDateTime` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QDateTimeAxis8setRangeE9QDateTimeS0_` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QDateTimeAxis9setFormatE7QString` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QDateTimeAxisC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QDateTimeAxisD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QLegendMarker10setVisibleEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QLegendMarker11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QLegendMarker11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QLegendMarker13setLabelBrushERK6QBrush` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QLegendMarker16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QLegendMarker6setPenERK4QPen` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QLegendMarker7setFontERK5QFont` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QLegendMarker8setBrushERK6QBrush` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QLegendMarker8setLabelERK7QString` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QLegendMarker8setShapeEN7QLegend11MarkerShapeE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QLegendMarkerD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QLogValueAxis11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QLogValueAxis11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QLogValueAxis14setLabelFormatERK7QString` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QLogValueAxis16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QLogValueAxis17setMinorTickCountEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QLogValueAxis6setMaxEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QLogValueAxis6setMinEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QLogValueAxis7setBaseEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QLogValueAxis8setRangeEdd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QLogValueAxisC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QLogValueAxisD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QSplineSeries11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QSplineSeries11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QSplineSeries16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QSplineSeriesC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN13QSplineSeriesD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QBoxPlotSeries11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QBoxPlotSeries11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QBoxPlotSeries11setBoxWidthEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QBoxPlotSeries16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QBoxPlotSeries17boxOutlineVisibleEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QBoxPlotSeries20setBoxOutlineVisibleEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QBoxPlotSeries4takeEP7QBoxSet` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QBoxPlotSeries5clearEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QBoxPlotSeries6appendEP7QBoxSet` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QBoxPlotSeries6appendERK5QListIP7QBoxSetE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QBoxPlotSeries6insertEiP7QBoxSet` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QBoxPlotSeries6removeEP7QBoxSet` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QBoxPlotSeries6setPenERK4QPen` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QBoxPlotSeries8boxWidthEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QBoxPlotSeries8setBrushERK6QBrush` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QBoxPlotSeriesC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QBoxPlotSeriesD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QScatterSeries11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QScatterSeries11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QScatterSeries13setMarkerSizeEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QScatterSeries14setBorderColorERK6QColor` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QScatterSeries14setMarkerShapeENS_11MarkerShapeE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QScatterSeries16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QScatterSeries6setPenERK4QPen` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QScatterSeries8setBrushERK6QBrush` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QScatterSeries8setColorERK6QColor` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QScatterSeriesC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QScatterSeriesD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QXYModelMapper11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QXYModelMapper11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QXYModelMapper11setXSectionEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QXYModelMapper11setYSectionEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QXYModelMapper14setOrientationEN2Qt11OrientationE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QXYModelMapper16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QXYModelMapper8setCountEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QXYModelMapper8setFirstEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QXYModelMapper8setModelEP18QAbstractItemModel` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QXYModelMapper9setSeriesEP9QXYSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN14QXYModelMapperC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QAbstractSeries10attachAxisEP13QAbstractAxis` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QAbstractSeries10detachAxisEP13QAbstractAxis` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QAbstractSeries10setOpacityEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QAbstractSeries10setVisibleEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QAbstractSeries11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QAbstractSeries11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QAbstractSeries12attachedAxesEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QAbstractSeries12setUseOpenGLEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QAbstractSeries16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QAbstractSeries4hideEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QAbstractSeries4showEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QAbstractSeries7setNameERK7QString` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QAbstractSeriesD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QBarModelMapper11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QBarModelMapper11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QBarModelMapper14setOrientationEN2Qt11OrientationE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QBarModelMapper16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QBarModelMapper20setLastBarSetSectionEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QBarModelMapper21setFirstBarSetSectionEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QBarModelMapper8setCountEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QBarModelMapper8setFirstEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QBarModelMapper8setModelEP18QAbstractItemModel` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QBarModelMapper9setSeriesEP18QAbstractBarSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QBarModelMapperC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QCandlestickSet11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QCandlestickSet11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QCandlestickSet12setTimestampEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QCandlestickSet16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QCandlestickSet6setLowEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QCandlestickSet6setPenERK4QPen` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QCandlestickSet7setHighEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QCandlestickSet7setOpenEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QCandlestickSet8setBrushERK6QBrush` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QCandlestickSet8setCloseEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QCandlestickSetC2EdP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QCandlestickSetC2EdddddP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QCandlestickSetD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QHXYModelMapper11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QHXYModelMapper11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QHXYModelMapper14setColumnCountEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QHXYModelMapper14setFirstColumnEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QHXYModelMapper16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QHXYModelMapper7setXRowEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QHXYModelMapper7setYRowEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QHXYModelMapper8setModelEP18QAbstractItemModel` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QHXYModelMapper9setSeriesEP9QXYSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QHXYModelMapperC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QPieModelMapper11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QPieModelMapper11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QPieModelMapper14setOrientationEN2Qt11OrientationE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QPieModelMapper16setLabelsSectionEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QPieModelMapper16setValuesSectionEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QPieModelMapper16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QPieModelMapper8setCountEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QPieModelMapper8setFirstEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QPieModelMapper8setModelEP18QAbstractItemModel` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QPieModelMapper9setSeriesEP10QPieSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QPieModelMapperC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QVXYModelMapper10setXColumnEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QVXYModelMapper10setYColumnEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QVXYModelMapper11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QVXYModelMapper11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QVXYModelMapper11setFirstRowEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QVXYModelMapper11setRowCountEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QVXYModelMapper16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QVXYModelMapper8setModelEP18QAbstractItemModel` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QVXYModelMapper9setSeriesEP9QXYSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QVXYModelMapperC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QXYLegendMarker11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QXYLegendMarker11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QXYLegendMarker16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QXYLegendMarker6seriesEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QXYLegendMarkerC2EP9QXYSeriesP7QLegendP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN15QXYLegendMarkerD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QBarCategoryAxis10categoriesEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QBarCategoryAxis11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QBarCategoryAxis11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QBarCategoryAxis13setCategoriesERK5QListI7QStringE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QBarCategoryAxis16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QBarCategoryAxis5clearEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QBarCategoryAxis6appendERK5QListI7QStringE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QBarCategoryAxis6appendERK7QString` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QBarCategoryAxis6insertEiRK7QString` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QBarCategoryAxis6removeERK7QString` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QBarCategoryAxis6setMaxERK7QString` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QBarCategoryAxis6setMinERK7QString` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QBarCategoryAxis7replaceERK7QStringS2_` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QBarCategoryAxis8setRangeERK7QStringS2_` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QBarCategoryAxisC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QBarCategoryAxisD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QBarLegendMarker11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QBarLegendMarker11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QBarLegendMarker16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QBarLegendMarker6barsetEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QBarLegendMarker6seriesEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QBarLegendMarkerC2EP18QAbstractBarSeriesP7QBarSetP7QLegendP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QBarLegendMarkerD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QHBarModelMapper11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QHBarModelMapper11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QHBarModelMapper14setColumnCountEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QHBarModelMapper14setFirstColumnEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QHBarModelMapper16setLastBarSetRowEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QHBarModelMapper16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QHBarModelMapper17setFirstBarSetRowEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QHBarModelMapper8setModelEP18QAbstractItemModel` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QHBarModelMapper9setSeriesEP18QAbstractBarSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QHBarModelMapperC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QHPieModelMapper11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QHPieModelMapper11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QHPieModelMapper12setLabelsRowEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QHPieModelMapper12setValuesRowEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QHPieModelMapper14setColumnCountEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QHPieModelMapper14setFirstColumnEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QHPieModelMapper16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QHPieModelMapper8setModelEP18QAbstractItemModel` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QHPieModelMapper9setSeriesEP10QPieSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QHPieModelMapperC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QPieLegendMarker11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QPieLegendMarker11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QPieLegendMarker16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QPieLegendMarker5sliceEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QPieLegendMarker6seriesEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QPieLegendMarkerC2EP10QPieSeriesP9QPieSliceP7QLegendP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QPieLegendMarkerD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QVBarModelMapper11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QVBarModelMapper11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QVBarModelMapper11setFirstRowEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QVBarModelMapper11setRowCountEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QVBarModelMapper16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QVBarModelMapper19setLastBarSetColumnEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QVBarModelMapper20setFirstBarSetColumnEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QVBarModelMapper8setModelEP18QAbstractItemModel` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QVBarModelMapper9setSeriesEP18QAbstractBarSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QVBarModelMapperC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QVPieModelMapper11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QVPieModelMapper11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QVPieModelMapper11setFirstRowEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QVPieModelMapper11setRowCountEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QVPieModelMapper15setLabelsColumnEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QVPieModelMapper15setValuesColumnEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QVPieModelMapper16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QVPieModelMapper8setModelEP18QAbstractItemModel` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QVPieModelMapper9setSeriesEP10QPieSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN16QVPieModelMapperC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN17QAreaLegendMarker11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN17QAreaLegendMarker11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN17QAreaLegendMarker16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN17QAreaLegendMarker6seriesEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN17QAreaLegendMarkerC2EP11QAreaSeriesP7QLegendP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN17QAreaLegendMarkerD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN17QPercentBarSeries11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN17QPercentBarSeries11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN17QPercentBarSeries16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN17QPercentBarSeriesC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN17QPercentBarSeriesD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN17QStackedBarSeries11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN17QStackedBarSeries11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN17QStackedBarSeries16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN17QStackedBarSeriesC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN17QStackedBarSeriesD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QAbstractBarSeries11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QAbstractBarSeries11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QAbstractBarSeries11setBarWidthEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QAbstractBarSeries14setLabelsAngleEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QAbstractBarSeries15setLabelsFormatERK7QString` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QAbstractBarSeries16setLabelsVisibleEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QAbstractBarSeries16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QAbstractBarSeries17setLabelsPositionENS_14LabelsPositionE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QAbstractBarSeries18setLabelsPrecisionEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QAbstractBarSeries4takeEP7QBarSet` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QAbstractBarSeries5clearEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QAbstractBarSeries6appendEP7QBarSet` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QAbstractBarSeries6appendERK5QListIP7QBarSetE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QAbstractBarSeries6insertEiP7QBarSet` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QAbstractBarSeries6removeEP7QBarSet` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QAbstractBarSeriesD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QCandlestickSeries11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QCandlestickSeries11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QCandlestickSeries12setBodyWidthEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QCandlestickSeries12setCapsWidthEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QCandlestickSeries14setCapsVisibleEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QCandlestickSeries16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QCandlestickSeries18setDecreasingColorERK6QColor` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QCandlestickSeries18setIncreasingColorERK6QColor` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QCandlestickSeries21setBodyOutlineVisibleEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QCandlestickSeries21setMaximumColumnWidthEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QCandlestickSeries21setMinimumColumnWidthEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QCandlestickSeries4takeEP15QCandlestickSet` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QCandlestickSeries5clearEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QCandlestickSeries6appendEP15QCandlestickSet` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QCandlestickSeries6appendERK5QListIP15QCandlestickSetE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QCandlestickSeries6insertEiP15QCandlestickSet` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QCandlestickSeries6removeEP15QCandlestickSet` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QCandlestickSeries6removeERK5QListIP15QCandlestickSetE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QCandlestickSeries6setPenERK4QPen` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QCandlestickSeries8setBrushERK6QBrush` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QCandlestickSeriesC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN18QCandlestickSeriesD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN19QBoxPlotModelMapper11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN19QBoxPlotModelMapper11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN19QBoxPlotModelMapper14setOrientationEN2Qt11OrientationE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN19QBoxPlotModelMapper16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN19QBoxPlotModelMapper20setLastBoxSetSectionEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN19QBoxPlotModelMapper21setFirstBoxSetSectionEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN19QBoxPlotModelMapper8setCountEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN19QBoxPlotModelMapper8setFirstEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN19QBoxPlotModelMapper8setModelEP18QAbstractItemModel` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN19QBoxPlotModelMapper9setSeriesEP14QBoxPlotSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN19QBoxPlotModelMapperC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN20QBoxPlotLegendMarker11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN20QBoxPlotLegendMarker11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN20QBoxPlotLegendMarker16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN20QBoxPlotLegendMarker6seriesEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN20QBoxPlotLegendMarkerC2EP14QBoxPlotSeriesP7QLegendP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN20QBoxPlotLegendMarkerD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN20QHBoxPlotModelMapper11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN20QHBoxPlotModelMapper11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN20QHBoxPlotModelMapper14setColumnCountEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN20QHBoxPlotModelMapper14setFirstColumnEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN20QHBoxPlotModelMapper16setLastBoxSetRowEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN20QHBoxPlotModelMapper16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN20QHBoxPlotModelMapper17setFirstBoxSetRowEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN20QHBoxPlotModelMapper8setModelEP18QAbstractItemModel` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN20QHBoxPlotModelMapper9setSeriesEP14QBoxPlotSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN20QHBoxPlotModelMapperC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN20QHorizontalBarSeries11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN20QHorizontalBarSeries11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN20QHorizontalBarSeries16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN20QHorizontalBarSeriesC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN20QHorizontalBarSeriesD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN20QVBoxPlotModelMapper11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN20QVBoxPlotModelMapper11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN20QVBoxPlotModelMapper11setFirstRowEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN20QVBoxPlotModelMapper11setRowCountEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN20QVBoxPlotModelMapper16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN20QVBoxPlotModelMapper19setLastBoxSetColumnEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN20QVBoxPlotModelMapper20setFirstBoxSetColumnEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN20QVBoxPlotModelMapper8setModelEP18QAbstractItemModel` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN20QVBoxPlotModelMapper9setSeriesEP14QBoxPlotSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN20QVBoxPlotModelMapperC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN23QCandlestickModelMapper11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN23QCandlestickModelMapper11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN23QCandlestickModelMapper12setTimestampEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN23QCandlestickModelMapper16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN23QCandlestickModelMapper17setLastSetSectionEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN23QCandlestickModelMapper18setFirstSetSectionEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN23QCandlestickModelMapper6setLowEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN23QCandlestickModelMapper7setHighEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN23QCandlestickModelMapper7setOpenEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN23QCandlestickModelMapper8setCloseEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN23QCandlestickModelMapper8setModelEP18QAbstractItemModel` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN23QCandlestickModelMapper9setSeriesEP18QCandlestickSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN23QCandlestickModelMapperC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN24QCandlestickLegendMarker11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN24QCandlestickLegendMarker11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN24QCandlestickLegendMarker16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN24QCandlestickLegendMarker4typeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN24QCandlestickLegendMarker6seriesEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN24QCandlestickLegendMarkerC2EP18QCandlestickSeriesP7QLegendP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN24QCandlestickLegendMarkerD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN24QHCandlestickModelMapper11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN24QHCandlestickModelMapper11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN24QHCandlestickModelMapper12setLowColumnEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN24QHCandlestickModelMapper13setHighColumnEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN24QHCandlestickModelMapper13setLastSetRowEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN24QHCandlestickModelMapper13setOpenColumnEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN24QHCandlestickModelMapper14setCloseColumnEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN24QHCandlestickModelMapper14setFirstSetRowEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN24QHCandlestickModelMapper16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN24QHCandlestickModelMapper18setTimestampColumnEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN24QHCandlestickModelMapperC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN24QVCandlestickModelMapper10setHighRowEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN24QVCandlestickModelMapper10setOpenRowEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN24QVCandlestickModelMapper11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN24QVCandlestickModelMapper11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN24QVCandlestickModelMapper11setCloseRowEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN24QVCandlestickModelMapper15setTimestampRowEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN24QVCandlestickModelMapper16setLastSetColumnEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN24QVCandlestickModelMapper16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN24QVCandlestickModelMapper17setFirstSetColumnEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN24QVCandlestickModelMapper9setLowRowEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN24QVCandlestickModelMapperC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN27QHorizontalPercentBarSeries11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN27QHorizontalPercentBarSeries11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN27QHorizontalPercentBarSeries16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN27QHorizontalPercentBarSeriesC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN27QHorizontalPercentBarSeriesD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN27QHorizontalStackedBarSeries11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN27QHorizontalStackedBarSeries11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN27QHorizontalStackedBarSeries16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN27QHorizontalStackedBarSeriesC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN27QHorizontalStackedBarSeriesD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart10mapToValueERK7QPointFP15QAbstractSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart10removeAxisEP13QAbstractAxis` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart10setMarginsERK8QMargins` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart11setPlotAreaERK6QRectF` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart12removeSeriesEP15QAbstractSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart12setTitleFontERK5QFont` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart13mapToPositionERK7QPointFP15QAbstractSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart13setTitleBrushERK6QBrush` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart15removeAllSeriesEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart16setBackgroundPenERK4QPen` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart17createDefaultAxesEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart18setBackgroundBrushERK6QBrush` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart18setLocalizeNumbersEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart19setAnimationOptionsE6QFlagsINS_15AnimationOptionEE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart20setAnimationDurationEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart20setBackgroundVisibleEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart20setDropShadowEnabledEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart22setBackgroundRoundnessEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart23setAnimationEasingCurveERK12QEasingCurve` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart24setPlotAreaBackgroundPenERK4QPen` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart26setPlotAreaBackgroundBrushERK6QBrush` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart28setPlotAreaBackgroundVisibleEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart4zoomEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart6scrollEdd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart6zoomInERK6QRectF` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart6zoomInEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart7addAxisEP13QAbstractAxis6QFlagsIN2Qt13AlignmentFlagEE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart7zoomOutEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart8isZoomedEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart8setAxisXEP13QAbstractAxisP15QAbstractSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart8setAxisYEP13QAbstractAxisP15QAbstractSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart8setThemeENS_10ChartThemeE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart8setTitleERK7QString` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart9addSeriesEP15QAbstractSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart9setLocaleERK7QLocale` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChart9zoomResetEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChartC2ENS_9ChartTypeEP13QGraphicsItem6QFlagsIN2Qt10WindowTypeEE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChartC2EP13QGraphicsItem6QFlagsIN2Qt10WindowTypeEE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN6QChartD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBarSet10labelColorEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBarSet10selectBarsERK5QListIiE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBarSet11borderColorEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBarSet11deselectBarEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBarSet11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBarSet11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBarSet12deselectBarsERK5QListIiE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBarSet12setLabelFontERK5QFont` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBarSet13selectAllBarsEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBarSet13setLabelBrushERK6QBrush` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBarSet13setLabelColorE6QColor` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBarSet14setBarSelectedEib` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBarSet14setBorderColorE6QColor` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBarSet15deselectAllBarsEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBarSet15toggleSelectionERK5QListIiE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBarSet16setSelectedColorERK6QColor` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBarSet16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBarSet5colorEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBarSet6appendERK5QListIdE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBarSet6appendEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBarSet6insertEid` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBarSet6removeEii` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBarSet6setPenERK4QPen` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBarSet7replaceEid` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBarSet8setBrushERK6QBrush` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBarSet8setColorE6QColor` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBarSet8setLabelE7QString` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBarSet9selectBarEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBarSetC2E7QStringP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBarSetD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBarSetlsERKd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBoxSet11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBoxSet11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBoxSet16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBoxSet5clearEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBoxSet6appendERK5QListIdE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBoxSet6appendEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBoxSet6setPenERK4QPen` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBoxSet8setBrushERK6QBrush` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBoxSet8setLabelE7QString` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBoxSet8setValueEid` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBoxSetC2E7QStringP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBoxSetC2Eddddd7QStringP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBoxSetD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QBoxSetlsERKd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QLegend11borderColorEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QLegend11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QLegend11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QLegend12setAlignmentE6QFlagsIN2Qt13AlignmentFlagEE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QLegend13attachToChartEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QLegend13setLabelBrushERK6QBrush` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QLegend13setLabelColorE6QColor` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QLegend14reverseMarkersEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QLegend14setBorderColorE6QColor` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QLegend14setInteractiveEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QLegend14setMarkerShapeENS_11MarkerShapeE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QLegend15detachFromChartEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QLegend15setShowToolTipsEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QLegend16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QLegend17isAttachedToChartEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QLegend17setReverseMarkersEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QLegend20setBackgroundVisibleEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QLegend5colorEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QLegend5paintEP8QPainterPK24QStyleOptionGraphicsItemP7QWidget` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QLegend6setPenERK4QPen` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QLegend7setFontERK5QFont` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QLegend8setBrushERK6QBrush` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QLegend8setColorE6QColor` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QLegend9hideEventEP10QHideEvent` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QLegend9showEventEP10QShowEvent` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN7QLegendD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QPieSlice10labelColorEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QPieSlice11borderColorEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QPieSlice11borderWidthEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QPieSlice11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QPieSlice11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QPieSlice11setExplodedEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QPieSlice12setLabelFontERK5QFont` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QPieSlice13labelPositionEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QPieSlice13setLabelBrushERK6QBrush` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QPieSlice13setLabelColorE6QColor` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QPieSlice14setBorderColorE6QColor` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QPieSlice14setBorderWidthEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QPieSlice15setLabelVisibleEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QPieSlice16setLabelPositionENS_13LabelPositionE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QPieSlice16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QPieSlice23setLabelArmLengthFactorEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QPieSlice24setExplodeDistanceFactorEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QPieSlice5colorEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QPieSlice6setPenERK4QPen` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QPieSlice8setBrushERK6QBrush` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QPieSlice8setColorE6QColor` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QPieSlice8setLabelE7QString` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QPieSlice8setValueEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QPieSliceC2E7QStringdP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QPieSliceC2EP7QObject` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QPieSliceD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries11qt_metacastEPKc` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries11selectPointEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries12removePointsEii` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries12selectPointsERK5QListIiE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries13deselectPointEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries13setMarkerSizeEd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries14deselectPointsERK5QListIiE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries14setLightMarkerERK6QImage` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries15isPointSelectedEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries15selectAllPointsEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries15toggleSelectionERK5QListIiE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries16setPointSelectedEib` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries16setPointsVisibleEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries16setSelectedColorERK6QColor` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries16staticMetaObjectE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries17deselectAllPointsEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries17setBestFitLinePenERK4QPen` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries18setPointLabelsFontERK5QFont` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries19setBestFitLineColorERK6QColor` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries19setPointLabelsColorERK6QColor` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries20setPointLabelsFormatERK7QString` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries21setBestFitLineVisibleEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries21setPointConfigurationEiNS_18PointConfigurationERK8QVariant` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries21setPointConfigurationEiRK5QHashINS_18PointConfigurationE8QVariantE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries21setPointLabelsVisibleEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries22setPointLabelsClippingEb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries22setPointsConfigurationERK5QHashIiS0_INS_18PointConfigurationE8QVariantEE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries22setSelectedLightMarkerERK6QImage` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries23clearPointConfigurationEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries23clearPointConfigurationEiNS_18PointConfigurationE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries24clearPointsConfigurationENS_18PointConfigurationE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries24clearPointsConfigurationEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries5clearEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries6appendERK5QListI7QPointFE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries6appendERK7QPointF` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries6appendEdd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries6insertEiRK7QPointF` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries6removeERK7QPointF` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries6removeEdd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries6removeEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries6setPenERK4QPen` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries6sizeByERK5QListIdEdd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries7colorByERK5QListIdERK15QLinearGradient` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries7replaceERK5QListI7QPointFE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries7replaceERK7QPointFS2_` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries7replaceEdddd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries7replaceEiRK7QPointF` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries7replaceEidd` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries8setBrushERK6QBrush` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeries8setColorERK6QColor` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSeriesD2Ev` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSerieslsERK5QListI7QPointFE` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZN9QXYSerieslsERK7QPointF` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QBarSeries10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QBarSeries4typeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QChartView10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QChartView10rubberBandEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QChartView5chartEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QColorAxis10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QColorAxis3maxEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QColorAxis3minEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QColorAxis4sizeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QColorAxis4typeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QColorAxis8gradientEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QColorAxis9autoRangeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QColorAxis9tickCountEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QPieSeries10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QPieSeries11pieEndAngleEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QPieSeries13pieStartAngleEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QPieSeries16verticalPositionEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QPieSeries18horizontalPositionEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QPieSeries3sumEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QPieSeries4typeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QPieSeries5countEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QPieSeries6slicesEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QPieSeries7isEmptyEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QPieSeries7pieSizeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QPieSeries8holeSizeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QValueAxis10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QValueAxis10tickAnchorEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QValueAxis11labelFormatEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QValueAxis12tickIntervalEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QValueAxis14minorTickCountEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QValueAxis3maxEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QValueAxis3minEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QValueAxis4typeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QValueAxis8tickTypeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK10QValueAxis9tickCountEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK11QAreaSeries10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK11QAreaSeries11borderColorEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK11QAreaSeries11lowerSeriesEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK11QAreaSeries11upperSeriesEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK11QAreaSeries13pointsVisibleEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK11QAreaSeries15pointLabelsFontEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK11QAreaSeries16pointLabelsColorEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK11QAreaSeries17pointLabelsFormatEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK11QAreaSeries18pointLabelsVisibleEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK11QAreaSeries19pointLabelsClippingEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK11QAreaSeries3penEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK11QAreaSeries4typeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK11QAreaSeries5brushEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK11QAreaSeries5colorEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK11QLineSeries10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK11QLineSeries4typeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK11QPolarChart10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QAbstractAxis10labelsFontEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QAbstractAxis10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QAbstractAxis10titleBrushEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QAbstractAxis11gridLinePenEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QAbstractAxis11labelsAngleEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QAbstractAxis11labelsBrushEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QAbstractAxis11labelsColorEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QAbstractAxis11orientationEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QAbstractAxis11shadesBrushEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QAbstractAxis11shadesColorEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QAbstractAxis12linePenColorEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QAbstractAxis13isLineVisibleEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QAbstractAxis13labelsVisibleEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QAbstractAxis13shadesVisibleEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QAbstractAxis14isTitleVisibleEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QAbstractAxis14labelsEditableEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QAbstractAxis14truncateLabelsEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QAbstractAxis15labelsTruncatedEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QAbstractAxis16minorGridLinePenEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QAbstractAxis17isGridLineVisibleEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QAbstractAxis17shadesBorderColorEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QAbstractAxis22isMinorGridLineVisibleEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QAbstractAxis7linePenEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QAbstractAxis9alignmentEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QAbstractAxis9isReverseEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QAbstractAxis9isVisibleEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QAbstractAxis9shadesPenEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QAbstractAxis9titleFontEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QAbstractAxis9titleTextEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QCategoryAxis10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QCategoryAxis10startValueERK7QString` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QCategoryAxis14labelsPositionEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QCategoryAxis4typeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QCategoryAxis5countEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QCategoryAxis8endValueERK7QString` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QDateTimeAxis10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QDateTimeAxis3maxEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QDateTimeAxis3minEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QDateTimeAxis4typeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QDateTimeAxis6formatEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QDateTimeAxis9tickCountEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QLegendMarker10labelBrushEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QLegendMarker10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QLegendMarker3penEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QLegendMarker4fontEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QLegendMarker5brushEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QLegendMarker5labelEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QLegendMarker5shapeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QLegendMarker9isVisibleEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QLogValueAxis10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QLogValueAxis11labelFormatEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QLogValueAxis14minorTickCountEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QLogValueAxis3maxEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QLogValueAxis3minEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QLogValueAxis4baseEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QLogValueAxis4typeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QLogValueAxis9tickCountEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QSplineSeries10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK13QSplineSeries4typeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK14QBoxPlotSeries10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK14QBoxPlotSeries3penEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK14QBoxPlotSeries4typeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK14QBoxPlotSeries5brushEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK14QBoxPlotSeries5countEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK14QBoxPlotSeries7boxSetsEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK14QScatterSeries10markerSizeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK14QScatterSeries10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK14QScatterSeries11borderColorEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK14QScatterSeries11markerShapeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK14QScatterSeries4typeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK14QScatterSeries5brushEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK14QScatterSeries5colorEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK14QXYModelMapper10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK14QXYModelMapper11orientationEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK14QXYModelMapper5countEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK14QXYModelMapper5firstEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK14QXYModelMapper5modelEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK14QXYModelMapper6seriesEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK14QXYModelMapper8xSectionEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK14QXYModelMapper8ySectionEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QAbstractSeries10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QAbstractSeries4nameEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QAbstractSeries5chartEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QAbstractSeries7opacityEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QAbstractSeries9isVisibleEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QAbstractSeries9useOpenGLEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QBarModelMapper10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QBarModelMapper11orientationEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QBarModelMapper17lastBarSetSectionEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QBarModelMapper18firstBarSetSectionEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QBarModelMapper5countEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QBarModelMapper5firstEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QBarModelMapper5modelEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QBarModelMapper6seriesEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QCandlestickSet10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QCandlestickSet3lowEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QCandlestickSet3penEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QCandlestickSet4highEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QCandlestickSet4openEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QCandlestickSet5brushEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QCandlestickSet5closeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QCandlestickSet9timestampEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QHXYModelMapper10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QHXYModelMapper11columnCountEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QHXYModelMapper11firstColumnEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QHXYModelMapper4xRowEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QHXYModelMapper4yRowEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QHXYModelMapper5modelEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QHXYModelMapper6seriesEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QPieModelMapper10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QPieModelMapper11orientationEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QPieModelMapper13labelsSectionEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QPieModelMapper13valuesSectionEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QPieModelMapper5countEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QPieModelMapper5firstEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QPieModelMapper5modelEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QPieModelMapper6seriesEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QVXYModelMapper10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QVXYModelMapper5modelEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QVXYModelMapper6seriesEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QVXYModelMapper7xColumnEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QVXYModelMapper7yColumnEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QVXYModelMapper8firstRowEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QVXYModelMapper8rowCountEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK15QXYLegendMarker10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QBarCategoryAxis10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QBarCategoryAxis2atEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QBarCategoryAxis3maxEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QBarCategoryAxis3minEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QBarCategoryAxis4typeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QBarCategoryAxis5countEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QBarLegendMarker10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QHBarModelMapper10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QHBarModelMapper11columnCountEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QHBarModelMapper11firstColumnEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QHBarModelMapper13lastBarSetRowEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QHBarModelMapper14firstBarSetRowEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QHBarModelMapper5modelEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QHBarModelMapper6seriesEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QHPieModelMapper10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QHPieModelMapper11columnCountEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QHPieModelMapper11firstColumnEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QHPieModelMapper5modelEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QHPieModelMapper6seriesEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QHPieModelMapper9labelsRowEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QHPieModelMapper9valuesRowEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QPieLegendMarker10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QVBarModelMapper10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QVBarModelMapper16lastBarSetColumnEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QVBarModelMapper17firstBarSetColumnEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QVBarModelMapper5modelEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QVBarModelMapper6seriesEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QVBarModelMapper8firstRowEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QVBarModelMapper8rowCountEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QVPieModelMapper10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QVPieModelMapper12labelsColumnEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QVPieModelMapper12valuesColumnEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QVPieModelMapper5modelEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QVPieModelMapper6seriesEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QVPieModelMapper8firstRowEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK16QVPieModelMapper8rowCountEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK17QAreaLegendMarker10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK17QPercentBarSeries10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK17QPercentBarSeries4typeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK17QStackedBarSeries10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK17QStackedBarSeries4typeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK18QAbstractBarSeries10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK18QAbstractBarSeries11labelsAngleEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK18QAbstractBarSeries12labelsFormatEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK18QAbstractBarSeries14labelsPositionEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK18QAbstractBarSeries15isLabelsVisibleEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK18QAbstractBarSeries15labelsPrecisionEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK18QAbstractBarSeries5countEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK18QAbstractBarSeries7barSetsEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK18QAbstractBarSeries8barWidthEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK18QCandlestickSeries10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK18QCandlestickSeries11capsVisibleEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK18QCandlestickSeries15decreasingColorEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK18QCandlestickSeries15increasingColorEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK18QCandlestickSeries18bodyOutlineVisibleEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK18QCandlestickSeries18maximumColumnWidthEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK18QCandlestickSeries18minimumColumnWidthEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK18QCandlestickSeries3penEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK18QCandlestickSeries4setsEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK18QCandlestickSeries4typeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK18QCandlestickSeries5brushEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK18QCandlestickSeries5countEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK18QCandlestickSeries9bodyWidthEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK18QCandlestickSeries9capsWidthEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK19QBoxPlotModelMapper10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK19QBoxPlotModelMapper11orientationEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK19QBoxPlotModelMapper17lastBoxSetSectionEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK19QBoxPlotModelMapper18firstBoxSetSectionEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK19QBoxPlotModelMapper5countEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK19QBoxPlotModelMapper5firstEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK19QBoxPlotModelMapper5modelEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK19QBoxPlotModelMapper6seriesEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK20QBoxPlotLegendMarker10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK20QHBoxPlotModelMapper10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK20QHBoxPlotModelMapper11columnCountEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK20QHBoxPlotModelMapper11firstColumnEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK20QHBoxPlotModelMapper13lastBoxSetRowEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK20QHBoxPlotModelMapper14firstBoxSetRowEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK20QHBoxPlotModelMapper5modelEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK20QHBoxPlotModelMapper6seriesEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK20QHorizontalBarSeries10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK20QHorizontalBarSeries4typeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK20QVBoxPlotModelMapper10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK20QVBoxPlotModelMapper16lastBoxSetColumnEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK20QVBoxPlotModelMapper17firstBoxSetColumnEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK20QVBoxPlotModelMapper5modelEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK20QVBoxPlotModelMapper6seriesEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK20QVBoxPlotModelMapper8firstRowEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK20QVBoxPlotModelMapper8rowCountEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK23QCandlestickModelMapper10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK23QCandlestickModelMapper14lastSetSectionEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK23QCandlestickModelMapper15firstSetSectionEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK23QCandlestickModelMapper3lowEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK23QCandlestickModelMapper4highEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK23QCandlestickModelMapper4openEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK23QCandlestickModelMapper5closeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK23QCandlestickModelMapper5modelEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK23QCandlestickModelMapper6seriesEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK23QCandlestickModelMapper9timestampEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK24QCandlestickLegendMarker10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK24QHCandlestickModelMapper10highColumnEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK24QHCandlestickModelMapper10lastSetRowEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK24QHCandlestickModelMapper10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK24QHCandlestickModelMapper10openColumnEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK24QHCandlestickModelMapper11closeColumnEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK24QHCandlestickModelMapper11firstSetRowEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK24QHCandlestickModelMapper11orientationEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK24QHCandlestickModelMapper15timestampColumnEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK24QHCandlestickModelMapper9lowColumnEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK24QVCandlestickModelMapper10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK24QVCandlestickModelMapper11orientationEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK24QVCandlestickModelMapper12timestampRowEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK24QVCandlestickModelMapper13lastSetColumnEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK24QVCandlestickModelMapper14firstSetColumnEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK24QVCandlestickModelMapper6lowRowEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK24QVCandlestickModelMapper7highRowEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK24QVCandlestickModelMapper7openRowEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK24QVCandlestickModelMapper8closeRowEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK27QHorizontalPercentBarSeries10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK27QHorizontalPercentBarSeries4typeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK27QHorizontalStackedBarSeries10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK27QHorizontalStackedBarSeries4typeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK6QChart10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK6QChart10titleBrushEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK6QChart13backgroundPenEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK6QChart15backgroundBrushEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK6QChart15localizeNumbersEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK6QChart16animationOptionsEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK6QChart17animationDurationEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK6QChart19backgroundRoundnessEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK6QChart19isBackgroundVisibleEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK6QChart19isDropShadowEnabledEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK6QChart20animationEasingCurveEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK6QChart21plotAreaBackgroundPenEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK6QChart23plotAreaBackgroundBrushEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK6QChart27isPlotAreaBackgroundVisibleEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK6QChart4axesE6QFlagsIN2Qt11OrientationEEP15QAbstractSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK6QChart5axisXEP15QAbstractSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK6QChart5axisYEP15QAbstractSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK6QChart5themeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK6QChart5titleEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK6QChart6legendEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK6QChart6localeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK6QChart6seriesEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK6QChart7marginsEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK6QChart8plotAreaEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK6QChart9chartTypeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK6QChart9titleFontEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK7QBarSet10labelBrushEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK7QBarSet10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK7QBarSet12selectedBarsEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK7QBarSet13isBarSelectedEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK7QBarSet13selectedColorEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK7QBarSet2atEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK7QBarSet3penEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK7QBarSet3sumEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK7QBarSet5brushEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK7QBarSet5countEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK7QBarSet5labelEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK7QBarSet9labelFontEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK7QBoxSet10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK7QBoxSet2atEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK7QBoxSet3penEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK7QBoxSet5brushEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK7QBoxSet5countEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK7QBoxSet5labelEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK7QLegend10labelBrushEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK7QLegend10labelColorEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK7QLegend10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK7QLegend11markerShapeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK7QLegend12showToolTipsEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK7QLegend13isInteractiveEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK7QLegend19isBackgroundVisibleEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK7QLegend3penEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK7QLegend4fontEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK7QLegend5brushEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK7QLegend7markersEP15QAbstractSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK7QLegend9alignmentEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QPieSlice10isExplodedEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QPieSlice10labelBrushEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QPieSlice10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QPieSlice10percentageEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QPieSlice10startAngleEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QPieSlice14isLabelVisibleEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QPieSlice20labelArmLengthFactorEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QPieSlice21explodeDistanceFactorEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QPieSlice3penEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QPieSlice5brushEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QPieSlice5labelEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QPieSlice5valueEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QPieSlice6seriesEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QPieSlice9angleSpanEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QPieSlice9labelFontEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QXYSeries10markerSizeEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QXYSeries10metaObjectEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QXYSeries11lightMarkerEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QXYSeries12pointsVectorEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QXYSeries13pointsVisibleEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QXYSeries13selectedColorEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QXYSeries14bestFitLinePenEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QXYSeries14selectedPointsEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QXYSeries15pointLabelsFontEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QXYSeries16bestFitLineColorEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QXYSeries16pointLabelsColorEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QXYSeries17pointLabelsFormatEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QXYSeries18bestFitLineVisibleEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QXYSeries18pointConfigurationEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QXYSeries18pointLabelsVisibleEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QXYSeries19bestFitLineEquationERb` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QXYSeries19pointLabelsClippingEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QXYSeries19pointsConfigurationEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QXYSeries19selectedLightMarkerEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QXYSeries2atEi` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QXYSeries3penEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QXYSeries5brushEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QXYSeries5colorEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QXYSeries5countEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZNK9QXYSeries6pointsEv` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI10QBarSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI10QChartView` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI10QColorAxis` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI10QPieSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI10QValueAxis` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI11QAreaSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI11QLineSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI11QPolarChart` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI13QAbstractAxis` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI13QCategoryAxis` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI13QDateTimeAxis` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI13QLegendMarker` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI13QLogValueAxis` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI13QSplineSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI14QBoxPlotSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI14QScatterSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI14QXYModelMapper` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI15QAbstractSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI15QBarModelMapper` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI15QCandlestickSet` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI15QHXYModelMapper` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI15QPieModelMapper` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI15QVXYModelMapper` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI15QXYLegendMarker` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI16QBarCategoryAxis` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI16QBarLegendMarker` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI16QHBarModelMapper` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI16QHPieModelMapper` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI16QPieLegendMarker` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI16QVBarModelMapper` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI16QVPieModelMapper` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI17QAreaLegendMarker` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI17QPercentBarSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI17QStackedBarSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI18QAbstractBarSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI18QCandlestickSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI19QBoxPlotModelMapper` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI20QBoxPlotLegendMarker` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI20QHBoxPlotModelMapper` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI20QHorizontalBarSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI20QVBoxPlotModelMapper` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI23QCandlestickModelMapper` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI24QCandlestickLegendMarker` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI24QHCandlestickModelMapper` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI24QVCandlestickModelMapper` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI27QHorizontalPercentBarSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI27QHorizontalStackedBarSeries` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI6QChart` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI7QBarSet` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI7QBoxSet` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI7QLegend` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI9QPieSlice` | 1 | freecad |
| libQt6Charts.6.dylib | `__ZTI9QXYSeries` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN10Q3DScatter11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN10Q3DScatter11qt_metacastEPKc` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN10Q3DScatter11releaseAxisEP12QValue3DAxis` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN10Q3DScatter12removeSeriesEP16QScatter3DSeries` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN10Q3DScatter16staticMetaObjectE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN10Q3DScatter7addAxisEP12QValue3DAxis` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN10Q3DScatter8setAxisXEP12QValue3DAxis` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN10Q3DScatter8setAxisYEP12QValue3DAxis` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN10Q3DScatter8setAxisZEP12QValue3DAxis` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN10Q3DScatter9addSeriesEP16QScatter3DSeries` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN10Q3DScatterC2EPK14QSurfaceFormatP7QWindow` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN10Q3DScatterD2Ev` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN10Q3DSurface11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN10Q3DSurface11qt_metacastEPKc` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN10Q3DSurface11releaseAxisEP12QValue3DAxis` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN10Q3DSurface12removeSeriesEP16QSurface3DSeries` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN10Q3DSurface16staticMetaObjectE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN10Q3DSurface21setFlipHorizontalGridEb` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN10Q3DSurface7addAxisEP12QValue3DAxis` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN10Q3DSurface8setAxisXEP12QValue3DAxis` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN10Q3DSurface8setAxisYEP12QValue3DAxis` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN10Q3DSurface8setAxisZEP12QValue3DAxis` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN10Q3DSurface9addSeriesEP16QSurface3DSeries` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN10Q3DSurfaceC2EPK14QSurfaceFormatP7QWindow` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN10Q3DSurfaceD2Ev` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN12QBar3DSeries14setSelectedBarERK6QPoint` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN12QBarDataItem15createExtraDataEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN12QBarDataItemC1ERKS_` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN12QBarDataItemC1Ef` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN12QBarDataItemC1Eff` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN12QBarDataItemC1Ev` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN12QBarDataItemD1Ev` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN12QBarDataItemaSERKS_` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN12QValue3DAxis15setSegmentCountEi` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN12QValue3DAxis18setSubSegmentCountEi` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN13QBarDataProxy10insertRowsEiRK5QListIPS0_I12QBarDataItemEE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN13QBarDataProxy10insertRowsEiRK5QListIPS0_I12QBarDataItemEERKS0_I7QStringE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN13QBarDataProxy10removeRowsEiib` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN13QBarDataProxy10resetArrayEP5QListIPS0_I12QBarDataItemEE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN13QBarDataProxy10resetArrayEP5QListIPS0_I12QBarDataItemEERKS0_I7QStringES9_` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN13QBarDataProxy12setRowLabelsERK5QListI7QStringE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN13QBarDataProxy15setColumnLabelsERK5QListI7QStringE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN13QBarDataProxy6addRowEP5QListI12QBarDataItemE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN13QBarDataProxy6addRowEP5QListI12QBarDataItemERK7QString` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN13QBarDataProxy6setRowEiP5QListI12QBarDataItemE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN13QBarDataProxy6setRowEiP5QListI12QBarDataItemERK7QString` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN13QBarDataProxy7addRowsERK5QListIPS0_I12QBarDataItemEE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN13QBarDataProxy7addRowsERK5QListIPS0_I12QBarDataItemEERKS0_I7QStringE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN13QBarDataProxy7setItemERK6QPointRK12QBarDataItem` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN13QBarDataProxy7setItemEiiRK12QBarDataItem` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN13QBarDataProxy7setRowsEiRK5QListIPS0_I12QBarDataItemEE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN13QBarDataProxy7setRowsEiRK5QListIPS0_I12QBarDataItemEERKS0_I7QStringE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN13QBarDataProxy9insertRowEiP5QListI12QBarDataItemE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN13QBarDataProxy9insertRowEiP5QListI12QBarDataItemERK7QString` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN13QCustom3DItem10setScalingERK9QVector3D` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN13QCustom3DItem11setPositionERK9QVector3D` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN13QCustom3DItem23setRotationAxisAndAngleERK9QVector3Df` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN13QCustom3DItemC2ERK7QStringRK9QVector3DS5_RK11QQuaternionRK6QImageP7QObject` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN14QCustom3DLabel12setTextColorERK6QColor` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN14QCustom3DLabel16setBorderEnabledEb` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN14QCustom3DLabel18setBackgroundColorERK6QColor` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN14QCustom3DLabel20setBackgroundEnabledEb` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN14QCustom3DLabelC2ERK7QStringRK5QFontRK9QVector3DS8_RK11QQuaternionP7QObject` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN15Q3DInputHandler10wheelEventEP11QWheelEvent` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN15Q3DInputHandler11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN15Q3DInputHandler11qt_metacastEPKc` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN15Q3DInputHandler14mouseMoveEventEP11QMouseEventRK6QPoint` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN15Q3DInputHandler14setZoomEnabledEb` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN15Q3DInputHandler15mousePressEventEP11QMouseEventRK6QPoint` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN15Q3DInputHandler16staticMetaObjectE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN15Q3DInputHandler17mouseReleaseEventEP11QMouseEventRK6QPoint` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN15Q3DInputHandler18setRotationEnabledEb` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN15Q3DInputHandler19setSelectionEnabledEb` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN15Q3DInputHandler22setZoomAtTargetEnabledEb` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN15Q3DInputHandlerC2EP7QObject` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN15Q3DInputHandlerD2Ev` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN15QAbstract3DAxis20setLabelAutoRotationEf` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN15QCustom3DVolume17setSliceFrameGapsERK9QVector3D` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN15QCustom3DVolume18setSliceFrameColorERK6QColor` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN15QCustom3DVolume19setSliceFrameWidthsERK9QVector3D` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN15QCustom3DVolume24setSliceFrameThicknessesERK9QVector3D` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN15QCustom3DVolumeC2ERK9QVector3DS2_RK11QQuaternioniiiP5QListIhEN6QImage6FormatERKS6_IjEP7QObject` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph10touchEventEP11QTouchEvent` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph10wheelEventEP11QWheelEvent` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph11exposeEventEP12QExposeEvent` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph11qt_metacastEPKc` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph11resizeEventEP12QResizeEvent` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph12releaseThemeEP8Q3DTheme` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph13addCustomItemEP13QCustom3DItem` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph13renderToImageEiRK5QSize` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph13setMeasureFpsEb` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph13setReflectionEb` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph14clearSelectionEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph14mouseMoveEventEP11QMouseEvent` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph14setActiveThemeEP8Q3DTheme` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph14setAspectRatioEd` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph15addInputHandlerEP23QAbstract3DInputHandler` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph15mousePressEventEP11QMouseEvent` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph15setReflectivityEd` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph16removeCustomItemEP13QCustom3DItem` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph16setSelectionModeE6QFlagsINS_13SelectionFlagEE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph16setShadowQualityENS_13ShadowQualityE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph16staticMetaObjectE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph17mouseReleaseEventEP11QMouseEvent` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph17releaseCustomItemEP13QCustom3DItem` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph17removeCustomItemsEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph18removeCustomItemAtERK9QVector3D` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph18setOrthoProjectionEb` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph19releaseInputHandlerEP23QAbstract3DInputHandler` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph20setOptimizationHintsE6QFlagsINS_16OptimizationHintEE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph20setRadialLabelOffsetEf` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph21mouseDoubleClickEventEP11QMouseEvent` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph21setActiveInputHandlerEP23QAbstract3DInputHandler` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph24setHorizontalAspectRatioEd` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph5eventEP6QEvent` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph8addThemeEP8Q3DTheme` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph8setPolarEb` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph9setLocaleERK7QLocale` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraph9setMarginEd` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QAbstract3DGraphD2Ev` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QScatter3DSeries15setSelectedItemEi` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QScatterDataItem15createExtraDataEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QScatterDataItemC1ERK9QVector3D` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QScatterDataItemC1ERK9QVector3DRK11QQuaternion` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QScatterDataItemC1ERKS_` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QScatterDataItemC1Ev` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QScatterDataItemD1Ev` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QScatterDataItemaSERKS_` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QSurface3DSeries16setSelectedPointERK6QPoint` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QSurface3DSeries17setWireframeColorERK6QColor` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QSurface3DSeries21setFlatShadingEnabledEb` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QSurfaceDataItem15createExtraDataEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QSurfaceDataItemC1ERK9QVector3D` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QSurfaceDataItemC1ERKS_` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QSurfaceDataItemC1Ev` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QSurfaceDataItemD1Ev` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN16QSurfaceDataItemaSERKS_` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN17QAbstract3DSeries12setBaseColorERK6QColor` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN17QAbstract3DSeries13setColorStyleEN8Q3DTheme10ColorStyleE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN17QAbstract3DSeries19setMeshAxisAndAngleERK9QVector3Df` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN17QAbstract3DSeries22setMultiHighlightColorERK6QColor` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN17QAbstract3DSeries23setSingleHighlightColorERK6QColor` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN17QScatterDataProxy10insertItemEiRK16QScatterDataItem` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN17QScatterDataProxy10resetArrayEP5QListI16QScatterDataItemE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN17QScatterDataProxy11insertItemsEiRK5QListI16QScatterDataItemE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN17QScatterDataProxy11removeItemsEii` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN17QScatterDataProxy7addItemERK16QScatterDataItem` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN17QScatterDataProxy7setItemEiRK16QScatterDataItem` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN17QScatterDataProxy8addItemsERK5QListI16QScatterDataItemE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN17QScatterDataProxy8setItemsEiRK5QListI16QScatterDataItemE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN17QSurfaceDataProxy10insertRowsEiRK5QListIPS0_I16QSurfaceDataItemEE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN17QSurfaceDataProxy10removeRowsEii` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN17QSurfaceDataProxy10resetArrayEP5QListIPS0_I16QSurfaceDataItemEE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN17QSurfaceDataProxy6addRowEP5QListI16QSurfaceDataItemE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN17QSurfaceDataProxy6setRowEiP5QListI16QSurfaceDataItemE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN17QSurfaceDataProxy7addRowsERK5QListIPS0_I16QSurfaceDataItemEE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN17QSurfaceDataProxy7setItemERK6QPointRK16QSurfaceDataItem` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN17QSurfaceDataProxy7setItemEiiRK16QSurfaceDataItem` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN17QSurfaceDataProxy7setRowsEiRK5QListIPS0_I16QSurfaceDataItemEE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN17QSurfaceDataProxy9insertRowEiP5QListI16QSurfaceDataItemE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN20QTouch3DInputHandler10touchEventEP11QTouchEvent` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN20QTouch3DInputHandler11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN20QTouch3DInputHandler11qt_metacastEPKc` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN20QTouch3DInputHandler16staticMetaObjectE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN20QTouch3DInputHandlerC2EP7QObject` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN20QTouch3DInputHandlerD2Ev` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN23QAbstract3DInputHandler10touchEventEP11QTouchEvent` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN23QAbstract3DInputHandler10wheelEventEP11QWheelEvent` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN23QAbstract3DInputHandler11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN23QAbstract3DInputHandler11qt_metacastEPKc` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN23QAbstract3DInputHandler12setInputViewENS_9InputViewE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN23QAbstract3DInputHandler14mouseMoveEventEP11QMouseEventRK6QPoint` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN23QAbstract3DInputHandler15mousePressEventEP11QMouseEventRK6QPoint` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN23QAbstract3DInputHandler15setPrevDistanceEi` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN23QAbstract3DInputHandler16setInputPositionERK6QPoint` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN23QAbstract3DInputHandler16staticMetaObjectE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN23QAbstract3DInputHandler17mouseReleaseEventEP11QMouseEventRK6QPoint` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN23QAbstract3DInputHandler19setPreviousInputPosERK6QPoint` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN23QAbstract3DInputHandler21mouseDoubleClickEventEP11QMouseEvent` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN23QAbstract3DInputHandler8setSceneEP8Q3DScene` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN23QAbstract3DInputHandlerC2EP7QObject` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN23QAbstract3DInputHandlerD2Ev` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN24QLogValue3DAxisFormatter17setShowEdgeLabelsEb` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN7Q3DBars10setRowAxisEP15QCategory3DAxis` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN7Q3DBars11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN7Q3DBars11qt_metacastEPKc` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN7Q3DBars11releaseAxisEP15QAbstract3DAxis` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN7Q3DBars12insertSeriesEiP12QBar3DSeries` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN7Q3DBars12removeSeriesEP12QBar3DSeries` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN7Q3DBars12setValueAxisEP12QValue3DAxis` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN7Q3DBars13setBarSpacingERK6QSizeF` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN7Q3DBars13setColumnAxisEP15QCategory3DAxis` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN7Q3DBars13setFloorLevelEf` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN7Q3DBars15setBarThicknessEf` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN7Q3DBars16setPrimarySeriesEP12QBar3DSeries` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN7Q3DBars16staticMetaObjectE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN7Q3DBars18setBarSeriesMarginERK6QSizeF` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN7Q3DBars21setBarSpacingRelativeEb` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN7Q3DBars21setMultiSeriesUniformEb` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN7Q3DBars7addAxisEP15QAbstract3DAxis` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN7Q3DBars9addSeriesEP12QBar3DSeries` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN7Q3DBarsC2EPK14QSurfaceFormatP7QWindow` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN7Q3DBarsD2Ev` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DLight11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DLight11qt_metacastEPKc` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DLight14isAutoPositionEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DLight15setAutoPositionEb` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DLight16staticMetaObjectE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DLightC2EP7QObject` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DLightD2Ev` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DScene14setActiveLightEP8Q3DLight` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DScene15setActiveCameraEP9Q3DCamera` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DScene19setDevicePixelRatioEf` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DScene21invalidSelectionPointEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DScene21setGraphPositionQueryERK6QPoint` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DScene21setPrimarySubViewportERK5QRect` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DScene23isPointInPrimarySubViewERK6QPoint` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DScene23setSecondarySubViewportERK5QRect` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DScene25isPointInSecondarySubViewERK6QPoint` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DScene25setSelectionQueryPositionERK6QPoint` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DTheme11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DTheme11qt_metacastEPKc` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DTheme13setBaseColorsERK5QListI6QColorE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DTheme13setColorStyleENS_10ColorStyleE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DTheme13setLightColorERK6QColor` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DTheme14setGridEnabledEb` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DTheme14setWindowColorERK6QColor` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DTheme16setBaseGradientsERK5QListI15QLinearGradientE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DTheme16setGridLineColorERK6QColor` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DTheme16setLightStrengthEf` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DTheme16staticMetaObjectE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DTheme17setLabelTextColorERK6QColor` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DTheme18setBackgroundColorERK6QColor` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DTheme20setBackgroundEnabledEb` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DTheme21setLabelBorderEnabledEb` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DTheme22setMultiHighlightColorERK6QColor` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DTheme23setAmbientLightStrengthEf` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DTheme23setLabelBackgroundColorERK6QColor` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DTheme23setSingleHighlightColorERK6QColor` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DTheme25setHighlightLightStrengthEf` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DTheme25setLabelBackgroundEnabledEb` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DTheme25setMultiHighlightGradientERK15QLinearGradient` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DTheme26setSingleHighlightGradientERK15QLinearGradient` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DTheme7setFontERK5QFont` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DTheme7setTypeENS_5ThemeE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DThemeC2ENS_5ThemeEP7QObject` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DThemeC2EP7QObject` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN8Q3DThemeD2Ev` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN9Q3DCamera11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN9Q3DCamera11qt_metacastEPKc` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN9Q3DCamera12setXRotationEf` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN9Q3DCamera12setYRotationEf` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN9Q3DCamera12setZoomLevelEf` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN9Q3DCamera14copyValuesFromERK9Q3DObject` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN9Q3DCamera15setCameraPresetENS_12CameraPresetE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN9Q3DCamera15setMaxZoomLevelEf` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN9Q3DCamera15setMinZoomLevelEf` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN9Q3DCamera16setWrapXRotationEb` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN9Q3DCamera16setWrapYRotationEb` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN9Q3DCamera16staticMetaObjectE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN9Q3DCamera17setCameraPositionEfff` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN9Q3DCamera9setTargetERK9QVector3D` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN9Q3DCameraC2EP7QObject` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN9Q3DCameraD2Ev` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN9Q3DObject11parentSceneEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN9Q3DObject11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN9Q3DObject11qt_metacastEPKc` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN9Q3DObject11setPositionERK9QVector3D` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN9Q3DObject14copyValuesFromERKS_` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN9Q3DObject16staticMetaObjectE` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN9Q3DObject8setDirtyEb` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN9Q3DObjectC2EP7QObject` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZN9Q3DObjectD2Ev` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK10Q3DScatter10metaObjectEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK10Q3DScatter10seriesListEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK10Q3DScatter14selectedSeriesEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK10Q3DScatter4axesEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK10Q3DScatter5axisXEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK10Q3DScatter5axisYEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK10Q3DScatter5axisZEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK10Q3DSurface10metaObjectEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK10Q3DSurface10seriesListEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK10Q3DSurface14selectedSeriesEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK10Q3DSurface18flipHorizontalGridEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK10Q3DSurface4axesEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK10Q3DSurface5axisXEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK10Q3DSurface5axisYEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK10Q3DSurface5axisZEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK13QBarDataProxy12columnLabelsEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK13QBarDataProxy5arrayEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK13QBarDataProxy5rowAtEi` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK13QBarDataProxy6itemAtERK6QPoint` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK13QBarDataProxy6itemAtEii` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK13QBarDataProxy9rowLabelsEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK14QCustom3DLabel15isBorderEnabledEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK14QCustom3DLabel19isBackgroundEnabledEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK15Q3DInputHandler10metaObjectEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK15Q3DInputHandler13isZoomEnabledEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK15Q3DInputHandler17isRotationEnabledEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK15Q3DInputHandler18isSelectionEnabledEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK15Q3DInputHandler21isZoomAtTargetEnabledEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK15QAbstract3DAxis17labelAutoRotationEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK16QAbstract3DGraph10currentFpsEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK16QAbstract3DGraph10hasContextEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK16QAbstract3DGraph10measureFpsEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK16QAbstract3DGraph10metaObjectEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK16QAbstract3DGraph11activeThemeEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK16QAbstract3DGraph11aspectRatioEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK16QAbstract3DGraph11customItemsEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK16QAbstract3DGraph12isReflectionEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK16QAbstract3DGraph12reflectivityEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK16QAbstract3DGraph12selectedAxisEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK16QAbstract3DGraph13inputHandlersEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK16QAbstract3DGraph13selectionModeEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK16QAbstract3DGraph13shadowQualityEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK16QAbstract3DGraph15selectedElementEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK16QAbstract3DGraph16shadowsSupportedEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK16QAbstract3DGraph17isOrthoProjectionEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK16QAbstract3DGraph17optimizationHintsEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK16QAbstract3DGraph17radialLabelOffsetEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK16QAbstract3DGraph18activeInputHandlerEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK16QAbstract3DGraph18selectedCustomItemEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK16QAbstract3DGraph18selectedLabelIndexEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK16QAbstract3DGraph20queriedGraphPositionEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK16QAbstract3DGraph21horizontalAspectRatioEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK16QAbstract3DGraph23selectedCustomItemIndexEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK16QAbstract3DGraph5sceneEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK16QAbstract3DGraph6localeEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK16QAbstract3DGraph6marginEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK16QAbstract3DGraph6themesEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK16QAbstract3DGraph7isPolarEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK16QAbstract3DGraph9hasSeriesEP17QAbstract3DSeries` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK16QSurface3DSeries20isFlatShadingEnabledEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK17QAbstract3DSeries9itemLabelEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK17QScatterDataProxy5arrayEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK17QScatterDataProxy6itemAtEi` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK17QSurfaceDataProxy5arrayEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK17QSurfaceDataProxy6itemAtERK6QPoint` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK17QSurfaceDataProxy6itemAtEii` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK20QTouch3DInputHandler10metaObjectEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK21QValue3DAxisFormatter12labelStringsEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK21QValue3DAxisFormatter12populateCopyERS_` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK21QValue3DAxisFormatter13gridPositionsEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK21QValue3DAxisFormatter14labelPositionsEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK21QValue3DAxisFormatter14stringForValueEdRK7QString` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK21QValue3DAxisFormatter16subGridPositionsEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK23QAbstract3DInputHandler10metaObjectEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK23QAbstract3DInputHandler12prevDistanceEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK23QAbstract3DInputHandler13inputPositionEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK23QAbstract3DInputHandler16previousInputPosEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK23QAbstract3DInputHandler5sceneEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK23QAbstract3DInputHandler9inputViewEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK24QLogValue3DAxisFormatter12populateCopyER21QValue3DAxisFormatter` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK24QLogValue3DAxisFormatter14showEdgeLabelsEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK7Q3DBars10barSpacingEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK7Q3DBars10columnAxisEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK7Q3DBars10floorLevelEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK7Q3DBars10metaObjectEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK7Q3DBars10seriesListEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK7Q3DBars12barThicknessEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK7Q3DBars13primarySeriesEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK7Q3DBars14selectedSeriesEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK7Q3DBars15barSeriesMarginEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK7Q3DBars20isBarSpacingRelativeEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK7Q3DBars20isMultiSeriesUniformEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK7Q3DBars4axesEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK7Q3DBars7rowAxisEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK7Q3DBars9valueAxisEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK8Q3DLight10metaObjectEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK8Q3DScene11activeLightEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK8Q3DScene12activeCameraEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK8Q3DTheme10baseColorsEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK8Q3DTheme10colorStyleEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK8Q3DTheme10lightColorEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK8Q3DTheme10metaObjectEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK8Q3DTheme11windowColorEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK8Q3DTheme13baseGradientsEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK8Q3DTheme13gridLineColorEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK8Q3DTheme13isGridEnabledEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK8Q3DTheme13lightStrengthEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK8Q3DTheme14labelTextColorEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK8Q3DTheme15backgroundColorEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK8Q3DTheme19isBackgroundEnabledEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK8Q3DTheme19multiHighlightColorEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK8Q3DTheme20ambientLightStrengthEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK8Q3DTheme20isLabelBorderEnabledEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK8Q3DTheme20labelBackgroundColorEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK8Q3DTheme20singleHighlightColorEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK8Q3DTheme22highlightLightStrengthEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK8Q3DTheme22multiHighlightGradientEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK8Q3DTheme23singleHighlightGradientEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK8Q3DTheme24isLabelBackgroundEnabledEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK8Q3DTheme4fontEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK8Q3DTheme4typeEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK9Q3DCamera10metaObjectEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK9Q3DCamera12cameraPresetEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK9Q3DCamera12maxZoomLevelEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK9Q3DCamera12minZoomLevelEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK9Q3DCamera13wrapXRotationEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK9Q3DCamera13wrapYRotationEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK9Q3DCamera6targetEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK9Q3DCamera9xRotationEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK9Q3DCamera9yRotationEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK9Q3DCamera9zoomLevelEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK9Q3DObject10metaObjectEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK9Q3DObject7isDirtyEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZNK9Q3DObject8positionEv` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZTI10Q3DScatter` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZTI10Q3DSurface` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZTI15Q3DInputHandler` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZTI16QAbstract3DGraph` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZTI20QTouch3DInputHandler` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZTI23QAbstract3DInputHandler` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZTI7Q3DBars` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZTI8Q3DLight` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZTI8Q3DTheme` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZTI9Q3DCamera` | 1 | freecad |
| libQt6DataVisualization.6.dylib | `__ZTI9Q3DObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__Z13comparesEqualRK11QGraphsLineS1_` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QBarSeries11deselectAllEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QBarSeries11setBarWidthEd` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QBarSeries11setBarsTypeENS_8BarsTypeE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QBarSeries14removeMultipleExx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QBarSeries14setBarDelegateEP13QQmlComponent` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QBarSeries14setLabelsAngleEd` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QBarSeries15setBorderColorsERK5QListI6QColorE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QBarSeries15setLabelsFormatERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QBarSeries15setLabelsMarginEd` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QBarSeries15setSeriesColorsERK5QListI6QColorE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QBarSeries16setLabelsVisibleEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QBarSeries17componentCompleteEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QBarSeries17setLabelsPositionENS_14LabelsPositionE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QBarSeries18setLabelsPrecisionEi` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QBarSeries2atEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QBarSeries4takeEP7QBarSet` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QBarSeries5clearEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QBarSeries6appendEP7QBarSet` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QBarSeries6appendERK5QListIP7QBarSetE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QBarSeries6insertExP7QBarSet` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QBarSeries6removeEP7QBarSet` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QBarSeries6removeEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QBarSeries7replaceEP7QBarSetS1_` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QBarSeries7replaceERK5QListIP7QBarSetE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QBarSeries7replaceExP7QBarSet` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QBarSeries9selectAllEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QPieSeries11setEndAngleEd` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QPieSeries13setStartAngleEd` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QPieSeries14removeMultipleExi` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QPieSeries17componentCompleteEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QPieSeries17handleSliceChangeEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QPieSeries2atEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QPieSeries4findERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QPieSeries6insertExP9QPieSlice` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QPieSeries6removeEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QPieSeries7replaceEP9QPieSliceS1_` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QPieSeries7replaceERK5QListIP9QPieSliceE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QPieSeries7replaceExP9QPieSlice` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QValueAxis15setSubTickCountEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QValueAxis16setLabelDecimalsEi` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN10QtGraphs3D16staticMetaObjectE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN11QAreaSeries11setSelectedEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN11QAreaSeries14setBorderColorE6QColor` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN11QAreaSeries14setBorderWidthEd` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN11QAreaSeries14setLowerSeriesEP9QXYSeries` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN11QAreaSeries14setUpperSeriesEP9QXYSeries` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN11QAreaSeries16setSelectedColorE6QColor` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN11QAreaSeries22setSelectedBorderColorE6QColor` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN11QAreaSeries8setColorE6QColor` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN11QGraphsLine11setSubColorE6QColor` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN11QGraphsLine11setSubWidthEd` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN11QGraphsLine12setMainColorE6QColor` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN11QGraphsLine12setMainWidthEd` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN11QGraphsLine16staticMetaObjectE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN11QGraphsLine17setLabelTextColorE6QColor` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN11QGraphsLineC1ERKS_` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN11QGraphsLineC1Ev` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN11QGraphsLineD1Ev` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN11QGraphsLineaSERKS_` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN11QLegendData16staticMetaObjectE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN11QLineSeries11setCapStyleEN2Qt11PenCapStyleE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN11QLineSeries17componentCompleteEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN11QLineSeries8setWidthEd` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QBar3DSeries10clearArrayEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QBar3DSeries11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QBar3DSeries11qt_metacastEPKc` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QBar3DSeries12setDataArrayERK5QListIS0_I12QBarDataItemEE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QBar3DSeries12setDataProxyEP13QBarDataProxy` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QBar3DSeries12setMeshAngleEf` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QBar3DSeries12setRowColorsERK5QListI6QColorE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QBar3DSeries12setRowLabelsERK5QListI7QStringE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QBar3DSeries14setSelectedBarE6QPoint` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QBar3DSeries15setColumnLabelsERK5QListI7QStringE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QBar3DSeries16staticMetaObjectE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QBar3DSeries24invalidSelectionPositionEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QBar3DSeries8clearRowEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QBar3DSeriesC2EP13QBarDataProxyP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QBar3DSeriesC2EP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QBar3DSeriesD2Ev` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme10classBeginEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme11qt_metacastEPKc` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme12setLabelFontERK5QFont` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme13setColorStyleENS_10ColorStyleE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme14resetDirtyBitsEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme14setBorderWidthEd` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme14setColorSchemeENS_11ColorSchemeE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme14setGridVisibleEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme15resetColorThemeEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme15resetThemeDirtyEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme15setBorderColorsERK5QListI6QColorE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme15setSeriesColorsERK5QListI6QColorE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme16setLabelsVisibleEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme16staticMetaObjectE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme17componentCompleteEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme17setAxisXLabelFontERK5QFont` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme17setAxisYLabelFontERK5QFont` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme17setAxisZLabelFontERK5QFont` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme17setLabelTextColorE6QColor` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme18setBackgroundColorE6QColor` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme18setSeriesGradientsERK5QListI15QLinearGradientE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme20setBackgroundVisibleEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme21handleBaseColorUpdateEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme21setLabelBorderVisibleEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme22setMultiHighlightColorE6QColor` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme23setLabelBackgroundColorE6QColor` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme23setSingleHighlightColorE6QColor` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme24handleBaseGradientUpdateEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme25setLabelBackgroundVisibleEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme25setMultiHighlightGradientERK15QLinearGradient` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme26setPlotAreaBackgroundColorE6QColor` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme26setSingleHighlightGradientERK15QLinearGradient` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme28setPlotAreaBackgroundVisibleEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme7setGridERK11QGraphsLine` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme8setAxisXERK11QGraphsLine` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme8setAxisYERK11QGraphsLine` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme8setAxisZERK11QGraphsLine` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme8setThemeENS_5ThemeENS_10ForceThemeE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsTheme9dirtyBitsEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsThemeC2EP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QGraphsThemeD2Ev` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QValue3DAxis10positionAtEf` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QValue3DAxis11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QValue3DAxis11qt_metacastEPKc` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QValue3DAxis11recalculateEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QValue3DAxis11setReversedEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QValue3DAxis11subGridSizeEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QValue3DAxis12setFormatterEP21QValue3DAxisFormatter` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QValue3DAxis14gridPositionAtEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QValue3DAxis14setLabelFormatERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QValue3DAxis14stringForValueEf` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QValue3DAxis15labelPositionAtEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QValue3DAxis15setSegmentCountEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QValue3DAxis16staticMetaObjectE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QValue3DAxis17subGridPositionAtEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QValue3DAxis18setSubSegmentCountEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QValue3DAxis8gridSizeEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QValue3DAxisC2EP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN12QValue3DAxisD2Ev` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QAbstractAxis13setTitleColorE6QColor` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QAbstractAxis14setGridVisibleEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QAbstractAxis14setLabelsAngleEd` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QAbstractAxis16setLabelDelegateEP13QQmlComponent` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QAbstractAxis17setSubGridVisibleEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QBarDataProxy10insertRowsEx5QListIS0_I12QBarDataItemEE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QBarDataProxy10insertRowsEx5QListIS0_I12QBarDataItemEES0_I7QStringE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QBarDataProxy10removeRowsExxNS_12RemoveLabelsE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QBarDataProxy10resetArrayE5QListIS0_I12QBarDataItemEE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QBarDataProxy10resetArrayE5QListIS0_I12QBarDataItemEES0_I7QStringES5_` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QBarDataProxy10resetArrayEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QBarDataProxy11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QBarDataProxy11qt_metacastEPKc` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QBarDataProxy16staticMetaObjectE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QBarDataProxy6addRowE5QListI12QBarDataItemE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QBarDataProxy6addRowE5QListI12QBarDataItemE7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QBarDataProxy6setRowEx5QListI12QBarDataItemE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QBarDataProxy6setRowEx5QListI12QBarDataItemE7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QBarDataProxy7addRowsE5QListIS0_I12QBarDataItemEE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QBarDataProxy7addRowsE5QListIS0_I12QBarDataItemEES0_I7QStringE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QBarDataProxy7setItemE6QPoint12QBarDataItem` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QBarDataProxy7setItemExx12QBarDataItem` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QBarDataProxy7setRowsEx5QListIS0_I12QBarDataItemEE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QBarDataProxy7setRowsEx5QListIS0_I12QBarDataItemEES0_I7QStringE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QBarDataProxy9insertRowEx5QListI12QBarDataItemE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QBarDataProxy9insertRowEx5QListI12QBarDataItemE7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QBarDataProxyC2EP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QBarDataProxyD2Ev` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QCustom3DItem10setScalingE9QVector3D` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QCustom3DItem10setVisibleEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QCustom3DItem11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QCustom3DItem11qt_metacastEPKc` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QCustom3DItem11setMeshFileERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QCustom3DItem11setPositionE9QVector3D` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QCustom3DItem11setRotationERK11QQuaternion` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QCustom3DItem14setTextureFileERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QCustom3DItem15setTextureImageERK6QImage` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QCustom3DItem16setShadowCastingEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QCustom3DItem16staticMetaObjectE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QCustom3DItem18setScalingAbsoluteEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QCustom3DItem19setPositionAbsoluteEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QCustom3DItem23setRotationAxisAndAngleE9QVector3Df` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QCustom3DItem8rotationEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QCustom3DItemC2EP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QCustom3DItemC2ERK7QString9QVector3DS3_RK11QQuaternionRK6QImageP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QCustom3DItemD2Ev` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QDateTimeAxis14setLabelFormatERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QDateTimeAxis15setSubTickCountEi` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QDateTimeAxis15setTickIntervalEd` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QDateTimeAxis6setMaxERK9QDateTime` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QDateTimeAxis6setMinERK9QDateTime` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QSplineSeries11setCapStyleEN2Qt11PenCapStyleE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QSplineSeries16getControlPointsEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QSplineSeries17componentCompleteEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN13QSplineSeries8setWidthEd` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN14QCustom3DLabel11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN14QCustom3DLabel11qt_metacastEPKc` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN14QCustom3DLabel12setTextColorE6QColor` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN14QCustom3DLabel15setFacingCameraEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN14QCustom3DLabel16setBorderVisibleEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN14QCustom3DLabel16staticMetaObjectE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN14QCustom3DLabel18setBackgroundColorE6QColor` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN14QCustom3DLabel20setBackgroundVisibleEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN14QCustom3DLabel7setFontERK5QFont` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN14QCustom3DLabel7setTextERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN14QCustom3DLabelC2EP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN14QCustom3DLabelC2ERK7QStringRK5QFont9QVector3DS6_RK11QQuaternionP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN14QCustom3DLabelD2Ev` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN14QScatterSeries17componentCompleteEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN14QXYModelMapper11setXSectionEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN14QXYModelMapper11setYSectionEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN14QXYModelMapper8setCountEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN14QXYModelMapper8setFirstEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN14QXYModelMapperD2Ev` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QAbstract3DAxis11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QAbstract3DAxis11qt_metacastEPKc` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QAbstract3DAxis13setTitleFixedEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QAbstract3DAxis14setTitleOffsetEf` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QAbstract3DAxis15setTitleVisibleEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QAbstract3DAxis16setLabelsVisibleEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QAbstract3DAxis16staticMetaObjectE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QAbstract3DAxis17setLabelAutoAngleEf` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QAbstract3DAxis18setAutoAdjustRangeEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QAbstract3DAxis6setMaxEf` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QAbstract3DAxis6setMinEf` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QAbstract3DAxis8setRangeEff` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QAbstract3DAxis8setTitleERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QAbstract3DAxis9setLabelsERK5QListI7QStringE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QAbstract3DAxisD2Ev` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QAbstractSeries10classBeginEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QAbstractSeries12setHoverableEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QAbstractSeries13setSelectableEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QAbstractSeries17componentCompleteEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QAbstractSeries19setValuesMultiplierEd` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QBarModelMapper20setLastBarSetSectionEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QBarModelMapper21setFirstBarSetSectionEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QBarModelMapper8setCountEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QBarModelMapper8setFirstEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QBarModelMapper9setSeriesEP10QBarSeries` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QBarModelMapperD2Ev` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCategory3DAxis11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCategory3DAxis11qt_metacastEPKc` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCategory3DAxis16staticMetaObjectE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCategory3DAxis9setLabelsERK5QListI7QStringE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCategory3DAxisC2EP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCategory3DAxisD2Ev` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCustom3DVolume11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCustom3DVolume11qt_metacastEPKc` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCustom3DVolume11renderSliceEN2Qt4AxisEi` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCustom3DVolume13setColorTableERK5QListIjE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCustom3DVolume13setDrawSlicesEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCustom3DVolume14setSliceIndexXEi` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCustom3DVolume14setSliceIndexYEi` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCustom3DVolume14setSliceIndexZEi` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCustom3DVolume14setTextureDataEP5QListIhE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCustom3DVolume15setSliceIndicesEiii` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCustom3DVolume15setTextureDepthEi` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCustom3DVolume15setTextureWidthEi` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCustom3DVolume16setTextureFormatEN6QImage6FormatE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCustom3DVolume16setTextureHeightEi` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCustom3DVolume16staticMetaObjectE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCustom3DVolume17createTextureDataERK5QListIP6QImageE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCustom3DVolume17setSliceFrameGapsE9QVector3D` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCustom3DVolume17setSubTextureDataEN2Qt4AxisEiPKh` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCustom3DVolume17setSubTextureDataEN2Qt4AxisEiRK6QImage` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCustom3DVolume18setAlphaMultiplierEf` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCustom3DVolume18setDrawSliceFramesEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCustom3DVolume18setPreserveOpacityEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCustom3DVolume18setSliceFrameColorE6QColor` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCustom3DVolume19setSliceFrameWidthsE9QVector3D` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCustom3DVolume19setUseHighDefShaderEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCustom3DVolume20setTextureDimensionsEiii` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCustom3DVolume24setSliceFrameThicknessesE9QVector3D` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCustom3DVolumeC2E9QVector3DS0_RK11QQuaternioniiiP5QListIhEN6QImage6FormatERKS4_IjEP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCustom3DVolumeC2EP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QCustom3DVolumeD2Ev` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QPieModelMapper16setLabelsSectionEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QPieModelMapper16setValuesSectionEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QPieModelMapper19onSliceLabelChangedEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QPieModelMapper19onSliceValueChangedEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QPieModelMapper8setCountEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QPieModelMapper8setFirstEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN15QPieModelMapperD2Ev` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN16QBarCategoryAxis6insertExRK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN16QBarCategoryAxis6removeEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN16QScatter3DSeries10clearArrayEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN16QScatter3DSeries11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN16QScatter3DSeries11qt_metacastEPKc` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN16QScatter3DSeries11setItemSizeEf` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN16QScatter3DSeries12setDataArrayERK5QListI16QScatterDataItemE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN16QScatter3DSeries12setDataProxyEP17QScatterDataProxy` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN16QScatter3DSeries15setSelectedItemEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN16QScatter3DSeries16staticMetaObjectE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN16QScatter3DSeries21invalidSelectionIndexEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN16QScatter3DSeriesC2EP17QScatterDataProxyP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN16QScatter3DSeriesC2EP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN16QScatter3DSeriesD2Ev` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN16QSurface3DSeries10clearArrayEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN16QSurface3DSeries10setShadingENS_7ShadingE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN16QSurface3DSeries10setTextureERK6QImage` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN16QSurface3DSeries11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN16QSurface3DSeries11qt_metacastEPKc` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN16QSurface3DSeries11setDrawModeE6QFlagsINS_8DrawFlagEE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN16QSurface3DSeries12setDataArrayERK5QListIS0_I16QSurfaceDataItemEE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN16QSurface3DSeries12setDataProxyEP17QSurfaceDataProxy` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN16QSurface3DSeries14setTextureFileERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN16QSurface3DSeries16setSelectedPointE6QPoint` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN16QSurface3DSeries16staticMetaObjectE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN16QSurface3DSeries17setWireframeColorE6QColor` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN16QSurface3DSeries24invalidSelectionPositionEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN16QSurface3DSeries8clearRowEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN16QSurface3DSeriesC2EP17QSurfaceDataProxyP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN16QSurface3DSeriesC2EP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN16QSurface3DSeriesD2Ev` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QAbstract3DSeries10setVisibleEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QAbstract3DSeries11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QAbstract3DSeries11qt_metacastEPKc` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QAbstract3DSeries12setBaseColorE6QColor` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QAbstract3DSeries13setColorStyleEN12QGraphsTheme10ColorStyleE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QAbstract3DSeries13setMeshSmoothEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QAbstract3DSeries15setBaseGradientERK15QLinearGradient` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QAbstract3DSeries15setMeshRotationERK11QQuaternion` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QAbstract3DSeries16staticMetaObjectE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QAbstract3DSeries18setItemLabelFormatERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QAbstract3DSeries18setUserDefinedMeshERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QAbstract3DSeries19setItemLabelVisibleEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QAbstract3DSeries19setMeshAxisAndAngleE9QVector3Df` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QAbstract3DSeries22setMultiHighlightColorE6QColor` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QAbstract3DSeries23setSingleHighlightColorE6QColor` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QAbstract3DSeries25setMultiHighlightGradientERK15QLinearGradient` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QAbstract3DSeries26setSingleHighlightGradientERK15QLinearGradient` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QAbstract3DSeries7setMeshENS_4MeshE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QAbstract3DSeries7setNameERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QAbstract3DSeries9itemLabelEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QAbstract3DSeriesD2Ev` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QScatterDataProxy10insertItemEx16QScatterDataItem` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QScatterDataProxy10resetArrayE5QListI16QScatterDataItemE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QScatterDataProxy10resetArrayEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QScatterDataProxy11insertItemsEx5QListI16QScatterDataItemE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QScatterDataProxy11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QScatterDataProxy11qt_metacastEPKc` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QScatterDataProxy11removeItemsExx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QScatterDataProxy16staticMetaObjectE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QScatterDataProxy7addItemE16QScatterDataItem` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QScatterDataProxy7setItemEx16QScatterDataItem` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QScatterDataProxy8addItemsE5QListI16QScatterDataItemE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QScatterDataProxy8setItemsEx5QListI16QScatterDataItemE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QScatterDataProxyC2EP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QScatterDataProxyD2Ev` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QSurfaceDataProxy10insertRowsEx5QListIS0_I16QSurfaceDataItemEE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QSurfaceDataProxy10removeRowsExx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QSurfaceDataProxy10resetArrayE5QListIS0_I16QSurfaceDataItemEE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QSurfaceDataProxy10resetArrayEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QSurfaceDataProxy11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QSurfaceDataProxy11qt_metacastEPKc` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QSurfaceDataProxy16staticMetaObjectE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QSurfaceDataProxy6addRowE5QListI16QSurfaceDataItemE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QSurfaceDataProxy6setRowEx5QListI16QSurfaceDataItemE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QSurfaceDataProxy7addRowsE5QListIS0_I16QSurfaceDataItemEE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QSurfaceDataProxy7setItemE6QPoint16QSurfaceDataItem` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QSurfaceDataProxy7setItemExx16QSurfaceDataItem` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QSurfaceDataProxy7setRowsEx5QListIS0_I16QSurfaceDataItemEE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QSurfaceDataProxy9insertRowEx5QListI16QSurfaceDataItemE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QSurfaceDataProxyC2EP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN17QSurfaceDataProxyD2Ev` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN18QAbstractDataProxy11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN18QAbstractDataProxy11qt_metacastEPKc` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN18QAbstractDataProxy16staticMetaObjectE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN18QAbstractDataProxyD2Ev` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN21QValue3DAxisFormatter11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN21QValue3DAxisFormatter11qt_metacastEPKc` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN21QValue3DAxisFormatter11recalculateEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN21QValue3DAxisFormatter12populateCopyERS_` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN21QValue3DAxisFormatter12setAllowZeroEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN21QValue3DAxisFormatter14stringForValueEdRK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN21QValue3DAxisFormatter15setGridPoitionsE5QListIfE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN21QValue3DAxisFormatter15setLabelStringsE5QListI7QStringE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN21QValue3DAxisFormatter16staticMetaObjectE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN21QValue3DAxisFormatter17setAllowNegativesEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN21QValue3DAxisFormatter17setlabelPositionsE5QListIfE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN21QValue3DAxisFormatter19setSubGridPositionsE5QListIfE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN21QValue3DAxisFormatter7setAxisEP12QValue3DAxis` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN21QValue3DAxisFormatter9markDirtyEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN21QValue3DAxisFormatter9setLocaleERK7QLocale` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN21QValue3DAxisFormatterC2EP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN21QValue3DAxisFormatterD2Ev` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxy10setRowRoleERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxy11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxy11qt_metacastEPKc` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxy12setItemModelEP18QAbstractItemModel` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxy12setValueRoleERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxy13setColumnRoleERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxy15setRotationRoleERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxy16rowCategoryIndexERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxy16setRowCategoriesERK5QListI7QStringE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxy16staticMetaObjectE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxy17setRowRolePatternERK18QRegularExpression` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxy17setRowRoleReplaceERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxy19columnCategoryIndexERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxy19setColumnCategoriesERK5QListI7QStringE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxy19setValueRolePatternERK18QRegularExpression` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxy19setValueRoleReplaceERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxy20setAutoRowCategoriesEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxy20setColumnRolePatternERK18QRegularExpression` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxy20setColumnRoleReplaceERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxy21setMultiMatchBehaviorENS_18MultiMatchBehaviorE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxy21setUseModelCategoriesEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxy22setRotationRolePatternERK18QRegularExpression` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxy22setRotationRoleReplaceERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxy23setAutoColumnCategoriesEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxy5remapERK7QStringS2_S2_S2_RK5QListIS0_ES6_` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxyC2EP18QAbstractItemModelP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxyC2EP18QAbstractItemModelRK7QStringP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxyC2EP18QAbstractItemModelRK7QStringS4_S4_P7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxyC2EP18QAbstractItemModelRK7QStringS4_S4_RK5QListIS2_ES8_P7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxyC2EP18QAbstractItemModelRK7QStringS4_S4_S4_P7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxyC2EP18QAbstractItemModelRK7QStringS4_S4_S4_RK5QListIS2_ES8_P7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxyC2EP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN22QItemModelBarDataProxyD2Ev` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN24QLogValue3DAxisFormatter11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN24QLogValue3DAxisFormatter11qt_metacastEPKc` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN24QLogValue3DAxisFormatter11recalculateEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN24QLogValue3DAxisFormatter12populateCopyER21QValue3DAxisFormatter` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN24QLogValue3DAxisFormatter14setAutoSubGridEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN24QLogValue3DAxisFormatter16staticMetaObjectE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN24QLogValue3DAxisFormatter20setEdgeLabelsVisibleEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN24QLogValue3DAxisFormatter7setBaseEd` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN24QLogValue3DAxisFormatterC2EP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN24QLogValue3DAxisFormatterD2Ev` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QHeightMapSurfaceDataProxy11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QHeightMapSurfaceDataProxy11qt_metacastEPKc` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QHeightMapSurfaceDataProxy12setHeightMapERK6QImage` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QHeightMapSurfaceDataProxy12setMaxXValueEf` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QHeightMapSurfaceDataProxy12setMaxYValueEf` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QHeightMapSurfaceDataProxy12setMaxZValueEf` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QHeightMapSurfaceDataProxy12setMinXValueEf` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QHeightMapSurfaceDataProxy12setMinYValueEf` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QHeightMapSurfaceDataProxy12setMinZValueEf` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QHeightMapSurfaceDataProxy13setAutoScaleYEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QHeightMapSurfaceDataProxy14setValueRangesEffff` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QHeightMapSurfaceDataProxy16setHeightMapFileERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QHeightMapSurfaceDataProxy16staticMetaObjectE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QHeightMapSurfaceDataProxy20handlePendingResolveEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QHeightMapSurfaceDataProxyC2EP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QHeightMapSurfaceDataProxyC2ERK6QImageP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QHeightMapSurfaceDataProxyC2ERK7QStringP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QHeightMapSurfaceDataProxyD2Ev` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelScatterDataProxy11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelScatterDataProxy11qt_metacastEPKc` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelScatterDataProxy11setXPosRoleERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelScatterDataProxy11setYPosRoleERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelScatterDataProxy11setZPosRoleERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelScatterDataProxy12setItemModelEP18QAbstractItemModel` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelScatterDataProxy15setRotationRoleERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelScatterDataProxy16staticMetaObjectE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelScatterDataProxy18setXPosRolePatternERK18QRegularExpression` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelScatterDataProxy18setXPosRoleReplaceERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelScatterDataProxy18setYPosRolePatternERK18QRegularExpression` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelScatterDataProxy18setYPosRoleReplaceERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelScatterDataProxy18setZPosRolePatternERK18QRegularExpression` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelScatterDataProxy18setZPosRoleReplaceERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelScatterDataProxy22setRotationRolePatternERK18QRegularExpression` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelScatterDataProxy22setRotationRoleReplaceERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelScatterDataProxy5remapERK7QStringS2_S2_S2_` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelScatterDataProxyC2EP18QAbstractItemModelP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelScatterDataProxyC2EP18QAbstractItemModelRK7QStringS4_S4_P7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelScatterDataProxyC2EP18QAbstractItemModelRK7QStringS4_S4_S4_P7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelScatterDataProxyC2EP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelScatterDataProxyD2Ev` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxy10setRowRoleERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxy11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxy11qt_metacastEPKc` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxy11setXPosRoleERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxy11setYPosRoleERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxy11setZPosRoleERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxy12setItemModelEP18QAbstractItemModel` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxy13setColumnRoleERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxy16rowCategoryIndexERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxy16setRowCategoriesERK5QListI7QStringE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxy16staticMetaObjectE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxy17setRowRolePatternERK18QRegularExpression` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxy17setRowRoleReplaceERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxy18setXPosRolePatternERK18QRegularExpression` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxy18setXPosRoleReplaceERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxy18setYPosRolePatternERK18QRegularExpression` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxy18setYPosRoleReplaceERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxy18setZPosRolePatternERK18QRegularExpression` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxy18setZPosRoleReplaceERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxy19columnCategoryIndexERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxy19setColumnCategoriesERK5QListI7QStringE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxy20setAutoRowCategoriesEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxy20setColumnRolePatternERK18QRegularExpression` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxy20setColumnRoleReplaceERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxy21setMultiMatchBehaviorENS_18MultiMatchBehaviorE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxy21setUseModelCategoriesEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxy23setAutoColumnCategoriesEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxy5remapERK7QStringS2_S2_S2_S2_RK5QListIS0_ES6_` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxyC2EP18QAbstractItemModelP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxyC2EP18QAbstractItemModelRK7QStringP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxyC2EP18QAbstractItemModelRK7QStringS4_S4_P7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxyC2EP18QAbstractItemModelRK7QStringS4_S4_RK5QListIS2_ES8_P7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxyC2EP18QAbstractItemModelRK7QStringS4_S4_S4_S4_P7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxyC2EP18QAbstractItemModelRK7QStringS4_S4_S4_S4_RK5QListIS2_ES8_P7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxyC2EP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN26QItemModelSurfaceDataProxyD2Ev` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN7QBarSet10selectBarsERK5QListIxE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN7QBarSet11deselectBarEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN7QBarSet12deselectBarsERK5QListIxE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN7QBarSet14setBarSelectedExb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN7QBarSet14setBorderWidthEd` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN7QBarSet15toggleSelectionERK5QListIxE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN7QBarSet16setSelectedColorE6QColor` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN7QBarSet5clearEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN7QBarSet6insertExd` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN7QBarSet6removeExx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN7QBarSet7replaceExd` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN7QBarSet8setLabelERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN7QBarSet9selectBarEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN7QBarSet9setValuesERK5QListI8QVariantE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN7QBarSetC2EP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN7QBarSetC2ERK7QStringP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN7QBarSetlsEd` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN8Q3DScene11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN8Q3DScene11qt_metacastEPKc` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN8Q3DScene16setSlicingActiveEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN8Q3DScene16staticMetaObjectE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN8Q3DScene19setDevicePixelRatioEd` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN8Q3DScene21setGraphPositionQueryE6QPoint` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN8Q3DScene21setPrimarySubViewportE5QRect` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN8Q3DScene23isPointInPrimarySubViewE6QPoint` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN8Q3DScene23setSecondarySubViewportE5QRect` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN8Q3DScene24setSecondarySubviewOnTopEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN8Q3DScene25isPointInSecondarySubViewE6QPoint` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN8Q3DScene25setSelectionQueryPositionE6QPoint` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN8Q3DSceneC2EP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN8Q3DSceneD2Ev` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN9QPieSlice14setBorderWidthEd` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN9QPieSlice8setLabelERK7QString` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN9QPieSliceC2ERK7QStringdP7QObject` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN9QXYSeries11selectPointEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN9QXYSeries12selectPointsERK5QListIxE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN9QXYSeries12setDraggableEb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN9QXYSeries13deselectPointEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN9QXYSeries14deselectPointsERK5QListIxE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN9QXYSeries14removeMultipleExx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN9QXYSeries15toggleSelectionERK5QListIxE` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN9QXYSeries16setPointDelegateEP13QQmlComponent` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN9QXYSeries16setPointSelectedExb` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN9QXYSeries16setSelectedColorE6QColor` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN9QXYSeries4takeE7QPointF` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN9QXYSeries6appendE7QPointF` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN9QXYSeries6insertEx7QPointF` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN9QXYSeries6removeE7QPointF` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN9QXYSeries6removeEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN9QXYSeries7replaceE7QPointFS0_` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN9QXYSeries7replaceEx7QPointF` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN9QXYSeries7replaceExdd` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN9QXYSeries8setColorE6QColor` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZN9QXYSerieslsE7QPointF` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK10QBarSeries11barDelegateEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK10QBarSeries11labelsAngleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK10QBarSeries12borderColorsEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK10QBarSeries12labelsFormatEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK10QBarSeries12labelsMarginEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK10QBarSeries12seriesColorsEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK10QBarSeries13labelsVisibleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK10QBarSeries14labelsPositionEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK10QBarSeries15labelsPrecisionEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK10QBarSeries4findEP7QBarSet` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK10QBarSeries5countEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK10QBarSeries7barSetsEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK10QBarSeries8barWidthEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK10QBarSeries8barsTypeEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK10QPieSeries10startAngleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK10QPieSeries8endAngleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK10QValueAxis12subTickCountEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK10QValueAxis13labelDecimalsEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK11QAreaSeries10isSelectedEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK11QAreaSeries11borderWidthEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK11QAreaSeries13selectedColorEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK11QAreaSeries19selectedBorderColorEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK11QGraphsLine14labelTextColorEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK11QGraphsLine8subColorEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK11QGraphsLine8subWidthEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK11QGraphsLine9mainColorEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK11QGraphsLine9mainWidthEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK11QLineSeries5widthEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK11QLineSeries8capStyleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QBar3DSeries10metaObjectEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QBar3DSeries11selectedBarEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QBar3DSeries12columnLabelsEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QBar3DSeries9dataProxyEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QBar3DSeries9meshAngleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QBar3DSeries9rowColorsEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QBar3DSeries9rowLabelsEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QGraphsTheme10colorStyleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QGraphsTheme10metaObjectEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QGraphsTheme10themeDirtyEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QGraphsTheme11borderWidthEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QGraphsTheme11colorSchemeEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QGraphsTheme12borderColorsEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QGraphsTheme12seriesColorsEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QGraphsTheme13isGridVisibleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QGraphsTheme13labelsVisibleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QGraphsTheme14axisXLabelFontEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QGraphsTheme14axisYLabelFontEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QGraphsTheme14axisZLabelFontEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QGraphsTheme14labelTextColorEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QGraphsTheme15backgroundColorEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QGraphsTheme15seriesGradientsEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QGraphsTheme19isBackgroundVisibleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QGraphsTheme19multiHighlightColorEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QGraphsTheme20isLabelBorderVisibleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QGraphsTheme20labelBackgroundColorEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QGraphsTheme20singleHighlightColorEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QGraphsTheme22multiHighlightGradientEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QGraphsTheme23plotAreaBackgroundColorEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QGraphsTheme23singleHighlightGradientEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QGraphsTheme24isLabelBackgroundVisibleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QGraphsTheme27isPlotAreaBackgroundVisibleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QGraphsTheme4gridEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QGraphsTheme5axisXEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QGraphsTheme5axisYEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QGraphsTheme5axisZEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QGraphsTheme5themeEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QGraphsTheme9labelFontEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QValue3DAxis10metaObjectEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QValue3DAxis11labelFormatEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QValue3DAxis12segmentCountEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QValue3DAxis15subSegmentCountEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QValue3DAxis8reversedEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK12QValue3DAxis9formatterEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK13QAbstractAxis10titleColorEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK13QAbstractAxis13isGridVisibleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK13QAbstractAxis13labelDelegateEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK13QAbstractAxis16isSubGridVisibleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK13QBarDataProxy10metaObjectEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK13QBarDataProxy5rowAtEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK13QBarDataProxy6itemAtE6QPoint` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK13QBarDataProxy6itemAtExx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK13QBarDataProxy6seriesEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK13QBarDataProxy8colCountEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK13QBarDataProxy8rowCountEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK13QCustom3DItem10metaObjectEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK13QCustom3DItem11textureFileEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK13QCustom3DItem15isShadowCastingEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK13QCustom3DItem17isScalingAbsoluteEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK13QCustom3DItem18isPositionAbsoluteEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK13QCustom3DItem7scalingEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK13QCustom3DItem8meshFileEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK13QCustom3DItem8positionEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK13QCustom3DItem9isVisibleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK13QDateTimeAxis11labelFormatEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK13QDateTimeAxis12subTickCountEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK13QDateTimeAxis12tickIntervalEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK13QSplineSeries5widthEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK13QSplineSeries8capStyleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK14QCustom3DLabel10metaObjectEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK14QCustom3DLabel14isFacingCameraEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK14QCustom3DLabel15backgroundColorEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK14QCustom3DLabel15isBorderVisibleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK14QCustom3DLabel19isBackgroundVisibleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK14QCustom3DLabel4fontEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK14QCustom3DLabel4textEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK14QCustom3DLabel9textColorEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QAbstract3DAxis10metaObjectEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QAbstract3DAxis11orientationEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QAbstract3DAxis11titleOffsetEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QAbstract3DAxis12isTitleFixedEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QAbstract3DAxis13labelsVisibleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QAbstract3DAxis14isTitleVisibleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QAbstract3DAxis14labelAutoAngleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QAbstract3DAxis17isAutoAdjustRangeEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QAbstract3DAxis3maxEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QAbstract3DAxis3minEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QAbstract3DAxis4typeEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QAbstract3DAxis5titleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QAbstract3DAxis6labelsEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QAbstractSeries10legendDataEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QAbstractSeries11isHoverableEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QAbstractSeries12isSelectableEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QAbstractSeries16valuesMultiplierEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QAbstractSeries9hasLoadedEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QCategory3DAxis10metaObjectEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QCategory3DAxis6labelsEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QCustom3DVolume10colorTableEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QCustom3DVolume10drawSlicesEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QCustom3DVolume10metaObjectEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QCustom3DVolume11sliceIndexXEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QCustom3DVolume11sliceIndexYEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QCustom3DVolume11sliceIndexZEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QCustom3DVolume11textureDataEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QCustom3DVolume12textureDepthEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QCustom3DVolume12textureWidthEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QCustom3DVolume13textureFormatEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QCustom3DVolume13textureHeightEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QCustom3DVolume14sliceFrameGapsEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QCustom3DVolume15alphaMultiplierEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QCustom3DVolume15drawSliceFramesEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QCustom3DVolume15preserveOpacityEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QCustom3DVolume15sliceFrameColorEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QCustom3DVolume16sliceFrameWidthsEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QCustom3DVolume16textureDataWidthEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QCustom3DVolume16useHighDefShaderEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK15QCustom3DVolume21sliceFrameThicknessesEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK16QBarCategoryAxis2atEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK16QScatter3DSeries10metaObjectEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK16QScatter3DSeries12selectedItemEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK16QScatter3DSeries8itemSizeEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK16QScatter3DSeries9dataProxyEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK16QSurface3DSeries10metaObjectEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK16QSurface3DSeries11textureFileEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK16QSurface3DSeries13selectedPointEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK16QSurface3DSeries14wireframeColorEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK16QSurface3DSeries22isFlatShadingSupportedEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK16QSurface3DSeries7shadingEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK16QSurface3DSeries7textureEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK16QSurface3DSeries8drawModeEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK16QSurface3DSeries9dataProxyEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK17QAbstract3DSeries10colorStyleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK17QAbstract3DSeries10metaObjectEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK17QAbstract3DSeries12baseGradientEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK17QAbstract3DSeries12isMeshSmoothEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK17QAbstract3DSeries12meshRotationEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK17QAbstract3DSeries15itemLabelFormatEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK17QAbstract3DSeries15userDefinedMeshEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK17QAbstract3DSeries18isItemLabelVisibleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK17QAbstract3DSeries19multiHighlightColorEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK17QAbstract3DSeries20singleHighlightColorEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK17QAbstract3DSeries22multiHighlightGradientEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK17QAbstract3DSeries23singleHighlightGradientEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK17QAbstract3DSeries4meshEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK17QAbstract3DSeries4nameEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK17QAbstract3DSeries4typeEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK17QAbstract3DSeries9baseColorEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK17QAbstract3DSeries9isVisibleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK17QScatterDataProxy10metaObjectEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK17QScatterDataProxy6itemAtEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK17QScatterDataProxy6seriesEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK17QScatterDataProxy9itemCountEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK17QSurfaceDataProxy10metaObjectEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK17QSurfaceDataProxy11columnCountEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK17QSurfaceDataProxy6itemAtE6QPoint` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK17QSurfaceDataProxy6itemAtExx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK17QSurfaceDataProxy6seriesEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK17QSurfaceDataProxy8rowCountEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK18QAbstractDataProxy10metaObjectEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK18QAbstractDataProxy4typeEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK21QValue3DAxisFormatter10metaObjectEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK21QValue3DAxisFormatter10positionAtEf` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK21QValue3DAxisFormatter14allowNegativesEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK21QValue3DAxisFormatter17createNewInstanceEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK21QValue3DAxisFormatter4axisEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK21QValue3DAxisFormatter6localeEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK21QValue3DAxisFormatter7valueAtEf` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK21QValue3DAxisFormatter9allowZeroEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK22QItemModelBarDataProxy10columnRoleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK22QItemModelBarDataProxy10metaObjectEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK22QItemModelBarDataProxy12rotationRoleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK22QItemModelBarDataProxy13rowCategoriesEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK22QItemModelBarDataProxy14rowRolePatternEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK22QItemModelBarDataProxy14rowRoleReplaceEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK22QItemModelBarDataProxy16columnCategoriesEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK22QItemModelBarDataProxy16valueRolePatternEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK22QItemModelBarDataProxy16valueRoleReplaceEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK22QItemModelBarDataProxy17autoRowCategoriesEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK22QItemModelBarDataProxy17columnRolePatternEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK22QItemModelBarDataProxy17columnRoleReplaceEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK22QItemModelBarDataProxy18multiMatchBehaviorEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK22QItemModelBarDataProxy18useModelCategoriesEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK22QItemModelBarDataProxy19rotationRolePatternEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK22QItemModelBarDataProxy19rotationRoleReplaceEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK22QItemModelBarDataProxy20autoColumnCategoriesEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK22QItemModelBarDataProxy7rowRoleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK22QItemModelBarDataProxy9itemModelEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK22QItemModelBarDataProxy9valueRoleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK24QLogValue3DAxisFormatter10metaObjectEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK24QLogValue3DAxisFormatter10positionAtEf` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK24QLogValue3DAxisFormatter11autoSubGridEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK24QLogValue3DAxisFormatter17createNewInstanceEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK24QLogValue3DAxisFormatter17edgeLabelsVisibleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK24QLogValue3DAxisFormatter4baseEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK24QLogValue3DAxisFormatter7valueAtEf` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QHeightMapSurfaceDataProxy10autoScaleYEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QHeightMapSurfaceDataProxy10metaObjectEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QHeightMapSurfaceDataProxy13heightMapFileEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QHeightMapSurfaceDataProxy9heightMapEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QHeightMapSurfaceDataProxy9maxXValueEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QHeightMapSurfaceDataProxy9maxYValueEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QHeightMapSurfaceDataProxy9maxZValueEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QHeightMapSurfaceDataProxy9minXValueEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QHeightMapSurfaceDataProxy9minYValueEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QHeightMapSurfaceDataProxy9minZValueEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelScatterDataProxy10metaObjectEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelScatterDataProxy12rotationRoleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelScatterDataProxy15xPosRolePatternEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelScatterDataProxy15xPosRoleReplaceEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelScatterDataProxy15yPosRolePatternEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelScatterDataProxy15yPosRoleReplaceEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelScatterDataProxy15zPosRolePatternEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelScatterDataProxy15zPosRoleReplaceEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelScatterDataProxy19rotationRolePatternEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelScatterDataProxy19rotationRoleReplaceEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelScatterDataProxy8xPosRoleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelScatterDataProxy8yPosRoleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelScatterDataProxy8zPosRoleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelScatterDataProxy9itemModelEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelSurfaceDataProxy10columnRoleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelSurfaceDataProxy10metaObjectEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelSurfaceDataProxy13rowCategoriesEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelSurfaceDataProxy14rowRolePatternEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelSurfaceDataProxy14rowRoleReplaceEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelSurfaceDataProxy15xPosRolePatternEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelSurfaceDataProxy15xPosRoleReplaceEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelSurfaceDataProxy15yPosRolePatternEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelSurfaceDataProxy15yPosRoleReplaceEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelSurfaceDataProxy15zPosRolePatternEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelSurfaceDataProxy15zPosRoleReplaceEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelSurfaceDataProxy16columnCategoriesEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelSurfaceDataProxy17autoRowCategoriesEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelSurfaceDataProxy17columnRolePatternEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelSurfaceDataProxy17columnRoleReplaceEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelSurfaceDataProxy18multiMatchBehaviorEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelSurfaceDataProxy18useModelCategoriesEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelSurfaceDataProxy20autoColumnCategoriesEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelSurfaceDataProxy7rowRoleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelSurfaceDataProxy8xPosRoleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelSurfaceDataProxy8yPosRoleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelSurfaceDataProxy8zPosRoleEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK26QItemModelSurfaceDataProxy9itemModelEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK7QBarSet10labelColorEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK7QBarSet11borderColorEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK7QBarSet11borderWidthEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK7QBarSet13isBarSelectedEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK7QBarSet2atEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK7QBarSet5colorEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK7QBarSet6valuesEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK8Q3DScene10metaObjectEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK8Q3DScene15isSlicingActiveEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK8Q3DScene16devicePixelRatioEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK8Q3DScene18graphPositionQueryEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK8Q3DScene18primarySubViewportEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK8Q3DScene20secondarySubViewportEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK8Q3DScene21invalidSelectionPointEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK8Q3DScene22selectionQueryPositionEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK8Q3DScene23isSecondarySubviewOnTopEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK8Q3DScene8viewportEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK9QPieSlice10labelColorEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK9QPieSlice11borderColorEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK9QPieSlice11borderWidthEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK9QPieSlice5colorEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK9QXYSeries11isDraggableEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK9QXYSeries13pointDelegateEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK9QXYSeries15isPointSelectedEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK9QXYSeries2atEx` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNK9QXYSeries4findE7QPointF` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNKR12QBar3DSeries9dataArrayEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNKR16QScatter3DSeries9dataArrayEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNKR16QSurface3DSeries9dataArrayEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNKR21QValue3DAxisFormatter12labelStringsEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNKR21QValue3DAxisFormatter13gridPositionsEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNKR21QValue3DAxisFormatter14labelPositionsEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZNKR21QValue3DAxisFormatter16subGridPositionsEv` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZTI12QBar3DSeries` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZTI12QGraphsTheme` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZTI12QValue3DAxis` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZTI13QBarDataProxy` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZTI13QCustom3DItem` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZTI14QCustom3DLabel` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZTI15QAbstract3DAxis` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZTI15QCategory3DAxis` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZTI15QCustom3DVolume` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZTI16QScatter3DSeries` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZTI16QSurface3DSeries` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZTI17QAbstract3DSeries` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZTI17QScatterDataProxy` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZTI17QSurfaceDataProxy` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZTI18QAbstractDataProxy` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZTI21QValue3DAxisFormatter` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZTI22QItemModelBarDataProxy` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZTI24QLogValue3DAxisFormatter` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZTI26QHeightMapSurfaceDataProxy` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZTI26QItemModelScatterDataProxy` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZTI26QItemModelSurfaceDataProxy` | 1 | freecad |
| libQt6Graphs.6.dylib | `__ZTI8Q3DScene` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN17Q3DBarsWidgetItem10setRowAxisEP15QCategory3DAxis` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN17Q3DBarsWidgetItem11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN17Q3DBarsWidgetItem11qt_metacastEPKc` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN17Q3DBarsWidgetItem11releaseAxisEP15QAbstract3DAxis` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN17Q3DBarsWidgetItem12insertSeriesEiP12QBar3DSeries` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN17Q3DBarsWidgetItem12removeSeriesEP12QBar3DSeries` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN17Q3DBarsWidgetItem12setValueAxisEP12QValue3DAxis` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN17Q3DBarsWidgetItem13setBarSpacingE6QSizeF` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN17Q3DBarsWidgetItem13setColumnAxisEP15QCategory3DAxis` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN17Q3DBarsWidgetItem13setFloorLevelEf` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN17Q3DBarsWidgetItem15setBarThicknessEf` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN17Q3DBarsWidgetItem16setPrimarySeriesEP12QBar3DSeries` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN17Q3DBarsWidgetItem16staticMetaObjectE` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN17Q3DBarsWidgetItem18setBarSeriesMarginE6QSizeF` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN17Q3DBarsWidgetItem21setBarSpacingRelativeEb` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN17Q3DBarsWidgetItem21setMultiSeriesUniformEb` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN17Q3DBarsWidgetItem5eventEP6QEvent` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN17Q3DBarsWidgetItem7addAxisEP15QAbstract3DAxis` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN17Q3DBarsWidgetItem9addSeriesEP12QBar3DSeries` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN17Q3DBarsWidgetItemC2EP7QObject` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN17Q3DBarsWidgetItemD2Ev` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem11eventFilterEP7QObjectP6QEvent` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem11qt_metacastEPKc` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem12releaseThemeEP12QGraphsTheme` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem13addCustomItemEP13QCustom3DItem` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem13setDragButtonE6QFlagsIN2Qt11MouseButtonEE` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem13setLightColorE6QColor` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem13setMeasureFpsEb` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem14clearSelectionEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem14setActiveThemeEP12QGraphsTheme` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem14setAspectRatioEd` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem14setLabelMarginEf` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem14setMsaaSamplesEi` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem14setZoomEnabledEb` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem15setCameraPresetEN10QtGraphs3D12CameraPresetE` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem15setGridLineTypeERKN10QtGraphs3D12GridLineTypeE` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem16removeCustomItemEP13QCustom3DItem` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem16setLightStrengthEf` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem16setSelectionModeERK6QFlagsIN10QtGraphs3D13SelectionFlagEE` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem16setShadowQualityERKN10QtGraphs3D13ShadowQualityE` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem16staticMetaObjectE` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem17releaseCustomItemEP13QCustom3DItem` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem17removeCustomItemsEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem17setCameraPositionEfff` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem17setShadowStrengthEf` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem18removeCustomItemAtE9QVector3D` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem18setCameraXRotationEf` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem18setCameraYRotationEf` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem18setCameraZoomLevelEf` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem18setOrthoProjectionEb` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem18setRotationEnabledEb` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem19setOptimizationHintEN10QtGraphs3D16OptimizationHintE` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem19setSelectionEnabledEb` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem20setRadialLabelOffsetEf` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem21setMaxCameraXRotationEf` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem21setMaxCameraYRotationEf` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem21setMaxCameraZoomLevelEf` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem21setMinCameraXRotationEf` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem21setMinCameraYRotationEf` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem21setMinCameraZoomLevelEf` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem22setDefaultInputHandlerEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem22setWrapCameraXRotationEb` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem22setWrapCameraYRotationEb` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem22setZoomAtTargetEnabledEb` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem22unsetDefaultTapHandlerEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem23setAmbientLightStrengthEf` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem23setCameraTargetPositionE9QVector3D` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem23unsetDefaultDragHandlerEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem24setHorizontalAspectRatioEd` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem24unsetDefaultInputHandlerEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem24unsetDefaultPinchHandlerEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem24unsetDefaultWheelHandlerEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem5eventEP6QEvent` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem8addThemeEP12QGraphsTheme` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem8setPolarEb` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem9doPickingE6QPoint` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem9setLocaleERK7QLocale` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem9setMarginEd` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItem9setWidgetEP12QQuickWidget` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN19Q3DGraphsWidgetItemD2Ev` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN20Q3DScatterWidgetItem11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN20Q3DScatterWidgetItem11qt_metacastEPKc` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN20Q3DScatterWidgetItem11releaseAxisEP12QValue3DAxis` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN20Q3DScatterWidgetItem12removeSeriesEP16QScatter3DSeries` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN20Q3DScatterWidgetItem16staticMetaObjectE` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN20Q3DScatterWidgetItem5eventEP6QEvent` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN20Q3DScatterWidgetItem7addAxisEP12QValue3DAxis` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN20Q3DScatterWidgetItem8setAxisXEP12QValue3DAxis` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN20Q3DScatterWidgetItem8setAxisYEP12QValue3DAxis` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN20Q3DScatterWidgetItem8setAxisZEP12QValue3DAxis` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN20Q3DScatterWidgetItem9addSeriesEP16QScatter3DSeries` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN20Q3DScatterWidgetItemC2EP7QObject` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN20Q3DScatterWidgetItemD2Ev` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN20Q3DSurfaceWidgetItem11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN20Q3DSurfaceWidgetItem11qt_metacastEPKc` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN20Q3DSurfaceWidgetItem11releaseAxisEP12QValue3DAxis` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN20Q3DSurfaceWidgetItem12removeSeriesEP16QSurface3DSeries` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN20Q3DSurfaceWidgetItem16staticMetaObjectE` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN20Q3DSurfaceWidgetItem21setFlipHorizontalGridEb` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN20Q3DSurfaceWidgetItem5eventEP6QEvent` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN20Q3DSurfaceWidgetItem7addAxisEP12QValue3DAxis` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN20Q3DSurfaceWidgetItem8setAxisXEP12QValue3DAxis` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN20Q3DSurfaceWidgetItem8setAxisYEP12QValue3DAxis` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN20Q3DSurfaceWidgetItem8setAxisZEP12QValue3DAxis` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN20Q3DSurfaceWidgetItem9addSeriesEP16QSurface3DSeries` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN20Q3DSurfaceWidgetItemC2EP7QObject` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZN20Q3DSurfaceWidgetItemD2Ev` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK17Q3DBarsWidgetItem10barSpacingEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK17Q3DBarsWidgetItem10columnAxisEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK17Q3DBarsWidgetItem10floorLevelEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK17Q3DBarsWidgetItem10metaObjectEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK17Q3DBarsWidgetItem10seriesListEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK17Q3DBarsWidgetItem12barThicknessEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK17Q3DBarsWidgetItem13primarySeriesEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK17Q3DBarsWidgetItem14selectedSeriesEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK17Q3DBarsWidgetItem15barSeriesMarginEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK17Q3DBarsWidgetItem20isBarSpacingRelativeEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK17Q3DBarsWidgetItem20isMultiSeriesUniformEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK17Q3DBarsWidgetItem4axesEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK17Q3DBarsWidgetItem7rowAxisEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK17Q3DBarsWidgetItem9valueAxisEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem10currentFpsEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem10lightColorEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem10measureFpsEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem10metaObjectEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem11activeThemeEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem11aspectRatioEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem11customItemsEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem11labelMarginEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem11msaaSamplesEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem12cameraPresetEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem12gridLineTypeEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem12selectedAxisEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem13isZoomEnabledEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem13lightStrengthEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem13renderToImageE5QSize` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem13selectionModeEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem13shadowQualityEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem14shadowStrengthEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem15cameraXRotationEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem15cameraYRotationEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem15cameraZoomLevelEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem15selectedElementEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem16optimizationHintEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem17isOrthoProjectionEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem17isRotationEnabledEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem17radialLabelOffsetEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem18isSelectionEnabledEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem18maxCameraXRotationEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem18maxCameraYRotationEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem18maxCameraZoomLevelEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem18minCameraXRotationEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem18minCameraYRotationEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem18minCameraZoomLevelEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem18selectedCustomItemEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem18selectedLabelIndexEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem19wrapCameraXRotationEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem19wrapCameraYRotationEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem20ambientLightStrengthEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem20cameraTargetPositionEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem20queriedGraphPositionEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem21horizontalAspectRatioEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem21isZoomAtTargetEnabledEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem23selectedCustomItemIndexEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem5sceneEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem6localeEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem6marginEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem6themesEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem6widgetEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem7isPolarEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK19Q3DGraphsWidgetItem9hasSeriesEP17QAbstract3DSeries` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK20Q3DScatterWidgetItem10metaObjectEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK20Q3DScatterWidgetItem10seriesListEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK20Q3DScatterWidgetItem14selectedSeriesEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK20Q3DScatterWidgetItem4axesEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK20Q3DScatterWidgetItem5axisXEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK20Q3DScatterWidgetItem5axisYEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK20Q3DScatterWidgetItem5axisZEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK20Q3DSurfaceWidgetItem10metaObjectEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK20Q3DSurfaceWidgetItem10seriesListEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK20Q3DSurfaceWidgetItem14selectedSeriesEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK20Q3DSurfaceWidgetItem18flipHorizontalGridEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK20Q3DSurfaceWidgetItem4axesEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK20Q3DSurfaceWidgetItem5axisXEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK20Q3DSurfaceWidgetItem5axisYEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZNK20Q3DSurfaceWidgetItem5axisZEv` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZTI17Q3DBarsWidgetItem` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZTI19Q3DGraphsWidgetItem` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZTI20Q3DScatterWidgetItem` | 1 | freecad |
| libQt6GraphsWidgets.6.dylib | `__ZTI20Q3DSurfaceWidgetItem` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN10QAudioSink11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN10QAudioSink11qt_metacastEPKc` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN10QAudioSink13setBufferSizeEx` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN10QAudioSink16staticMetaObjectE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN10QAudioSink4stopEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN10QAudioSink5resetEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN10QAudioSink5startEP9QIODevice` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN10QAudioSink5startEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN10QAudioSink6resumeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN10QAudioSink7suspendEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN10QAudioSink9setVolumeEd` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN10QAudioSinkC2ERK12QAudioDeviceRK12QAudioFormatP7QObject` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN10QAudioSinkC2ERK12QAudioFormatP7QObject` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN10QAudioSinkD2Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN10QVideoSink11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN10QVideoSink11qt_metacastEPKc` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN10QVideoSink13setVideoFrameERK11QVideoFrame` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN10QVideoSink15setSubtitleTextERK7QString` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN10QVideoSink16staticMetaObjectE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN10QVideoSink6setRhiEP4QRhi` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN10QVideoSinkC2EP7QObject` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN10QVideoSinkD2Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN11QAudioInput11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN11QAudioInput11qt_metacastEPKc` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN11QAudioInput16staticMetaObjectE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN11QAudioInput8setMutedEb` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN11QAudioInput9setDeviceERK12QAudioDevice` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN11QAudioInput9setVolumeEf` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN11QAudioInputC2EP7QObject` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN11QAudioInputC2ERK12QAudioDeviceP7QObject` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN11QAudioInputD2Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN11QVideoFrame10setEndTimeEx` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN11QVideoFrame11setMirroredEb` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN11QVideoFrame11setRotationEN7QtVideo8RotationE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN11QVideoFrame12setStartTimeEx` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN11QVideoFrame15setSubtitleTextERK7QString` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN11QVideoFrame16staticMetaObjectE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN11QVideoFrame18setStreamFrameRateEd` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN11QVideoFrame3mapENS_7MapModeE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN11QVideoFrame4bitsEi` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN11QVideoFrame5paintEP8QPainterRK6QRectFRKNS_12PaintOptionsE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN11QVideoFrame5unmapEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN11QVideoFrameC1EP20QAbstractVideoBufferRK17QVideoFrameFormat` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN11QVideoFrameC1ERK17QVideoFrameFormat` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN11QVideoFrameC1ERK6QImage` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN11QVideoFrameC1ERKS_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN11QVideoFrameC1Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN11QVideoFrameD1Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN11QVideoFrameaSERKS_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioBuffer4dataEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioBufferC1ERK10QByteArrayRK12QAudioFormatx` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioBufferC1ERKS_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioBufferC1EiRK12QAudioFormatx` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioBufferC1Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioBufferD1Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioBufferaSERKS_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioDevice16staticMetaObjectE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioDeviceC1ERKS_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioDeviceC1Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioDeviceD1Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioDeviceaSERKS_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioFormat16setChannelConfigENS_13ChannelConfigE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioFormat35defaultChannelConfigForChannelCountEi` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioOutput11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioOutput11qt_metacastEPKc` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioOutput16staticMetaObjectE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioOutput8setMutedEb` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioOutput9setDeviceERK12QAudioDevice` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioOutput9setVolumeEf` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioOutputC2EP7QObject` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioOutputC2ERK12QAudioDeviceP7QObject` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioOutputD2Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioSource11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioSource11qt_metacastEPKc` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioSource13setBufferSizeEx` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioSource16staticMetaObjectE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioSource4stopEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioSource5resetEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioSource5startEP9QIODevice` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioSource5startEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioSource6resumeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioSource7suspendEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioSource9setVolumeEd` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioSourceC2ERK12QAudioDeviceRK12QAudioFormatP7QObject` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioSourceC2ERK12QAudioFormatP7QObject` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QAudioSourceD2Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaFormat14audioCodecNameENS_10AudioCodecE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaFormat14fileFormatNameENS_10FileFormatE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaFormat14videoCodecNameENS_10VideoCodecE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaFormat16staticMetaObjectE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaFormat18resolveForEncodingENS_12ResolveFlagsE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaFormat20supportedAudioCodecsENS_14ConversionModeE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaFormat20supportedFileFormatsENS_14ConversionModeE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaFormat20supportedVideoCodecsENS_14ConversionModeE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaFormat21audioCodecDescriptionENS_10AudioCodecE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaFormat21fileFormatDescriptionENS_10FileFormatE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaFormat21videoCodecDescriptionENS_10VideoCodecE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaFormatC1ENS_10FileFormatE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaFormatC1ERKS_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaFormatD1Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaFormataSERKS_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaPlayer11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaPlayer11qt_metacastEPKc` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaPlayer11setPositionEx` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaPlayer12setVideoSinkEP10QVideoSink` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaPlayer14setAudioOutputEP12QAudioOutput` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaPlayer14setVideoOutputEP7QObject` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaPlayer15setPlaybackRateEd` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaPlayer15setSourceDeviceEP9QIODeviceRK4QUrl` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaPlayer16staticMetaObjectE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaPlayer19setActiveAudioTrackEi` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaPlayer19setActiveVideoTrackEi` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaPlayer20setAudioBufferOutputEP18QAudioBufferOutput` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaPlayer22setActiveSubtitleTrackEi` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaPlayer4playEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaPlayer4stopEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaPlayer5pauseEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaPlayer8setLoopsEi` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaPlayer9setSourceERK4QUrl` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaPlayerC2EP7QObject` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QMediaPlayerD2Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QSoundEffect11audioDeviceEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QSoundEffect11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QSoundEffect11qt_metacastEPKc` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QSoundEffect12setLoopCountEi` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QSoundEffect14setAudioDeviceERK12QAudioDevice` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QSoundEffect16staticMetaObjectE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QSoundEffect18supportedMimeTypesEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QSoundEffect4playEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QSoundEffect4stopEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QSoundEffect8setMutedEb` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QSoundEffect9setSourceERK4QUrl` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QSoundEffect9setVolumeEf` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QSoundEffectC2EP7QObject` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QSoundEffectC2ERK12QAudioDeviceP7QObject` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QSoundEffectD2Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QWaveDecoder11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QWaveDecoder11qt_metacastEPKc` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QWaveDecoder12headerLengthEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QWaveDecoder16staticMetaObjectE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QWaveDecoder4openE6QFlagsIN13QIODeviceBase12OpenModeFlagEE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QWaveDecoder4seekEx` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QWaveDecoder5closeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QWaveDecoder8readDataEPcx` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QWaveDecoder9getDeviceEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QWaveDecoder9writeDataEPKcx` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QWaveDecoderC2EP9QIODeviceP7QObject` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QWaveDecoderC2EP9QIODeviceRK12QAudioFormatP7QObject` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN12QWaveDecoderD2Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QAudioDecoder11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QAudioDecoder11qt_metacastEPKc` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QAudioDecoder14setAudioFormatERK12QAudioFormat` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QAudioDecoder15setSourceDeviceEP9QIODevice` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QAudioDecoder16staticMetaObjectE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QAudioDecoder4stopEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QAudioDecoder5startEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QAudioDecoder9setSourceERK4QUrl` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QAudioDecoderC2EP7QObject` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QAudioDecoderD2Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QCameraDevice16staticMetaObjectE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QCameraDeviceC1ERKS_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QCameraDeviceC1Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QCameraDeviceD1Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QCameraDeviceaSERKS_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QCameraFormat16staticMetaObjectE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QCameraFormatC1ERKS_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QCameraFormatC1Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QCameraFormatD1Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QCameraFormataSERKS_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QImageCapture10setQualityENS_7QualityE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QImageCapture11addMetaDataERK14QMediaMetaData` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QImageCapture11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QImageCapture11qt_metacastEPKc` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QImageCapture11setMetaDataERK14QMediaMetaData` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QImageCapture13captureToFileERK7QString` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QImageCapture13setFileFormatENS_10FileFormatE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QImageCapture13setResolutionERK5QSize` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QImageCapture13setResolutionEii` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QImageCapture14fileFormatNameENS_10FileFormatE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QImageCapture16staticMetaObjectE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QImageCapture16supportedFormatsEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QImageCapture21fileFormatDescriptionENS_10FileFormatE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QImageCapture7captureEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QImageCaptureC2EP7QObject` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QImageCaptureD2Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QMediaDevices11audioInputsEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QMediaDevices11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QMediaDevices11qt_metacastEPKc` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QMediaDevices11videoInputsEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QMediaDevices12audioOutputsEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QMediaDevices13connectNotifyERK11QMetaMethod` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QMediaDevices16staticMetaObjectE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QMediaDevices17defaultAudioInputEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QMediaDevices17defaultVideoInputEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QMediaDevices18defaultAudioOutputEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QMediaDevicesC2EP7QObject` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN13QMediaDevicesD2Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QMediaMetaData16staticMetaObjectE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QMediaMetaData19metaDataKeyToStringENS_3KeyE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QMediaMetaData7keyTypeENS_3KeyE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QMediaRecorder10setQualityENS_7QualityE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QMediaRecorder11addMetaDataERK14QMediaMetaData` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QMediaRecorder11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QMediaRecorder11qt_metacastEPKc` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QMediaRecorder11setAutoStopEb` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QMediaRecorder11setMetaDataERK14QMediaMetaData` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QMediaRecorder14setMediaFormatERK12QMediaFormat` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QMediaRecorder15setAudioBitRateEi` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QMediaRecorder15setEncodingModeENS_12EncodingModeE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QMediaRecorder15setOutputDeviceEP9QIODevice` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QMediaRecorder15setVideoBitRateEi` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QMediaRecorder16staticMetaObjectE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QMediaRecorder17setOutputLocationERK4QUrl` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QMediaRecorder17setVideoFrameRateEd` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QMediaRecorder18setAudioSampleRateEi` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QMediaRecorder18setVideoResolutionERK5QSize` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QMediaRecorder20setAudioChannelCountEi` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QMediaRecorder4stopEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QMediaRecorder5pauseEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QMediaRecorder6recordEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QMediaRecorderC2EP7QObject` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QMediaRecorderD2Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QScreenCapture11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QScreenCapture11qt_metacastEPKc` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QScreenCapture16staticMetaObjectE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QScreenCapture9setActiveEb` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QScreenCapture9setScreenEP7QScreen` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QScreenCaptureC2EP7QObject` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QScreenCaptureD2Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QWindowCapture11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QWindowCapture11qt_metacastEPKc` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QWindowCapture16staticMetaObjectE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QWindowCapture17capturableWindowsEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QWindowCapture9setActiveEb` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QWindowCapture9setWindowE17QCapturableWindow` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QWindowCaptureC2EP7QObject` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN14QWindowCaptureD2Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN15QMediaTimeRange11addIntervalERKNS_8IntervalE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN15QMediaTimeRange11addIntervalExx` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN15QMediaTimeRange12addTimeRangeERKS_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN15QMediaTimeRange14removeIntervalERKNS_8IntervalE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN15QMediaTimeRange14removeIntervalExx` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN15QMediaTimeRange15removeTimeRangeERKS_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN15QMediaTimeRange5clearEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN15QMediaTimeRangeC1ERKNS_8IntervalE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN15QMediaTimeRangeC1ERKS_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN15QMediaTimeRangeC1Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN15QMediaTimeRangeC1Exx` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN15QMediaTimeRangeD1Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN15QMediaTimeRangeaSERKS_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN15QMediaTimeRangemIERKNS_8IntervalE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN15QMediaTimeRangemIERKS_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN15QMediaTimeRangepLERKNS_8IntervalE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN15QMediaTimeRangepLERKS_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN16QVideoFrameInput11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN16QVideoFrameInput11qt_metacastEPKc` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN16QVideoFrameInput14sendVideoFrameERK11QVideoFrame` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN16QVideoFrameInput16staticMetaObjectE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN16QVideoFrameInputC2EP7QObject` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN16QVideoFrameInputC2ERK17QVideoFrameFormatP7QObject` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN16QVideoFrameInputD2Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QAudioBufferInput11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QAudioBufferInput11qt_metacastEPKc` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QAudioBufferInput15sendAudioBufferERK12QAudioBuffer` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QAudioBufferInput16staticMetaObjectE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QAudioBufferInputC2EP7QObject` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QAudioBufferInputC2ERK12QAudioFormatP7QObject` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QAudioBufferInputD2Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QCapturableWindow16staticMetaObjectE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QCapturableWindowC1ERKS_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QCapturableWindowC1Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QCapturableWindowD1Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QCapturableWindowaSERKS_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QVideoFrameFormat11setMirroredEb` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QVideoFrameFormat11setRotationEN7QtVideo8RotationE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QVideoFrameFormat11setViewportERK5QRect` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QVideoFrameFormat12setFrameRateEd` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QVideoFrameFormat12setFrameSizeERK5QSize` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QVideoFrameFormat12setFrameSizeEii` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QVideoFrameFormat13setColorRangeENS_10ColorRangeE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QVideoFrameFormat13setColorSpaceENS_10ColorSpaceE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QVideoFrameFormat15setMaxLuminanceEf` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QVideoFrameFormat16setColorTransferENS_13ColorTransferE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QVideoFrameFormat16staticMetaObjectE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QVideoFrameFormat18setStreamFrameRateEd` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QVideoFrameFormat18setYCbCrColorSpaceENS_15YCbCrColorSpaceE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QVideoFrameFormat19pixelFormatToStringENS_11PixelFormatE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QVideoFrameFormat20setScanLineDirectionENS_9DirectionE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QVideoFrameFormat26imageFormatFromPixelFormatENS_11PixelFormatE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QVideoFrameFormat26pixelFormatFromImageFormatEN6QImage6FormatE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QVideoFrameFormatC1ERK5QSizeNS_11PixelFormatE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QVideoFrameFormatC1ERKS_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QVideoFrameFormatC1Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QVideoFrameFormatD1Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN17QVideoFrameFormataSERKS_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN18QAudioBufferOutput11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN18QAudioBufferOutput11qt_metacastEPKc` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN18QAudioBufferOutput16staticMetaObjectE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN18QAudioBufferOutputC2EP7QObject` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN18QAudioBufferOutputC2ERK12QAudioFormatP7QObject` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN18QAudioBufferOutputD2Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN20QAbstractVideoBuffer5unmapEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN20QAbstractVideoBufferD2Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN20QMediaCaptureSession11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN20QMediaCaptureSession11qt_metacastEPKc` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN20QMediaCaptureSession11setRecorderEP14QMediaRecorder` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN20QMediaCaptureSession12imageCaptureEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN20QMediaCaptureSession12setVideoSinkEP10QVideoSink` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN20QMediaCaptureSession13screenCaptureEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN20QMediaCaptureSession13setAudioInputEP11QAudioInput` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN20QMediaCaptureSession13windowCaptureEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN20QMediaCaptureSession14setAudioOutputEP12QAudioOutput` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN20QMediaCaptureSession14setVideoOutputEP7QObject` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN20QMediaCaptureSession15setImageCaptureEP13QImageCapture` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN20QMediaCaptureSession16setScreenCaptureEP14QScreenCapture` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN20QMediaCaptureSession16setWindowCaptureEP14QWindowCapture` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN20QMediaCaptureSession16staticMetaObjectE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN20QMediaCaptureSession18setVideoFrameInputEP16QVideoFrameInput` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN20QMediaCaptureSession19setAudioBufferInputEP17QAudioBufferInput` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN20QMediaCaptureSession8recorderEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN20QMediaCaptureSession9setCameraEP7QCamera` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN20QMediaCaptureSessionC2EP7QObject` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN20QMediaCaptureSessionD2Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN6QAudio13convertVolumeEfNS_11VolumeScaleES0_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN7QCamera11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN7QCamera11qt_metacastEPKc` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN7QCamera12setFlashModeENS_9FlashModeE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN7QCamera12setFocusModeENS_9FocusModeE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN7QCamera12setTorchModeENS_9TorchModeE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN7QCamera13setZoomFactorEf` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN7QCamera15setCameraDeviceERK13QCameraDevice` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN7QCamera15setCameraFormatERK13QCameraFormat` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN7QCamera15setExposureModeENS_12ExposureModeE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN7QCamera16setFocusDistanceEf` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN7QCamera16staticMetaObjectE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN7QCamera19setAutoExposureTimeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN7QCamera19setColorTemperatureEi` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN7QCamera19setCustomFocusPointERK7QPointF` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN7QCamera19setWhiteBalanceModeENS_16WhiteBalanceModeE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN7QCamera21setAutoIsoSensitivityEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN7QCamera21setManualExposureTimeEf` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN7QCamera23setExposureCompensationEf` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN7QCamera23setManualIsoSensitivityEi` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN7QCamera6zoomToEff` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN7QCamera9setActiveEb` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN7QCameraC2EN13QCameraDevice8PositionEP7QObject` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN7QCameraC2EP7QObject` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN7QCameraC2ERK13QCameraDeviceP7QObject` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN7QCameraD2Ev` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZN7QtVideo16staticMetaObjectE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK10QAudioSink10bufferSizeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK10QAudioSink10metaObjectEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK10QAudioSink12elapsedUSecsEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK10QAudioSink14processedUSecsEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK10QAudioSink5errorEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK10QAudioSink5stateEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK10QAudioSink6formatEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK10QAudioSink6volumeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK10QAudioSink9bytesFreeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK10QVideoSink10metaObjectEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK10QVideoSink10videoFrameEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK10QVideoSink12subtitleTextEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK10QVideoSink3rhiEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK10QVideoSink9videoSizeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK11QAudioInput10metaObjectEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK11QAudioInput6deviceEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK11QAudioInput6volumeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK11QAudioInput7isMutedEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK11QVideoFrame10handleTypeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK11QVideoFrame10isReadableEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK11QVideoFrame10isWritableEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK11QVideoFrame10planeCountEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK11QVideoFrame11mappedBytesEi` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK11QVideoFrame11pixelFormatEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK11QVideoFrame11videoBufferEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK11QVideoFrame12bytesPerLineEi` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK11QVideoFrame12subtitleTextEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK11QVideoFrame13surfaceFormatEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK11QVideoFrame15streamFrameRateEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK11QVideoFrame4sizeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK11QVideoFrame5widthEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK11QVideoFrame6heightEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK11QVideoFrame7endTimeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK11QVideoFrame7isValidEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK11QVideoFrame7mapModeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK11QVideoFrame7toImageEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK11QVideoFrame8isMappedEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK11QVideoFrame8mirroredEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK11QVideoFrame8rotationEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK11QVideoFrame9startTimeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK11QVideoFrameeqERKS_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK11QVideoFrameneERKS_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioBuffer10frameCountEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioBuffer11sampleCountEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioBuffer6formatEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioBuffer8durationEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioBuffer9byteCountEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioBuffer9constDataEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioBuffer9startTimeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioDevice11descriptionEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioDevice15preferredFormatEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioDevice17isFormatSupportedERK12QAudioFormat` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioDevice17maximumSampleRateEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioDevice17minimumSampleRateEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioDevice19maximumChannelCountEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioDevice19minimumChannelCountEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioDevice20channelConfigurationEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioDevice22supportedSampleFormatsEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioDevice2idEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioDevice4modeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioDevice6isNullEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioDevice9isDefaultEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioDeviceeqERKS_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioDeviceneERKS_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioFormat13channelOffsetENS_20AudioChannelPositionE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioFormat14bytesForFramesEi` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioFormat14framesForBytesEi` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioFormat16bytesForDurationEx` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioFormat16durationForBytesEi` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioFormat17durationForFramesEi` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioFormat17framesForDurationEx` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioFormat21normalizedSampleValueEPKv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioOutput10metaObjectEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioOutput6deviceEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioOutput6volumeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioOutput7isMutedEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioSource10bufferSizeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioSource10metaObjectEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioSource12elapsedUSecsEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioSource14bytesAvailableEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioSource14processedUSecsEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioSource5errorEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioSource5stateEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioSource6formatEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QAudioSource6volumeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QMediaFormat11isSupportedENS_14ConversionModeE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QMediaFormat8mimeTypeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QMediaFormateqERKS_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QMediaPlayer10isSeekableEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QMediaPlayer10metaObjectEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QMediaPlayer11audioOutputEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QMediaPlayer11audioTracksEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QMediaPlayer11errorStringEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QMediaPlayer11isAvailableEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QMediaPlayer11mediaStatusEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QMediaPlayer11videoOutputEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QMediaPlayer11videoTracksEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QMediaPlayer12playbackRateEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QMediaPlayer12sourceDeviceEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QMediaPlayer13playbackStateEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QMediaPlayer14bufferProgressEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QMediaPlayer14subtitleTracksEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QMediaPlayer16activeAudioTrackEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QMediaPlayer16activeVideoTrackEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QMediaPlayer17audioBufferOutputEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QMediaPlayer17bufferedTimeRangeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QMediaPlayer19activeSubtitleTrackEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QMediaPlayer5errorEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QMediaPlayer5loopsEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QMediaPlayer6sourceEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QMediaPlayer8durationEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QMediaPlayer8hasAudioEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QMediaPlayer8hasVideoEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QMediaPlayer8metaDataEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QMediaPlayer8positionEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QMediaPlayer9isPlayingEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QMediaPlayer9videoSinkEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QSoundEffect10metaObjectEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QSoundEffect14loopsRemainingEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QSoundEffect6sourceEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QSoundEffect6statusEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QSoundEffect6volumeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QSoundEffect7isMutedEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QSoundEffect8isLoadedEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QSoundEffect9isPlayingEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QSoundEffect9loopCountEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QWaveDecoder10metaObjectEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QWaveDecoder11audioFormatEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QWaveDecoder12isSequentialEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QWaveDecoder14bytesAvailableEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QWaveDecoder3posEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QWaveDecoder4sizeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK12QWaveDecoder8durationEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QAudioDecoder10isDecodingEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QAudioDecoder10metaObjectEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QAudioDecoder11audioFormatEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QAudioDecoder11errorStringEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QAudioDecoder11isSupportedEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QAudioDecoder12sourceDeviceEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QAudioDecoder15bufferAvailableEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QAudioDecoder4readEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QAudioDecoder5errorEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QAudioDecoder6sourceEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QAudioDecoder8durationEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QAudioDecoder8positionEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QCameraDevice11descriptionEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QCameraDevice12videoFormatsEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QCameraDevice15correctionAngleEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QCameraDevice16photoResolutionsEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QCameraDevice2idEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QCameraDevice6isNullEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QCameraDevice8positionEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QCameraDevice9isDefaultEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QCameraDeviceeqERKS_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QCameraFormat10resolutionEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QCameraFormat11pixelFormatEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QCameraFormat12maxFrameRateEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QCameraFormat12minFrameRateEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QCameraFormateqERKS_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QImageCapture10fileFormatEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QImageCapture10metaObjectEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QImageCapture10resolutionEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QImageCapture11errorStringEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QImageCapture11isAvailableEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QImageCapture14captureSessionEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QImageCapture17isReadyForCaptureEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QImageCapture5errorEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QImageCapture7qualityEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QImageCapture8metaDataEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK13QMediaDevices10metaObjectEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QMediaMetaData11stringValueENS_3KeyE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QMediaRecorder10metaObjectEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QMediaRecorder11errorStringEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QMediaRecorder11isAvailableEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QMediaRecorder11mediaFormatEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QMediaRecorder12audioBitRateEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QMediaRecorder12encodingModeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QMediaRecorder12outputDeviceEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QMediaRecorder12videoBitRateEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QMediaRecorder13recorderStateEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QMediaRecorder14actualLocationEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QMediaRecorder14captureSessionEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QMediaRecorder14outputLocationEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QMediaRecorder14videoFrameRateEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QMediaRecorder15audioSampleRateEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QMediaRecorder15videoResolutionEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QMediaRecorder17audioChannelCountEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QMediaRecorder5errorEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QMediaRecorder7qualityEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QMediaRecorder8autoStopEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QMediaRecorder8durationEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QMediaRecorder8metaDataEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QScreenCapture10metaObjectEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QScreenCapture11errorStringEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QScreenCapture14captureSessionEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QScreenCapture5errorEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QScreenCapture6screenEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QScreenCapture8isActiveEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QWindowCapture10metaObjectEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QWindowCapture11errorStringEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QWindowCapture14captureSessionEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QWindowCapture5errorEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QWindowCapture6windowEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK14QWindowCapture8isActiveEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK15QMediaTimeRange10latestTimeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK15QMediaTimeRange12earliestTimeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK15QMediaTimeRange12isContinuousEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK15QMediaTimeRange7isEmptyEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK15QMediaTimeRange8containsEx` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK15QMediaTimeRange9intervalsEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK16QVideoFrameInput10metaObjectEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK16QVideoFrameInput14captureSessionEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK16QVideoFrameInput6formatEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK17QAudioBufferInput10metaObjectEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK17QAudioBufferInput14captureSessionEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK17QAudioBufferInput6formatEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK17QCapturableWindow11descriptionEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK17QCapturableWindow7isValidEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK17QVideoFrameFormat10colorRangeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK17QVideoFrameFormat10colorSpaceEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK17QVideoFrameFormat10frameWidthEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK17QVideoFrameFormat10isMirroredEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK17QVideoFrameFormat10planeCountEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK17QVideoFrameFormat11frameHeightEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK17QVideoFrameFormat11pixelFormatEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK17QVideoFrameFormat12maxLuminanceEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK17QVideoFrameFormat13colorTransferEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK17QVideoFrameFormat15streamFrameRateEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK17QVideoFrameFormat15yCbCrColorSpaceEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK17QVideoFrameFormat17scanLineDirectionEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK17QVideoFrameFormat17updateUniformDataEP10QByteArrayRK11QVideoFrameRK10QMatrix4x4f` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK17QVideoFrameFormat20vertexShaderFileNameEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK17QVideoFrameFormat22fragmentShaderFileNameEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK17QVideoFrameFormat7isValidEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK17QVideoFrameFormat8rotationEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK17QVideoFrameFormat8viewportEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK17QVideoFrameFormat9frameRateEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK17QVideoFrameFormat9frameSizeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK17QVideoFrameFormateqERKS_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK17QVideoFrameFormatneERKS_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK18QAudioBufferOutput10metaObjectEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK18QAudioBufferOutput6formatEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK20QMediaCaptureSession10audioInputEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK20QMediaCaptureSession10metaObjectEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK20QMediaCaptureSession11audioOutputEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK20QMediaCaptureSession11videoOutputEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK20QMediaCaptureSession15videoFrameInputEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK20QMediaCaptureSession16audioBufferInputEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK20QMediaCaptureSession6cameraEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK20QMediaCaptureSession9videoSinkEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera10focusPointEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera10metaObjectEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera10zoomFactorEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera11errorStringEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera11isAvailableEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera12cameraDeviceEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera12cameraFormatEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera12exposureModeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera12exposureTimeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera12isFlashReadyEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera13focusDistanceEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera14captureSessionEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera14isoSensitivityEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera16colorTemperatureEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera16customFocusPointEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera16whiteBalanceModeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera17maximumZoomFactorEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera17minimumZoomFactorEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera17supportedFeaturesEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera18manualExposureTimeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera19maximumExposureTimeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera19minimumExposureTimeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera20exposureCompensationEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera20isFlashModeSupportedENS_9FlashModeE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera20isFocusModeSupportedENS_9FocusModeE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera20isTorchModeSupportedENS_9TorchModeE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera20manualIsoSensitivityEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera21maximumIsoSensitivityEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera21minimumIsoSensitivityEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera23isExposureModeSupportedENS_12ExposureModeE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera27isWhiteBalanceModeSupportedENS_16WhiteBalanceModeE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera5errorEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera8isActiveEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera9flashModeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera9focusModeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZNK7QCamera9torchModeEv` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZTI10QAudioSink` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZTI10QVideoSink` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZTI11QAudioInput` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZTI12QAudioOutput` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZTI12QAudioSource` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZTI12QMediaPlayer` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZTI12QSoundEffect` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZTI12QWaveDecoder` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZTI13QAudioDecoder` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZTI13QImageCapture` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZTI13QMediaDevices` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZTI14QMediaRecorder` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZTI14QScreenCapture` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZTI14QWindowCapture` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZTI16QVideoFrameInput` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZTI17QAudioBufferInput` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZTI18QAudioBufferOutput` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZTI20QAbstractVideoBuffer` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZTI20QMediaCaptureSession` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZTI7QCamera` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__ZeqRK17QCapturableWindowS1_` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__Zls6QDebugN11QVideoFrame10HandleTypeE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__Zls6QDebugN12QAudioDevice4ModeE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__Zls6QDebugN12QAudioFormat12SampleFormatE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__Zls6QDebugN17QVideoFrameFormat10ColorSpaceE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__Zls6QDebugN17QVideoFrameFormat11PixelFormatE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__Zls6QDebugN17QVideoFrameFormat15YCbCrColorSpaceE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__Zls6QDebugN17QVideoFrameFormat9DirectionE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__Zls6QDebugN6QAudio11VolumeScaleE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__Zls6QDebugN6QAudio5ErrorE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__Zls6QDebugN6QAudio5StateE` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__Zls6QDebugRK11QVideoFrame` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__Zls6QDebugRK12QAudioFormat` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__Zls6QDebugRK13QCameraDevice` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__Zls6QDebugRK14QMediaMetaData` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__Zls6QDebugRK15QMediaTimeRange` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__Zls6QDebugRK17QCapturableWindow` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__Zls6QDebugRK17QVideoFrameFormat` | 1 | freecad |
| libQt6Multimedia.6.dylib | `__Zls6QDebugRKN15QMediaTimeRange8IntervalE` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZN12QVideoWidget11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZN12QVideoWidget11qt_metacastEPKc` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZN12QVideoWidget11resizeEventEP12QResizeEvent` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZN12QVideoWidget13setFullScreenEb` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZN12QVideoWidget16staticMetaObjectE` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZN12QVideoWidget18setAspectRatioModeEN2Qt15AspectRatioModeE` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZN12QVideoWidget5eventEP6QEvent` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZN12QVideoWidget9hideEventEP10QHideEvent` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZN12QVideoWidget9moveEventEP10QMoveEvent` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZN12QVideoWidget9showEventEP10QShowEvent` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZN12QVideoWidgetC2EP7QWidget` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZN12QVideoWidgetD2Ev` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZN18QGraphicsVideoItem10itemChangeEN13QGraphicsItem18GraphicsItemChangeERK8QVariant` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZN18QGraphicsVideoItem10timerEventEP11QTimerEvent` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZN18QGraphicsVideoItem11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZN18QGraphicsVideoItem11qt_metacastEPKc` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZN18QGraphicsVideoItem16staticMetaObjectE` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZN18QGraphicsVideoItem18setAspectRatioModeEN2Qt15AspectRatioModeE` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZN18QGraphicsVideoItem5paintEP8QPainterPK24QStyleOptionGraphicsItemP7QWidget` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZN18QGraphicsVideoItem7setSizeERK6QSizeF` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZN18QGraphicsVideoItem9setOffsetERK7QPointF` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZN18QGraphicsVideoItemC2EP13QGraphicsItem` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZN18QGraphicsVideoItemD2Ev` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZNK12QVideoWidget10metaObjectEv` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZNK12QVideoWidget15aspectRatioModeEv` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZNK12QVideoWidget8sizeHintEv` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZNK12QVideoWidget9videoSinkEv` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZNK18QGraphicsVideoItem10metaObjectEv` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZNK18QGraphicsVideoItem10nativeSizeEv` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZNK18QGraphicsVideoItem12boundingRectEv` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZNK18QGraphicsVideoItem15aspectRatioModeEv` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZNK18QGraphicsVideoItem4sizeEv` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZNK18QGraphicsVideoItem6offsetEv` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZNK18QGraphicsVideoItem9videoSinkEv` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZTI12QVideoWidget` | 1 | freecad |
| libQt6MultimediaWidgets.6.dylib | `__ZTI18QGraphicsVideoItem` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN14QQuick3DObject10classBeginEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN14QQuick3DObject10itemChangeENS_10ItemChangeERKNS_14ItemChangeDataE` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN14QQuick3DObject11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN14QQuick3DObject11qt_metacastEPKc` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN14QQuick3DObject12markAllDirtyEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN14QQuick3DObject13setParentItemEPS_` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN14QQuick3DObject16staticMetaObjectE` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN14QQuick3DObject17componentCompleteEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN14QQuick3DObject17updateSpatialNodeEP21QSSGRenderGraphObject` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN14QQuick3DObject6updateEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN14QQuick3DObject7preSyncEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN14QQuick3DObject8setStateERK7QString` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN14QQuick3DObjectD2Ev` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN16QQuick3DGeometry11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN16QQuick3DGeometry11qt_metacastEPKc` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN16QQuick3DGeometry12addAttributeENS_9Attribute8SemanticEiNS0_13ComponentTypeE` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN16QQuick3DGeometry12addAttributeERKNS_9AttributeE` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN16QQuick3DGeometry12markAllDirtyEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN16QQuick3DGeometry12setIndexDataERK10QByteArray` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN16QQuick3DGeometry12setIndexDataEiRK10QByteArray` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN16QQuick3DGeometry13setTargetDataERK10QByteArray` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN16QQuick3DGeometry13setTargetDataEiRK10QByteArray` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN16QQuick3DGeometry13setVertexDataERK10QByteArray` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN16QQuick3DGeometry13setVertexDataEiRK10QByteArray` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN16QQuick3DGeometry16setPrimitiveTypeENS_13PrimitiveTypeE` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN16QQuick3DGeometry16staticMetaObjectE` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN16QQuick3DGeometry17updateSpatialNodeEP21QSSGRenderGraphObject` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN16QQuick3DGeometry18addTargetAttributeERKNS_15TargetAttributeE` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN16QQuick3DGeometry18addTargetAttributeEjNS_9Attribute8SemanticEii` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN16QQuick3DGeometry5clearEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN16QQuick3DGeometry9addSubsetEiiRK9QVector3DS2_RK7QString` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN16QQuick3DGeometry9setBoundsERK9QVector3DS2_` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN16QQuick3DGeometry9setStrideEi` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN16QQuick3DGeometryC2EP14QQuick3DObject` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN16QQuick3DGeometryD2Ev` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN18QQuick3DInstancing11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN18QQuick3DInstancing11qt_metacastEPKc` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN18QQuick3DInstancing13instanceColorEi` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN18QQuick3DInstancing13instanceScaleEi` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN18QQuick3DInstancing14instanceBufferEPi` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN18QQuick3DInstancing16instancePositionEi` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN18QQuick3DInstancing16instanceRotationEi` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN18QQuick3DInstancing16staticMetaObjectE` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN18QQuick3DInstancing17updateSpatialNodeEP21QSSGRenderGraphObject` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN18QQuick3DInstancing18instanceCustomDataEi` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN18QQuick3DInstancing18setHasTransparencyEb` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN18QQuick3DInstancing19calculateTableEntryERK9QVector3DS2_S2_RK6QColorRK9QVector4D` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN18QQuick3DInstancing22setDepthSortingEnabledEb` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN18QQuick3DInstancing24setInstanceCountOverrideEi` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN18QQuick3DInstancing33calculateTableEntryFromQuaternionERK9QVector3DS2_RK11QQuaternionRK6QColorRK9QVector4D` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN18QQuick3DInstancing9markDirtyEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN18QQuick3DInstancingC2EP14QQuick3DObject` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN18QQuick3DInstancingD2Ev` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN19QQuick3DTextureData11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN19QQuick3DTextureData11qt_metacastEPKc` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN19QQuick3DTextureData12markAllDirtyEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN19QQuick3DTextureData14setTextureDataERK10QByteArray` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN19QQuick3DTextureData16staticMetaObjectE` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN19QQuick3DTextureData17updateSpatialNodeEP21QSSGRenderGraphObject` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN19QQuick3DTextureData18setHasTransparencyEb` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN19QQuick3DTextureData7setSizeERK5QSize` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN19QQuick3DTextureData8setDepthEi` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN19QQuick3DTextureData9setFormatENS_6FormatE` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN19QQuick3DTextureDataC2EP14QQuick3DObject` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN19QQuick3DTextureDataD2Ev` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN23QQuick3DRenderExtension11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN23QQuick3DRenderExtension11qt_metacastEPKc` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN23QQuick3DRenderExtension16staticMetaObjectE` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN23QQuick3DRenderExtension17updateSpatialNodeEP21QSSGRenderGraphObject` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN23QQuick3DRenderExtensionC2EP14QQuick3DObject` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN23QQuick3DRenderExtensionD2Ev` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZN8QQuick3D18idealSurfaceFormatEi` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK14QQuick3DObject10childItemsEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK14QQuick3DObject10metaObjectEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK14QQuick3DObject10parentItemEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK14QQuick3DObject19isComponentCompleteEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK14QQuick3DObject5stateEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK16QQuick3DGeometry10metaObjectEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK16QQuick3DGeometry10subsetNameEi` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK16QQuick3DGeometry10targetDataEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK16QQuick3DGeometry10vertexDataEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK16QQuick3DGeometry11subsetCountEi` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK16QQuick3DGeometry11subsetCountEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK16QQuick3DGeometry12subsetOffsetEi` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK16QQuick3DGeometry13primitiveTypeEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK16QQuick3DGeometry14attributeCountEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK16QQuick3DGeometry15subsetBoundsMaxEi` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK16QQuick3DGeometry15subsetBoundsMinEi` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK16QQuick3DGeometry15targetAttributeEi` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK16QQuick3DGeometry20targetAttributeCountEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK16QQuick3DGeometry6strideEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK16QQuick3DGeometry9attributeEi` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK16QQuick3DGeometry9boundsMaxEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK16QQuick3DGeometry9boundsMinEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK16QQuick3DGeometry9indexDataEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK18QQuick3DInstancing10metaObjectEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK18QQuick3DInstancing15hasTransparencyEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK18QQuick3DInstancing18InstanceTableEntry11getPositionEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK18QQuick3DInstancing18InstanceTableEntry11getRotationEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK18QQuick3DInstancing18InstanceTableEntry8getColorEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK18QQuick3DInstancing18InstanceTableEntry8getScaleEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK18QQuick3DInstancing19depthSortingEnabledEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK18QQuick3DInstancing21instanceCountOverrideEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK19QQuick3DTextureData10metaObjectEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK19QQuick3DTextureData11textureDataEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK19QQuick3DTextureData15hasTransparencyEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK19QQuick3DTextureData4sizeEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK19QQuick3DTextureData5depthEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK19QQuick3DTextureData6formatEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZNK23QQuick3DRenderExtension10metaObjectEv` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZTI14QQuick3DObject` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZTI16QQuick3DGeometry` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZTI18QQuick3DInstancing` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZTI19QQuick3DTextureData` | 1 | freecad |
| libQt6Quick3D.6.dylib | `__ZTI23QQuick3DRenderExtension` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN10QAudioRoom11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN10QAudioRoom11qt_metacastEPKc` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN10QAudioRoom11setPositionE9QVector3D` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN10QAudioRoom11setRotationERK11QQuaternion` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN10QAudioRoom13setDimensionsE9QVector3D` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN10QAudioRoom13setReverbGainEf` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN10QAudioRoom13setReverbTimeEf` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN10QAudioRoom15setWallMaterialENS_4WallENS_8MaterialE` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN10QAudioRoom16staticMetaObjectE` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN10QAudioRoom17setReflectionGainEf` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN10QAudioRoom19setReverbBrightnessEf` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN10QAudioRoomC2EP12QAudioEngine` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN10QAudioRoomD2Ev` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN12QAudioEngine11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN12QAudioEngine11qt_metacastEPKc` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN12QAudioEngine13setOutputModeENS_10OutputModeE` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN12QAudioEngine15setMasterVolumeEf` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN12QAudioEngine15setOutputDeviceERK12QAudioDevice` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN12QAudioEngine16setDistanceScaleEf` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN12QAudioEngine16staticMetaObjectE` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN12QAudioEngine21setRoomEffectsEnabledEb` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN12QAudioEngine4stopEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN12QAudioEngine5startEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN12QAudioEngine9setPausedEb` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN12QAudioEngineC2EiP7QObject` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN12QAudioEngineD2Ev` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QAmbientSound11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QAmbientSound11qt_metacastEPKc` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QAmbientSound11setAutoPlayEb` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QAmbientSound16staticMetaObjectE` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QAmbientSound4playEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QAmbientSound4stopEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QAmbientSound5pauseEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QAmbientSound8setLoopsEi` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QAmbientSound9setSourceERK4QUrl` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QAmbientSound9setVolumeEf` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QAmbientSoundC2EP12QAudioEngine` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QAmbientSoundD2Ev` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QSpatialSound11qt_metacallEN11QMetaObject4CallEiPPv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QSpatialSound11qt_metacastEPKc` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QSpatialSound11setAutoPlayEb` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QSpatialSound11setPositionE9QVector3D` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QSpatialSound11setRotationERK11QQuaternion` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QSpatialSound14setDirectivityEf` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QSpatialSound16setDistanceModelENS_13DistanceModelE` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QSpatialSound16setNearFieldGainEf` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QSpatialSound16staticMetaObjectE` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QSpatialSound17setDistanceCutoffEf` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QSpatialSound19setDirectivityOrderEf` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QSpatialSound20setManualAttenuationEf` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QSpatialSound21setOcclusionIntensityEf` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QSpatialSound4playEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QSpatialSound4stopEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QSpatialSound5pauseEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QSpatialSound7setSizeEf` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QSpatialSound8setLoopsEi` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QSpatialSound9setSourceERK4QUrl` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QSpatialSound9setVolumeEf` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QSpatialSoundC2EP12QAudioEngine` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN13QSpatialSoundD2Ev` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN14QAudioListener11setPositionE9QVector3D` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN14QAudioListener11setRotationERK11QQuaternion` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN14QAudioListenerC2EP12QAudioEngine` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZN14QAudioListenerD2Ev` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK10QAudioRoom10dimensionsEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK10QAudioRoom10metaObjectEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK10QAudioRoom10reverbGainEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK10QAudioRoom10reverbTimeEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK10QAudioRoom12wallMaterialENS_4WallE` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK10QAudioRoom14reflectionGainEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK10QAudioRoom16reverbBrightnessEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK10QAudioRoom8positionEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK10QAudioRoom8rotationEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK12QAudioEngine10metaObjectEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK12QAudioEngine10outputModeEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK12QAudioEngine10sampleRateEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK12QAudioEngine12masterVolumeEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK12QAudioEngine12outputDeviceEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK12QAudioEngine13distanceScaleEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK12QAudioEngine18roomEffectsEnabledEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK12QAudioEngine6pausedEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK13QAmbientSound10metaObjectEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK13QAmbientSound5loopsEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK13QAmbientSound6engineEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK13QAmbientSound6sourceEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK13QAmbientSound6volumeEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK13QAmbientSound8autoPlayEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK13QSpatialSound10metaObjectEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK13QSpatialSound11directivityEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK13QSpatialSound13distanceModelEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK13QSpatialSound13nearFieldGainEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK13QSpatialSound14distanceCutoffEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK13QSpatialSound16directivityOrderEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK13QSpatialSound17manualAttenuationEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK13QSpatialSound18occlusionIntensityEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK13QSpatialSound4sizeEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK13QSpatialSound5loopsEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK13QSpatialSound6engineEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK13QSpatialSound6sourceEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK13QSpatialSound6volumeEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK13QSpatialSound8autoPlayEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK13QSpatialSound8positionEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK13QSpatialSound8rotationEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK14QAudioListener6engineEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK14QAudioListener8positionEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZNK14QAudioListener8rotationEv` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZTI10QAudioRoom` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZTI12QAudioEngine` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZTI13QAmbientSound` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZTI13QSpatialSound` | 1 | freecad |
| libQt6SpatialAudio.6.dylib | `__ZTI14QAudioListener` | 1 | freecad |
| libSystem.B.dylib | `_CCDeriveKey` | 1 | orion |
| libSystem.B.dylib | `_CCECCryptorComputeSharedSecret` | 1 | orion |
| libSystem.B.dylib | `_CCECCryptorExportKey` | 1 | orion |
| libSystem.B.dylib | `_CCECCryptorGeneratePair` | 1 | orion |
| libSystem.B.dylib | `_CCECCryptorImportKey` | 1 | orion |
| libSystem.B.dylib | `_CCECCryptorRelease` | 1 | orion |
| libSystem.B.dylib | `_CCKDFParametersCreateHkdf` | 1 | orion |
| libSystem.B.dylib | `_CCKDFParametersDestroy` | 1 | orion |
| libSystem.B.dylib | `_CCRSACryptorCreateFromData` | 1 | orion |
| libSystem.B.dylib | `_CCRSACryptorDecrypt` | 1 | orion |
| libSystem.B.dylib | `_CCRSACryptorEncrypt` | 1 | orion |
| libSystem.B.dylib | `_CCRSACryptorExport` | 1 | orion |
| libSystem.B.dylib | `_CCRSACryptorGeneratePair` | 1 | orion |
| libSystem.B.dylib | `_CCRSACryptorGetPublicKeyFromPrivateKey` | 1 | orion |
| libSystem.B.dylib | `_CCRSACryptorImport` | 1 | orion |
| libSystem.B.dylib | `_CCRSACryptorRelease` | 1 | orion |
| libSystem.B.dylib | `_CCRSACryptorSign` | 1 | orion |
| libSystem.B.dylib | `_CCRSACryptorVerify` | 1 | orion |
| libSystem.B.dylib | `_CCRSAGetCRTComponents` | 1 | orion |
| libSystem.B.dylib | `_CCRSAGetCRTComponentsSizes` | 1 | orion |
| libSystem.B.dylib | `_CCRSAGetKeyComponents` | 1 | orion |
| libSystem.B.dylib | `_CCRSAGetKeyType` | 1 | orion |
| libSystem.B.dylib | `_NSCreateObjectFileImageFromFile` | 1 | keka |
| libSystem.B.dylib | `_NSIsSymbolNameDefined` | 1 | adobe-acrobat-reader |
| libSystem.B.dylib | `_NSVersionOfLinkTimeLibrary` | 1 | orion |
| libSystem.B.dylib | `_NSVersionOfRunTimeLibrary` | 1 | orion |
| libSystem.B.dylib | `_OSAtomicCompareAndSwap32` | 1 | protonvpn |
| libSystem.B.dylib | `_OSAtomicOr32Orig` | 1 | microsoft-outlook |
| libSystem.B.dylib | `_SANDBOX_EXTENSION_CANONICAL` | 1 | orion |
| libSystem.B.dylib | `_SANDBOX_EXTENSION_NO_REPORT` | 1 | orion |
| libSystem.B.dylib | `_SANDBOX_EXTENSION_USER_INTENT` | 1 | orion |
| libSystem.B.dylib | `_XPC_ACTIVITY_ALLOW_BATTERY` | 1 | orion |
| libSystem.B.dylib | `_XPC_ACTIVITY_CHECK_IN` | 1 | orion |
| libSystem.B.dylib | `_XPC_ACTIVITY_GRACE_PERIOD` | 1 | orion |
| libSystem.B.dylib | `_XPC_ACTIVITY_INTERVAL` | 1 | orion |
| libSystem.B.dylib | `_XPC_ACTIVITY_PRIORITY` | 1 | orion |
| libSystem.B.dylib | `_XPC_ACTIVITY_PRIORITY_MAINTENANCE` | 1 | orion |
| libSystem.B.dylib | `_XPC_ACTIVITY_RANDOM_INITIAL_DELAY` | 1 | orion |
| libSystem.B.dylib | `_XPC_ACTIVITY_REPEATING` | 1 | orion |
| libSystem.B.dylib | `_XPC_ACTIVITY_REQUIRE_NETWORK_CONNECTIVITY` | 1 | orion |
| libSystem.B.dylib | `__Block_has_signature` | 1 | orion |
| libSystem.B.dylib | `__Block_signature` | 1 | orion |
| libSystem.B.dylib | `__NSConcreteAutoBlock` | 1 | localsend |
| libSystem.B.dylib | `__NSConcreteFinalizingBlock` | 1 | localsend |
| libSystem.B.dylib | `__NSConcreteWeakBlockVariable` | 1 | localsend |
| libSystem.B.dylib | `__Unwind_Find_FDE` | 1 | crossover |
| libSystem.B.dylib | `___fixdfti` | 1 | orion |
| libSystem.B.dylib | `___floattixf` | 1 | qgis |
| libSystem.B.dylib | `___opendir2$INODE64` | 1 | feishu |
| libSystem.B.dylib | `___powisf2` | 1 | shotcut |
| libSystem.B.dylib | `___pthread_chdir` | 1 | android-studio |
| libSystem.B.dylib | `___pthread_kill` | 1 | crossover |
| libSystem.B.dylib | `__kernelrpc_mach_port_allocate_trap` | 1 | capcut |
| libSystem.B.dylib | `__kernelrpc_mach_port_deallocate_trap` | 1 | capcut |
| libSystem.B.dylib | `__kernelrpc_mach_port_insert_right_trap` | 1 | capcut |
| libSystem.B.dylib | `__kernelrpc_mach_port_mod_refs_trap` | 1 | capcut |
| libSystem.B.dylib | `__kernelrpc_mach_vm_allocate_trap` | 1 | capcut |
| libSystem.B.dylib | `__kernelrpc_mach_vm_deallocate_trap` | 1 | capcut |
| libSystem.B.dylib | `__keymgr_get_and_lock_processwide_ptr` | 1 | inkscape |
| libSystem.B.dylib | `__keymgr_set_and_unlock_processwide_ptr` | 1 | inkscape |
| libSystem.B.dylib | `__keymgr_unlock_processwide_ptr` | 1 | inkscape |
| libSystem.B.dylib | `__os_feature_enabled_impl` | 1 | orion |
| libSystem.B.dylib | `__qtn_proc_alloc` | 1 | orion |
| libSystem.B.dylib | `__qtn_proc_apply_to_self` | 1 | orion |
| libSystem.B.dylib | `__qtn_proc_free` | 1 | orion |
| libSystem.B.dylib | `__qtn_proc_init` | 1 | orion |
| libSystem.B.dylib | `__qtn_proc_init_with_self` | 1 | orion |
| libSystem.B.dylib | `__qtn_proc_set_flags` | 1 | orion |
| libSystem.B.dylib | `__simd_round_d2` | 1 | linearmouse |
| libSystem.B.dylib | `__simd_sin_f4` | 1 | macwhisper |
| libSystem.B.dylib | `_arc4random_stir` | 1 | bbedit |
| libSystem.B.dylib | `_asl_key` | 1 | tunnelblick |
| libSystem.B.dylib | `_asl_log_descriptor` | 1 | xquartz |
| libSystem.B.dylib | `_atof_l` | 1 | affinity |
| libSystem.B.dylib | `_atoi_l` | 1 | affinity |
| libSystem.B.dylib | `_cache_simulate_memory_warning_event` | 1 | orion |
| libSystem.B.dylib | `_chmodx_np` | 1 | utm |
| libSystem.B.dylib | `_dirstat_np` | 1 | telegram |
| libSystem.B.dylib | `_ecvt` | 1 | microsoft-word |
| libSystem.B.dylib | `_endhostent` | 1 | vlc |
| libSystem.B.dylib | `_ether_aton` | 1 | docker-desktop |
| libSystem.B.dylib | `_fchmodx_np` | 1 | utm |
| libSystem.B.dylib | `_ffsctl` | 1 | utm |
| libSystem.B.dylib | `_filesec_dup` | 1 | utm |
| libSystem.B.dylib | `_filesec_query_property` | 1 | openclaw |
| libSystem.B.dylib | `_filesec_set_property` | 1 | utm |
| libSystem.B.dylib | `_freehostent` | 1 | deluge |
| libSystem.B.dylib | `_ftw$INODE64` | 1 | gcc-arm-embedded |
| libSystem.B.dylib | `_get_end` | 1 | inkscape |
| libSystem.B.dylib | `_get_etext` | 1 | inkscape |
| libSystem.B.dylib | `_getaudit_addr` | 1 | android-studio |
| libSystem.B.dylib | `_getipnodebyaddr` | 1 | deluge |
| libSystem.B.dylib | `_getipnodebyname` | 1 | deluge |
| libSystem.B.dylib | `_getopt_long_only` | 1 | android-studio |
| libSystem.B.dylib | `_getsubopt` | 1 | freecad |
| libSystem.B.dylib | `_i386_set_ldt` | 1 | crossover |
| libSystem.B.dylib | `_imaxabs` | 1 | android-studio |
| libSystem.B.dylib | `_in6addr_linklocal_allnodes` | 1 | telegram |
| libSystem.B.dylib | `_in6addr_linklocal_allrouters` | 1 | telegram |
| libSystem.B.dylib | `_in6addr_nodelocal_allnodes` | 1 | telegram |
| libSystem.B.dylib | `_inet_nsap_addr` | 1 | emacs-app |
| libSystem.B.dylib | `_inet_nsap_ntoa` | 1 | emacs-app |
| libSystem.B.dylib | `_lstatx_np$INODE64` | 1 | utm |
| libSystem.B.dylib | `_malloc_zone_check` | 1 | epic-games |
| libSystem.B.dylib | `_minherit` | 1 | tor-browser |
| libSystem.B.dylib | `_mkostemps` | 1 | orion |
| libSystem.B.dylib | `_mount` | 1 | orbstack |
| libSystem.B.dylib | `_os_fault_with_payload` | 1 | orion |
| libSystem.B.dylib | `_os_state_add_handler` | 1 | orion |
| libSystem.B.dylib | `_os_transaction_create` | 1 | orion |
| libSystem.B.dylib | `_os_variant_allows_internal_security_policies` | 1 | orion |
| libSystem.B.dylib | `_os_variant_is_basesystem` | 1 | orion |
| libSystem.B.dylib | `_pthread_cond_signal_thread_np` | 1 | wechat |
| libSystem.B.dylib | `_sethostent` | 1 | vlc |
| libSystem.B.dylib | `_si_async_call` | 1 | emacs-app |
| libSystem.B.dylib | `_si_async_cancel` | 1 | emacs-app |
| libSystem.B.dylib | `_si_async_handle_reply` | 1 | emacs-app |
| libSystem.B.dylib | `_si_item_release` | 1 | emacs-app |
| libSystem.B.dylib | `_si_module_release` | 1 | emacs-app |
| libSystem.B.dylib | `_si_module_with_name` | 1 | emacs-app |
| libSystem.B.dylib | `_strcasecmp_l` | 1 | blender |
| libSystem.B.dylib | `_swap_dylib_command` | 1 | steam |
| libSystem.B.dylib | `_swap_linkedit_data_command` | 1 | microsoft-edge |
| libSystem.B.dylib | `_swap_mach_header_64` | 1 | microsoft-edge |
| libSystem.B.dylib | `_swap_section` | 1 | steam |
| libSystem.B.dylib | `_swap_segment_command_64` | 1 | microsoft-edge |
| libSystem.B.dylib | `_swap_symtab_command` | 1 | microsoft-edge |
| libSystem.B.dylib | `_sys_errlist` | 1 | vlc |
| libSystem.B.dylib | `_voucher_replace_default_voucher` | 1 | orion |
| libSystem.B.dylib | `_vswscanf` | 1 | kicad |
| libSystem.B.dylib | `_xdr_bytes` | 1 | freecad |
| libSystem.B.dylib | `_xdr_double` | 1 | freecad |
| libSystem.B.dylib | `_xdr_float` | 1 | freecad |
| libSystem.B.dylib | `_xdr_int` | 1 | freecad |
| libSystem.B.dylib | `_xdr_long` | 1 | freecad |
| libSystem.B.dylib | `_xdr_opaque` | 1 | freecad |
| libSystem.B.dylib | `_xdr_u_long` | 1 | freecad |
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
| libbz2.1.0.dylib | `_BZ2_bzerror` | 1 | inkscape |
| libcarla_native-plugin.dylib | `_carla_get_library_filename` | 1 | lmms |
| libcarla_native-plugin.dylib | `_carla_get_native_patchbay_plugin` | 1 | lmms |
| libcarla_native-plugin.dylib | `_carla_get_native_rack_plugin` | 1 | lmms |
| libcups.2.dylib | `_cupsPrintFile` | 1 | teamviewer |
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
| libffi.dylib | `_ffi_find_closure_for_code_np` | 1 | crossover |
| libffi.dylib | `_ffi_prep_closure` | 1 | mysqlworkbench |
| libgdbm_compat.4.dylib | `_dbm_clearerr` | 1 | qgis |
| libgdbm_compat.4.dylib | `_dbm_close` | 1 | qgis |
| libgdbm_compat.4.dylib | `_dbm_delete` | 1 | qgis |
| libgdbm_compat.4.dylib | `_dbm_error` | 1 | qgis |
| libgdbm_compat.4.dylib | `_dbm_fetch` | 1 | qgis |
| libgdbm_compat.4.dylib | `_dbm_firstkey` | 1 | qgis |
| libgdbm_compat.4.dylib | `_dbm_nextkey` | 1 | qgis |
| libgdbm_compat.4.dylib | `_dbm_open` | 1 | qgis |
| libgdbm_compat.4.dylib | `_dbm_store` | 1 | qgis |
| libicucore.A.dylib | `_UCNV_FROM_U_CALLBACK_ESCAPE` | 1 | orion |
| libicucore.A.dylib | `_UCNV_TO_U_CALLBACK_SUBSTITUTE` | 1 | orion |
| libicucore.A.dylib | `_u_charMirror` | 1 | orion |
| libicucore.A.dylib | `_u_foldCase` | 1 | orion |
| libicucore.A.dylib | `_u_getCombiningClass` | 1 | bbedit |
| libicucore.A.dylib | `_u_getPropertyValueEnum` | 1 | orion |
| libicucore.A.dylib | `_u_isalnum` | 1 | orion |
| libicucore.A.dylib | `_u_isalpha` | 1 | orion |
| libicucore.A.dylib | `_u_iscntrl` | 1 | bbedit |
| libicucore.A.dylib | `_u_isdigit` | 1 | orion |
| libicucore.A.dylib | `_u_isprint` | 1 | orion |
| libicucore.A.dylib | `_u_strFoldCase` | 1 | orion |
| libicucore.A.dylib | `_u_stringHasBinaryProperty` | 1 | orion |
| libicucore.A.dylib | `_u_totitle` | 1 | orion |
| libicucore.A.dylib | `_ubidi_close` | 1 | orion |
| libicucore.A.dylib | `_ubidi_getBaseDirection` | 1 | orion |
| libicucore.A.dylib | `_ubidi_getLogicalRun` | 1 | orion |
| libicucore.A.dylib | `_ubidi_open` | 1 | orion |
| libicucore.A.dylib | `_ubidi_reorderVisual` | 1 | orion |
| libicucore.A.dylib | `_ubidi_setPara` | 1 | orion |
| libicucore.A.dylib | `_ublock_getCode` | 1 | orion |
| libicucore.A.dylib | `_ubrk_isBoundary` | 1 | orion |
| libicucore.A.dylib | `_ucnv_cbFromUWriteBytes` | 1 | orion |
| libicucore.A.dylib | `_ucnv_close` | 1 | orion |
| libicucore.A.dylib | `_ucnv_fromUnicode` | 1 | orion |
| libicucore.A.dylib | `_ucnv_getCanonicalName` | 1 | orion |
| libicucore.A.dylib | `_ucnv_getName` | 1 | orion |
| libicucore.A.dylib | `_ucnv_open` | 1 | orion |
| libicucore.A.dylib | `_ucnv_reset` | 1 | orion |
| libicucore.A.dylib | `_ucnv_setFallback` | 1 | orion |
| libicucore.A.dylib | `_ucnv_setFromUCallBack` | 1 | orion |
| libicucore.A.dylib | `_ucnv_setToUCallBack` | 1 | orion |
| libicucore.A.dylib | `_ucnv_toUnicode` | 1 | orion |
| libicucore.A.dylib | `_ucol_getStrength` | 1 | orion |
| libicucore.A.dylib | `_ucol_setStrength` | 1 | orion |
| libicucore.A.dylib | `_ucol_strcollIter` | 1 | orion |
| libicucore.A.dylib | `_ucsdet_close` | 1 | orion |
| libicucore.A.dylib | `_ucsdet_detectAll` | 1 | orion |
| libicucore.A.dylib | `_ucsdet_enableInputFilter` | 1 | orion |
| libicucore.A.dylib | `_ucsdet_getConfidence` | 1 | orion |
| libicucore.A.dylib | `_ucsdet_getName` | 1 | orion |
| libicucore.A.dylib | `_ucsdet_open` | 1 | orion |
| libicucore.A.dylib | `_ucsdet_setText` | 1 | orion |
| libicucore.A.dylib | `_uidna_IDNToASCII` | 1 | adobe-acrobat-reader |
| libicucore.A.dylib | `_uiter_setString` | 1 | orion |
| libicucore.A.dylib | `_uiter_setUTF8` | 1 | orion |
| libicucore.A.dylib | `_uloc_getISOCountries` | 1 | orion |
| libicucore.A.dylib | `_uscript_getScript` | 1 | orion |
| libicucore.A.dylib | `_usearch_getCollator` | 1 | orion |
| libicucore.A.dylib | `_usearch_getMatchedLength` | 1 | orion |
| libicucore.A.dylib | `_usearch_next` | 1 | orion |
| libicucore.A.dylib | `_usearch_open` | 1 | orion |
| libicucore.A.dylib | `_usearch_reset` | 1 | orion |
| libicucore.A.dylib | `_usearch_setAttribute` | 1 | orion |
| libicucore.A.dylib | `_usearch_setOffset` | 1 | orion |
| libicucore.A.dylib | `_usearch_setPattern` | 1 | orion |
| libicucore.A.dylib | `_usearch_setText` | 1 | orion |
| libicucore.A.dylib | `_utext_close` | 1 | orion |
| libicucore.A.dylib | `_utext_setup` | 1 | orion |
| libiodbc.2.dylib | `_SQLAllocHandle` | 1 | anki |
| libiodbc.2.dylib | `_SQLBindParameter` | 1 | anki |
| libiodbc.2.dylib | `_SQLCloseCursor` | 1 | anki |
| libiodbc.2.dylib | `_SQLColAttributeW` | 1 | anki |
| libiodbc.2.dylib | `_SQLColumnsW` | 1 | anki |
| libiodbc.2.dylib | `_SQLDescribeColW` | 1 | anki |
| libiodbc.2.dylib | `_SQLDisconnect` | 1 | anki |
| libiodbc.2.dylib | `_SQLDriverConnectW` | 1 | anki |
| libiodbc.2.dylib | `_SQLEndTran` | 1 | anki |
| libiodbc.2.dylib | `_SQLExecDirectW` | 1 | anki |
| libiodbc.2.dylib | `_SQLExecute` | 1 | anki |
| libiodbc.2.dylib | `_SQLFetch` | 1 | anki |
| libiodbc.2.dylib | `_SQLFetchScroll` | 1 | anki |
| libiodbc.2.dylib | `_SQLFreeHandle` | 1 | anki |
| libiodbc.2.dylib | `_SQLGetData` | 1 | anki |
| libiodbc.2.dylib | `_SQLGetDiagRecW` | 1 | anki |
| libiodbc.2.dylib | `_SQLGetFunctions` | 1 | anki |
| libiodbc.2.dylib | `_SQLGetInfoW` | 1 | anki |
| libiodbc.2.dylib | `_SQLGetStmtAttrW` | 1 | anki |
| libiodbc.2.dylib | `_SQLGetTypeInfoW` | 1 | anki |
| libiodbc.2.dylib | `_SQLMoreResults` | 1 | anki |
| libiodbc.2.dylib | `_SQLNumResultCols` | 1 | anki |
| libiodbc.2.dylib | `_SQLPrepareW` | 1 | anki |
| libiodbc.2.dylib | `_SQLPrimaryKeysW` | 1 | anki |
| libiodbc.2.dylib | `_SQLRowCount` | 1 | anki |
| libiodbc.2.dylib | `_SQLSetConnectAttrW` | 1 | anki |
| libiodbc.2.dylib | `_SQLSetEnvAttr` | 1 | anki |
| libiodbc.2.dylib | `_SQLSetStmtAttrW` | 1 | anki |
| libiodbc.2.dylib | `_SQLSpecialColumnsW` | 1 | anki |
| libiodbc.2.dylib | `_SQLTablesW` | 1 | anki |
| libncurses.5.4.dylib | `_COLOR_PAIR` | 1 | gcc-arm-embedded |
| libncurses.5.4.dylib | `_boolnames` | 1 | iterm2 |
| libncurses.5.4.dylib | `_cur_term` | 1 | iterm2 |
| libncurses.5.4.dylib | `_curscr` | 1 | gcc-arm-embedded |
| libncurses.5.4.dylib | `_delscreen` | 1 | gcc-arm-embedded |
| libncurses.5.4.dylib | `_erase` | 1 | gcc-arm-embedded |
| libncurses.5.4.dylib | `_getcchar` | 1 | kitty |
| libncurses.5.4.dylib | `_mvwaddnstr` | 1 | gcc-arm-embedded |
| libncurses.5.4.dylib | `_mvwaddstr` | 1 | gcc-arm-embedded |
| libncurses.5.4.dylib | `_newterm` | 1 | gcc-arm-embedded |
| libncurses.5.4.dylib | `_numnames` | 1 | iterm2 |
| libncurses.5.4.dylib | `_strnames` | 1 | iterm2 |
| libncurses.5.4.dylib | `_waddstr` | 1 | gcc-arm-embedded |
| libncurses.5.4.dylib | `_wattroff` | 1 | gcc-arm-embedded |
| libncurses.5.4.dylib | `_wattron` | 1 | gcc-arm-embedded |
| libncurses.5.4.dylib | `_win_wch` | 1 | kitty |
| libncurses.5.4.dylib | `_wins_wch` | 1 | kitty |
| libncurses.5.4.dylib | `_wstandend` | 1 | gcc-arm-embedded |
| libncurses.5.4.dylib | `_wstandout` | 1 | gcc-arm-embedded |
| libodbc.2.dylib | `_SQLAllocHandle` | 1 | beekeeper-studio |
| libodbc.2.dylib | `_SQLBindCol` | 1 | beekeeper-studio |
| libodbc.2.dylib | `_SQLBindParameter` | 1 | beekeeper-studio |
| libodbc.2.dylib | `_SQLCancel` | 1 | beekeeper-studio |
| libodbc.2.dylib | `_SQLCancelHandle` | 1 | beekeeper-studio |
| libodbc.2.dylib | `_SQLCloseCursor` | 1 | beekeeper-studio |
| libodbc.2.dylib | `_SQLColAttributeW` | 1 | beekeeper-studio |
| libodbc.2.dylib | `_SQLDescribeColW` | 1 | beekeeper-studio |
| libodbc.2.dylib | `_SQLDisconnect` | 1 | beekeeper-studio |
| libodbc.2.dylib | `_SQLDriverConnectW` | 1 | beekeeper-studio |
| libodbc.2.dylib | `_SQLDriversW` | 1 | beekeeper-studio |
| libodbc.2.dylib | `_SQLEndTran` | 1 | beekeeper-studio |
| libodbc.2.dylib | `_SQLExecDirectW` | 1 | beekeeper-studio |
| libodbc.2.dylib | `_SQLExecute` | 1 | beekeeper-studio |
| libodbc.2.dylib | `_SQLFetch` | 1 | beekeeper-studio |
| libodbc.2.dylib | `_SQLFetchScroll` | 1 | beekeeper-studio |
| libodbc.2.dylib | `_SQLFreeHandle` | 1 | beekeeper-studio |
| libodbc.2.dylib | `_SQLFreeStmt` | 1 | beekeeper-studio |
| libodbc.2.dylib | `_SQLGetData` | 1 | beekeeper-studio |
| libodbc.2.dylib | `_SQLGetDescFieldW` | 1 | beekeeper-studio |
| libodbc.2.dylib | `_SQLGetDiagFieldW` | 1 | beekeeper-studio |
| libodbc.2.dylib | `_SQLGetDiagRecW` | 1 | beekeeper-studio |
| libodbc.2.dylib | `_SQLGetStmtAttrW` | 1 | beekeeper-studio |
| libodbc.2.dylib | `_SQLMoreResults` | 1 | beekeeper-studio |
| libodbc.2.dylib | `_SQLNumResultCols` | 1 | beekeeper-studio |
| libodbc.2.dylib | `_SQLPrepareW` | 1 | beekeeper-studio |
| libodbc.2.dylib | `_SQLRowCount` | 1 | beekeeper-studio |
| libodbc.2.dylib | `_SQLSetConnectAttrW` | 1 | beekeeper-studio |
| libodbc.2.dylib | `_SQLSetDescFieldW` | 1 | beekeeper-studio |
| libodbc.2.dylib | `_SQLSetEnvAttr` | 1 | beekeeper-studio |
| libodbc.2.dylib | `_SQLSetStmtAttrW` | 1 | beekeeper-studio |
| libpam.2.dylib | `_pam_authenticate` | 1 | freecad |
| libpam.2.dylib | `_pam_get_item` | 1 | freecad |
| libpcap.A.dylib | `_bpf_image` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_bufsize` | 1 | crossover |
| libpcap.A.dylib | `_pcap_dispatch` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_get_tstamp_precision` | 1 | crossover |
| libpcap.A.dylib | `_pcap_getnonblock` | 1 | crossover |
| libpcap.A.dylib | `_pcap_major_version` | 1 | crossover |
| libpcap.A.dylib | `_pcap_minor_version` | 1 | crossover |
| libpcap.A.dylib | `_pcap_open_dead` | 1 | wireshark-app |
| libpcap.A.dylib | `_pcap_sendpacket` | 1 | crossover |
| libpcap.A.dylib | `_pcap_set_tstamp_precision` | 1 | crossover |
| libpcap.A.dylib | `_pcap_set_want_pktap` | 1 | istat-menus |
| libpcap.A.dylib | `_pcap_setnonblock` | 1 | crossover |
| libpcap.A.dylib | `_pcap_statustostr` | 1 | wireshark-app |
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
| libpq.5.dylib | `_PQbackendPID` | 1 | anki |
| libpq.5.dylib | `_PQclear` | 1 | anki |
| libpq.5.dylib | `_PQcmdStatus` | 1 | anki |
| libpq.5.dylib | `_PQcmdTuples` | 1 | anki |
| libpq.5.dylib | `_PQconnectdb` | 1 | anki |
| libpq.5.dylib | `_PQconsumeInput` | 1 | anki |
| libpq.5.dylib | `_PQerrorMessage` | 1 | anki |
| libpq.5.dylib | `_PQescapeByteaConn` | 1 | anki |
| libpq.5.dylib | `_PQexec` | 1 | anki |
| libpq.5.dylib | `_PQexecPrepared` | 1 | anki |
| libpq.5.dylib | `_PQfinish` | 1 | anki |
| libpq.5.dylib | `_PQfmod` | 1 | anki |
| libpq.5.dylib | `_PQfname` | 1 | anki |
| libpq.5.dylib | `_PQfreemem` | 1 | anki |
| libpq.5.dylib | `_PQfsize` | 1 | anki |
| libpq.5.dylib | `_PQftable` | 1 | anki |
| libpq.5.dylib | `_PQftype` | 1 | anki |
| libpq.5.dylib | `_PQgetResult` | 1 | anki |
| libpq.5.dylib | `_PQgetisnull` | 1 | anki |
| libpq.5.dylib | `_PQgetvalue` | 1 | anki |
| libpq.5.dylib | `_PQnfields` | 1 | anki |
| libpq.5.dylib | `_PQnotifies` | 1 | anki |
| libpq.5.dylib | `_PQntuples` | 1 | anki |
| libpq.5.dylib | `_PQoidValue` | 1 | anki |
| libpq.5.dylib | `_PQprepare` | 1 | anki |
| libpq.5.dylib | `_PQresultErrorField` | 1 | anki |
| libpq.5.dylib | `_PQresultStatus` | 1 | anki |
| libpq.5.dylib | `_PQsendQuery` | 1 | anki |
| libpq.5.dylib | `_PQsetSingleRowMode` | 1 | anki |
| libpq.5.dylib | `_PQsocket` | 1 | anki |
| libpq.5.dylib | `_PQstatus` | 1 | anki |
| libpq.5.dylib | `_PQunescapeBytea` | 1 | anki |
| libsandbox.1.dylib | `_SANDBOX_BUILD_ID` | 1 | orion |
| libsandbox.1.dylib | `_sandbox_apply_container` | 1 | iterm2 |
| libsandbox.1.dylib | `_sandbox_compile_file` | 1 | orion |
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
| libstdc++.6.dylib | `__ZNKSs13find_first_ofEPKcmm` | 1 | android-file-transfer |
| libstdc++.6.dylib | `__ZNKSs17find_first_not_ofEPKcmm` | 1 | android-file-transfer |
| libstdc++.6.dylib | `__ZNKSs6substrEmm` | 1 | android-file-transfer |
| libstdc++.6.dylib | `__ZNKSs7compareEPKc` | 1 | android-file-transfer |
| libstdc++.6.dylib | `__ZNKSs7compareERKSs` | 1 | android-file-transfer |
| libstdc++.6.dylib | `__ZNSs12_M_leak_hardEv` | 1 | android-file-transfer |
| libstdc++.6.dylib | `__ZNSs6appendEPKcm` | 1 | android-file-transfer |
| libstdc++.6.dylib | `__ZNSs6appendERKSs` | 1 | android-file-transfer |
| libstdc++.6.dylib | `__ZNSs6appendEmc` | 1 | android-file-transfer |
| libstdc++.6.dylib | `__ZNSs6assignERKSs` | 1 | android-file-transfer |
| libstdc++.6.dylib | `__ZNSsC1EPKcmRKSaIcE` | 1 | android-file-transfer |
| libstdc++.6.dylib | `__ZNSsD1Ev` | 1 | deluge |
| libstdc++.6.dylib | `__ZNSt8ios_base4InitC1Ev` | 1 | android-file-transfer |
| libstdc++.6.dylib | `__ZNSt8ios_base4InitD1Ev` | 1 | android-file-transfer |
| libstdc++.6.dylib | `__ZSt18_Rb_tree_decrementPSt18_Rb_tree_node_base` | 1 | android-file-transfer |
| libstdc++.6.dylib | `__ZSt18_Rb_tree_incrementPKSt18_Rb_tree_node_base` | 1 | android-file-transfer |
| libstdc++.6.dylib | `__ZSt18_Rb_tree_incrementPSt18_Rb_tree_node_base` | 1 | android-file-transfer |
| libstdc++.6.dylib | `__ZSt20__throw_length_errorPKc` | 1 | android-file-transfer |
| libstdc++.6.dylib | `__ZSt20__throw_out_of_rangePKc` | 1 | android-file-transfer |
| libstdc++.6.dylib | `__ZSt28_Rb_tree_rebalance_for_erasePSt18_Rb_tree_node_baseRS_` | 1 | android-file-transfer |
| libstdc++.6.dylib | `__ZSt29_Rb_tree_insert_and_rebalancebPSt18_Rb_tree_node_baseS0_RS_` | 1 | android-file-transfer |
| libxar.1.dylib | `_xar_err_get_errno` | 1 | zoom |
| libxar.1.dylib | `_xar_err_get_string` | 1 | zoom |
| libxar.1.dylib | `_xar_get_safe_path` | 1 | docker-desktop |
| libxar.1.dylib | `_xar_get_toc_checksum` | 1 | opera |
| libxar.1.dylib | `_xar_get_toc_checksum_type` | 1 | opera |
| libxar.1.dylib | `_xar_register_errhandler` | 1 | zoom |
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
| libzstd.1.dylib | `_ZSTD_compress` | 1 | gcc-arm-embedded |
| libzstd.1.dylib | `_ZSTD_compressBound` | 1 | gcc-arm-embedded |
| libzstd.1.dylib | `_ZSTD_decompress` | 1 | gcc-arm-embedded |
| libzstd.1.dylib | `_ZSTD_getErrorName` | 1 | gcc-arm-embedded |
| libzstd.1.dylib | `_ZSTD_getFrameContentSize` | 1 | gcc-arm-embedded |
| libzstd.1.dylib | `_ZSTD_isError` | 1 | gcc-arm-embedded |
| libzstd.1.dylib | `_ZSTD_maxCLevel` | 1 | gcc-arm-embedded |
