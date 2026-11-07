# Skills do projeto mOS

Skills do Claude Code instaladas no projeto (carregadas automaticamente de `.claude/skills/`).
Todas vieram de repositórios públicos no GitHub, foram revisadas antes da cópia e mantêm o
`LICENSE` original dentro de cada pasta. Instaladas em 2026-11-07.

Para buscar mais: use a skill `find-skills` (ou `npx skills find <termo>`; precisa de Node.js,
instalado via `winget install OpenJS.NodeJS.LTS`).

## Busca de skills
| Skill | Origem | Licença |
|---|---|---|
| find-skills | vercel-labs/skills | MIT |

## Swift
| Skill | Origem | Licença |
|---|---|---|
| swift-expert | Jeffallan/claude-skills | MIT |
| swift-concurrency | AvdLee/Swift-Concurrency-Agent-Skill | MIT |
| swift-concurrency-pro | twostraws/Swift-Concurrency-Agent-Skill | MIT |
| swift-api-design-guidelines | Erikote04/Swift-API-Design-Guidelines-Agent-Skill | MIT |
| swift-memory-span (era `memory`) | rshankras/claude-code-apple-skills | MIT |

## SwiftUI / UIKit / AppKit / macOS Tahoe
| Skill | Origem | Licença |
|---|---|---|
| swiftui-expert-skill | AvdLee/SwiftUI-Agent-Skill | MIT |
| swiftui-pro | twostraws/SwiftUI-Agent-Skill | MIT |
| swiftui-ui-patterns, swiftui-liquid-glass, swiftui-performance-audit, swiftui-view-refactor | Dimillian/Skills | MIT |
| swiftui-gotchas | daneden/agent-skills | MIT |
| uikit-expert | ivan-magda/uikit-expert-skill | MIT |
| appkit-swiftui-bridge, macos-tahoe-apis, ui-review-tahoe, macos-coding-best-practices (era `coding-best-practices`) | rshankras/claude-code-apple-skills | MIT |

## Objective-C
| Skill | Origem | Licença |
|---|---|---|
| objc-arc-patterns, objc-blocks-gcd, objc-protocols-categories | thebushidocollective/han | FSL-1.1 (source-available) |

## C / C++ / baixo nível / kernel
| Skill | Origem | Licença |
|---|---|---|
| c-systems-programming, c-memory-management, c-data-structures | thebushidocollective/han | FSL-1.1 (source-available) |
| cpp-pro, embedded-systems, debugging-wizard | Jeffallan/claude-skills | MIT |
| memory-safety-patterns, binary-analysis-patterns | wshobson/agents | MIT |
| os-dev-scratch, assembly-x86, abi-and-calling-conventions, dynamic-linking, linker-scripts, virtual-memory-paging-and-tlb, qemu-for-kernel-development, sanitizers, clang, gdb, lldb, binutils, custom-allocators, memory-model, static-analysis | mohitmishra786/low-level-dev-skills | MIT |
| re-format-macho (texto em chinês) | dslsdzc/rev-skills | Apache-2.0 |

## Ajustes locais
- Removidos `agents/openai.yaml`, `.claude-plugin/` e cópias aninhadas (não usados pelo Claude Code).
- Campo `name:` alinhado ao nome da pasta em: macos-coding-best-practices, swift-memory-span,
  swift-api-design-guidelines, objc-arc-patterns, objc-blocks-gcd, objc-protocols-categories.
- Os scripts Python de `swiftui-expert-skill` usam `xctrace` (só macOS); não rodam no Windows.
