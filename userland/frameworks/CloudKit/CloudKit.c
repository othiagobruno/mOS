/*
 * mOS — CloudKit.framework vazio, só para o vínculo.
 *
 * O machocheck do OpenBot 0.66.0 (docs/compat-open-usage.md, 10 nov. 2026) lista
 * CloudKit.framework entre as dylibs absolutas que o binário pede e que
 * libsystem_dylib_ok não substitui. A tabela de imports desse mesmo relatório não
 * tem nenhum símbolo de CloudKit: o app liga o framework e não chama nada dele.
 * Por isso este arquivo não declara CKContainer, CKRecord nem outra API.
 * Sem esta dylib no disco, o carregador recusa o processo (link_abs_dep).
 *
 * Limitação: não há conta iCloud, banco nem notificação. Um app que passe a
 * importar símbolos daqui continua sem eles, de propósito.
 */
void mos_cloudkit_linked(void) {}
