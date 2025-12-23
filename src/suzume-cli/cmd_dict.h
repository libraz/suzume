#ifndef SUZUME_CLI_CMD_DICT_H_
#define SUZUME_CLI_CMD_DICT_H_

#include "cli_common.h"

namespace suzume::cli {

/**
 * @brief Execute dict command
 * @param args Parsed command arguments
 * @return Exit code (0 = success)
 */
int cmdDict(const CommandArgs& args);

}  // namespace suzume::cli

#endif  // SUZUME_CLI_CMD_DICT_H_
