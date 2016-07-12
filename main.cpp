#include <string>
#include <stdexcept>
#include <fstream>
#include <sstream>

#include <readline/readline.h>
#include <readline/history.h>

#include "Parser.h"
#include "Runner.h"
#include "Debug.h"

int main(int argc, char const *argv[]) {
	Parser parser = Parser();
	Runner runner = Runner();
	//first, we print fun info and load the prelude
	printf("Charm Interpreter v%s\n", "0.0.1");
	printf("Made by @Aearnus\n");
	printf("Looking for Prelude.charm...\n");
	std::stringstream preludeFile;
	try {
		std::ifstream fd("Prelude.charm");
		std::string line;
		while (std::getline(fd, line)) {
			preludeFile << line;
			printf("%s\n", line.c_str());
		}
		//load up the Prelude.charm file
		runner.run(parser.parse(preludeFile.str()));
		printf("Prelude.charm loaded.\n");
	} catch (std::exception &e) {
		printf("Prelude.charm nonexistant or unopenable.\n");
	}

	//begin the interactive loop
	while (true) {
		std::string codeInput(readline("Charm$ "));
		std::vector<CharmFunction> parsedProgram = parser.parse(codeInput);
		ONLYDEBUG printf("TOKEN TYPES: ");
		for (auto currentFunction : parsedProgram) {
			ONLYDEBUG printf("%i ", currentFunction.functionType);
		}
		ONLYDEBUG printf("\n");
		try {
			runner.run(parsedProgram);
		} catch (const std::runtime_error& e) {
			printf("ERRROR: %s\n", e.what());
			return -1;
		}
		ONLYDEBUG printf("MODIFIED STACK AREA: %i\n", runner.getModifiedStackArea());
		ONLYDEBUG printf("THE STACK (just the types): ");
		std::vector<CharmFunction> postStack = runner.getStack();
		for (unsigned int stackIndex = runner.getModifiedStackArea(); stackIndex > 0; stackIndex--) {
			ONLYDEBUG printf("%i ", postStack.at(postStack.size() - stackIndex).functionType);
		}
		ONLYDEBUG printf("\n");
		ONLYDEBUG printf("DEFINED FUNCTIONS: ");
		auto functionDefinitions = runner.getFunctionDefinitions();
		for (auto currentFunction : functionDefinitions) {
			ONLYDEBUG printf("%s ", currentFunction.functionName.c_str());
		}
		ONLYDEBUG printf("\n");
	}
	return 0;
}
