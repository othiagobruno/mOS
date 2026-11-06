// Tarefas — app SwiftUI de exemplo para o mOS (o mesmo código compila no macOS com o SwiftUI da Apple).
import SwiftUI

struct Tarefa {
    var titulo: String
    var feita: Bool
}

struct ContentView: View {
    @State private var tarefas: [Tarefa] = [Tarefa(titulo: "Testar SwiftUI no mOS", feita: true),
                                            Tarefa(titulo: "Escrever mais apps em Swift", feita: false)]
    @State private var nova = ""
    @State private var urgencia = 0.5
    @State private var soPendentes = false

    var pendentes: Int { tarefas.filter { !$0.feita }.count }

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("Minhas tarefas").font(.title)
            HStack {
                TextField("Nova tarefa", text: $nova)
                Button("Adicionar") {
                    if !nova.isEmpty {
                        tarefas.append(Tarefa(titulo: nova, feita: false))
                        nova = ""
                    }
                }
                .disabled(nova.isEmpty)
            }
            Toggle("Mostrar só pendentes", isOn: $soPendentes)
            Divider()
            VStack(alignment: .leading, spacing: 6) {
                ForEach(Array(tarefas.indices)) { i in
                    if !soPendentes || !tarefas[i].feita {
                        HStack {
                            Toggle(tarefas[i].titulo, isOn: Binding(get: { tarefas[i].feita },
                                                                     set: { tarefas[i].feita = $0 }))
                            Spacer()
                            Button("Remover") { tarefas.remove(at: i) }
                        }
                    }
                }
            }
            Spacer()
            HStack {
                Text("Urgência")
                Slider(value: $urgencia, in: 0...1)
                Text(urgencia > 0.66 ? "alta" : urgencia > 0.33 ? "média" : "baixa").foregroundColor(.secondary)
            }
            Text("\(pendentes) pendente(s) de \(tarefas.count)").font(.caption).foregroundColor(.secondary)
        }
        .padding(20)
    }
}
