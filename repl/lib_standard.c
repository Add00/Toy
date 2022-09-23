#include "lib_standard.h"

#include "memory.h"

#include <time.h>

static int nativeClock(Interpreter* interpreter, LiteralArray* arguments) {
	//no arguments
	if (arguments->count != 0) {
		interpreter->errorOutput("Incorrect number of arguments to clock\n");
		return -1;
	}

	//get the time from C (what a pain)
	time_t rawtime = time(NULL);
	struct tm* timeinfo = localtime( &rawtime );
	char* timestr = asctime(timeinfo);

	//push to the stack
	int len = strlen(timestr) - 1; //-1 for the newline
	Literal timeLiteral = TO_STRING_LITERAL(copyString(timestr, len), len);

	//push to the stack
	pushLiteralArray(&interpreter->stack, timeLiteral);

	//cleanup
	freeLiteral(timeLiteral);

	return 1;
}

//call the hook
typedef struct Natives {
	char* name;
	NativeFn fn;
} Natives;

int hookStandard(Interpreter* interpreter, Literal identifier, Literal alias) {
	//build the natives list
	Natives natives[] = {
		{"clock", nativeClock},
		{NULL, NULL}
	};

	/* TODO: too exhuasted to fix this right now
	//store the library in an aliased dictionary
	if (!IS_NULL(alias)) {
		//get the alias as a string
		Literal dictName = TO_STRING_LITERAL( copyString(AS_IDENTIFIER(alias), alias.as.identifier.length), alias.as.identifier.length );

		//make sure the name isn't taken
		if (existsLiteralDictionary(&interpreter->scope->variables, dictName)) {
			interpreter->errorOutput("Can't override an existing variable\n");
			freeLiteral(dictName);
			return false;
		}

		LiteralDictionary* dictionary = ALLOCATE(LiteralDictionary, 1);
		initLiteralDictionary(dictionary);

		for (int i = 0; natives[i].name; i++) {
			Literal name = TO_STRING_LITERAL(copyString(natives[i].name, strlen(natives[i].name)), strlen(natives[i].name));
			Literal func = TO_FUNCTION_LITERAL((void*)natives[i].fn, 0);
			func.type = LITERAL_FUNCTION_NATIVE;

			setLiteralDictionary(dictionary, name, func);

			freeLiteral(name);
			freeLiteral(func);
		}

		//build the type
		Literal type = TO_TYPE_LITERAL(LITERAL_DICTIONARY, true);
		Literal strType = TO_TYPE_LITERAL(LITERAL_STRING, true);
		Literal fnType = TO_TYPE_LITERAL(LITERAL_FUNCTION_NATIVE, true);
		TYPE_PUSH_SUBTYPE(&type, strType);
		TYPE_PUSH_SUBTYPE(&type, fnType);

		//set scope
		Literal dict = TO_DICTIONARY_LITERAL(dictionary);
		setLiteralDictionary(&interpreter->scope->variables, dictName, dict);
		setLiteralDictionary(&interpreter->scope->types, dictName, type);

		//cleanup
		freeLiteral(dict);
		freeLiteral(type);
		freeLiteral(dictName);
		return 0;
	}
	*/

	//default
	for (int i = 0; natives[i].name; i++) {
		injectNativeFn(interpreter, natives[i].name, natives[i].fn);
	}

	return 0;
}
