// mOS — Foundation (fase 5, marco B): Hashable dos tipos-string ObjC (NS_TYPED_EXTENSIBLE_ENUM). Na Apple o importador os
// faz Hashable pelo rawValue String; dentro deste módulo ele não faz a ponte NSString<->String, então o rawValue é a
// NSString (Hashable pelo overlay ObjectiveC) e a conformidade vem daqui.

extension URLResourceKey: Hashable {}
extension NSNotificationName: Hashable {}
