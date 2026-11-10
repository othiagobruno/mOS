# CallKit, CoreTelephony, CoreLocation e BackgroundTasks

O WhatsApp 2.26.45.13 (zip oficial, SHA-256 `29c12f980448668107782f6f1b52fe28f60e8a6729032177f6002eedd917cb3f`) importa 12 símbolos de CallKit, 15 de CoreTelephony, 8 de CoreLocation e 4 de BackgroundTasks. Eles entram no libSystem (`wa_small_core.c` e `wa_small_fw.c`). O binário do app não entra no repositório.

O miolo é estado local. Nada disso abre uma chamada de telefonia, consulta operadora, lê GPS ou dispara a tarefa em segundo plano.

- CallKit: `CXProvider` e `CXCallController` aceitam a transação, guardam a ação e o delegate recebe o objeto com o estado pendente. `fulfill` e `fail` mudam só esse estado. Sem provedor a conclusão recebe `CXErrorDomainIncomingCall`.
- CoreTelephony: `subscriberCellularProvider`, `dataServiceIdentifier` e `currentRadioAccessTechnology` são nil. Os mapas por serviço são dicionários vazios. As constantes `CTRadioAccessTechnology*` são os nomes da API.
- CoreLocation: a autorização lê o TCC `kTCCServiceLocation`. Concessão vira When In Use. `requestLocation` e `startUpdatingLocation` chamam `locationManager:didFailWithError:` com `kCLErrorLocationUnknown` (0) se autorizado, ou `kCLErrorDenied` (1) se não. `location` fica nil. `kCLLocationCoordinate2DInvalid` é NaN, não o par 0,0.
- BackgroundTasks: `register` guarda o identificador. `submit` guarda o pedido só se ele foi registrado e devolve `BGTaskSchedulerErrorCodeUnavailable` (2). Sem registro, não guarda e devolve `NotPermitted` (4). O handler não roda.

`CLLocationManager` e `kCLLocationAccuracyBest` / `HundredMeters` já existiam. Os três raios que faltavam e as funções de coordenada foram acrescentados.

Teste de host: `gcc -DWA_HOST -I userland/libsystem tools/waframeworks_hosttest.c userland/libsystem/wa_small_core.c -o /tmp/waframeworks`.
