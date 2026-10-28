// Print data references to a Gangstar ELF address and their containing functions.
// @category GangstarRio
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;

public class FindGangstarReferences extends GhidraScript {
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) throw new IllegalArgumentException("elf-address");
        long elfAddress = Long.parseUnsignedLong(args[0].replace("0x", ""), 16);
        Address target = toAddr(currentProgram.getImageBase().getOffset() + elfAddress);
        println("References to ELF address " + Long.toHexString(elfAddress) +
                " (Ghidra " + target + "):");
        for (Reference reference : getReferencesTo(target)) {
            Function function = getFunctionContaining(reference.getFromAddress());
            println("  " + reference.getFromAddress() + " from " +
                    (function == null ? "<no function>" : function.getName() + " at " + function.getEntryPoint()));
        }
    }
}
