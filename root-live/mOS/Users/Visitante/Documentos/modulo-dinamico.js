// Carregado sob demanda com import()
import Pilha, { contador } from "./modulo-util.js";
export const mensagem = "carregado dinamicamente";
export default function () { return new Pilha().empilhar(1).empilhar(2).tamanho + contador; }