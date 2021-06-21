#include "tools/shell/include/embedded_shell.h"

#include <iostream>

// prompt for user input
const char* PROMPT = "graphflowdb> ";
// file to read/write shell history
const char* HISTORY_PATH = "history.txt";

// build-in shell command
const char* HELP = ":help";
const char* CLEAR = ":clear";
const char* QUIT = ":quit";
const char* THREAD = ":thread";

const char* TAB = "    ";

// cypher keyword (LOAD CSV related keyword are not added yet)
const char* EXPLAIN = "EXPLAIN";
const char* PROFILE = "PROFILE";
const char* WITH = "WITH";
const char* AS = "AS";
const char* MATCH = "MATCH";
const char* RETURN = "RETURN";
const char* WHERE = "WHERE";
const char* OR = "OR";
const char* AND = "AND";
const char* NOT = "NOT";
const char* STARTS = "STARTS";
const char* ENDS = "ENDS";
const char* CONTAINS = "CONTAINS";
const char* IS = "IS";
const char* NULL_ = "NULL";
const char* COUNT = "COUNT";
const char* TRUE = "TRUE";
const char* FALSE = "FALSE";

void completion(const char* buf, linenoiseCompletions* lc) {
    if (buf[0] == 'E' || buf[0] == 'e') {
        if (strlen(buf) < 2 || buf[1] == 'X' || buf[1] == 'x') {
            linenoiseAddCompletion(lc, EXPLAIN);
        } else {
            linenoiseAddCompletion(lc, ENDS);
        }
    } else if (buf[0] == 'P' || buf[0] == 'p') {
        linenoiseAddCompletion(lc, PROFILE);
    } else if (buf[0] == 'W' || buf[0] == 'w') {
        if (strlen(buf) < 2 || buf[1] == 'I' || buf[1] == 'i') {
            linenoiseAddCompletion(lc, WITH);
        } else {
            linenoiseAddCompletion(lc, WHERE);
        }
    } else if (buf[0] == 'A' || buf[0] == 'a') {
        linenoiseAddCompletion(lc, AND);
    } else if (buf[0] == 'M' || buf[0] == 'm') {
        linenoiseAddCompletion(lc, MATCH);
    } else if (buf[0] == 'R' || buf[0] == 'r') {
        linenoiseAddCompletion(lc, RETURN);
    } else if (buf[0] == 'N' || buf[0] == 'n') {
        linenoiseAddCompletion(lc, NOT);
    } else if (buf[0] == 'S' || buf[0] == 's') {
        linenoiseAddCompletion(lc, STARTS);
    } else if (buf[0] == 'C' || buf[0] == 'c') {
        if (strlen(buf) < 3 || buf[2] == 'U' || buf[2] == 'u') {
            linenoiseAddCompletion(lc, COUNT);
        } else {
            linenoiseAddCompletion(lc, CONTAINS);
        }
    }
}

void EmbeddedShell::initialize() {
    linenoiseHistoryLoad(HISTORY_PATH);
    linenoiseSetCompletionCallback(completion);
}

void EmbeddedShell::run() {
    char* line;
    while ((line = linenoise(PROMPT)) != nullptr) {
        auto lineStr = string(line);
        if (strcmp(line, HELP) == 0) {
            printHelp();
        } else if (strcmp(line, CLEAR) == 0) {
            linenoiseClearScreen();
        } else if (strcmp(line, QUIT) == 0) {
            free(line);
            break;
        } else if (lineStr.rfind(THREAD) == 0) {
            try {
                context.numThreads = stoi(lineStr.substr(string(THREAD).length()));
                printf("numThreads set as %lu\n", context.numThreads);
            } catch (exception& e) { printf("%s\n", e.what()); }

        } else {
            context.query = lineStr;
            try {
                system.executeQuery(context);
                printExecutionResult();
            } catch (exception& e) { printf("%s\n", e.what()); }
        }
        linenoiseHistoryAdd(line);
        linenoiseHistorySave("history.txt");
        free(line);
    }
}

void EmbeddedShell::printHelp() {
    printf("%s:help %sget command list\n", TAB, TAB);
    printf("%s:clear %sclear shell\n", TAB, TAB);
    printf("%s:quit %sexit from shell\n", TAB, TAB);
    printf("%s:thread [thread] %snumber of threads for execution\n", TAB, TAB);
}

void EmbeddedShell::printExecutionResult() {
    if (context.enable_explain) {
        // print plan without execution
        string plan = context.planPrinter->printPlanToJson(*context.profiler).dump(4);
        printf("%s\n", plan.c_str());
    } else {
        // print query result (numTuples & tuples)
        printf(">> Number of output tuples: %lu\n", context.queryResult->numTuples);
        if (context.profiler->enabled) {
            // print plan with profiling metrics
            printf("==============================================\n");
            printf("=============== Profiler Summary =============\n");
            printf("==============================================\n");
            printf(">> %s: %.2fms\n", BINDING_STAGE.c_str(),
                context.profiler->sumAllTimeMetricsWithKey(BINDING_STAGE));
            printf(">> %s: %.2fms\n", PLANNING_STAGE.c_str(),
                context.profiler->sumAllTimeMetricsWithKey(PLANNING_STAGE));
            printf(">> %s: %.2fms\n", MAPPING_STAGE.c_str(),
                context.profiler->sumAllTimeMetricsWithKey(MAPPING_STAGE));
            printf(">> %s: %.2fms\n", EXECUTING_STAGE.c_str(),
                context.profiler->sumAllTimeMetricsWithKey(EXECUTING_STAGE));
            printf(">> plan\n");
            string plan = context.planPrinter->printPlanToJson(*context.profiler).dump(4);
            printf("%s\n", plan.c_str());
        }
    }
}
