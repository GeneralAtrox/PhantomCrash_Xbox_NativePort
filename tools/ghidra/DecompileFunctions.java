// Decompiles the functions at the given addresses into one C file each.
// Usage (headless post-script): DecompileFunctions.java <output-dir> <addr> [<addr> ...]
// Addresses are hexadecimal, with or without 0x. Also decompiles each function's
// direct callees when an address is suffixed with '+' (one level deep). Creates a
// function at an address with none (unsaved when the project is opened -readOnly).
// @category PhantomCrash

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import java.io.File;
import java.io.PrintWriter;
import java.util.LinkedHashSet;
import java.util.Set;

public class DecompileFunctions extends GhidraScript {
  @Override
  protected void run() throws Exception {
    String[] args = getScriptArgs();
    if (args.length < 2) {
      throw new IllegalArgumentException("usage: <output-dir> <addr> [<addr> ...]");
    }
    File outDir = new File(args[0]);
    if (!outDir.isDirectory() && !outDir.mkdirs()) {
      throw new IllegalStateException("cannot create " + outDir);
    }

    Set<Function> targets = new LinkedHashSet<>();
    for (int i = 1; i < args.length; i++) {
      boolean withCallees = args[i].endsWith("+");
      String text = withCallees ? args[i].substring(0, args[i].length() - 1) : args[i];
      Address address = toAddr(Long.parseLong(text.replaceFirst("^0[xX]", ""), 16));
      Function function = getFunctionContaining(address);
      if (function == null) {
        // Code reached only through data pointers (task callbacks) has no function yet.
        disassemble(address);
        function = createFunction(address, null);
      }
      if (function == null) {
        println("no function at " + address);
        continue;
      }
      targets.add(function);
      if (withCallees) {
        targets.addAll(function.getCalledFunctions(monitor));
      }
    }

    DecompInterface decompiler = new DecompInterface();
    decompiler.openProgram(currentProgram);
    try {
      for (Function function : targets) {
        if (function.isThunk() || function.isExternal()) {
          continue;
        }
        DecompileResults results = decompiler.decompileFunction(function, 60, monitor);
        String safeName = function.getName().replaceAll("[^A-Za-z0-9_.-]", "_");
        String name = function.getEntryPoint().toString() + "_" + safeName + ".c";
        try (PrintWriter out = new PrintWriter(new File(outDir, name), "UTF-8")) {
          if (results.decompileCompleted()) {
            out.print(results.getDecompiledFunction().getC());
          } else {
            out.println("// decompile failed: " + results.getErrorMessage());
          }
        }
      }
      println("decompiled " + targets.size() + " functions to " + outDir);
    } finally {
      decompiler.dispose();
    }
  }
}
