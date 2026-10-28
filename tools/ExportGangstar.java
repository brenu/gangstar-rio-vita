// Export selected functions from the private Gangstar Rio ELF for analysis.
// @category GangstarRio
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.listing.Function;
import java.io.PrintWriter;
import java.io.File;

public class ExportGangstar extends GhidraScript {
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) throw new IllegalArgumentException("output-dir name:elf-address [...]");
        File output = new File(args[0]);
        output.mkdirs();
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        long imageBase = currentProgram.getImageBase().getOffset();
        println("Ghidra image base: " + Long.toHexString(imageBase));
        for (int i = 1; i < args.length; i++) {
            String[] pair = args[i].split(":", 2);
            var address = toAddr(imageBase + Long.parseUnsignedLong(pair[1], 16));
            Function function = getFunctionAt(address);
            if (function == null) {
                disassemble(address);
                function = createFunction(address, pair[0]);
            }
            if (function == null) { printerr("No function at " + address); continue; }
            DecompileResults result = decompiler.decompileFunction(function, 60, monitor);
            try (PrintWriter writer = new PrintWriter(new File(output, pair[0] + ".c"))) {
                writer.println("/* ELF virtual address " + pair[1] + "; Ghidra address " + address +
                               "; verify inferred types against Java/JNI declarations. */");
                if (result.decompileCompleted()) writer.print(result.getDecompiledFunction().getC());
                else writer.println("/* Failed: " + result.getErrorMessage() + " */");
            }
            println("Exported " + pair[0] + " at " + address);
        }
        decompiler.dispose();
    }
}
