# FileIO Library

## Defined Constants

### MAX_FILENAME_SIZE: int

Represents the longest allowed filename, passing longer names to `open` will result in an error.

### MAX_FILES_OPEN: int

The max number of concurrent files that can be open at the same time.

### END_OF_FILE: int

This constant represents the End-of-File (EOF) character. It signifies the termination point at the end of a file.

## Defined Variables

### input: opaque&lt;file&gt;

This variable represents the standard input file. It is meant solely for reading data from it.

### output: opaque&lt;file&gt;

This variable represents the standard output file. It is meant exclusively for writing data to it.

## Defined Access Functions

### open(filename: string, mode: string): opaque&lt;file&gt; | null

Opens a file with the speifide name in the required mode. The `filename` should be less then `MAX_FILENAME_SIZE`, and be located in the drive system. Additionally, `mode` can be one of `r`, `w`, `a`, `r+`, `w+`, `a+`, see [file access flags](https://en.cppreference.com/w/c/io/fopen) for more information.Will Returns a file object on a successful opening or returns null if the file cannot be opened.

### close(): bool

Closes the file stream and returns false if an error happens, true otherwise. Method must be called on a variable of type opaque&lt;file&gt;.

## Defined Operation Functions

All operations can only be called on an object of type opaque&lt;file&gt;.

### read(type: type): type

Read a type from the file. The `type` must be a literal type, such as `bool`, `int`, `float`, or `string`. Reading a string is greedy, it will attempt to read the largest possible string, up to the max toy string size. Returns null if an invalid type is passed in, otherwise it will return the same type as `type`.

### write(value: any): bool

Write a value to the file. The `value` must be a literal type, such as `bool`, `int`, `float`, or `string`. Returns a boolean representing the success of the operation.

### rename(newName: string): bool

Changes the current name of the file to `newName`. `newName` must also be within the drive system. Returns true if it succeeds, false otherwise.

### seek(origin: string, offset: int): bool

Changes the current position in the file. The `origin` is the starting place of the seek operation can be one of: `bgn`, `cur`, `end`. `offset` is how many bytes to move the file position. Returns true if the operation succeeds, false otherwise.

## Defined Accessor Functions

All accessors can only be called on an object of type opaque&lt;file&gt;.

### error(): bool

Returns true if the file has an error false otherwise.

### completed(): bool

Returns true if the current position in the file is at the end, otherwise false.

### position(): int

Returns the current position in the file. Or returns `INT_MAX` if the position in the file is greater then what can be stored in an integer.

### size(): int

Returns the size of the file in bytes. Will return `INT_MAX` if the size of the file is greater then what can be stored in an integer.

### mode(): string

Returns the current `mode` of the file object.
