# SagePico firmware entry — loads native bridge then enters REPL

let pico = ffi_open("pico")

print "SagePico: Hello from Sage on RP2350!"
print "Native bridge loaded — entering REPL"
