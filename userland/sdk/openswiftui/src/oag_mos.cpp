/*
 * mOS — stubs do servidor de depuração do OpenAttributeGraph (DebugServer.mm é Objective-C++ e usa sockets
 * do macOS): indisponível no mOS, start devolve nulo. O CFRuntime do mOS fica em cf_mos.cpp.
 */
#include <OpenAttributeGraphCxx/DebugServer/DebugServer.hpp>

/* servidor de depuração: indisponível no mOS */
namespace OAG {
DebugServer *DebugServer::_shared_server = nullptr;
DebugServer *DebugServer::start(OAGDebugServerMode mode) { (void)mode; return nullptr; }
void DebugServer::stop() {}
void DebugServer::run(int timeout) const { (void)timeout; }
CFURLRef DebugServer::copy_url() const { return nullptr; }
} // namespace OAG
