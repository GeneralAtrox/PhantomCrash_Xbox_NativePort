// Read-only summary of an analysed XBE: every function with its section and
// namespace, plus counts separating SDK-identified code from game code.
// Arguments: <output directory>
// @category PhantomCrash
import java.io.BufferedWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Namespace;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;

public class ExportXbeSummary extends GhidraScript {
  // Namespaces the XbSymbolDatabase analyzer creates for SDK matches.
  private static final Set<String> SDK_NAMESPACES =
      Set.of("D3D8", "D3DX8", "DSOUND", "XAPILIB", "XGRAPHC", "XACTENG", "XNET", "XONLINE");

  private static String csv(String value) {
    return "\"" + value.replace("\"", "\"\"") + "\"";
  }

  @Override
  protected void run() throws Exception {
    String[] args = getScriptArgs();
    if (args.length != 1) {
      throw new IllegalArgumentException("usage: ExportXbeSummary <output directory>");
    }
    Path output = Path.of(args[0]);
    Files.createDirectories(output);

    Map<String, int[]> bySection = new TreeMap<>();
    Map<String, Integer> byNamespace = new TreeMap<>();
    int total = 0;
    int sdkIdentified = 0;
    int userNamed = 0;
    try (BufferedWriter writer = Files.newBufferedWriter(output.resolve("functions.csv"),
        StandardCharsets.UTF_8)) {
      writer.write("address,size,section,namespace,name,source\n");
      for (Function function : currentProgram.getFunctionManager().getFunctions(true)) {
        monitor.checkCancelled();
        MemoryBlock block = currentProgram.getMemory().getBlock(function.getEntryPoint());
        String section = block == null ? "(none)" : block.getName();
        Namespace namespace = function.getParentNamespace();
        String namespaceName = namespace.isGlobal() ? "" : namespace.getName(true);
        Symbol symbol = function.getSymbol();
        boolean sdk = SDK_NAMESPACES.contains(namespaceName);
        long size = function.getBody().getNumAddresses();

        total++;
        sdkIdentified += sdk ? 1 : 0;
        userNamed += symbol.getSource() == SourceType.USER_DEFINED ? 1 : 0;
        int[] counts = bySection.computeIfAbsent(section, key -> new int[3]);
        counts[0]++;
        counts[1] += sdk ? 1 : 0;
        counts[2] += (int) size;
        byNamespace.merge(namespaceName.isEmpty() ? "(global)" : namespaceName, 1, Integer::sum);
        writer.write(String.join(",", "0x" + function.getEntryPoint(), Long.toString(size),
            csv(section), csv(namespaceName), csv(function.getName()),
            symbol.getSource().toString()) + "\n");
      }
    }

    StringBuilder json = new StringBuilder();
    json.append("{\n  \"program\": ").append(csv(currentProgram.getName()))
        .append(",\n  \"functions\": ").append(total)
        .append(",\n  \"sdk_identified_functions\": ").append(sdkIdentified)
        .append(",\n  \"user_named_functions\": ").append(userNamed)
        .append(",\n  \"sections\": {");
    String separator = "\n";
    for (Map.Entry<String, int[]> entry : bySection.entrySet()) {
      int[] counts = entry.getValue();
      json.append(separator).append("    ").append(csv(entry.getKey()))
          .append(": {\"functions\": ").append(counts[0])
          .append(", \"sdk_identified\": ").append(counts[1])
          .append(", \"bytes\": ").append(counts[2]).append("}");
      separator = ",\n";
    }
    json.append("\n  },\n  \"namespaces\": {");
    separator = "\n";
    for (Map.Entry<String, Integer> entry : byNamespace.entrySet()) {
      json.append(separator).append("    ").append(csv(entry.getKey())).append(": ")
          .append(entry.getValue());
      separator = ",\n";
    }
    json.append("\n  }\n}\n");
    Files.writeString(output.resolve("summary.json"), json.toString(), StandardCharsets.UTF_8);
    println("ExportXbeSummary: " + total + " functions, " + sdkIdentified
        + " SDK-identified; wrote " + output);
  }
}
