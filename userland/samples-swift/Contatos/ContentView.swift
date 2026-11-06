// Contatos — app SwiftUI de exemplo: barra lateral com lista selecionável, abas, formulário,
// navegação em pilha, folha modal e alerta.
import SwiftUI

struct Contato {
    var id: Int
    var nome: String
    var cidade: String
    var idade: Int
    var tipo: Int      // 0 amigo, 1 família, 2 trabalho
}

final class Agenda: ObservableObject {
    @Published var contatos: [Contato] = [
        Contato(id: 1, nome: "Ana Souza", cidade: "Recife", idade: 31, tipo: 0),
        Contato(id: 2, nome: "Bruno Lima", cidade: "Curitiba", idade: 45, tipo: 2),
        Contato(id: 3, nome: "Carla Dias", cidade: "Salvador", idade: 27, tipo: 1),
    ]
    var proximo = 4
}

struct Detalhe: View {
    @ObservedObject var agenda: Agenda
    let indice: Int
    @State private var aba = 0
    @State private var aviso = false

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text(agenda.contatos[indice].nome).font(.title)
            TabView(selection: $aba) {
                Form {
                    Section("Dados") {
                        TextField("Nome", text: Binding(get: { agenda.contatos[indice].nome }, set: { agenda.contatos[indice].nome = $0 }))
                        TextField("Cidade", text: Binding(get: { agenda.contatos[indice].cidade }, set: { agenda.contatos[indice].cidade = $0 }))
                        Stepper("Idade: \(agenda.contatos[indice].idade)", value: Binding(get: { agenda.contatos[indice].idade }, set: { agenda.contatos[indice].idade = $0 }), in: 0...120)
                    }
                    Section("Grupo") {
                        Picker("Tipo", selection: Binding(get: { agenda.contatos[indice].tipo }, set: { agenda.contatos[indice].tipo = $0 })) {
                            Text("Amigo").tag(0)
                            Text("Família").tag(1)
                            Text("Trabalho").tag(2)
                        }
                    }
                }
                .tabItem { Label("Dados", systemImage: "person") }

                NavigationStack {
                    VStack(alignment: .leading, spacing: 8) {
                        Text("Histórico de \(agenda.contatos[indice].nome)").font(.headline)
                        NavigationLink("Última conversa", destination: Text("Conversamos sobre o mOS ontem.").padding(8))
                        NavigationLink("Aniversário", destination: Text("Faz \(agenda.contatos[indice].idade + 1) anos em breve!").padding(8))
                    }
                }
                .tabItem { Label("Histórico", systemImage: "clock") }
            }
            Button("Avisar") { aviso = true }
        }
        .alert("Contato salvo", isPresented: $aviso, message: "As alterações ficam na agenda.")
    }
}

struct ContentView: View {
    @StateObject private var agenda = Agenda()
    @State private var selecionado: Int? = 1
    @State private var novo = false
    @State private var nomeNovo = ""
    @State private var mudancas = 0

    var body: some View {
        NavigationSplitView {
            Text("Agenda").font(.headline)
            List(agenda.contatos, id: { $0.id }, selection: $selecionado) { c in
                Text(c.nome)
            }
            Button("Novo contato…") { novo = true }
            Text("\(mudancas) seleção(ões)").font(.caption).foregroundColor(.secondary)
        } detail: {
            if let s = selecionado, let i = agenda.contatos.firstIndex(where: { $0.id == s }) {
                Detalhe(agenda: agenda, indice: i)
            } else {
                Text("Escolha um contato").foregroundColor(.secondary)
            }
        }
        .onChange(of: selecionado ?? 0) { _ in mudancas += 1 }
        .sheet(isPresented: $novo) {
            VStack(alignment: .leading, spacing: 10) {
                Text("Novo contato").font(.headline)
                TextField("Nome", text: $nomeNovo).frame(width: 240)
                HStack {
                    Button("Cancelar") { novo = false }
                    Button("Adicionar") {
                        agenda.contatos.append(Contato(id: agenda.proximo, nome: nomeNovo, cidade: "", idade: 30, tipo: 0))
                        selecionado = agenda.proximo
                        agenda.proximo += 1
                        nomeNovo = ""
                        novo = false
                    }
                    .disabled(nomeNovo.isEmpty)
                }
            }
        }
    }
}
