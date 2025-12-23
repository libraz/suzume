#include <iostream>

#include "cli_common.h"
#include "cmd_analyze.h"
#include "cmd_dict.h"
#include "cmd_test.h"

using namespace suzume::cli;

int main(int argc, char* argv[]) {
  // Parse arguments
  auto args = parseArgs(argc, argv);

  // Handle help and version
  if (args.help && args.command.empty()) {
    printHelp();
    return 0;
  }

  if (args.command == "help") {
    printHelp();
    return 0;
  }

  if (args.command == "version") {
    printVersion();
    return 0;
  }

  // Route to command handlers
  if (args.command == "analyze") {
    return cmdAnalyze(args);
  }

  if (args.command == "dict") {
    return cmdDict(args);
  }

  if (args.command == "test") {
    return cmdTest(args);
  }

  // Unknown command
  printError("Unknown command: " + args.command);
  printHelp();
  return 1;
}