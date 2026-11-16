# Lacunas de Leonardo (runtime C/C++)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Leonardo (runtime C/C++) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**719** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| libSystem.B.dylib | `_semctl` | 9 | firefox, gimp, iina, krita |
| libSystem.B.dylib | `_semget` | 9 | firefox, gimp, iina, krita |
| libSystem.B.dylib | `_semop` | 9 | firefox, gimp, iina, krita |
| libSystem.B.dylib | `_ftok` | 8 | firefox, gimp, krita, libreoffice |
| libSystem.B.dylib | `_getprotobyname` | 8 | blender, firefox, gimp, inkscape |
| libSystem.B.dylib | `_sem_unlink` | 8 | blender, gimp, inkscape, krita |
| libSystem.B.dylib | `_shmat` | 7 | gimp, krita, libreoffice, musescore |
| libSystem.B.dylib | `_shmctl` | 7 | gimp, krita, libreoffice, musescore |
| libSystem.B.dylib | `_shmdt` | 7 | gimp, krita, libreoffice, musescore |
| libSystem.B.dylib | `_getloadavg` | 6 | blender, gimp, inkscape, krita |
| libSystem.B.dylib | `_if_freenameindex` | 6 | blender, gimp, inkscape, krita |
| libSystem.B.dylib | `_if_nameindex` | 6 | blender, gimp, inkscape, krita |
| libSystem.B.dylib | `_putenv` | 6 | firefox, gimp, krita, libreoffice |
| libSystem.B.dylib | `_shmget` | 6 | gimp, krita, libreoffice, musescore |
| libSystem.B.dylib | `__os_activity_create` | 5 | krita, musescore, obs, qbittorrent |
| libSystem.B.dylib | `_ctermid_r` | 5 | blender, gimp, inkscape, krita |
| libSystem.B.dylib | `_dbm_clearerr` | 5 | blender, gimp, inkscape, krita |
| libSystem.B.dylib | `_dbm_close` | 5 | blender, gimp, inkscape, krita |
| libSystem.B.dylib | `_dbm_delete` | 5 | blender, gimp, inkscape, krita |
| libSystem.B.dylib | `_dbm_error` | 5 | blender, gimp, inkscape, krita |
| libSystem.B.dylib | `_dbm_fetch` | 5 | blender, gimp, inkscape, krita |
| libSystem.B.dylib | `_dbm_firstkey` | 5 | blender, gimp, inkscape, krita |
| libSystem.B.dylib | `_dbm_nextkey` | 5 | blender, gimp, inkscape, krita |
| libSystem.B.dylib | `_dbm_open` | 5 | blender, gimp, inkscape, krita |
| libSystem.B.dylib | `_dbm_store` | 5 | blender, gimp, inkscape, krita |
| libSystem.B.dylib | `_getprotobynumber` | 5 | firefox, gimp, libreoffice, thunderbird |
| libSystem.B.dylib | `_hstrerror` | 5 | blender, gimp, inkscape, krita |
| libSystem.B.dylib | `_pthread_attr_getschedpolicy` | 5 | krita, musescore, obs, qbittorrent |
| libSystem.B.dylib | `_scalbnf` | 5 | firefox, inkscape, krita, shotcut |
| libSystem.B.dylib | `_sem_trywait` | 5 | blender, gimp, inkscape, krita |
| libSystem.B.dylib | `_sethostname` | 5 | blender, gimp, inkscape, krita |
| libSystem.B.dylib | `_setpgrp` | 5 | blender, gimp, inkscape, krita |
| libSystem.B.dylib | `_uuid_generate_time` | 5 | blender, gimp, inkscape, krita |
| libSystem.B.dylib | `_wcsrchr` | 5 | blender, gimp, inkscape, krita |
| libSystem.B.dylib | `_wcstok` | 5 | blender, gimp, inkscape, krita |
| libSystem.B.dylib | `_wcsxfrm` | 5 | blender, gimp, inkscape, krita |
| libSystem.B.dylib | `_OSAtomicAdd32Barrier` | 4 | firefox, obs, thunderbird, vlc |
| libSystem.B.dylib | `_fegetround` | 4 | firefox, inkscape, libreoffice, thunderbird |
| libSystem.B.dylib | `_fmaxl` | 4 | blender, inkscape, monitorcontrol, mos |
| libSystem.B.dylib | `_gethostbyname2` | 4 | firefox, gimp, libreoffice, thunderbird |
| libSystem.B.dylib | `_getsectdatafromheader_64` | 4 | alt-tab, firefox, inkscape, thunderbird |
| libSystem.B.dylib | `_hash_create` | 4 | alt-tab, macdown, transmission, vlc |
| libSystem.B.dylib | `_hash_search` | 4 | alt-tab, macdown, transmission, vlc |
| libSystem.B.dylib | `_nextafterl` | 4 | blender, inkscape, monitorcontrol, mos |
| libSystem.B.dylib | `_pthread_setcanceltype` | 4 | inkscape, krita, obs, shotcut |
| libSystem.B.dylib | `_wcsstr` | 4 | gimp, krita, libreoffice, shotcut |
| libcups.2.dylib | `_httpGetHostname` | 4 | firefox, gimp, inkscape, thunderbird |
| libcups.2.dylib | `_ppdClose` | 4 | gimp, inkscape, krita, musescore |
| libcups.2.dylib | `_ppdFindOption` | 4 | gimp, inkscape, krita, musescore |
| libncurses.5.4.dylib | `_setupterm` | 4 | blender, inkscape, iterm2, krita |
| libncurses.5.4.dylib | `_tigetflag` | 4 | blender, inkscape, iterm2, krita |
| libncurses.5.4.dylib | `_tigetnum` | 4 | blender, inkscape, iterm2, krita |
| libncurses.5.4.dylib | `_tigetstr` | 4 | blender, inkscape, iterm2, krita |
| libSystem.B.dylib | `_OSAtomicCompareAndSwapIntBarrier` | 3 | firefox, obs, thunderbird |
| libSystem.B.dylib | `___divdc3` | 3 | krita, libreoffice, shotcut |
| libSystem.B.dylib | `__res` | 3 | musescore, obs, shotcut |
| libSystem.B.dylib | `_csqrt` | 3 | blender, gimp, inkscape |
| libSystem.B.dylib | `_csqrtf` | 3 | blender, gimp, inkscape |
| libSystem.B.dylib | `_csqrtl` | 3 | blender, gimp, inkscape |
| libSystem.B.dylib | `_drand48` | 3 | gimp, iterm2, krita |
| libSystem.B.dylib | `_duplocale` | 3 | gimp, krita, thunderbird |
| libSystem.B.dylib | `_fmemopen` | 3 | gimp, libreoffice, shotcut |
| libSystem.B.dylib | `_fminl` | 3 | blender, monitorcontrol, mos |
| libSystem.B.dylib | `_logbl` | 3 | inkscape, monitorcontrol, mos |
| libSystem.B.dylib | `_pthread_rwlockattr_destroy` | 3 | firefox, obs, thunderbird |
| libSystem.B.dylib | `_pthread_rwlockattr_init` | 3 | firefox, obs, thunderbird |
| libSystem.B.dylib | `_swab` | 3 | krita, shotcut, vlc |
| libcups.2.dylib | `_ppdOpenFile` | 3 | inkscape, krita, musescore |
| libedit.3.dylib | `_add_history` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_clear_history` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_emacs_meta_keymap` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_history_get` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_history_get_history_state` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_history_truncate_file` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_read_history` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_remove_history` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_replace_history_entry` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_rl_attempted_completion_function` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_rl_attempted_completion_over` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_rl_basic_word_break_characters` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_rl_bind_key` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_rl_bind_key_in_map` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_rl_callback_handler_install` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_rl_callback_handler_remove` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_rl_callback_read_char` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_rl_cleanup_after_signal` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_rl_complete` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_rl_completer_word_break_characters` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_rl_completion_append_character` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_rl_completion_display_matches_hook` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_rl_completion_matches` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_rl_completion_type` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_rl_free_line_state` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_rl_initialize` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_rl_insert` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_rl_insert_text` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_rl_instream` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_rl_library_version` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_rl_line_buffer` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_rl_outstream` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_rl_parse_and_bind` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_rl_pre_input_hook` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_rl_prep_terminal` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_rl_read_init_file` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_rl_readline_name` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_rl_readline_version` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_rl_redisplay` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_rl_startup_hook` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_rl_variable_bind` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_using_history` | 3 | blender, krita, libreoffice |
| libedit.3.dylib | `_write_history` | 3 | blender, krita, libreoffice |
| libncurses.5.4.dylib | `_COLORS` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_COLOR_PAIRS` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_COLS` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_ESCDELAY` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_LINES` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_TABSIZE` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_acs_map` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_baudrate` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_beep` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_can_change_color` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_cbreak` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_clearok` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_color_content` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_copywin` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_curs_set` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_def_prog_mode` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_def_shell_mode` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_delay_output` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_delwin` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_derwin` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_doupdate` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_echo` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_endwin` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_erasechar` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_filter` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_flash` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_flushinp` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_getmouse` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_getwin` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_halfdelay` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_has_colors` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_has_ic` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_has_il` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_has_key` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_idcok` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_idlok` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_immedok` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_init_color` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_init_pair` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_initscr` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_intrflush` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_is_linetouched` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_is_term_resized` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_is_wintouched` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_isendwin` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_keyname` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_keypad` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_killchar` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_leaveok` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_longname` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_meta` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_mouseinterval` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_mousemask` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_mvderwin` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_mvwin` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_napms` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_newpad` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_newscr` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_newwin` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_nl` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_nocbreak` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_nodelay` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_noecho` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_nonl` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_noqiflush` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_noraw` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_notimeout` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_overlay` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_overwrite` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_pair_content` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_pechochar` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_pnoutrefresh` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_prefresh` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_putp` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_putwin` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_qiflush` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_raw` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_reset_prog_mode` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_reset_shell_mode` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_resetty` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_resize_term` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_resizeterm` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_savetty` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_scrollok` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_set_escdelay` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_set_tabsize` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_start_color` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_stdscr` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_subpad` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_subwin` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_syncok` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_termattrs` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_termname` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_tparm` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_typeahead` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_unctrl` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_ungetch` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_ungetmouse` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_use_default_colors` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_use_env` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_waddch` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_waddnstr` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_wattr_off` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_wattr_on` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_wbkgd` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_wbkgdset` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_wborder` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_wchgat` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_wclear` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_wclrtobot` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_wclrtoeol` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_wcursyncup` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_wdelch` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_wechochar` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_wenclose` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_werase` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_wgetch` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_wgetnstr` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_whline` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_winch` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_winnstr` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_winsch` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_winsdelln` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_winsnstr` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_wmove` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_wnoutrefresh` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_wredrawln` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_wrefresh` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_wresize` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_wscrl` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_wsetscrreg` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_wsyncdown` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_wsyncup` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_wtimeout` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_wtouchln` | 3 | blender, inkscape, krita |
| libncurses.5.4.dylib | `_wvline` | 3 | blender, inkscape, krita |
| libpanel.5.4.dylib | `_bottom_panel` | 3 | blender, inkscape, krita |
| libpanel.5.4.dylib | `_del_panel` | 3 | blender, inkscape, krita |
| libpanel.5.4.dylib | `_hide_panel` | 3 | blender, inkscape, krita |
| libpanel.5.4.dylib | `_move_panel` | 3 | blender, inkscape, krita |
| libpanel.5.4.dylib | `_new_panel` | 3 | blender, inkscape, krita |
| libpanel.5.4.dylib | `_panel_above` | 3 | blender, inkscape, krita |
| libpanel.5.4.dylib | `_panel_below` | 3 | blender, inkscape, krita |
| libpanel.5.4.dylib | `_panel_hidden` | 3 | blender, inkscape, krita |
| libpanel.5.4.dylib | `_panel_userptr` | 3 | blender, inkscape, krita |
| libpanel.5.4.dylib | `_replace_panel` | 3 | blender, inkscape, krita |
| libpanel.5.4.dylib | `_set_panel_userptr` | 3 | blender, inkscape, krita |
| libpanel.5.4.dylib | `_show_panel` | 3 | blender, inkscape, krita |
| libpanel.5.4.dylib | `_top_panel` | 3 | blender, inkscape, krita |
| libpanel.5.4.dylib | `_update_panels` | 3 | blender, inkscape, krita |
| libSystem.B.dylib | `___divsc3` | 2 | krita, shotcut |
| libSystem.B.dylib | `___divxc3` | 2 | krita, shotcut |
| libSystem.B.dylib | `___invert_d2` | 2 | monitorcontrol, mos |
| libSystem.B.dylib | `___invert_d3` | 2 | monitorcontrol, mos |
| libSystem.B.dylib | `___invert_d4` | 2 | monitorcontrol, mos |
| libSystem.B.dylib | `___invert_f2` | 2 | monitorcontrol, mos |
| libSystem.B.dylib | `___invert_f4` | 2 | monitorcontrol, mos |
| libSystem.B.dylib | `___mulxc3` | 2 | krita, shotcut |
| libSystem.B.dylib | `_cabs` | 2 | blender, inkscape |
| libSystem.B.dylib | `_cabsf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_cabsl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_cacos` | 2 | blender, inkscape |
| libSystem.B.dylib | `_cacosf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_cacosh` | 2 | blender, inkscape |
| libSystem.B.dylib | `_cacoshf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_cacoshl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_cacosl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_carg` | 2 | blender, inkscape |
| libSystem.B.dylib | `_cargf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_cargl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_casin` | 2 | blender, inkscape |
| libSystem.B.dylib | `_casinf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_casinh` | 2 | blender, inkscape |
| libSystem.B.dylib | `_casinhf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_casinhl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_casinl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_catan` | 2 | blender, inkscape |
| libSystem.B.dylib | `_catanf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_catanh` | 2 | blender, inkscape |
| libSystem.B.dylib | `_catanhf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_catanhl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_catanl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_ccos` | 2 | blender, inkscape |
| libSystem.B.dylib | `_ccosf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_ccosh` | 2 | blender, inkscape |
| libSystem.B.dylib | `_ccoshf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_ccoshl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_ccosl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_cexp` | 2 | blender, inkscape |
| libSystem.B.dylib | `_cexpf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_cexpl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_clnt_create` | 2 | blender, inkscape |
| libSystem.B.dylib | `_clnt_spcreateerror` | 2 | blender, inkscape |
| libSystem.B.dylib | `_clog` | 2 | blender, inkscape |
| libSystem.B.dylib | `_clogf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_clogl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_cpow` | 2 | blender, inkscape |
| libSystem.B.dylib | `_cpowf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_cpowl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_csin` | 2 | blender, inkscape |
| libSystem.B.dylib | `_csinf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_csinh` | 2 | blender, inkscape |
| libSystem.B.dylib | `_csinhf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_csinhl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_csinl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_ctan` | 2 | blender, inkscape |
| libSystem.B.dylib | `_ctanf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_ctanh` | 2 | blender, inkscape |
| libSystem.B.dylib | `_ctanhf` | 2 | blender, inkscape |
| libSystem.B.dylib | `_ctanhl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_ctanl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_exc_server` | 2 | inkscape, vlc |
| libSystem.B.dylib | `_fdiml` | 2 | monitorcontrol, mos |
| libSystem.B.dylib | `_fesetround` | 2 | inkscape, libreoffice |
| libSystem.B.dylib | `_funopen` | 2 | gimp, libreoffice |
| libSystem.B.dylib | `_logbf` | 2 | monitorcontrol, mos |
| libSystem.B.dylib | `_lrintf` | 2 | inkscape, vlc |
| libSystem.B.dylib | `_mbr_gid_to_uuid` | 2 | gimp, iina |
| libSystem.B.dylib | `_mbr_uid_to_uuid` | 2 | gimp, iina |
| libSystem.B.dylib | `_mbr_uuid_to_id` | 2 | gimp, iina |
| libSystem.B.dylib | `_modfl` | 2 | blender, inkscape |
| libSystem.B.dylib | `_nanl` | 2 | monitorcontrol, mos |
| libSystem.B.dylib | `_pthread_key_init_np` | 2 | monitorcontrol, mos |
| libSystem.B.dylib | `_remquof` | 2 | monitorcontrol, mos |
| libSystem.B.dylib | `_remquol` | 2 | monitorcontrol, mos |
| libSystem.B.dylib | `_wcscasecmp` | 2 | krita, shotcut |
| libSystem.B.dylib | `_wcscspn` | 2 | gimp, libreoffice |
| libSystem.B.dylib | `_wcslcat` | 2 | krita, shotcut |
| libSystem.B.dylib | `_wcslcpy` | 2 | krita, shotcut |
| libSystem.B.dylib | `_wcsncasecmp` | 2 | krita, shotcut |
| libSystem.B.dylib | `_xdr_enum` | 2 | blender, inkscape |
| libSystem.B.dylib | `_xdr_pointer` | 2 | blender, inkscape |
| libSystem.B.dylib | `_xdr_string` | 2 | blender, inkscape |
| libSystem.B.dylib | `_yp_all` | 2 | blender, inkscape |
| libSystem.B.dylib | `_yp_get_default_domain` | 2 | blender, inkscape |
| libSystem.B.dylib | `_yp_master` | 2 | blender, inkscape |
| libSystem.B.dylib | `_yp_match` | 2 | blender, inkscape |
| libSystem.B.dylib | `_yperr_string` | 2 | blender, inkscape |
| libcups.2.dylib | `_cupsArrayFirst` | 2 | gimp, inkscape |
| libcups.2.dylib | `_cupsConnectDest` | 2 | firefox, thunderbird |
| libcups.2.dylib | `_cupsDoAuthentication` | 2 | gimp, inkscape |
| libcups.2.dylib | `_cupsEncryption` | 2 | gimp, inkscape |
| libcups.2.dylib | `_cupsFreeOptions` | 2 | gimp, inkscape |
| libcups.2.dylib | `_cupsGetDestMediaByIndex` | 2 | firefox, thunderbird |
| libcups.2.dylib | `_cupsGetDestMediaCount` | 2 | firefox, thunderbird |
| libcups.2.dylib | `_cupsGetDestMediaDefault` | 2 | firefox, thunderbird |
| libcups.2.dylib | `_cupsGetDests` | 2 | firefox, thunderbird |
| libcups.2.dylib | `_cupsGetDests2` | 2 | firefox, thunderbird |
| libcups.2.dylib | `_cupsLangDefault` | 2 | gimp, inkscape |
| libcups.2.dylib | `_cupsLangFree` | 2 | gimp, inkscape |
| libcups.2.dylib | `_cupsLocalizeDestMedia` | 2 | firefox, thunderbird |
| libcups.2.dylib | `_cupsParseOptions` | 2 | gimp, inkscape |
| libcups.2.dylib | `_cupsSetEncryption` | 2 | gimp, inkscape |
| libcups.2.dylib | `_cupsSetPasswordCB` | 2 | gimp, inkscape |
| libcups.2.dylib | `_cupsSetUser` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpAddrFreeList` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpAddrGetList` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpAddrLength` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpAddrPort` | 2 | firefox, thunderbird |
| libcups.2.dylib | `_httpAssembleURIf` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpBlocking` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpClearFields` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpEncryption` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpError` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpFlush` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpGet` | 2 | gimp, inkscape |
| libcups.2.dylib | `_httpGetAddress` | 2 | firefox, thunderbird |
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
| libcups.2.dylib | `_ppdFindAttr` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ppdFindChoice` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ppdFindCustomOption` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ppdFindMarkedChoice` | 2 | krita, musescore |
| libcups.2.dylib | `_ppdFirstCustomParam` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ppdLocalize` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ppdMarkDefaults` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ppdMarkOption` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ppdNextCustomParam` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ppdOpenFd` | 2 | gimp, inkscape |
| libcups.2.dylib | `_ppdPageSize` | 2 | gimp, inkscape |
| libexpat.1.dylib | `_XML_ErrorString` | 2 | iina, inkscape |
| libexpat.1.dylib | `_XML_GetBuffer` | 2 | iina, inkscape |
| libexpat.1.dylib | `_XML_GetCurrentLineNumber` | 2 | iina, inkscape |
| libexpat.1.dylib | `_XML_GetErrorCode` | 2 | iina, inkscape |
| libexpat.1.dylib | `_XML_ParseBuffer` | 2 | iina, inkscape |
| libexpat.1.dylib | `_XML_ParserCreate` | 2 | iina, inkscape |
| libexpat.1.dylib | `_XML_ParserFree` | 2 | iina, inkscape |
| libexpat.1.dylib | `_XML_SetCharacterDataHandler` | 2 | iina, inkscape |
| libexpat.1.dylib | `_XML_SetDoctypeDeclHandler` | 2 | iina, inkscape |
| libexpat.1.dylib | `_XML_SetElementHandler` | 2 | iina, inkscape |
| libexpat.1.dylib | `_XML_SetUserData` | 2 | iina, inkscape |
| libexslt.0.dylib | `_exsltRegisterAll` | 2 | inkscape, libreoffice |
| libffi.dylib | `_ffi_call` | 2 | krita, libreoffice |
| libffi.dylib | `_ffi_closure_alloc` | 2 | krita, libreoffice |
| libffi.dylib | `_ffi_closure_free` | 2 | krita, libreoffice |
| libffi.dylib | `_ffi_prep_cif` | 2 | krita, libreoffice |
| libffi.dylib | `_ffi_prep_cif_var` | 2 | krita, libreoffice |
| libffi.dylib | `_ffi_prep_closure_loc` | 2 | krita, libreoffice |
| libffi.dylib | `_ffi_type_double` | 2 | krita, libreoffice |
| libffi.dylib | `_ffi_type_float` | 2 | krita, libreoffice |
| libffi.dylib | `_ffi_type_longdouble` | 2 | krita, libreoffice |
| libffi.dylib | `_ffi_type_pointer` | 2 | krita, libreoffice |
| libffi.dylib | `_ffi_type_sint16` | 2 | krita, libreoffice |
| libffi.dylib | `_ffi_type_sint32` | 2 | krita, libreoffice |
| libffi.dylib | `_ffi_type_sint64` | 2 | krita, libreoffice |
| libffi.dylib | `_ffi_type_sint8` | 2 | krita, libreoffice |
| libffi.dylib | `_ffi_type_uint16` | 2 | krita, libreoffice |
| libffi.dylib | `_ffi_type_uint32` | 2 | krita, libreoffice |
| libffi.dylib | `_ffi_type_uint64` | 2 | krita, libreoffice |
| libffi.dylib | `_ffi_type_uint8` | 2 | krita, libreoffice |
| libffi.dylib | `_ffi_type_void` | 2 | krita, libreoffice |
| libicucore.A.dylib | `_u_charAge` | 2 | monitorcontrol, mos |
| libicucore.A.dylib | `_u_charName` | 2 | monitorcontrol, mos |
| libicucore.A.dylib | `_u_getNumericValue` | 2 | monitorcontrol, mos |
| libicucore.A.dylib | `_u_strToLower` | 2 | monitorcontrol, mos |
| libicucore.A.dylib | `_u_strToTitle` | 2 | monitorcontrol, mos |
| libicucore.A.dylib | `_u_strToUpper` | 2 | monitorcontrol, mos |
| libicucore.A.dylib | `_ubrk_close` | 2 | monitorcontrol, mos |
| libicucore.A.dylib | `_ubrk_following` | 2 | monitorcontrol, mos |
| libicucore.A.dylib | `_ubrk_open` | 2 | monitorcontrol, mos |
| libicucore.A.dylib | `_ubrk_preceding` | 2 | monitorcontrol, mos |
| libicucore.A.dylib | `_ubrk_setText` | 2 | monitorcontrol, mos |
| libicucore.A.dylib | `_ubrk_setUText` | 2 | monitorcontrol, mos |
| libicucore.A.dylib | `_unorm2_getNFCInstance` | 2 | monitorcontrol, mos |
| libicucore.A.dylib | `_unorm2_hasBoundaryBefore` | 2 | monitorcontrol, mos |
| libicucore.A.dylib | `_unorm2_normalize` | 2 | monitorcontrol, mos |
| libicucore.A.dylib | `_unorm2_spanQuickCheckYes` | 2 | monitorcontrol, mos |
| libicucore.A.dylib | `_utext_openUChars` | 2 | monitorcontrol, mos |
| libicucore.A.dylib | `_utext_openUTF8` | 2 | monitorcontrol, mos |
| libncurses.5.4.dylib | `_setcchar` | 2 | blender, inkscape |
| libncurses.5.4.dylib | `_unget_wch` | 2 | blender, inkscape |
| libncurses.5.4.dylib | `_wadd_wch` | 2 | blender, inkscape |
| libncurses.5.4.dylib | `_waddnwstr` | 2 | blender, inkscape |
| libncurses.5.4.dylib | `_wget_wch` | 2 | blender, inkscape |
| libncurses.5.4.dylib | `_wins_nwstr` | 2 | blender, inkscape |
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
| Tcl.framework | `_TclBN_mp_clear` | 1 | inkscape |
| Tcl.framework | `_TclBN_mp_init` | 1 | inkscape |
| Tcl.framework | `_TclBN_mp_read_radix` | 1 | inkscape |
| Tcl.framework | `_TclBN_mp_to_unsigned_bin_n` | 1 | inkscape |
| Tcl.framework | `_TclBN_mp_unsigned_bin_size` | 1 | inkscape |
| Tcl.framework | `_TclFreeObj` | 1 | inkscape |
| Tcl.framework | `_Tcl_AddErrorInfo` | 1 | inkscape |
| Tcl.framework | `_Tcl_AttemptAlloc` | 1 | inkscape |
| Tcl.framework | `_Tcl_ConditionFinalize` | 1 | inkscape |
| Tcl.framework | `_Tcl_ConditionNotify` | 1 | inkscape |
| Tcl.framework | `_Tcl_ConditionWait` | 1 | inkscape |
| Tcl.framework | `_Tcl_CreateFileHandler` | 1 | inkscape |
| Tcl.framework | `_Tcl_CreateInterp` | 1 | inkscape |
| Tcl.framework | `_Tcl_CreateObjCommand` | 1 | inkscape |
| Tcl.framework | `_Tcl_CreateTimerHandler` | 1 | inkscape |
| Tcl.framework | `_Tcl_DeleteCommand` | 1 | inkscape |
| Tcl.framework | `_Tcl_DeleteFileHandler` | 1 | inkscape |
| Tcl.framework | `_Tcl_DeleteInterp` | 1 | inkscape |
| Tcl.framework | `_Tcl_DeleteTimerHandler` | 1 | inkscape |
| Tcl.framework | `_Tcl_DoOneEvent` | 1 | inkscape |
| Tcl.framework | `_Tcl_Eval` | 1 | inkscape |
| Tcl.framework | `_Tcl_EvalFile` | 1 | inkscape |
| Tcl.framework | `_Tcl_EvalObjv` | 1 | inkscape |
| Tcl.framework | `_Tcl_ExprBoolean` | 1 | inkscape |
| Tcl.framework | `_Tcl_ExprDouble` | 1 | inkscape |
| Tcl.framework | `_Tcl_ExprLong` | 1 | inkscape |
| Tcl.framework | `_Tcl_ExprString` | 1 | inkscape |
| Tcl.framework | `_Tcl_FindExecutable` | 1 | inkscape |
| Tcl.framework | `_Tcl_Free` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetBignumFromObj` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetBoolean` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetBooleanFromObj` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetByteArrayFromObj` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetCurrentThread` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetDouble` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetDoubleFromObj` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetLongFromObj` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetObjResult` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetObjType` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetString` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetStringFromObj` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetStringResult` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetThreadData` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetVar` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetVar2Ex` | 1 | inkscape |
| Tcl.framework | `_Tcl_GetWideIntFromObj` | 1 | inkscape |
| Tcl.framework | `_Tcl_Init` | 1 | inkscape |
| Tcl.framework | `_Tcl_ListObjGetElements` | 1 | inkscape |
| Tcl.framework | `_Tcl_ListObjIndex` | 1 | inkscape |
| Tcl.framework | `_Tcl_ListObjLength` | 1 | inkscape |
| Tcl.framework | `_Tcl_MutexLock` | 1 | inkscape |
| Tcl.framework | `_Tcl_MutexUnlock` | 1 | inkscape |
| Tcl.framework | `_Tcl_NewBignumObj` | 1 | inkscape |
| Tcl.framework | `_Tcl_NewBooleanObj` | 1 | inkscape |
| Tcl.framework | `_Tcl_NewByteArrayObj` | 1 | inkscape |
| Tcl.framework | `_Tcl_NewDoubleObj` | 1 | inkscape |
| Tcl.framework | `_Tcl_NewListObj` | 1 | inkscape |
| Tcl.framework | `_Tcl_NewLongObj` | 1 | inkscape |
| Tcl.framework | `_Tcl_NewStringObj` | 1 | inkscape |
| Tcl.framework | `_Tcl_NewWideIntObj` | 1 | inkscape |
| Tcl.framework | `_Tcl_RecordAndEval` | 1 | inkscape |
| Tcl.framework | `_Tcl_ResetResult` | 1 | inkscape |
| Tcl.framework | `_Tcl_SetObjResult` | 1 | inkscape |
| Tcl.framework | `_Tcl_SetVar` | 1 | inkscape |
| Tcl.framework | `_Tcl_SetVar2` | 1 | inkscape |
| Tcl.framework | `_Tcl_SetVar2Ex` | 1 | inkscape |
| Tcl.framework | `_Tcl_SplitList` | 1 | inkscape |
| Tcl.framework | `_Tcl_ThreadAlert` | 1 | inkscape |
| Tcl.framework | `_Tcl_ThreadQueueEvent` | 1 | inkscape |
| Tcl.framework | `_Tcl_UnsetVar2` | 1 | inkscape |
| Tk.framework | `_Tk_GetNumMainWindows` | 1 | inkscape |
| Tk.framework | `_Tk_Init` | 1 | inkscape |
| Tk.framework | `_Tk_MainWindow` | 1 | inkscape |
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
| libSystem.B.dylib | `_NSLibraryNameForModule` | 1 | inkscape |
| libSystem.B.dylib | `_NSLookupAndBindSymbol` | 1 | inkscape |
| libSystem.B.dylib | `_NSModuleForSymbol` | 1 | inkscape |
| libSystem.B.dylib | `_NXGetArchInfoFromName` | 1 | vlc |
| libSystem.B.dylib | `_OSAtomicDequeue` | 1 | obs |
| libSystem.B.dylib | `_OSAtomicEnqueue` | 1 | obs |
| libSystem.B.dylib | `_OSSpinLockLock` | 1 | alt-tab |
| libSystem.B.dylib | `_OSSpinLockUnlock` | 1 | alt-tab |
| libSystem.B.dylib | `___isnanf` | 1 | shotcut |
| libSystem.B.dylib | `___powidf2` | 1 | shotcut |
| libSystem.B.dylib | `___powisf2` | 1 | shotcut |
| libSystem.B.dylib | `__keymgr_get_and_lock_processwide_ptr` | 1 | inkscape |
| libSystem.B.dylib | `__keymgr_set_and_unlock_processwide_ptr` | 1 | inkscape |
| libSystem.B.dylib | `__keymgr_unlock_processwide_ptr` | 1 | inkscape |
| libSystem.B.dylib | `__os_activity_initiate` | 1 | alt-tab |
| libSystem.B.dylib | `__os_log_create` | 1 | monitorcontrol |
| libSystem.B.dylib | `__os_log_internal` | 1 | monitorcontrol |
| libSystem.B.dylib | `__os_trace_with_buffer` | 1 | alt-tab |
| libSystem.B.dylib | `_daemon$1050` | 1 | vlc |
| libSystem.B.dylib | `_endhostent` | 1 | vlc |
| libSystem.B.dylib | `_erand48` | 1 | vlc |
| libSystem.B.dylib | `_fabsl` | 1 | blender |
| libSystem.B.dylib | `_feholdexcept` | 1 | inkscape |
| libSystem.B.dylib | `_fesetenv` | 1 | inkscape |
| libSystem.B.dylib | `_feupdateenv` | 1 | inkscape |
| libSystem.B.dylib | `_ffs` | 1 | shotcut |
| libSystem.B.dylib | `_fmtcheck` | 1 | shotcut |
| libSystem.B.dylib | `_get_end` | 1 | inkscape |
| libSystem.B.dylib | `_get_etext` | 1 | inkscape |
| libSystem.B.dylib | `_getsectbynamefromheader_64` | 1 | inkscape |
| libSystem.B.dylib | `_getsectdatafromheader` | 1 | alt-tab |
| libSystem.B.dylib | `_iswalnum` | 1 | inkscape |
| libSystem.B.dylib | `_iswcntrl` | 1 | inkscape |
| libSystem.B.dylib | `_iswlower` | 1 | inkscape |
| libSystem.B.dylib | `_iswupper` | 1 | inkscape |
| libSystem.B.dylib | `_jrand48` | 1 | vlc |
| libSystem.B.dylib | `_llrint` | 1 | vlc |
| libSystem.B.dylib | `_llrintf` | 1 | vlc |
| libSystem.B.dylib | `_lrint` | 1 | vlc |
| libSystem.B.dylib | `_lroundl` | 1 | inkscape |
| libSystem.B.dylib | `_nexttowardf` | 1 | blender |
| libSystem.B.dylib | `_nrand48` | 1 | vlc |
| libSystem.B.dylib | `_open_memstream` | 1 | shotcut |
| libSystem.B.dylib | `_os_trace_debug_enabled` | 1 | alt-tab |
| libSystem.B.dylib | `_pthread_getconcurrency` | 1 | libreoffice |
| libSystem.B.dylib | `_pthread_setconcurrency` | 1 | libreoffice |
| libSystem.B.dylib | `_putwc` | 1 | inkscape |
| libSystem.B.dylib | `_regcomp_l` | 1 | iterm2 |
| libSystem.B.dylib | `_scalbnl` | 1 | inkscape |
| libSystem.B.dylib | `_sethostent` | 1 | vlc |
| libSystem.B.dylib | `_sqrtl` | 1 | inkscape |
| libSystem.B.dylib | `_strcasecmp_l` | 1 | blender |
| libSystem.B.dylib | `_strunvis` | 1 | gimp |
| libSystem.B.dylib | `_strvis` | 1 | gimp |
| libSystem.B.dylib | `_swapcontext` | 1 | blender |
| libSystem.B.dylib | `_sys_errlist` | 1 | vlc |
| libSystem.B.dylib | `_tempnam` | 1 | vlc |
| libSystem.B.dylib | `_timespec_get` | 1 | obs |
| libSystem.B.dylib | `_towlower` | 1 | inkscape |
| libSystem.B.dylib | `_towupper` | 1 | inkscape |
| libSystem.B.dylib | `_vfprintf_l` | 1 | inkscape |
| libSystem.B.dylib | `_vis` | 1 | gimp |
| libSystem.B.dylib | `_wcsncat` | 1 | inkscape |
| libaprutil-1.0.dylib | `_apr_base64_decode` | 1 | iterm2 |
| libaprutil-1.0.dylib | `_apr_base64_decode_len` | 1 | iterm2 |
| libaprutil-1.0.dylib | `_apr_base64_encode_binary` | 1 | iterm2 |
| libaprutil-1.0.dylib | `_apr_base64_encode_len` | 1 | iterm2 |
| libbz2.1.0.dylib | `_BZ2_bzclose` | 1 | inkscape |
| libbz2.1.0.dylib | `_BZ2_bzerror` | 1 | inkscape |
| libbz2.1.0.dylib | `_BZ2_bzflush` | 1 | inkscape |
| libbz2.1.0.dylib | `_BZ2_bzopen` | 1 | inkscape |
| libbz2.1.0.dylib | `_BZ2_bzread` | 1 | inkscape |
| libbz2.1.0.dylib | `_BZ2_bzwrite` | 1 | inkscape |
| libcupsimage.2.dylib | `_cupsRasterClose` | 1 | inkscape |
| libcupsimage.2.dylib | `_cupsRasterOpen` | 1 | inkscape |
| libcupsimage.2.dylib | `_cupsRasterWriteHeader2` | 1 | inkscape |
| libcupsimage.2.dylib | `_cupsRasterWritePixels` | 1 | inkscape |
| libexslt.0.dylib | `_exsltDateXpathCtxtRegister` | 1 | inkscape |
| libexslt.0.dylib | `_exsltMathXpathCtxtRegister` | 1 | inkscape |
| libexslt.0.dylib | `_exsltSetsXpathCtxtRegister` | 1 | inkscape |
| libexslt.0.dylib | `_exsltStrXpathCtxtRegister` | 1 | inkscape |
| libiconv.2.dylib | `_libiconv_set_relocation_prefix` | 1 | krita |
| libicucore.A.dylib | `_u_errorName` | 1 | iterm2 |
| libicucore.A.dylib | `_u_strlen` | 1 | iterm2 |
| libicucore.A.dylib | `_uregex_appendReplacement` | 1 | iterm2 |
| libicucore.A.dylib | `_uregex_appendTail` | 1 | iterm2 |
| libicucore.A.dylib | `_uregex_clone` | 1 | iterm2 |
| libicucore.A.dylib | `_uregex_close` | 1 | iterm2 |
| libicucore.A.dylib | `_uregex_end` | 1 | iterm2 |
| libicucore.A.dylib | `_uregex_find` | 1 | iterm2 |
| libicucore.A.dylib | `_uregex_findNext` | 1 | iterm2 |
| libicucore.A.dylib | `_uregex_groupCount` | 1 | iterm2 |
| libicucore.A.dylib | `_uregex_open` | 1 | iterm2 |
| libicucore.A.dylib | `_uregex_reset` | 1 | iterm2 |
| libicucore.A.dylib | `_uregex_setText` | 1 | iterm2 |
| libicucore.A.dylib | `_uregex_start` | 1 | iterm2 |
| libncurses.5.4.dylib | `_BC` | 1 | inkscape |
| libncurses.5.4.dylib | `_PC` | 1 | inkscape |
| libncurses.5.4.dylib | `_UP` | 1 | inkscape |
| libncurses.5.4.dylib | `_boolnames` | 1 | iterm2 |
| libncurses.5.4.dylib | `_cur_term` | 1 | iterm2 |
| libncurses.5.4.dylib | `_curses_version` | 1 | krita |
| libncurses.5.4.dylib | `_getattrs` | 1 | blender |
| libncurses.5.4.dylib | `_getbegx` | 1 | blender |
| libncurses.5.4.dylib | `_getbegy` | 1 | blender |
| libncurses.5.4.dylib | `_getbkgd` | 1 | blender |
| libncurses.5.4.dylib | `_getcurx` | 1 | blender |
| libncurses.5.4.dylib | `_getcury` | 1 | blender |
| libncurses.5.4.dylib | `_getmaxx` | 1 | blender |
| libncurses.5.4.dylib | `_getmaxy` | 1 | blender |
| libncurses.5.4.dylib | `_getparx` | 1 | blender |
| libncurses.5.4.dylib | `_getpary` | 1 | blender |
| libncurses.5.4.dylib | `_is_leaveok` | 1 | blender |
| libncurses.5.4.dylib | `_numnames` | 1 | iterm2 |
| libncurses.5.4.dylib | `_redrawwin` | 1 | blender |
| libncurses.5.4.dylib | `_strnames` | 1 | iterm2 |
| libncurses.5.4.dylib | `_tgetent` | 1 | inkscape |
| libncurses.5.4.dylib | `_tgetflag` | 1 | inkscape |
| libncurses.5.4.dylib | `_tgetnum` | 1 | inkscape |
| libncurses.5.4.dylib | `_tgetstr` | 1 | inkscape |
| libncurses.5.4.dylib | `_tgoto` | 1 | inkscape |
| libncurses.5.4.dylib | `_tputs` | 1 | inkscape |
| libncurses.5.4.dylib | `_wattrset` | 1 | blender |
| libsandbox.1.dylib | `_sandbox_apply_container` | 1 | iterm2 |
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
