/*
 * mOS — o que o Compute (motor de grafo do OpenSwiftUI) define só em Objective-C++ (*.mm) ou sobre sockets
 * do macOS, e que no mOS fica como stub:
 *  - servidor de depuração (DebugServer.cpp/Connection.cpp: sockets BSD + dispatch_source): indisponível;
 *  - Graph::print_cycle e Graph::write_to_file (Graph.mm, descrição em JSON via Foundation): mensagem curta
 *    no stderr / nada;
 *  - IAGAppObserverStartObserving (IAGAppObserver.mm, notificações do UIKit/AppKit): nada.
 * O CFRuntime do mOS fica em cf_mos.cpp (libMOSCFRuntime.a).
 */
#include <stdio.h>
#include "Debug/DebugServer.h"
#include "Graph/Graph.h"
#include "Graph/IAGAppObserver.h"

namespace IAG {

DebugServer *DebugServer::_shared_server = nullptr;
DebugServer *DebugServer::start(IAGDebugServerOptions options) { (void)options; return nullptr; }
void DebugServer::stop() {}
void DebugServer::run(uint32_t timeout) { (void)timeout; }
CFURLRef DebugServer::copy_url() const { return nullptr; }

void Graph::print_cycle(data::ptr<Node> node) {
    (void)node;
    fprintf(stderr, "=== AttributeGraph: cycle detected through attribute ===\n");
}
void Graph::write_to_file(Graph *graph, const char *filename, bool exclude_values) {
    (void)graph; (void)filename; (void)exclude_values;
}

} // namespace IAG

void IAGAppObserverStartObserving() {}
