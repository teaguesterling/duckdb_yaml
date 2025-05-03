# YAML Extension for DuckDB

This extension allows DuckDB to read YAML files directly into tables. It provides a simple interface for accessing YAML data within SQL queries.

## AI-written Extension
Claude.ai wrote 99% of the code in this project over the course of a weekend.

## Features

- Read YAML files into DuckDB tables with `read_yaml`
- Preserve document structure with `read_yaml_objects`
- Auto-detect data types from YAML content
- Support for multi-document YAML files
- Handle top-level sequences as rows
- Robust parameter handling and error recovery

## Usage

### Quickstart
```sql
LOAD yaml;
FROM 'data/*.yml'
```

### Loading the Extension

```sql
LOAD yaml;
```

### Reading YAML Files

#### Basic Usage

```sql
-- Single file path
SELECT * FROM read_yaml('data/config.yaml');

-- Glob pattern (uses filesystem glob matching)
SELECT * FROM read_yaml('data/*.yaml');

-- List of files (processes all files in the array)
SELECT * FROM read_yaml(['config1.yaml', 'config2.yaml', 'data/settings.yaml']);

-- Directory path (reads all .yaml and .yml files in directory)
SELECT * FROM read_yaml('data/configs/');

-- Read from a file (creates a row per document or sequence item)
SELECT * FROM read_yaml('path/to/file.yaml');

-- Read preserving document structure (one row per file)
SELECT * FROM read_yaml_objects('path/to/file.yaml');
```

#### With Parameters

```sql
-- Auto-detect types (default: true)
SELECT * FROM read_yaml('file.yaml', auto_detect=true);

-- Handle multiple YAML documents (default: true)
SELECT * FROM read_yaml('file.yaml', multi_document=true);

-- Expand top-level sequences into rows (default: true)
SELECT * FROM read_yaml('file.yaml', expand_root_sequence=true);

-- Ignore parsing errors (default: false)
SELECT * FROM read_yaml('file.yaml', ignore_errors=true);

-- Set maximum file size in bytes (default: 16MB)
SELECT * FROM read_yaml('file.yaml', maximum_object_size=1048576);
```

## Direct File Path Support

The YAML extension supports directly using file paths in the `FROM` clause, similar to other DuckDB file formats like JSON, CSV, and Parquet.

```sql
-- These are equivalent:
SELECT * FROM 'data.yaml';
SELECT * FROM read_yaml('data.yaml');

-- The extension supports both .yaml and .yml file extensions
SELECT * FROM 'config.yml';

-- Glob patterns work the same way
SELECT * FROM 'data/*.yaml';

-- You can use all DuckDB SQL capabilities
SELECT name, age FROM 'people.yaml' WHERE age > 30;

-- Create tables directly
CREATE TABLE config AS SELECT * FROM 'config.yaml';
```

This allows for a more concise and natural SQL syntax when working with YAML files.

Note: When using this direct syntax, the `read_yaml` function is used with default parameters. If you need to customize parameters like `auto_detect`, `ignore_errors`, etc., you should still use the `read_yaml` function explicitly.

### Error Handling

The YAML extension provides robust error handling capabilities:

 * Use `ignore_errors=true` to continue processing despite errors in some files
 * With `ignore_errors=true` and multi-document YAML, valid documents are still processed even if some have errors
 * When providing a file list, parsing continues with the next file if an error occurs in one file

### Accessing YAML Data

The reader converts YAML structures to native DuckDB types:

```sql
-- Access scalar values (using DuckDB's dot notation)
SELECT name, age FROM yaml_table;

-- Access array elements (1-based indexing)
SELECT tags[1], tags[2] FROM yaml_table;

-- Access nested structures
SELECT person.addresses[1].city FROM yaml_table;
```

### Multi-document and Sequence Handling

YAML files can contain multiple documents separated by `---` or top-level sequences. Both are handled automatically:

```yaml
# Multi-document example
---
id: 1
name: John
---
id: 2
name: Jane

# Sequence example
- id: 1
  name: John
- id: 2
  name: Jane
```

Both formats will produce the same result when read with `read_yaml()`:

```sql
SELECT id, name FROM read_yaml('file.yaml');
```

Result:
```
┌─────┬──────┐
│ id  │ name │
├─────┼──────┤
│ 1   │ John │
│ 2   │ Jane │
└─────┴──────┘
```

## Building from Source

This extension uses the DuckDB extension framework. To build:

```bash
make
```

## Testing

To run the test suite:

```bash
make test
```

## Limitations and Future Work

Current limitations:
- No support for YAML anchors and aliases
- No direct YAML to JSON conversion
- Limited type detection
- No streaming for large files

Planned features:
- YAML type system and conversion functions
- Improved type detection for dates, timestamps, etc.
- Support for YAML-specific features
- Path expressions and extraction functions
- Streaming for large files

## Considerations and Thoughts for AI Generation

This extension is an exercise in having Claude.ai create an extension for me, using other extensions as examples, while I try to delay diving into the code myself for as long as possible. The inital chat I used to start this project is [shared here](https://claude.ai/share/b7778f6e-cce4-4bba-9809-8682468a6940). Eventually, I created a project with integrations into this GitHub repository, which prevents additional sharing, but I will add the chats to this repository for reference. More information on this is provided later in the README.

I made an intentional effort to not modify much of the code directly unless I needed to fix small inconsistencies or could not make my point clear to the LLM. At the end of last PR, I some minor manual changes were required to fix implementation/test inconsistenties.

One aspect that became very helpful in this was having Claude maintain its own diary throughout the process to maintain consistency between chats. This can be seen in the `CLAUDE.md` file. Sadly, due to my own sloppiness, I did not do a great job of maintaing the git log of this file.

After the initial chat above, I created a project in Claude Desktop with a connection to this repository (after some initial miss-steps) and provided the following instructions to the Project:

```yaml
# Introduction
This is a project that you have largely written. It is a DuckDB extension that allows addings processing of YAML files. We are working together to develop, test, document, and improve this. We have used the DuckDB JSON and duckdb_iceberg extensions as examples.

# Reference Information

## Project Information
This project contains a number of documents that should be consulted to better understand the current state of the project. We will periodically update these as once we have met major milestones, made significant changes, or are finishing a chat session. One of these files is specifically for you. It is discussed in a subsequent subsection. These project reference files are:
 - `README.md`: The general introduction to this project. It contains information about the project itself as well as a note about how you developed most of this application.
 - `BACKGROUND.md`: This is a document written to summarize our first explorations on this project before the project was created.
 - `TODO.md`: A list tracking the status of planned activities in the repository.

## Tests
We have written and will continue to write a robust set of unit tests in `test/sql` using data in `test/yaml`. These can also be used as examples of how the code should (or shouldn't be used). Feel free to ask for the results of tests as we continue to develop.

## Your Memories (`CLAUDE.md`)
You have access to the source code for a project we are working on. In that repository is a file you wrote called CLAUDE.md. This file is specifically for you that describes any current thoughts, concerns, questions, ideas, instructions, or considerations you have about this project. I would like this file to be self-describing so you will understand its purpose if we begin another chat so please provide a header section that will make this clear to you. I would also like you to propose updates to it as we continue. To save context space, the updates can deltas, changes, or diffs. Alternatively, you could keep an eye on your context utilization, your ability to retain key details, or the complexity of the conversation and propose changes to this document as needed. While updating this file, take a moment to reflect on the project details as well as any thoughts you have about me as the prompter and make updates to ensure it's accurate. Feel free to write candidly for yourself to improve your ability to aid me in this process. I will periodically prompt you to update this file, but also consider updating it yourself after we achieve milestones, have gone down a a long tangent, or our chat is getting long. 

# Additional Interaction Instructions
 - At any time, feel free to ask me questions  about this project or the process
 - If you want me to run a piece of code or a test, you can ask me and I will share the results
 - Consider use cases frequently to ensure we build a useful and pleasant interface
 - We should try to keep each chat session to a small number of topics. Feel free to suggest we start a new one instead of letting the thread go too long.
```

## License

This extension is released under the MIT License.
