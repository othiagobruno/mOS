// mOS — teste do PhotosUI do Catalyst (PhotosUI.framework + libswiftPhotosUI) sobre o Photos.framework do mOS:
// filtros, configuração (ida e volta pela classe ObjC), PHPickerViewController(configuration:), delegate Swift, toques
// na grade, seleção ordenada, limite, cancelar, resultados com identificador e NSItemProvider de arquivo.
// A raiz do teste (tools/test-photosui.sh) tem três PNG em /Users/Visitante/Imagens e o kTCCServicePhotos concedido.
// Imprime "photosui: N/N ok".
import UIKit
import PhotosUI
import Foundation

var ok = 0, total = 0
func check(_ c: Bool, _ what: String) {
    total += 1
    if c { ok += 1 }
    print(c ? "  ok   \(what)" : "  FALHA \(what)")
}

print("photosui: PHPicker do Catalyst")

check(PHPickerFilter.images == PHPickerFilter.images && PHPickerFilter.images != PHPickerFilter.videos, "PHPickerFilter: igualdade")
let combo = PHPickerFilter.any(of: [.images, .videos])
check(combo == PHPickerFilter.any(of: [.images, .videos]) && combo != PHPickerFilter.all(of: [.images, .videos]), "PHPickerFilter.any/all")
check(Set([PHPickerFilter.images, .images, .livePhotos]).count == 2, "PHPickerFilter: Hashable")

var cfg = PHPickerConfiguration()
check(cfg.selectionLimit == 1 && cfg.selection == .default && cfg.filter == nil, "PHPickerConfiguration(): padrões da Apple")
cfg.selectionLimit = 0
cfg.selection = .ordered
cfg.filter = .images
cfg.preferredAssetRepresentationMode = .current
let pc = PHPickerViewController(configuration: cfg)
let back = pc.configuration
check(back.selectionLimit == 0 && back.selection == .ordered && back.filter == .images &&
      back.preferredAssetRepresentationMode == .current, "configuração: ida e volta pelo PHPickerViewController")

final class Recebe: PHPickerViewControllerDelegate {
    var chamadas = 0
    var resultados: [PHPickerResult] = []
    func picker(_ picker: PHPickerViewController, didFinishPicking results: [PHPickerResult]) {
        chamadas += 1
        resultados = results
    }
}

// os ganchos de teste do PhotosUI.framework (_mosTap..., _mosItemCount) pelo IMP, com os tipos C certos
func tocar(_ p: PHPickerViewController, _ i: Int) {
    typealias F = @convention(c) (AnyObject, Selector, Int) -> Void
    let sel = Selector("_mosTapItemAtIndex:")
    unsafeBitCast(p.method(for: sel), to: F.self)(p, sel, i)
}
func enviar(_ o: AnyObject, _ nome: String) {
    typealias F = @convention(c) (AnyObject, Selector) -> Void
    let sel = Selector(nome)
    unsafeBitCast(o.method(for: sel), to: F.self)(o, sel)
}
func itens(_ p: PHPickerViewController) -> Int {
    typealias F = @convention(c) (AnyObject, Selector) -> UInt
    let sel = Selector("_mosItemCount")
    return Int(unsafeBitCast(p.method(for: sel), to: F.self)(p, sel))
}
typealias Carga = @convention(c) (AnyObject, Selector, NSString, AnyObject) -> AnyObject?
func carregar(_ o: AnyObject, _ nome: String, _ tipo: String, _ bloco: AnyObject) {
    let sel = Selector(nome)
    _ = unsafeBitCast(o.method(for: sel), to: Carga.self)(o, sel, tipo._bridgeToObjectiveC(), bloco)
}

// biblioteca com 3 imagens, seleção ordenada sem limite, com biblioteca (há identificador)
var c1 = PHPickerConfiguration(photoLibrary: .shared())
c1.selectionLimit = 0
c1.selection = .ordered
let p1 = PHPickerViewController(configuration: c1)
let d1 = Recebe()
p1.delegate = d1
check(p1.delegate === d1, "delegate Swift guardado")
check(itens(p1) == 3, "grade com as 3 imagens da biblioteca")
tocar(p1, 2); tocar(p1, 0)
enviar(p1, "_mosTapAdd")
check(d1.chamadas == 1 && d1.resultados.count == 2, "Adicionar entrega 2 resultados")
let ids = d1.resultados.map { $0.assetIdentifier ?? "" }
let p1b = PHPickerViewController(configuration: c1)
let d1b = Recebe()
p1b.delegate = d1b
tocar(p1b, 0); tocar(p1b, 2)
enviar(p1b, "_mosTapAdd")
let idsB = d1b.resultados.map { $0.assetIdentifier ?? "" }
check(ids.count == 2 && ids[0] != ids[1] && ids.allSatisfy { $0.hasPrefix("/Users/Visitante/Imagens/") } &&
      idsB == Array(ids.reversed()), "ordem do toque e assetIdentifier com biblioteca")
if let r = d1.resultados.first {
    let prov = r.itemProvider
    check(prov.hasItemConforming(toTypeIdentifier: "public.png") && prov.hasItemConforming(toTypeIdentifier: "public.image") &&
          !prov.hasItemConforming(toTypeIdentifier: "public.movie"), "NSItemProvider: tipos do arquivo")
    var bytes = -1
    let b1: @convention(block) (NSData?, NSError?) -> Void = { d, _ in bytes = d.map { Int($0.length) } ?? -2 }
    carregar(prov, "loadDataRepresentationForTypeIdentifier:completionHandler:", "public.png", b1 as AnyObject)
    check(bytes > 8, "NSItemProvider: dados do PNG (\(bytes) bytes)")
    var existia = false
    var caminho = ""
    let b2: @convention(block) (NSURL?, NSError?) -> Void = { u, _ in
        caminho = u?.path.map { String._unconditionallyBridgeFromObjectiveC($0) } ?? ""
        existia = FileManager.default.fileExists(atPath: caminho)
    }
    carregar(prov, "loadFileRepresentationForTypeIdentifier:completionHandler:", "public.image", b2 as AnyObject)
    check(existia && !FileManager.default.fileExists(atPath: caminho), "loadFileRepresentation: cópia temporária, apagada depois")
} else {
    check(false, "NSItemProvider: tipos do arquivo"); check(false, "NSItemProvider: dados do PNG"); check(false, "loadFileRepresentation")
}

// limite 1 e seleção padrão: o toque já conclui; sem biblioteca, sem identificador
let p2 = PHPickerViewController(configuration: PHPickerConfiguration())
let d2 = Recebe()
p2.delegate = d2
tocar(p2, 1)
check(d2.chamadas == 1 && d2.resultados.count == 1 && d2.resultados[0].assetIdentifier == nil, "limite 1: toque conclui, sem identificador")

// limite 2: o terceiro toque não entra; Cancelar entrega lista vazia
var c3 = PHPickerConfiguration()
c3.selectionLimit = 2
let p3 = PHPickerViewController(configuration: c3)
let d3 = Recebe()
p3.delegate = d3
tocar(p3, 0); tocar(p3, 1); tocar(p3, 2)
enviar(p3, "_mosTapCancel")
check(d3.chamadas == 1 && d3.resultados.isEmpty, "Cancelar entrega lista vazia")
let p3b = PHPickerViewController(configuration: c3)
let d3b = Recebe()
p3b.delegate = d3b
tocar(p3b, 0); tocar(p3b, 1); tocar(p3b, 2)
enviar(p3b, "_mosTapAdd")
check(d3b.resultados.count == 2, "limite 2 respeitado")

// filtro de vídeo numa biblioteca só de imagens: grade vazia
var c4 = PHPickerConfiguration()
c4.filter = .videos
check(itens(PHPickerViewController(configuration: c4)) == 0, "filtro .videos esconde as imagens")
var c5 = PHPickerConfiguration()
c5.filter = .not(.videos)
check(itens(PHPickerViewController(configuration: c5)) == 3, "filtro .not(.videos)")

// pré-seleção + seleção contínua: cada toque entrega
var c6 = PHPickerConfiguration(photoLibrary: .shared())
c6.selectionLimit = 0
c6.selection = .continuous
c6.preselectedAssetIdentifiers = [ids.count > 1 ? ids[1] : ""]
let p6 = PHPickerViewController(configuration: c6)
let d6 = Recebe()
p6.delegate = d6
tocar(p6, 1)
check(d6.chamadas == 1 && d6.resultados.count == 2, "pré-seleção e seleção contínua")

print("photosui: \(ok)/\(total) ok")
