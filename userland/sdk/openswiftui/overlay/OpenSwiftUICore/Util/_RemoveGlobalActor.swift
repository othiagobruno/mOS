//
//  _RemoveGlobalActor.swift
//  OpenSwiftUICore
//
//  mOS: sem _Concurrency não há ator global para o _ImpossibleActor. O protocolo marcador continua (é API pública que
//  os tipos usam para "tirar" o isolamento herdado); sem atores, não há isolamento a tirar.

@_marker
public protocol _RemoveGlobalActorIsolation {}
