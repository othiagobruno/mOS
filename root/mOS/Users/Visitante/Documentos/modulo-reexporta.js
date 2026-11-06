// Reexportações: export *, export * as e export { default as ... } from
export * from "./modulo-util.js";
export * as util from "./modulo-util.js";
export { default as PilhaPadrao, VERSAO as versao } from "./modulo-util.js";
import { quadrado } from "./modulos/matematica.js";
export const area = lado => quadrado(lado);
export const origem = import.meta.url;