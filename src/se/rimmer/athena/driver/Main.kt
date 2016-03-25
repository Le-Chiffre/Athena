package se.rimmer.athena.driver

import se.rimmer.athena.parser.DefinitionParser
import se.rimmer.athena.parser.Module
import se.rimmer.athena.parser.Parser
import se.rimmer.athena.parser.PrintDiagnostics
import java.io.File

fun main(args: Array<String>) {
    val sourceFolder = if(args.size > 0) args[0] else "./"
    val sourceDir = File(sourceFolder).listFiles()
    if(sourceDir == null) {
        println("No input files found in $sourceFolder.")
        return
    }

    val sourceFiles = sourceDir.filter {it.isFile && it.extension == "at"}
    val module = Module("")
    val diag = PrintDiagnostics()

    for(source in sourceFiles) {
        val parser = DefinitionParser(source.readText(), diag)

        try {
            parser.parseModule(module)
        } catch(e: Parser.ParseError) {
            println("Error in ${source.canonicalFile}")
            println("    ${e.message}")
        } catch(e: Exception) {
            println("Internal error ${source.canonicalFile}")
            e.printStackTrace()
        }
    }

    for(d in module.declarations) {
        println(d)
    }
}