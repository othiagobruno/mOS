// Módulo de exemplo do Brisa: exportações nomeadas, default e bindings vivos
globalThis.avaliacoesUtil = (globalThis.avaliacoesUtil || 0) + 1;

export const VERSAO = "1.0";
export let contador = 0;
export function incrementar() { contador++; return contador; }
export function soma(...xs) { return xs.reduce((a, b) => a + b, 0); }
export { soma as adicionar };

export default class Pilha {
  #itens = [];
  empilhar(x) { this.#itens.push(x); return this; }
  desempilhar() { return this.#itens.pop(); }
  get tamanho() { return this.#itens.length; }
}