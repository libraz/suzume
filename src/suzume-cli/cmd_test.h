#ifndef SUZUME_CLI_CMD_TEST_H_
#define SUZUME_CLI_CMD_TEST_H_

#include "cli_common.h"

namespace suzume::cli {

/**
 * @brief Execute test command
 * @param args Parsed command arguments
 * @return Exit code (0 = success)
 */
int cmdTest(const CommandArgs& args);

}  // namespace suzume::cli

#endif  // SUZUME_CLI_CMD_TEST_H_
