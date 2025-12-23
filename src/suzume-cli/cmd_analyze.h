#ifndef SUZUME_CLI_CMD_ANALYZE_H_
#define SUZUME_CLI_CMD_ANALYZE_H_

#include "cli_common.h"

namespace suzume::cli {

/**
 * @brief Execute analyze command
 * @param args Parsed command arguments
 * @return Exit code (0 = success)
 */
int cmdAnalyze(const CommandArgs& args);

}  // namespace suzume::cli

#endif  // SUZUME_CLI_CMD_ANALYZE_H_
