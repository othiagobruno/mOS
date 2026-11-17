/* mOS — o que o Accessibility.swift chama no AppKit do libSystem (cgdisplay.c) */
#pragma once
void cx_ax_announce(const char *text, long priority);
void cx_ax_post_named(const char *note);
