// Módulo em subpasta, importado por caminho relativo
export function quadrado(x) { return x * x; }
export const fatorialGrande = n => (n <= 1n ? 1n : n * fatorialGrande(n - 1n));