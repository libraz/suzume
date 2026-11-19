#include <iostream>

#include "cli_common.h"
#include "cmd_analyze.h"
#include "cmd_dict.h"
#include "cmd_test.h"

using namespace suzume::cli;

int main(int argc, char* argv[]) {
  try {
    // Parse arguments
    auto args = parseArgs(argc, argv);

    if (!args.parse_error.empty()) {
      printError(args.parse_error);
      return 2;
    }

    if (args.version) {
      printVersion();
      std::cout.flush();
      if (!std::cout) {
        printError("failed to write output");
        return 1;
      }
      return 0;
    }

    int exit_code = 0;

    // Handle help and version
    if (args.help && args.command.empty()) {
      printHelp();
    } else if (args.command == "help") {
      printHelp();
    } else if (args.command == "version") {
      printVersion();
    } else if (args.command == "analyze") {
      exit_code = cmdAnalyze(args);
    } else if (args.command == "dict") {
      exit_code = cmdDict(args);
    } else if (args.command == "test") {
      exit_code = cmdTest(args);
    } else {
      printError("unknown command: " + args.command);
      printHelp(std::cerr);
      exit_code = 1;
    }

    // iostream buffers can defer failures such as a full output filesystem.
    // Do not report success after writing only a truncated response.
    std::cout.flush();
    if (!std::cout) {
      printError("failed to write output");
      return 1;
    }
    return exit_code;
  } catch (const std::exception& error) {
    printError("unexpected error: " + std::string(error.what()));
    return 1;
  } catch (...) {
    printError("unexpected non-standard error");
    return 1;
  }
}
