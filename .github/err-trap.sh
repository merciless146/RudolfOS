# Sourced by every workflow step (BASH_ENV): when a command fails, its step, line and text are written as a GitHub
# annotation, which shows on the run's summary page and can be read through the checks API without the full log.
set -E
trap 'echo "::error title=Failed in ${GITHUB_ACTION:-step}::line $LINENO: $BASH_COMMAND (exit $?)"' ERR
