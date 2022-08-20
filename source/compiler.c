#include "compiler.h"

#include "memory.h"

#include "literal.h"
#include "literal_array.h"
#include "literal_dictionary.h"

#include "console_colors.h"

#include <stdio.h>

void initCompiler(Compiler* compiler) {
	initLiteralArray(&compiler->literalCache);
	compiler->bytecode = NULL;
	compiler->capacity = 0;
	compiler->count = 0;
}

//separated out, so it can be recursive
static int writeNodeCompoundToCache(Compiler* compiler, Node* node) {
	int index = -1;

	//for both, stored as an array
	LiteralArray* store = ALLOCATE(LiteralArray, 1);
	initLiteralArray(store);

	//emit an array or a dictionary definition
	if (node->compound.literalType == LITERAL_DICTIONARY) {
		//ensure each literal key and value are in the cache, individually
		for (int i = 0; i < node->compound.count; i++) {
			//keys
			switch(node->compound.nodes[i].pair.left->type) {
				case NODE_LITERAL: {
					//keys are literals
					int key = findLiteralIndex(&compiler->literalCache, node->compound.nodes[i].pair.left->atomic.literal);
					if (key < 0) {
						key = pushLiteralArray(&compiler->literalCache, node->compound.nodes[i].pair.left->atomic.literal);
					}

					pushLiteralArray(store, TO_INTEGER_LITERAL(key));
				}
				break;

				case NODE_COMPOUND: {
						int key = writeNodeCompoundToCache(compiler, node->compound.nodes[i].pair.left);

						pushLiteralArray(store, TO_INTEGER_LITERAL(key));
				}
				break;

				default:
					fprintf(stderr, "[Internal] Unrecognized key node type in writeNodeCompoundToCache()");
					return -1;
			}

			//values
			switch(node->compound.nodes[i].pair.right->type) {
				case NODE_LITERAL: {
					//values are literals
					int val = findLiteralIndex(&compiler->literalCache, node->compound.nodes[i].pair.right->atomic.literal);
					if (val < 0) {
						val = pushLiteralArray(&compiler->literalCache, node->compound.nodes[i].pair.right->atomic.literal);
					}

					pushLiteralArray(store, TO_INTEGER_LITERAL(val));
				}
				break;

				case NODE_COMPOUND: {
						int val = writeNodeCompoundToCache(compiler, node->compound.nodes[i].pair.right);

						pushLiteralArray(store, TO_INTEGER_LITERAL(val));
				}
				break;

				default:
					fprintf(stderr, "[Internal] Unrecognized value node type in writeNodeCompoundToCache()");
					return -1;
			}
		}

		//push the store to the cache, with instructions about how pack it
		index = pushLiteralArray(&compiler->literalCache, TO_DICTIONARY_LITERAL(store)); //WARNING: pushed as a dictionary, so below can recognize it
	}
	else if (node->compound.literalType == LITERAL_ARRAY) {
		//ensure each literal value is in the cache, individually
		for (int i = 0; i < node->compound.count; i++) {
			switch(node->compound.nodes[i].type) {
				case NODE_LITERAL: {
					//values
					int val = findLiteralIndex(&compiler->literalCache, node->compound.nodes[i].atomic.literal);
					if (val < 0) {
						val = pushLiteralArray(&compiler->literalCache, node->compound.nodes[i].atomic.literal);
					}

					pushLiteralArray(store, TO_INTEGER_LITERAL(val));
				}
				break;

				case NODE_COMPOUND: {
					int val = writeNodeCompoundToCache(compiler, &node->compound.nodes[i]);

					index = pushLiteralArray(store, TO_INTEGER_LITERAL(val));
				}
				break;

				default:
					fprintf(stderr, "[Internal] Unrecognized node type in writeNodeCompoundToCache()");
					return -1;
			}
		}

		//push the store to the cache, with instructions about how pack it
		index = pushLiteralArray(&compiler->literalCache, TO_ARRAY_LITERAL(store));
	}
	else {
		fprintf(stderr, "[Internal] Unrecognized compound type in writeNodeCompoundToCache()");
	}

	return index;
}

static int writeLiteralTypeToCache(LiteralArray* literalCache, Literal literal) {
	//I don't like storing types in an array, but it's the easiest and most straight forward method
	LiteralArray* store = ALLOCATE(LiteralArray, 1);
	initLiteralArray(store);

	//store the base literal in the store
	pushLiteralArray(store, literal);

	//if it's a compound type, recurse and store the results
	if (AS_TYPE(literal).typeOf == LITERAL_ARRAY || AS_TYPE(literal).typeOf == LITERAL_DICTIONARY) {
		for (int i = 0; i < AS_TYPE(literal).count; i++) {
			//write the values to the cache, and the indexes to the store
			int subIndex = writeLiteralTypeToCache(literalCache, ((Literal*)(AS_TYPE(literal).subtypes))[i]);
			pushLiteralArray(store, TO_INTEGER_LITERAL(subIndex));
		}
	}

	//push the store to the cache, tweaking the type
	Literal lit = TO_ARRAY_LITERAL(store);
	lit.type = LITERAL_TYPE_INTERMEDIATE; //NOTE: tweaking the type usually isn't a good idea
	return pushLiteralArray(literalCache, lit);
}

void writeCompiler(Compiler* compiler, Node* node) {
	//grow if the bytecode space is too small
	if (compiler->capacity < compiler->count + 1) {
		int oldCapacity = compiler->capacity;

		compiler->capacity = GROW_CAPACITY(oldCapacity);
		compiler->bytecode = GROW_ARRAY(unsigned char, compiler->bytecode, oldCapacity, compiler->capacity);
	}

	//determine node type
	switch(node->type) {
		//TODO: more types, like variables, etc.
		case NODE_ERROR: {
			fprintf(stderr, ERROR "[Internal] NODE_ERROR encountered in writeCompiler()\n" RESET);
			compiler->bytecode[compiler->count++] = OP_EOF; //1 byte
		}
		break;

		case NODE_LITERAL: {
			//ensure the literal is in the cache
			int index = findLiteralIndex(&compiler->literalCache, node->atomic.literal);
			if (index < 0) {
				index = pushLiteralArray(&compiler->literalCache, node->atomic.literal);
			}

			//push the node opcode to the bytecode
			if (index >= 256) {
				//push a "long" index
				compiler->bytecode[compiler->count++] = OP_LITERAL_LONG; //1 byte
				*((unsigned short*)(compiler->bytecode + compiler->count)) = (unsigned short)index; //2 bytes

				compiler->count += sizeof(unsigned short);
			}
			else {
				//push the index
				compiler->bytecode[compiler->count++] = OP_LITERAL; //1 byte
				compiler->bytecode[compiler->count++] = (unsigned char)index; //1 byte
			}
		}
		break;

		case NODE_UNARY:
			//pass to the child node, then embed the unary command (print, negate, etc.)
			writeCompiler(compiler, node->unary.child);
			compiler->bytecode[compiler->count++] = (unsigned char)node->unary.opcode; //1 byte
		break;

		case NODE_BINARY:
			//pass to the child nodes, then embed the binary command (math, etc.)
			writeCompiler(compiler, node->binary.left);
			writeCompiler(compiler, node->binary.right);
			compiler->bytecode[compiler->count++] = (unsigned char)node->binary.opcode; //1 byte
		break;

		case NODE_GROUPING:
			compiler->bytecode[compiler->count++] = (unsigned char)OP_GROUPING_BEGIN; //1 byte
			writeCompiler(compiler, node->grouping.child);
			compiler->bytecode[compiler->count++] = (unsigned char)OP_GROUPING_END; //1 byte
		break;

		case NODE_BLOCK:
			compiler->bytecode[compiler->count++] = (unsigned char)OP_SCOPE_BEGIN; //1 byte

			for (int i = 0; i < node->block.count; i++) {
				writeCompiler(compiler, &(node->block.nodes[i]));
			}

			compiler->bytecode[compiler->count++] = (unsigned char)OP_SCOPE_END; //1 byte
		break;

		case NODE_COMPOUND: {
			int index = writeNodeCompoundToCache(compiler, node);

			//push the node opcode to the bytecode
			if (index >= 256) {
				//push a "long" index
				compiler->bytecode[compiler->count++] = OP_LITERAL_LONG; //1 byte
				*((unsigned short*)(compiler->bytecode + compiler->count)) = (unsigned short)index; //2 bytes

				compiler->count += sizeof(unsigned short);
			}
			else {
				//push the index
				compiler->bytecode[compiler->count++] = OP_LITERAL; //1 byte
				compiler->bytecode[compiler->count++] = (unsigned char)index; //1 byte
			}
		}
		break;

		case NODE_PAIR:
			fprintf(stderr, ERROR "[Internal] NODE_PAIR encountered in writeCompiler()\n" RESET);
		break;

		case NODE_VAR_TYPES: { //TODO: the "type" keyword
			int index = writeLiteralTypeToCache(&compiler->literalCache, node->varTypes.typeLiteral);

			//embed the info into the bytecode
			if (index >= 256) {
				//push a "long" index
				compiler->bytecode[compiler->count++] = OP_TYPE_DECL_LONG; //1 byte
				*((unsigned short*)(compiler->bytecode + compiler->count)) = (unsigned short)index; //2 bytes

				compiler->count += sizeof(unsigned short);
			}
			else {
				//push the index
				compiler->bytecode[compiler->count++] = OP_TYPE_DECL; //1 byte
				compiler->bytecode[compiler->count++] = (unsigned char)index; //1 byte
			}
		}
		break;

		case NODE_VAR_DECL: {
			//first, embed the expression (leaves it on the stack)
			writeCompiler(compiler, node->varDecl.expression);

			//write each piece of the declaration to the bytecode
			int identifierIndex = findLiteralIndex(&compiler->literalCache, node->varDecl.identifier);
			if (identifierIndex < 0) {
				identifierIndex = pushLiteralArray(&compiler->literalCache, node->varDecl.identifier);
			}

			int typeIndex = writeLiteralTypeToCache(&compiler->literalCache, node->varDecl.typeLiteral);

			//embed the info into the bytecode
			if (identifierIndex >= 256 || typeIndex >= 256) {
				//push a "long" declaration
				compiler->bytecode[compiler->count++] = OP_VAR_DECL_LONG; //1 byte

				*((unsigned short*)(compiler->bytecode + compiler->count)) = (unsigned short)identifierIndex; //2 bytes
				compiler->count += sizeof(unsigned short);

				*((unsigned short*)(compiler->bytecode + compiler->count)) = (unsigned short)typeIndex; //2 bytes
				compiler->count += sizeof(unsigned short);
			}
			else {
				//push a declaration
				compiler->bytecode[compiler->count++] = OP_VAR_DECL; //1 byte
				compiler->bytecode[compiler->count++] = (unsigned char)identifierIndex; //1 byte
				compiler->bytecode[compiler->count++] = (unsigned char)typeIndex; //1 byte
			}
		}
		break;

		case NODE_PATH_IF: {
			//process the condition
			writeCompiler(compiler, node->path.condition);

			//cache the point to insert the jump distance at
			compiler->bytecode[compiler->count++] = OP_IF_FALSE_JUMP; //1 byte
			int jumpToElse = compiler->count;
			compiler->count += sizeof(unsigned short); //2 bytes

			//write the then path
			writeCompiler(compiler, node->path.thenPath);

			int jumpToEnd = 0;

			if (node->path.elsePath) {
				//insert jump to end
				compiler->bytecode[compiler->count++] = OP_JUMP; //1 byte
				jumpToEnd = compiler->count;
				compiler->count += sizeof(unsigned short); //2 bytes
			}

			//update the jumpToElse to point here
			compiler->bytecode[jumpToElse] = compiler->count;

			if (node->path.elsePath) {
				//if there's an else path, write it and 
				writeCompiler(compiler, node->path.elsePath);

				//update the jumpToEnd to point here
				compiler->bytecode[jumpToEnd] = compiler->count;
			}
		}
		break;

		// case NODE_PATH_WHILE: {
		// 	//cache the jump point
		// 	int jumpToBeginning = compiler->count;
		// 	compiler->count += sizeof(unsigned short); //2 bytes

		// 	//
		// }
	}
}

void freeCompiler(Compiler* compiler) {
	freeLiteralArray(&compiler->literalCache);
	FREE(unsigned char, compiler->bytecode);
	compiler->bytecode = NULL;
	compiler->capacity = 0;
	compiler->count = 0;
}

static void emitByte(unsigned char** collationPtr, int* capacityPtr, int* countPtr, unsigned char byte) {
	//grow the array
	if (*countPtr + 1 > *capacityPtr) {
		int oldCapacity = *capacityPtr;
		*capacityPtr = GROW_CAPACITY(*capacityPtr);
		*collationPtr = GROW_ARRAY(unsigned char, *collationPtr, oldCapacity, *capacityPtr);
	}

	//append to the collation
	(*collationPtr)[(*countPtr)++] = byte;
}

static void emitShort(unsigned char** collationPtr, int* capacityPtr, int* countPtr, unsigned short bytes) {
	char* ptr = (char*)&bytes;

	emitByte(collationPtr, capacityPtr, countPtr, *ptr);
	ptr++;
	emitByte(collationPtr, capacityPtr, countPtr, *ptr);
}

static void emitInt(unsigned char** collationPtr, int* capacityPtr, int* countPtr, int bytes) {
	char* ptr = (char*)&bytes;

	emitByte(collationPtr, capacityPtr, countPtr, *ptr);
	ptr++;
	emitByte(collationPtr, capacityPtr, countPtr, *ptr);
	ptr++;
	emitByte(collationPtr, capacityPtr, countPtr, *ptr);
	ptr++;
	emitByte(collationPtr, capacityPtr, countPtr, *ptr);
}

static void emitFloat(unsigned char** collationPtr, int* capacityPtr, int* countPtr, float bytes) {
	char* ptr = (char*)&bytes;

	emitByte(collationPtr, capacityPtr, countPtr, *ptr);
	ptr++;
	emitByte(collationPtr, capacityPtr, countPtr, *ptr);
	ptr++;
	emitByte(collationPtr, capacityPtr, countPtr, *ptr);
	ptr++;
	emitByte(collationPtr, capacityPtr, countPtr, *ptr);
}

//return the result
unsigned char* collateCompiler(Compiler* compiler, int* size) {
	int capacity = GROW_CAPACITY(0);
	int count = 0;
	unsigned char* collation = ALLOCATE(unsigned char, capacity);

	//embed the header with version information
	emitByte(&collation, &capacity, &count, TOY_VERSION_MAJOR);
	emitByte(&collation, &capacity, &count, TOY_VERSION_MINOR);
	emitByte(&collation, &capacity, &count, TOY_VERSION_PATCH);

	//embed the build info
	if (strlen(TOY_VERSION_BUILD) + count + 1 > capacity) {
		int oldCapacity = capacity;
		capacity = strlen(TOY_VERSION_BUILD) + count + 1; //full header size
		collation = GROW_ARRAY(unsigned char, collation, oldCapacity, capacity);
	}

	memcpy(&collation[count], TOY_VERSION_BUILD, strlen(TOY_VERSION_BUILD));
	count += strlen(TOY_VERSION_BUILD);
	collation[count++] = '\0'; //terminate the build string

	emitByte(&collation, &capacity, &count, OP_SECTION_END); //terminate header

	//embed the data section (first short is the number of literals)
	emitShort(&collation, &capacity, &count, compiler->literalCache.count);

	//emit each literal by type
	for (int i = 0; i < compiler->literalCache.count; i++) {
		//literal Opcode
		// emitShort(&collation, &capacity, &count, OP_LITERAL); //This isn't needed

		//literal type, followed by literal value
		switch(compiler->literalCache.literals[i].type) {
			case LITERAL_NULL:
				emitByte(&collation, &capacity, &count, LITERAL_NULL);
				//null has no following value
			break;

			case LITERAL_BOOLEAN:
				emitByte(&collation, &capacity, &count, LITERAL_BOOLEAN);
				emitByte(&collation, &capacity, &count, AS_BOOLEAN(compiler->literalCache.literals[i]));
			break;

			case LITERAL_INTEGER:
				emitByte(&collation, &capacity, &count, LITERAL_INTEGER);
				emitInt(&collation, &capacity, &count, AS_INTEGER(compiler->literalCache.literals[i]));
			break;

			case LITERAL_FLOAT:
				emitByte(&collation, &capacity, &count, LITERAL_FLOAT);
				emitFloat(&collation, &capacity, &count, AS_FLOAT(compiler->literalCache.literals[i]));
			break;

			case LITERAL_STRING: {
				emitByte(&collation, &capacity, &count, LITERAL_STRING);

				Literal str = compiler->literalCache.literals[i];

				for (int c = 0; c < STRLEN(str); c++) {
					emitByte(&collation, &capacity, &count, AS_STRING(str)[c]);
				}

				emitByte(&collation, &capacity, &count, '\0'); //terminate the string
			}
			break;

			case LITERAL_ARRAY: {
				emitByte(&collation, &capacity, &count, LITERAL_ARRAY);

				LiteralArray* ptr = AS_ARRAY(compiler->literalCache.literals[i]);

				//length of the array, as a short
				emitShort(&collation, &capacity, &count, ptr->count);

				//each element of the array
				for (int i = 0; i < ptr->count; i++) {
					emitShort(&collation, &capacity, &count, (unsigned short)AS_INTEGER(ptr->literals[i])); //shorts representing the indexes of the values
				}

				freeLiteralArray(ptr);
				FREE(LiteralArray, ptr);
			}
			break;

			case LITERAL_DICTIONARY: {
				emitByte(&collation, &capacity, &count, LITERAL_DICTIONARY);

				LiteralArray* ptr = AS_ARRAY(compiler->literalCache.literals[i]); //used an array for storage above

				//length of the array, as a short
				emitShort(&collation, &capacity, &count, ptr->count); //count is the array size, NOT the dictionary size

				//each element of the array
				for (int i = 0; i < ptr->count; i++) {
					emitShort(&collation, &capacity, &count, (unsigned short)AS_INTEGER(ptr->literals[i])); //shorts representing the indexes of the values
				}

				freeLiteralArray(ptr);
				FREE(LiteralArray, ptr);
			}
			break;

			//TODO: function

			case LITERAL_IDENTIFIER: {
				emitByte(&collation, &capacity, &count, LITERAL_IDENTIFIER);

				Literal identifier = compiler->literalCache.literals[i];

				for (int c = 0; c < STRLEN_I(identifier); c++) {
					emitByte(&collation, &capacity, &count, AS_IDENTIFIER(identifier)[c]);
				}

				emitByte(&collation, &capacity, &count, '\0'); //terminate the string
			}
			break;

			case LITERAL_TYPE: {
				//push a raw type
				emitByte(&collation, &capacity, &count, LITERAL_TYPE);

				Literal typeLiteral = compiler->literalCache.literals[i];

				//what type this literal represents
				emitByte(&collation, &capacity, &count, AS_TYPE(typeLiteral).typeOf);
				emitByte(&collation, &capacity, &count, AS_TYPE(typeLiteral).constant); //if it's constant
			}
			break;

			case LITERAL_TYPE_INTERMEDIATE: {
				emitByte(&collation, &capacity, &count, LITERAL_TYPE_INTERMEDIATE);

				LiteralArray* ptr = AS_ARRAY(compiler->literalCache.literals[i]); //used an array for storage above

				//the base literal
				Literal typeLiteral = ptr->literals[0];

				//what type this literal represents
				emitByte(&collation, &capacity, &count, AS_TYPE(typeLiteral).typeOf);
				emitByte(&collation, &capacity, &count, AS_TYPE(typeLiteral).constant); //if it's constant

				//each element of the array, If they exist, representing sub-types already in the cache
				if (AS_TYPE(typeLiteral).typeOf == LITERAL_ARRAY || AS_TYPE(typeLiteral).typeOf == LITERAL_DICTIONARY) {
					//the type will represent how many to expect in the array
					for (int i = 1; i < ptr->count; i++) {
						emitShort(&collation, &capacity, &count, (unsigned short)AS_INTEGER(ptr->literals[i])); //shorts representing the indexes of the types
					}
				}

				freeLiteralArray(ptr);
				FREE(LiteralArray, ptr);
			}
			break;

			default:
				fprintf(stderr, "[Internal] Unknown literal type encountered within literal cache: %d\n", compiler->literalCache.literals[i].type);
				return NULL;
		}
	}

	emitByte(&collation, &capacity, &count, OP_SECTION_END); //terminate data

	//code section
	for (int i = 0; i < compiler->count; i++) {
		emitByte(&collation, &capacity, &count, compiler->bytecode[i]);
	}

	emitByte(&collation, &capacity, &count, OP_SECTION_END); //terminate code

	emitByte(&collation, &capacity, &count, OP_EOF); //terminate bytecode

	//finalize
	SHRINK_ARRAY(unsigned char, collation, capacity, count);

	*size = count;

	return collation;	
}