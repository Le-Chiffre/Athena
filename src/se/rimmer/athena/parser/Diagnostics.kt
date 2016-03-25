package se.rimmer.athena.parser

interface Diagnostics {
    fun warning(text: String)
    fun error(text: String)
}

class PrintDiagnostics: Diagnostics {
    override fun warning(text: String) {
        println("Warning: $text")
    }

    override fun error(text: String) {
        println("Error: $text")
    }
}