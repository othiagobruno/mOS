/* Teste de host de userland/libsystem/appearance_core.c.
 * gcc -O1 -Wall -Wextra -I userland/libsystem userland/tests-appkit/appearance_host.c userland/libsystem/appearance_core.c */
#include <stdio.h>
#include "appearance_core.h"

static int fails, total;
#define CHECK(c, m) do { total++; if (!(c)) { fails++; printf("FALHOU: %s\n", m); } } while (0)
static int near(double a, double b) { return a - b < 0.002 && b - a < 0.002; }

int main(void) {
    double v[4];
    CHECK(ac_count() > 50 && ac_count() <= 128, "tabela com as cores do sistema (cabe nas 128 únicas)");
    CHECK(ac_index("labelColor") >= 0 && ac_index("nadaColor") < 0, "busca pelo seletor");
    ac_resolve(ac_index("labelColor"), 0, 0x007AFF, v);
    CHECK(near(v[0], 0) && near(v[3], 0.85), "labelColor clara: preto 85%");
    ac_resolve(ac_index("labelColor"), 1, 0x007AFF, v);
    CHECK(near(v[0], 1) && near(v[3], 0.85), "labelColor escura: branco 85%");
    ac_resolve(ac_index("windowBackgroundColor"), 1, 0x007AFF, v);
    CHECK(v[0] < 0.25 && near(v[3], 1), "fundo da janela escuro");
    ac_resolve(ac_index("controlAccentColor"), 0, 0xFF2D55, v);
    CHECK(near(v[0], 1) && near(v[1], 45 / 255.0) && near(v[2], 85 / 255.0), "controlAccentColor = destaque do sistema");
    ac_resolve(ac_index("selectedTextBackgroundColor"), 0, 0x000000, v);
    CHECK(near(v[0], 0.65) && near(v[3], 1), "seleção de texto clara: destaque misturado com branco");
    ac_resolve(ac_index("selectedTextBackgroundColor"), 1, 0xFFFFFF, v);
    CHECK(near(v[0], 0.5), "seleção de texto escura: destaque misturado com preto");
    ac_resolve(ac_index("keyboardFocusIndicatorColor"), 0, 0x112233, v);
    CHECK(near(v[3], 0.5) && near(v[2], 0x33 / 255.0), "anel de foco: destaque com alfa 0,5");
    ac_resolve(-1, 0, 0, v);
    CHECK(near(v[0], 0.5), "fora da tabela: cinza");

    CHECK(ac_is_dark("NSAppearanceNameDarkAqua") && !ac_is_dark("NSAppearanceNameAqua"), "lado da aparência");
    const char *two[] = {"NSAppearanceNameAqua", "NSAppearanceNameDarkAqua"};
    CHECK(ac_best_match("NSAppearanceNameDarkAqua", two, 2) == 1, "escura escolhe DarkAqua");
    CHECK(ac_best_match("NSAppearanceNameAqua", two, 2) == 0, "clara escolhe Aqua");
    const char *only_dark[] = {"NSAppearanceNameDarkAqua"};
    CHECK(ac_best_match("NSAppearanceNameAqua", only_dark, 1) == -1, "nenhuma do mesmo lado: nil");
    const char *vib[] = {"NSAppearanceNameDarkAqua", "NSAppearanceNameVibrantDark"};
    CHECK(ac_best_match("NSAppearanceNameVibrantDark", vib, 2) == 1, "mesmo nome primeiro");
    const char *vib2[] = {"NSAppearanceNameDarkAqua", "NSAppearanceNameAccessibilityHighContrastVibrantDark", "NSAppearanceNameVibrantDark"};
    CHECK(ac_best_match("NSAppearanceNameAccessibilityHighContrastVibrantDark", vib2 + 0, 1) == 0, "sem igual: do mesmo lado");
    const char *vib3[] = {"NSAppearanceNameDarkAqua", "NSAppearanceNameVibrantDark"};
    CHECK(ac_best_match("NSAppearanceNameAccessibilityHighContrastVibrantDark", vib3, 2) == 1, "vibração conta mais que contraste");

    printf("appearance_host: %d/%d\n", total - fails, total);
    return fails != 0;
}
